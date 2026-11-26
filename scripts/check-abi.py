#!/usr/bin/env python3
"""ABI checks of the public header (Phase 11, plan 11-02).

Modes:
  header   --include DIR [--header NAME]...
           compile each named public header alone, as C99 and as C++17, with
           the pedantic warning set and -Werror (CTest repo.abi-header).
  classes  --header FILE --table FILE [--consumer-table FILE]
           every T64_STATUS_* and T64_STOP_REASON_* constant of the header
           has exactly one row in the status-class table, with the same value
           and a class from a closed set (CTest repo.abi-status-classes).
           --consumer-table names the hand-written copy of the table of the
           installed consumer (tests/installed-consumer/t64c_classes.h, plan
           11-20): it must have one row per row of the table, with the same
           class and resume, and no row of its own.
  layout   --write FILE [--include DIR] | --check FILE [--include DIR]
           the generated pins of docs/abi/layout.c: sizeof of every public
           struct, offsetof of every member, the value of every integer
           T64_* constant and the size of every string constant. --write
           measures the header under DIR (default core/include) with a probe
           program and writes FILE; --check compiles FILE against DIR as C99,
           C11 and C++17 (pedantic, -Werror) and regenerates it from DIR and
           diffs (CTests repo.abi-layout against core/include and
           install.abi-layout against the installed include directory).
  symbols  --write FILE --lib LIB | --check FILE --lib LIB
           the defined global symbols of the archive LIB (nm -P -g
           --defined-only, Darwin underscore stripped): the names that do not
           start with t64i_ must equal FILE (the public functions, one per
           line), every other name must start with t64i_, and no data symbol
           may exist (CTest install.abi-symbols).
  abspaths --prefix DIR --forbid STRING...
           no file below DIR (the installed tree, the archive included)
           contains any of the forbidden path strings (CTest
           install.abs-paths). The tool reads bytes and needs no strings(1).
Self-test (CTest repo.abi-selftest):
  --list-cases | --case NAME | --selftest
           seeded breakages of temporary copies of the header and the table,
           each of which classes mode must report with its own finding text,
           and seeded layout, symbol-list and path cases.

Standard library only. Run as `python3 -B scripts/check-abi.py ...`.
Exit codes: 0 clean, 1 a finding, 2 usage error or a missing tool.
"""

import argparse
import difflib
import os
import pathlib
import platform
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


# One row of the consumer's X-macro table: X(NAME, T64C_CLASS_x, T64C_RESUME_y).
CONSUMER_ROW_RE = re.compile(
    r"\bX\(\s*(T64_(?:STATUS|STOP_REASON)_\w+)\s*,\s*T64C_CLASS_(\w+)\s*,"
    r"\s*T64C_RESUME_(\w+)\s*\)")
CONSUMER_CLASSES = {"OK": "ok", "REFUSED": "refused", "GUEST_STOP": "guest-stop",
                    "DEFECT": "defect", "RESERVED": "reserved"}
CONSUMER_RESUMES = {"NA": "n/a", "RESUMABLE": "resumable", "TERMINAL": "terminal"}


def parse_consumer_rows(text):
    """Return ({name: (class, resume)}, findings) of the consumer's X-macro rows."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    text = text.replace("\\\n", " ")
    rows = {}
    findings = []
    for name, class_name, resume in CONSUMER_ROW_RE.findall(text):
        if name in rows:
            findings.append(f"consumer-duplicate: {name}")
            continue
        if class_name not in CONSUMER_CLASSES:
            findings.append(f"consumer-unknown-class: {name} {class_name}")
            continue
        if resume not in CONSUMER_RESUMES:
            findings.append(f"consumer-unknown-resume: {name} {resume}")
            continue
        rows[name] = (CONSUMER_CLASSES[class_name], CONSUMER_RESUMES[resume])
    return rows, findings


def check_consumer_table(rows, consumer_text):
    """Findings of the consumer's copy against the table rows (value, class, resume)."""
    consumer_rows, findings = parse_consumer_rows(consumer_text)
    for name, (_value, class_name, resume) in rows.items():
        if name not in consumer_rows:
            findings.append(f"consumer-missing: {name} has no row in the consumer table")
            continue
        seen_class, seen_resume = consumer_rows[name]
        if seen_class != class_name:
            findings.append(f"consumer-class: {name} consumer={seen_class} table={class_name}")
        if seen_resume != resume:
            findings.append(f"consumer-resume: {name} consumer={seen_resume} table={resume}")
    for name in consumer_rows:
        if name not in rows:
            findings.append(f"consumer-orphan: {name} is no row of the table")
    return len(consumer_rows), findings


