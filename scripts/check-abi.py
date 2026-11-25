#!/usr/bin/env python3
"""ABI checks of the public header (Phase 11, plan 11-02).

Modes:
  header   --include DIR [--header NAME]...
           compile each named public header alone, as C99 and as C++17, with
           the pedantic warning set and -Werror (CTest repo.abi-header).

Standard library only. Run as `python3 -B scripts/check-abi.py ...`.
Exit codes: 0 clean, 1 a finding, 2 usage error or a missing tool.
"""

import argparse
import os
import pathlib
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


def build_parser():
    parser = argparse.ArgumentParser(prog="check-abi.py")
    modes = parser.add_subparsers(dest="mode", required=True)
    header = modes.add_parser("header", help="compile public headers alone")
    header.add_argument("--include", required=True, metavar="DIR",
                        help="include directory that holds the header")
    header.add_argument("--header", action="append", metavar="NAME",
                        help=f"header path under DIR (default {DEFAULT_HEADER})")
    header.set_defaults(run=run_header)
    return parser


def main(argv):
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.run(args)
    except UsageError as error:
        sys.stderr.write(f"{error}\n")
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
