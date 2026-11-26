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
  freeze   --record FILE --prefix DIR [--goldens DIR] [--nm-file FILE] [--write]
           the bound freeze-candidate record (plan 11-23, API-03): recompute
           the four goldens from the installed tree under DIR (the SHA-256 of
           the installed headers after LF normalisation, the public symbols of
           the installed archive, the regenerated layout pins, the status
           classes against the installed header), compare each with its golden
           file in the goldens directory (default: the directory of FILE) and
           each golden's own SHA-256 and the counts, versions and padding holes
           with the machine-read block of the record FILE; any difference is
           printed as "freeze-record: KEY differs" with the re-bind procedure
           (CTest repo.abi-freeze-record). --write rewrites header.sha256 and
           the block of FILE after the three other goldens match the tree.
           --nm-file replaces the nm run on the archive by a listing file.
           The ABI minor of the installed header must equal 1 plus the
           dated rows of the amendment log (a candidate record), or, when
           the block holds bound_minor and candidate_amendments (a bound
           record, plan 12-13), bound_minor plus the rows made after the
           bind (rows minus candidate_amendments).
Self-test (CTest repo.abi-selftest):
  --list-cases | --case NAME | --selftest
           seeded breakages of temporary copies of the header and the table,
           each of which classes mode must report with its own finding text,
           and seeded layout, symbol-list and path cases.
Freeze self-test (CTest repo.abi-freeze-selftest):
  --selftest-prefix DIR with --freeze-selftest | --case freeze-NAME
           seeded incompatible changes of temporary copies of the installed
           tree, the goldens and the record; each must make freeze mode fail
           with its own text, the clean control and the CRLF control must pass.

