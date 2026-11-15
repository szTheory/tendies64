#!/usr/bin/env python3
"""Compose the audited CPU and RSP source selectors for one private BASE image."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import pathlib
import re
import shutil
import stat
import sys

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE_LEVELS = frozenset({
    "BasicFunctionality", "RarelyUsed", "Weird", "RDPBasic", "RDPPrecise",
})
INFORMATIONAL_LEVELS = frozenset({"Timing", "Cycle", "COP0Hazard"})
SOURCE_LEVELS = BASE_LEVELS | INFORMATIONAL_LEVELS | {"StressTest"}
NONREQUIRED_LANES = frozenset({
    "timing", "cycle", "cop0-hazard", "informational", "stress",
})
CPU_OVERLAY = ROOT / "scripts/apply-cpu-diagnostic-overlay.py"
HEX64 = re.compile(r"^[0-9a-f]{64}$")


class Refusal(ValueError):
    pass


def refuse(message: str) -> None:
    raise Refusal(message)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def checked_projection(value: object) -> dict:
    if (not isinstance(value, dict) or
            value.get("schema") != "t64.cpu-source-projection" or
            value.get("schema_version") != 2 or
            not isinstance(value.get("source_revision"), str) or
            not re.fullmatch(r"[0-9a-f]{40}", value["source_revision"])):
        refuse("accepted CPU source projection is malformed")
    seal = value.get("projection_sha256")
    body = {key: item for key, item in value.items()
            if key != "projection_sha256"}
    if not isinstance(seal, str) or digest(canonical(body)) != seal:
        refuse("accepted CPU source projection seal does not match")
    if not isinstance(value.get("registrations"), list):
        refuse("accepted CPU source registration list is malformed")
    return value


def checked_rsp_projection(value: object, cpu_projection_bytes: bytes) -> dict:
    if (not isinstance(value, dict) or
            value.get("schema") != "t64.rsp-source-projection" or
            value.get("schema_version") != 1 or
            value.get("component") != "rsp" or
            value.get("status") != "membership-prepared" or
            not isinstance(value.get("registrations"), list)):
        refuse("accepted RSP source projection is malformed")
    inputs = value.get("inputs")
    if (not isinstance(inputs, dict) or
            inputs.get("cpu_source_projection_sha256") != digest(cpu_projection_bytes)):
        refuse("RSP source projection does not bind the accepted CPU projection")
    return value


def registration_expression(source_lines: list[str], audit_row: dict) -> str:
    constructor = audit_row.get("path")
    anchor = audit_row.get("line")
    if not isinstance(constructor, str) or not constructor or type(anchor) is not int or anchor < 0:
        refuse("source audit has no pinned registration constructor anchor")
    spec = importlib.util.spec_from_file_location("t64_base_cpu_overlay", CPU_OVERLAY)
    if spec is None or spec.loader is None:
        refuse("CPU overlay constructor recognizer cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    # The audited parser can anchor a registration at its adjacent comment.
    # Skip only contiguous blank/line-comment text, never another expression.
    limit = min(anchor + 8, len(source_lines))
    while anchor < limit and (not source_lines[anchor].strip() or
                              source_lines[anchor].lstrip().startswith("//")):
        anchor += 1
    if anchor == limit:
        refuse("registration constructor anchor contains no expression")
    try:
        return module.render_registration_expression(source_lines, anchor, constructor)
    except ValueError:
        refuse("registration constructor differs from its pinned source expression")


def _aligned_row(source_row: object, audit_row: object, index: int) -> tuple[dict, dict]:
    if not isinstance(source_row, dict) or not isinstance(audit_row, dict):
        refuse("accepted source membership row is malformed")
    if (source_row.get("index") != index or audit_row.get("index") != index or
            source_row.get("constructor") != audit_row.get("path") or
            source_row.get("source_name") != audit_row.get("upstream_name") or
            source_row.get("level") != audit_row.get("level") or
            source_row.get("source_value_count") != audit_row.get("source_value_count") or
            source_row.get("source_file_sha256") != audit_row.get("source_sha256") or
            source_row.get("values_method_sha256") != audit_row.get("values_method_sha256")):
        refuse("accepted source projection and audit differ")
    path_text = source_row.get("source_file")
    if not isinstance(path_text, str):
        refuse("accepted registration source path is malformed")
    relative = pathlib.PurePosixPath(path_text)
    if (relative.is_absolute() or not relative.parts or
            any(part in {"", ".", "..", ".git"} for part in relative.parts)):
        refuse("accepted registration source path escapes its checkout")
    return source_row, audit_row


def selected_rows(cpu_projection: dict, rsp_projection: dict,
                  source_audit: dict, cpu_closure: dict, rsp_closure: dict,
                  source_root: pathlib.Path,
                  registration_source_root: pathlib.Path) -> list[dict[str, object]]:
    cpu_rows = cpu_projection["registrations"]
    audit_rows = source_audit.get("registrations")
    rsp_rows = rsp_projection["registrations"]
    cpu_module_rows = cpu_closure.get("module_files")
    rsp_overlay_rows = rsp_closure.get("overlay", {}).get("files")
    if not isinstance(cpu_module_rows, list) or not isinstance(rsp_overlay_rows, list):
        refuse("accepted CPU/RSP source module closures are incomplete")
    cpu_module_hashes = {
        row.get("path"): row.get("sha256") for row in cpu_module_rows
        if isinstance(row, dict) and isinstance(row.get("path"), str)
    }
    rsp_module_hashes = {
        row.get("path"): row.get("sha256") for row in rsp_overlay_rows
        if isinstance(row, dict) and isinstance(row.get("path"), str)
    }
    if (not isinstance(audit_rows, list) or len(audit_rows) != len(cpu_rows) or
            not isinstance(rsp_rows, list)):
        refuse("accepted CPU/RSP source partitions have different registration bounds")
    source_lines = (registration_source_root / "src/tests/testlist.rs").read_text(
        encoding="utf-8").splitlines()
    selected: list[dict[str, object]] = []
    seen: set[tuple[int, int]] = set()

    def append(row: dict, audit: dict, values: object, lane: str,
               source_ids: object) -> None:
        index = row.get("index")
        count = row.get("source_value_count")
        effective = max(1, count) if type(count) is int and count >= 0 else None
        if (type(index) is not int or not 0 <= index <= 0xFFFF or
                effective is None or not isinstance(source_ids, list) or
                len(source_ids) != effective or not isinstance(values, list) or
                len(values) != effective):
            refuse("selected source registration has malformed value ordinals")
        expected_ids = (["once"] if count == 0 else
                        [f"v{value_index:05d}" for value_index in range(count)])
        if source_ids != expected_ids:
            refuse("selected source value identities differ from the pinned ordinals")
        if row.get("level") not in SOURCE_LEVELS or lane not in (
                {"required-base"} | NONREQUIRED_LANES):
            refuse("selected source registration is outside the global BASE lanes")
        source_text = str(row.get("source_file", ""))
        source_path = source_root / pathlib.PurePosixPath(source_text)
        if (source_path.is_symlink() or not source_path.is_file() or
                digest(source_path.read_bytes()) != row.get("source_file_sha256")):
            refuse("selected registration implementation differs from its source seal")
        expression = registration_expression(source_lines, audit)
        for value_index, value in enumerate(values):
            if (not isinstance(value, dict) or value.get("index") != value_index or
                    value.get("id") != source_ids[value_index] or
                    value.get("selector") != f"r{index:04d}v{value_index:05d}"):
                refuse("selected source value ordinal is malformed")
            key = (index, value_index)
            if key in seen:
                refuse("CPU and RSP source selections overlap at a global ordinal")
            seen.add(key)
            if count == 0 and value_index != 0:
                refuse("zero-value source registration is not represented once")
            selected.append({
                "expression": expression,
                "group": row.get("group"),
                "lane": lane,
                "level": row["level"],
                "registration_index": index,
                "source_file": source_text,
                "source_file_sha256": row["source_file_sha256"],
                "value_index": value_index,
            })

    for index, row in enumerate(cpu_rows):
        row, audit = _aligned_row(row, audit_rows[index], index)
        lane = row.get("lane")
        level = row.get("level")
        if level not in SOURCE_LEVELS:
            continue
        if lane == "required-base" and level in BASE_LEVELS:
            selected_lane = "required-base"
        elif lane == "skipped-non-cpu" and level in BASE_LEVELS:
            source_path = row.get("source_file")
            if cpu_module_hashes.get(source_path) != row.get("source_file_sha256"):
                continue
            selected_lane = "required-base"
        elif lane in NONREQUIRED_LANES:
            selected_lane = str(lane)
        else:
            continue
        source_path = row.get("source_file")
        source_sha = row.get("source_file_sha256")
        if cpu_module_hashes.get(source_path) != source_sha:
            continue
        values = row.get("values")
        source_ids = row.get("source_value_ids")
        if lane == "skipped-non-cpu" and values == []:
            effective = row.get("effective_value_count")
            if (type(effective) is not int or effective < 1 or
                    not isinstance(source_ids, list) or len(source_ids) != effective):
                refuse("non-CPU source value identities are incomplete")
            values = [{
                "id": source_ids[value_index], "index": value_index,
                "selector": f"r{index:04d}v{value_index:05d}",
            } for value_index in range(effective)]
        append(row, audit, values, selected_lane, source_ids)

    for rsp_row in rsp_rows:
        if not isinstance(rsp_row, dict):
            refuse("accepted RSP source registration is malformed")
        index = rsp_row.get("index")
        if type(index) is not int or index < 0 or index >= len(cpu_rows):
            refuse("accepted RSP source registration ordinal is outside the global source")
        source_row, audit = _aligned_row(cpu_rows[index], audit_rows[index], index)
        if (rsp_row.get("level") not in SOURCE_LEVELS or
                rsp_row.get("lane") not in ({"required-base"} | NONREQUIRED_LANES)):
            continue
        if (rsp_row.get("source_file") != source_row.get("source_file") or
                rsp_row.get("source_file_sha256") != source_row.get("source_file_sha256") or
                rsp_row.get("source_value_count") != source_row.get("source_value_count") or
                rsp_row.get("constructor") != source_row.get("constructor") or
                rsp_row.get("source_name") != source_row.get("source_name")):
            refuse("accepted RSP source selection differs from the global source row")
        if rsp_module_hashes.get(rsp_row.get("source_file")) != rsp_row.get("source_file_sha256"):
            continue
        append(rsp_row, audit, rsp_row.get("values"), rsp_row["lane"],
               source_row.get("source_value_ids"))

    selected.sort(key=lambda item: (int(item["registration_index"]),
                                    int(item["value_index"])))
    if not selected:
        refuse("combined BASE source selection is empty")
    return selected


def render_testlist(selected: list[dict[str, object]]) -> bytes:
    lines = [
        "// Generated privately from the accepted BASE source projection.\n",
        "use alloc::boxed::Box;\nuse alloc::vec;\nuse alloc::vec::Vec;\n",
        "use super::Test;\n\n",
        "pub fn tests() -> Vec<(usize, Box<dyn Test>)> {\n    vec![\n",
    ]
    emitted: dict[int, str] = {}
    for row in selected:
        index = int(row["registration_index"])
        expression = str(row["expression"])
        if index in emitted:
            if emitted[index] != expression:
                refuse("one registration has conflicting pinned constructors")
            continue
        emitted[index] = expression
        lines.append(
            f"        ({row['registration_index']}usize, {row['expression']} as Box<dyn Test>),\n"
        )
    lines.append("    ]\n}\n")
    return "".join(lines).encode("utf-8")


def selector_value(registration_index: int, value_index: int) -> bytes:
    if (type(registration_index) is not int or type(value_index) is not int or
            not 0 <= registration_index <= 0xFFFF or
            not 0 <= value_index <= 0xFFFF):
        refuse("BASE selector ordinal is outside its two-word encoding")
    return registration_index.to_bytes(2, "big") + value_index.to_bytes(2, "big")


def selected_rom_sha256(base_rom: bytes, selector_offset: int,
                        registration_index: int, value_index: int) -> str:
    if (type(selector_offset) is not int or selector_offset < 0 or
            selector_offset + 4 > len(base_rom) or
            base_rom[selector_offset:selector_offset + 4] != bytes(4)):
        refuse("combined BASE image selector is absent, nonzero, or out of bounds")
    patched = bytearray(base_rom)
    patched[selector_offset:selector_offset + 4] = selector_value(
        registration_index, value_index)
    return digest(bytes(patched))


def merge_test_modules(cpu_modules: bytes) -> bytes:
    try:
        text = cpu_modules.decode("utf-8")
    except UnicodeDecodeError:
        refuse("CPU test module root is not UTF-8")
    for name in ("rsp", "soft_asserts"):
        declaration = re.compile(r"(?m)^\s*(?:pub(?:\([^\n)]+\))?\s+)?mod\s+" +
                                 re.escape(name) + r"\s*;")
        if declaration.search(text) is None:
            text = text.rstrip() + "\npub mod " + name + ";\n"
    return text.encode("utf-8")


def apply_combined_overlay(cpu_source_root: pathlib.Path,
                           rsp_overlay_root: pathlib.Path,
                           cpu_closure: dict,
                           rsp_closure: dict,
                           cpu_projection: dict,
                           rsp_projection: dict,
                           source_audit: dict,
                           registration_source_root: pathlib.Path,
                           output_root: pathlib.Path) -> tuple[list[dict[str, object]], dict[str, object]]:
    if cpu_source_root.is_symlink() or rsp_overlay_root.is_symlink():
        refuse("combined BASE source roots are linked")
    cpu_source_root = cpu_source_root.resolve(strict=True)
    rsp_overlay_root = rsp_overlay_root.resolve(strict=True)
    if output_root.exists() or output_root.is_symlink():
        refuse("refusing to overwrite an existing combined BASE source tree")
    approved_files = rsp_closure.get("overlay", {}).get("files")
    if not isinstance(approved_files, list) or not approved_files:
        refuse("accepted RSP source closure has no overlay file manifest")
    rsp_paths: set[str] = set()
    for row in approved_files:
        if not isinstance(row, dict) or set(row) != {"path", "sha256"}:
            refuse("accepted RSP overlay file manifest is malformed")
        relative = pathlib.PurePosixPath(str(row["path"]))
        if (relative.is_absolute() or any(part in {"", ".", "..", ".git"}
                                          for part in relative.parts) or
                not HEX64.fullmatch(str(row["sha256"]))):
            refuse("accepted RSP overlay path or digest is malformed")
        source = rsp_overlay_root.joinpath(*relative.parts)
        if (source.is_symlink() or not source.is_file() or
                digest(source.read_bytes()) != row["sha256"]):
            refuse("RSP overlay file differs from its accepted source closure")
        rsp_paths.add(relative.as_posix())

    output_root.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    shutil.copytree(cpu_source_root, output_root,
                    ignore=shutil.ignore_patterns(".git", "target"))
    os.chmod(output_root, 0o700)
    for current, dirs, files in os.walk(output_root, followlinks=False):
        current_path = pathlib.Path(current)
        os.chmod(current_path, 0o700)
        for name in dirs:
            child = current_path / name
            if child.is_symlink():
                refuse("combined BASE source copy contains a symbolic link")
            os.chmod(child, 0o700)
        for name in files:
            child = current_path / name
            if child.is_symlink() or not child.is_file():
                refuse("combined BASE source copy contains a linked or special file")
            os.chmod(child, 0o600)

    rsp_runtime_paths = {
        path for path in rsp_paths
        if path.startswith("src/rsp/") or path.startswith("src/tests/rsp/") or
        path in {"src/tests/soft_asserts.rs", "src/text_out.rs", "src/print.rs",
                 "src/graphics/mod.rs"}
    }
    if not any(path.startswith("src/rsp/") for path in rsp_runtime_paths) or not any(
            path.startswith("src/tests/rsp/") for path in rsp_runtime_paths):
        refuse("accepted RSP source closure has no runtime or BASE test modules")
    for relative in sorted(rsp_runtime_paths):
        source = rsp_overlay_root / pathlib.PurePosixPath(relative)
        target = output_root / pathlib.PurePosixPath(relative)
        target.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        target.chmod(0o600)

    selected = selected_rows(cpu_projection, rsp_projection, source_audit,
                             cpu_closure, rsp_closure, output_root,
                             registration_source_root)
    testlist = render_testlist(selected)
    tests_root = output_root / "src/tests"
    (tests_root / "testlist.rs").write_bytes(testlist)
    (tests_root / "testlist.rs").chmod(0o600)
    module_path = tests_root / "mod.rs"
    module_path.write_bytes(merge_test_modules(module_path.read_bytes()))
    module_path.chmod(0o600)

    manifest_files = []
    for relative in sorted(rsp_runtime_paths | {"src/tests/testlist.rs", "src/tests/mod.rs"}):
        path = output_root / pathlib.PurePosixPath(relative)
        manifest_files.append({"path": relative, "sha256": digest(path.read_bytes())})
    overlay = {
        "schema": "t64.base-combined-source-overlay.v1",
        "source_revision": cpu_projection["source_revision"],
        "cpu_projection_sha256": digest(canonical(cpu_projection)),
        "rsp_projection_sha256": digest(canonical(rsp_projection)),
        "files": manifest_files,
        "registration_count": len({int(row["registration_index"]) for row in selected}),
        "member_count": len(selected),
    }
    return selected, overlay


if __name__ == "__main__":
    print("This module is imported by the private BASE image builder.", file=sys.stderr)
    raise SystemExit(2)
