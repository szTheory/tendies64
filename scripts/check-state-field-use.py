#!/usr/bin/env python3
"""State field-use extractor and mapped audit (plans 09-09 and 09-09.1, D-45).

Lists every member of the state structures reachable from `struct t64_machine`
that core code uses as an index, a length, a shift or a loop bound (`--list`),
and checks that each such member is mapped to a closed importer rule and to a
named reader function of core/t64_state.c whose body mentions it (`--check`).
The mapping is the data file docs/spec/state-field-rules.txt. The audit proves
that every used member has a named rule in a named reader; it does not prove
that the rule is correct, which is the work of the structure-aware mutants and
the sweep of the state import test.

Modes (exit 0 clean, 1 findings, 2 usage):
  --list           the used-member list (plan 09-09)
  --check          the mapped audit; prints one finding per line
  --selftest       the five self-test cases on temporary copies of the tree
  --list-cases     the self-test case names, one per line, nothing else
  --case NAME      one self-test case
  --emit-template  one mapping line per used member with suggestions
  --root DIR       the source tree to read (default: this script's repository)

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

Mapping row format: `member :: RULE :: reader_function`, with the optional
fourth and fifth fields `:: core/<file>.c :: function` for MASKED_AT_USE (the
consuming site). Lines beginning with `#` are comments. The rule names are the
T64_STATE_RULE_* names of core/include/tendies64/t64_machine.h without the
prefix; NONE names an aggregate member (a structure whose scalar members are
mapped under their own names).
"""

from __future__ import annotations

import pathlib
import re
import shutil
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
INTERNAL_HEADER = "core/t64_internal.h"
MACHINE_HEADER = "core/include/tendies64/t64_machine.h"
STATE_SOURCE = "core/t64_state.c"
MAPPING_FILE = "docs/spec/state-field-rules.txt"
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


# ---- the mapped audit (plan 09-09.1) -------------------------------------


def used_members() -> dict[str, str]:
    """Map each used member to its site classes (from the --list scan)."""

    used: dict[str, str] = {}
    for line in build_list():
        if line.startswith("members="):
            continue
        member, classes, _count = line.split(" :: ")
        used[member] = classes
    return used


def closed_rules(root: pathlib.Path) -> list[str]:
    text = (root / MACHINE_HEADER).read_text(encoding="utf-8")
    return re.findall(r"#define\s+T64_STATE_RULE_([A-Z0-9_]+)\s", text)


def function_table(text: str) -> dict[str, str]:
    """Map the name of every file-scope function definition to its body."""

    table: dict[str, str] = {}
    depth = 0
    index = 0
    length = len(text)
    while index < length:
        char = text[index]
        if char == "{":
            if depth == 0:
                prefix = text[:index].rstrip()
                if prefix.endswith(")"):
                    close = matching_brace(text, index)
                    if close > index:
                        # walk back to the parenthesis that opens the parameters
                        nesting = 0
                        position = len(prefix) - 1
                        while position >= 0:
                            if prefix[position] == ")":
                                nesting += 1
                            elif prefix[position] == "(":
                                nesting -= 1
                                if nesting == 0:
                                    break
                            position -= 1
                        name = re.search(r"([A-Za-z_]\w*)\s*$", prefix[:position])
                        if name:
                            table[name.group(1)] = text[index + 1:close]
                        index = close + 1
                        continue
            depth += 1
        elif char == "}":
            depth = max(0, depth - 1)
        index += 1
    return table


def mentions(body: str, member: str) -> bool:
    return re.search(r"(?<![A-Za-z0-9_])" + re.escape(member) + r"(?![A-Za-z0-9_])", body) is not None


def stripped(path: pathlib.Path) -> str:
    return strip_c_comments_and_literals(path.read_text(encoding="utf-8"))


def parse_mapping(root: pathlib.Path) -> tuple[list[list[str]], list[str]]:
    """Return the mapping rows (each a list of fields) and the parse findings."""

    rows: list[list[str]] = []
    findings: list[str] = []
    path = root / MAPPING_FILE
    if not path.is_file():
        return rows, findings
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        fields = [field.strip() for field in line.split(" :: ")]
        if len(fields) not in (3, 5) or not all(fields):
            findings.append(f"malformed mapping row at line {number}")
            continue
        rows.append(fields)
    return rows, findings


