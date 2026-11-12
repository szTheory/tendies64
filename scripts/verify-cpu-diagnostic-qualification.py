#!/usr/bin/env python3
"""Fail-closed pre-use check for the private CPU diagnostic receipt."""

from __future__ import annotations

import hashlib
import json
import pathlib
import re
import sys
import tomllib

SOURCE_REVISION = "9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c"
CHANNEL_MANIFEST_SHA256 = "808b3fb16540d28b08fc8f9db812a129a13129d5413cdb30607a12ec7fa09632"
REQUIRED_CHECKS = {
    "host_toolchain",
    "rust_source",
    "project_lock",
    "registry_dependencies",
    "build_hooks",
    "target_tools",
    "prebuild_asset_closure",
    "source_membership",
}
REQUIRED_ARCHIVES = {"rustc", "cargo", "host-rust-std", "rust-src"}
REQUIRED_ASSETS = {"font", "framebuffer", "graphics", "non_cpu_rsp_startup"}
HEX64 = re.compile(r"[0-9a-f]{64}\Z")


class Refusal(Exception):
    pass


def refuse(message: str) -> None:
    raise Refusal(message)


def object_with_keys(value: object, keys: set[str], label: str) -> dict:
    if not isinstance(value, dict) or set(value) != keys:
        refuse(f"{label} fields do not match the closed qualification schema")
    return value


def checked_file(repository: pathlib.Path, value: object, label: str) -> str:
    row = object_with_keys(value, {"path", "sha256"}, label)
    path_text = row["path"]
    digest = row["sha256"]
    if not isinstance(path_text, str) or not path_text:
        refuse(f"{label} path is missing")
    if not isinstance(digest, str) or HEX64.fullmatch(digest) is None:
        refuse(f"{label} SHA-256 is malformed")
    path = pathlib.Path(path_text)
    if not path.is_absolute():
        path = repository / path
    try:
        resolved = path.resolve(strict=True)
        resolved.relative_to(repository)
        cursor = path
        while cursor != repository and cursor != cursor.parent:
            if cursor.is_symlink():
                refuse(f"{label} traverses a symlink")
            cursor = cursor.parent
        if not resolved.is_file():
            refuse(f"{label} is not a regular private file")
        actual = hashlib.sha256(resolved.read_bytes()).hexdigest()
    except (OSError, ValueError):
        refuse(f"{label} is missing or outside the qualification root")
    if actual != digest:
        refuse(f"{label} SHA-256 does not match the qualified bytes")
    return actual