def run_classes(args):
    header_text = read_text(args.header, "header")
    table_text = read_text(args.table, "table")
    constants, rows, findings = check_classes(header_text, table_text)
    consumer = ""
    if args.consumer_table is not None:
        consumer_count, consumer_findings = check_consumer_table(
            rows, read_text(args.consumer_table, "consumer table"))
        findings.extend(consumer_findings)
        consumer = f" consumer_rows={consumer_count}"
    for finding in findings:
        print(finding)
    print(f"check-abi: classes constants={len(constants)} rows={len(rows)}{consumer} "
          f"findings={len(findings)}")
    return 1 if findings else 0


# ---- layout mode: generated size, offset and constant pins ------------------

ROOT = pathlib.Path(__file__).resolve().parent.parent
SCRIPT = pathlib.Path(__file__).resolve()
REAL_HEADER = ROOT / "core" / "include" / "tendies64" / "t64_machine.h"
REAL_TABLE = ROOT / "docs" / "abi" / "status-classes.txt"
REAL_CONSUMER_TABLE = ROOT / "tests" / "installed-consumer" / "t64c_classes.h"
REAL_LAYOUT = ROOT / "docs" / "abi" / "layout.c"
REAL_SYMBOLS = ROOT / "docs" / "abi" / "public-symbols.txt"
DEFAULT_INCLUDE = ROOT / "core" / "include"
LAYOUT_REGENERATE = "python3 -B scripts/check-abi.py layout --write docs/abi/layout.c"

COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)
STRUCT_RE = re.compile(
    r"typedef\s+struct\s*\{(?P<body>[^{}]*)\}\s*(?P<name>t64_\w+)\s*;")
DEFINE_RE = re.compile(
    r"^[ \t]*#[ \t]*define[ \t]+(T64_\w+)(\()?(?:[ \t]+(.*))?$")
MEMBER_RE = re.compile(r"(\w+)\s*(?:\[[^\]]*\]\s*)*$")
STRING_VALUE_RE = re.compile(r'^"[^"\n]*"$')

# (label, environment variable, default tool, language flags). C99 takes the
# negative-array pin, C11 _Static_assert and C++17 static_assert.
LAYOUT_LANGUAGES = (
    ("c99", "CC", "cc", ["-std=c99"]),
    ("c11", "CC", "cc", ["-std=c11"]),
    ("c++17", "CXX", "c++", ["-std=c++17", "-x", "c++"]),
)

LAYOUT_BANNER = """\
/*
 * GENERATED FILE: do not edit. Written by
 *   %s
 * from the public header tendies64/t64_machine.h.
 *
 * Freeze-candidate record of the public layout (Phase 11, plan 11-17, API-03):
 * sizeof of every public struct, offsetof of every member, the value of every
 * integer T64_* constant and the size of every string constant, as compile-time
 * assertions. The file is compiled with -fsyntax-only against the source
 * include directory (CTest repo.abi-layout) and against the installed include
 * directory (CTest install.abi-layout) as C99, C11 and C++17, and regenerated
 * and diffed on every run, so a changed, added or removed member or constant
 * fails. It is not part of any build target. Re-bind with the command above in
 * a commit that also records the amendment; the pins hold for targets with
 * 64-bit pointers (macOS arm64, Linux x86_64), other data models compile it
 * without checking anything.
 */
#include <stddef.h>
#include <stdint.h>

#include <tendies64/t64_machine.h>

#define T64_LAYOUT_JOIN2(a, b) a##b
#define T64_LAYOUT_JOIN(a, b) T64_LAYOUT_JOIN2(a, b)
#if defined(__cplusplus)
#define T64_LAYOUT_PIN(condition) static_assert(condition, #condition)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define T64_LAYOUT_PIN(condition) _Static_assert(condition, #condition)
#else
/* C99 has no static assertion: a negative array size is the portable form. */
#define T64_LAYOUT_PIN(condition) \\
  typedef char T64_LAYOUT_JOIN(t64_layout_pin_, __LINE__)[(condition) ? 1 : -1]
#endif

#if UINTPTR_MAX == 0xffffffffffffffffu
"""

