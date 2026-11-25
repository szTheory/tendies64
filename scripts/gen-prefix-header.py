#!/usr/bin/env python3
"""Generate and check the internal symbol rename header (Phase 11, plan 11-11).

core/t64_prefix.h holds one `#define name t64i_name` line per global of the
core archive that is not part of the public API (DR-17). Every translation
unit of the core, every white-box test and the determinism lint see the header
through core/t64_internal.h and core/softfloat/platform.h, so the archive
defines exactly the public functions of core/include/tendies64/t64_machine.h
plus names that start with t64i_ and a host link line cannot override or
collide with an internal name.

Rename rule: t64_X becomes t64i_X; every other name (the Berkeley SoftFloat
names) becomes t64i_<name>.

Modes:
  --write LIB   regenerate the header from the defined global symbols of the
                archive LIB (nm -P -g --defined-only, Darwin underscore
                stripped): the new names (defined, not public, not t64i_)
                merged with the header's existing entries whose rename target
                the archive still defines, sorted, with a generated-file banner
  --check LIB   exit 1 when the archive defines a global that is neither
                public nor t64i_-prefixed, when an entry's target is not
                defined by the archive, when a public function is missing from
                the archive, or when regeneration differs from the committed
                header (a unified diff is printed)
Options: --header FILE (default core/t64_prefix.h), --public FILE (default
core/include/tendies64/t64_machine.h), --nm TOOL (default $NM or nm).

Standard library only. Run as `python3 -B scripts/gen-prefix-header.py ...`.
Exit codes: 0 clean, 1 a finding, 2 usage error or a missing tool.
"""

from __future__ import annotations

import argparse
import difflib
import os
import pathlib
import platform
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_HEADER = ROOT / "core" / "t64_prefix.h"
DEFAULT_PUBLIC = ROOT / "core" / "include" / "tendies64" / "t64_machine.h"

PREFIX = "t64i_"
GUARD = "T64_PREFIX_H"
BANNER = (
    "/* GENERATED FILE: do not edit by hand.\n"
    " * Written by scripts/gen-prefix-header.py --write build/gate/core/libt64_core.a\n"
    " * and checked by CTest repo.abi-prefix (gen-prefix-header.py --check).\n"
    " *\n"
    " * One rename per global of the core archive that is not public API: a host\n"
    " * link line cannot override or collide with an internal name (DR-17).\n"
    " * t64_X becomes t64i_X; every other name becomes t64i_<name>. The header\n"
    " * is seen through core/t64_internal.h and core/softfloat/platform.h by\n"
    " * every core translation unit, white-box test and the determinism lint,\n"
    " * and by the in-tree runner header t64_facade.h.\n"
    " */\n"
)
ENTRY_RE = re.compile(r"^#define ([A-Za-z_][A-Za-z0-9_]*) ([A-Za-z_][A-Za-z0-9_]*)$")
PUBLIC_RE = re.compile(
    r"\bT64_API\b[^;{}()]*?\b(t64_[A-Za-z0-9_]+)\s*\(", re.DOTALL)
COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)


class UsageError(Exception):
    """A missing tool or an unusable argument: exit status 2."""


def read_text(path: pathlib.Path, what: str) -> str:
    try:
        return pathlib.Path(path).read_text(encoding="utf-8")
    except OSError as error:
        raise UsageError(
            f"gen-prefix-header: cannot read the {what} {path}: {error.strerror}"
        ) from error


def public_names(header: pathlib.Path) -> set[str]:
    """The T64_API function names declared by the public header."""
    text = COMMENT_RE.sub(" ", read_text(header, "public header"))
    text = "\n".join(line for line in text.split("\n")
                     if not line.lstrip().startswith("#"))
    return set(PUBLIC_RE.findall(text))


