#!/usr/bin/env python3
"""ABI checks of the public header (Phase 11, plan 11-02).

Modes:
  header   --include DIR [--header NAME]...
           compile each named public header alone, as C99 and as C++17, with
           the pedantic warning set and -Werror (CTest repo.abi-header).
  classes  --header FILE --table FILE
           every T64_STATUS_* and T64_STOP_REASON_* constant of the header
           has exactly one row in the status-class table, with the same value
           and a class from a closed set (CTest repo.abi-status-classes).
Self-test (CTest repo.abi-selftest):
  --list-cases | --case NAME | --selftest
           seeded breakages of temporary copies of the header and the table,
           each of which classes mode must report with its own finding text.

Standard library only. Run as `python3 -B scripts/check-abi.py ...`.
Exit codes: 0 clean, 1 a finding, 2 usage error or a missing tool.
"""

import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

DEFAULT_HEADER = "tendies64/t64_machine.h"

# (label, environment variable, default tool, language flags, file name)
HEADER_COMPILES = (
    ("c99", "CC", "cc", ["-std=c99"], "header_probe.c"),
    ("c++17", "CXX", "c++", ["-std=c++17", "-x", "c++"], "header_probe.cpp"),
)
WARNING_FLAGS = ["-pedantic", "-Wall", "-Wextra", "-Werror", "-fsyntax-only"]


class UsageError(Exception):
    """A missing tool or an unusable argument: exit status 2."""


def find_tool(variable, default):
    command = os.environ.get(variable) or default
    parts = command.split()
    if not parts or shutil.which(parts[0]) is None:
        raise UsageError(f"check-abi: compiler not found: {command} (set {variable})")
    return parts


def check_header(include_dir, headers):
    """Return the findings of compiling each header alone in C99 and C++17."""
    include_path = pathlib.Path(include_dir)
    if not include_path.is_dir():
        raise UsageError(f"check-abi: include directory not found: {include_dir}")
    tools = {label: find_tool(variable, default)
             for label, variable, default, _flags, _name in HEADER_COMPILES}
    findings = []
    with tempfile.TemporaryDirectory(prefix="abi-header-") as scratch:
        for header in headers:
            if not (include_path / header).is_file():
                findings.append(f"missing header: {header} under {include_dir}")
                continue
            for label, _variable, _default, flags, name in HEADER_COMPILES:
                probe = pathlib.Path(scratch) / name
                probe.write_text(f'#include "{header}"\n', encoding="utf-8")
                command = tools[label] + flags + WARNING_FLAGS + [
                    "-I", str(include_path), str(probe)]
                result = subprocess.run(command, capture_output=True, text=True,
                                        check=False)
                if result.returncode == 0:
                    print(f"abi-header: {header} {label} ok")
                    continue
                findings.append(f"{header} does not compile alone as {label}")
                for line in (result.stdout + result.stderr).splitlines():
                    findings.append(f"  {line}")
    return findings


def run_header(args):
    headers = args.header or [DEFAULT_HEADER]
    findings = check_header(args.include, headers)
    for finding in findings:
        print(finding)
    print(f"check-abi: header findings={len([f for f in findings if not f.startswith('  ')])}")
    return 1 if findings else 0


# ---- classes mode: the status-class table against the header ---------------

CLASS_NAMES = ("ok", "refused", "guest-stop", "defect", "reserved")
RESUME_NAMES = ("resumable", "terminal", "n/a")
CONSTANT_PREFIXES = ("T64_STATUS_", "T64_STOP_REASON_")
CONSTANT_RE = re.compile(r"^\s*#\s*define\s+((?:T64_STATUS_|T64_STOP_REASON_)\w+)\b(.*)$")
VALUE_RE = re.compile(r"^\s*UINT32_C\(\s*(\d+)\s*\)")
ROW_RE = re.compile(
    r"^((?:T64_STATUS_|T64_STOP_REASON_)\w+) :: (\d+) :: (\S+) :: (\S+) :: (\S.*)$")


def read_text(path, what):
    try:
        return pathlib.Path(path).read_text(encoding="utf-8")
    except OSError as error:
        raise UsageError(f"check-abi: cannot read the {what} {path}: {error}") from error


def parse_constants(header_text):
    """Return ({name: value}, findings) for every status and stop-reason define."""
    constants = {}
    findings = []
    for line in header_text.split("\n"):
        found = CONSTANT_RE.match(line)
        if found is None:
            continue
        name, rest = found.group(1), found.group(2)
        value = VALUE_RE.match(rest)
        if value is None:
            findings.append(f"unparsed: {name} is not defined as UINT32_C(n)")
            continue
        if name in constants:
            findings.append(f"duplicate: {name} (defined twice in the header)")
            continue
        constants[name] = int(value.group(1))
    return constants, findings