LAYOUT_FOOTER = """\

#endif /* UINTPTR_MAX == 0xffffffffffffffffu */
"""


def parse_public_layout(header_text):
    """Return (structs, constants, findings) of the public header text.

    structs is [(type name, [member names])] for every `typedef struct {...}
    t64_x;` in header order; constants is [(name, "int" or "string")] for every
    object-like T64_* define that is neither an initializer nor T64_API.
    """
    text = COMMENT_RE.sub(" ", header_text).replace("\\\n", " ")
    findings = []
    structs = []
    for found in STRUCT_RE.finditer(text):
        members = []
        for declaration in found.group("body").split(";"):
            declaration = " ".join(declaration.split())
            if not declaration:
                continue
            member = None if "(" in declaration else MEMBER_RE.search(declaration)
            if member is None:
                findings.append(
                    f"unsupported member declaration in {found.group('name')}: "
                    f"{declaration}")
                continue
            members.append(member.group(1))
        structs.append((found.group("name"), members))
    constants = []
    seen = set()
    for line in text.split("\n"):
        found = DEFINE_RE.match(line)
        if found is None:
            continue
        name, function_like, value = found.group(1), found.group(2), (found.group(3) or "").strip()
        if (function_like or not value or value.startswith("{") or name == "T64_API"
                or name.endswith(("_INIT", "_INIT_WITH")) or name in seen):
            continue
        seen.add(name)
        constants.append((name, "string" if STRING_VALUE_RE.match(value) else "int"))
    return structs, constants, findings


def probe_source(structs, constants):
    lines = ["#include <stdio.h>", "#include <stddef.h>", "#include <stdint.h>",
             "#include <tendies64/t64_machine.h>", "int main(void) {"]
    for name, members in structs:
        lines.append(f'  printf("size {name} %llu\\n", '
                     f'(unsigned long long)sizeof({name}));')
        for member in members:
            lines.append(f'  printf("offset {name} {member} %llu\\n", '
                         f'(unsigned long long)offsetof({name}, {member}));')
    for name, kind in constants:
        if kind == "string":
            lines.append(f'  printf("strsize {name} %llu\\n", '
                         f'(unsigned long long)sizeof({name}));')
        else:
            lines.append(f'  printf("const {name} %lld\\n", (long long)({name}));')
    lines += ["  return 0;", "}", ""]
    return "\n".join(lines)


def measure_layout(include_dir, scratch):
    """Compile and run the probe; return (records, findings).

    A record is a tuple whose first item is size, offset, strsize or const.
    """
    include_path = pathlib.Path(include_dir)
    header = include_path / DEFAULT_HEADER
    if not header.is_file():
        raise UsageError(f"check-abi: header not found: {header}")
    structs, constants, findings = parse_public_layout(
        header.read_text(encoding="utf-8"))
    if not structs or not constants:
        raise UsageError(f"check-abi: no structs or constants parsed from {header}")
    compiler = find_tool("CC", "cc")
    source = pathlib.Path(scratch) / "layout_probe.c"
    program = pathlib.Path(scratch) / "layout_probe"
    source.write_text(probe_source(structs, constants), encoding="utf-8")
    built = subprocess.run(
        compiler + ["-std=c99", "-I", str(include_path), str(source), "-o", str(program)],
        capture_output=True, text=True, check=False)
    if built.returncode != 0:
        findings.append("the layout probe does not compile against the header")
        findings.extend(f"  {line}" for line in (built.stdout + built.stderr).splitlines()[:12])
        return [], findings
    ran = subprocess.run([str(program)], capture_output=True, text=True, check=False)
    if ran.returncode != 0:
        findings.append(f"the layout probe exited {ran.returncode}")
        return [], findings
    records = [tuple(line.split()) for line in ran.stdout.splitlines() if line.strip()]
    return records, findings


