#!/usr/bin/env python3
"""Derive private whole-BASE membership from the accepted source projection."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import stat
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_PRIVATE = ROOT / "build/private-base-diagnostic"
PROJECTION = ROOT / "build/private-cpu-diagnostic/source-projection.json"
HEX40 = re.compile(r"^[0-9a-f]{40}$")
BASE_LEVELS = frozenset({
    "BasicFunctionality", "RarelyUsed", "Weird", "RDPBasic", "RDPPrecise",
})


class Refusal(ValueError):
    pass


def refuse(message: str) -> None:
    raise Refusal(message)


def canonical_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def read_projection(path: pathlib.Path) -> dict:
    if path.is_symlink() or not path.is_file():
        refuse("accepted source projection is missing or linked")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        refuse(f"accepted source projection cannot be parsed: {exc}")
    if not isinstance(value, dict) or value.get("schema") != "t64.cpu-source-projection":
        refuse("input is not the accepted CPU source projection")
    if value.get("schema_version") != 2 or not HEX40.fullmatch(
        str(value.get("source_revision", ""))
    ):
        refuse("accepted source projection identity is malformed")
    seal = value.get("projection_sha256")
    body = {key: item for key, item in value.items() if key != "projection_sha256"}
    if not isinstance(seal, str) or hashlib.sha256(canonical_bytes(body)).hexdigest() != seal:
        refuse("accepted source projection seal does not match its contents")
    rows = value.get("registrations")
    if not isinstance(rows, list) or not rows:
        refuse("accepted source projection has no registrations")
    return value


def derive_members(projection: dict) -> list[dict[str, object]]:
    source_rows = projection["registrations"]
    members: list[dict[str, object]] = []
    for ordinal, row in enumerate(source_rows):
        if not isinstance(row, dict) or row.get("index") != ordinal:
            refuse("source projection registration order is malformed")
        lane = row.get("lane")
        if (lane not in {"required-base", "skipped-non-cpu"}
                or row.get("level") not in BASE_LEVELS):
            continue
        source_count = row.get("source_value_count")
        effective_count = row.get("effective_value_count")
        source_ids = row.get("source_value_ids")
        if (type(source_count) is not int or source_count < 0
                or type(effective_count) is not int
                or effective_count != max(1, source_count)
                or not isinstance(source_ids, list)
                or len(source_ids) != effective_count
                or not all(isinstance(item, str) for item in source_ids)):
            refuse("source projection value ordinals are incomplete")

        required = lane == "required-base"
        if required:
            values = row.get("values")
            if not isinstance(values, list) or len(values) != effective_count:
                refuse("required BASE values are not source-derived")
            identifiers = [item.get("id") for item in values
                           if isinstance(item, dict)]
            if len(identifiers) != effective_count or identifiers != source_ids:
                refuse("required BASE value identities differ from the source")
            group = row.get("group")
            member_lane = "required-base"
            reason = "required"
            execution = "once" if source_count == 0 else "values"
        else:
            group = row.get("source_group")
            member_lane = "blocked-non-cpu"
            reason = "source member is outside the qualified CPU dispatch"
            execution = "blocked"

        if not all(isinstance(item, str) and item for item in (
            group, row.get("level"), row.get("name")
        )):
            refuse("source registration labels are malformed")
        for value_index in range(effective_count):
            value_id = "" if source_count == 0 else source_ids[value_index]
            members.append({
                "execution": execution,
                "group": group,
                "index": ordinal,
                "lane": member_lane,
                "level": row["level"],
                "name": row["name"],
                "reason": reason,
                "required": True,
                "value_id": value_id,
                "value_index": value_index,
            })
    if not members:
        refuse("accepted projection yields no BASE members")
    return members


def write_private(path: pathlib.Path, data: bytes) -> None:
    if path.exists() and path.is_symlink():
        refuse("private output path is linked")
    fd, name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    temporary = pathlib.Path(name)
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, "wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
        os.chmod(path, 0o600, follow_symlinks=False)
    finally:
        temporary.unlink(missing_ok=True)


def derive(private_root: pathlib.Path) -> None:
    private_root = private_root.absolute()
    try:
        relative = private_root.relative_to(ROOT / "build")
    except ValueError:
        refuse("BASE projection output must remain beneath the ignored build root")
    cursor = ROOT / "build"
    for part in relative.parts:
        cursor = cursor / part
        if cursor.exists() and cursor.is_symlink():
            refuse("private BASE output path traverses a link")
    private_root.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(private_root, 0o700)
    if stat.S_IMODE(private_root.stat().st_mode) & 0o077:
        refuse("private BASE output directory is accessible to group or others")

    projection = read_projection(PROJECTION)
    members = derive_members(projection)
    source_revision = projection["source_revision"]
    authority = {
        "members": members,
        "schema": "t64.base-source-authority",
        "schema_version": 1,
        "source_revision": source_revision,
    }
    authority_bytes = canonical_bytes(authority)
    authority_sha256 = hashlib.sha256(authority_bytes).hexdigest()
    inventory = {
        "members": members,
        "schema": "t64.base-inventory",
        "schema_version": 1,
        "source_authority_sha256": authority_sha256,
        "source_revision": source_revision,
    }
    write_private(private_root / "source-authority.json", authority_bytes)
    write_private(private_root / "inventory.json", canonical_bytes(inventory))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--derive", action="store_true",
                        help="derive private BASE membership from the accepted projection")
    parser.add_argument("--private-root", type=pathlib.Path, default=DEFAULT_PRIVATE)
    args = parser.parse_args()
    if not args.derive:
        parser.error("--derive is required")
    try:
        derive(args.private_root)
    except (OSError, Refusal) as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 2
    print("PASS: private source authority and BASE inventory derived")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
