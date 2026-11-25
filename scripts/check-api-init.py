#!/usr/bin/env python3
"""Init-form guard for the public in/out structs (Phase 11, plan 11-10).

Every public in/out struct of core/include/tendies64/t64_machine.h will carry a
leading caller-filled struct_size (plan 11-12). The T64_*_INIT macros of that
header fill it. The compiler cannot see the forms that leave it zero: an
uninitialized declaration, a positional or zero brace initializer, a compound
literal, or a memset of the object. This guard finds those forms in the source
trees and reports them as

    RULE path:line: message

Rules:
  API-INIT-UNINIT      a declaration of a public struct object without an
                       initializer (or several declarators in one statement)
  API-INIT-POSITIONAL  a brace initializer outside the init-macro form: a
                       positional or zero list, or an array entry that is not
                       the macro (or an array with fewer entries than its size)
  API-INIT-COMPOUND    a compound literal (T){...} outside the macro form
  API-INIT-MEMSET      a memset or bzero of an object of a type that has an
                       init macro, including the pointer form memset(out, ...)

Type handling. The struct types are parsed from the public header. The macro of
a type is T64_<NAME>_INIT (and T64_<NAME>_INIT_WITH) for t64_<name>.
  t64_host_storage     exempt: the core fills it before each allocate call
  t64_trace_record     exempt: array elements carry no struct_size
  t64_profile_descriptor
                       only brace initializers are checked; a designated
                       ".member = value" list passes (registered descriptor
                       tables), a positional member is a finding, and a lone
                       zero ("{0}" or "{}") is the deliberate empty-descriptor
                       fixture and passes
Arrays whose entries all use the macro are accepted; struct members of these
types cannot carry an initializer and are not findings. The fixed exclusion
EXCLUDED lists the files that are never scanned, each with its reason.

Usage:
  python3 -B scripts/check-api-init.py [--root DIR] [--exclude-oracle-bound]
                                       [PATH ...]
  python3 -B scripts/check-api-init.py --list-cases | --case NAME | --selftest

PATH is a file or a directory (every *.c and *.h below it), relative to the
root; the default is core/tests runner tests tools. --exclude-oracle-bound
skips every path of EXPECTED_CORPUS_PATHS of scripts/check-rdp-oracle-authority.py
(read with ast, never imported): those files carry the old forms until the
isolated bound-file commit of plan 11-14, and plan 11-16 removes the flag.

Standard library only. Exit codes: 0 clean, 1 findings (or a failed selftest),
2 usage error or unreadable input.
"""

from __future__ import annotations

import argparse
import ast
import bisect
import pathlib
import re
import shutil
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = "core/include/tendies64/t64_machine.h"
ORACLE_CHECKER = "scripts/check-rdp-oracle-authority.py"
DEFAULT_PATHS = ("core/tests", "runner", "tests", "tools")
SUFFIXES = (".c", ".h")

# Files that are never scanned, with the reason (a file outside the scanned
# directories is listed too, so the exclusion is visible in one place).
EXCLUDED = {
    "tools/digest-sweep/t64_digest_sweep.c": (
        "the digest-sweep printer keeps two branches by design and its base "
        "branch must compile against the Wave 0 tree, so it keeps its own "
        "SWEEP_* initializer macros"),
}

# t64_host_storage: the core fills it (it is an out struct handed to the host
# allocator callback). t64_trace_record: array elements carry no struct_size.
EXEMPT_TYPES = frozenset({"t64_host_storage", "t64_trace_record"})
DESCRIPTOR_TYPES = frozenset({"t64_profile_descriptor"})

UNINIT = "API-INIT-UNINIT"
POSITIONAL = "API-INIT-POSITIONAL"
COMPOUND = "API-INIT-COMPOUND"
MEMSET = "API-INIT-MEMSET"


class UsageError(Exception):
    """An unreadable input or a bad argument: exit status 2."""


# ---------------------------------------------------------------- text tools


def blank(chars, start, end):
    for index in range(start, min(end, len(chars))):
        if chars[index] != "\n":
            chars[index] = " "


