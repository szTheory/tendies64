#!/usr/bin/env python3
"""Two-tree digest sweep driver (plan 10-02, research correction C4).

The 09-10 comparison of the 17 pre-Phase-9 ids was an ad hoc harness that was
never committed. This driver is its committed, re-runnable replacement. It

  1. extracts a BASE commit and a HEAD tree (a ref or the working tree) into
     scratch directories under build/10-sweep (git archive; never the main
     build/gate tree and never build/fixtures),
  2. rewrites the set(T64_BUILD_ID ...) line of core/CMakeLists.txt in each
     scratch copy to one fixed string so the build identity, which enters the
     packet-bearing digests, cannot differ between the trees,
  3. builds only the t64_core and t64_sha256 targets there (Ninja, Release,
     T64_WERROR=ON: the cache variables of the gate preset),
  4. compiles tools/digest-sweep/t64_digest_sweep.c against each tree's library
     and headers and runs it with the same build/fixtures directory,
  5. sorts both outputs by id, ROM and checkpoint and compares them line by
     line.

Usage:
  python3 -B scripts/digest-sweep.py --base COMMIT --head REF|WORKTREE
                                     [--head-extra-id ID ...]
  python3 -B scripts/digest-sweep.py --selftest

--base accepts only a full 40-hex lowercase commit id that resolves to a
commit; HEAD, branch and tag names and short hashes are refused (exit 2), so a
wave base can never silently move. --head accepts any ref, or WORKTREE for the
current working tree including uncommitted edits (tracked files and untracked
files that are not ignored). --head-extra-id adds ids to the HEAD list only
(the four ids registered by later Phase 10 plans); the BASE list is always the
21 shipped ids.

Output: "sweep: N shared lines, D differences, H head-only lines". N counts
lines identical in both trees, D counts keys whose line differs plus keys
present only in BASE, H counts keys present only in HEAD. The report goes to
build/gate/10-sweep/<base12>-<head12|worktree>.txt, each tree's sorted output
to build/10-sweep/<role>-<label>/sweep.txt.

Exit codes: 0 no difference, 1 at least one difference, 2 usage, an invalid
base, a build failure or a failed coverage check. Standard library only; run
with `python3 -B`. No absolute path is printed.
"""

import argparse
import concurrent.futures
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SWEEP_ROOT = "build/10-sweep"
REPORT_ROOT = "build/gate/10-sweep"
FIXTURES = "build/fixtures"
SWEEP_SOURCE = "tools/digest-sweep/t64_digest_sweep.c"
FORCED_BUILD_ID = "t64-build-v1-sweep"
BUILD_ID_LINE = 'set(T64_BUILD_ID "t64-build-v1-${T64_BUILD_FINGERPRINT}")'
BUILD_ID_FORCED_LINE = 'set(T64_BUILD_ID "%s")' % FORCED_BUILD_ID
CORE_CMAKE = "core/CMakeLists.txt"
BASE_PATTERN = re.compile(r"^[0-9a-f]{40}$")

# The 21 ids registered at the Wave 0 base: 17 pre-Phase-9 ids and the four
# si-v1 ids (the ai-v1 id is the 17th pre-Phase-9 id).
SHIPPED_IDS = [
    "ntsc-cart-4m-skeleton",
    "ntsc-cart-4m-openipl3-warm-v1",
    "ntsc-cart-4m-openipl3-vr4300-v1",
    "ntsc-cart-4m-openipl3-vr4300-compat-v1",
    "ntsc-cart-4m-openipl3-vr4300-compat-v2",
    "ntsc-cart-4m-openipl3-rsp-functional-v1",
    "ntsc-cart-4m-openipl3-rdram-a-t1-v1",
    "ntsc-cart-4m-openipl3-rdram-b-t1-v1",
    "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2",
    "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3",
    "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4",
    "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-combiner-v5",
    "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6",
    "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7",
    "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8",
    "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9",
    "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1",
    "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1",
    "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k",
    "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k",
    "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k",
]
SI_IDS = SHIPPED_IDS[-4:]

# Candidate ROM files of the fixtures directory (the sweep program also tries
# one generated authored image); recorded with their SHA-256 in the report.
FIXTURE_FILES = [
    "ipl3_compat.z64",
    "hello.z64",
    "rdp-fill.z64",
    "audio-pcm.z64",
    "phase09-admit-si-v1.z64",
    "phase09-admit-e4k.z64",
    "phase09-admit-e16k.z64",
    "phase09-admit-s32k.z64",
]

