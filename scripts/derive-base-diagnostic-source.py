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
RSP_PROJECTION = ROOT / "build/private-rsp-diagnostic/rsp-source-projection.json"
CPU_QUALIFICATION = ROOT / "build/private-cpu-diagnostic/qualification.json"
RSP_QUALIFICATION = ROOT / "build/private-rsp-diagnostic/qualification.json"
HISTORICAL_ARCHIVE = ROOT / "build/private-base-diagnostic-history/precombined-active"
HEX40 = re.compile(r"^[0-9a-f]{40}$")
BASE_LEVELS = frozenset({
    "BasicFunctionality", "RarelyUsed", "Weird", "RDPBasic", "RDPPrecise",
})
INFORMATIONAL_LEVELS = frozenset({"Timing", "Cycle", "COP0Hazard"})
SOURCE_LEVELS = BASE_LEVELS | INFORMATIONAL_LEVELS | {"StressTest"}
NONREQUIRED_LANES = frozenset({
    "timing", "cycle", "cop0-hazard", "informational", "stress",
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


def _rsp_projection_rows(projection: object, source_revision: str) -> dict[int, dict]:
    if projection is None:
        return {}
    if (not isinstance(projection, dict) or
            projection.get("schema") != "t64.rsp-source-projection" or
            projection.get("schema_version") != 1 or
            projection.get("component") != "rsp" or
            projection.get("status") != "membership-prepared" or
            projection.get("source_revision") != source_revision):
        refuse("accepted RSP source projection is malformed or targets another source")
    rows = projection.get("registrations")
    if not isinstance(rows, list):
        refuse("accepted RSP source projection has no registration list")
    result: dict[int, dict] = {}
    for row in rows:
        if not isinstance(row, dict):
            refuse("accepted RSP source registration is malformed")
        index = row.get("index")
        if type(index) is not int or index < 0 or index > 0xFFFF or index in result:
            refuse("accepted RSP source registration ordinals are malformed")
        if row.get("level") not in SOURCE_LEVELS:
            continue
        source_path = pathlib.PurePosixPath(str(row.get("source_file", "")))
        if (not isinstance(row.get("source_file"), str) or
                not row["source_file"].startswith("src/tests/rsp/") or
                source_path.is_absolute() or "\\" in row["source_file"] or
                any(part in {"", ".", "..", ".git"} for part in source_path.parts) or
                row.get("group") != "rsp" or
                row.get("lane") not in ({"required-base"} | NONREQUIRED_LANES)):
            refuse("BASE RSP registration has an unsupported source path or lane")
        count = row.get("source_value_count")
        effective = row.get("effective_value_count")
        values = row.get("values")
        if (type(count) is not int or count < 0 or
                effective != max(1, count) or not isinstance(values, list) or
                len(values) != effective):
            refuse("accepted RSP source value ordinals are incomplete")
        for value_index, value in enumerate(values):
            if (not isinstance(value, dict) or value.get("index") != value_index or
                    value.get("id") != ("once" if count == 0 else f"v{value_index:05d}") or
                    value.get("selector") != f"r{index:04d}v{value_index:05d}"):
                refuse("accepted RSP source selector is malformed")
        result[index] = row
    return result


def derive_members(projection: dict,
                   rsp_projection: dict | None = None,
                   cpu_source_files: dict[str, str] | None = None,
                   rsp_source_files: dict[str, str] | None = None) -> list[dict[str, object]]:
    legacy_mode = (rsp_projection is None and cpu_source_files is None and
                   rsp_source_files is None)
    selected_levels = BASE_LEVELS if legacy_mode else SOURCE_LEVELS
    source_rows = projection["registrations"]
    rsp_rows = _rsp_projection_rows(
        rsp_projection, str(projection.get("source_revision", ""))
    )
    members: list[dict[str, object]] = []
    for ordinal, row in enumerate(source_rows):
        if not isinstance(row, dict) or row.get("index") != ordinal:
            refuse("source projection registration order is malformed")
        if row.get("level") not in selected_levels:
            continue
        source_lane = row.get("lane")
        if legacy_mode and source_lane not in {"required-base", "skipped-non-cpu"}:
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
        expected_ids = (["once"] if source_count == 0 else
                        [f"v{value_index:05d}" for value_index in range(source_count)])
        if not legacy_mode and source_ids != expected_ids:
            refuse("source projection value identities or ordering are incomplete")

        rsp_row = rsp_rows.get(ordinal)
        if rsp_row is not None and (
                rsp_row.get("level") != row.get("level") or
                rsp_row.get("source_value_count") != source_count):
            refuse("RSP source membership differs from the global source ordinal")

        source_file = row.get("source_file")
        source_file_sha = row.get("source_file_sha256")
        if not legacy_mode and (not isinstance(source_file, str) or not source_file or
                                not isinstance(source_file_sha, str) or
                                re.fullmatch(r"[0-9a-f]{64}", source_file_sha) is None):
            refuse("BASE source implementation binding is malformed")

        def in_closure(files: dict[str, str] | None) -> bool:
            return (files is not None and isinstance(source_file, str) and
                    files.get(source_file) == source_file_sha)

        if rsp_row is not None:
            member_lane = str(rsp_row["lane"])
            required = member_lane == "required-base"
            group = "rsp"
            closed = in_closure(rsp_source_files)
            reason = "" if closed else "rsp member source or asset closure is not qualified"
            execution = (("once" if source_count == 0 else "values")
                         if closed else "blocked")
        elif source_lane == "required-base":
            values = row.get("values")
            if not isinstance(values, list) or len(values) != effective_count:
                refuse("required BASE values are not source-derived")
            identifiers = [item.get("id") for item in values
                           if isinstance(item, dict)]
            if len(identifiers) != effective_count or identifiers != source_ids:
                refuse("required BASE value identities differ from the source")
            group = row.get("group")
            member_lane = "required-base"
            required = True
            closed = legacy_mode or in_closure(cpu_source_files)
            reason = ("required" if legacy_mode else
                      "" if closed else
                      f"{group} member source or asset closure is not qualified")
            execution = (("once" if source_count == 0 else "values")
                         if closed else "blocked")
        elif source_lane == "skipped-non-cpu":
            group = row.get("source_group")
            required = True
            closed = (not legacy_mode and in_closure(cpu_source_files))
            member_lane = "required-base" if closed else "blocked-non-cpu"
            reason = ("source member is outside the qualified CPU dispatch"
                      if legacy_mode else "" if closed else
                      f"{group or 'source'} source module is absent from the accepted component closures")
            execution = ("once" if source_count == 0 else "values") if closed else "blocked"
        elif source_lane in NONREQUIRED_LANES:
            group = row.get("group") or row.get("source_group")
            member_lane = source_lane
            required = False
            closed = in_closure(cpu_source_files)
            reason = "" if closed else f"{group} informational module or asset closure is not qualified"
            execution = (("once" if source_count == 0 else "values")
                         if closed else "blocked")
        else:
            if legacy_mode:
                continue
            refuse("global BASE source registration has an unclassified lane")

        if not legacy_mode and (source_lane == "required-base" or
                                source_lane in NONREQUIRED_LANES):
            values = row.get("values")
            if not isinstance(values, list) or len(values) != effective_count:
                refuse("source registration selector list is incomplete")
            for value_index, value in enumerate(values):
                if (not isinstance(value, dict) or value.get("id") != source_ids[value_index] or
                        value.get("index") != value_index or
                        value.get("selector") != f"r{ordinal:04d}v{value_index:05d}"):
                    refuse("source registration selector differs from its global ordinal")

        if not all(isinstance(item, str) and item for item in (
            group, member_lane, row.get("level"), row.get("name")
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
                "required": required,
                "value_id": value_id,
                "value_index": value_index,
            })
    if not members:
        refuse("accepted projection yields no BASE members")
    return members


def _verify_historical_archive(private_root: pathlib.Path) -> None:
    archive = HISTORICAL_ARCHIVE
    manifest_path = archive / "archive-manifest.json"
    if (archive.is_symlink() or not archive.is_dir() or
            stat.S_IMODE(archive.stat(follow_symlinks=False).st_mode) != 0o700 or
            manifest_path.is_symlink() or not manifest_path.is_file() or
            stat.S_IMODE(manifest_path.stat(follow_symlinks=False).st_mode) != 0o600):
        refuse("historical BASE state was not archived before source re-derivation")
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        refuse("historical BASE archive manifest is invalid")
    if (not isinstance(manifest, dict) or
            manifest.get("schema") != "t64.base-precombined-active-archive.v1" or
            type(manifest.get("source_present")) is not bool or
            not isinstance(manifest.get("files"), list)):
        refuse("historical BASE archive manifest has an unsupported schema")
    archived: dict[str, str] = {}
    for row in manifest["files"]:
        if (not isinstance(row, dict) or not isinstance(row.get("path"), str) or
                not isinstance(row.get("sha256"), str) or
                re.fullmatch(r"[0-9a-f]{64}", row["sha256"]) is None):
            refuse("historical BASE archive file list is malformed")
        relative = pathlib.PurePosixPath(row["path"])
        if (relative.is_absolute() or any(part in {"", ".", ".."}
                                          for part in relative.parts) or
                row["path"] in archived):
            refuse("historical BASE archive path list is malformed")
        saved_path = archive.joinpath(*relative.parts)
        if (saved_path.is_symlink() or not saved_path.is_file() or
                stat.S_IMODE(saved_path.stat(follow_symlinks=False).st_mode) != 0o600 or
                hashlib.sha256(saved_path.read_bytes()).hexdigest() != row["sha256"]):
            refuse("historical BASE archived control tree no longer matches its manifest")
        archived[row["path"]] = row["sha256"]
    if private_root.exists():
        controls = ("source-authority.json", "inventory.json", "context.json",
                    "binding.json", "qualification.json")
        for path in private_root.iterdir():
            if (path.name not in controls and not path.name.startswith("status")):
                continue
            if path.is_symlink():
                refuse("active BASE control state is linked")
            if path.is_file():
                relative = path.relative_to(private_root).as_posix()
                if archived.get(relative) != hashlib.sha256(path.read_bytes()).hexdigest():
                    refuse("active BASE control state differs from the archived history")


def _private_source_files(rows: object, label: str) -> dict[str, str]:
    if not isinstance(rows, list):
        refuse(f"accepted {label} file closure is malformed")
    result: dict[str, str] = {}
    for row in rows:
        if (not isinstance(row, dict) or not isinstance(row.get("path"), str) or
                not isinstance(row.get("sha256"), str) or
                re.fullmatch(r"[0-9a-f]{64}", row["sha256"]) is None):
            refuse(f"accepted {label} file closure row is malformed")
        relative = pathlib.PurePosixPath(row["path"])
        if (relative.is_absolute() or any(part in {"", ".", ".."}
                                          for part in relative.parts) or
                row["path"] in result):
            refuse(f"accepted {label} file closure path is malformed")
        result[row["path"]] = row["sha256"]
    return result


def accepted_component_closures(source_revision: str,
                                rsp_projection_bytes: bytes | None) -> tuple[dict[str, str] | None,
                                                                            dict[str, str] | None]:
    cpu_files: dict[str, str] | None = None
    rsp_files: dict[str, str] | None = None
    if CPU_QUALIFICATION.is_file() and not CPU_QUALIFICATION.is_symlink():
        try:
            qualification = json.loads(CPU_QUALIFICATION.read_bytes())
        except (OSError, UnicodeError, json.JSONDecodeError) as exc:
            refuse(f"accepted CPU qualification cannot be parsed: {exc}")
        assets = qualification.get("assets") if isinstance(qualification, dict) else None
        reference = assets.get("closure_report") if isinstance(assets, dict) else None
        if (not isinstance(reference, dict) or
                not isinstance(reference.get("path"), str) or
                not isinstance(reference.get("sha256"), str)):
            refuse("accepted CPU source closure reference is malformed")
        path = ROOT / pathlib.PurePosixPath(reference["path"])
        if (path.is_symlink() or not path.is_file() or
                hashlib.sha256(path.read_bytes()).hexdigest() != reference["sha256"]):
            refuse("accepted CPU source closure differs from its qualification")
        try:
            closure = json.loads(path.read_bytes())
        except (OSError, UnicodeError, json.JSONDecodeError) as exc:
            refuse(f"accepted CPU source closure cannot be parsed: {exc}")
        if (not isinstance(closure, dict) or
                closure.get("status") != "PASS_PREBUILD_SOURCE" or
                closure.get("source_revision") != source_revision):
            refuse("accepted CPU source closure targets a different source")
        cpu_files = _private_source_files(closure.get("module_files"), "CPU source")

    if (rsp_projection_bytes is not None and RSP_QUALIFICATION.is_file() and
            not RSP_QUALIFICATION.is_symlink()):
        try:
            qualification = json.loads(RSP_QUALIFICATION.read_bytes())
        except (OSError, UnicodeError, json.JSONDecodeError) as exc:
            refuse(f"accepted RSP qualification cannot be parsed: {exc}")
        reference = qualification.get("closure") if isinstance(qualification, dict) else None
        membership = qualification.get("membership") if isinstance(qualification, dict) else None
        if (not isinstance(reference, dict) or
                not isinstance(reference.get("path"), str) or
                not isinstance(reference.get("sha256"), str) or
                not isinstance(membership, dict) or
                membership.get("path") != RSP_PROJECTION.relative_to(ROOT).as_posix() or
                membership.get("sha256") != hashlib.sha256(rsp_projection_bytes).hexdigest()):
            refuse("accepted RSP qualification does not bind the source projection")
        path = ROOT / pathlib.PurePosixPath(reference["path"])
        if (path.is_symlink() or not path.is_file() or
                hashlib.sha256(path.read_bytes()).hexdigest() != reference["sha256"]):
            refuse("accepted RSP source closure differs from its qualification")
        try:
            closure = json.loads(path.read_bytes())
        except (OSError, UnicodeError, json.JSONDecodeError) as exc:
            refuse(f"accepted RSP source closure cannot be parsed: {exc}")
        if (not isinstance(closure, dict) or
                closure.get("status") != "PASS_PREBUILD_RSP" or
                closure.get("source_revision") != source_revision):
            refuse("accepted RSP source closure targets a different source")
        overlay = closure.get("overlay")
        if not isinstance(overlay, dict):
            refuse("accepted RSP source closure has no overlay file set")
        rsp_files = _private_source_files(overlay.get("files"), "RSP overlay")
    return cpu_files, rsp_files


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

    _verify_historical_archive(private_root)
    projection = read_projection(PROJECTION)
    rsp_projection = None
    rsp_projection_bytes: bytes | None = None
    if RSP_PROJECTION.is_file() and not RSP_PROJECTION.is_symlink():
        try:
            rsp_projection_bytes = RSP_PROJECTION.read_bytes()
            rsp_projection = json.loads(rsp_projection_bytes)
        except (OSError, UnicodeError, json.JSONDecodeError) as exc:
            refuse(f"accepted RSP source projection cannot be parsed: {exc}")
    cpu_source_files, rsp_source_files = accepted_component_closures(
        projection["source_revision"], rsp_projection_bytes)
    if cpu_source_files is None or (rsp_projection is not None and rsp_source_files is None):
        refuse("accepted CPU/RSP source closure is required for BASE derivation")
    members = derive_members(projection, rsp_projection,
                             cpu_source_files, rsp_source_files)
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