def archive_names(library: pathlib.Path, nm_tool: str) -> set[str]:
    """Defined global symbols of the archive, platform underscore stripped."""
    parts = nm_tool.split()
    if not parts or shutil.which(parts[0]) is None:
        raise UsageError(f"gen-prefix-header: nm not found: {nm_tool} (set NM)")
    if not pathlib.Path(library).is_file():
        raise UsageError(f"gen-prefix-header: archive not found: {library}")
    result = subprocess.run(
        parts + ["-P", "-g", "--defined-only", str(library)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=False)
    if result.returncode != 0:
        raise UsageError(
            f"gen-prefix-header: nm failed on {library}: {result.stderr.strip()}")
    darwin = platform.system() == "Darwin"
    names = set()
    for line in result.stdout.split("\n"):
        fields = line.split()
        if len(fields) < 3 or not re.fullmatch(r"[A-Z]", fields[1]):
            continue
        name = fields[0]
        if darwin and name.startswith("_"):
            name = name[1:]
        names.add(name)
    return names


def target_of(name: str) -> str:
    """The rename target of an unprefixed internal name."""
    if name.startswith("t64_"):
        return PREFIX + name[len("t64_"):]
    return PREFIX + name


def parse_entries(text: str) -> dict[str, str]:
    entries: dict[str, str] = {}
    for line in text.split("\n"):
        found = ENTRY_RE.match(line)
        if found is not None:
            entries[found.group(1)] = found.group(2)
    return entries


def render(entries: dict[str, str]) -> str:
    lines = [BANNER, f"#ifndef {GUARD}\n", f"#define {GUARD}\n", "\n"]
    for name in sorted(entries):
        lines.append(f"#define {name} {entries[name]}\n")
    lines.append("\n")
    lines.append(f"#endif /* {GUARD} */\n")
    return "".join(lines)


def build_entries(defined: set[str], public: set[str],
                  existing: dict[str, str]) -> tuple[dict[str, str], list[str]]:
    """Return (entries, findings) for the archive's defined set."""
    findings: list[str] = []
    entries = {name: target for name, target in existing.items()
               if target in defined}
    for name in sorted(defined):
        if name in public or name.startswith(PREFIX):
            continue
        target = target_of(name)
        if target in defined or target in entries.values():
            findings.append(
                f"collision: {name} would be renamed to {target}, which already "
                f"exists")
            continue
        entries[name] = target
    return entries, findings


def check(library: pathlib.Path, header: pathlib.Path, public_header: pathlib.Path,
          nm_tool: str) -> list[str]:
    """Return the findings of the archive against the committed header."""
    public = public_names(public_header)
    if not public:
        raise UsageError(
            f"gen-prefix-header: no T64_API function found in {public_header}")
    defined = archive_names(library, nm_tool)
    committed = read_text(header, "prefix header")
    existing = parse_entries(committed)
    findings = []
    for name in sorted(defined):
        if name not in public and not name.startswith(PREFIX):
            findings.append(
                f"unprefixed: the archive defines {name}, which is neither a "
                f"public function nor a {PREFIX} name")
    for name in sorted(public - defined):
        findings.append(f"missing-public: {name} is declared T64_API but the "
                        f"archive does not define it")
    for name, target in sorted(existing.items()):
        if target not in defined:
            findings.append(
                f"stale: {header.name} renames {name} to {target}, which the "
                f"archive does not define")
    entries, collisions = build_entries(defined, public, existing)
    findings.extend(collisions)
    regenerated = render(entries)
    if regenerated != committed and not findings:
        diff = "".join(difflib.unified_diff(
            committed.splitlines(keepends=True),
            regenerated.splitlines(keepends=True),
            fromfile="committed", tofile="regenerated"))
        findings.append("differs: regeneration differs from the committed header\n"
                        + diff.rstrip("\n"))
    return findings


def write(library: pathlib.Path, header: pathlib.Path, public_header: pathlib.Path,
          nm_tool: str) -> list[str]:
    public = public_names(public_header)
    if not public:
        raise UsageError(
            f"gen-prefix-header: no T64_API function found in {public_header}")
    defined = archive_names(library, nm_tool)
    existing = {}
    if pathlib.Path(header).is_file():
        existing = parse_entries(read_text(header, "prefix header"))
    entries, findings = build_entries(defined, public, existing)
    if findings:
        return findings
    pathlib.Path(header).write_text(render(entries), encoding="utf-8")
    print(f"gen-prefix-header: wrote {len(entries)} entries "
          f"({len(public)} public functions kept)")
    return []


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="gen-prefix-header.py", description=__doc__.split("\n")[0])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--write", metavar="LIB", help="regenerate the header")
    group.add_argument("--check", metavar="LIB", help="verify the archive")
    parser.add_argument("--header", default=str(DEFAULT_HEADER))
    parser.add_argument("--public", default=str(DEFAULT_PUBLIC))
    parser.add_argument("--nm", default=os.environ.get("NM") or "nm")
    return parser


def main(argv: list[str]) -> int:
    args = build_parser().parse_args(argv[1:])
    header = pathlib.Path(args.header)
    public_header = pathlib.Path(args.public)
    try:
        if args.write is not None:
            findings = write(pathlib.Path(args.write), header, public_header, args.nm)
        else:
            findings = check(pathlib.Path(args.check), header, public_header, args.nm)
    except UsageError as error:
        print(error, file=sys.stderr)
        return 2
    for finding in findings:
        print(f"gen-prefix-header: {finding}")
    if findings:
        return 1
    if args.check is not None:
        public = public_names(public_header)
        print(f"gen-prefix-header: ok ({len(public)} public functions, "
              f"{len(parse_entries(read_text(header, 'prefix header')))} renames)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
