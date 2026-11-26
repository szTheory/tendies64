#!/usr/bin/env python3
"""Phase 11 initializer codemod (plan 11-05, decision DR-09).

Rewrites the initializer forms of the public in/out structs of
core/include/tendies64/t64_machine.h into the T64_*_INIT macros, so the header
and not the call site owns the layout of those objects:

  T x = {a, b};            ->  T x = MACRO(a, b);          (declaration)
  T x;                     ->  T x = INIT;                 (uninitialized local)
  &(T){a, b}               ->  &(T)MACRO(a, b)             (compound literal)

It is mechanical and re-runnable: a second run over the same files converts
nothing. Sites it cannot convert without a judgment (arrays of the types,
non-zero values for a zero-only type, designated or commented initializers,
several declarators in one statement, a memset of a converted object, which
would clear a leading struct_size after the flip) are reported for hand
conversion; struct members of these types are listed and left alone.

  python3 -B scripts/codemod-phase11.py --dir PATH...      rewrite in place
  python3 -B scripts/codemod-phase11.py --check PATH...    report remaining sites
  python3 -B scripts/codemod-phase11.py --selftest         built-in text cases

PATH is a file or a directory (every *.c and *.h below it). Counts are printed
per file; 0 is a valid count. Exit status: 0 done (check: nothing remains), 1
check found remaining sites or a file could not be read, 2 refused or misused.

Refusals (exit 2, nothing written). The codemod never writes a path listed in
EXPECTED_CORPUS_PATHS of scripts/check-rdp-oracle-authority.py (read with ast
from the shared tree): those files are bound by the Phase 7 oracle request and
only the isolated bound-file commit of the shape wave may touch their call
sites. The one recorded exception is --scratch-root ROOT, which re-bases every
path under ROOT and permits bound paths only when ROOT resolves below build/
of the shared tree, is the git toplevel of its own clone (different from the
shared toplevel), is not a link to a shared bound file, and the marker file
.t64-bound-scratch exists in the parent directory of ROOT. The printer
tools/digest-sweep/t64_digest_sweep.c keeps two branches by design and is never
written, scratch root or not.

Call-site modes (plan 11-12, the flip), with --dir and instead of the
initializer pass: --create rewrites t64_machine_create(A, B, C) into
t64_machine_create(&(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(A, B),
C) (the compound-literal form at every site: a named local "info" would collide
with the many test locals of that name and with a second create in one block)
and gives a brace-initialized t64_profile_descriptor of a non-bound file its
struct_size; --trace inserts (uint32_t)sizeof(t64_trace_record) after the
records argument of t64_machine_trace_drain. Both are re-runnable (a call that
already has the new arity is left alone) and keep the same path refusals.
"""

from __future__ import annotations

import argparse
import ast
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import NoReturn

CHECKER = "scripts/check-rdp-oracle-authority.py"
PRINTER = "tools/digest-sweep/t64_digest_sweep.c"
MARKER = ".t64-bound-scratch"
SUFFIXES = (".c", ".h")
LIMIT = 80


@dataclass(frozen=True)
class Spec:
    """How one public struct is initialized by macro."""

    zero: str | None = None  # macro without arguments for an all-zero object
    args_macro: str | None = None  # macro taking the members, in order
    arity: int = 0  # member count of args_macro
    pad: str = "0u"  # text for a member the brace list leaves out
    pointers: bool = False  # a literal zero member is written NULL
    uninit: str = ""  # initializer text for an uninitialized declaration


SPECS: dict[str, Spec] = {
    "t64_host_allocator": Spec(
        args_macro="T64_HOST_ALLOCATOR_INIT", arity=3, pad="NULL",
        pointers=True, uninit="T64_HOST_ALLOCATOR_INIT(NULL, NULL, NULL)"),
    "t64_stop_request": Spec(
        args_macro="T64_STOP_REQUEST_INIT", arity=2,
        uninit="T64_STOP_REQUEST_INIT(0u, 0u)"),
    "t64_stop_result": Spec(
        zero="T64_STOP_RESULT_INIT", args_macro="T64_STOP_RESULT_INIT_WITH",
        arity=2, uninit="T64_STOP_RESULT_INIT"),
    "t64_cpu_stop_detail": Spec(
        zero="T64_CPU_STOP_DETAIL_INIT", uninit="T64_CPU_STOP_DETAIL_INIT"),
    "t64_output_drain_result": Spec(
        zero="T64_OUTPUT_DRAIN_RESULT_INIT",
        uninit="T64_OUTPUT_DRAIN_RESULT_INIT"),
    "t64_video_packet_info": Spec(
        zero="T64_VIDEO_PACKET_INFO_INIT", uninit="T64_VIDEO_PACKET_INFO_INIT"),
    "t64_audio_packet_info": Spec(
        zero="T64_AUDIO_PACKET_INFO_INIT", uninit="T64_AUDIO_PACKET_INFO_INIT"),
    "t64_state_import_report": Spec(
        zero="T64_STATE_IMPORT_REPORT_INIT",
        uninit="T64_STATE_IMPORT_REPORT_INIT"),
    "t64_save_info": Spec(
        zero="T64_SAVE_INFO_INIT", uninit="T64_SAVE_INFO_INIT"),
    "t64_output_status": Spec(
        zero="T64_OUTPUT_STATUS_INIT", uninit="T64_OUTPUT_STATUS_INIT"),
}
# t64_host_storage (the core fills it) and t64_trace_record (array elements)
# are deliberately not here; t64_profile_descriptor is handled at the flip.

TYPE_ALT = "|".join(sorted(SPECS, key=len, reverse=True))
DECL_RE = re.compile(
    r"(?<![\w])((?:(?:static|const|volatile|register|extern)\s+)*)"
    r"\b(" + TYPE_ALT + r")\b(\s+)(\w+)((?:\s*\[[^\]]*\])*)\s*(=|;|,)")
