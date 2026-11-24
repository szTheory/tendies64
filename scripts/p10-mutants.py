#!/usr/bin/env python3
"""Out-of-tree mutant driver for the Phase 10 VG, VID and CORE tests (plan 10-01).

Modeled on scripts/k5-mutants.py. Each mutant is a small text edit of tracked
production source. The edit is never applied in the working tree: --run creates
a throwaway git worktree under build/10-mutants, applies the edits there,
builds the core library with the gate flags (Werror on) and links the named
test binaries. A mutant is

  KILLED    it compiled, the accepted baseline passed in the same worktree, and a
            named check failed with a failed CHECK line of its own test file
            (OWN_CHECK_LINE) or, for a matrix check, the expected COMPARATOR
            label;
  SURVIVED  it compiled and every named check passed;
  INVALID   the build failed (a mutant that does not compile is never a kill).

Two controls prove the driver itself: an equivalent edit that must SURVIVE and
an edit that cannot compile and must be INVALID. A green process therefore
never stands for a reached comparator. Plan 10-01 ships the controls only; the
plans 10-02 (K1), 10-06 and 10-07 (VG) and 10-10 and 10-12 (VID and CORE) append
mutant rows to MUTANTS and the test sources they need to TEST_SOURCES.

The controls use the existing core/tests/test_rdp_depth.c read-only: the source
is compiled in the worktree and never edited (it is bound by the oracle
authority record).

Modes:
  --list                     one line per mutant or control
  --check-anchors            every edit anchor occurs exactly once in its tracked
                             file and differs from its replacement
  --run [ID ...]             build and classify (default: all mutants and both
                             controls); --no-controls skips the controls

Exit codes: 0 all as expected, 1 a survivor or an unexpected control result,
2 usage or a dirty tree (tracked files outside .planning/ must equal the
committed source; planning records cannot affect a mutant), 3 an anchor missing
or not unique, 4 a baseline failure. Standard library only; run with
`python3 -B`. No absolute path is printed. Results go to RESULTS.
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORKTREE = "build/10-mutants/wt"
RESULTS = "build/gate/10-p10-results.txt"
ASSET = "build/fixtures/ipl3_compat.z64"
ASSET_PIN_SOURCE = "scripts/build-boot-fixtures.sh"
FILL_ROM = "build/fixtures/rdp-fill.z64"
FILL_MANIFEST = "fixtures/manifest.json"
FILL_MANIFEST_ID = "rdp-fill"

# The gate warning flags of core/CMakeLists.txt t64_target_warnings, used to
# compile the test sources next to the core library built by CMake.
WARNING_FLAGS = (
    "-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes "
    "-Wmissing-prototypes -Wundef -Wvla -Wcast-qual -Wformat=2 "
    "-Wwrite-strings -Werror"
)
COMPILE_FLAGS = ["-O3", "-DNDEBUG", "-std=c17"] + WARNING_FLAGS.split()
INCLUDES = ["-I", "core", "-I", "core/include", "-I", "tools/common", "-I", "tests"]

# Test binaries the checks run: name -> source. Each links the core library and
# the SHA-256 library built in the worktree. Later plans append their new test
# sources here.
TEST_SOURCES = {
    "test_rdp_depth": "core/tests/test_rdp_depth.c",
}

OWN_CHECK_LINE = re.compile(r"test_[a-z_0-9]+\.c:\d+: CHECK(_INT|_STR)? failed")


def own_check(binary, args):
    """A check of a non-matrix test: killed by a failed CHECK line of its own file."""
    return {"bin": binary, "args": list(args), "labels": None}


def matrix_check(binary, args, labels):
    """A check of a matrix test: killed by one of the COMPARATOR labels."""
    return {"bin": binary, "args": list(args), "labels": labels}


# One table. Anchors are the shortest unique multi-line strings of the committed
# source; the anchor check keeps a refactor from silently retiring a mutant.
MUTANTS = [
    {
        "id": "control-equivalent-depth-cap",
        "tag": "control",
        "expect": "SURVIVED",
        "title": "primitive Z cap written as >= 0x8000 instead of > 0x7fff (equivalent for an unsigned integer)",
        "edits": [
            (
                "core/t64_rdp_depth.c",
                "    if (raw_z > UINT32_C(0x7fff)) raw_z = UINT32_C(0x7fff);\n",
                "    if (raw_z >= UINT32_C(0x8000)) raw_z = UINT32_C(0x7fff);\n",
            )
        ],
        "kill": [],
        "record": [],
        # The whole existing depth test binary (every case) must still pass.
        "all_pass": [own_check("test_rdp_depth", [])],
    },
    {
        "id": "control-no-compile",
        "tag": "control",
        "expect": "INVALID",
        "title": "a deliberately non-compiling source (reported INVALID, never KILLED)",
        "edits": [
            (
                "core/t64_rdp_depth.c",
                '#include "t64_internal.h"\n',
                '#include "t64_internal.h"\n#error p10-control: this line must not compile\n',
            )
        ],
        "kill": [],
        "record": [],
    },
]


def log(message):
    print(message, flush=True)


# ---------------------------------------------------------------------------
# anchors


def read_tracked(relative):
    with open(os.path.join(ROOT, relative), "r", encoding="utf-8") as handle:
        return handle.read()


def check_anchors(selected=None):
    """Return a list of problems; empty when every anchor is unique."""
    problems = []
    tracked = subprocess.run(
        ["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=False
    )
    tracked_files = set(tracked.stdout.split("\n")) if tracked.returncode == 0 else None
    for name, source in TEST_SOURCES.items():
        if tracked_files is not None and source not in tracked_files:
            problems.append("%s: test source not tracked: %s" % (name, source))
    for mutant in MUTANTS:
        if selected is not None and mutant["id"] not in selected:
            continue
        for relative, anchor, replacement in mutant["edits"]:
            if tracked_files is not None and relative not in tracked_files:
                problems.append("%s: file not tracked: %s" % (mutant["id"], relative))
                continue
            try:
                text = read_tracked(relative)
            except OSError:
                problems.append("%s: cannot read %s" % (mutant["id"], relative))
                continue
            count = text.count(anchor)
            if count != 1:
                problems.append(
                    "%s: anchor occurs %d times in %s (want 1)"
                    % (mutant["id"], count, relative)
                )
            if anchor == replacement:
                problems.append("%s: replacement equals anchor in %s" % (mutant["id"], relative))
    # The test sources are compiled by this driver with the warning flags that
    # t64_target_warnings in the top level CMakeLists.txt gives every target.
    cmake_text = " ".join(read_tracked("CMakeLists.txt").split())
    if WARNING_FLAGS.replace(" -Werror", "") not in cmake_text:
        problems.append("warning flags differ from CMakeLists.txt t64_target_warnings")
    return problems


# ---------------------------------------------------------------------------
# process helpers


def run(command, cwd, env=None, timeout=900):
    merged = dict(os.environ)
    if env:
        merged.update(env)
    started = time.time()
    done = subprocess.run(
        command,
        cwd=cwd,
        env=merged,
        capture_output=True,
        text=True,
        timeout=timeout,
        check=False,
    )
    return done, time.time() - started


def tail(done, lines=6):
    """The last lines of a failed command, with the checkout path removed."""
    text = (done.stderr or "") + (done.stdout or "")
    kept = [line.replace(ROOT, "<root>") for line in text.strip().split("\n")[-lines:]]
    return "\n".join("  " + line for line in kept)


def worktree_path():
    return os.path.join(ROOT, WORKTREE)


def remove_worktree():
    path = worktree_path()
    subprocess.run(
        ["git", "worktree", "remove", "--force", WORKTREE],
        cwd=ROOT,
        capture_output=True,
        check=False,
    )
    if os.path.isdir(path):
        shutil.rmtree(path, ignore_errors=True)
    subprocess.run(["git", "worktree", "prune"], cwd=ROOT, capture_output=True, check=False)


def tree_is_clean():
    """True when no tracked file outside .planning/ differs from the commit.

    The throwaway worktree is created from HEAD, so a modified tracked source
    file would make the main tree differ from what the mutants are built from.
    Planning records (for example STATE.md written by the executor before its
    metadata commit) cannot affect a mutant and are not counted.
    """
    done = subprocess.run(
        ["git", "status", "--porcelain", "--untracked-files=no", "--",
         ".", ":(exclude).planning"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    return done.returncode == 0 and done.stdout.strip() == ""


def sha256_file(path):
    with open(path, "rb") as handle:
        return hashlib.sha256(handle.read()).hexdigest()


def asset_path_and_check():
    """The cleared openIPL3 asset of the main tree, checked against its pin."""
    path = os.path.join(ROOT, ASSET)
    pin_text = read_tracked(ASSET_PIN_SOURCE)
    match = re.search(r"^EXPECTED_ASSET_SHA256=([0-9a-f]{64})$", pin_text, re.M)
    if match is None or not os.path.isfile(path):
        return None
    return path if sha256_file(path) == match.group(1) else None


def fill_rom_path_and_check():
    """The authored fill ROM of the main tree, checked against its manifest row."""
    path = os.path.join(ROOT, FILL_ROM)
    if not os.path.isfile(path):
        return None
    manifest = json.loads(read_tracked(FILL_MANIFEST))
    rows = [e for e in manifest["entries"] if e.get("id") == FILL_MANIFEST_ID]
    if len(rows) != 1:
        return None
    return path if sha256_file(path) == rows[0].get("sha256") else None


def preset_values_ok():
    presets = json.loads(read_tracked("CMakePresets.json"))
    gate = [p for p in presets["configurePresets"] if p["name"] == "gate"][0]
    cache = gate["cacheVariables"]
    return (
        gate["generator"] == "Ninja"
        and cache.get("CMAKE_BUILD_TYPE") == "Release"
        and cache.get("T64_WERROR") == "ON"
    )


# ---------------------------------------------------------------------------
# worktree build and checks


class Worktree:
    def __init__(self, asset, fill_rom):
        self.env = {"T64_TEST_IPL3_PATH": asset, "T64_TEST_RDP_FILL_PATH": fill_rom}
        self.objects = os.path.join(worktree_path(), "build", "mut")
        self.last_build = None

    def build_core(self):
        done, seconds = run(
            ["cmake", "--build", "--preset", "gate", "--target", "t64_core", "t64_sha256",
             "--parallel", "4"],
            worktree_path(),
        )
        self.last_build = done
        return done.returncode == 0, seconds

    def compile_tests(self):
        os.makedirs(self.objects, exist_ok=True)
        total = 0.0
        for name, source in TEST_SOURCES.items():
            done, seconds = run(
                ["cc"] + COMPILE_FLAGS + INCLUDES
                + ["-c", "-o", os.path.join("build", "mut", name + ".o"), source],
                worktree_path(),
            )
            total += seconds
            if done.returncode != 0:
                return False, total
        return True, total

    def link_tests(self):
        for name in TEST_SOURCES:
            done, _ = run(
                ["cc", "-o", os.path.join("build", "mut", name),
                 os.path.join("build", "mut", name + ".o"),
                 os.path.join("build", "gate", "core", "libt64_core.a"),
                 os.path.join("build", "gate", "tools", "common", "libt64_sha256.a")],
                worktree_path(),
            )
            if done.returncode != 0:
                return False
        return True

    def run_check(self, check):
        done, _ = run(
            [os.path.join("build", "mut", check["bin"])] + check["args"],
            worktree_path(),
            env=self.env,
            timeout=300,
        )
        return done.returncode, done.stderr

    def revert(self):
        run(["git", "checkout", "--", "."], worktree_path())
        done, _ = run(["git", "status", "--porcelain", "--untracked-files=no"], worktree_path())
        return done.stdout.strip() == ""

    def apply_edits(self, edits):
        for relative, anchor, replacement in edits:
            path = os.path.join(worktree_path(), relative)
            with open(path, "r", encoding="utf-8") as handle:
                text = handle.read()
            if text.count(anchor) != 1:
                return False
            with open(path, "w", encoding="utf-8") as handle:
                handle.write(text.replace(anchor, replacement))
        return True


def label_present(stderr, labels):
    return any(("COMPARATOR %s " % label) in stderr for label in labels)


def check_key(check):
    return "%s:%s" % (check["bin"], check["args"][-1] if check["args"] else "all")


def classify(tree, mutant):
    """Return (class, failing check or '-', seconds)."""
    started = time.time()
    if not tree.apply_edits(mutant["edits"]):
        return "ANCHOR", "-", time.time() - started
    built, _ = tree.build_core()
    if not built or not tree.link_tests():
        tree.revert()
        return "INVALID", "-", time.time() - started
    failing = "-"
    result = "SURVIVED"
    for check in mutant["kill"]:
        code, stderr = tree.run_check(check)
        if code == 0:
            continue
        if check["labels"] is not None:
            if label_present(stderr, check["labels"]):
                result, failing = "KILLED", check_key(check)
        elif OWN_CHECK_LINE.search(stderr):
            result, failing = "KILLED", check_key(check)
    for check in mutant.get("all_pass", []):
        code, _ = tree.run_check(check)
        if code != 0:
            result = "KILLED"
            failing = check_key(check)
            break
    for check in mutant["record"]:
        code, _ = tree.run_check(check)
        log("record %s %s exit %d" % (mutant["id"], check["bin"], code))
    clean = tree.revert()
    if not clean:
        return "DIRTY", "-", time.time() - started
    return result, failing, time.time() - started


def run_mode(selected, with_controls):
    if not tree_is_clean():
        log("error: tracked files outside .planning/ are modified; the worktree must equal the committed source")
        return 2
    problems = check_anchors()
    if problems:
        for problem in problems:
            log("anchor: " + problem)
        return 3
    if not preset_values_ok():
        log("error: gate preset is not Ninja, Release and T64_WERROR=ON")
        return 4
    asset = asset_path_and_check()
    if asset is None:
        log("error: the cleared asset is missing or differs from its pin")
        return 4
    fill_rom = fill_rom_path_and_check()
    if fill_rom is None:
        log("error: the authored fill ROM is missing or differs from its manifest row")
        return 4
    chosen = [
        m for m in MUTANTS
        if (selected is None or m["id"] in selected)
        and (with_controls or m["tag"] != "control")
    ]
    remove_worktree()
    os.makedirs(os.path.dirname(worktree_path()), exist_ok=True)
    done, _ = run(["git", "worktree", "add", "--detach", WORKTREE, "HEAD"], ROOT)
    if done.returncode != 0:
        log("error: cannot create the worktree")
        return 4
    rows = []
    try:
        tree = Worktree(asset, fill_rom)
        log("step fixtures 0.0 (the main tree assets are reused; their SHA-256 equals the pins)")
        done, seconds = run(["cmake", "--preset", "gate"], worktree_path())
        log("step configure %.1f" % seconds)
        if done.returncode != 0:
            return 4
        built, seconds = tree.build_core()
        log("step core-build %.1f" % seconds)
        if not built:
            log(tail(tree.last_build))
            return 4
        compiled, seconds = tree.compile_tests()
        log("step test-compile %.1f" % seconds)
        if not compiled or not tree.link_tests():
            return 4
        # The accepted baseline: every named check passes unmutated.
        baseline_started = time.time()
        seen = set()
        for mutant in chosen:
            for check in mutant["kill"] + mutant["record"] + mutant.get("all_pass", []):
                key = (check["bin"], tuple(check["args"]))
                if key in seen:
                    continue
                seen.add(key)
                code, _ = tree.run_check(check)
                if code != 0:
                    log("baseline failure: %s %s" % (check["bin"], " ".join(check["args"])))
                    return 4
        log("step baseline %.1f (%d checks)" % (time.time() - baseline_started, len(seen)))
        for mutant in chosen:
            result, failing, seconds = classify(tree, mutant)
            if result in ("ANCHOR", "DIRTY"):
                log("error: %s: %s" % (mutant["id"], result.lower()))
                return 3 if result == "ANCHOR" else 4
            rows.append((mutant, result, failing, seconds))
            log("mutant %s %s %.1f" % (mutant["id"], result, seconds))
    finally:
        remove_worktree()
    lines = [
        "%s %s %s %.1f %s" % (m["id"], result, m["expect"], seconds, failing)
        for m, result, failing, seconds in rows
    ]
    os.makedirs(os.path.join(ROOT, os.path.dirname(RESULTS)), exist_ok=True)
    with open(os.path.join(ROOT, RESULTS), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    for line in lines:
        log(line)
    return 0 if all(result == m["expect"] for m, result, _, _ in rows) else 1


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--list", action="store_true")
    mode.add_argument("--check-anchors", action="store_true")
    mode.add_argument("--run", nargs="*", metavar="ID")
    parser.add_argument("--no-controls", action="store_true")
    try:
        args = parser.parse_args(argv)
    except SystemExit as error:
        return 2 if error.code not in (0, None) else 0
    if args.list:
        for mutant in MUTANTS:
            print("%s %s %s" % (mutant["id"], mutant["expect"], mutant["title"]))
        return 0
    if args.check_anchors:
        problems = check_anchors()
        for problem in problems:
            print(problem)
        return 3 if problems else 0
    ids = {m["id"] for m in MUTANTS}
    selected = set(args.run) if args.run else None
    if selected is not None and not selected <= ids:
        print("unknown mutant id")
        return 2
    return run_mode(selected, not args.no_controls)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