Standard library only. Run as `python3 -B scripts/check-abi.py ...`.
Exit codes: 0 clean, 1 a finding, 2 usage error or a missing tool.
"""

import argparse
import contextlib
import difflib
import hashlib
import io
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
            lines.append(f'  printf("msize {name} {member} %llu\\n", '
                         f'(unsigned long long)sizeof((({name} *)0)->{member}));')
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
        if kind == "msize":
            continue  # member sizes feed the padding table of freeze mode only
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


def measure_records(include_dir):
    """Return (records, findings): the probe measurement of the header under DIR."""
    with tempfile.TemporaryDirectory(prefix="abi-layout-") as scratch:
        return measure_layout(include_dir, scratch)


def layout_counts(records):
    """(structs, members, constants) of a probe measurement."""
    return (sum(1 for r in records if r[0] == "size"),
            sum(1 for r in records if r[0] == "offset"),
            sum(1 for r in records if r[0] in ("const", "strsize")))


def generate_layout(include_dir):
    """Return (layout text or None, findings, counts) for the header under DIR."""
    records, findings = measure_records(include_dir)
    if not records:
        return None, findings, (0, 0, 0)
    return render_layout(records), findings, layout_counts(records)


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
    return parse_nm(nm_listing(library, nm_tool), platform.system() == "Darwin")


def nm_listing(library, nm_tool):
    """The text of `nm -P -g --defined-only` over the archive LIB."""
    parts = nm_tool.split()
    if not parts or shutil.which(parts[0]) is None:
        raise UsageError(f"check-abi: nm not found: {nm_tool} (set NM)")
    if not pathlib.Path(library).is_file():
        raise UsageError(f"check-abi: archive not found: {library}")
    result = subprocess.run(parts + ["-P", "-g", "--defined-only", str(library)],
                            capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise UsageError(f"check-abi: nm failed on {library}: {result.stderr.strip()}")
    return result.stdout


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


# ---- freeze mode: the bound freeze-candidate record ---------------------------

# The record docs/abi/FREEZE-CANDIDATE.md holds one fenced block of "key: value"
# lines (the machine-read block). Four goldens sit beside it in docs/abi: the
# manifest of the installed headers (header.sha256), the public symbol list, the
# generated layout pins and the status-class table. Freeze mode recomputes what
# can be recomputed from the installed prefix, compares it with each golden, each
# golden's own SHA-256 and the counts of the block, and fails on any difference.

FREEZE_FENCE = "```freeze-record"
FREEZE_CLOSE = "```"
# A record without bound keys is the ABI 0.1 candidate; every amendment adds one.
FREEZE_CANDIDATE_MINOR = 1
REAL_RECORD = ROOT / "docs" / "abi" / "FREEZE-CANDIDATE.md"
REAL_ABI_DIR = ROOT / "docs" / "abi"
GOLDEN_FILES = (("header", "header.sha256"),
                ("public_symbols", "public-symbols.txt"),
                ("layout", "layout.c"),
                ("status_classes", "status-classes.txt"))
SCALAR_KEYS = ("abi_version", "package_version", "amendments",
               "header_sha256", "public_symbols_sha256", "layout_sha256",
               "status_classes_sha256", "public_function_count", "status_count",
               "stop_reason_count", "layout_structs", "layout_members",
               "layout_constants", "padding_holes")
LIST_KEYS = ("padding_hole",)
# Present together only in a record that has been bound (Phase 12, plan 12-13; DR-25):
# the ABI minor the bind fixed and the number of log rows made before it.
BOUND_KEYS = ("bound_minor", "candidate_amendments")
RECORD_LINE_RE = re.compile(r"^([a-z0-9_]+): (\S.*)$")
AMENDMENT_ROW_RE = re.compile(r"^\|\s*\d{4}-\d{2}-\d{2}\s*\|", re.MULTILINE)
VERSION_PART_RE = re.compile(
    r"^#\s*define\s+T64_ABI_VERSION_(MAJOR|MINOR)\s+UINT32_C\(\s*(\d+)\s*\)",
    re.MULTILINE)
PACKAGE_VERSION_RE = re.compile(r'set\(PACKAGE_VERSION\s+"([^"]+)"\)')
PACKAGE_VERSION_FILE = pathlib.Path("lib") / "cmake" / "tendies64" / "tendies64ConfigVersion.cmake"
REBIND_PROCEDURE = (
    "freeze-record: this record is the contract; a change of any bound element is an",
    "  amendment, not an edit. Procedure: (1) evidence of the defect; (2) regenerate the",
    "  changed golden with its generator (layout --write, symbols --write, or edit the",
    "  class table) and add 1 to T64_ABI_VERSION_MINOR and the package version;",
    "  (3) rebuild, install, and re-run the installed-consumer suite; (4) run",
    "  python3 -B scripts/check-abi.py freeze --record docs/abi/FREEZE-CANDIDATE.md",
    "  --prefix <installed prefix> --write; (5) add an amendment row (date | golden |",
    "  change | defect evidence | consumer suite run | new minor) to the record.",
)

_MEASURE_CACHE = {}


def sha256_hex(data):
    return hashlib.sha256(data).hexdigest()


def canonical_bytes(data):
    """The bytes with CRLF and lone CR line endings turned into LF."""
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def header_manifest(prefix):
    """(manifest text, header count) of every file below PREFIX/include.

    One line per file, "sha256  include/<posix path>", sorted by path, the hash
    taken after the line endings of the file are normalised to LF.
    """
    include = pathlib.Path(prefix) / "include"
    if not include.is_dir():
        raise UsageError(f"check-abi: no include directory below the prefix: {prefix}")
    entries = sorted((p.relative_to(include).as_posix(), p)
                     for p in include.rglob("*") if p.is_file())
    lines = [f"{sha256_hex(canonical_bytes(p.read_bytes()))}  include/{name}\n"
             for name, p in entries]
    return "".join(lines), len(lines)


def read_installed(path, what):
    try:
        return pathlib.Path(path).read_bytes()
    except OSError as error:
        raise UsageError(f"check-abi: cannot read the {what} {path}: {error}") from error


def padding_holes(records):
    """Sorted "struct after-member offset bytes" rows of the implicit padding.

    A hole is the gap between a member's end and the next member (or the end of
    the struct), measured by the probe on the target the check runs on.
    """
    sizes, members = {}, {}
    for record in records:
        if record[0] == "size":
            sizes[record[1]] = int(record[2])
            members[record[1]] = []
        elif record[0] == "offset":
            members[record[1]].append([record[2], int(record[3]), 0])
        elif record[0] == "msize":
            members[record[1]][-1][2] = int(record[3])
    holes = []
    for name, size in sizes.items():
        rows = members[name]
        for index, (member, offset, width) in enumerate(rows):
            end = offset + width
            following = rows[index + 1][1] if index + 1 < len(rows) else size
            if following > end:
                holes.append(f"{name} {member} {end} {following - end}")
    return sorted(holes)


def measure_install(prefix, nm_text):
    """Everything freeze mode recomputes from the installed tree under PREFIX."""
    header_path = pathlib.Path(prefix) / "include" / DEFAULT_HEADER
    header_bytes = read_installed(header_path, "installed header")
    header_text = canonical_bytes(header_bytes).decode("utf-8")
    manifest, header_count = header_manifest(prefix)
    version = {key: int(value) for key, value in VERSION_PART_RE.findall(header_text)}
    if len(version) != 2:
        raise UsageError("check-abi: the installed header defines no "
                         "T64_ABI_VERSION_MAJOR and T64_ABI_VERSION_MINOR")
    package = PACKAGE_VERSION_RE.search(
        read_installed(pathlib.Path(prefix) / PACKAGE_VERSION_FILE,
                       "installed package version file").decode("utf-8"))
    if package is None:
        raise UsageError("check-abi: the installed package version file sets no PACKAGE_VERSION")
    if nm_text is None:
        nm_text = nm_listing(pathlib.Path(prefix) / "lib" / "libt64_core.a",
                             os.environ.get("NM") or "nm")
    key = sha256_hex(header_bytes)
    if key not in _MEASURE_CACHE:
        _MEASURE_CACHE[key] = measure_records(pathlib.Path(prefix) / "include")
    records, layout_findings = _MEASURE_CACHE[key]
    constants, _classes_findings = parse_constants(header_text)
    return {
        "manifest": manifest,
        "header_count": header_count,
        "header_text": header_text,
        "abi_version": f"{version['MAJOR']}.{version['MINOR']}",
        "abi_minor": version["MINOR"],
        "package_version": package.group(1),
        "entries": parse_nm(nm_text, platform.system() == "Darwin"),
        "records": records,
        "layout_findings": layout_findings,
        "layout_text": render_layout(records) if records else None,
        "layout_counts": layout_counts(records),
        "holes": padding_holes(records),
        "status_count": sum(1 for n in constants if n.startswith("T64_STATUS_")),
        "stop_reason_count": sum(1 for n in constants if n.startswith("T64_STOP_REASON_")),
    }


def parse_record(text):
    """Return ({key: [values]}, findings) of the machine-read block of the record."""
    lines = text.split("\n")
    opens = [i for i, line in enumerate(lines) if line.rstrip() == FREEZE_FENCE]
    if len(opens) != 1:
        return {}, [f"the record has {len(opens)} fenced blocks of kind freeze-record, wanted 1"]
    values, findings = {}, []
    for line in lines[opens[0] + 1:]:
        if line.rstrip() == FREEZE_CLOSE:
            break
        if not line.strip():
            continue
        found = RECORD_LINE_RE.match(line)
        if found is None:
            findings.append(f"not a key: value line: {line}")
            continue
        key, value = found.group(1), found.group(2).strip()
        if key not in SCALAR_KEYS and key not in LIST_KEYS and key not in BOUND_KEYS:
            findings.append(f"unknown key: {key}")
        elif key in values and key not in LIST_KEYS:
            findings.append(f"duplicate key: {key}")
        else:
            values.setdefault(key, []).append(value)
    else:
        findings.append("the block is not closed")
    return values, findings


def diff_lines(bound, found, limit=6):
    delta = difflib.unified_diff(bound.splitlines(), found.splitlines(),
                                 "bound", "installed", lineterm="", n=0)
    return [f"  {line}" for line in list(delta)[2:2 + limit]]


def golden_files(goldens, optional=()):
    """{element: bytes} of the four golden files in the directory GOLDENS.

    A file of an element in OPTIONAL may be missing (read as empty): --write
    creates header.sha256.
    """
    files = {}
    for element, name in GOLDEN_FILES:
        path = pathlib.Path(goldens) / name
        files[element] = (b"" if element in optional and not path.exists()
                          else read_installed(path, f"golden {name}"))
    return files


def content_findings(measured, golden, check_header):
    """Findings of the installed tree against the golden file texts.

    check_header is false when --write is about to rewrite header.sha256.
    """
    findings = []

    def differs(key, detail):
        findings.append(f"freeze-record: {key} differs: {detail}")

    if check_header and golden["header"].decode("utf-8") != measured["manifest"]:
        differs("header_sha256",
                "the installed headers do not hash to the bound manifest "
                "docs/abi/header.sha256 (any edit of an installed header is an amendment)")
        findings.extend(diff_lines(golden["header"].decode("utf-8"), measured["manifest"]))
    names = read_golden_text(golden["public_symbols"])
    for finding in check_symbols(measured["entries"], names):
        differs("public_symbols", finding)
    for finding in measured["layout_findings"]:
        differs("layout", finding)
    if measured["layout_text"] is not None and measured["layout_text"] != golden["layout"].decode("utf-8"):
        differs("layout",
                "docs/abi/layout.c does not match the layout of the installed header")
        findings.extend(diff_lines(golden["layout"].decode("utf-8"), measured["layout_text"], 8))
    _constants, _rows, class_findings = check_classes(
        measured["header_text"], canonical_bytes(golden["status_classes"]).decode("utf-8"))
    for finding in class_findings:
        differs("classes", finding)
    return findings


def read_golden_text(data):
    return [line for line in data.decode("utf-8").split("\n") if line]


def block_values(measured, golden, amendments, bound=()):
    """The (key, value) lines of the machine-read block for the measured tree.

    BOUND is the (key, value) pairs of a bound record, kept as they are.
    """
    names = read_golden_text(golden["public_symbols"])
    structs, members, constants = measured["layout_counts"]
    pairs = [
        ("abi_version", measured["abi_version"]),
        ("package_version", measured["package_version"]),
        ("amendments", str(amendments)),
        *bound,
        ("header_sha256", sha256_hex(golden["header"])),
        ("public_symbols_sha256", sha256_hex(golden["public_symbols"])),
        ("layout_sha256", sha256_hex(golden["layout"])),
        ("status_classes_sha256", sha256_hex(golden["status_classes"])),
        ("public_function_count", str(len(names))),
        ("status_count", str(measured["status_count"])),
        ("stop_reason_count", str(measured["stop_reason_count"])),
        ("layout_structs", str(structs)),
        ("layout_members", str(members)),
        ("layout_constants", str(constants)),
        ("padding_holes", str(len(measured["holes"]))),
    ]
    pairs.extend(("padding_hole", row) for row in measured["holes"])
    return pairs


def bound_numbers(values):
    """((bound_minor, candidate_amendments) or None for a candidate record, findings).

    Both keys are present or both are absent, and each is a decimal number.
    """
    present = [key for key in BOUND_KEYS if key in values]
    if not present:
        return None, []
    if len(present) != len(BOUND_KEYS):
        absent = [key for key in BOUND_KEYS if key not in values]
        return None, [f"freeze-record: bound keys differ: the block holds {present[0]} but not "
                      f"{absent[0]} ({BOUND_KEYS[0]} and {BOUND_KEYS[1]} are both present or "
                      f"both absent)"]
    findings = [f"freeze-record: {key} differs: {values[key][0]!r} is not a decimal number"
                for key in BOUND_KEYS if not re.fullmatch(r"[0-9]+", values[key][0])]
    if findings:
        return None, findings
    return tuple(int(values[key][0]) for key in BOUND_KEYS), []


def minor_findings(values, rows, minor):
    """Findings of the rule that ties the ABI minor of the header to the amendment log.

    A candidate record (no bound keys): minor == FREEZE_CANDIDATE_MINOR + rows.
    A bound record (bound_minor and candidate_amendments):
    minor == bound_minor + (rows - candidate_amendments); the rows made before
    the bind do not pay for a minor raised after it.
    """
    bound, findings = bound_numbers(values)
    if findings:
        return findings
    if bound is None:
        if minor != FREEZE_CANDIDATE_MINOR + rows:
            return [f"freeze-record: amendments differs: ABI minor {minor} of the installed "
                    f"header needs {minor - FREEZE_CANDIDATE_MINOR} amendment row(s) (the "
                    f"candidate is minor {FREEZE_CANDIDATE_MINOR}), the log has {rows}"]
        return []
    bound_minor, before = bound
    if rows < before:
        return [f"freeze-record: candidate_amendments differs: the block counts {before} "
                f"amendment row(s) made before the bind, the log has {rows}"]
    after = rows - before
    if minor != bound_minor + after:
        return [f"freeze-record: amendments differs: ABI minor {minor} of the installed header "
                f"needs {minor - bound_minor} amendment row(s) after the bind (the bound minor "
                f"is {bound_minor}), the log has {after} after the bind ({rows} in all, "
                f"{before} of them made before it)"]
    return []


def record_findings(record_text, measured, golden):
    """Findings of the record's block against the golden files and the tree."""
    values, block_findings = parse_record(record_text)
    findings = [f"freeze-record: record malformed: {f}" for f in block_findings]
    if values == {} and block_findings:
        return findings
    rows = len(AMENDMENT_ROW_RE.findall(record_text))
    wanted = dict(block_values(measured, golden, rows))
    for key in SCALAR_KEYS:
        if key not in values:
            findings.append(f"freeze-record: {key} differs: the record has no {key} line")
            continue
        if values[key][0] != wanted[key]:
            hint = ""
            if key.endswith("_sha256"):
                hint = (f" (the golden file hashes to {wanted[key]}, "
                        f"the record binds {values[key][0]})")
            elif key == "amendments":
                hint = f" (the amendment log has {rows} row(s))"
            else:
                hint = f" (measured {wanted[key]}, recorded {values[key][0]})"
            findings.append(f"freeze-record: {key} differs:{hint}")
    findings.extend(minor_findings(values, rows, measured["abi_minor"]))
    recorded = sorted(values.get("padding_hole", []))
    for row in sorted(set(measured["holes"]) - set(recorded)):
        findings.append(f"freeze-record: padding_hole differs: measured {row}, not in the record")
    for row in sorted(set(recorded) - set(measured["holes"])):
        findings.append(f"freeze-record: padding_hole differs: recorded {row}, not measured")
    return findings