def mask(text):
    """Same-length text with comments, literal contents and directives blanked."""
    chars = list(text)
    size = len(text)
    index = 0
    while index < size:
        char = text[index]
        if char == "/" and text.startswith("//", index):
            end = text.find("\n", index)
            end = size if end < 0 else end
            blank(chars, index, end)
            index = end
        elif char == "/" and text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = size if end < 0 else end + 2
            blank(chars, index, end)
            index = end
        elif char in "\"'":
            end = index + 1
            while end < size and text[end] != char and text[end] != "\n":
                end += 2 if text[end] == "\\" else 1
            blank(chars, index + 1, end)
            index = end + 1
        elif char == "#":
            line_start = text.rfind("\n", 0, index) + 1
            if text[line_start:index].strip() == "":
                end = index
                while end < size:
                    stop = text.find("\n", end)
                    stop = size if stop < 0 else stop
                    end = stop
                    if stop > 0 and text[stop - 1] == "\\" and stop < size:
                        end = stop + 1
                        continue
                    break
                blank(chars, index, end)
                index = end
            else:
                index += 1
        else:
            index += 1
    return "".join(chars)


def classify_open(masked, pos, parent):
    """Kind of the brace at pos: code, init (initializer) or aggregate."""
    if parent == "init":
        return "init"
    start = pos - 1
    while start >= 0 and masked[start] not in ";{}":
        start -= 1
    prefix = masked[start + 1:pos].strip()
    if prefix.endswith(")"):
        depth = 0
        index = len(prefix) - 1
        while index >= 0:
            if prefix[index] == ")":
                depth += 1
            elif prefix[index] == "(":
                depth -= 1
                if depth == 0:
                    break
            index -= 1
        before = prefix[:max(index, 0)].rstrip()
        word = re.search(r"(\w+)$", before)
        if word:
            return "init" if word.group(1) == "return" else "code"
        return "code" if before == "" else "init"
    if re.search(r"(?<![=!<>])=(?!=)", prefix):
        return "init"
    if re.search(r"\b(struct|union|enum)\b", prefix) and "(" not in prefix:
        return "aggregate"
    return "code"


def brace_intervals(masked):
    """Sorted (open, close, kind, depth) of every brace pair."""
    intervals = []
    stack = []
    for pos, char in enumerate(masked):
        if char == "{":
            kind = classify_open(masked, pos, stack[-1][1] if stack else None)
            stack.append((pos, kind, len(stack)))
        elif char == "}" and stack:
            start, kind, depth = stack.pop()
            intervals.append((start, pos, kind, depth))
    for start, kind, depth in stack:
        intervals.append((start, len(masked), kind, depth))
    intervals.sort()
    return intervals


def innermost_kind(intervals, starts, pos):
    """Kind of the innermost brace pair that holds pos, or None."""
    found = None
    limit = bisect.bisect_right(starts, pos)
    for open_pos, close_pos, kind, _depth in reversed(intervals[:limit]):
        if open_pos < pos < close_pos:
            found = kind
            break
    return found


def match_close(masked, open_pos, opener="{", closer="}"):
    depth = 0
    for pos in range(open_pos, len(masked)):
        if masked[pos] == opener:
            depth += 1
        elif masked[pos] == closer:
            depth -= 1
            if depth == 0:
                return pos
    return -1


def split_elements(masked, open_pos, close_pos):
    """(start, text) of the top-level comma separated elements of a brace list."""
    elements = []
    depth = 0
    begin = open_pos + 1
    for pos in range(open_pos + 1, close_pos):
        char = masked[pos]
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        elif char == "," and depth == 0:
            elements.append((begin, masked[begin:pos].strip()))
            begin = pos + 1
    elements.append((begin, masked[begin:close_pos].strip()))
    if elements and elements[-1][1] == "":
        elements.pop()
    return elements


def statement_end(masked, pos):
    """Position of the depth-0 ',' or ';' that ends the declarator at pos."""
    depth = 0
    for index in range(pos, len(masked)):
        char = masked[index]
        if char in "([{":
            depth += 1
        elif char in ")]}":
            if depth == 0:
                return index
            depth -= 1
        elif char in ",;" and depth == 0:
            return index
    return len(masked)


def paren_depth_open(masked, pos):
    """True when pos sits inside an unclosed parenthesis of its statement."""
    closers = 0
    for index in range(pos - 1, -1, -1):
        char = masked[index]
        if char == ")":
            closers += 1
        elif char == "(":
            if closers == 0:
                return True
            closers -= 1
        elif char in ";{}" and closers == 0:
            return False
    return False