def render_layout(records):
    """The text of layout.c for the measured records."""
    out = [(LAYOUT_BANNER % LAYOUT_REGENERATE).rstrip("\n")]
    previous = None
    for record in records:
        kind = record[0]
        if kind == "size":
            out.append(f"\n/* {record[1]} */")
            out.append(f"T64_LAYOUT_PIN(sizeof({record[1]}) == {record[2]}u);")
        elif kind == "offset":
            out.append(f"T64_LAYOUT_PIN(offsetof({record[1]}, {record[2]}) == {record[3]}u);")
        else:
            if previous in ("size", "offset"):
                out.append("\n/* constants: values of the integer T64_* defines, size of the string ones */")
            if kind == "strsize":
                out.append(f"T64_LAYOUT_PIN(sizeof({record[1]}) == {record[2]}u);")
            else:
                out.append(f"T64_LAYOUT_PIN((long long)({record[1]}) == {record[2]}ll);")
        previous = kind
    out.append(LAYOUT_FOOTER)
    return "\n".join(out)


def generate_layout(include_dir):
    """Return (layout text or None, findings, counts) for the header under DIR."""
    with tempfile.TemporaryDirectory(prefix="abi-layout-") as scratch:
        records, findings = measure_layout(include_dir, scratch)
    if not records:
        return None, findings, (0, 0, 0)
    counts = (sum(1 for r in records if r[0] == "size"),
              sum(1 for r in records if r[0] == "offset"),
              sum(1 for r in records if r[0] in ("const", "strsize")))
    return render_layout(records), findings, counts


def compile_layout(layout_file, include_dir):
    """Findings of compiling the layout file against DIR in every language."""
    include_path = pathlib.Path(include_dir)
    findings = []
    for label, variable, default, flags in LAYOUT_LANGUAGES:
        command = (find_tool(variable, default) + flags + WARNING_FLAGS
                   + ["-I", str(include_path), str(layout_file)])
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        if result.returncode == 0:
            print(f"abi-layout: {pathlib.Path(layout_file).name} {label} ok")
            continue
        findings.append(f"layout pins fail to compile as {label}")
        findings.extend(f"  {line}" for line in (result.stdout + result.stderr).splitlines()[:10])
    return findings


def check_layout(layout_file, include_dir):
    """Findings of the compile of FILE against DIR plus the regeneration diff."""
    layout_path = pathlib.Path(layout_file)
    if not layout_path.is_file():
        raise UsageError(f"check-abi: layout file not found: {layout_file}")
    if not pathlib.Path(include_dir).is_dir():
        raise UsageError(f"check-abi: include directory not found: {include_dir}")
    findings = compile_layout(layout_path, include_dir)
    generated, generate_findings, counts = generate_layout(include_dir)
    findings.extend(generate_findings)
    if generated is not None:
        committed = layout_path.read_text(encoding="utf-8")
        if committed != generated:
            findings.append("layout file differs from the header it pins; re-bind with: "
                            + LAYOUT_REGENERATE)
            delta = difflib.unified_diff(committed.splitlines(), generated.splitlines(),
                                         "committed", "regenerated", lineterm="", n=0)
            findings.extend(f"  {line}" for line in list(delta)[:14])
        else:
            print(f"abi-layout: regenerated file is identical "
                  f"(structs={counts[0]} members={counts[1]} constants={counts[2]})")
    return findings