def freeze_findings(record_text, goldens, measured):
    """All findings of freeze mode: the tree against the goldens, then the record."""
    golden = golden_files(goldens)
    return (content_findings(measured, golden, True)
            + record_findings(record_text, measured, golden))


def render_block(pairs):
    return "\n".join([FREEZE_FENCE] + [f"{key}: {value}" for key, value in pairs]
                     + [FREEZE_CLOSE])


def write_freeze(record, goldens, measured):
    """Rewrite header.sha256 and the block of RECORD; 1 when a golden does not match."""
    golden = golden_files(goldens, optional=("header",))
    golden["header"] = measured["manifest"].encode("utf-8")
    findings = content_findings(measured, golden, False)
    for finding in findings:
        print(finding)
    if findings:
        print("freeze-record: not written: regenerate the other goldens with their generators first")
        return 1
    record_path = pathlib.Path(record)
    text = (record_path.read_text(encoding="utf-8") if record_path.is_file()
            else "# Freeze-candidate record\n\n" + FREEZE_FENCE + "\n" + FREEZE_CLOSE + "\n")
    lines = text.split("\n")
    opens = [i for i, line in enumerate(lines) if line.rstrip() == FREEZE_FENCE]
    if len(opens) != 1:
        raise UsageError(f"check-abi: {record} has {len(opens)} freeze-record blocks, wanted 1")
    closes = [i for i in range(opens[0] + 1, len(lines)) if lines[i].rstrip() == FREEZE_CLOSE]
    if not closes:
        raise UsageError(f"check-abi: the freeze-record block of {record} is not closed")
    rows = len(AMENDMENT_ROW_RE.findall(text))
    values, _block_findings = parse_record(text)
    _bound, bound_findings = bound_numbers(values)
    for finding in bound_findings:
        print(finding)
    if bound_findings:
        print("freeze-record: not written: fix the bound keys of the block first")
        return 1
    bound = [(key, values[key][0]) for key in BOUND_KEYS if key in values]
    block = render_block(block_values(measured, golden, rows, bound)).split("\n")
    lines[opens[0]:closes[0] + 1] = block
    (pathlib.Path(goldens) / "header.sha256").write_bytes(golden["header"])
    record_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"check-abi: freeze wrote header.sha256 and the block of {record_path.name} "
          f"(abi {measured['abi_version']}, package {measured['package_version']}, "
          f"amendments {rows}, headers {measured['header_count']})")
    return 0