# ------------------------------------------------------------------- header


def public_types(root):
    """Names of the typedef'd structs of the public header, in file order."""
    path = root / HEADER
    try:
        text = mask(path.read_text(encoding="utf-8"))
    except OSError as error:
        raise UsageError(f"check-api-init: cannot read {HEADER}: {error}")
    names = re.findall(r"typedef\s+struct\s*\{[^{}]*\}\s*(\w+)\s*;", text)
    if not names:
        raise UsageError(f"check-api-init: no struct typedef found in {HEADER}")
    return names


class Model:
    """The checked types and the regexes derived from them."""

    def __init__(self, names):
        self.descriptor = [n for n in names if n in DESCRIPTOR_TYPES]
        self.macro_types = [n for n in names
                            if n not in EXEMPT_TYPES and n not in DESCRIPTOR_TYPES]
        self.checked = self.macro_types + self.descriptor
        alternation = "|".join(sorted(self.checked, key=len, reverse=True))
        self.alternation = alternation
        self.decl = re.compile(
            r"(?<![\w.>])(?P<quals>(?:(?:static|const|volatile|register|extern)\s+)*)"
            r"\b(?P<type>" + alternation + r")\b(?P<ws>\s+)(?P<name>[A-Za-z_]\w*)"
            r"(?P<dims>(?:\s*\[[^\]]*\])*)\s*(?P<sep>=|;|,|\))")
        self.pointer = re.compile(
            r"\b(?P<type>" + alternation + r")\b\s*(?:const\s+)?\*+\s*"
            r"(?:const\s+)?(?P<name>[A-Za-z_]\w*)")
        self.compound = re.compile(r"\(\s*(?P<type>" + alternation + r")\s*\)\s*\{")
        self.type_word = re.compile(r"\b(" + "|".join(self.macro_types) + r")\b")

    def macro_re(self, type_name):
        stem = "T64_" + type_name[4:].upper() + "_INIT"
        return re.compile(r"\b" + stem + r"(?:_WITH)?\b")


# ------------------------------------------------------------------- rules


def line_table(text):
    starts = [0]
    for found in re.finditer("\n", text):
        starts.append(found.end())
    return starts


DESIGNATOR = re.compile(r"^\[[^\]]*\]\s*=\s*")
MEMSET_CALL = re.compile(r"\b(memset|bzero)\s*\(")
CAST_PREFIX = re.compile(r"^\(\s*[A-Za-z_][\w\s]*\*+\s*\)\s*")
PATH_PREFIX = re.compile(
    r"([A-Za-z_]\w*)((?:\s*(?:\.|->)\s*[A-Za-z_]\w*|\s*\[[^\]]*\])*)")
LONE_ZERO = re.compile(r"0[uUlL]*")


def macro_hint(type_name):
    return "T64_" + type_name[4:].upper() + "_INIT"


def check_descriptor(masked, open_pos, close_pos, line_of):
    """A descriptor brace list: designated members pass, a lone zero passes."""
    elements = split_elements(masked, open_pos, close_pos)
    if not elements:
        return []
    if len(elements) == 1 and LONE_ZERO.fullmatch(elements[0][1]):
        return []
    if all(text.startswith("{") for _start, text in elements):
        found = []
        for start, _text in elements:
            inner = masked.find("{", start)
            found += check_descriptor(
                masked, inner, match_close(masked, inner), line_of)
        return found
    for start, element in elements:
        if not element.startswith("."):
            lead = start
            while lead < close_pos and masked[lead].isspace():
                lead += 1
            return [(POSITIONAL, line_of(lead),
                     "positional member in a t64_profile_descriptor initializer; "
                     "write designated .member = value forms")]
    return []