COMPOUND_RE = re.compile(r"\(\s*(" + TYPE_ALT + r")\s*\)\s*\{")
ZERO_RE = re.compile(
    r"(?:0[uUlL]*|0[xX]0+[uUlL]*|NULL|false|\(\s*void\s*\*\s*\)\s*0"
    r"|UINT(?:8|16|32|64)_C\(\s*0\s*\)|\{\s*(?:0[uUlL]*)?\s*\})")


NONZERO = "non-zero values for a zero-only initializer"
HEADER = "core/include/tendies64/t64_machine.h"
MEMBER_TABLE: dict[str, list[str]] | None = None


def member_names(type_name: str) -> list[str] | None:
    """Member names of a public struct, in order, parsed from the header."""
    global MEMBER_TABLE
    if MEMBER_TABLE is None:
        MEMBER_TABLE = {}
        try:
            header = mask((shared_top() / HEADER).read_text(encoding="utf-8"))
        except OSError:
            header = ""
        for body, name in re.findall(
                r"typedef\s+struct\s*\{([^{}]*)\}\s*(\w+)\s*;", header):
            members = []
            for declaration in body.split(";"):
                found = re.search(r"(\w+)\s*(?:\[[^\]]*\]\s*)*$", declaration.strip())
                if found:
                    members.append(found.group(1))
            MEMBER_TABLE[name] = members
    return MEMBER_TABLE.get(type_name)


@dataclass
class Site:
    kind: str  # brace, uninit, compound (converted); hand; member
    type: str
    line: int
    note: str = ""


@dataclass
class Result:
    text: str
    sites: list[Site] = field(default_factory=list)

    def count(self, kind: str) -> int:
        return sum(1 for site in self.sites if site.kind == kind)

    @property
    def converted(self) -> int:
        return sum(self.count(k) for k in ("brace", "uninit", "compound"))

    @property
    def hand(self) -> int:
        return self.count("hand")


def blank(chars: list[str], start: int, end: int) -> None:
    for index in range(start, min(end, len(chars))):
        if chars[index] != "\n":
            chars[index] = " "


def mask(text: str) -> str:
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


def classify_open(masked: str, pos: int, parent: str | None) -> str:
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


def brace_intervals(masked: str) -> list[tuple[int, int, str]]:
    intervals: list[tuple[int, int, str]] = []
    stack: list[tuple[int, str]] = []
    for pos, char in enumerate(masked):
        if char == "{":
            kind = classify_open(masked, pos, stack[-1][1] if stack else None)
            stack.append((pos, kind))
        elif char == "}" and stack:
            start, kind = stack.pop()
            intervals.append((start, pos, kind))
    for start, kind in stack:
        intervals.append((start, len(masked), kind))
    intervals.sort()
    return intervals


def innermost(intervals: list[tuple[int, int, str]], pos: int):
    found = None
    for interval in intervals:
        if interval[0] > pos:
            break
        if pos < interval[1]:
            found = interval
    return found


def match_brace(masked: str, open_pos: int) -> int:
    depth = 0
    for pos in range(open_pos, len(masked)):
        if masked[pos] == "{":
            depth += 1
        elif masked[pos] == "}":
            depth -= 1
            if depth == 0:
                return pos
    return -1


def split_elements(text: str, masked: str, open_pos: int, close_pos: int):
    """Top-level comma separated element texts of the brace list."""
    elements: list[str] = []
    depth = 0
    begin = open_pos + 1
    for pos in range(open_pos + 1, close_pos):
        char = masked[pos]
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        elif char == "," and depth == 0:
            elements.append(text[begin:pos].strip())
            begin = pos + 1
    elements.append(text[begin:close_pos].strip())
    if elements and elements[-1] == "":
        elements.pop()
    return elements


def convert_init(spec: Spec, elements: list[str]):
    """(macro text, None) or (None, reason for hand conversion)."""
    if any(e == "" for e in elements):
        return None, "empty element in the initializer list"
    if any(e.startswith((".", "[")) for e in elements):
        return None, "designated initializer"
    if any("/*" in e or "//" in e for e in elements):
        return None, "comment inside the initializer"
    if any("\n" in e for e in elements):
        return None, "element spans several lines"
    all_zero = all(ZERO_RE.fullmatch(e) for e in elements)
    if spec.zero is not None and all_zero:
        return spec.zero, None
    if spec.args_macro is None:
        return None, NONZERO
    if len(elements) > spec.arity:
        return None, "more elements than members"
    args = [("NULL" if spec.pointers and ZERO_RE.fullmatch(e) else e)
            for e in elements]
    args += [spec.pad] * (spec.arity - len(args))
    return (spec.args_macro, args), None


def wrap_call(name: str, args: list[str], col: int, trailing: int) -> str:
    pad = " " * (col + len(name) + 1)
    lines: list[str] = []
    cur = name + "("
    cur_col = col
    for index, arg in enumerate(args):
        last = index == len(args) - 1
        piece = arg + (")" if last else ",")
        if cur.endswith("("):
            cur += piece
        elif cur_col + len(cur) + 1 + len(piece) + (trailing if last else 0) \
                <= LIMIT:
            cur += " " + piece
        else:
            lines.append(cur)
            cur = piece
            cur_col = len(pad)
    lines.append(cur)
    return ("\n" + pad).join(lines)