def run_freeze(args):
    record = pathlib.Path(args.record)
    goldens = pathlib.Path(args.goldens) if args.goldens else record.parent
    nm_text = None
    if args.nm_file is not None:
        nm_text = read_installed(args.nm_file, "nm listing").decode("utf-8")
    measured = measure_install(args.prefix, nm_text)
    if args.write:
        return write_freeze(record, goldens, measured)
    record_text = read_installed(record, "freeze record").decode("utf-8")
    findings = freeze_findings(record_text, goldens, measured)
    for finding in findings:
        print(finding)
    if findings:
        for line in REBIND_PROCEDURE:
            print(line)
    else:
        structs, members, constants = measured["layout_counts"]
        symbols = len(read_golden_text(golden_files(goldens)["public_symbols"]))
        print(f"abi-freeze: headers={measured['header_count']} abi={measured['abi_version']} "
              f"package={measured['package_version']} symbols={symbols} "
              f"layout={structs}/{members}/{constants} holes={len(measured['holes'])} "
              f"statuses={measured['status_count']} stop_reasons={measured['stop_reason_count']}")
    count = len([f for f in findings if f.startswith("freeze-record:")])
    print(f"check-abi: freeze findings={count}")
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


# ---- freeze self-test: seeded incompatible changes ----------------------------

# Every case copies the installed headers and package version file of PREFIX, the
# four goldens and the record into a scratch directory below build/, applies ONE
# change, and runs freeze mode (through run_freeze, with an nm listing file, the
# way the CTest name calls it). A case names the texts the output must contain,
# the texts it must not contain and the exit status. The nm listing of the real
# archive is taken once per process.

_NM_CACHE = {}


class FreezeScratch:
    """One scratch copy of the installed tree, the goldens and the record."""

    def __init__(self, prefix, base):
        self.base = base
        self.docs = base / "abi"
        self.tree = base / "prefix"
        self.record = self.docs / "FREEZE-CANDIDATE.md"
        self.header = self.tree / "include" / DEFAULT_HEADER
        self.facts = {}  # {name: text} a seed fills in for "{name}" in the wanted texts
        self.docs.mkdir()
        for name in [name for _element, name in GOLDEN_FILES] + [REAL_RECORD.name]:
            shutil.copy(REAL_ABI_DIR / name, self.docs / name)
        shutil.copytree(pathlib.Path(prefix) / "include", self.tree / "include")
        version_file = self.tree / PACKAGE_VERSION_FILE
        version_file.parent.mkdir(parents=True)
        shutil.copy(pathlib.Path(prefix) / PACKAGE_VERSION_FILE, version_file)
        if prefix not in _NM_CACHE:
            _NM_CACHE[prefix] = nm_listing(pathlib.Path(prefix) / "lib" / "libt64_core.a",
                                           os.environ.get("NM") or "nm")
        self.nm = _NM_CACHE[prefix]

    def edit(self, path, change):
        path.write_text(change(path.read_text(encoding="utf-8")), encoding="utf-8")

    def edit_header(self, change):
        self.edit(self.header, change)

    def edit_golden(self, name, change):
        self.edit(self.docs / name, change)

    def edit_record(self, change):
        self.edit(self.record, change)

    def run(self, write=False):
        """Run freeze mode; return (exit status, printed text)."""
        listing = self.base / "nm-listing.txt"
        listing.write_text(self.nm, encoding="utf-8")
        printed = io.StringIO()
        with contextlib.redirect_stdout(printed):
            status = run_freeze(argparse.Namespace(
                record=str(self.record), prefix=str(self.tree), goldens=None,
                nm_file=str(listing), write=write))
        return status, printed.getvalue()


def nm_without(listing, name):
    """The nm listing without the lines that define NAME."""
    kept = [line for line in listing.split("\n")
            if line.split()[:1] not in ([name], ["_" + name])]
    return "\n".join(kept)


def seed_nm_without(name):
    def seed(ctx):
        ctx.nm = nm_without(ctx.nm, name)
    return seed