def check_tree(root: pathlib.Path) -> list[str]:
    """Run the audit over a source tree; returns the findings."""

    global ROOT
    saved = ROOT
    ROOT = root
    try:
        used = used_members()
        rules = closed_rules(root)
        readers = function_table(stripped(root / STATE_SOURCE))
        rows, findings = parse_mapping(root)
        seen: set[str] = set()
        by_member: dict[str, list[str]] = {}
        for row in rows:
            member = row[0]
            if member in seen:
                findings.append(f"duplicate mapping row {member}")
                continue
            seen.add(member)
            by_member[member] = row
        for member in sorted(used):
            if member not in by_member:
                findings.append(f"unmapped member {member} (classes {used[member]})")
        for member, row in sorted(by_member.items()):
            if member not in used:
                findings.append(f"stale mapping row {member}")
                continue
            rule, reader = row[1], row[2]
            if rule in ("?", "TODO", "TBD") or reader in ("?", "TODO", "TBD"):
                findings.append(f"placeholder rule for {member}")
                continue
            if rule not in rules:
                findings.append(f"unknown rule {rule} for {member}")
                continue
            if reader not in readers:
                findings.append(f"reader {reader} not found for {member}")
            elif not mentions(readers[reader], member):
                findings.append(f"reader {reader} does not mention {member}")
            if rule == "MASKED_AT_USE":
                site_ok = False
                if len(row) == 5:
                    site_path = root / row[3]
                    if site_path.is_file() and row[3].startswith("core/"):
                        sites = function_table(stripped(site_path))
                        site_ok = row[4] in sites and mentions(sites[row[4]], member)
                if not site_ok:
                    findings.append(f"masked-at-use row {member} lacks a consuming site")
            elif len(row) == 5:
                findings.append(f"unexpected consuming site for {member}")
        return findings
    finally:
        ROOT = saved


def emit_template(root: pathlib.Path) -> list[str]:
    """One line per used member with the reader and rule suggested from the
    state reader code; every unresolved suggestion is the placeholder `?`."""

    global ROOT
    saved = ROOT
    ROOT = root
    try:
        used = used_members()
        readers = function_table(stripped(root / STATE_SOURCE))
        lines = []
        for member in sorted(used):
            candidates = [
                name for name, body in readers.items()
                if (name.startswith("state_section_") or name.startswith("state_check_"))
                and mentions(body, member)
            ]
            reader = candidates[0] if len(candidates) == 1 else "?"
            rule_names: set[str] = set()
            for name in candidates:
                for statement in readers[name].split(";"):
                    if mentions(statement, member):
                        rule_names.update(
                            r for r in re.findall(r"T64_STATE_RULE_([A-Z0-9_]+)", statement)
                            if r != "NONE"
                        )
            rule = next(iter(rule_names)) if len(rule_names) == 1 else "?"
            lines.append(f"{member} :: {rule} :: {reader}")
        return lines
    finally:
        ROOT = saved


# ---- the self-test (compiled-in seeds, run on temporary copies) ------------

SELFTEST_CASES = (
    "baseline-clean",
    "new-indexed-member-unmapped",
    "mapping-row-removed",
    "unknown-rule-name",
    "reader-lacks-member",
)

# The member the first seed adds to a copied structure and the function that
# uses it as a subscript; neither exists in the real tree.
SEED_MEMBER = "seeded_audit_index"
SEED_STRUCT_ANCHOR = "typedef struct {\n  uint32_t dram_address;\n  uint32_t cart_address;"
SEED_FUNCTION = (
    "\nstatic uint32_t seeded_audit_use(const t64_machine *machine,\n"
    "                                   const uint32_t *table) {\n"
    "  return table[machine->pi.seeded_audit_index];\n"
    "}\n"
)
SEED_UNKNOWN_RULE = "NO_SUCH_RULE"
SEED_WRONG_READER = "state_rd32"

COPIED_FILES = (INTERNAL_HEADER, MACHINE_HEADER, MAPPING_FILE)


def copy_tree(source: pathlib.Path, destination: pathlib.Path) -> None:
    names = list(COPIED_FILES) + [
        str(path.relative_to(source)) for path in sorted((source / "core").glob("t64_*.c"))
    ]
    for name in names:
        source_path = source / name
        if not source_path.is_file():
            continue
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source_path, target)


def mapping_rows(path: pathlib.Path) -> list[str]:
    return path.read_text(encoding="utf-8").splitlines()


def first_row_index(lines: list[str]) -> int:
    for index, line in enumerate(lines):
        if line.strip() and not line.strip().startswith("#"):
            return index
    return -1