def check_brace(model, masked, type_name, dims, open_pos, close_pos, line_of):
    """Findings of the brace initializer of one declaration."""
    if type_name in DESCRIPTOR_TYPES:
        return check_descriptor(masked, open_pos, close_pos, line_of)
    hint = macro_hint(type_name)
    if not dims.strip():
        return [(POSITIONAL, line_of(open_pos),
                 f"brace initializer of {type_name}; write {hint}... instead")]
    macro = model.macro_re(type_name)
    leaves = []
    designated = False

    def walk(opening, closing):
        nonlocal designated
        for start, element in split_elements(masked, opening, closing):
            lead = start
            while lead < closing and masked[lead].isspace():
                lead += 1
            if DESIGNATOR.match(element):
                designated = True
                element = DESIGNATOR.sub("", element)
            if element.startswith("{"):
                inner = masked.find("{", start)
                walk(inner, match_close(masked, inner))
            else:
                leaves.append((lead, element))

    walk(open_pos, close_pos)
    found = []
    for lead, element in leaves:
        if not macro.search(element):
            found.append((POSITIONAL, line_of(lead),
                          f"array entry of {type_name} is not {hint}..."))
    sizes = re.findall(r"\[([^\]]*)\]", dims)
    total = 1
    for size in sizes:
        if re.fullmatch(r"\s*\d+[uUlL]*\s*", size):
            total *= int(re.sub(r"\D", "", size))
        else:
            total = None
            break
    if found:
        return found[:1]  # one finding per declaration
    if not designated and total is not None and len(leaves) < total:
        found.append((POSITIONAL, line_of(open_pos),
                      f"array initializer of {type_name} covers {len(leaves)} of "
                      f"{total} entries; the rest start zeroed ({hint}... for each)"))
    elif not leaves:
        found.append((POSITIONAL, line_of(open_pos),
                      f"empty array initializer of {type_name}"))
    return found


def scan_text(model, path, text):
    """Findings (rule, line, message) of one file."""
    masked = mask(text)
    table = line_table(masked)

    def line_of(pos):
        return bisect.bisect_right(table, pos)

    intervals = brace_intervals(masked)
    opens = [interval[0] for interval in intervals]
    functions = []
    for open_pos, close_pos, kind, depth in intervals:
        if depth == 0 and kind == "code":
            begin = max(masked.rfind(";", 0, open_pos), masked.rfind("}", 0, open_pos)) + 1
            functions.append((begin, close_pos))
    function_starts = [begin for begin, _end in functions]

    def scope_of(pos):
        index = bisect.bisect_right(function_starts, pos) - 1
        if index >= 0 and pos <= functions[index][1]:
            return index
        return -1

    found = []
    typed = {}
    member_typed = {}

    for match in model.pointer.finditer(masked):
        pos = match.start("type")
        if innermost_kind(intervals, opens, pos) == "aggregate":
            member_typed[match.group("name")] = match.group("type")
        else:
            typed.setdefault(scope_of(pos), {})[match.group("name")] = match.group("type")

    for match in model.decl.finditer(masked):
        type_name = match.group("type")
        name = match.group("name")
        sep = match.group("sep")
        dims = match.group("dims")
        pos = match.start("type")
        kind = innermost_kind(intervals, opens, pos)
        if kind == "aggregate":
            member_typed[name] = type_name
            continue
        typed.setdefault(scope_of(pos), {})[name] = type_name
        if kind == "init" or "extern" in match.group("quals").split():
            continue
        begin = max(masked.rfind(";", 0, pos), masked.rfind("{", 0, pos),
                    masked.rfind("}", 0, pos)) + 1
        if re.search(r"\btypedef\b", masked[begin:pos]):
            continue
        in_parens = paren_depth_open(masked, pos)
        line = line_of(match.start("name"))
        if sep == ")" or (sep == "," and in_parens):
            continue
        if sep in (";", ","):
            if type_name in DESCRIPTOR_TYPES or in_parens:
                continue
            if sep == ",":
                found.append((UNINIT, line,
                              f"several declarators of {type_name} in one declaration; "
                              f"give each its own {macro_hint(type_name)}... initializer"))
            else:
                found.append((UNINIT, line,
                              f"{type_name} {name} has no initializer; "
                              f"write = {macro_hint(type_name)}..."))
            continue
        index = match.end()
        while index < len(masked) and masked[index].isspace():
            index += 1
        after = index
        if masked.startswith("{", index):
            close = match_close(masked, index)
            found += check_brace(model, masked, type_name, dims, index, close, line_of)
            after = close + 1
        end = statement_end(masked, after)
        if (end < len(masked) and masked[end] == "," and not in_parens
                and type_name not in DESCRIPTOR_TYPES):
            found.append((UNINIT, line,
                          f"several declarators of {type_name} in one declaration; "
                          f"give each its own {macro_hint(type_name)}... initializer"))

    for match in model.compound.finditer(masked):
        open_pos = match.end() - 1
        close_pos = match_close(masked, open_pos)
        type_name = match.group("type")
        if type_name in DESCRIPTOR_TYPES:
            found += check_descriptor(masked, open_pos, close_pos, line_of)
        else:
            found.append((COMPOUND, line_of(match.start()),
                          f"compound literal of {type_name}; write "
                          f"({type_name}){macro_hint(type_name)}..."))

    def resolve(expression, scope):
        value = expression.strip()
        while True:
            stripped = CAST_PREFIX.sub("", value)
            if stripped == value:
                break
            value = stripped
        value = value.lstrip("&* (")
        parts = PATH_PREFIX.match(value)
        if parts is None:
            return None
        members = re.findall(r"(?:\.|->)\s*([A-Za-z_]\w*)", parts.group(2))
        if members:
            return member_typed.get(members[-1])
        return typed.get(scope, {}).get(parts.group(1)) or typed.get(-1, {}).get(parts.group(1))

    for match in MEMSET_CALL.finditer(masked):
        open_pos = match.end() - 1
        close_pos = match_close(masked, open_pos, "(", ")")
        if close_pos < 0:
            continue
        arguments = [element for _start, element in split_elements(masked, open_pos, close_pos)]
        want = 3 if match.group(1) == "memset" else 2
        if len(arguments) < want:
            continue
        scope = scope_of(match.start())
        type_name = resolve(arguments[0], scope)
        if type_name not in model.macro_types:
            type_name = None
            size = arguments[want - 1]
            if "sizeof" in size:
                operand = size[size.index("sizeof") + len("sizeof"):]
                word = model.type_word.search(operand)
                candidate = word.group(1) if word else resolve(operand, scope)
                if candidate in model.macro_types:
                    type_name = candidate
        if type_name is not None:
            found.append((MEMSET, line_of(match.start()),
                          f"{match.group(1)} of a {type_name} object ({arguments[0]}); "
                          f"assign {macro_hint(type_name)}... instead, a zeroing call "
                          f"clears struct_size"))
    found.sort(key=lambda item: (item[1], item[0]))
    return found