def render(text: str, start: int, end: int, head: str, conv, after_eq: bool):
    """(start, end, replacement) for text[start:end] with the macro call."""
    name, args = (conv, None) if isinstance(conv, str) else conv
    one = name if args is None else f"{name}({', '.join(args)})"
    line_start = text.rfind("\n", 0, start) + 1
    line_end = text.find("\n", end)
    line_end = len(text) if line_end < 0 else line_end
    prefix = text[line_start:start]
    suffix = text[end:line_end]
    if len(prefix) + len(head) + len(one) + len(suffix) <= LIMIT:
        return start, end, head + one
    if after_eq and prefix.rstrip().endswith("="):
        indent = len(prefix) - len(prefix.lstrip()) + 4
        new_start = line_start + len(prefix.rstrip())
        if indent + len(one) + len(suffix) <= LIMIT or args is None:
            return new_start, end, "\n" + " " * indent + one
        return (new_start, end,
                "\n" + " " * indent + wrap_call(name, args, indent, len(suffix)))
    if args is None:
        return start, end, head + one
    # An argument of a call: break before the argument when it follows another
    # argument on its line, then keep or move the arguments after it.
    broke = re.match(r"^(\s*)([^,()]+,)\s*(&?)$", prefix)
    if broke or prefix.strip() in ("", "&"):
        indent = broke.group(1) if broke else prefix[:len(prefix) - len(prefix.lstrip())]
        lead = indent + (broke.group(3) if broke else prefix.strip())
        new_start = line_start + len(broke.group(1)) + len(broke.group(2)) \
            if broke else start
        newline = "\n" if broke else ""
        rest = suffix.strip()
        if len(lead) + len(head) + len(one) + len(suffix) <= LIMIT:
            return new_start, end, newline + (lead if broke else "") + head + one
        moved = rest.startswith(",") and len(rest) > 1
        tail = ",\n" + indent + rest[1:].lstrip() if moved else ""
        stop = line_end if moved else end
        if moved and len(lead) + len(head) + len(one) + 1 <= LIMIT:
            return (new_start, stop,
                    newline + (lead if broke else "") + head + one + tail)
        col = len(lead) + len(head)
        wrapped = wrap_call(name, args, col, 1 if moved else len(suffix))
        return (new_start, stop,
                newline + (lead if broke else "") + head + wrapped + tail)
    col = len(prefix) + len(head)
    return start, end, head + wrap_call(name, args, col, len(suffix))


def process(text: str) -> Result:
    masked = mask(text)
    intervals = brace_intervals(masked)
    depth_before = [0] * (len(masked) + 1)
    for pos, char in enumerate(masked):
        delta = 1 if char == "(" else -1 if char == ")" else 0
        depth_before[pos + 1] = depth_before[pos] + delta
    result = Result(text)
    repls: list[tuple[int, int, str]] = []
    declared: list[tuple[str, int, int]] = []  # name, position, block end

    def line_of(pos: int) -> int:
        return text.count("\n", 0, pos) + 1

    def prev_char(pos: int) -> str:
        idx = pos - 1
        while idx >= 0 and masked[idx].isspace():
            idx -= 1
        return masked[idx] if idx >= 0 else ""

    for match in DECL_RE.finditer(masked):
        quals, type_name, _, name, dims, term = match.groups()
        if "extern" in quals:
            continue
        if depth_before[match.start()] > 0:
            continue
        if prev_char(match.start()) not in ("", ";", "{", "}"):
            continue
        spec = SPECS[type_name]
        scope = innermost(intervals, match.start())
        kind = scope[2] if scope else "file"
        line = line_of(match.start())
        if kind == "init":
            continue
        if kind == "aggregate":
            result.sites.append(Site(
                "member", type_name, line,
                f"{type_name} {name} is a struct member; no initializer is "
                "possible, check how it is filled"))
            continue
        block_end = scope[1] if scope else len(masked)
        declared.append((name, match.end(4), block_end))
        if term == ",":
            result.sites.append(Site(
                "hand", type_name, line, "several declarators in one statement"))
            continue
        if dims.strip():
            if term == "=" and array_uses_init(text, masked, match.end(), spec):
                continue  # every entry already uses the INIT form (11-12)
            result.sites.append(Site(
                "hand", type_name, line, f"array of {type_name}"))
            continue
        if term == ";":
            repls.append((match.end(4), match.end(4), " = " + spec.uninit))
            result.sites.append(Site("uninit", type_name, line))
            continue
        pos = match.end()
        while pos < len(masked) and masked[pos].isspace():
            pos += 1
        if pos >= len(masked) or masked[pos] != "{":
            continue  # an expression: already a macro, a copy or a call
        close = match_brace(masked, pos)
        if close < 0:
            continue
        tail = close + 1
        while tail < len(masked) and masked[tail].isspace():
            tail += 1
        if tail >= len(masked) or masked[tail] != ";":
            result.sites.append(Site(
                "hand", type_name, line, "several declarators in one statement"))
            continue
        elements = split_elements(text, masked, pos, close)
        conv, reason = convert_init(spec, elements)
        if conv is None and reason == NONZERO and kind == "code" \
                and not {"static", "const"} & set(quals.split()):
            # Zero-only type with sentinel values: INIT, then one assignment
            # per non-zero member, by name (a positional list would shift once
            # the flip adds a leading struct_size).
            members = member_names(type_name)
            line_end = text.find("\n", tail)
            line_end = len(text) if line_end < 0 else line_end
            if members and len(elements) <= len(members) \
                    and text[tail + 1:line_end].strip() == "":
                indent = text[text.rfind("\n", 0, match.start()) + 1:match.start()]
                if indent.strip() == "":
                    body = "".join(
                        f"\n{indent}{name}.{member} = {value};"
                        for member, value in zip(members, elements)
                        if not ZERO_RE.fullmatch(value))
                    repls.append((pos, tail + 1, spec.zero + ";" + body))
                    result.sites.append(Site(
                        "brace", type_name, line, "INIT plus member assignments"))
                    continue
        if conv is None:
            result.sites.append(Site("hand", type_name, line, reason))
            continue
        repls.append(render(text, pos, close + 1, "", conv, True))
        result.sites.append(Site("brace", type_name, line))

    for match in COMPOUND_RE.finditer(masked):
        type_name = match.group(1)
        spec = SPECS[type_name]
        open_pos = match.end() - 1
        close = match_brace(masked, open_pos)
        line = line_of(match.start())
        if close < 0:
            continue
        conv, reason = convert_init(spec, split_elements(text, masked, open_pos, close))
        if conv is None:
            result.sites.append(Site("hand", type_name, line, reason))
            continue
        head = text[match.start():open_pos].rstrip()
        repls.append(render(text, match.start(), close + 1, head, conv, False))
        result.sites.append(Site("compound", type_name, line))

    for name, begin, end in declared:
        pattern = re.compile(
            r"\b(?:memset|bzero)\s*\(\s*&?\s*" + re.escape(name) + r"\s*[,)]")
        for found in pattern.finditer(masked, begin, end):
            result.sites.append(Site(
                "hand", "memset", line_of(found.start()),
                f"zeroing call on {name} would clear a leading struct_size "
                "after the flip; replace it by the INIT form"))

    repls.sort()
    accepted: list[tuple[int, int, str]] = []
    last_end = -1
    for start, end, new in repls:
        if start < last_end:
            result.sites.append(Site(
                "hand", "overlap", line_of(start),
                "overlapping rewrites; run the tool again"))
            continue
        accepted.append((start, end, new))
        last_end = max(last_end, end)
    out = text
    for start, end, new in reversed(accepted):
        out = out[:start] + new + out[end:]
    result.text = out
    return result