def validate(receipt_path: pathlib.Path, repository: pathlib.Path) -> None:
    private_root = (repository / "build/private-cpu-diagnostic").resolve(strict=True)
    if receipt_path.is_symlink():
        refuse("qualification receipt must not be a symlink")
    try:
        receipt_path = receipt_path.resolve(strict=True)
        receipt_path.relative_to(private_root)
        if not receipt_path.is_file() or receipt_path.stat().st_size > 1_048_576:
            refuse("qualification receipt is not a bounded private file")
        mode = receipt_path.stat().st_mode & 0o777
        if mode & 0o077:
            refuse("qualification receipt permissions expose private metadata")
        data = json.loads(receipt_path.read_text(encoding="utf-8"))
    except (OSError, ValueError, json.JSONDecodeError):
        refuse("qualification receipt is missing or malformed")

    data = object_with_keys(
        data,
        {
            "schema",
            "schema_version",
            "status",
            "source_revision",
            "checks",
            "toolchain",
            "project",
            "dependencies",
            "assets",
            "source",
        },
        "qualification receipt",
    )
    if data["schema"] != "t64.cpu-qualification" or data["schema_version"] != 1:
        refuse("qualification schema is unsupported")
    if data["status"] != "qualified":
        refuse("qualification status is not qualified")
    if data["source_revision"] != SOURCE_REVISION:
        refuse("pinned source revision does not match the qualification")

    checks = object_with_keys(data["checks"], REQUIRED_CHECKS, "qualification checks")
    if any(value != "pass" for value in checks.values()):
        refuse("one or more required audit checks are not passed")

    toolchain = object_with_keys(
        data["toolchain"],
        {"channel", "host_target", "manifest", "archives"},
        "toolchain",
    )
    if toolchain["channel"] != "nightly-2026-07-16":
        refuse("Rust channel does not match the audited dated toolchain")
    if toolchain["host_target"] != "aarch64-apple-darwin":
        refuse("Rust host target does not match the audited machine")
    manifest_sha = checked_file(
        repository, toolchain["manifest"], "dated Rust channel manifest"
    )
    if manifest_sha != CHANNEL_MANIFEST_SHA256:
        refuse("dated Rust channel manifest SHA-256 differs from the audited manifest")
    try:
        manifest_path = pathlib.Path(toolchain["manifest"]["path"])
        if not manifest_path.is_absolute():
            manifest_path = repository / manifest_path
        manifest = tomllib.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, KeyError, TypeError, tomllib.TOMLDecodeError):
        refuse("dated Rust channel manifest cannot be parsed")
    if manifest.get("date") != "2026-07-16":
        refuse("dated Rust channel manifest has the wrong release date")
    archives = toolchain["archives"]
    if not isinstance(archives, list):
        refuse("toolchain archives are missing")
    archive_names: set[str] = set()
    archive_rows: list[tuple[str, dict, str]] = []
    for entry in archives:
        row = object_with_keys(
            entry,
            {"name", "target", "manifest_sha256", "archive"},
            "toolchain archive",
        )
        name = row["name"]
        if not isinstance(name, str) or name in archive_names:
            refuse("toolchain archive names are missing or duplicated")
        archive_names.add(name)
        if row["target"] not in {toolchain["host_target"], "*"}:
            refuse(f"{name} target does not match the audited toolchain")
        manifest_target = manifest["pkg"][name]["target"][row["target"]]
        official_sha = manifest_target.get("xz_hash")
        if row["manifest_sha256"] != official_sha:
            refuse(f"{name} archive SHA-256 differs from the dated manifest")
        archive_rows.append((name, row, official_sha))
    if archive_names != REQUIRED_ARCHIVES:
        refuse("required Rust component archives are incomplete")

    project = object_with_keys(
        data["project"],
        {"manifest", "lockfile", "resolution_sha256", "locked"},
        "project package",
    )
    checked_file(repository, project["manifest"], "packaged Cargo.toml")
    lock_sha = checked_file(repository, project["lockfile"], "packaged Cargo.lock")
    if project["locked"] is not True:
        refuse("project build is not pinned with --locked")
    if project["resolution_sha256"] != lock_sha:
        refuse("resolved dependency graph differs from the packaged Cargo.lock")

    lock_path = pathlib.Path(project["lockfile"]["path"])
    if not lock_path.is_absolute():
        lock_path = repository / lock_path
    try:
        lock = tomllib.loads(lock_path.read_text(encoding="utf-8"))
    except (OSError, tomllib.TOMLDecodeError):
        refuse("packaged Cargo.lock cannot be parsed")
    locked_packages = {
        (row.get("name"), row.get("version"), row.get("checksum"))
        for row in lock.get("package", [])
        if isinstance(row, dict)
        and isinstance(row.get("source"), str)
        and row["source"].startswith("registry+")
    }

    dependencies = data["dependencies"]
    if not isinstance(dependencies, list) or not dependencies:
        refuse("registry dependency review is missing")
    reviewed_packages: set[tuple[str, str, str]] = set()
    for entry in dependencies:
        row = object_with_keys(
            entry,
            {
                "name",
                "version",
                "checksum",
                "archive",
                "legitimacy",
                "license_reviewed",
                "source_reviewed",
                "build_hook_reviewed",
                "proc_macro_reviewed",
            },
            "registry dependency review",
        )
        if (
            not isinstance(row["name"], str)
            or not row["name"]
            or not isinstance(row["version"], str)
            or not row["version"]
            or not isinstance(row["checksum"], str)
            or HEX64.fullmatch(row["checksum"]) is None
            or row["legitimacy"] != "verified"
            or any(
                row[field] is not True
                for field in (
                    "license_reviewed",
                    "source_reviewed",
                    "build_hook_reviewed",
                    "proc_macro_reviewed",
                )
            )
        ):
            refuse("a registry dependency is unverified or has an unreviewed hook")
        archive_sha = checked_file(
            repository, row["archive"], f"{row['name']} registry archive"
        )
        if archive_sha != row["checksum"]:
            refuse(f"{row['name']} source archive differs from its Cargo.lock checksum")
        package = (row["name"], row["version"], row["checksum"])
        if package in reviewed_packages:
            refuse("registry dependency review contains a duplicate package")
        reviewed_packages.add(package)
    if reviewed_packages != locked_packages:
        refuse("registry dependency review differs from the packaged Cargo.lock")

    assets = object_with_keys(
        data["assets"],
        {"prebuild_closure_reviewed", "closure_report", "excluded_before_compile"},
        "assets",
    )
    if assets["prebuild_closure_reviewed"] is not True:
        refuse("prebuild asset closure has not been reviewed")
    if not isinstance(assets["excluded_before_compile"], list) or set(
        assets["excluded_before_compile"]
    ) != REQUIRED_ASSETS:
        refuse("uncleared font, graphics, framebuffer, or non-CPU RSP startup remains")
    closure_sha = checked_file(
        repository, assets["closure_report"], "prebuild asset-closure report"
    )
    try:
        closure_path = pathlib.Path(assets["closure_report"]["path"])
        if not closure_path.is_absolute():
            closure_path = repository / closure_path
        closure = json.loads(closure_path.read_text(encoding="utf-8"))
    except (OSError, KeyError, TypeError, json.JSONDecodeError):
        refuse("prebuild asset-closure report is malformed")
    if (
        not isinstance(closure, dict)
        or closure.get("status") != "PASS_PREBUILD"
        or closure.get("source_revision") != SOURCE_REVISION
        or set(closure.get("excluded_before_compile", [])) != REQUIRED_ASSETS
    ):
        refuse("prebuild asset-closure report has unresolved inputs")

    source = object_with_keys(
        data["source"],
        {"membership_sealed", "projection", "overlay", "overlay_sha256"},
        "source projection",
    )
    if source["membership_sealed"] is not True:
        refuse("exact source membership is not sealed")
    projection_info = object_with_keys(
        source["projection"], {"path", "sha256", "projection_sha256"}, "source projection"
    )
    projection_sha = checked_file(
        repository,
        {"path": projection_info["path"], "sha256": projection_info["sha256"]},
        "prebuild source projection",
    )
    overlay_sha = checked_file(repository, source["overlay"], "authored source overlay")
    try:
        projection_path = pathlib.Path(source["projection"]["path"])
        if not projection_path.is_absolute():
            projection_path = repository / projection_path
        projection = json.loads(projection_path.read_text(encoding="utf-8"))
    except (OSError, KeyError, TypeError, json.JSONDecodeError):
        refuse("prebuild source projection is malformed")
    if (
        not isinstance(projection, dict)
        or projection.get("schema") != "t64.cpu-source-projection"
        or projection.get("schema_version") != 2
        or projection.get("source_revision") != SOURCE_REVISION
        or projection.get("prebuild_gate") != "ready-for-qualified-build"
        or projection.get("projection_sha256") != projection_info["projection_sha256"]
        or HEX64.fullmatch(str(projection.get("projection_sha256", ""))) is None
    ):
        refuse("source projection is not complete for the pinned source")
    if source["overlay_sha256"] != overlay_sha:
        refuse("authored overlay SHA-256 differs from its qualified bytes")

    for name, row, official_sha in archive_rows:
        archive_sha = checked_file(repository, row["archive"], f"{name} archive")
        if archive_sha != official_sha:
            refuse(f"{name} archive bytes differ from the dated manifest")


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: verify-cpu-diagnostic-qualification.py RECEIPT REPOSITORY", file=sys.stderr)
        return 2
    try:
        validate(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]).resolve(strict=True))
    except Refusal as error:
        print(f"pre-use qualification refused: {error}", file=sys.stderr)
        return 4
    print("pre-use qualification accepted")
    return 0


if __name__ == "__main__":
    sys.exit(main())