def run_layout(args):
    include_dir = args.include or str(DEFAULT_INCLUDE)
    if args.write is not None:
        generated, findings, counts = generate_layout(include_dir)
        for finding in findings:
            print(finding)
        if generated is None or findings:
            print(f"check-abi: layout write findings={len(findings)}")
            return 1
        pathlib.Path(args.write).write_text(generated, encoding="utf-8")
        print(f"check-abi: layout wrote {args.write} "
              f"(structs={counts[0]} members={counts[1]} constants={counts[2]})")
        return 0
    findings = check_layout(args.check, include_dir)
    for finding in findings:
        print(finding)
    print(f"check-abi: layout findings={len([f for f in findings if not f.startswith('  ')])}")
    return 1 if findings else 0


# ---- symbols mode: nm golden of the installed archive -------------------------

INTERNAL_PREFIX = "t64i_"


def archive_symbols(library, nm_tool):
    """[(name, nm type)] of the defined global symbols of the archive."""
    parts = nm_tool.split()
    if not parts or shutil.which(parts[0]) is None:
        raise UsageError(f"check-abi: nm not found: {nm_tool} (set NM)")
    if not pathlib.Path(library).is_file():
        raise UsageError(f"check-abi: archive not found: {library}")
    result = subprocess.run(parts + ["-P", "-g", "--defined-only", str(library)],
                            capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise UsageError(f"check-abi: nm failed on {library}: {result.stderr.strip()}")
    return parse_nm(result.stdout, platform.system() == "Darwin")


def parse_nm(text, darwin):
    """Parse `nm -P` text of an archive: member header lines are skipped."""
    entries = []
    for line in text.split("\n"):
        fields = line.split()
        if len(fields) < 3 or not re.fullmatch(r"[A-Za-z]", fields[1]):
            continue
        name = fields[0]
        if darwin and name.startswith("_"):
            name = name[1:]
        entries.append((name, fields[1]))
    return entries


def check_symbols(entries, golden):
    """Findings for the defined globals against the golden public list."""
    findings = []
    public = set()
    for name, kind in sorted(set(entries)):
        if kind != "T":
            findings.append(f"data or non-text global symbol: {name} (nm type {kind})")
        elif not name.startswith(INTERNAL_PREFIX):
            public.add(name)
    golden_set = set(golden)
    if len(golden_set) != len(golden):
        findings.append("the golden list has a duplicated name")
    if golden != sorted(golden):
        findings.append("the golden list is not sorted (LC_ALL=C order)")
    for name in sorted(public - golden_set):
        findings.append(f"unexpected global: {name} (neither a golden public name "
                        f"nor prefixed {INTERNAL_PREFIX})")
    for name in sorted(golden_set - public):
        findings.append(f"missing public symbol: {name} (in the golden, not defined)")
    return findings


def read_golden(path):
    try:
        text = pathlib.Path(path).read_text(encoding="utf-8")
    except OSError as error:
        raise UsageError(f"check-abi: cannot read the golden {path}: {error}") from error
    return [line for line in text.split("\n") if line]


def run_symbols(args):
    entries = archive_symbols(args.lib, os.environ.get("NM") or "nm")
    if args.write is not None:
        public = sorted({name for name, kind in entries
                         if kind == "T" and not name.startswith(INTERNAL_PREFIX)})
        data = [e for e in entries if e[1] != "T"]
        if data:
            for name, kind in data:
                print(f"data or non-text global symbol: {name} (nm type {kind})")
            return 1
        pathlib.Path(args.write).write_text("\n".join(public) + "\n", encoding="utf-8")
        print(f"check-abi: symbols wrote {args.write} ({len(public)} public names)")
        return 0
    findings = check_symbols(entries, read_golden(args.check))
    for finding in findings:
        print(finding)
    internal = sum(1 for name, kind in set(entries)
                   if kind == "T" and name.startswith(INTERNAL_PREFIX))
    print(f"check-abi: symbols public={len(read_golden(args.check))} internal={internal} "
          f"findings={len(findings)}")
    return 1 if findings else 0


# ---- abspaths mode: no private path in the installed tree ----------------------

# Generic prefixes that are printed by value; every other forbidden string (the
# source and build roots) is reported by its position on the command line so a
# path never reaches a log.
GENERIC_FORBIDDEN = ("/Users/", "/home/", "/private/", "/var/folders")


def scan_paths(root, forbidden):
    """Return (files scanned, findings): files of ROOT that contain a string."""
    root_path = pathlib.Path(root)
    findings = []
    scanned = 0
    for path in sorted(root_path.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        scanned += 1
        data = path.read_bytes()
        for index, needle in enumerate(forbidden, start=1):
            if needle and needle.encode("utf-8") in data:
                shown = needle if needle in GENERIC_FORBIDDEN else f"forbidden string #{index}"
                findings.append(f"{path.relative_to(root_path).as_posix()} contains {shown}")
    return scanned, findings


def run_abspaths(args):
    prefix = pathlib.Path(args.prefix)
    if not prefix.is_dir():
        raise UsageError(f"check-abi: install prefix not found: {args.prefix}")
    if not (prefix / "lib" / "libt64_core.a").is_file():
        raise UsageError("check-abi: the install prefix holds no lib/libt64_core.a "
                         "(an empty scan proves nothing)")
    scanned, findings = scan_paths(prefix, args.forbid)
    for finding in findings:
        print(finding)
    print(f"check-abi: abspaths files={scanned} forbidden={len(args.forbid)} "
          f"findings={len(findings)}")
    return 1 if findings else 0


# ---- the self-test (compiled-in seeds, run on temporary copies) ------------


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


def scratch_dir(prefix):
    build_dir = ROOT / "build"
    build_dir.mkdir(exist_ok=True)
    return pathlib.Path(tempfile.mkdtemp(prefix=prefix, dir=build_dir))


def lacks(findings, *wanted):
    """A problem text when a wanted finding fragment is missing, else None."""
    text = "\n".join(findings)
    missing = [w for w in wanted if w not in text]
    if missing:
        return f"findings lack {missing}: {findings[:3]}"
    return None


def case_layout_offset_edited():
    """One offset of a temporary copy of layout.c is wrong: layout check fails."""
    text = REAL_LAYOUT.read_text(encoding="utf-8")
    edited, count = re.subn(
        r"(offsetof\(t64_machine_create_info, allocator\) == )(\d+)u",
        lambda m: f"{m.group(1)}{int(m.group(2)) + 8}u", text)
    if count != 1:
        return f"seed anchor occurs {count} times"
    scratch = scratch_dir("abi-layout-selftest-")
    try:
        copy = scratch / "layout.c"
        copy.write_text(edited, encoding="utf-8")
        return lacks(check_layout(copy, DEFAULT_INCLUDE),
                     "layout pins fail to compile as c99",
                     "layout pins fail to compile as c11",
                     "layout pins fail to compile as c++17",
                     "layout file differs from the header it pins")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def case_layout_header_drift():
    """A member added to one struct of a temporary header copy: layout check fails."""
    text = REAL_HEADER.read_text(encoding="utf-8")
    drifted = seed_replace(text, "} t64_save_info;",
                           "  uint32_t seeded_member;\n} t64_save_info;")
    scratch = scratch_dir("abi-layout-selftest-")
    try:
        include = scratch / "include" / "tendies64"
        include.mkdir(parents=True)
        (include / "t64_machine.h").write_text(drifted, encoding="utf-8")
        return lacks(check_layout(REAL_LAYOUT, scratch / "include"),
                     "layout pins fail to compile as c99",
                     "layout file differs from the header it pins",
                     "seeded_member")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def case_symbols_controls():
    """The golden comparison on synthetic nm text: one clean run, four breakages."""
    golden = ["t64_alpha", "t64_beta"]
    base = "lib.a[x.o]:\n_t64_alpha T 0 0\n_t64_beta T 10 0\n_t64i_inner T 20 0\n"
    clean = check_symbols(parse_nm(base, True), golden)
    if clean:
        return f"the clean control reported {clean}"
    seeds = (
        ("unexpected global: helper", base + "_helper T 30 0\n", golden),
        ("missing public symbol: t64_beta", base.replace("_t64_beta T 10 0\n", ""), golden),
        ("data or non-text global symbol: t64i_table", base + "_t64i_table D 40 0\n", golden),
        ("not sorted", base, ["t64_beta", "t64_alpha"]),
    )
    for wanted, text, seed_golden in seeds:
        problem = lacks(check_symbols(parse_nm(text, True), seed_golden), wanted)
        if problem is not None:
            return f"seed {wanted!r}: {problem}"
    if parse_nm("lib.a[x.o]:\nt64_alpha T 0 0\n", False) != [("t64_alpha", "T")]:
        return "the non-Darwin parse changed a name"
    return None


def case_abspaths_controls():
    """The path scan on seeded trees: hits by position, generic ones by value."""
    scratch = scratch_dir("abi-paths-selftest-")
    try:
        secret = "/seeded/private-root"
        tree = scratch / "tree"
        (tree / "lib").mkdir(parents=True)
        (tree / "include").mkdir()
        (tree / "lib" / "libt64_core.a").write_bytes(b"\x00\x01" + secret.encode() + b"/x.c\x00")
        (tree / "include" / "h.h").write_text("/* the /Users/ prefix */\n", encoding="utf-8")
        scanned, findings = scan_paths(tree, [secret, "/Users/", "/nowhere/"])
        problem = lacks(findings, "lib/libt64_core.a contains forbidden string #1",
                        "include/h.h contains /Users/")
        if problem is not None:
            return problem
        if scanned != 2 or len(findings) != 2 or any(secret in f for f in findings):
            return f"unexpected scan result: scanned={scanned} findings={findings}"
        (tree / "lib" / "libt64_core.a").write_bytes(b"clean")
        (tree / "include" / "h.h").write_text("/* clean */\n", encoding="utf-8")
        if scan_paths(tree, [secret, "/Users/"])[1]:
            return "the clean control reported a finding"
        empty = scratch / "empty"
        empty.mkdir()
        try:
            run_abspaths(argparse.Namespace(prefix=str(empty), forbid=["/x"]))
        except UsageError:
            return None
        return "an empty install prefix did not fail as a usage error"
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def case_consumer_table_controls():
    """The consumer's copy against the table: one clean run, six seeded breakages."""
    try:
        header_text = REAL_HEADER.read_text(encoding="utf-8")
        table_text = REAL_TABLE.read_text(encoding="utf-8")
        consumer_text = REAL_CONSUMER_TABLE.read_text(encoding="utf-8")
    except OSError as error:
        return f"cannot read the committed inputs: {error}"
    constants, rows, header_findings = check_classes(header_text, table_text)
    if header_findings:
        return f"the committed header and table are not clean: {header_findings[:2]}"
    count, clean = check_consumer_table(rows, consumer_text)
    if clean or count != len(constants):
        return f"the clean control reported {clean[:2]} with {count} rows of {len(constants)}"
    seeds = (
        ("consumer-missing: T64_STATUS_BUS_RANGE",
         seed_drop_line(consumer_text, "  X(T64_STATUS_BUS_RANGE,")),
        ("consumer-missing: T64_STOP_REASON_QUIESCENT",
         seed_drop_line(consumer_text, "  X(T64_STOP_REASON_QUIESCENT,")),
        ("consumer-class: T64_STATUS_OK consumer=refused table=ok",
         seed_replace(consumer_text, "X(T64_STATUS_OK, T64C_CLASS_OK,",
                      "X(T64_STATUS_OK, T64C_CLASS_REFUSED,")),
        ("consumer-resume: T64_STATUS_SEQUENCE_EXHAUSTED consumer=n/a table=terminal",
         seed_replace(consumer_text,
                      "X(T64_STATUS_SEQUENCE_EXHAUSTED, T64C_CLASS_REFUSED, T64C_RESUME_TERMINAL)",
                      "X(T64_STATUS_SEQUENCE_EXHAUSTED, T64C_CLASS_REFUSED, T64C_RESUME_NA)")),
        ("consumer-orphan: T64_STATUS_GHOST",
         seed_replace(consumer_text, "  X(T64_STATUS_OK, T64C_CLASS_OK, T64C_RESUME_NA) ",
                      "  X(T64_STATUS_GHOST, T64C_CLASS_REFUSED, T64C_RESUME_NA)\\\n"
                      "  X(T64_STATUS_OK, T64C_CLASS_OK, T64C_RESUME_NA) ")),
        ("consumer-duplicate: T64_STATUS_BUS_RANGE",
         seed_duplicate_line(consumer_text, "  X(T64_STATUS_BUS_RANGE,")),
    )
    for wanted, text in seeds:
        problem = lacks(check_consumer_table(rows, text)[1], wanted)
        if problem is not None:
            return f"seed {wanted!r}: {problem}"
    return None


# name -> (what is seeded, runner returning a problem text or None)
EXTRA_CASES = {
    "consumer-table-controls": (
        "the consumer's class table: a missing, changed, orphan and duplicate row",
        case_consumer_table_controls),
    "layout-offset-edited": (
        "an offset of the layout file edited", case_layout_offset_edited),
    "layout-header-drift": (
        "a member added to a struct of the header", case_layout_header_drift),
    "symbols-controls": (
        "synthetic nm text: clean, extra, missing, data and unsorted",
        case_symbols_controls),
    "abspaths-controls": (
        "seeded path strings in an install tree", case_abspaths_controls),
}


def run_extra_case(name):
    return EXTRA_CASES[name][1]()


def run_selftest():
    failed = 0
    for name in CASES:
        problem = run_case(name)
        if problem is None:
            print(f"selftest {name}: ok")
        else:
            print(f"selftest {name}: FAILED: {problem}")
            failed += 1
    for name in EXTRA_CASES:
        problem = run_extra_case(name)
        if problem is None:
            print(f"selftest {name}: ok")
        else:
            print(f"selftest {name}: FAILED: {problem}")
            failed += 1
    print(f"check-abi: selftest cases={len(CASES) + len(EXTRA_CASES)} failed={failed}")
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
    classes.add_argument("--consumer-table", metavar="FILE",
                         help="the consumer's own copy of the table (t64c_classes.h)")
    classes.set_defaults(run=run_classes)
    layout = modes.add_parser(
        "layout", help="write or check the generated layout pins")
    layout_action = layout.add_mutually_exclusive_group(required=True)
    layout_action.add_argument("--write", metavar="FILE",
                               help="measure the header and write the layout file")
    layout_action.add_argument("--check", metavar="FILE",
                               help="compile and regenerate-and-diff the layout file")
    layout.add_argument("--include", metavar="DIR",
                        help="include directory that holds tendies64/t64_machine.h "
                             "(default core/include)")
    layout.set_defaults(run=run_layout)
    symbols = modes.add_parser(
        "symbols", help="write or check the nm golden of an archive")
    symbols_action = symbols.add_mutually_exclusive_group(required=True)
    symbols_action.add_argument("--write", metavar="FILE",
                                help="write the public names of the archive")
    symbols_action.add_argument("--check", metavar="FILE",
                                help="compare the archive with the golden file")
    symbols.add_argument("--lib", required=True, metavar="LIB",
                         help="the static archive (nm -P -g --defined-only)")
    symbols.set_defaults(run=run_symbols)
    abspaths = modes.add_parser(
        "abspaths", help="no private path string below an install prefix")
    abspaths.add_argument("--prefix", required=True, metavar="DIR",
                          help="the installed tree (holds lib/libt64_core.a)")
    abspaths.add_argument("--forbid", required=True, nargs="+", metavar="STRING",
                          help="forbidden path strings; roots are reported by position")
    abspaths.set_defaults(run=run_abspaths)
    return parser


def main(argv):
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        if args.list_cases:
            for name in list(CASES) + list(EXTRA_CASES):
                print(name)
            return 0
        if args.case is not None:
            if args.case not in CASES and args.case not in EXTRA_CASES:
                raise UsageError(f"check-abi: unknown case {args.case}")
            problem = (run_case(args.case) if args.case in CASES
                       else run_extra_case(args.case))
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