# ----------------------------------------------------------------- scanning


def oracle_bound_paths(root):
    """EXPECTED_CORPUS_PATHS of the oracle authority checker, read with ast."""
    path = root / ORACLE_CHECKER
    try:
        tree = ast.parse(path.read_text(encoding="utf-8"))
    except (OSError, SyntaxError) as error:
        raise UsageError(f"check-api-init: cannot read {ORACLE_CHECKER}: {error}")
    for node in tree.body:
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name) and target.id == "EXPECTED_CORPUS_PATHS":
                    try:
                        return set(ast.literal_eval(node.value))
                    except ValueError as error:
                        raise UsageError(
                            f"check-api-init: EXPECTED_CORPUS_PATHS is not a literal: {error}")
    raise UsageError(f"check-api-init: EXPECTED_CORPUS_PATHS not found in {ORACLE_CHECKER}")


def collect_files(root, paths, strict):
    files = []
    for entry in paths:
        base = root / entry
        if base.is_file():
            candidates = [base]
        elif base.is_dir():
            candidates = sorted(p for p in base.rglob("*") if p.is_file())
        elif strict:
            raise UsageError(f"check-api-init: no such path: {entry}")
        else:
            continue
        for candidate in candidates:
            if candidate.suffix in SUFFIXES:
                files.append(candidate.relative_to(root).as_posix())
    return sorted(set(files))


def run_check(root, paths, exclude_oracle_bound):
    """(findings, scanned count, excluded relative paths)."""
    model = Model(public_types(root))
    skipped = set(EXCLUDED)
    if exclude_oracle_bound:
        skipped |= oracle_bound_paths(root)
    findings = []
    scanned = 0
    excluded = []
    for relative in collect_files(root, paths or DEFAULT_PATHS, bool(paths)):
        if relative in skipped:
            excluded.append(relative)
            continue
        try:
            text = (root / relative).read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as error:
            raise UsageError(f"check-api-init: cannot read {relative}: {error}")
        scanned += 1
        for rule, line, message in scan_text(model, relative, text):
            findings.append(f"{rule} {relative}:{line}: {message}")
    return findings, scanned, excluded


# ----------------------------------------------------------------- selftest

CASES = {}


