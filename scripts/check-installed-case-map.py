#!/usr/bin/env python3
"""Case-to-CTest map of the installed consumer (plan 11-22, carry-in T8, W-5).

Three lists describe the cases of the installed consumer and must agree:

  LISTED      the cases the built consumer prints for `t64c --list-cases`
              (<build>/install-check/consumer/t64c, built by the CTest fixture
              installed.consumer.build against the moved prefix);
  FILE        tests/installed-consumer/cases.txt (the closed list: every line
              that starts with a lowercase letter);
  REGISTERED  the CTest names that start with `installed.consumer.`, read from
              `ctest -N -R '^installed\\.consumer\\.' --show-only=json-v1`.

Rules (each finding is one line "case-map: RULE name"):

  LIST-DUPLICATE       a case is listed twice by the binary
  FILE-DUPLICATE       a case is listed twice in cases.txt
  LISTED-NOT-IN-FILE   the binary lists a case cases.txt does not
  FILE-NOT-LISTED      cases.txt names a case the binary does not list
  CASE-SHADOWS-SCRIPT  a case has the name of an allowlisted script name
  NO-CTEST-NAME        a case has no CTest name installed.consumer.<case>
  REGISTERED-TWICE     a CTest name occurs twice
  UNKNOWN-NAME         an installed.consumer.* name that is neither
                       installed.consumer.<case> nor an allowlisted script name
  SCRIPT-NAME-MISSING  an allowlisted script name is not registered

The script names are not binary cases: installed.consumer.build builds the
consumer, installed.consumer.literals compares its literals with the committed
fixtures and installed.consumer.three-way compares it with the in-tree runner.
The allowlist is the tuple SCRIPT_NAMES below, kept in this checker and not
only in a comment of cases.txt, so a new name has to be added here on purpose.
At the plan 11-22 head: 24 cases plus 3 script names, 27 names in total.

Usage: check-installed-case-map.py [--build DIR] [--selftest]
  --build DIR   the configured build tree (default build/gate)
  --selftest    first run the seeded cases of compare(): each seeded defect
                must be reported with its own rule, the clean case with none
Exit: 0 clean, 1 findings or a failed seeded case, 2 usage error or a missing
tool or tree. Standard library only; run with `python3 -B`.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES_FILE = "tests/installed-consumer/cases.txt"
CONSUMER = os.path.join("install-check", "consumer", "t64c")
PREFIX = "installed.consumer."
SCRIPT_NAMES = (
    "installed.consumer.build",
    "installed.consumer.literals",
    "installed.consumer.three-way",
)


def duplicates(items):
    seen = set()
    repeated = []
    for item in items:
        if item in seen and item not in repeated:
            repeated.append(item)
        seen.add(item)
    return repeated


def compare(listed, file_cases, registered):
    """Return the findings (rule, name) for the three lists."""
    findings = []
    for name in duplicates(listed):
        findings.append(("LIST-DUPLICATE", name))
    for name in duplicates(file_cases):
        findings.append(("FILE-DUPLICATE", name))
    for name in sorted(set(listed) - set(file_cases)):
        findings.append(("LISTED-NOT-IN-FILE", name))
    for name in sorted(set(file_cases) - set(listed)):
        findings.append(("FILE-NOT-LISTED", name))
    cases = sorted(set(listed) | set(file_cases))
    for name in cases:
        if PREFIX + name in SCRIPT_NAMES:
            findings.append(("CASE-SHADOWS-SCRIPT", name))
    for name in duplicates(registered):
        findings.append(("REGISTERED-TWICE", name))
    present = set(registered)
    for name in cases:
        if PREFIX + name not in present and PREFIX + name not in SCRIPT_NAMES:
            findings.append(("NO-CTEST-NAME", PREFIX + name))
    allowed = {PREFIX + name for name in cases} | set(SCRIPT_NAMES)
    for name in sorted(present - allowed):
        findings.append(("UNKNOWN-NAME", name))
    for name in SCRIPT_NAMES:
        if name not in present:
            findings.append(("SCRIPT-NAME-MISSING", name))
    return findings


def read_cases_file(path):
    with open(path, "r", encoding="utf-8") as handle:
        return [line.strip() for line in handle if line[:1].islower()]


def run(command, cwd):
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                          check=False, timeout=60)


def listed_cases(build):
    binary = os.path.join(build, CONSUMER)
    if not os.path.isfile(binary) or not os.access(binary, os.X_OK):
        print("case-map: no consumer executable below the build tree "
              "(the fixture installed.consumer.build builds it)", file=sys.stderr)
        return None
    done = run([binary, "--list-cases"], build)
    if done.returncode != 0:
        print("case-map: the consumer --list-cases failed", file=sys.stderr)
        return None
    return [line.strip() for line in done.stdout.splitlines() if line.strip()]


def registered_names(build):
    ctest = shutil.which("ctest")
    if ctest is None:
        print("case-map: ctest not found", file=sys.stderr)
        return None
    done = run([ctest, "--test-dir", build, "-N", "-R", r"^installed\.consumer\.",
                "--show-only=json-v1"], ROOT)
    if done.returncode != 0:
        print("case-map: ctest -N failed", file=sys.stderr)
        return None
    try:
        tests = json.loads(done.stdout)["tests"]
        names = [test["name"] for test in tests]
    except (ValueError, KeyError, TypeError):
        print("case-map: unreadable ctest json", file=sys.stderr)
        return None
    # The required fixture setup tests (install.setup) join the list; the map
    # concerns the installed.consumer.* names only.
    return [name for name in names if name.startswith(PREFIX)]


# One seeded defect per case: (label, listed, file, registered, rule that must
# be reported, or None for the clean case).
def seed_cases():
    cases = ["smoke", "composite", "ops-bus"]
    names = ([PREFIX + case for case in cases] + list(SCRIPT_NAMES))
    return [
        ("clean", cases, cases, names, None),
        ("binary-extra-case", cases + ["extra"], cases, names + [PREFIX + "extra"],
         "LISTED-NOT-IN-FILE"),
        ("file-extra-case", cases, cases + ["extra"], names + [PREFIX + "extra"],
         "FILE-NOT-LISTED"),
        ("case-without-ctest-name", cases, cases, names[:1] + names[2:],
         "NO-CTEST-NAME"),
        ("unknown-name", cases, cases, names + [PREFIX + "stray"], "UNKNOWN-NAME"),
        ("name-registered-twice", cases, cases, names + [PREFIX + "composite"],
         "REGISTERED-TWICE"),
        ("script-name-registered-twice", cases, cases,
         names + [SCRIPT_NAMES[0]], "REGISTERED-TWICE"),
        ("script-name-missing", cases, cases, names[:-1], "SCRIPT-NAME-MISSING"),
        ("case-shadows-script-name", cases + ["build"], cases + ["build"], names,
         "CASE-SHADOWS-SCRIPT"),
        ("duplicate-in-binary", cases + ["smoke"], cases, names, "LIST-DUPLICATE"),
        ("duplicate-in-file", cases, cases + ["smoke"], names, "FILE-DUPLICATE"),
    ]


def selftest():
    failures = 0
    for label, listed, file_cases, registered, want in seed_cases():
        rules = {rule for rule, _ in compare(listed, file_cases, registered)}
        ok = (not rules) if want is None else (want in rules)
        print("case-map: seeded %-30s %s" % (label, "ok" if ok else "FAILED (rules: %s)"
                                              % ", ".join(sorted(rules))))
        if not ok:
            failures += 1
    return failures


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("--build", default=os.path.join(ROOT, "build", "gate"),
                        metavar="DIR")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    build = os.path.abspath(args.build)
    if args.selftest and selftest() != 0:
        print("case-map: the seeded cases did not all fail as seeded", file=sys.stderr)
        return 1
    if not os.path.isfile(os.path.join(build, "CMakeCache.txt")):
        print("case-map: not a configured CMake tree: %s" % args.build, file=sys.stderr)
        return 2
    try:
        file_cases = read_cases_file(os.path.join(ROOT, CASES_FILE))
    except OSError:
        print("case-map: cannot read %s" % CASES_FILE, file=sys.stderr)
        return 2
    listed = listed_cases(build)
    registered = registered_names(build)
    if listed is None or registered is None or not file_cases:
        return 2
    findings = compare(listed, file_cases, registered)
    for rule, name in findings:
        print("case-map: %s %s" % (rule, name))
    if findings:
        print("case-map: findings=%d" % len(findings))
        return 1
    print("case-map: ok: %d cases, %d script names, %d registered names"
          % (len(set(file_cases)), len(SCRIPT_NAMES), len(registered)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