def seed_nm_added(ctx):
    prefix = "_" if platform.system() == "Darwin" else ""
    ctx.nm = ctx.nm.rstrip("\n") + f"\n{prefix}t64_machine_seeded T 0 0\n"


def seed_header_edit(old, new):
    def seed(ctx):
        ctx.edit_header(lambda text: seed_replace(text, old, new))
    return seed


def seed_header_crlf(ctx):
    ctx.header.write_bytes(ctx.header.read_bytes().replace(b"\n", b"\r\n"))


def seed_header_added(ctx):
    (ctx.tree / "include" / "tendies64" / "t64_extra.h").write_text(
        "/* a second installed header */\n", encoding="utf-8")


def seed_golden_edit(name, old, new):
    def seed(ctx):
        ctx.edit_golden(name, lambda text: seed_replace(text, old, new))
    return seed


def seed_layout_golden_offset(ctx):
    def change(text):
        edited, count = re.subn(
            r"(offsetof\(t64_machine_create_info, allocator\) == )(\d+)u",
            lambda m: f"{m.group(1)}{int(m.group(2)) + 8}u", text)
        if count != 1:
            raise ValueError(f"seed anchor occurs {count} times")
        return edited
    ctx.edit_golden("layout.c", change)


def seed_header_golden_digit(ctx):
    ctx.edit_golden("header.sha256",
                    lambda text: ("0" if text[0] != "0" else "1") + text[1:])


def seed_record_line(key, new_value):
    def seed(ctx):
        def change(text):
            lines = text.split("\n")
            hits = [i for i, line in enumerate(lines) if line.startswith(f"{key}: ")]
            if len(hits) != 1:
                raise ValueError(f"record line {key!r} occurs {len(hits)} times")
            lines[hits[0]] = f"{key}: {new_value}"
            return "\n".join(lines)
        ctx.edit_record(change)
    return seed


def seed_record_hash_digit(key):
    def seed(ctx):
        def change(text):
            lines = text.split("\n")
            hits = [i for i, line in enumerate(lines) if line.startswith(f"{key}: ")]
            if len(hits) != 1:
                raise ValueError(f"record line {key!r} occurs {len(hits)} times")
            value = lines[hits[0]].split(": ", 1)[1]
            lines[hits[0]] = f"{key}: {'0' if value[0] != '0' else '1'}{value[1:]}"
            return "\n".join(lines)
        ctx.edit_record(change)
    return seed


def seed_record_drop_hole(ctx):
    def change(text):
        lines = text.split("\n")
        hits = [i for i, line in enumerate(lines) if line.startswith("padding_hole: ")]
        if not hits:
            raise ValueError("the record has no padding_hole line")
        del lines[hits[0]]
        return "\n".join(lines)
    ctx.edit_record(change)


def seed_record_no_block(ctx):
    ctx.edit_record(lambda text: text.replace(FREEZE_FENCE, "```text"))


# The rule cases rebind a scratch copy at another ABI version: the major and minor
# of the header are edited, docs/abi/layout.c is regenerated from the edited header,
# the amendment log gets the wanted number of dated rows, --write rewrites
# header.sha256 and the block, and the bound keys are put back last, so the seeded
# state does not depend on what --write keeps. The committed tree never changes.

SEED_ROW = "| 2000-01-01 | header | seeded change | seeded evidence | seeded suite run | {minor} |"
BOUND_LINE_PREFIXES = ("bound_minor: ", "candidate_amendments: ")


def header_with_version(text, major, minor):
    """The header text with T64_ABI_VERSION_MAJOR and _MINOR set (None keeps one)."""
    for part, value in (("MAJOR", major), ("MINOR", minor)):
        if value is None:
            continue
        text, count = re.subn(
            rf"^(#\s*define\s+T64_ABI_VERSION_{part}\s+UINT32_C\(\s*)\d+(\s*\))",
            lambda m: f"{m.group(1)}{value}{m.group(2)}", text, flags=re.MULTILINE)
        if count != 1:
            raise ValueError(f"T64_ABI_VERSION_{part} occurs {count} times")
    return text


def record_with_bound(text, lines):
    """The record with its bound key lines replaced by LINES (after the amendments line)."""
    kept = [line for line in text.split("\n") if not line.startswith(BOUND_LINE_PREFIXES)]
    hits = [i for i, line in enumerate(kept) if line.startswith("amendments: ")]
    if len(hits) != 1:
        raise ValueError(f"record line 'amendments' occurs {len(hits)} times")
    kept[hits[0] + 1:hits[0] + 1] = list(lines)
    return "\n".join(kept)


def record_with_rows(text, count):
    """The record with exactly COUNT dated rows in its amendment log."""
    lines = [line for line in text.split("\n") if not AMENDMENT_ROW_RE.match(line)]
    heads = [i for i, line in enumerate(lines) if line.startswith("| date |")]
    if len(heads) != 1:
        raise ValueError(f"the amendment log head occurs {len(heads)} times")
    lines[heads[0] + 2:heads[0] + 2] = [SEED_ROW.format(minor=n + 1) for n in range(count)]
    return "\n".join(lines)


def seed_rebound(major, minor, bound=(), rows=0):
    """A seed: the scratch tree rebound at ABI MAJOR.MINOR, ROWS log rows, BOUND key lines."""
    def seed(ctx):
        ctx.edit_header(lambda text: header_with_version(text, major, minor))
        measured = measure_install(ctx.tree, ctx.nm)
        if measured["layout_text"] is None:
            raise ValueError("the layout probe of the edited header failed")
        (ctx.docs / "layout.c").write_text(measured["layout_text"], encoding="utf-8")
        ctx.edit_record(lambda text: record_with_rows(record_with_bound(text, ()), rows))
        status, printed = ctx.run(write=True)
        if status != 0:
            raise ValueError(f"the scratch rebind was refused: {printed.strip()[-160:]}")
        ctx.edit_record(lambda text: record_with_bound(text, bound))
    return seed


def seed_minor_raised(ctx):
    """Raise the ABI minor the scratch header holds by one (it holds minor 1 or 0)."""
    found = dict(VERSION_PART_RE.findall(ctx.header.read_text(encoding="utf-8")))
    raised = int(found["MINOR"]) + 1
    ctx.edit_header(lambda text: header_with_version(text, None, raised))
    ctx.facts["raised"] = str(raised)