def case(name, files, expect_rules=(), expect_clean=False, flag=False, also_clean_with_flag=False):
    CASES[name] = (files, tuple(expect_rules), expect_clean, flag, also_clean_with_flag)


def seed_function(body):
    return (
        '#include "tendies64/t64_machine.h"\n'
        "static void seed(t64_machine *machine) {\n" + body + "\n}\n")


case("uninit-stop-request",
     {"core/tests/seed.c": seed_function(
         "  t64_stop_request r;\n  r.kind = 1u;\n  r.value = 2u;\n  (void)machine;")},
     expect_rules=[UNINIT])
case("positional-allocator",
     {"core/tests/seed.c": seed_function(
         "  t64_host_allocator a = {f, g, NULL};\n  (void)a;\n  (void)machine;")},
     expect_rules=[POSITIONAL])
case("zero-result",
     {"core/tests/seed.c": seed_function(
         "  t64_stop_result s = {0};\n  (void)s;\n  (void)machine;")},
     expect_rules=[POSITIONAL])
case("compound-request",
     {"core/tests/seed.c": seed_function(
         "  use(machine, &(t64_stop_request){1, 2});")},
     expect_rules=[COMPOUND])
case("macro-forms-clean",
     {"core/tests/seed.c": seed_function(
         "  t64_stop_request r = T64_STOP_REQUEST_INIT(1u, 2u);\n"
         "  t64_stop_result s = T64_STOP_RESULT_INIT;\n"
         "  t64_stop_result w = T64_STOP_RESULT_INIT_WITH(1u, 2u);\n"
         "  t64_host_allocator a = T64_HOST_ALLOCATOR_INIT(f, g, NULL);\n"
         "  t64_output_drain_result d = T64_OUTPUT_DRAIN_RESULT_INIT;\n"
         "  use(machine, &(t64_stop_request)T64_STOP_REQUEST_INIT(1u, 2u), &s);\n"
         "  (void)r; (void)w; (void)a; (void)d;")},
     expect_clean=True)
case("designated-descriptor",
     {"core/tests/seed.c": seed_function(
         "  static const t64_profile_descriptor d = {\n"
         "      .profile_id = \"A\",\n      .tv_standard = 1u,\n      .region = 2u};\n"
         "  (void)d;\n  (void)machine;")},
     expect_clean=True)
case("positional-descriptor",
     {"core/tests/seed.c": seed_function(
         "  static const t64_profile_descriptor d = {\"A\", \"B\", 1u};\n"
         "  (void)d;\n  (void)machine;")},
     expect_rules=[POSITIONAL])
case("empty-descriptor-fixture",
     {"core/tests/seed.c": seed_function(
         "  t64_profile_descriptor empty = {0};\n  (void)empty;\n  (void)machine;")},
     expect_clean=True)
case("memset-macro-object",
     {"core/tests/seed.c": seed_function(
         "  t64_output_drain_result d = T64_OUTPUT_DRAIN_RESULT_INIT;\n"
         "  memset(&d, 0, sizeof d);\n  (void)machine;")},
     expect_rules=[MEMSET])
case("memset-pointer-form",
     {"core/tests/seed.c": (
         '#include "tendies64/t64_machine.h"\n'
         "static void clear(t64_save_info *out) {\n"
         "  memset(out, 0, sizeof *out);\n}\n")},
     expect_rules=[MEMSET])
case("array-entries-clean",
     {"core/tests/seed.c": seed_function(
         "  t64_audio_packet_info a[2] = {T64_AUDIO_PACKET_INFO_INIT,\n"
         "                                T64_AUDIO_PACKET_INFO_INIT};\n"
         "  t64_audio_packet_info b[2][2] = {\n"
         "      {T64_AUDIO_PACKET_INFO_INIT, T64_AUDIO_PACKET_INFO_INIT},\n"
         "      {T64_AUDIO_PACKET_INFO_INIT, T64_AUDIO_PACKET_INFO_INIT}};\n"
         "  (void)a; (void)b;\n  (void)machine;")},
     expect_clean=True)
case("array-zero-entry",
     {"core/tests/seed.c": seed_function(
         "  t64_video_packet_info v[2] = {0};\n  (void)v;\n  (void)machine;")},
     expect_rules=[POSITIONAL])
