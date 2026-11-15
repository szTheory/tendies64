#!/usr/bin/env python3
"""Validate the retained BASE evidence without launching guest code."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import pathlib
import re
import subprocess
import stat
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE_ROOT = pathlib.Path("build/private-base-diagnostic")
AUTHORED_BINDING = pathlib.Path(
    "build/gate/06-05-final-authored-comparisons/inputs/binding.json")
AUTHORED_MANIFEST = pathlib.Path("fixtures/manifest.json")
AUTHORED_BOOT_EVIDENCE = pathlib.Path("docs/evidence/phase06-cold-boot.md")
AUTHORED_CLAIMS = pathlib.Path("docs/claims/phase06.md")
HEX40 = re.compile(r"[0-9a-f]{40}\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
MAX_JSON = 64 * 1024 * 1024
CONTEXT_BASE_KEYS = {
    "bindings", "inventory", "profile", "schema", "schema_version",
    "source_authority", "source_projection", "source_revision",
}
CONTEXT_V2_KEYS = CONTEXT_BASE_KEYS | {"rsp_source_projection"}
BINDING_KEYS = {
    "context_sha256", "inventory_sha256", "schema", "schema_version",
    "source_authority_sha256", "source_projection_sha256",
}
AUTHORITY_KEYS = {"members", "schema", "schema_version", "source_revision"}
INVENTORY_KEYS = AUTHORITY_KEYS | {"source_authority_sha256"}
PUBLIC_RECEIPT_KEYS = {
    "artifact_binding", "core_build_id", "fixture_manifest", "profiles",
    "project_version", "schema", "schema_version", "source_annotation",
    "source_annotation_dirty",
}
PUBLIC_BINDING_KEYS = {"artifacts", "path", "sha256"}
PUBLIC_MANIFEST_KEYS = {"fixtures", "path", "sha256"}
PUBLIC_FIXTURE_KEYS = {"expected_kind", "id", "oracle_revision", "profile_id", "rom_sha256"}
PUBLIC_PROFILE_KEYS = {
    "boot_fail", "evidence_tier", "fixture_id", "guest_fail", "harness_error",
    "model", "missing", "not_claimed", "oracle_kind", "oracle_revision",
    "output_bytes", "output_sha256", "output_text", "pass", "profile_id",
    "profile_sha256", "retired_instructions", "rom_sha256", "skipped",
    "state_version", "terminal_tick", "timeout", "total",
}
PUBLIC_ARTIFACT_KEYS = {"name", "sha256"}
CLAIM_KEYS = {
    "build_id", "profile_id", "tier", "fixture_set", "manifest_digest",
    "pass", "total", "guest_fail", "boot_fail", "timeout", "missing",
    "harness_error", "skipped", "oracle_kind", "oracle_revision",
    "not_claimed", "statement",
}
PRIVATE_ALIAS = re.compile(
    r"selected[\s_-]*(?:member|rom)|external[\s_-]*(?:count|result)|"
    r"registration[\s_-]*index|value[\s_-]*id|member[\s_-]*results|"
    r"expected[\s_-]*members|source[\s_-]*authority|private[\s_-]*(?:count|result|identity)",
    re.IGNORECASE,
)


class Refusal(ValueError):
    """Evidence is missing or inconsistent."""


def refuse() -> None:
    raise Refusal("BASE evidence is missing or inconsistent")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def parse_canonical(data: bytes) -> dict:
    def unique_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                refuse()
            result[key] = value
        return result

    try:
        value = json.loads(data, object_pairs_hook=unique_pairs)
    except (UnicodeError, json.JSONDecodeError, RecursionError):
        refuse()
    if not isinstance(value, dict) or canonical(value) != data:
        refuse()
    return value


def parse_json(data: bytes) -> dict:
    def unique_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                refuse()
            result[key] = value
        return result
    try:
        value = json.loads(data, object_pairs_hook=unique_pairs)
    except (UnicodeError, json.JSONDecodeError, RecursionError):
        refuse()
    if not isinstance(value, dict):
        refuse()
    return value


def exact(value: object, keys: set[str]) -> dict:
    if not isinstance(value, dict) or set(value) != keys:
        refuse()
    return value


def _public_file(path: pathlib.Path, maximum: int = MAX_JSON) -> bytes:
    if path.is_symlink() or not path.is_file():
        refuse()
    try:
        data = path.read_bytes()
    except OSError:
        refuse()
    if not data or len(data) > maximum:
        refuse()
    return data


def _privacy_scan(value: object) -> None:
    if isinstance(value, dict):
        for key, child in value.items():
            if not isinstance(key, str) or PRIVATE_ALIAS.search(key):
                refuse()
            _privacy_scan(child)
    elif isinstance(value, list):
        for child in value:
            _privacy_scan(child)
    elif isinstance(value, str) and PRIVATE_ALIAS.search(value):
        refuse()


def _render_claim(profile: dict, receipt: dict) -> dict[str, str]:
    versioned_build = receipt["project_version"] + "+" + receipt["core_build_id"]
    values = {
        "build_id": versioned_build,
        "profile_id": profile["profile_id"],
        "tier": profile["evidence_tier"],
        "fixture_set": profile["fixture_id"],
        "manifest_digest": "sha256:" + receipt["fixture_manifest"]["sha256"],
        "pass": str(profile["pass"]), "total": str(profile["total"]),
        "guest_fail": str(profile["guest_fail"]), "boot_fail": str(profile["boot_fail"]),
        "timeout": str(profile["timeout"]), "missing": str(profile["missing"]),
        "harness_error": str(profile["harness_error"]), "skipped": str(profile["skipped"]),
        "oracle_kind": profile["oracle_kind"], "oracle_revision": profile["oracle_revision"],
        "not_claimed": profile["not_claimed"],
    }
    phrase = {"self-checking-fixture": "passes self-checking fixtures"}.get(values["tier"])
    if phrase is None:
        refuse()
    values["statement"] = (
        f"Tendies64 {values['build_id']} on profile {values['profile_id']}: {phrase} "
        f"for {values['fixture_set']} @ {values['manifest_digest']} "
        f"({values['pass']}/{values['total']}; missing {values['missing']}, "
        f"skipped {values['skipped']}, timeout {values['timeout']}, "
        f"harness-error {values['harness_error']}, guest-fail {values['guest_fail']}, "
        f"boot-fail {values['boot_fail']}). Oracle: {values['oracle_kind']} "
        f"{values['oracle_revision']}. Not claimed: {values['not_claimed']}."
    )
    return values


def validate_public_schema(receipt: object) -> None:
    """Reject unapproved public fields and aliases before any evidence is read."""
    exact(receipt, PUBLIC_RECEIPT_KEYS)
    if (receipt.get("schema") != "t64.phase06.final-evidence" or
            type(receipt.get("schema_version")) is not int or receipt["schema_version"] != 1 or
            not isinstance(receipt.get("project_version"), str) or
            not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", receipt["project_version"]) or
            not isinstance(receipt.get("core_build_id"), str) or
            not re.fullmatch(r"t64-build-v1-[0-9a-f]{64}", receipt["core_build_id"]) or
            not isinstance(receipt.get("source_annotation"), str) or
            not HEX40.fullmatch(receipt["source_annotation"]) or
            type(receipt.get("source_annotation_dirty")) is not bool):
        refuse()
    binding = exact(receipt.get("artifact_binding"), PUBLIC_BINDING_KEYS)
    manifest = exact(receipt.get("fixture_manifest"), PUBLIC_MANIFEST_KEYS)
    for reference, expected_path in ((binding, AUTHORED_BINDING.as_posix()),
                                     (manifest, AUTHORED_MANIFEST.as_posix())):
        if (reference.get("path") != expected_path or
                not isinstance(reference.get("sha256"), str) or
                not HEX64.fullmatch(reference["sha256"])):
            refuse()
    artifact_rows = binding.get("artifacts")
    if not isinstance(artifact_rows, list) or not artifact_rows:
        refuse()
    artifact_names = set()
    for row in artifact_rows:
        exact(row, PUBLIC_ARTIFACT_KEYS)
        if (not isinstance(row.get("name"), str) or row["name"] in artifact_names or
                pathlib.PurePosixPath(row["name"]).name != row["name"] or
                not isinstance(row.get("sha256"), str) or not HEX64.fullmatch(row["sha256"])):
            refuse()
        artifact_names.add(row["name"])
    fixture_rows = manifest.get("fixtures")
    expected_ids = {"hello-openipl3-rdram-a-t1", "hello-openipl3-rdram-b-t1"}
    if (not isinstance(fixture_rows, list) or len(fixture_rows) != len(expected_ids) or
            {row.get("id") for row in fixture_rows if isinstance(row, dict)} != expected_ids):
        refuse()
    for row in fixture_rows:
        exact(row, PUBLIC_FIXTURE_KEYS)
        if (row.get("expected_kind") != "self-checking" or
                not isinstance(row.get("profile_id"), str) or
                not isinstance(row.get("oracle_revision"), str) or
                not isinstance(row.get("rom_sha256"), str) or
                not HEX64.fullmatch(row["rom_sha256"])):
            refuse()
    profiles = receipt.get("profiles")
    if not isinstance(profiles, list) or len(profiles) != 2:
        refuse()
    profile_ids = set()
    for profile in profiles:
        exact(profile, PUBLIC_PROFILE_KEYS)
        if (not isinstance(profile.get("profile_id"), str) or
                profile["profile_id"] in profile_ids or
                not isinstance(profile.get("fixture_id"), str) or
                not isinstance(profile.get("profile_sha256"), str) or
                not HEX64.fullmatch(profile["profile_sha256"]) or
                not isinstance(profile.get("rom_sha256"), str) or
                not HEX64.fullmatch(profile["rom_sha256"]) or
                not isinstance(profile.get("output_text"), str) or
                not isinstance(profile.get("output_sha256"), str) or
                not HEX64.fullmatch(profile["output_sha256"]) or
                not isinstance(profile.get("evidence_tier"), str) or
                not isinstance(profile.get("oracle_kind"), str) or
                not isinstance(profile.get("oracle_revision"), str) or
                not isinstance(profile.get("not_claimed"), str) or
                any(type(profile.get(key)) is not int for key in (
                    "model", "state_version", "terminal_tick", "retired_instructions",
                    "output_bytes", "pass", "total", "guest_fail", "boot_fail",
                    "timeout", "missing", "harness_error", "skipped"))):
            refuse()
        profile_ids.add(profile["profile_id"])
    if profile_ids != {row["profile_id"] for row in fixture_rows}:
        refuse()
    _privacy_scan(receipt)


def validate_public_receipt(repository_root: pathlib.Path = ROOT) -> dict:
    """Check the closed authored receipt against its exact manifest and archive."""
    receipt_path = repository_root / "docs/evidence/phase06-final.json"
    receipt = parse_json(_public_file(receipt_path, maximum=1024 * 1024))
    validate_public_schema(receipt)
    if (receipt.get("schema") != "t64.phase06.final-evidence" or
            type(receipt.get("schema_version")) is not int or
            receipt["schema_version"] != 1 or
            not isinstance(receipt.get("project_version"), str) or
            not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", receipt["project_version"]) or
            not isinstance(receipt.get("core_build_id"), str) or
            not re.fullmatch(r"t64-build-v1-[0-9a-f]{64}", receipt["core_build_id"]) or
            type(receipt.get("source_annotation_dirty")) is not bool):
        refuse()
    binding_reference = exact(receipt.get("artifact_binding"), PUBLIC_BINDING_KEYS)
    manifest_reference = exact(receipt.get("fixture_manifest"), PUBLIC_MANIFEST_KEYS)
    if (binding_reference.get("path") != AUTHORED_BINDING.as_posix() or
            manifest_reference.get("path") != AUTHORED_MANIFEST.as_posix() or
            not isinstance(binding_reference.get("sha256"), str) or
            not HEX64.fullmatch(binding_reference["sha256"]) or
            not isinstance(manifest_reference.get("sha256"), str) or
            not HEX64.fullmatch(manifest_reference["sha256"])):
        refuse()

    binding_bytes = _public_file(repository_root / AUTHORED_BINDING, maximum=1024 * 1024)
    binding = parse_json(binding_bytes)
    if (sha256(binding_bytes) != binding_reference["sha256"] or
            set(binding) != {"artifacts", "caps", "dirty_source", "intent", "source_head"} or
            binding.get("dirty_source") is not receipt["source_annotation_dirty"] or
            binding.get("source_head") != receipt["source_annotation"]):
        refuse()
    artifact_rows = binding_reference.get("artifacts")
    if not isinstance(artifact_rows, list):
        refuse()
    actual_artifacts = binding.get("artifacts")
    if not isinstance(actual_artifacts, dict) or len(artifact_rows) != len(actual_artifacts):
        refuse()
    seen_artifacts = set()
    for item in artifact_rows:
        exact(item, PUBLIC_ARTIFACT_KEYS)
        name, digest = item.get("name"), item.get("sha256")
        if (not isinstance(name, str) or name in seen_artifacts or
                pathlib.PurePosixPath(name).name != name or
                not isinstance(digest, str) or not HEX64.fullmatch(digest) or
                actual_artifacts.get(name) != digest):
            refuse()
        path = repository_root / AUTHORED_BINDING.parent / name
        if _file_sha256(path) != digest:
            refuse()
        seen_artifacts.add(name)
    if seen_artifacts != set(actual_artifacts):
        refuse()

    manifest_bytes = _public_file(repository_root / AUTHORED_MANIFEST, maximum=4 * 1024 * 1024)
    if sha256(manifest_bytes) != manifest_reference["sha256"]:
        refuse()
    manifest = parse_json(manifest_bytes)
    if manifest.get("schema") != "t64.manifest" or not isinstance(manifest.get("entries"), list):
        refuse()
    expected_ids = {"hello-openipl3-rdram-a-t1", "hello-openipl3-rdram-b-t1"}
    manifest_rows = {row.get("id"): row for row in manifest["entries"] if isinstance(row, dict)}
    fixture_rows = manifest_reference.get("fixtures")
    if not isinstance(fixture_rows, list) or len(fixture_rows) != len(expected_ids):
        refuse()
    if {row.get("id") for row in fixture_rows if isinstance(row, dict)} != expected_ids:
        refuse()

    evidence_text = _public_file(repository_root / AUTHORED_BOOT_EVIDENCE, maximum=256 * 1024).decode("utf-8")
    build_match = re.search(r"Tested build ID: `(t64-build-v1-[0-9a-f]{64})`\.", evidence_text)
    source_match = re.search(r"recorded source annotation is\s+`([0-9a-f]{40})`", evidence_text)
    if (build_match is None or build_match.group(1) != receipt["core_build_id"] or
            source_match is None or source_match.group(1) != receipt["source_annotation"]):
        refuse()
    if not re.search(rf"project\(tendies64 VERSION {re.escape(receipt['project_version'])} LANGUAGES C\)",
                     (repository_root / "CMakeLists.txt").read_text()):
        refuse()
    timing_text = _public_file(
        repository_root / "docs/spec/cpu-timing.md", maximum=256 * 1024).decode("utf-8")

    profiles = receipt.get("profiles")
    if not isinstance(profiles, list) or len(profiles) != 2:
        refuse()
    claim_values = {}
    for fixture in fixture_rows:
        exact(fixture, PUBLIC_FIXTURE_KEYS)
        source = manifest_rows.get(fixture["id"])
        if (source is None or source.get("profile") != fixture.get("profile_id") or
                source.get("sha256") != fixture.get("rom_sha256") or
                not isinstance(source.get("expected"), dict) or
                source["expected"].get("kind") != fixture.get("expected_kind") or
                source["expected"].get("revision") != fixture.get("oracle_revision") or
                fixture.get("expected_kind") != "self-checking"):
            refuse()
    for profile in profiles:
        exact(profile, PUBLIC_PROFILE_KEYS)
        fixture_id = profile.get("fixture_id")
        fixture = next((row for row in fixture_rows if row["id"] == fixture_id), None)
        if fixture is None:
            refuse()
        profile_id = profile.get("profile_id")
        short = "A" if profile_id.endswith("-a-t1-v1") else "B"
        profile_hash = re.search(
            rf"- {short} profile SHA-256: `([0-9a-f]{{64}})`", evidence_text)
        timing = re.search(
            rf"\| `{re.escape(profile_id)}` \| ([0-9]+) \| ([0-9]+) \|",
            timing_text)
        observed = re.search(
            rf"\| `{re.escape(profile_id)}` \| ([0-9]+) \| ([0-9]+) \| Exact ([0-9]+) bytes \|",
            evidence_text)
        if (profile.get("profile_id") != fixture["profile_id"] or
                profile.get("rom_sha256") != fixture["rom_sha256"] or
                profile_hash is None or profile_hash.group(1) != profile.get("profile_sha256") or
                timing is None or int(timing.group(1)) != profile.get("model") or
                int(timing.group(2)) != profile.get("state_version") or
                observed is None or int(observed.group(1)) != profile.get("terminal_tick") or
                int(observed.group(2)) != profile.get("retired_instructions") or
                int(observed.group(3)) != profile.get("output_bytes") or
                profile.get("evidence_tier") != "self-checking-fixture" or
                profile.get("oracle_kind") != "authored-literal-transcript" or
                profile.get("oracle_revision") != fixture["oracle_revision"] or
                profile.get("output_bytes") != 30 or
                profile.get("output_text") != "Tendies64 hello via open IPL3\n" or
                profile.get("output_sha256") != sha256(profile["output_text"].encode("utf-8")) or
                any(type(profile.get(key)) is not int for key in (
                    "model", "state_version", "terminal_tick", "retired_instructions",
                    "output_bytes", "pass", "total", "guest_fail", "boot_fail",
                    "timeout", "missing", "harness_error", "skipped")) or
                profile.get("pass") != 1 or profile.get("total") != 1 or
                any(profile.get(key) != 0 for key in (
                    "guest_fail", "boot_fail", "timeout", "missing", "harness_error", "skipped"))):
            refuse()
        claim_values[profile["profile_id"]] = _render_claim(profile, receipt)
    if set(claim_values) != {row["profile_id"] for row in fixture_rows}:
        refuse()

    claims_text = _public_file(repository_root / AUTHORED_CLAIMS, maximum=256 * 1024).decode("utf-8")
    blocks = re.findall(r"^```t64-claim\n(.*?)^```$", claims_text, re.MULTILINE | re.DOTALL)
    if len(blocks) != 2:
        refuse()
    checked_claims = set()
    for block in blocks:
        values = {}
        for line in block.splitlines():
            if ":" not in line:
                refuse()
            key, value = line.split(":", 1)
            if key in values:
                refuse()
            values[key] = value.lstrip()
        exact(values, CLAIM_KEYS)
        expected = claim_values.get(values.get("profile_id"))
        if expected is None or values != expected or values["profile_id"] in checked_claims:
            refuse()
        checked_claims.add(values["profile_id"])

    _privacy_scan(receipt)
    return {"schema": "t64.phase06-authored-evidence", "schema_version": 1,
            "status": "verified"}


def read_private(path: pathlib.Path, *, maximum: int = MAX_JSON,
                 minimum: int = 1) -> bytes:
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(path, flags)
    except OSError:
        refuse()
    try:
        before = os.fstat(descriptor)
        if (not stat.S_ISREG(before.st_mode) or
                stat.S_IMODE(before.st_mode) != 0o600 or
                before.st_uid != os.geteuid() or before.st_size < minimum or
                before.st_size > maximum):
            refuse()
        chunks = []
        total = 0
        while True:
            chunk = os.read(descriptor, min(1024 * 1024, maximum + 1 - total))
            if not chunk:
                break
            total += len(chunk)
            if total > maximum:
                refuse()
            chunks.append(chunk)
        after = os.fstat(descriptor)
        if ((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) !=
                (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns) or
                total != before.st_size):
            refuse()
        return b"".join(chunks)
    finally:
        os.close(descriptor)


def _load_verifier(repository_root: pathlib.Path):
    path = repository_root / "scripts/verify-base-diagnostic-qualification.py"
    spec = importlib.util.spec_from_file_location("t64_base_evidence_verifier", path)
    if spec is None or spec.loader is None:
        refuse()
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _context_ref(value: object, path: str) -> str:
    if (not isinstance(value, dict) or set(value) != {"path", "sha256"} or
            value.get("path") != path or not isinstance(value.get("sha256"), str) or
            not HEX64.fullmatch(value["sha256"])):
        refuse()
    return value["sha256"]


def validate_artifact_pair(context_root: pathlib.Path,
                           repository_root: pathlib.Path,
                           *, require_live_executable: bool = False,
                           snapshot_root: pathlib.Path | None = None) -> dict:
    """Check the exact retained executable/context/binding and bound inventories."""
    context_root = pathlib.Path(context_root)
    try:
        root_info = context_root.lstat()
    except OSError:
        refuse()
    if stat.S_ISLNK(root_info.st_mode) or not stat.S_ISDIR(root_info.st_mode):
        refuse()
    context_root = context_root.resolve(strict=True)
    repository_root = repository_root.resolve(strict=True)
    root_info = context_root.stat(follow_symlinks=False)
    if (not stat.S_ISDIR(root_info.st_mode) or stat.S_IMODE(root_info.st_mode) != 0o700 or
            root_info.st_uid != os.geteuid()):
        refuse()

    context_bytes = read_private(context_root / "context.json", maximum=2 * 1024 * 1024)
    binding_bytes = read_private(context_root / "binding.json", maximum=64 * 1024)
    context = parse_canonical(context_bytes)
    binding = parse_canonical(binding_bytes)
    version = context.get("schema_version")
    if (type(version) is not int or version not in {1, 2} or
            set(context) != (CONTEXT_BASE_KEYS if version == 1 else CONTEXT_V2_KEYS) or
            context.get("schema") != "t64.base-qualification-context" or
            not isinstance(context.get("source_revision"), str) or
            not HEX40.fullmatch(context["source_revision"]) or
            not isinstance(context.get("profile"), (str, dict))):
        refuse()
    if (set(binding) != BINDING_KEYS or
            binding.get("schema") != "t64.base-qualification-binding" or
            binding.get("schema_version") != 1 or
            binding.get("context_sha256") != sha256(context_bytes)):
        refuse()

    authority_sha = _context_ref(
        context.get("source_authority"), "build/private-base-diagnostic/source-authority.json")
    inventory_sha = _context_ref(
        context.get("inventory"), "build/private-base-diagnostic/inventory.json")
    projection_sha = _context_ref(
        context.get("source_projection"), "build/private-cpu-diagnostic/source-projection.json")
    if (binding.get("source_authority_sha256") != authority_sha or
            binding.get("inventory_sha256") != inventory_sha or
            binding.get("source_projection_sha256") != projection_sha):
        refuse()

    authority_path = context_root / "source-authority.json"
    inventory_path = context_root / "inventory.json"
    authority_bytes = read_private(authority_path)
    inventory_bytes = read_private(inventory_path)
    authority = parse_canonical(authority_bytes)
    inventory = parse_canonical(inventory_bytes)
    if (set(authority) != AUTHORITY_KEYS or
            authority.get("schema") != "t64.base-source-authority" or
            authority.get("schema_version") != 1 or
            set(inventory) != INVENTORY_KEYS or
            inventory.get("schema") != "t64.base-inventory" or
            inventory.get("schema_version") != 1 or
            authority.get("source_revision") != context["source_revision"] or
            inventory.get("source_revision") != context["source_revision"] or
            authority.get("members") != inventory.get("members") or
            sha256(authority_bytes) != authority_sha or
            sha256(inventory_bytes) != inventory_sha or
            inventory.get("source_authority_sha256") != authority_sha or
            binding.get("source_authority_sha256") != sha256(authority_bytes) or
            binding.get("inventory_sha256") != sha256(inventory_bytes)):
        refuse()

    verifier = _load_verifier(repository_root)
    verifier._verify_retained_execution_pair(
        snapshot_root or context_root, context, context_bytes, binding_bytes)
    executable_rows = context["bindings"].get("executable")
    if not isinstance(executable_rows, list) or len(executable_rows) != 1:
        refuse()
    executable = executable_rows[0]
    if (not isinstance(executable, dict) or set(executable) != {"path", "sha256"} or
            not isinstance(executable.get("path"), str) or
            not isinstance(executable.get("sha256"), str) or
            not HEX64.fullmatch(executable["sha256"])):
        refuse()
    if require_live_executable:
        live = repository_root / executable["path"]
        if live.is_symlink() or not live.is_file():
            refuse()
        try:
            live_bytes = live.read_bytes()
        except OSError:
            refuse()
        if sha256(live_bytes) != executable["sha256"]:
            refuse()

    return {"schema": "t64.base-evidence-artifacts", "schema_version": 1,
            "status": "verified"}


def _private_path(root: pathlib.Path, value: object) -> pathlib.Path:
    if not isinstance(value, str) or not value:
        refuse()
    path = pathlib.Path(value)
    if not path.is_absolute():
        refuse()
    try:
        resolved = path.resolve(strict=True)
        resolved.relative_to(root.resolve(strict=True))
    except (OSError, ValueError):
        refuse()
    cursor = path
    while cursor != root:
        if cursor.is_symlink():
            refuse()
        cursor = cursor.parent
    return resolved


def _file_sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            while True:
                chunk = stream.read(1024 * 1024)
                if not chunk:
                    break
                digest.update(chunk)
    except OSError:
        refuse()
    return digest.hexdigest()


def _validate_batch(batch_root: pathlib.Path, repository_root: pathlib.Path,
                    expected_lane: str) -> None:
    batch_root = batch_root.resolve(strict=True)
    batch_info = batch_root.stat(follow_symlinks=False)
    if (not stat.S_ISDIR(batch_info.st_mode) or stat.S_IMODE(batch_info.st_mode) != 0o700 or
            batch_info.st_uid != os.geteuid()):
        refuse()
    validate_artifact_pair(
        batch_root, repository_root,
        snapshot_root=repository_root / PRIVATE_ROOT)
    context_bytes = read_private(batch_root / "context.json", maximum=2 * 1024 * 1024)
    binding = parse_canonical(read_private(batch_root / "binding.json", maximum=64 * 1024))
    context_sha = sha256(context_bytes)
    executable = batch_root / "consumer"
    try:
        executable_info = executable.lstat()
    except OSError:
        refuse()
    if (stat.S_ISLNK(executable_info.st_mode) or not stat.S_ISREG(executable_info.st_mode) or
            stat.S_IMODE(executable_info.st_mode) != 0o700 or
            executable_info.st_uid != os.geteuid()):
        refuse()
    context = parse_canonical(context_bytes)
    executable_rows = context.get("bindings", {}).get("executable")
    if (not isinstance(executable_rows, list) or len(executable_rows) != 1 or
            _file_sha256(executable) != executable_rows[0].get("sha256")):
        refuse()

    batch_receipt = parse_canonical(read_private(batch_root / "batch.json", maximum=64 * 1024))
    batch_keys = {
        "classification_exit_code", "classification_input", "classification_report",
        "context_sha256", "interrupted", "lane", "schema", "schema_version",
        "stop_reason", "stopped",
    }
    if (set(batch_receipt) != batch_keys or
            batch_receipt.get("schema") != "t64.base-batch" or
            batch_receipt.get("schema_version") != 1 or
            batch_receipt.get("context_sha256") != context_sha or
            batch_receipt.get("lane") != expected_lane or
            batch_receipt.get("interrupted") is not False or
            batch_receipt.get("stopped") is not False or
            batch_receipt.get("classification_exit_code") not in {0, 3}):
        refuse()
    for input_key in ("classification_input", "classification_report"):
        name = batch_receipt[input_key]
        if (not isinstance(name, str) or pathlib.PurePosixPath(name).name != name or
                name in {"", ".", ".."}):
            refuse()
    input_path = batch_root / batch_receipt["classification_input"]
    report_path = batch_root / batch_receipt["classification_report"]
    input_bytes = read_private(input_path, maximum=MAX_JSON)
    expected_report = read_private(report_path, maximum=MAX_JSON)
    manifest = parse_canonical(input_bytes)
    if (set(manifest) != {"members", "schema", "schema_version"} or
            manifest.get("schema") != "t64.base-classification-input" or
            manifest.get("schema_version") != 1 or
            not isinstance(manifest.get("members"), list)):
        refuse()
    record_root = (repository_root / PRIVATE_ROOT).resolve(strict=True)
    for row in manifest["members"]:
        if (not isinstance(row, dict) or set(row) != {
                "expected_rom_sha256", "original", "outcome", "reason", "record",
                "registration_index", "value_index"}):
            refuse()
        original = _private_path(record_root, row["original"])
        record = _private_path(record_root, row["record"])
        read_private(original, maximum=4096, minimum=0)
        read_private(record, maximum=8192)

    authority = batch_root / "source-authority.json"
    inventory = batch_root / "inventory.json"
    if (_file_sha256(authority) != binding.get("source_authority_sha256") or
            _file_sha256(inventory) != binding.get("inventory_sha256")):
        refuse()
    authority_sha = binding["source_authority_sha256"]
    before = _file_sha256(executable)
    private_root = (repository_root / PRIVATE_ROOT).resolve(strict=True)
    verification_manifest = {
        "members": [dict(row, outcome="record", reason="")
                    for row in manifest["members"]],
        "schema": "t64.base-classification-input", "schema_version": 1,
    }
    with tempfile.TemporaryDirectory(prefix="check-", dir=private_root) as temporary:
        temporary_root = pathlib.Path(temporary)
        manifest_path = temporary_root / "classification-input.json"
        manifest_path.write_bytes(canonical(verification_manifest))
        manifest_path.chmod(0o600)
        regenerated = temporary_root / "classification.json"
        command = [str(executable), "--classify-manifest", str(authority), authority_sha,
                   str(inventory), context_sha, str(manifest_path), str(regenerated)]
        try:
            completed = subprocess.run(
                command, cwd=batch_root, stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60,
                check=False, start_new_session=True,
                env={"CI": "", "T64_REQUIRE_FIXTURES": "0", "LC_ALL": "C",
                     "PATH": os.environ.get("PATH", "")},
            )
        except (OSError, subprocess.TimeoutExpired):
            refuse()
        if (_file_sha256(executable) != before or
                completed.returncode != batch_receipt["classification_exit_code"] or
                not regenerated.is_file()):
            refuse()
        reproduced = regenerated.read_bytes()
    reproduced_report = parse_canonical(reproduced)
    if (reproduced != expected_report or
            reproduced_report.get("schema") != "t64.base-classification" or
            reproduced_report.get("schema_version") != 1 or
            reproduced_report.get("context_sha256") != context_sha or
            reproduced_report.get("inventory_sha256") != binding["inventory_sha256"] or
            reproduced_report.get("source_complete") is not True):
        refuse()


def check_current(repository_root: pathlib.Path = ROOT) -> dict:
    """Reclassify every retained original batch using its exact archived C consumer."""
    private_root = repository_root / PRIVATE_ROOT
    summary = parse_canonical(read_private(
        private_root / "plan06-06-execution-summary.json", maximum=2 * 1024 * 1024))
    lanes = summary.get("lanes")
    if not isinstance(lanes, list):
        refuse()
    found = set()
    for row in lanes:
        if not isinstance(row, dict) or not isinstance(row.get("lane"), str):
            refuse()
        lane = row["lane"]
        if lane not in {"required", "informational", "stress"} or lane in found:
            refuse()
        relative = row.get("batch")
        relative_path = pathlib.PurePosixPath(relative) if isinstance(relative, str) else None
        if (relative_path is None or relative_path.is_absolute() or
                relative_path.parts[:2] != ("build", "private-base-diagnostic") or
                any(part in {"", ".", ".."} for part in relative_path.parts)):
            refuse()
        batch = repository_root / pathlib.Path(*relative_path.parts)
        private_root = (repository_root / PRIVATE_ROOT).resolve(strict=True)
        try:
            batch.resolve(strict=True).relative_to(private_root)
        except (OSError, ValueError):
            refuse()
        _validate_batch(batch, repository_root, lane)
        found.add(lane)
    if found != {"required", "informational", "stress"}:
        refuse()
    return {"schema": "t64.base-evidence-current", "schema_version": 1,
            "status": "verified"}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--check-current", action="store_true")
    modes.add_argument("--check-artifacts", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.check_artifacts:
            validate_artifact_pair(ROOT / PRIVATE_ROOT, ROOT,
                                   require_live_executable=True)
            result = validate_public_receipt(ROOT)
        else:
            result = check_current()
        print(json.dumps(result, sort_keys=True, separators=(",", ":")))
        return 0
    except (OSError, Refusal, ValueError):
        print("BASE evidence refused", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