def parse_rows(table_text):
    """Return ({name: (value, class, resume)}, findings) for the table rows."""
    rows = {}
    findings = []
    reported = set()
    for number, line in enumerate(table_text.split("\n"), start=1):
        if not line.strip() or line.startswith("#"):
            continue
        found = ROW_RE.match(line)
        if found is None:
            findings.append(f"malformed: line {number}: {line}")
            continue
        name, value, class_name, resume, _note = found.groups()
        if name in rows:
            if name not in reported:
                findings.append(f"duplicate: {name}")
                reported.add(name)
            continue
        rows[name] = (int(value), class_name, resume)
    return rows, findings


def check_classes(header_text, table_text):
    constants, findings = parse_constants(header_text)
    rows, row_findings = parse_rows(table_text)
    findings.extend(row_findings)
    for name, value in constants.items():
        if name not in rows:
            findings.append(f"unclassified: {name}")
            continue
        table_value, class_name, resume = rows[name]
        if table_value != value:
            findings.append(
                f"renumbered: {name} header={value} table={table_value}")
        if class_name not in CLASS_NAMES:
            findings.append(f"unknown-class: {name} {class_name}")
        if resume not in RESUME_NAMES:
            findings.append(f"unknown-resume: {name} {resume}")
    for name in rows:
        if name not in constants:
            findings.append(f"orphan: {name} names no constant of the header")
    return constants, rows, findings


def run_classes(args):
    header_text = read_text(args.header, "header")
    table_text = read_text(args.table, "table")
    constants, rows, findings = check_classes(header_text, table_text)
    for finding in findings:
        print(finding)
    print(f"check-abi: classes constants={len(constants)} rows={len(rows)} "
          f"findings={len(findings)}")
    return 1 if findings else 0


# ---- the self-test (compiled-in seeds, run on temporary copies) ------------

ROOT = pathlib.Path(__file__).resolve().parent.parent
SCRIPT = pathlib.Path(__file__).resolve()
REAL_HEADER = ROOT / "core" / "include" / "tendies64" / "t64_machine.h"
REAL_TABLE = ROOT / "docs" / "abi" / "status-classes.txt"


def seed_replace(text, old, new):
    """Replace the one occurrence of old; a seed that finds none is a bug."""
    if text.count(old) != 1:
        raise ValueError(f"seed anchor occurs {text.count(old)} times: {old!r}")
    return text.replace(old, new)


def seed_drop_line(text, prefix):
    """Remove the one line that starts with prefix."""
    lines = text.split("\n")
    hits = [i for i, line in enumerate(lines) if line.startswith(prefix)]
    if len(hits) != 1:
        raise ValueError(f"seed line {prefix!r} occurs {len(hits)} times")
    del lines[hits[0]]
    return "\n".join(lines)


def seed_duplicate_line(text, prefix):
    """Repeat the one line that starts with prefix."""
    lines = text.split("\n")
    hits = [i for i, line in enumerate(lines) if line.startswith(prefix)]
    if len(hits) != 1:
        raise ValueError(f"seed line {prefix!r} occurs {len(hits)} times")
    lines.insert(hits[0], lines[hits[0]])
    return "\n".join(lines)


STATUS_ANCHOR = "#define T64_STATUS_INTERNAL_DEFECT UINT32_C(38)\n"
STOP_ANCHOR = "#define T64_STOP_REASON_GUEST_COMPLETION UINT32_C(8)\n"

# name -> (what is seeded, header seed, table seed, expected exit, expected text)
CASES = {
    "baseline-clean": (
        "the committed header and table", None, None, 0, "findings=0"),
    "unclassified-status": (
        "a status constant the table does not list",
        lambda h: seed_replace(
            h, STATUS_ANCHOR,
            STATUS_ANCHOR + "#define T64_STATUS_SEED UINT32_C(39)\n"),
        None, 1, "unclassified: T64_STATUS_SEED"),
    "unclassified-stop-reason": (
        "a stop reason the table does not list",
        lambda h: seed_replace(
            h, STOP_ANCHOR,
            STOP_ANCHOR + "#define T64_STOP_REASON_SEED UINT32_C(9)\n"),
        None, 1, "unclassified: T64_STOP_REASON_SEED"),
    "duplicate-row": (
        "a table row repeated",
        None,
        lambda t: seed_duplicate_line(t, "T64_STATUS_BUS_RANGE ::"),
        1, "duplicate: T64_STATUS_BUS_RANGE"),
    "renumbered-value": (
        "a status whose header value moved",
        lambda h: seed_replace(
            h, "#define T64_STATUS_QUEUE_FULL UINT32_C(11)",
            "#define T64_STATUS_QUEUE_FULL UINT32_C(40)"),
        None, 1, "renumbered: T64_STATUS_QUEUE_FULL header=40 table=11"),
    "removed-row": (
        "a stop-reason row deleted from the table",
        None,
        lambda t: seed_drop_line(t, "T64_STOP_REASON_HALT ::"),
        1, "unclassified: T64_STOP_REASON_HALT"),
    "orphan-row": (
        "a row that names no constant",
        None,
        lambda t: t.rstrip("\n") + "\nT64_STATUS_GHOST :: 39 :: refused :: n/a :: seeded row\n",
        1, "orphan: T64_STATUS_GHOST"),
    "unknown-class": (
        "a class outside the closed set",
        None,
        lambda t: seed_replace(t, "T64_STATUS_OK :: 0 :: ok ::",
                               "T64_STATUS_OK :: 0 :: fine ::"),
        1, "unknown-class: T64_STATUS_OK fine"),
    "unknown-resume": (
        "a resume value outside the closed set",
        None,
        lambda t: seed_replace(
            t, "T64_STOP_REASON_BOUND :: 1 :: guest-stop :: resumable ::",
            "T64_STOP_REASON_BOUND :: 1 :: guest-stop :: later ::"),
        1, "unknown-resume: T64_STOP_REASON_BOUND later"),
    "malformed-row": (
        "a row without its value and note fields",
        None,
        lambda t: t.rstrip("\n") + "\nT64_STATUS_BROKEN :: not-a-number :: refused\n",
        1, "malformed: line"),
}