case("array-partial",
     {"core/tests/seed.c": seed_function(
         "  t64_video_packet_info v[2] = {T64_VIDEO_PACKET_INFO_INIT};\n"
         "  (void)v;\n  (void)machine;")},
     expect_rules=[POSITIONAL])
case("exempt-types-clean",
     {"core/tests/seed.c": seed_function(
         "  t64_host_storage image = {NULL, 0u, 0u};\n"
         "  t64_trace_record records[4];\n"
         "  (void)image; (void)records;\n  (void)machine;")},
     expect_clean=True)
case("members-clean",
     {"core/tests/seed.c": (
         '#include "tendies64/t64_machine.h"\n'
         "typedef struct {\n  t64_stop_result stop;\n  t64_host_allocator allocator;\n"
         "} fixture;\n")},
     expect_clean=True)
case("oracle-bound-flag",
     {"scripts/check-rdp-oracle-authority.py":
          "EXPECTED_CORPUS_PATHS = {\n    \"core/tests/test_bound.c\",\n}\n",
      "core/tests/test_bound.c": seed_function(
          "  t64_stop_result s = {0u, 0u};\n  (void)s;\n  (void)machine;")},
     expect_rules=[POSITIONAL], flag=True, also_clean_with_flag=True)
case("digest-sweep-excluded",
     {"tools/digest-sweep/t64_digest_sweep.c": seed_function(
         "  t64_stop_result s = {0u, 0u};\n  (void)s;\n  (void)machine;")},
     expect_clean=True)


def run_case(name):
    """None when the case behaves as seeded, else the problem text."""
    files, expect_rules, expect_clean, flag, also_clean_with_flag = CASES[name]
    build = ROOT / "build"
    build.mkdir(exist_ok=True)
    scratch = pathlib.Path(tempfile.mkdtemp(prefix="api-init-selftest-", dir=build))
    try:
        header = scratch / HEADER
        header.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / HEADER, header)
        for relative, content in files.items():
            target = scratch / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(content, encoding="utf-8")
        findings, _scanned, _excluded = run_check(scratch, (), False)
        if expect_clean:
            if findings:
                return "expected a clean run, got: " + "; ".join(findings)
            return None
        for rule in expect_rules:
            if not any(line.startswith(rule + " ") for line in findings):
                return f"{rule} not reported (findings: {findings or 'none'})"
        if also_clean_with_flag:
            findings, _scanned, excluded = run_check(scratch, (), True)
            if findings or not excluded:
                return "--exclude-oracle-bound did not skip the bound file: " + str(findings)
        return None
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def run_selftest():
    failures = 0
    for name in CASES:
        problem = run_case(name)
        if problem is None:
            print(f"selftest {name}: ok")
        else:
            print(f"selftest {name}: FAILED: {problem}")
            failures += 1
    print(f"check-api-init selftest: cases={len(CASES)} failed={failures}")
    return 1 if failures else 0


# --------------------------------------------------------------------- main


def build_parser():
    parser = argparse.ArgumentParser(
        prog="check-api-init.py", description="init-form guard for the public structs")
    parser.add_argument("--root", default=None, help="tree to scan (default: the repository)")
    parser.add_argument("--exclude-oracle-bound", action="store_true",
                        help="skip the files of EXPECTED_CORPUS_PATHS")
    parser.add_argument("--list-cases", action="store_true")
    parser.add_argument("--case", metavar="NAME")
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("paths", nargs="*")
    return parser


def main(argv):
    global ROOT
    args = build_parser().parse_args(argv)
    if args.list_cases:
        for name in CASES:
            print(name)
        return 0
    try:
        if args.case is not None:
            if args.case not in CASES:
                sys.stderr.write(f"check-api-init: unknown case {args.case}\n")
                return 2
            problem = run_case(args.case)
            if problem is None:
                print(f"selftest {args.case}: ok")
                return 0
            print(f"selftest {args.case}: FAILED: {problem}")
            return 1
        if args.selftest:
            return run_selftest()
        root = pathlib.Path(args.root).resolve() if args.root else ROOT
        findings, scanned, excluded = run_check(root, args.paths, args.exclude_oracle_bound)
    except UsageError as error:
        sys.stderr.write(f"{error}\n")
        return 2
    for line in findings:
        print(line)
    print(f"check-api-init: files={scanned} findings={len(findings)} excluded={len(excluded)}")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