def array_uses_init(text: str, masked: str, pos: int, spec: Spec) -> bool:
    """True when the brace list at pos (after '=') holds only INIT forms."""
    while pos < len(masked) and masked[pos].isspace():
        pos += 1
    if pos >= len(masked) or masked[pos] != "{":
        return False
    close = match_brace(masked, pos)
    if close < 0:
        return False
    elements = split_elements(text, masked, pos, close)
    if not elements:
        return False
    for element in elements:
        if element.startswith("{") and element.endswith("}"):
            if array_uses_init(element, mask(element), 0, spec):
                continue  # a row of a multi-dimensional array
            return False
        if spec.zero is not None and element == spec.zero:
            continue
        if spec.args_macro is not None and re.fullmatch(
                re.escape(spec.args_macro) + r"\s*\(.*\)", element, re.S):
            continue
        return False
    return True


# ------------------------------------------------- create and trace (11-12)

CREATE_INFO = "&(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT"
RECORD_SIZE = "(uint32_t)sizeof(t64_trace_record)"
DESCRIPTOR_SIZE = "(uint32_t)sizeof(t64_profile_descriptor)"
CALL_RE = re.compile(r"\b(t64_machine_create|t64_machine_trace_drain)\s*\(")
DESCRIPTOR_RE = re.compile(
    r"(?<![\w])((?:(?:static|const)\s+)*)\bt64_profile_descriptor\s+(\w+)\s*=\s*\{")


def call_arguments(text: str, masked: str, open_pos: int):
    """(argument spans, close position) of the call whose '(' is at open_pos."""
    depth = 0
    spans: list[tuple[int, int]] = []
    begin = open_pos + 1
    for pos in range(open_pos, len(masked)):
        char = masked[pos]
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
            if depth == 0:
                spans.append((begin, pos))
                return spans, pos
        elif char == "," and depth == 1:
            spans.append((begin, pos))
            begin = pos + 1
    return spans, -1


def flat(text: str) -> str:
    return re.sub(r"\s+", " ", text.strip())


def render_create(text: str, start: int, close: int, args: list[str]) -> str:
    line_start = text.rfind("\n", 0, start) + 1
    col = start - line_start
    line_end = text.find("\n", close)
    line_end = len(text) if line_end < 0 else line_end
    suffix = len(text[close + 1:line_end])
    init = f"{CREATE_INFO}({args[0]}, {args[1]})"
    one = f"t64_machine_create({init}, {args[2]})"
    if col + len(one) + suffix <= LIMIT:
        return one
    pad = " " * (col + 4)
    if col + 4 + len(init) + 2 + len(args[2]) + 1 + suffix <= LIMIT:
        return f"t64_machine_create(\n{pad}{init}, {args[2]})"
    if col + 4 + len(init) + 1 <= LIMIT:
        return f"t64_machine_create(\n{pad}{init},\n{pad}{args[2]})"
    inner = " " * (col + 8)
    return (f"t64_machine_create(\n{pad}{CREATE_INFO}(\n{inner}{args[0]}, "
            f"{args[1]}),\n{pad}{args[2]})")


