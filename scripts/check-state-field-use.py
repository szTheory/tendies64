#!/usr/bin/env python3
"""State field-use extractor (plan 09-09, decision D-45).

Lists every member of the state structures reachable from `struct t64_machine`
that core code uses as an index, a length, a shift or a loop bound. The list is
the coverage checklist of the state importer: a member that is read from a blob
and then used in one of these ways must be bounded by a named rule before the
use. This plan provides the `--list` mode only; the check, the mapping data and
the self-test land with plan 09-09.1.

The scan is lexical (comments and literals blanked, balanced-brace function
bodies) and does not resolve types: a member name is attributed wherever the
token `->name` or `.name` appears in a use position, so the list is a
conservative checklist, not a proof. Standard library only; run with
`python3 -B` and delete any generated `__pycache__`.

A local variable that is assigned from an expression holding a member token
carries that member: a later use of the local in one of the site classes below
counts as an indirect use of the member and is printed with a lowercase class
letter (s, l, h, m), next to the uppercase direct classes.

Site classes:
  S  the member occurs inside a subscript bracket
  L  the member is an operand of a comparison inside a `for` or `while`
     condition
  H  the member is (part of) the right operand of a shift
  M  the member is in the length argument of memcpy, memmove, memset or
     memcmp
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
INTERNAL_HEADER = "core/t64_internal.h"
ROOT_STRUCT = "t64_machine"
MEM_FUNCTIONS = ("memcpy", "memmove", "memset", "memcmp")
KEYWORDS = {
    "if", "for", "while", "switch", "return", "sizeof", "else", "do", "case",
}


def strip_c_comments_and_literals(source: str) -> str:
    """Blank comments and literals while preserving newlines."""

    pattern = re.compile(
        r"/\*.*?\*/|//[^\n]*|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'",
        re.DOTALL,
    )

    def blank(match: re.Match[str]) -> str:
        return "".join("\n" if char == "\n" else " " for char in match.group(0))

    return pattern.sub(blank, source)


def matching_brace(text: str, open_index: int) -> int:
    depth = 0
    for index in range(open_index, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return -1


def struct_bodies(source: str) -> dict[str, str]:
    """Map a struct tag or typedef name to its body text."""

    bodies: dict[str, str] = {}
    for match in re.finditer(r"\b(?:typedef\s+)?struct\s*([A-Za-z_]\w*)?\s*\{", source):
        open_index = match.end() - 1
        close_index = matching_brace(source, open_index)
        if close_index < 0:
            continue
        body = source[open_index + 1:close_index]
        tail = re.match(r"\s*([A-Za-z_]\w*)\s*;", source[close_index + 1:])
        is_typedef = source[match.start():match.start() + 7] == "typedef"
        names = []
        if match.group(1):
            names.append(match.group(1))
        if is_typedef and tail:
            names.append(tail.group(1))
        for name in names:
            bodies[name] = body
    return bodies


def split_declarations(body: str) -> list[str]:
    """Split a struct body into declaration statements at brace depth 0."""

    statements = []
    depth = 0
    current = []
    for char in body:
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
        if char == ";" and depth == 0:
            statements.append("".join(current))
            current = []
        else:
            current.append(char)
    return statements


def declared_members(body: str) -> list[tuple[str, list[str]]]:
    """Return (member name, type tokens) for each declarator of a body."""

    result = []
    for statement in split_declarations(body):
        statement = statement.strip()
        if not statement or "{" in statement:
            continue
        parts = []
        depth = 0
        start = 0
        for index, char in enumerate(statement):
            if char in "([":
                depth += 1
            elif char in ")]":
                depth -= 1
            elif char == "," and depth == 0:
                parts.append(statement[start:index])
                start = index + 1
        parts.append(statement[start:])
        first = parts[0]
        type_tokens = re.findall(r"[A-Za-z_]\w*", re.sub(r"\[[^\]]*\]", "", first))
        for position, part in enumerate(parts):
            cleaned = re.sub(r"\[[^\]]*\]", " ", part)
            cleaned = re.sub(r"\(\*\s*([A-Za-z_]\w*)\s*\)\s*\(.*\)", r" \1 ", cleaned)
            names = re.findall(r"[A-Za-z_]\w*", cleaned)
            if not names:
                continue
            name = names[-1]
            if position == 0 and len(names) < 2:
                continue
            result.append((name, type_tokens[:-1] if position == 0 else type_tokens))
    return result


def reachable_members(header_text: str) -> set[str]:
    bodies = struct_bodies(header_text)
    if ROOT_STRUCT not in bodies:
        raise SystemExit("check-state-field-use: struct t64_machine not found")
    seen: set[str] = set()
    members: set[str] = set()
    pending = [ROOT_STRUCT]
    while pending:
        name = pending.pop()
        if name in seen or name not in bodies:
            continue
        seen.add(name)
        for member, type_tokens in declared_members(bodies[name]):
            members.add(member)
            for token in type_tokens:
                if token in bodies and token not in seen:
                    pending.append(token)
    return members


def function_bodies(text: str) -> list[str]:
    """Return the body text of every file-scope function definition."""

    bodies = []
    depth = 0
    index = 0
    length = len(text)
    while index < length:
        char = text[index]
        if char == "{":
            if depth == 0:
                # a function body is introduced by a closing parenthesis
                prefix = text[:index].rstrip()
                if prefix.endswith(")"):
                    close = matching_brace(text, index)
                    if close > index:
                        bodies.append(text[index + 1:close])
                        index = close + 1
                        continue
            depth += 1
        elif char == "}":
            depth = max(0, depth - 1)
        index += 1
    return bodies


def member_pattern(members: set[str]) -> re.Pattern[str]:
    ordered = sorted(members, key=len, reverse=True)
    return re.compile(r"(?:->|(?<![A-Za-z0-9_])\.)\s*(" + "|".join(map(re.escape, ordered)) + r")\b")


def balanced_segment(text: str, start: int, open_char: str, close_char: str) -> int:
    depth = 0
    for index in range(start, len(text)):
        if text[index] == open_char:
            depth += 1
        elif text[index] == close_char:
            depth -= 1
            if depth == 0:
                return index
    return -1


def derive_locals(body: str, pattern: re.Pattern[str]) -> dict[str, set[str]]:
    """Map each local assigned from member tokens to the members it carries."""

    carried: dict[str, set[str]] = {}
    assignment = re.compile(r"(?<![.>\w])([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*=(?!=)\s*([^;]*);")
    for _round in range(3):
        for match in assignment.finditer(body):
            name = match.group(1)
            if name in KEYWORDS:
                continue
            right = match.group(2)
            found = {m.group(1) for m in pattern.finditer(right)}
            for other, members in carried.items():
                if other != name and re.search(r"(?<![.>\w])" + re.escape(other) + r"\b", right):
                    found |= members
            if found:
                carried.setdefault(name, set()).update(found)
    return carried


def uses_in(text: str, pattern: re.Pattern[str], carried: dict[str, set[str]]):
    """Yield (member, direct) for each member token or carried local in text."""

    for match in pattern.finditer(text):
        yield match.group(1), True
    for name, members in carried.items():
        if re.search(r"(?<![.>\w])" + re.escape(name) + r"\b", text):
            for member in members:
                yield member, False


def classify(body: str, pattern: re.Pattern[str], counts: dict[str, dict[str, int]]) -> None:
    carried = derive_locals(body, pattern)

    def add(member: str, site: str, direct: bool) -> None:
        entry = counts.setdefault(member, {})
        key = site if direct else site.lower()
        entry[key] = entry.get(key, 0) + 1

    # S: subscript brackets
    index = 0
    while True:
        index = body.find("[", index)
        if index < 0:
            break
        close = balanced_segment(body, index, "[", "]")
        if close < 0:
            index += 1
            continue
        for member, direct in uses_in(body[index + 1:close], pattern, carried):
            add(member, "S", direct)
        index += 1

    # L: comparisons inside for/while conditions
    for keyword in ("for", "while"):
        for match in re.finditer(r"\b" + keyword + r"\s*\(", body):
            open_index = match.end() - 1
            close = balanced_segment(body, open_index, "(", ")")
            if close < 0:
                continue
            condition = body[open_index + 1:close]
            if keyword == "for":
                parts = condition.split(";")
                condition = parts[1] if len(parts) >= 2 else ""
            if not re.search(r"<=|>=|<|>|==|!=", condition):
                continue
            for member, direct in uses_in(condition, pattern, carried):
                add(member, "L", direct)

    # H: right operand of a shift
    for match in re.finditer(r"(?<![<>])(<<|>>)(?![<>=])", body):
        tail = body[match.end():]
        stop = re.search(r"[;,)&|?:=<>!^]", tail)
        operand = tail[:stop.start()] if stop else tail[:80]
        for member, direct in uses_in(operand, pattern, carried):
            add(member, "H", direct)

    # M: the length argument of the memory functions
    for function in MEM_FUNCTIONS:
        for match in re.finditer(r"\b" + function + r"\s*\(", body):
            open_index = match.end() - 1
            close = balanced_segment(body, open_index, "(", ")")
            if close < 0:
                continue
            arguments = []
            depth = 0
            start = open_index + 1
            for position in range(open_index + 1, close):
                char = body[position]
                if char in "([":
                    depth += 1
                elif char in ")]":
                    depth -= 1
                elif char == "," and depth == 0:
                    arguments.append(body[start:position])
                    start = position + 1
            arguments.append(body[start:close])
            if len(arguments) >= 3:
                for member, direct in uses_in(arguments[2], pattern, carried):
                    add(member, "M", direct)


def source_files() -> list[pathlib.Path]:
    return sorted((ROOT / "core").glob("t64_*.c"))


def inline_header_bodies(header_text: str) -> list[str]:
    bodies = []
    for match in re.finditer(r"\bstatic\s+inline\b[^;{]*\(", header_text):
        open_paren = match.end() - 1
        close_paren = balanced_segment(header_text, open_paren, "(", ")")
        if close_paren < 0:
            continue
        cursor = close_paren + 1
        while cursor < len(header_text) and header_text[cursor].isspace():
            cursor += 1
        if cursor < len(header_text) and header_text[cursor] == "{":
            close = matching_brace(header_text, cursor)
            if close > cursor:
                bodies.append(header_text[cursor + 1:close])
    return bodies


def build_list() -> list[str]:
    header_text = strip_c_comments_and_literals(
        (ROOT / INTERNAL_HEADER).read_text(encoding="utf-8")
    )
    members = reachable_members(header_text)
    pattern = member_pattern(members)
    counts: dict[str, dict[str, int]] = {}
    for path in source_files():
        text = strip_c_comments_and_literals(path.read_text(encoding="utf-8"))
        for body in function_bodies(text):
            classify(body, pattern, counts)
    for body in inline_header_bodies(header_text):
        classify(body, pattern, counts)
    lines = []
    for member in sorted(counts):
        sites = counts[member]
        classes = ",".join(site for site in "SsLlHhMm" if site in sites)
        total = sum(sites.values())
        lines.append(f"{member} :: {classes} :: {total}")
    lines.append(f"members={len(counts)} reachable={len(members)}")
    return lines


def main(argv: list[str]) -> int:
    if argv != ["--list"]:
        sys.stderr.write("usage: check-state-field-use.py --list\n")
        return 2
    for line in build_list():
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