# The gate warning flags of core/CMakeLists.txt t64_target_warnings, used to
# compile the sweep program next to the libraries built by CMake.
SWEEP_FLAGS = (
    "-O2 -std=c17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion "
    "-Wstrict-prototypes -Wmissing-prototypes -Wundef -Wvla -Wcast-qual "
    "-Wformat=2 -Wwrite-strings -Werror"
).split()


class SweepError(Exception):
    """A usage, base, build or coverage problem (exit 2)."""


def log(message):
    print(message, flush=True)


def scrub(text):
    return text.replace(ROOT, "<root>")


def git(args, check=False, text=True, input_bytes=None):
    return subprocess.run(
        ["git"] + args,
        cwd=ROOT,
        capture_output=True,
        text=text,
        input=input_bytes,
        check=check,
    )


def run(command, cwd, timeout=1800):
    return subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
        timeout=timeout,
        check=False,
    )


# ---------------------------------------------------------------------------
# tree sources


def resolve_base(value):
    """The base must be a full lowercase 40-hex commit id that exists."""
    if not BASE_PATTERN.match(value):
        raise SweepError("base must be a 40-hex commit id")
    done = git(["cat-file", "-e", value + "^{commit}"])
    if done.returncode != 0:
        raise SweepError("base must be a 40-hex commit id that names a commit")
    return value


def resolve_head(value):
    if value == "WORKTREE":
        return "WORKTREE"
    done = git(["rev-parse", "--verify", "--quiet", value + "^{commit}"])
    commit = done.stdout.strip()
    if done.returncode != 0 or not BASE_PATTERN.match(commit):
        raise SweepError("head must be a ref that names a commit, or WORKTREE")
    return commit


def tree_dir(role, label):
    return os.path.join(ROOT, SWEEP_ROOT, "%s-%s" % (role, label))


def force_build_id(path):
    """Rewrite the build identity line of one scratch copy of the core CMake file."""
    cmake = os.path.join(path, CORE_CMAKE)
    with open(cmake, "r", encoding="utf-8") as handle:
        text = handle.read()
    if text.count(BUILD_ID_LINE) != 1:
        raise SweepError("the build identity line of %s is not unique" % CORE_CMAKE)
    with open(cmake, "w", encoding="utf-8") as handle:
        handle.write(text.replace(BUILD_ID_LINE, BUILD_ID_FORCED_LINE))


def extract_commit(commit, source):
    """git archive of one commit into a clean scratch source directory."""
    shutil.rmtree(source, ignore_errors=True)
    os.makedirs(source)
    archive = subprocess.Popen(
        ["git", "archive", "--format=tar", commit],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    unpack = subprocess.run(
        ["tar", "-xf", "-", "-C", source],
        stdin=archive.stdout,
        capture_output=True,
        check=False,
    )
    archive.stdout.close()
    archive.wait()
    if archive.returncode != 0 or unpack.returncode != 0:
        raise SweepError("git archive of %s failed" % commit[:12])
    force_build_id(source)


def worktree_files():
    """Tracked files plus untracked files that are not ignored, that exist."""
    done = git(["ls-files", "-z", "--cached", "--others", "--exclude-standard"],
               text=False)
    if done.returncode != 0:
        raise SweepError("git ls-files failed")
    names = [n.decode("utf-8") for n in done.stdout.split(b"\0") if n]
    return sorted(
        n for n in set(names)
        if os.path.isfile(os.path.join(ROOT, n)) and not os.path.islink(os.path.join(ROOT, n))
    )


def sync_worktree(source):
    """Copy the working tree into the scratch source, touching only changed files.

    Unchanged files keep their modification time so the incremental build of an
    earlier WORKTREE run is reused.
    """
    names = worktree_files()
    wanted = set(names)
    os.makedirs(source, exist_ok=True)
    for directory, _dirs, files in os.walk(source):
        for name in files:
            relative = os.path.relpath(os.path.join(directory, name), source)
            if relative not in wanted:
                os.remove(os.path.join(directory, name))
    for relative in names:
        with open(os.path.join(ROOT, relative), "rb") as handle:
            data = handle.read()
        if relative == CORE_CMAKE:
            text = data.decode("utf-8")
            if text.count(BUILD_ID_LINE) != 1:
                raise SweepError("the build identity line of %s is not unique" % CORE_CMAKE)
            data = text.replace(BUILD_ID_LINE, BUILD_ID_FORCED_LINE).encode("utf-8")
        target = os.path.join(source, relative)
        if os.path.isfile(target):
            with open(target, "rb") as handle:
                if handle.read() == data:
                    continue
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "wb") as handle:
            handle.write(data)
        shutil.copymode(os.path.join(ROOT, relative), target)