def seed_then(*seeds):
    def seed(ctx):
        for each in seeds:
            each(ctx)
    return seed


BOUND_ZERO = ("bound_minor: 0", "candidate_amendments: 0")
BOUND_ONE_BEFORE = ("bound_minor: 0", "candidate_amendments: 1")
BOUND_KEYS_TEXT = "bound_minor and candidate_amendments are both present or both absent"


STOP_RESULT_TAIL = "  uint64_t tick;\n} t64_stop_result;"
STOP_REQUEST_TAIL = "  uint32_t kind;\n  uint64_t value;\n} t64_stop_request;"
COMMENT_LINE = "/* A stop request of the given T64_STOP_* kind and value. */"

# name -> (what is seeded, seed or None, wanted texts, unwanted texts, exit status)
FREEZE_CASES = {
    "freeze-clean": (
        "the committed record, goldens and installed tree", None,
        ("check-abi: freeze findings=0", "abi-freeze: headers=1"), ("differs",), 0),
    "freeze-crlf-canonical": (
        "the installed header with CRLF line endings: the hash is of the LF form",
        seed_header_crlf, ("check-abi: freeze findings=0",), ("differs",), 0),
    "freeze-member-removed": (
        "the last member of t64_stop_result removed",
        seed_header_edit(STOP_RESULT_TAIL, "} t64_stop_result;"),
        ("freeze-record: layout differs", "freeze-record: header_sha256 differs"), (), 1),
    "freeze-member-reordered": (
        "two members of t64_stop_request swapped",
        seed_header_edit(STOP_REQUEST_TAIL,
                         "  uint64_t value;\n  uint32_t kind;\n} t64_stop_request;"),
        ("freeze-record: layout differs", "freeze-record: header_sha256 differs"), (), 1),
    "freeze-status-renumbered": (
        "T64_STATUS_POISONED moved from 37 to 40",
        seed_header_edit("#define T64_STATUS_POISONED UINT32_C(37)",
                         "#define T64_STATUS_POISONED UINT32_C(40)"),
        ("freeze-record: classes differs: renumbered: T64_STATUS_POISONED header=40 table=37",
         "freeze-record: layout differs"), (), 1),
    "freeze-function-removed": (
        "t64_machine_unload missing from the archive listing",
        seed_nm_without("t64_machine_unload"),
        ("freeze-record: public_symbols differs: missing public symbol: t64_machine_unload",),
        ("header_sha256 differs", "layout differs"), 1),
    "freeze-function-added": (
        "a public function t64_machine_seeded in the archive listing",
        seed_nm_added,
        ("freeze-record: public_symbols differs: unexpected global: t64_machine_seeded",),
        ("header_sha256 differs", "layout differs"), 1),
    "freeze-unclassified-status": (
        "a new T64_STATUS_SEED the class table does not list",
        seed_header_edit("#define T64_STATUS_INTERNAL_DEFECT UINT32_C(38)\n",
                         "#define T64_STATUS_INTERNAL_DEFECT UINT32_C(38)\n"
                         "#define T64_STATUS_SEED UINT32_C(39)\n"),
        ("freeze-record: classes differs: unclassified: T64_STATUS_SEED",
         "freeze-record: header_sha256 differs"), (), 1),
    "freeze-comment-edit": (
        "one comment line of the installed header changed",
        seed_header_edit(COMMENT_LINE, COMMENT_LINE[:-3] + " (seeded edit). */"),
        ("freeze-record: header_sha256 differs",),
        ("layout differs", "classes differs", "public_symbols differs"), 1),
    "freeze-header-added": (
        "a second file below the installed include directory", seed_header_added,
        ("freeze-record: header_sha256 differs",), ("layout differs",), 1),
    "freeze-class-changed": (
        "the class of T64_STATUS_QUEUE_FULL changed from refused to defect in the table",
        seed_golden_edit("status-classes.txt", "T64_STATUS_QUEUE_FULL :: 11 :: refused ::",
                         "T64_STATUS_QUEUE_FULL :: 11 :: defect ::"),
        ("freeze-record: status_classes_sha256 differs",), ("classes differs",), 1),
    "freeze-symbols-golden-edited": (
        "t64_machine_unload removed from the public symbol golden",
        seed_golden_edit("public-symbols.txt", "t64_machine_unload\n", ""),
        ("freeze-record: public_symbols_sha256 differs",
         "freeze-record: public_symbols differs: unexpected global: t64_machine_unload"),
        (), 1),
    "freeze-layout-golden-edited": (
        "one offset of the layout golden edited", seed_layout_golden_offset,
        ("freeze-record: layout_sha256 differs",
         "freeze-record: layout differs: docs/abi/layout.c does not match"), (), 1),
    "freeze-header-golden-edited": (
        "the first digit of the header manifest golden changed", seed_header_golden_digit,
        ("freeze-record: header_sha256 differs: the installed headers do not hash",
         "freeze-record: header_sha256 differs: (the golden file hashes to"), (), 1),
    "freeze-count-edited": (
        "public_function_count of the record changed from 35 to 34",
        seed_record_line("public_function_count", "34"),
        ("freeze-record: public_function_count differs",), ("public_symbols differs",), 1),
    "freeze-record-hash-edited": (
        "one digit of layout_sha256 in the record changed", seed_record_hash_digit("layout_sha256"),
        ("freeze-record: layout_sha256 differs",), ("layout differs",), 1),
    "freeze-amendment-missing": (
        "the ABI minor of the installed header raised by one with no amendment row",
        seed_minor_raised,
        ("freeze-record: amendments differs: ABI minor {raised} of the installed header needs",
         "amendment row(s)", "freeze-record: abi_version differs"), (), 1),
    "freeze-padding-record-edited": (
        "one padding_hole row dropped from the record", seed_record_drop_hole,
        ("freeze-record: padding_hole differs: measured",), (), 1),
    "freeze-record-block-missing": (
        "the machine-read block removed from the record", seed_record_no_block,
        ("freeze-record: record malformed:",), (), 1),
    "freeze-candidate-clean": (
        "a candidate record (no bound keys) at ABI minor 1 with no amendment row",
        seed_rebound(None, 1), ("check-abi: freeze findings=0",), ("differs",), 0),
    "freeze-bound-clean": (
        "a bound record (bound_minor 0, candidate_amendments 0) at ABI 1.0, no amendment row",
        seed_rebound(1, 0, BOUND_ZERO),
        ("check-abi: freeze findings=0", "abi-freeze: headers=1"), ("differs",), 0),
    "freeze-bound-minor-raised": (
        "a bound record at ABI 1.1 with no amendment row",
        seed_rebound(1, 1, BOUND_ZERO),
        ("freeze-record: amendments differs: ABI minor 1 of the installed header needs 1 "
         "amendment row(s) after the bind (the bound minor is 0)",),
        ("abi_version differs", "header_sha256 differs"), 1),
    "freeze-candidate-minor-raised": (
        "a candidate record at ABI minor 2 with no amendment row",
        seed_rebound(None, 2),
        ("freeze-record: amendments differs: ABI minor 2 of the installed header needs 1 "
         "amendment row(s) (the candidate is minor 1), the log has 0",),
        ("abi_version differs", "bound minor"), 1),
    "freeze-candidate-row-counted": (
        "a candidate record at ABI minor 2 with one amendment row",
        seed_rebound(None, 2, rows=1), ("check-abi: freeze findings=0",), ("differs",), 0),
    "freeze-bound-post-bind-row": (
        "a bound record at ABI 1.1 with one amendment row made after the bind",
        seed_rebound(1, 1, BOUND_ZERO, rows=1),
        ("check-abi: freeze findings=0",), ("differs",), 0),
    "freeze-bound-row-without-minor": (
        "a bound record at ABI 1.0 with one amendment row and the minor not raised",
        seed_rebound(1, 0, BOUND_ZERO, rows=1),
        ("freeze-record: amendments differs: ABI minor 0 of the installed header needs 0 "
         "amendment row(s) after the bind (the bound minor is 0), the log has 1 after the "
         "bind (1 in all, 0 of them made before it)",), ("abi_version differs",), 1),
    "freeze-bound-candidate-row-not-counted": (
        "a bound record at ABI 1.0 whose one log row was made before the bind",
        seed_rebound(1, 0, BOUND_ONE_BEFORE, rows=1),
        ("check-abi: freeze findings=0",), ("differs",), 0),
    "freeze-bound-candidate-row-pays-nothing": (
        "a bound record at ABI 1.1 whose only log row was made before the bind",
        seed_rebound(1, 1, BOUND_ONE_BEFORE, rows=1),
        ("freeze-record: amendments differs: ABI minor 1 of the installed header needs 1 "
         "amendment row(s) after the bind (the bound minor is 0), the log has 0 after the "
         "bind (1 in all, 1 of them made before it)",), ("abi_version differs",), 1),
    "freeze-bound-rows-below-candidate": (
        "a bound record that counts 2 rows made before the bind while the log has 1",
        seed_rebound(1, 0, ("bound_minor: 0", "candidate_amendments: 2"), rows=1),
        ("freeze-record: candidate_amendments differs: the block counts 2 amendment row(s) "
         "made before the bind, the log has 1",), ("bound minor",), 1),
    "freeze-bound-minor-alone": (
        "bound_minor in the block without candidate_amendments",
        seed_rebound(None, 1, ("bound_minor: 0",)),
        (f"freeze-record: bound keys differ: the block holds bound_minor but not "
         f"candidate_amendments ({BOUND_KEYS_TEXT})",), (), 1),
    "freeze-bound-candidate-alone": (
        "candidate_amendments in the block without bound_minor",
        seed_rebound(None, 1, ("candidate_amendments: 0",)),
        (f"freeze-record: bound keys differ: the block holds candidate_amendments but not "
         f"bound_minor ({BOUND_KEYS_TEXT})",), (), 1),
    "freeze-bound-not-decimal": (
        "bound_minor edited to -1",
        seed_rebound(1, 0, ("bound_minor: -1", "candidate_amendments: 0")),
        ("freeze-record: bound_minor differs: '-1' is not a decimal number",), (), 1),
    "freeze-amendment-missing-bound": (
        "the ABI minor of a bound record's header raised by one with no amendment row",
        seed_then(seed_rebound(1, 0, BOUND_ZERO), seed_minor_raised),
        ("freeze-record: amendments differs: ABI minor 1 of the installed header needs 1 "
         "amendment row(s) after the bind (the bound minor is 0)",
         "freeze-record: abi_version differs"), (), 1),
}