def process_calls(text: str, create: bool, trace: bool,
                  descriptors: bool) -> Result:
    """Rewrite t64_machine_create into the create-info form (--create) and
    insert the trace record size after the records argument (--trace)."""
    masked = mask(text)
    result = Result(text)
    repls: list[tuple[int, int, str]] = []

    def line_of(pos: int) -> int:
        return text.count("\n", 0, pos) + 1

    for match in CALL_RE.finditer(masked):
        name = match.group(1)
        if name == "t64_machine_create" and not create:
            continue
        if name == "t64_machine_trace_drain" and not trace:
            continue
        before = masked[:match.start()].rstrip()
        word = re.search(r"(\w+)$", before)
        if word and word.group(1) != "return":
            continue  # a declaration or definition, not a call
        open_pos = match.end() - 1
        spans, close = call_arguments(text, masked, open_pos)
        line = line_of(match.start())
        if close < 0:
            result.sites.append(Site("hand", name, line, "unbalanced call"))
            continue
        args = [text[b:e] for b, e in spans]
        masked_args = [masked[b:e] for b, e in spans]
        if any("/*" in t or "//" in t for t in args) \
                or any('"' in m or "'" in m for m in masked_args):
            result.sites.append(Site(
                "hand", name, line, "comment or literal inside the arguments"))
            continue
        if name == "t64_machine_create":
            if len(args) == 2:
                continue  # already the create-info form
            if len(args) != 3:
                result.sites.append(Site(
                    "hand", name, line, f"{len(args)} arguments"))
                continue
            new = render_create(text, match.start(), close,
                                [flat(a) for a in args])
            repls.append((match.start(), close + 1, new))
            result.sites.append(Site("create", name, line))
        else:
            if len(args) == 7:
                continue  # already carries the record size
            if len(args) != 6:
                result.sites.append(Site(
                    "hand", name, line, f"{len(args)} arguments"))
                continue
            comma = spans[1][1]
            line_start = text.rfind("\n", 0, comma) + 1
            line_end = text.find("\n", comma)
            line_end = len(text) if line_end < 0 else line_end
            if len(text[line_start:line_end]) + len(RECORD_SIZE) + 2 <= LIMIT:
                new = f", {RECORD_SIZE},"
            else:
                arg_start = spans[0][0]
                while text[arg_start].isspace():
                    arg_start += 1
                indent = arg_start - (text.rfind("\n", 0, arg_start) + 1)
                new = ",\n" + " " * indent + RECORD_SIZE + ","
            repls.append((comma, comma + 1, new))
            result.sites.append(Site("trace", name, line))

    if descriptors:
        for match in DESCRIPTOR_RE.finditer(masked):
            open_pos = match.end() - 1
            close = match_brace(masked, open_pos)
            if close < 0:
                continue
            elements = split_elements(text, masked, open_pos, close)
            line = line_of(match.start())
            if elements and (elements[0].startswith(".struct_size")
                             or elements[0] == DESCRIPTOR_SIZE):
                continue
            if all(ZERO_RE.fullmatch(e) for e in elements):
                # Designated, so the other members stay zero without a
                # missing-field diagnostic (C99; the callers are C).
                body = "{.struct_size = " + DESCRIPTOR_SIZE + "}"
                line_start = text.rfind("\n", 0, open_pos) + 1
                line_end = text.find("\n", close)
                line_end = len(text) if line_end < 0 else line_end
                head = text[line_start:open_pos]
                if len(head) + len(body) + line_end - close - 1 > LIMIT \
                        and head.rstrip().endswith("="):
                    indent = len(head) - len(head.lstrip()) + 4
                    repls.append((line_start + len(head.rstrip()), close + 1,
                                  "\n" + " " * indent + body))
                else:
                    repls.append((open_pos, close + 1, body))
            elif elements and elements[0].startswith("."):
                first = open_pos + 1
                while first < close and masked[first] in " \t":
                    first += 1
                if masked[first] == "\n":
                    start = first + 1
                    while masked[start] in " \t":
                        start += 1
                    indent = text[first + 1:start]
                    repls.append((first + 1, first + 1,
                                  f"{indent}.struct_size = {DESCRIPTOR_SIZE},\n"))
                else:
                    repls.append((open_pos + 1, open_pos + 1,
                                  f".struct_size = {DESCRIPTOR_SIZE}, "))
            else:
                repls.append((open_pos + 1, open_pos + 1, DESCRIPTOR_SIZE + ", "))
            result.sites.append(Site("descriptor", "t64_profile_descriptor", line))

    repls.sort()
    out = text
    for start, end, new in reversed(repls):
        out = out[:start] + new + out[end:]
    result.text = out
    return result


# ---------------------------------------------------------------- path safety


def shared_top() -> Path:
    here = Path(__file__).resolve().parent
    try:
        out = subprocess.run(
            ["git", "-C", str(here), "rev-parse", "--show-toplevel"],
            check=True, capture_output=True, text=True).stdout.strip()
        return Path(out).resolve()
    except (OSError, subprocess.CalledProcessError):
        return here.parent


def refuse(message: str) -> NoReturn:
    print(f"codemod-phase11: {message}", file=sys.stderr)
    raise SystemExit(2)


def load_bound(top: Path) -> frozenset[str]:
    """EXPECTED_CORPUS_PATHS of the shared tree's authority checker, by ast."""
    checker = top / CHECKER
    try:
        tree = ast.parse(checker.read_text(encoding="utf-8"))
    except (OSError, SyntaxError, UnicodeDecodeError) as error:
        refuse(f"cannot read {CHECKER}: {error}")
    for node in tree.body:
        targets = []
        if isinstance(node, ast.Assign):
            targets = [t for t in node.targets if isinstance(t, ast.Name)]
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            targets = [node.target]
        if any(t.id == "EXPECTED_CORPUS_PATHS" for t in targets):
            try:
                value = ast.literal_eval(node.value)
            except ValueError as error:
                refuse(f"EXPECTED_CORPUS_PATHS is not a literal: {error}")
            if (not isinstance(value, (set, frozenset, list, tuple)) or not value
                    or not all(isinstance(p, str) for p in value)):
                refuse("EXPECTED_CORPUS_PATHS is not a non-empty set of strings")
            return frozenset(value)
    refuse(f"EXPECTED_CORPUS_PATHS not found in {CHECKER}")


def is_bound(real: Path, top: Path, bound: frozenset[str]) -> bool:
    try:
        rel = real.relative_to(top).as_posix()
    except ValueError:
        rel = ""
    if rel in bound:
        return True
    for path in bound:
        candidate = top / path
        try:
            if candidate.exists() and real.exists() and os.path.samefile(real, candidate):
                return True
        except OSError:
            continue
    return False


