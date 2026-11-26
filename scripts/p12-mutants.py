#!/usr/bin/env python3
"""Out-of-tree mutant driver for the Phase 12 host pump and the title (plan 12-08, DR-32).

Modeled on scripts/p10-mutants.py. Each mutant is a small text edit of tracked source. The
edit is never applied in the working tree: --run creates a throwaway git worktree under
build/12-mutants, applies the edit there, rebuilds what the edit touches and runs the host
case that must notice it. A mutant is

  KILLED    it built, the unmutated baseline passed in the same worktree, and the named host
            case failed with a failed check line of its own test file (t64h_tests.c) or a
            MISMATCH line of the title comparator (scripts/check-host-title.py);
  SURVIVED  it built and every named case passed (or failed without an own check line:
            a crash or a timeout is never a kill);
  INVALID   the build failed (a mutant that does not compile is never a kill).

Two controls prove the driver itself: an equivalent edit that must SURVIVE and an edit that
cannot compile and must be INVALID. A green process therefore never stands for a reached
check.

The 8 pump mutants edit host/pump (the pump, the input table and the frame helpers); the host
project is rebuilt from the worktree against the moved prefix of the gate tree
(scripts/check-host.sh --host-build) and the named cases run on the title ROM of the main tree
(checked against its manifest row). The 4 title mutants edit fixtures/title/title.S; the
worktree rebuilds the title ROM (scripts/build-title-fixture.sh --build with the main tree's
pinned prerequisites copied in) and the case title-two-speeds runs on the mutated ROM through
scripts/check-host-title.py (a behavioral comparison with the oracle and the pins, not the
manifest identity). The full --run is a check of the 12-12 review clone and a qualifier stage;
plan 12-08 gates only the anchors (CTest repo.p12-mutant-anchors).

Modes:
  --list                     one line per mutant or control: ID, the CTest name of the host
                             case that must kill it, the file its first edit anchors in,
                             the expected class and a title
  --check-anchors            every edit anchor occurs exactly once in its tracked file and
                             differs from its replacement; every killing test is registered
                             in tools/hostcheck/CMakeLists.txt and runs a case of the host
                             test binary; the table holds 12 mutants and 2 controls
  --run [ID ...]             build and classify (default: all mutants and both controls);
                             --no-controls skips the controls

Exit codes: 0 all as expected, 1 a survivor or an unexpected control result, 2 usage or a
dirty tree (tracked files outside .planning/ must equal the committed source), 3 an anchor
missing or not unique, 4 a baseline failure or a missing prerequisite. Standard library only;
run with `python3 -B`. No absolute path is printed. Results go to RESULTS.
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
WORKTREE = "build/12-mutants/wt"
GATE = "build/gate"
RESULTS = "build/gate/12-p12-results.txt"
TITLE_ROM = "build/fixtures/phase12-title-s32k.z64"
TITLE_MANIFEST = "fixtures/manifest.json"
TITLE_MANIFEST_ID = "phase12-title-s32k"
HOST_REGISTRATION = "tools/hostcheck/CMakeLists.txt"
HOST_TESTS_SOURCE = "host/tests/t64h_tests.c"
TITLE_SOURCE = "fixtures/title/title.S"

# Files the title builder reads from build/ (copied into the worktree: the builder writes its
# outputs beside them and must never write into the main tree) and the directory of tools it
# only executes (linked).
TITLE_BUILD_COPIES = [
    "build/fixtures/ipl3_compat.z64",
    "build/fixtures/ipl3_compat.payload.bin",
    "build/fixtures/t64_rom_pack",
]
TITLE_BUILD_LINKS = ["build/tools"]

# The CTest names of the host cases and the case each one runs.
TEST_CASES = {
    "host.pump.drain-order": "drain-order",
    "host.pump.clock": "clock",
    "host.pump.stall": "stall",
    "host.pump.title-two-speeds": "title-two-speeds",
    "host.pump.input-map": "input-map",
    "host.pump.script": "script",
    "host.pump.frames": "frames",
}

# A failure line of the host test binary's own checks or of the title comparator.
OWN_FAILURE = re.compile(r"t64h_tests\.c:\d+: |check-host-title: MISMATCH ")

PUMP_C = "host/pump/t64h_pump.c"
PUMP_H = "host/pump/t64h_pump.h"
INPUT_C = "host/pump/t64h_input.c"
FRAMES_C = "host/pump/t64h_frames.c"

# The "queue after the advance" mutant: the hook call moves from before each advance to the
# end of the step, behind every advance of it. A step that spans many events (the 3 Hz clock
# of the two-speed case jumps over the whole run) then runs the guest past events it never
# saw, and the late call reaches a machine that has already passed them (TIME_REVERSED, the
# step ends in INPUT_ERROR); a small step is late by less than the gap to the next poll and
# does not show it.
LATE_HOOK = (
    "  if (pump->config.input != NULL) {\n"
    "    uint64_t late = bound;\n"
    "\n"
    "    if (pump->config.input(pump->config.input_context, pump->machine, pump->tick, &late) != 0) {\n"
    "      result->message = \"the input hook failed\";\n"
    "      return T64H_INPUT_ERROR;\n"
    "    }\n"
    "  }\n"
    "  return drain_pass(pump, &delivered, result);\n"
    "}\n"
)

# One table. Anchors are the shortest unique multi-line strings of the committed source; the
# anchor check keeps a refactor from silently retiring a mutant.
MUTANTS = [
    {
        "id": "P12-P01",
        "tag": "mutant",
        "kind": "pump",
        "expect": "KILLED",
        "test": "host.pump.clock",
        "title": "the tick divisor 16 written as 15 (the conversion of host nanoseconds to guest ticks)",
        "edits": [
            (PUMP_H, "#define T64H_TICK_NS UINT64_C(16)\n", "#define T64H_TICK_NS UINT64_C(15)\n"),
        ],
    },
    {
        "id": "P12-P02",
        "tag": "mutant",
        "kind": "pump",
        "expect": "KILLED",
        "test": "host.pump.title-two-speeds",
        "title": "the input hook called once at the end of a step instead of before each advance (a large step runs past events it never saw)",
        "edits": [
            (
                PUMP_C,
                "    if (pump->config.input != NULL) {\n      const uint64_t offered = bound;\n",
                "    if (pump->config.input != NULL && pump->counters.advances == UINT64_MAX) {\n"
                "      const uint64_t offered = bound;\n",
            ),
            (
                PUMP_C,
                "  return drain_pass(pump, &delivered, result);\n}\n",
                LATE_HOOK,
            ),
        ],
    },
    {
        "id": "P12-P03",
        "tag": "mutant",
        "kind": "pump",
        "expect": "KILLED",
        "test": "host.pump.drain-order",
        "title": "the ISViewer text drain skipped at the end of every drain pass",
        "edits": [
            (
                PUMP_C,
                "  return drain_text(pump, delivered, result);\n",
                "  (void)drain_text;\n  return T64H_OK;\n",
            ),
        ],
    },
    {
        "id": "P12-P04",
        "tag": "mutant",
        "kind": "pump",
        "expect": "KILLED",
        "test": "host.pump.stall",
        "title": "a video packet the sink refused is dropped instead of held (the first packet is lost on a stall)",
        "edits": [
            (PUMP_C, "      pump->video_held = 1;\n", "      pump->video_held = 0;\n"),
        ],
    },
    {
        "id": "P12-P05",
        "tag": "mutant",
        "kind": "pump",
        "expect": "KILLED",
        "test": "host.pump.frames",
        "title": "the frame copy reads the source rows at the row length instead of stride_bytes",
        "edits": [
            (
                FRAMES_C,
                "    (void)memcpy(dest + y * dest_pitch, packet + y * stride, row_bytes);\n",
                "    (void)memcpy(dest + y * dest_pitch, packet + y * row_bytes, row_bytes);\n",
            ),
        ],
    },
    {
        "id": "P12-P06",
        "tag": "mutant",
        "kind": "pump",
        "expect": "KILLED",
        "test": "host.pump.input-map",
        "title": "the stick dead zone one unit too wide (an output of 8 maps to 0)",
        "edits": [
            (
                INPUT_C,
                "  if (scaled > -T64H_AXIS_DEADZONE && scaled < T64H_AXIS_DEADZONE) {\n",
                "  if (scaled > -T64H_AXIS_DEADZONE && scaled <= T64H_AXIS_DEADZONE) {\n",
            ),
        ],
    },
    {
        "id": "P12-P07",
        "tag": "mutant",
        "kind": "pump",
        "expect": "KILLED",
        "test": "host.pump.input-map",
        "title": "the vertical axis inverted by negation instead of complement (full host down reaches -127, not -128)",
        "edits": [
            (INPUT_C, "    v = -1 - v;\n", "    v = -v;\n"),
        ],
    },
    {
        "id": "P12-P08",
        "tag": "mutant",
        "kind": "pump",
        "expect": "KILLED",
        "test": "host.pump.stall",
        "title": "a guest completion treated as resumable (the pump is not terminal after it)",
        "edits": [
            (
                PUMP_C,
                "    if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION ||\n"
                "        stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED ||\n",
                "    if (stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED ||\n",
            ),
        ],
    },
    {
        "id": "P12-T01",
        "tag": "mutant",
        "kind": "title",
        "expect": "KILLED",
        "test": "host.pump.title-two-speeds",
        "title": "the title's D-pad right and left bits swapped (the square moves the wrong way)",
        "edits": [
            (
                TITLE_SOURCE,
                "  DIRECTION $t6, T64_PAD_RIGHT, $t4, 1\n  DIRECTION $t7, T64_PAD_LEFT, $t4, -1\n",
                "  DIRECTION $t6, T64_PAD_LEFT, $t4, 1\n  DIRECTION $t7, T64_PAD_RIGHT, $t4, -1\n",
            ),
        ],
    },
    {
        "id": "P12-T02",
        "tag": "mutant",
        "kind": "title",
        "expect": "KILLED",
        "test": "host.pump.title-two-speeds",
        "title": "the title's horizontal step 3 pixels instead of 2",
        "edits": [
            (
                TITLE_SOURCE,
                "  sll $t6, $t6, 1\n  addu $s5, $s5, $t6\n",
                "  sll $t7, $t6, 1\n  addu $t6, $t6, $t7\n  addu $s5, $s5, $t6\n",
            ),
        ],
    },
    {
        "id": "P12-T03",
        "tag": "mutant",
        "kind": "title",
        "expect": "KILLED",
        "test": "host.pump.title-two-speeds",
        "title": "one tone table sample wrong (0x2001 instead of 0x2000 in the right channel)",
        "edits": [
            (TITLE_SOURCE, "  .4byte 0x20002000\n", "  .4byte 0x20002001\n"),
        ],
    },
    {
        "id": "P12-T04",
        "tag": "mutant",
        "kind": "title",
        "expect": "KILLED",
        "test": "host.pump.title-two-speeds",
        "title": "the press count stored at slot offset 8 instead of 12 (a wrong save offset)",
        "edits": [
            (
                TITLE_SOURCE,
                "  sw $s7, T64_SLOT_RDRAM + 12($s2)\n",
                "  sw $s7, T64_SLOT_RDRAM + 8($s2)\n",
            ),
        ],
    },
    {
        "id": "P12-C-EQUIV",
        "tag": "control",
        "kind": "pump",
        "expect": "SURVIVED",
        "test": "host.pump.clock",
        "title": "the tick conversion written as a shift by 4 instead of a division by 16 (equivalent for an unsigned integer)",
        "edits": [
            (PUMP_C, "  return elapsed_ns / T64H_TICK_NS;\n", "  return elapsed_ns >> 4;\n"),
        ],
        "also": ["host.pump.title-two-speeds"],
    },
    {
        "id": "P12-C-INVALID",
        "tag": "control",
        "kind": "pump",
        "expect": "INVALID",
        "test": "host.pump.clock",
        "title": "a deliberately non-compiling source (reported INVALID, never KILLED)",
        "edits": [
            (
                PUMP_C,
                '#include "t64h_pump.h"\n',
                '#include "t64h_pump.h"\n#error p12-control: this line must not compile\n',
            ),
        ],
    },
]


def log(message):
    print(message, flush=True)


# ---------------------------------------------------------------------------
# anchors


def read_tracked(relative):
    with open(os.path.join(ROOT, relative), "r", encoding="utf-8") as handle:
        return handle.read()


def tracked_files():
    done = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=False)
    return set(done.stdout.split("\n")) if done.returncode == 0 else None


def check_anchors():
    """Return a list of problems; empty when the table and every anchor are sound."""
    problems = []
    tracked = tracked_files()
    ids = [m["id"] for m in MUTANTS]
    if len(set(ids)) != len(ids):
        problems.append("duplicate mutant id")
    if len([m for m in MUTANTS if m["tag"] == "mutant" and m["kind"] == "pump"]) != 8:
        problems.append("the table does not hold 8 pump mutants")
    if len([m for m in MUTANTS if m["tag"] == "mutant" and m["kind"] == "title"]) != 4:
        problems.append("the table does not hold 4 title mutants")
    if len([m for m in MUTANTS if m["tag"] == "control"]) != 2:
        problems.append("the table does not hold 2 controls")
    try:
        registration = read_tracked(HOST_REGISTRATION)
        tests_source = read_tracked(HOST_TESTS_SOURCE)
    except OSError:
        return problems + ["cannot read the host registration or the host test source"]
    registered = set(re.findall(r"add_test\(NAME (host\.pump\.[a-z-]+)", registration))
    for mutant in MUTANTS:
        tests = [mutant["test"]] + mutant.get("also", [])
        for test in tests:
            if test not in TEST_CASES:
                problems.append("%s: killing test %s has no case in this driver" % (mutant["id"], test))
            elif test not in registered:
                problems.append("%s: killing test %s is not registered in %s" % (mutant["id"], test, HOST_REGISTRATION))
            elif ('{"%s",' % TEST_CASES[test]) not in tests_source:
                problems.append("%s: case %s is not in the host test binary" % (mutant["id"], TEST_CASES[test]))
        for relative, anchor, replacement in mutant["edits"]:
            if tracked is not None and relative not in tracked:
                problems.append("%s: file not tracked: %s" % (mutant["id"], relative))
                continue
            try:
                text = read_tracked(relative)
            except OSError:
                problems.append("%s: cannot read %s" % (mutant["id"], relative))
                continue
            count = text.count(anchor)
            if count != 1:
                problems.append("%s: anchor occurs %d times in %s (want 1)" % (mutant["id"], count, relative))
            if anchor == replacement:
                problems.append("%s: replacement equals anchor in %s" % (mutant["id"], relative))
    return problems


# ---------------------------------------------------------------------------
# process helpers


def run(command, cwd, env=None, timeout=900):
    merged = dict(os.environ)
    if env:
        merged.update(env)
    started = time.time()
    try:
        done = subprocess.run(
            command, cwd=cwd, env=merged, capture_output=True, text=True, timeout=timeout, check=False
        )
    except subprocess.TimeoutExpired:
        done = subprocess.CompletedProcess(command, 124, "", "timeout")
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
        ["git", "worktree", "remove", "--force", WORKTREE], cwd=ROOT, capture_output=True, check=False
    )
    if os.path.isdir(path):
        shutil.rmtree(path, ignore_errors=True)
    subprocess.run(["git", "worktree", "prune"], cwd=ROOT, capture_output=True, check=False)


def tree_is_clean():
    """True when no tracked file outside .planning/ differs from the commit."""
    done = subprocess.run(
        ["git", "status", "--porcelain", "--untracked-files=no", "--", ".", ":(exclude).planning"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    return done.returncode == 0 and done.stdout.strip() == ""


def sha256_file(path):
    with open(path, "rb") as handle:
        return hashlib.sha256(handle.read()).hexdigest()


def title_rom_matches_manifest(path):
    """The authored title ROM at path equals its manifest row (SHA-256 and size)."""
    if not os.path.isfile(path):
        return False
    manifest = json.loads(read_tracked(TITLE_MANIFEST))
    rows = [e for e in manifest["entries"] if e.get("id") == TITLE_MANIFEST_ID]
    if len(rows) != 1:
        return False
    return sha256_file(path) == rows[0].get("sha256") and os.path.getsize(path) == rows[0].get("size")


# ---------------------------------------------------------------------------
# worktree build and checks


class Worktree:
    def __init__(self):
        self.gate = os.path.join(ROOT, GATE)
        self.main_rom = os.path.join(ROOT, TITLE_ROM)
        self.wt_rom = os.path.join(worktree_path(), TITLE_ROM)
        self.pump_build = os.path.join(worktree_path(), "build", "mut-host")
        self.base_build = os.path.join(worktree_path(), "build", "mut-host-base")
        self.last = None

    def host_script(self, host_build, *args, timeout=600):
        """Run scripts/check-host.sh of the worktree against the gate tree's moved prefix."""
        command = ["sh", "scripts/check-host.sh", "--build", self.gate, "--host-build", host_build] + list(args)
        done, seconds = run(command, worktree_path(), timeout=timeout)
        self.last = done
        return done, seconds

    def build_host(self, host_build):
        done, seconds = self.host_script(host_build, "build")
        return done.returncode == 0, seconds

    def build_title_rom(self):
        done, seconds = run(["sh", "scripts/build-title-fixture.sh", "--build"], worktree_path())
        self.last = done
        return done.returncode == 0, seconds

    def prepare_title_build(self):
        """Copy the pinned prerequisites of the title builder into the worktree."""
        for relative in TITLE_BUILD_COPIES:
            source = os.path.join(ROOT, relative)
            target = os.path.join(worktree_path(), relative)
            if not os.path.isfile(source):
                return False
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copy2(source, target)
        for relative in TITLE_BUILD_LINKS:
            source = os.path.join(ROOT, relative)
            target = os.path.join(worktree_path(), relative)
            if not os.path.isdir(source):
                return False
            os.makedirs(os.path.dirname(target), exist_ok=True)
            if not os.path.lexists(target):
                os.symlink(source, target)
        return True

    def run_case(self, test, kind, host_build):
        """Run the host case of a CTest name; (exit code, own failure line seen)."""
        case = TEST_CASES[test]
        rom = self.wt_rom if kind == "title" else self.main_rom
        done, _ = self.host_script(host_build, "run", case, "--rom", rom, timeout=300)
        text = (done.stderr or "") + (done.stdout or "")
        return done.returncode, OWN_FAILURE.search(text) is not None

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

    def revert(self, edits):
        """Restore exactly the edited files and prove the worktree equals its commit."""
        files = sorted({relative for relative, _, _ in edits})
        top, _ = run(["git", "rev-parse", "--show-toplevel"], worktree_path())
        if os.path.realpath(top.stdout.strip()) != os.path.realpath(worktree_path()):
            return False
        run(["git", "checkout", "--"] + files, worktree_path())
        done, _ = run(["git", "status", "--porcelain", "--untracked-files=no"], worktree_path())
        return done.stdout.strip() == ""