def case_freeze_write_idempotent(prefix):
    """--write over the committed record and goldens reproduces them byte for byte."""
    scratch = scratch_dir("abi-freeze-selftest-")
    try:
        ctx = FreezeScratch(prefix, scratch)
        before = {name: (ctx.docs / name).read_bytes() for _e, name in GOLDEN_FILES}
        before[ctx.record.name] = ctx.record.read_bytes()
        status, printed = ctx.run(write=True)
        if status != 0:
            return f"--write exited {status}: {printed.strip().splitlines()[-1:]}"
        for name, data in before.items():
            if (ctx.docs / name).read_bytes() != data:
                return f"--write changed {name} of an already bound tree"
        status, printed = ctx.run()
        if status != 0:
            return f"the check failed after --write: {printed.strip().splitlines()[-1:]}"
        return None
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def case_freeze_write_refuses(prefix):
    """--write binds nothing while another golden does not match the installed tree."""
    scratch = scratch_dir("abi-freeze-selftest-")
    try:
        ctx = FreezeScratch(prefix, scratch)
        ctx.edit_golden("status-classes.txt",
                        lambda text: seed_drop_line(text, "T64_STATUS_QUEUE_FULL ::"))
        ctx.edit_header(lambda text: seed_replace(
            text, COMMENT_LINE, COMMENT_LINE[:-3] + " (seeded edit). */"))
        record_before = ctx.record.read_bytes()
        manifest_before = (ctx.docs / "header.sha256").read_bytes()
        status, printed = ctx.run(write=True)
        if status != 1 or "unclassified: T64_STATUS_QUEUE_FULL" not in printed:
            return f"--write over an unclassified status exited {status}: {printed.strip()[-120:]}"
        if ctx.record.read_bytes() != record_before:
            return "--write changed the record although it refused"
        if (ctx.docs / "header.sha256").read_bytes() != manifest_before:
            return "--write changed header.sha256 although it refused"
        return None
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def case_freeze_bound_write_keeps_keys(prefix):
    """--write over a bound record keeps bound_minor and candidate_amendments byte for byte."""
    scratch = scratch_dir("abi-freeze-selftest-")
    try:
        ctx = FreezeScratch(prefix, scratch)
        seed_rebound(1, 0, BOUND_ONE_BEFORE, rows=1)(ctx)
        before = ctx.record.read_bytes()
        status, printed = ctx.run(write=True)
        if status != 0:
            return f"--write exited {status}: {printed.strip().splitlines()[-1:]}"
        after = ctx.record.read_bytes()
        for line in BOUND_ONE_BEFORE:
            if line.encode("utf-8") + b"\n" not in after:
                return f"--write dropped or changed the block line {line!r}"
        if after != before:
            return "--write changed a bound record that already matches the tree"
        status, printed = ctx.run()
        if status != 0:
            return f"the check failed after --write: {printed.strip().splitlines()[-1:]}"
        return None
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def case_freeze_bound_write_refuses_partial(prefix):
    """--write binds nothing while the block holds only one of the two bound keys."""
    scratch = scratch_dir("abi-freeze-selftest-")
    try:
        ctx = FreezeScratch(prefix, scratch)
        seed_rebound(None, 1, ("bound_minor: 0",))(ctx)
        record_before = ctx.record.read_bytes()
        manifest_before = (ctx.docs / "header.sha256").read_bytes()
        status, printed = ctx.run(write=True)
        if status != 1 or "freeze-record: bound keys differ" not in printed:
            return f"--write over a lone bound_minor exited {status}: {printed.strip()[-120:]}"
        if ctx.record.read_bytes() != record_before:
            return "--write changed the record although it refused"
        if (ctx.docs / "header.sha256").read_bytes() != manifest_before:
            return "--write changed header.sha256 although it refused"
        return None
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