def prepare_tree(role, spec):
    """Extract (or sync) and build one tree; return its scratch directory.

    spec is a 40-hex commit id, "WORKTREE", or ("edit", commit, relative, old,
    new) for the selftest's seeded tree (a commit with one text edit).
    """
    if isinstance(spec, tuple):
        _kind, commit, _relative, _old, _new = spec
        label = "seeded"
    else:
        commit = spec
        label = "worktree" if spec == "WORKTREE" else spec[:12]
    base = tree_dir(role, label)
    source = os.path.join(base, "src")
    build = os.path.join(base, "build")
    stamp = os.path.join(base, "stamp.txt")
    stamp_text = "%s %s\n" % (repr(spec), FORCED_BUILD_ID)
    if spec == "WORKTREE":
        sync_worktree(source)
    else:
        reuse = (
            os.path.isfile(stamp)
            and open(stamp, "r", encoding="utf-8").read() == stamp_text
            and os.path.isfile(os.path.join(build, "core", "libt64_core.a"))
        )
        if not reuse:
            shutil.rmtree(base, ignore_errors=True)
            extract_commit(commit, source)
            if isinstance(spec, tuple):
                apply_seed(source, spec)
    os.makedirs(base, exist_ok=True)
    started = time.time()
    done = run(["cmake", "-S", source, "-B", build, "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", "-DT64_WERROR=ON"], ROOT)
    if done.returncode == 0:
        done = run(["cmake", "--build", build, "--target", "t64_core",
                    "t64_sha256", "--parallel", "4"], ROOT)
    if done.returncode != 0:
        tail = "\n".join(scrub(done.stderr + done.stdout).strip().split("\n")[-8:])
        raise SweepError("build of the %s tree %s failed:\n%s" % (role, label, tail))
    if spec != "WORKTREE":
        with open(stamp, "w", encoding="utf-8") as handle:
            handle.write(stamp_text)
    log("step build %s-%s %.1f" % (role, label, time.time() - started))
    return base


def apply_seed(source, spec):
    """The selftest's seeded edit: one text replacement in the scratch copy."""
    _kind, _commit, relative, old, new = spec
    path = os.path.join(source, relative)
    with open(path, "r", encoding="utf-8") as handle:
        text = handle.read()
    if text.count(old) != 1:
        raise SweepError("the seeded edit anchor is not unique in %s" % relative)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text.replace(old, new))


# ---------------------------------------------------------------------------
# sweep program