def run_case(name: str, root: pathlib.Path) -> str | None:
    """Run one case; returns None on success, else the failure text."""

    build = root / "build"
    build.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="state-field-audit-", dir=build) as work:
        tree = pathlib.Path(work)
        copy_tree(root, tree)
        # the unmutated baseline passes in the same setup, for every case
        baseline = check_tree(tree)
        if baseline:
            return "baseline copy is not clean: " + baseline[0]
        if name == "baseline-clean":
            return None
        mapping = tree / MAPPING_FILE
        lines = mapping_rows(mapping)
        index = first_row_index(lines)
        if index < 0:
            return "the mapping has no rows to seed"
        if name == "new-indexed-member-unmapped":
            header = tree / INTERNAL_HEADER
            text = header.read_text(encoding="utf-8")
            if SEED_STRUCT_ANCHOR not in text:
                return "seed anchor not found in the copied header"
            text = text.replace(
                SEED_STRUCT_ANCHOR,
                SEED_STRUCT_ANCHOR + f"\n  uint32_t {SEED_MEMBER};",
                1,
            )
            header.write_text(text, encoding="utf-8")
            state = tree / STATE_SOURCE
            state.write_text(
                state.read_text(encoding="utf-8") + SEED_FUNCTION, encoding="utf-8"
            )
            expected = f"unmapped member {SEED_MEMBER}"
        elif name == "mapping-row-removed":
            member = lines[index].split(" :: ")[0].strip()
            del lines[index]
            mapping.write_text("\n".join(lines) + "\n", encoding="utf-8")
            expected = f"unmapped member {member}"
        elif name == "unknown-rule-name":
            fields = lines[index].split(" :: ")
            member = fields[0].strip()
            fields[1] = SEED_UNKNOWN_RULE
            lines[index] = " :: ".join(fields)
            mapping.write_text("\n".join(lines) + "\n", encoding="utf-8")
            expected = f"unknown rule {SEED_UNKNOWN_RULE} for {member}"
        elif name == "reader-lacks-member":
            fields = lines[index].split(" :: ")
            member = fields[0].strip()
            fields[2] = SEED_WRONG_READER
            lines[index] = " :: ".join(fields[:3])
            mapping.write_text("\n".join(lines) + "\n", encoding="utf-8")
            expected = f"reader {SEED_WRONG_READER} does not mention {member}"
        else:
            return f"unknown case {name}"
        findings = check_tree(tree)
        if expected not in findings:
            return f"the seed did not reach its own finding {expected!r}; got {findings[:3]!r}"
        others = [item for item in findings if item != expected]
        if others and name != "new-indexed-member-unmapped":
            return f"the seed produced an unrelated finding {others[0]!r}"
        return None


def selftest(root: pathlib.Path) -> int:
    failed = 0
    for name in SELFTEST_CASES:
        problem = run_case(name, root)
        if problem is None:
            print(f"selftest {name}: ok")
        else:
            print(f"selftest {name}: FAILED: {problem}")
            failed += 1
    return 1 if failed else 0


def main(argv: list[str]) -> int:
    global ROOT
    args = list(argv)
    if "--root" in args:
        position = args.index("--root")
        if position + 1 >= len(args):
            sys.stderr.write("check-state-field-use: --root needs a directory\n")
            return 2
        ROOT = pathlib.Path(args[position + 1]).resolve()
        del args[position:position + 2]
    usage = (
        "usage: check-state-field-use.py [--root DIR] "
        "(--list | --check | --selftest | --list-cases | --case NAME | --emit-template)\n"
    )
    if args == ["--list-cases"]:
        for name in SELFTEST_CASES:
            print(name)
        return 0
    if args == ["--list"]:
        for line in build_list():
            print(line)
        return 0
    if args == ["--emit-template"]:
        for line in emit_template(ROOT):
            print(line)
        return 0
    if args and set(args) <= {"--check", "--selftest"}:
        status = 0
        if "--check" in args:
            if not (ROOT / MAPPING_FILE).is_file():
                sys.stderr.write(f"check-state-field-use: {MAPPING_FILE} is absent\n")
            findings = check_tree(ROOT)
            for finding in findings:
                print(finding)
            print(f"check-state-field-use: findings={len(findings)}")
            if findings:
                status = 1
        if "--selftest" in args:
            status = max(status, selftest(ROOT))
        return status
    if len(args) == 2 and args[0] == "--case":
        if args[1] not in SELFTEST_CASES:
            sys.stderr.write(f"check-state-field-use: unknown case {args[1]}\n")
            return 2
        problem = run_case(args[1], ROOT)
        if problem is None:
            print(f"selftest {args[1]}: ok")
            return 0
        print(f"selftest {args[1]}: FAILED: {problem}")
        return 1
    sys.stderr.write(usage)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