FREEZE_SPECIAL = {
    "freeze-write-idempotent": (
        "--write over the committed bound tree changes nothing",
        case_freeze_write_idempotent),
    "freeze-write-refuses": (
        "--write with an unclassified status binds nothing", case_freeze_write_refuses),
    "freeze-bound-write-keeps-keys": (
        "--write over a bound record keeps both bound keys byte for byte",
        case_freeze_bound_write_keeps_keys),
    "freeze-bound-write-refuses-partial": (
        "--write with only one bound key binds nothing",
        case_freeze_bound_write_refuses_partial),
}


def run_freeze_case(name, prefix):
    """Run one freeze case; return a problem text or None."""
    if prefix is None or not pathlib.Path(prefix, "include").is_dir():
        raise UsageError(f"check-abi: {name} needs --selftest-prefix with an installed tree")
    prefix = str(prefix)
    if name in FREEZE_SPECIAL:
        return FREEZE_SPECIAL[name][1](prefix)
    _what, seed, wanted, unwanted, want_exit = FREEZE_CASES[name]
    scratch = scratch_dir("abi-freeze-selftest-")
    try:
        ctx = FreezeScratch(prefix, scratch)
        if seed is not None:
            try:
                seed(ctx)
            except ValueError as error:
                return f"seed did not apply: {error}"
        status, printed = ctx.run()
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    if status != want_exit:
        return f"freeze mode exited {status}, wanted {want_exit}: {printed.strip()[-200:]}"
    for name, value in ctx.facts.items():
        wanted = [text.replace("{" + name + "}", value) for text in wanted]
        unwanted = [text.replace("{" + name + "}", value) for text in unwanted]
    missing = [text for text in wanted if text not in printed]
    present = [text for text in unwanted if text in printed]
    if missing:
        return f"output lacks {missing}: {printed.strip()[:300]}"
    if present:
        return f"output holds {present}: {printed.strip()[:300]}"
    return None


def run_freeze_selftest(prefix):
    failed = 0
    names = list(FREEZE_CASES) + list(FREEZE_SPECIAL)
    for name in names:
        problem = run_freeze_case(name, prefix)
        if problem is None:
            if name in FREEZE_SPECIAL:
                seen = FREEZE_SPECIAL[name][0]
            else:
                _what, _seed, wanted, _unwanted, want_exit = FREEZE_CASES[name]
                seen = ("fails with: " + "; ".join(text.replace("freeze-record: ", "")
                                                     for text in wanted)
                        if want_exit else "passes")
            print(f"selftest {name}: ok ({seen})")
        else:
            print(f"selftest {name}: FAILED: {problem}")
            failed += 1
    print(f"check-abi: freeze selftest cases={len(names)} failed={failed}")
    return 1 if failed else 0


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
    parser.add_argument("--freeze-selftest", action="store_true",
                        help="run every freeze self-test case (needs --selftest-prefix)")
    parser.add_argument("--selftest-prefix", metavar="DIR",
                        help="the installed tree the freeze self-test cases copy")
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
    freeze = modes.add_parser(
        "freeze", help="recompute the freeze-candidate record from the installed tree")
    freeze.add_argument("--record", required=True, metavar="FILE",
                        help="the record with the machine-read block")
    freeze.add_argument("--prefix", required=True, metavar="DIR",
                        help="the installed tree (include/, lib/libt64_core.a, package files)")
    freeze.add_argument("--goldens", metavar="DIR",
                        help="the directory of the four golden files (default: that of FILE)")
    freeze.add_argument("--nm-file", metavar="FILE",
                        help="an nm -P -g --defined-only listing instead of running nm")
    freeze.add_argument("--write", action="store_true",
                        help="rewrite header.sha256 and the block after the other goldens match")
    freeze.set_defaults(run=run_freeze)
    return parser


def main(argv):
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        if args.list_cases:
            for name in list(CASES) + list(EXTRA_CASES) + list(FREEZE_CASES) + list(FREEZE_SPECIAL):
                print(name)
            return 0
        if args.freeze_selftest:
            return run_freeze_selftest(args.selftest_prefix)
        if args.case is not None:
            if args.case in FREEZE_CASES or args.case in FREEZE_SPECIAL:
                problem = run_freeze_case(args.case, args.selftest_prefix)
            elif args.case not in CASES and args.case not in EXTRA_CASES:
                raise UsageError(f"check-abi: unknown case {args.case}")
            else:
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