def run_case(name):
    """Seed one breakage into temporary copies; return a problem text or None."""
    _what, seed_header, seed_table, want_exit, want_text = CASES[name]
    try:
        header_text = REAL_HEADER.read_text(encoding="utf-8")
        table_text = REAL_TABLE.read_text(encoding="utf-8")
    except OSError as error:
        return f"cannot read the committed inputs: {error}"
    try:
        if seed_header is not None:
            header_text = seed_header(header_text)
        if seed_table is not None:
            table_text = seed_table(table_text)
    except ValueError as error:
        return f"seed did not apply: {error}"
    build_dir = ROOT / "build"
    build_dir.mkdir(exist_ok=True)
    scratch = pathlib.Path(tempfile.mkdtemp(prefix="abi-selftest-", dir=build_dir))
    try:
        header_copy = scratch / "t64_machine.h"
        table_copy = scratch / "status-classes.txt"
        header_copy.write_text(header_text, encoding="utf-8")
        table_copy.write_text(table_text, encoding="utf-8")
        result = subprocess.run(
            [sys.executable, "-B", str(SCRIPT), "classes",
             "--header", str(header_copy), "--table", str(table_copy)],
            capture_output=True, text=True, check=False)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    output = result.stdout + result.stderr
    if result.returncode != want_exit:
        return (f"classes mode exited {result.returncode}, wanted {want_exit}: "
                f"{output.strip().splitlines()[-1] if output.strip() else 'no output'}")
    if want_text not in output:
        return f"output lacks {want_text!r}"
    return None


def run_selftest():
    failed = 0
    for name in CASES:
        problem = run_case(name)
        if problem is None:
            print(f"selftest {name}: ok")
        else:
            print(f"selftest {name}: FAILED: {problem}")
            failed += 1
    print(f"check-abi: selftest cases={len(CASES)} failed={failed}")
    return 1 if failed else 0


def build_parser():
    parser = argparse.ArgumentParser(prog="check-abi.py")
    parser.add_argument("--list-cases", action="store_true",
                        help="print the self-test case names")
    parser.add_argument("--case", metavar="NAME",
                        help="run one self-test case")
    parser.add_argument("--selftest", action="store_true",
                        help="run every self-test case")
    modes = parser.add_subparsers(dest="mode")
    header = modes.add_parser("header", help="compile public headers alone")
    header.add_argument("--include", required=True, metavar="DIR",
                        help="include directory that holds the header")
    header.add_argument("--header", action="append", metavar="NAME",
                        help=f"header path under DIR (default {DEFAULT_HEADER})")
    header.set_defaults(run=run_header)
    classes = modes.add_parser(
        "classes", help="check the status-class table against the header")
    classes.add_argument("--header", required=True, metavar="FILE",
                         help="the public header with the T64_STATUS_* defines")
    classes.add_argument("--table", required=True, metavar="FILE",
                         help="the status-class table")
    classes.set_defaults(run=run_classes)
    return parser


def main(argv):
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        if args.list_cases:
            for name in CASES:
                print(name)
            return 0
        if args.case is not None:
            if args.case not in CASES:
                raise UsageError(f"check-abi: unknown case {args.case}")
            problem = run_case(args.case)
            if problem is None:
                print(f"selftest {args.case}: ok")
                return 0
            print(f"selftest {args.case}: FAILED: {problem}")
            return 1
        if args.selftest:
            return run_selftest()
        if args.mode is None:
            parser.error("a mode or one of --list-cases, --case, --selftest is required")
        return args.run(args)
    except UsageError as error:
        sys.stderr.write(f"{error}\n")
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