def validate_scratch_root(root_arg: str, top: Path) -> Path:
    root = Path(root_arg)
    problems: list[str] = []
    real = root.resolve()
    if not real.is_dir():
        refuse(f"scratch root refused: {root_arg} is not a directory")
    build = top / "build"
    try:
        below = real.relative_to(build.resolve()).parts != ()
    except ValueError:
        below = False
    if not below:
        problems.append("it is not below build/ of the shared tree")
    try:
        found = subprocess.run(
            ["git", "-C", str(real), "rev-parse", "--show-toplevel"],
            check=True, capture_output=True, text=True).stdout.strip()
        clone_top = Path(found).resolve()
    except (OSError, subprocess.CalledProcessError):
        clone_top = None
    if clone_top is None or clone_top != real:
        problems.append("it is not the git toplevel of its own clone")
    elif clone_top == top:
        problems.append("it is the shared toplevel")
    if not (real.parent / MARKER).is_file():
        problems.append(f"{MARKER} is missing in its parent directory")
    if problems:
        refuse("scratch root refused: " + "; ".join(problems))
    return real


def expand(paths: list[str], base: Path, writing: bool, top: Path,
           bound: frozenset[str], scratch: bool):
    """(files to process, excluded notes). Refuses (exit 2) on a bound or
    printer path named explicitly when writing outside a scratch root."""
    files: list[Path] = []
    excluded: list[str] = []
    real_base = base.resolve()
    printer = (real_base / PRINTER)

    def excluded_reason(real: Path, explicit: bool):
        if printer.exists() and real.exists() and os.path.samefile(real, printer):
            return "printer (two-branch source, never written)"
        if not scratch and is_bound(real, top, bound):
            return "oracle-bound"
        return None

    for raw in paths:
        candidate = Path(raw)
        if not candidate.is_absolute():
            candidate = (base / candidate) if scratch else (Path.cwd() / candidate)
        real = candidate.resolve()
        try:
            real.relative_to(real_base)
        except ValueError:
            refuse(f"{raw} is outside {'the scratch root' if scratch else 'the shared tree'}")
        if not real.exists():
            refuse(f"{raw} does not exist")
        if real.is_dir():
            for dirpath, dirnames, filenames in os.walk(real):
                dirnames[:] = sorted(d for d in dirnames if d != ".git")
                for filename in sorted(filenames):
                    if not filename.endswith(SUFFIXES):
                        continue
                    child = Path(dirpath, filename).resolve()
                    reason = excluded_reason(child, False)
                    if reason:
                        excluded.append(f"{child.relative_to(real_base).as_posix()} ({reason})")
                    else:
                        files.append(child)
        else:
            reason = excluded_reason(real, True)
            if reason and writing:
                refuse(f"refused {real.relative_to(real_base).as_posix()}: {reason}")
            if reason:
                excluded.append(f"{real.relative_to(real_base).as_posix()} ({reason})")
            else:
                files.append(real)
    if scratch and writing:
        for path in files:
            for shared in bound:
                shared_path = top / shared
                if shared_path.exists() and os.path.samefile(path, shared_path):
                    refuse(f"scratch root refused: {path.name} is a link to a shared bound file")
    return sorted(set(files)), sorted(set(excluded))


# ------------------------------------------------------------------ commands


def run(args: argparse.Namespace) -> int:
    writing = args.dir is not None
    paths = args.dir if writing else args.check
    top = shared_top()
    bound = load_bound(top)
    scratch = args.scratch_root is not None
    base = validate_scratch_root(args.scratch_root, top) if scratch else top
    files, excluded = expand(paths, base, writing, top, bound, scratch)
    total_converted = total_hand = 0
    failed = False
    calls = args.create or args.trace
    for note in excluded:
        print(f"excluded: {note}")
    for path in files:
        rel = path.relative_to(base.resolve()).as_posix()
        try:
            source = path.read_bytes().decode("utf-8")
        except (OSError, UnicodeDecodeError) as error:
            print(f"error: {rel}: {error}", file=sys.stderr)
            failed = True
            continue
        if calls:
            result = process_calls(source, args.create, args.trace,
                                   args.create and rel not in bound)
            for site in result.sites:
                if site.kind == "hand":
                    print(f"hand: {rel}:{site.line}: {site.type}: {site.note}")
            if result.text != source:
                path.write_bytes(result.text.encode("utf-8"))
            changed = (result.count("create") + result.count("trace")
                       + result.count("descriptor"))
            print(f"{rel}: converted {changed} (create {result.count('create')}, "
                  f"trace {result.count('trace')}, descriptor "
                  f"{result.count('descriptor')}), hand {result.hand}")
            total_converted += changed
            total_hand += result.hand
            continue
        result = process(source)
        for site in result.sites:
            if site.kind == "hand":
                print(f"hand: {rel}:{site.line}: {site.type}: {site.note}")
            elif site.kind == "member":
                print(f"member: {rel}:{site.line}: {site.note}")
        if writing:
            if result.text != source:
                path.write_bytes(result.text.encode("utf-8"))
            print(f"{rel}: converted {result.converted} "
                  f"(brace {result.count('brace')}, uninit {result.count('uninit')}, "
                  f"compound {result.count('compound')}), hand {result.hand}")
        elif result.converted + result.hand:
            print(f"{rel}: remaining {result.converted + result.hand} "
                  f"(convertible {result.converted}, hand {result.hand})")
        total_converted += result.converted
        total_hand += result.hand
    verb = "converted" if writing else "remaining"
    print(f"total: {verb} {total_converted if writing else total_converted + total_hand}, "
          f"hand {total_hand}, files {len(files)}, excluded {len(excluded)}")
    if failed:
        return 1
    if not writing and total_converted + total_hand:
        return 1
    return 0