def run_sweep(base, ids):
    """Compile the sweep program against one tree, run it, return sorted lines."""
    source = os.path.join(base, "src")
    program = os.path.join(base, "sweep")
    ids_file = os.path.join(base, "ids.txt")
    with open(ids_file, "w", encoding="utf-8") as handle:
        handle.write("\n".join(ids) + "\n")
    compiled = run(
        ["cc"] + SWEEP_FLAGS
        + ["-I", os.path.join(source, "core", "include"),
           "-I", os.path.join(source, "tools", "common"),
           "-o", program, os.path.join(ROOT, SWEEP_SOURCE),
           os.path.join(base, "build", "core", "libt64_core.a"),
           os.path.join(base, "build", "tools", "common", "libt64_sha256.a")],
        ROOT,
    )
    if compiled.returncode != 0:
        tail = "\n".join(scrub(compiled.stderr).strip().split("\n")[-8:])
        raise SweepError("the sweep program does not compile against %s:\n%s"
                         % (os.path.basename(base), tail))
    started = time.time()
    done = run([program, "--fixtures", os.path.join(ROOT, FIXTURES),
                "--ids", ids_file], ROOT)
    if done.returncode != 0:
        raise SweepError("the sweep program failed in %s (exit %d)"
                         % (os.path.basename(base), done.returncode))
    lines = sorted_lines(done.stdout.split("\n"))
    with open(os.path.join(base, "sweep.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    log("step sweep %s %.1f" % (os.path.basename(base), time.time() - started))
    return lines


def parse_line(line):
    """Return (sort key, comparison key, value) of one output line."""
    tokens = line.split()
    if tokens and tokens[0] == "skip" and len(tokens) == 4:
        return (tokens[1], tokens[2], -1, 0), tuple(tokens[:3]), tokens[3]
    if tokens and tokens[0] == "stop" and len(tokens) == 7:
        tick = int(tokens[3])
        return (tokens[1], tokens[2], tick, 1), tuple(tokens[:4]), " ".join(tokens[4:])
    if len(tokens) == 3 and tokens[1] == "export":
        return (tokens[0], "export", 0, 0), (tokens[0], "export"), tokens[2]
    if len(tokens) == 4 and tokens[2].isdigit():
        return (tokens[0], tokens[1], int(tokens[2]), 2), tuple(tokens[:3]), tokens[3]
    raise SweepError("unrecognized sweep line: %s" % line[:80])


def sorted_lines(raw):
    """The output sorted by id, ROM and checkpoint (numeric tick order)."""
    rows = [(parse_line(line)[0], line) for line in raw if line.strip()]
    rows.sort(key=lambda row: row[0])
    return [line for _key, line in rows]


def coverage_problems(lines, ids):
    """Every id must have at least one admitted ROM with a digest line."""
    admitted = set()
    exports = set()
    for line in lines:
        tokens = line.split()
        if len(tokens) == 3 and tokens[1] == "export":
            exports.add(tokens[0])
        elif len(tokens) == 4 and tokens[2].isdigit():
            admitted.add(tokens[0])
    problems = ["id %s has no admitted ROM" % i for i in ids if i not in admitted]
    problems += ["id %s has no export line" % i for i in SI_IDS
                 if i in ids and i not in exports]
    return problems


# ---------------------------------------------------------------------------
# comparison


def compare(base_lines, head_lines):
    """Return (shared, differences, head_only, detail lines)."""
    base = {}
    for line in base_lines:
        _sort, key, value = parse_line(line)
        base[key] = (value, line)
    head = {}
    for line in head_lines:
        _sort, key, value = parse_line(line)
        head[key] = (value, line)
    shared = 0
    differences = 0
    head_only = 0
    detail = []
    for key in sorted(set(base) | set(head), key=lambda k: parse_line(
            (base.get(k) or head.get(k))[1])[0]):
        if key in base and key in head:
            if base[key][0] == head[key][0]:
                shared += 1
            else:
                differences += 1
                detail.append("< " + base[key][1])
                detail.append("> " + head[key][1])
        elif key in base:
            differences += 1
            detail.append("< " + base[key][1])
        else:
            head_only += 1
            detail.append("+ " + head[key][1])
    return shared, differences, head_only, detail


def fixture_records():
    rows = []
    for name in FIXTURE_FILES:
        path = os.path.join(ROOT, FIXTURES, name)
        if os.path.isfile(path):
            with open(path, "rb") as handle:
                rows.append("# fixture %s %s" % (name, hashlib.sha256(handle.read()).hexdigest()))
        else:
            rows.append("# fixture %s missing" % name)
    return rows


def write_report(name, summary, detail):
    os.makedirs(os.path.join(ROOT, REPORT_ROOT), exist_ok=True)
    path = os.path.join(ROOT, REPORT_ROOT, name)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(summary + "\n")
        handle.write("\n".join(fixture_records()) + "\n")
        handle.write("\n".join(detail) + ("\n" if detail else ""))
    return path


def sweep_pair(base_spec, head_spec, head_extra, cache=None):
    """Build, run and compare two trees; return (shared, differences, head_only)."""
    base_ids = list(SHIPPED_IDS)
    head_ids = list(SHIPPED_IDS) + [i for i in head_extra if i not in SHIPPED_IDS]
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        base_job = pool.submit(prepare_tree, "base", base_spec)
        head_job = pool.submit(prepare_tree, "head", head_spec)
        base_dir = base_job.result()
        head_dir = head_job.result()
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        if cache is not None and base_dir in cache:
            base_lines = cache[base_dir]
            head_job = pool.submit(run_sweep, head_dir, head_ids)
        else:
            base_job = pool.submit(run_sweep, base_dir, base_ids)
            head_job = pool.submit(run_sweep, head_dir, head_ids)
            base_lines = base_job.result()
            if cache is not None:
                cache[base_dir] = base_lines
        head_lines = head_job.result()
    problems = coverage_problems(base_lines, base_ids)
    if problems:
        raise SweepError("coverage: " + "; ".join(problems[:4]))
    shared, differences, head_only, detail = compare(base_lines, head_lines)
    base_name = base_spec[:12] if isinstance(base_spec, str) else base_spec[1][:12]
    if isinstance(head_spec, tuple):
        head_name = "seeded"
    elif head_spec == "WORKTREE":
        head_name = "worktree"
    else:
        head_name = head_spec[:12]
    summary = "sweep: %d shared lines, %d differences, %d head-only lines" % (
        shared, differences, head_only)
    write_report("%s-%s.txt" % (base_name, head_name), summary, detail)
    log(summary)
    for line in detail[:12]:
        log(line)
    return shared, differences, head_only


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("--base")
    parser.add_argument("--head")
    parser.add_argument("--head-extra-id", action="append", default=[])
    parser.add_argument("--selftest", action="store_true")
    try:
        args = parser.parse_args(argv)
    except SystemExit as error:
        return 2 if error.code not in (0, None) else 0
    try:
        if args.selftest:
            if args.base or args.head or args.head_extra_id:
                raise SweepError("--selftest takes no other option")
            return selftest()
        if not args.base or not args.head:
            raise SweepError("usage: --base COMMIT --head REF|WORKTREE")
        base = resolve_base(args.base)
        head = resolve_head(args.head)
        _shared, differences, _head_only = sweep_pair(base, head, args.head_extra_id)
    except SweepError as error:
        log("error: " + str(error))
        return 2
    return 1 if differences else 0


# The seeded edit of the selftest: one character of the digest domain string of
# state version 20, which only the rdp-blender-v6 id hashes. The edit is made
# in the scratch copy of the head tree; the working tree is never touched.
SEED_FILE = "core/t64_digest.c"
SEED_OLD = '"t64.state.rdp.vi.blender.20"'
SEED_NEW = '"t64.state.rdp.vi.blender.2X"'
SEED_IDS = {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6"}


def line_id(line):
    tokens = line.split()
    return tokens[1] if tokens and tokens[0] in ("skip", "stop") else tokens[0]


def selftest():
    """Prove that the sweep reports no difference for identical trees, reports
    a difference confined to the right ids for a seeded digest edit, and
    refuses a symbolic base. Exit 0 only when all three hold."""
    head = git(["rev-parse", "HEAD"]).stdout.strip()
    resolve_base(head)
    cache = {}
    ok = True

    shared, differences, _head_only = sweep_pair(head, head, [], cache)
    if differences == 0 and shared > 0:
        log("selftest: identical trees 0 differences (%d shared lines)" % shared)
    else:
        log("selftest: FAIL identical trees reported %d differences" % differences)
        ok = False

    seeded = ("edit", head, SEED_FILE, SEED_OLD, SEED_NEW)
    report = os.path.join(ROOT, REPORT_ROOT, "%s-seeded.txt" % head[:12])
    _shared, differences, _head_only = sweep_pair(head, seeded, [], cache)
    with open(report, "r", encoding="utf-8") as handle:
        changed = {line_id(l[2:]) for l in handle.read().split("\n")
                   if l.startswith(("< ", "> "))}
    if differences > 0 and changed and changed <= SEED_IDS:
        log("selftest: seeded edit detected (%d differences, only the ids of state version 20)"
            % differences)
    else:
        log("selftest: FAIL seeded edit gave %d differences in %d ids"
            % (differences, len(changed)))
        ok = False

    done = subprocess.run(
        [sys.executable, "-B", os.path.join(ROOT, "scripts", "digest-sweep.py"),
         "--base", "HEAD", "--head", "HEAD"],
        cwd=ROOT, capture_output=True, text=True, check=False)
    if done.returncode == 2 and "base must be a 40-hex commit id" in done.stdout:
        log("selftest: symbolic base refused with exit 2")
    else:
        log("selftest: FAIL symbolic base exit %d" % done.returncode)
        ok = False
    log("selftest: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