def classify(tree, mutant):
    """Return (class, failing test or '-', seconds)."""
    started = time.time()
    kind = mutant["kind"]
    if not tree.apply_edits(mutant["edits"]):
        return "ANCHOR", "-", time.time() - started
    if kind == "pump":
        built, _ = tree.build_host(tree.pump_build)
        host_build = tree.pump_build
    else:
        built, _ = tree.build_title_rom()
        host_build = tree.base_build
    result = "SURVIVED"
    failing = "-"
    if not built:
        result = "INVALID"
    else:
        for test in [mutant["test"]] + mutant.get("also", []):
            code, own = tree.run_case(test, kind, host_build)
            if code != 0 and own:
                result, failing = "KILLED", test
                break
    clean = tree.revert(mutant["edits"])
    if kind == "title":
        # The next title mutant starts from the unmutated ROM.
        tree.build_title_rom()
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
    if not os.path.isfile(os.path.join(ROOT, GATE, "CMakeCache.txt")) or not os.path.isdir(
        os.path.join(ROOT, GATE, "install-check", "moved prefix")
    ):
        log("error: the gate tree has no moved prefix; build the gate preset and run ctest -R '^host\\.pump\\.build$' first")
        return 4
    if not title_rom_matches_manifest(os.path.join(ROOT, TITLE_ROM)):
        log("error: the authored title ROM is missing or differs from its manifest row")
        return 4
    chosen = [
        m
        for m in MUTANTS
        if (selected is None or m["id"] in selected) and (with_controls or m["tag"] != "control")
    ]
    need_title = any(m["kind"] == "title" for m in chosen)
    remove_worktree()
    os.makedirs(os.path.dirname(worktree_path()), exist_ok=True)
    done, _ = run(["git", "worktree", "add", "--detach", WORKTREE, "HEAD"], ROOT)
    if done.returncode != 0:
        log("error: cannot create the worktree")
        return 4
    rows = []
    try:
        tree = Worktree()
        # The baseline: the unmutated host builds from the worktree and every named case
        # passes there (the pump cases on the main tree's title ROM, the title case on the
        # ROM the worktree builds from the unmutated source, which equals the manifest row).
        built, seconds = tree.build_host(tree.base_build)
        log("step host-build %.1f" % seconds)
        if not built:
            log(tail(tree.last))
            return 4
        if need_title:
            if not tree.prepare_title_build():
                log("error: a prerequisite of the title builder is missing in the main tree")
                return 4
            built, seconds = tree.build_title_rom()
            log("step title-rom-build %.1f" % seconds)
            if not built or not title_rom_matches_manifest(tree.wt_rom):
                log(tail(tree.last))
                log("error: the worktree title ROM differs from its manifest row")
                return 4
        started = time.time()
        seen = set()
        for mutant in chosen:
            for test in [mutant["test"]] + mutant.get("also", []):
                key = (test, mutant["kind"])
                if key in seen:
                    continue
                seen.add(key)
                code, _ = tree.run_case(test, mutant["kind"], tree.base_build)
                if code != 0:
                    log("baseline failure: %s (%s mutants)" % (test, mutant["kind"]))
                    log(tail(tree.last))
                    return 4
        log("step baseline %.1f (%d checks)" % (time.time() - started, len(seen)))
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
            print(
                "%s %s %s %s %s"
                % (mutant["id"], mutant["test"], mutant["edits"][0][0], mutant["expect"], mutant["title"])
            )
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