SELFTEST_CASES = [
    # name, source, expected output
    ("allocator positional",
     "  t64_host_allocator a = {f, g, NULL};\n",
     "  t64_host_allocator a = T64_HOST_ALLOCATOR_INIT(f, g, NULL);\n"),
    ("allocator static const, zero context padded",
     "  static const t64_host_allocator a = {f, g};\n",
     "  static const t64_host_allocator a = T64_HOST_ALLOCATOR_INIT(f, g, NULL);\n"),
    ("allocator all zero",
     "  t64_host_allocator a = {0};\n",
     "  t64_host_allocator a = T64_HOST_ALLOCATOR_INIT(NULL, NULL, NULL);\n"),
    ("allocator wrapped after the equals sign",
     "  static const t64_host_allocator allocator = {\n"
     "      allocator_allocate, allocator_release, NULL};\n",
     "  static const t64_host_allocator allocator =\n"
     "      T64_HOST_ALLOCATOR_INIT(allocator_allocate, allocator_release, NULL);\n"),
    ("stop result zero and with",
     "  t64_stop_result a = {0u, 0u};\n  t64_stop_result b = {0};\n"
     "  t64_stop_result c = {UINT32_MAX, UINT64_MAX};\n"
     "  t64_stop_result d = {T64_STOP_REASON_BOUND, 0u};\n",
     "  t64_stop_result a = T64_STOP_RESULT_INIT;\n"
     "  t64_stop_result b = T64_STOP_RESULT_INIT;\n"
     "  t64_stop_result c = T64_STOP_RESULT_INIT_WITH(UINT32_MAX, UINT64_MAX);\n"
     "  t64_stop_result d = T64_STOP_RESULT_INIT_WITH(T64_STOP_REASON_BOUND, 0u);\n"),
    ("stop request declaration and compound literal",
     "  const t64_stop_request r = {T64_STOP_GUEST_TICK, tick};\n"
     "  x = f(m, &(t64_stop_request){K, target}, &s);\n"
     "  x = f(m, &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &s);\n",
     "  const t64_stop_request r = T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, tick);\n"
     "  x = f(m, &(t64_stop_request)T64_STOP_REQUEST_INIT(K, target), &s);\n"
     "  x = f(m, &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK,\n"
     "                                                    target), &s);\n"),
    ("compound literal argument moves the arguments after it to the next line",
     "  status = t64_machine_advance(\n      machine,\n"
     "      &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);\n",
     "  status = t64_machine_advance(\n      machine,\n"
     "      &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, target),\n"
     "      &stop);\n"),
    ("compound literal after another argument breaks before the argument",
     "    t64_status status = t64_machine_advance(\n"
     "        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);\n"
     "    if (t64_machine_advance(\n"
     "            machine, &(t64_stop_request){T64_STOP_GUEST_TICK, target},\n"
     "            &stop) != T64_STATUS_OK) {}\n",
     "    t64_status status = t64_machine_advance(\n"
     "        machine,\n"
     "        &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, target),\n"
     "        &stop);\n"
     "    if (t64_machine_advance(\n"
     "            machine,\n"
     "            &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK,\n"
     "                                                     target),\n"
     "            &stop) != T64_STATUS_OK) {}\n"),
    ("uninitialized locals and file scope",
     "t64_stop_result g;\nvoid f(void) {\n  t64_stop_request r;\n"
     "  t64_save_info info;\n  t64_state_import_report rep;\n}\n",
     "t64_stop_result g = T64_STOP_RESULT_INIT;\nvoid f(void) {\n"
     "  t64_stop_request r = T64_STOP_REQUEST_INIT(0u, 0u);\n"
     "  t64_save_info info = T64_SAVE_INFO_INIT;\n"
     "  t64_state_import_report rep = T64_STATE_IMPORT_REPORT_INIT;\n}\n"),
    ("zero-only types with brace zero",
     "  t64_output_drain_result r = {0};\n  t64_cpu_stop_detail d = {0u, 0u, 0u};\n",
     "  t64_output_drain_result r = T64_OUTPUT_DRAIN_RESULT_INIT;\n"
     "  t64_cpu_stop_detail d = T64_CPU_STOP_DETAIL_INIT;\n"),
    ("members, parameters, pointers, prototypes, extern, typedef, comments and strings stay",
     "typedef struct {\n  t64_video_packet_info info;\n} entry;\n"
     "static bool f(t64_stop_result stop, const t64_audio_packet_info *p);\n"
     "static t64_stop_result g(void);\nextern t64_stop_result e;\n"
     "typedef t64_stop_result alias;\n"
     "/* t64_stop_result c; */\nconst char *s = \"t64_stop_result s;\";\n"
     "#define M t64_stop_result m;\n",
     "typedef struct {\n  t64_video_packet_info info;\n} entry;\n"
     "static bool f(t64_stop_result stop, const t64_audio_packet_info *p);\n"
     "static t64_stop_result g(void);\nextern t64_stop_result e;\n"
     "typedef t64_stop_result alias;\n"
     "/* t64_stop_result c; */\nconst char *s = \"t64_stop_result s;\";\n"
     "#define M t64_stop_result m;\n"),
    ("sentinel values: INIT plus member assignments by name",
     "void f(void) {\n  t64_cpu_stop_detail d = {UINT64_MAX, UINT32_MAX, 0u};\n"
     "  use(&d);\n}\n",
     "void f(void) {\n  t64_cpu_stop_detail d = T64_CPU_STOP_DETAIL_INIT;\n"
     "  d.retired_instructions = UINT64_MAX;\n  d.pc = UINT32_MAX;\n"
     "  use(&d);\n}\n"),
    ("hand sites stay untouched",
     "static const t64_cpu_stop_detail s = {1u, 2u, 3u};\n"
     "void f(void) {\n"
     "  t64_stop_result rs[2];\n  t64_stop_result a, b;\n"
     "  t64_host_allocator h = {.allocate = f};\n"
     "  t64_state_import_report rep;\n  memset(&rep, 0, sizeof rep);\n}\n",
     "static const t64_cpu_stop_detail s = {1u, 2u, 3u};\n"
     "void f(void) {\n"
     "  t64_stop_result rs[2];\n  t64_stop_result a, b;\n"
     "  t64_host_allocator h = {.allocate = f};\n"
     "  t64_state_import_report rep = T64_STATE_IMPORT_REPORT_INIT;\n"
     "  memset(&rep, 0, sizeof rep);\n}\n"),
    ("already converted text is stable",
     "  t64_stop_result a = T64_STOP_RESULT_INIT;\n"
     "  t64_output_status s = T64_OUTPUT_STATUS_INIT;\n"
     "  t64_save_info x = other;\n",
     "  t64_stop_result a = T64_STOP_RESULT_INIT;\n"
     "  t64_output_status s = T64_OUTPUT_STATUS_INIT;\n"
     "  t64_save_info x = other;\n"),
]
CALL_CASES = [
    # name, source, expected output (create and trace modes, non-bound file)
    ("create in a check macro",
     "  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),\n"
     "                T64_STATUS_OK);\n",
     "  T64_CHECK_INT(t64_machine_create(\n"
     "                    &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(\n"
     "                        &allocator, profile),\n"
     "                    &machine),\n"
     "                T64_STATUS_OK);\n"),
    ("create short form, return statement and declarations stay",
     "t64_status t64_machine_create(const t64_machine_create_info *i,\n"
     "                              t64_machine **m);\n"
     "  return t64_machine_create(a, p, m);\n",
     "t64_status t64_machine_create(const t64_machine_create_info *i,\n"
     "                              t64_machine **m);\n"
     "  return t64_machine_create(\n"
     "             &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(a, p), m);\n"),
    ("trace drain record size",
     "  s = t64_machine_trace_drain(m, r, 4u, &c, &d, &f);\n"
     "  s = t64_machine_trace_drain(machine, records, T64_TRACE_RECORD_CAPACITY,\n"
     "                              &count, &dropped, &saturated);\n",
     "  s = t64_machine_trace_drain(m, r,\n"
     "                              (uint32_t)sizeof(t64_trace_record), 4u, &c, &d, &f);\n"
     "  s = t64_machine_trace_drain(machine, records,\n"
     "                              (uint32_t)sizeof(t64_trace_record), T64_TRACE_RECORD_CAPACITY,\n"
     "                              &count, &dropped, &saturated);\n"),
    ("zero descriptor gets its struct_size",
     "  t64_profile_descriptor empty = {0};\n",
     "  t64_profile_descriptor empty =\n"
     "      {.struct_size = (uint32_t)sizeof(t64_profile_descriptor)};\n"),
    ("designated descriptor gets a struct_size line",
     "static const t64_profile_descriptor d = {\n    .profile_id = \"A\",\n};\n",
     "static const t64_profile_descriptor d = {\n"
     "    .struct_size = (uint32_t)sizeof(t64_profile_descriptor),\n"
     "    .profile_id = \"A\",\n};\n"),
]

SELFTEST_HAND = {
    # static non-zero, array, declarators, designated, memset
    "hand sites stay untouched": 5,
}


def selftest() -> int:
    global MEMBER_TABLE
    MEMBER_TABLE = {"t64_cpu_stop_detail": ["retired_instructions", "pc", "opcode"]}
    failures = 0
    for name, source, expected in SELFTEST_CASES:
        first = process(source)
        second = process(first.text)
        ok = first.text == expected and second.text == first.text \
            and second.converted == 0
        want_hand = SELFTEST_HAND.get(name, 0)
        if first.hand != want_hand:
            ok = False
        if not ok:
            failures += 1
            print(f"selftest FAIL: {name}\n--- got\n{first.text}--- want\n{expected}"
                  f"(hand {first.hand}, want {want_hand}; second pass converted "
                  f"{second.converted})")
        else:
            print(f"selftest ok: {name}")
    for name, source, expected in CALL_CASES:
        first = process_calls(source, True, True, True)
        second = process_calls(first.text, True, True, True)
        ok = first.text == expected and second.text == first.text \
            and first.hand == 0
        if not ok:
            failures += 1
            print(f"selftest FAIL: {name}\n--- got\n{first.text}--- want\n{expected}")
        else:
            print(f"selftest ok: {name}")
    total = len(SELFTEST_CASES) + len(CALL_CASES)
    print(f"selftest: {total - failures} of {total} passed")
    return 1 if failures else 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        prog="codemod-phase11.py", description=__doc__.split("\n")[0])
    parser.add_argument("--dir", nargs="+", metavar="PATH",
                        help="rewrite these files or directories in place")
    parser.add_argument("--check", nargs="+", metavar="PATH",
                        help="report remaining sites without writing")
    parser.add_argument("--scratch-root", metavar="ROOT",
                        help="re-base paths under a marked scratch clone")
    parser.add_argument("--create", action="store_true",
                        help="with --dir: rewrite t64_machine_create calls into "
                        "the create-info form (plan 11-12)")
    parser.add_argument("--trace", action="store_true",
                        help="with --dir: insert the record size into "
                        "t64_machine_trace_drain calls (plan 11-12)")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    if args.selftest:
        return selftest()
    if (args.create or args.trace) and args.dir is None:
        print("codemod-phase11: --create and --trace need --dir", file=sys.stderr)
        return 2
    if (args.dir is None) == (args.check is None):
        print("codemod-phase11: give exactly one of --dir and --check", file=sys.stderr)
        return 2
    return run(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
