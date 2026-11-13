#!/usr/bin/env python3
"""Materialize an offline Cargo directory source from reviewed .crate archives."""

from __future__ import annotations

import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tarfile
import tomllib


MAX_ARCHIVE_BYTES = 64 * 1024 * 1024
MAX_UNPACKED_BYTES = 128 * 1024 * 1024


class Refusal(Exception):
    pass


def refuse(message: str) -> None:
    raise Refusal(message)


def require_private_root(repository: pathlib.Path) -> pathlib.Path:
    expected = repository / "build/private-cpu-diagnostic"
    cursor = repository
    for part in pathlib.PurePosixPath("build/private-cpu-diagnostic").parts:
        cursor = cursor / part
        if cursor.is_symlink():
            refuse("private qualification root traverses a symlink")
    if not expected.is_dir() or expected.resolve(strict=True) != expected:
        refuse("private qualification root is missing or noncanonical")
    if expected.stat().st_mode & 0o077:
        refuse("private qualification root is accessible to group or other users")
    return expected


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def checked_path(path: pathlib.Path, boundary: pathlib.Path, label: str) -> pathlib.Path:
    try:
        relative = path.relative_to(boundary)
    except ValueError:
        refuse(f"{label} escapes private storage")
    cursor = boundary
    if cursor.is_symlink():
        refuse(f"{label} traverses a symlink")
    for part in relative.parts:
        cursor = cursor / part
        if cursor.is_symlink():
            refuse(f"{label} traverses a symlink")
    try:
        resolved = path.resolve(strict=True)
        resolved.relative_to(boundary)
    except (OSError, ValueError):
        refuse(f"{label} is missing or outside private storage")
    if not resolved.is_file():
        refuse(f"{label} is not a regular file")
    return resolved


def file_map(root: pathlib.Path, *, allow_cargo_checksum: bool = False) -> dict[str, str]:
    result: dict[str, str] = {}
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            refuse("reviewed crate source contains a symbolic link")
        if path.is_dir():
            continue
        if not path.is_file():
            refuse("reviewed crate source contains a non-regular file")
        relative = path.relative_to(root).as_posix()
        if relative == ".cargo-checksum.json":
            if allow_cargo_checksum:
                continue
            refuse("reviewed source unexpectedly already contains Cargo vendor metadata")
        result[relative] = file_sha256(path)
    return result


def safe_member_path(name: str, expected_root: str) -> pathlib.PurePosixPath:
    if not name or name.startswith("/") or "\\" in name:
        refuse("crate archive contains an unsafe member path")
    raw_parts = name.split("/")
    if any(part in ("", ".", "..") for part in raw_parts):
        refuse("crate archive contains a non-canonical member path")
    path = pathlib.PurePosixPath(name)
    if any(part in ("", ".", "..") for part in path.parts):
        refuse("crate archive contains a non-canonical member path")
    if not path.parts or path.parts[0] != expected_root:
        refuse("crate archive member does not match its package identity")
    return path


def extract_reviewed_archive(
    archive: pathlib.Path,
    output: pathlib.Path,
    package_root: str,
    package_name: str,
    package_version: str,
    expected_source_files: dict[str, str],
) -> dict[str, str]:
    if archive.stat().st_size > MAX_ARCHIVE_BYTES:
        refuse("crate archive exceeds the audited size bound")
    expected_paths: dict[str, str] = {}
    unpacked_total = 0
    output.mkdir(mode=0o700)
    try:
        with tarfile.open(archive, mode="r:gz") as tar:
            members = tar.getmembers()
            if not members:
                refuse("crate archive is empty")
            seen: set[str] = set()
            for member in members:
                path = safe_member_path(member.name, package_root)
                if member.name in seen:
                    refuse("crate archive contains a duplicate path")
                seen.add(member.name)
                if member.isdir():
                    continue
                if not member.isfile() or member.issparse():
                    refuse("crate archive contains a link or special file")
                relative = pathlib.PurePosixPath(*path.parts[1:]).as_posix()
                if not relative or relative == ".cargo-checksum.json":
                    refuse("crate archive contains an invalid package file")
                unpacked_total += member.size
                if unpacked_total > MAX_UNPACKED_BYTES:
                    refuse("crate archive exceeds the unpacked size bound")
                destination = output.joinpath(*path.parts[1:])
                try:
                    destination.resolve(strict=False).relative_to(output.resolve(strict=True))
                except ValueError:
                    refuse("crate extraction would escape the private vendor root")
                destination.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
                extracted = tar.extractfile(member)
                if extracted is None:
                    refuse("crate archive file contents are missing")
                data = extracted.read(MAX_UNPACKED_BYTES + 1)
                if len(data) != member.size:
                    refuse("crate archive file size differs from its header")
                with destination.open("xb") as stream:
                    stream.write(data)
                mode = 0o755 if member.mode & 0o111 else 0o644
                os.chmod(destination, mode)
                expected_paths[relative] = sha256(data)
    except (OSError, tarfile.TarError) as error:
        refuse(f"crate archive cannot be safely extracted: {type(error).__name__}")
    if expected_paths != expected_source_files:
        refuse("crate archive contents differ from the previously reviewed source tree")
    cargo_manifest = output / "Cargo.toml"
    try:
        manifest = tomllib.loads(cargo_manifest.read_text(encoding="utf-8"))
        package = manifest["package"]
        identity = (package["name"], package["version"])
    except (OSError, KeyError, TypeError, tomllib.TOMLDecodeError):
        refuse("crate package manifest is missing or malformed")
    expected_identity = (package_name, package_version)
    if identity != expected_identity:
        refuse("crate package manifest identity differs from its archive path")
    return expected_paths


def prepare_sysroot_union(repository: pathlib.Path, private_root: pathlib.Path) -> int:
    project_verify = subprocess.run(
        [sys.executable, "-B", str(pathlib.Path(__file__).resolve()), "--verify-existing", str(repository)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
    )
    if project_verify.returncode != 0:
        refuse("project-only vendor did not pass its exact archive/source verification")

    toolchain_prefix = private_root / "toolchain-prefix"
    library = toolchain_prefix / "lib/rustlib/src/rust/library"
    sysroot_lock_path = library / "Cargo.lock"
    vendor_source = library / "vendor"
    archived_source = private_root / "root-rust-src-audit/source/library/vendor"
    audit_path = private_root / "root-rust-src-audit/audit.json"
    checks_path = private_root / "root-rust-src-audit/vendor-checks.json"
    output = private_root / "cargo-vendor-complete"
    report_path = private_root / "vendor-manifest-complete.json"
    stage = private_root / f"cargo-vendor-complete.stage.{os.getpid()}"
    for path, label in ((sysroot_lock_path, "sysroot lock"), (audit_path, "rust-src audit"),
                        (checks_path, "rust-src package checks")):
        if path.is_symlink() or not path.is_file():
            refuse(f"{label} is missing or linked")
    if (
        toolchain_prefix.is_symlink()
        or library.is_symlink()
        or vendor_source.is_symlink()
        or not vendor_source.is_dir()
        or archived_source.is_symlink()
        or not archived_source.is_dir()
    ):
        refuse("audited rust-src vendor source trees are missing or linked")
    if output.exists() or output.is_symlink() or report_path.exists() or report_path.is_symlink() or stage.exists() or stage.is_symlink():
        refuse("refusing to reuse an existing union vendor tree, report, or stage")

    rust_src_audit = json.loads(audit_path.read_text(encoding="utf-8"))
    if (
        rust_src_audit.get("status") != "SOURCE_COMPONENT_VERIFIED_EXPECTED_GRAPH_REVIEWED; ACTUAL_UNIT_GRAPH_CHECK_PENDING"
        or rust_src_audit.get("host_target") != "aarch64-apple-darwin"
        or rust_src_audit.get("sysroot_lock_sha256") != file_sha256(sysroot_lock_path)
        or rust_src_audit.get("vendored_registry_metadata", {}).get("all_file_hashes_match") is not True
        or rust_src_audit.get("vendored_registry_metadata", {}).get("all_package_checksums_match_sysroot_lock") is not True
    ):
        refuse("rust-src registry audit is not bound to the installed sysroot")
    checks = json.loads(checks_path.read_text(encoding="utf-8"))
    if not isinstance(checks, list) or len(checks) != 30:
        refuse("expected audited Rust sysroot registry set is missing")
    try:
        sysroot_lock = tomllib.loads(sysroot_lock_path.read_text(encoding="utf-8"))
    except (OSError, tomllib.TOMLDecodeError):
        refuse("installed rust-src Cargo.lock is malformed")
    locked_registry: dict[tuple[str, str], str] = {}
    path_packages: set[tuple[str, str]] = set()
    for row in sysroot_lock.get("package", []):
        if not isinstance(row, dict) or not isinstance(row.get("name"), str) or not isinstance(row.get("version"), str):
            refuse("installed rust-src lock contains a malformed package row")
        identity = (row["name"], row["version"])
        source = row.get("source")
        if source is None:
            if identity in path_packages:
                refuse("installed rust-src lock duplicates a path package")
            path_packages.add(identity)
            continue
        if source != "registry+https://github.com/rust-lang/crates.io-index":
            refuse("installed rust-src lock contains an unreviewed non-registry source")
        checksum = row.get("checksum")
        if not isinstance(checksum, str) or len(checksum) != 64 or identity in locked_registry:
            refuse("installed rust-src lock contains an invalid or duplicate registry package")
        locked_registry[identity] = checksum
    audited_registry: dict[tuple[str, str], dict] = {}
    for row in checks:
        if (
            not isinstance(row, dict)
            or row.get("source") != "registry+https://github.com/rust-lang/crates.io-index"
            or row.get("all_file_hashes_match") is not True
            or row.get("build_script_present") not in (True, False)
            or not isinstance(row.get("license"), str)
            or not row.get("license")
        ):
            refuse("rust-src registry source, license, or hook review is incomplete")
        identity = (row.get("name"), row.get("version"))
        if identity in audited_registry:
            refuse("rust-src registry audit contains a duplicate package")
        audited_registry[identity] = row
    if set(locked_registry) != set(audited_registry) or any(
        locked_registry[identity] != row.get("lock_checksum")
        for identity, row in audited_registry.items()
    ):
        refuse("rust-src vendor audit differs from every registry row in its lock")
    expected_sysroot_dirs = {f"{name}-{version}" for name, version in audited_registry}
    for source_root, label in ((vendor_source, "installed rust-src"), (archived_source, "audited rust-src")):
        actual_dirs = {item.name for item in source_root.iterdir() if item.is_dir() and not item.is_symlink()}
        if actual_dirs != expected_sysroot_dirs or any(
            item.is_symlink() or not item.is_dir() for item in source_root.iterdir()
        ):
            refuse(f"{label} vendor tree has missing or unexpected registry packages")

    project_report = json.loads((private_root / "vendor-manifest.json").read_text(encoding="utf-8"))
    project_rows = project_report.get("packages")
    if not isinstance(project_rows, list) or len(project_rows) != 14:
        refuse("project vendor manifest does not contain its exact 14 registry packages")
    identities: set[tuple[str, str]] = set()
    for row in project_rows:
        identity = (row.get("name"), row.get("version"))
        if identity in identities:
            refuse("project vendor manifest contains a duplicate package")
        identities.add(identity)
        if identity in audited_registry and audited_registry[identity].get("lock_checksum") != row.get("archive_sha256"):
            refuse("project and rust-src vendor contain conflicting bytes for one package identity")

    stage.mkdir(mode=0o700)
    project_vendor = private_root / "cargo-vendor"
    project_names = {f"{row['name']}-{row['version']}" for row in project_rows}
    actual_project_names = {item.name for item in project_vendor.iterdir() if item.is_dir() and not item.is_symlink()}
    if actual_project_names != project_names or any(item.is_symlink() or not item.is_dir() for item in project_vendor.iterdir()):
        refuse("project vendor tree has missing or unexpected package directories")
    for package_dir in sorted(project_names):
        shutil.copytree(project_vendor / package_dir, stage / package_dir, symlinks=False)

    sysroot_names: set[str] = set()
    sysroot_package_rows: list[dict[str, object]] = []
    for identity, row in sorted(audited_registry.items()):
        name, version = identity
        package_dir = f"{name}-{version}"
        source = vendor_source / package_dir
        archived = archived_source / package_dir
        if source.is_symlink() or not source.is_dir() or archived.is_symlink() or not archived.is_dir():
            refuse("audited rust-src vendor package is missing or linked")
        source_files = file_map(source, allow_cargo_checksum=True)
        archived_files = file_map(archived, allow_cargo_checksum=True)
        if source_files != archived_files or len(source_files) != row.get("vendored_file_count"):
            refuse("installed rust-src vendor files differ from the audited archive source")
        checksum_path = source / ".cargo-checksum.json"
        if checksum_path.is_symlink() or not checksum_path.is_file():
            refuse("audited rust-src vendor checksum is missing or linked")
        checksum = json.loads(checksum_path.read_text(encoding="utf-8"))
        if (
            not isinstance(checksum, dict)
            or set(checksum) - {"$comment", "files", "package"}
            or not {"files", "package"}.issubset(checksum)
            or ("$comment" in checksum and not isinstance(checksum["$comment"], str))
            or checksum.get("files") != source_files
            or checksum.get("package") != row["lock_checksum"]
        ):
            refuse("audited rust-src vendor checksum differs from its reviewed files/lock")
        manifest = tomllib.loads((source / "Cargo.toml").read_text(encoding="utf-8"))
        package = manifest.get("package", {})
        if (package.get("name"), package.get("version")) != identity:
            refuse("audited rust-src vendor manifest differs from its lock identity")
        if package_dir in project_names:
            if row["lock_checksum"] != next(
                item["archive_sha256"] for item in project_rows
                if f"{item['name']}-{item['version']}" == package_dir
            ):
                refuse("same project/sysroot vendor identity has conflicting checksums")
            sysroot_package_rows.append(
                {
                    "name": name,
                    "version": version,
                    "lock_checksum": row["lock_checksum"],
                    "vendored_file_count": len(source_files),
                    "build_script_present": row["build_script_present"],
                    "selection": "resolution-only until actual unit graph proves otherwise",
                    "materialization": "shared identical project registry package",
                }
            )
            continue
        shutil.copytree(source, stage / package_dir, symlinks=False)
        sysroot_names.add(package_dir)
        sysroot_package_rows.append(
            {
                "name": name,
                "version": version,
                "lock_checksum": row["lock_checksum"],
                "vendored_file_count": len(source_files),
                "build_script_present": row["build_script_present"],
                "selection": "resolution-only until actual unit graph proves otherwise",
                "materialization": "copied from audited rust-src vendor tree",
            }
        )

    report = {
        "schema": "t64.cpu-offline-vendor-union.v1",
        "status": "PASS_PROJECT_AND_SYSROOT_RESOLUTION_SOURCES_MATERIALIZED",
        "project_lock_sha256": project_report["source_lock_sha256"],
        "sysroot_lock_sha256": file_sha256(sysroot_lock_path),
        "project_packages": project_rows,
        "sysroot_registry_packages": sysroot_package_rows,
        "sysroot_path_packages": sorted(
            ({"name": n, "version": v} for n, v in path_packages),
            key=lambda row: (row["name"], row["version"]),
        ),
        "source_vendor_packages_copied": len(sysroot_names),
        "compiled_units": "pending actual offline --unit-graph",
        "vendor_path": "build/private-cpu-diagnostic/cargo-vendor-complete",
    }
    temporary_report = private_root / f"vendor-manifest-complete.json.tmp.{os.getpid()}"
    with temporary_report.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, sort_keys=True, indent=2)
        stream.write("\n")
    os.chmod(temporary_report, 0o600)
    os.replace(stage, output)
    os.replace(temporary_report, report_path)
    print("PASS: exact project registry sources and audited rust-src resolution vendor set are privately merged")
    return 0


def main() -> int:
    os.umask(0o077)
    mode = "--prepare"
    if len(sys.argv) == 3 and sys.argv[1] in {"--prepare", "--verify-existing", "--prepare-sysroot-union"}:
        mode, repository_arg = sys.argv[1], sys.argv[2]
    elif len(sys.argv) == 2:
        repository_arg = sys.argv[1]
    else:
        print("usage: prepare-cpu-diagnostic-vendor.py [--prepare|--verify-existing] REPOSITORY", file=sys.stderr)
        return 2
    repository = pathlib.Path(repository_arg).resolve(strict=True)
    private_root = require_private_root(repository)
    receipt_path = private_root / "qualification.json"
    if receipt_path.is_symlink() or not receipt_path.is_file():
        refuse("pre-use qualification receipt is missing or linked")
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    if receipt.get("schema") != "t64.cpu-qualification" or receipt.get("status") != "qualified":
        refuse("pre-use qualification receipt is not accepted")
    if any(value != "pass" for value in receipt.get("checks", {}).values()):
        refuse("pre-use qualification contains a failed check")
    verifier = repository / "scripts/verify-cpu-diagnostic-qualification.py"
    try:
        qualified = subprocess.run(
            [sys.executable, "-B", str(verifier), str(receipt_path), str(repository)],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
        )
    except OSError as error:
        refuse(f"production pre-use verifier could not run: {type(error).__name__}")
    if qualified.returncode != 0:
        refuse("production pre-use verifier refused offline vendoring")
    review_path = private_root / "package-registry-audit/qualification-source-review.json"
    if review_path.is_symlink() or not review_path.is_file():
        refuse("package source audit report is missing or linked")
    review = json.loads(review_path.read_text(encoding="utf-8"))
    if review.get("status") != "PASS_INPUT_AUDIT_NO_USE":
        refuse("package source audit is not complete")
    if mode == "--prepare-sysroot-union":
        return prepare_sysroot_union(repository, private_root)
    vendor_root = private_root / "cargo-vendor"
    report_path = private_root / "vendor-manifest.json"
    stage_root = private_root / f"cargo-vendor.stage.{os.getpid()}"
    if stage_root.exists() or stage_root.is_symlink():
        refuse("refusing to reuse an existing private Cargo vendor stage")
    if mode == "--prepare":
        if report_path.exists() or report_path.is_symlink():
            refuse("refusing to overwrite an existing private Cargo vendor receipt")
        if vendor_root.exists() or vendor_root.is_symlink():
            refuse("refusing to overwrite an existing private Cargo vendor tree")
        stage_root.mkdir(mode=0o700)
    else:
        if report_path.is_symlink() or not report_path.is_file():
            refuse("existing private Cargo vendor receipt is missing or linked")
        if vendor_root.is_symlink() or not vendor_root.is_dir():
            refuse("existing private Cargo vendor tree is missing or linked")

    review_by_package = {(row["name"], row["version"]): row for row in review["packages"]}
    dependency_rows = receipt.get("dependencies")
    if not isinstance(dependency_rows, list) or len(dependency_rows) != len(review_by_package):
        refuse("qualified dependency set differs from the package source review")
    packages: list[dict[str, object]] = []
    expected_package_dirs: set[str] = set()
    for dependency in dependency_rows:
        name = dependency.get("name")
        version = dependency.get("version")
        identity = (name, version)
        review_row = review_by_package.get(identity)
        if review_row is None or review_row.get("checksum") != dependency.get("checksum"):
            refuse("qualified dependency identity differs from package source review")
        if dependency.get("legitimacy") != "verified" or any(
            dependency.get(field) is not True
            for field in ("license_reviewed", "source_reviewed", "build_hook_reviewed", "proc_macro_reviewed")
        ):
            refuse("dependency has an unreviewed package source or hook")
        archive_text = dependency.get("archive", {}).get("path")
        if not isinstance(archive_text, str):
            refuse("qualified dependency archive path is missing")
        archive = checked_path(repository / archive_text, private_root, f"{name} archive")
        if file_sha256(archive) != dependency["checksum"]:
            refuse("qualified dependency archive bytes changed")
        source_dir = private_root / "package-registry-audit/source" / f"{name}-{version}"
        if source_dir.is_symlink() or not source_dir.is_dir():
            refuse("previously reviewed package source tree is missing or linked")
        reviewed_files = file_map(source_dir)
        if len(reviewed_files) != review_row.get("source_files_verified"):
            refuse("previously reviewed package file count changed")
        package_dir_name = f"{name}-{version}"
        expected_package_dirs.add(package_dir_name)
        if mode == "--prepare":
            destination = stage_root / package_dir_name
            archive_files = extract_reviewed_archive(
                archive,
                destination,
                package_dir_name,
                name,
                version,
                reviewed_files,
            )
            checksum_manifest = json.dumps(
                {"files": archive_files, "package": dependency["checksum"]},
                sort_keys=True,
                separators=(",", ":"),
            )
            checksum_path = destination / ".cargo-checksum.json"
            with checksum_path.open("x", encoding="utf-8") as stream:
                stream.write(checksum_manifest + "\n")
            os.chmod(checksum_path, 0o600)
        else:
            destination = vendor_root / package_dir_name
            if destination.is_symlink() or not destination.is_dir():
                refuse("existing vendor package directory is missing or linked")
            if file_map(destination, allow_cargo_checksum=True) != reviewed_files:
                refuse("existing vendor package differs from its reviewed archive source")
            checksum_path = destination / ".cargo-checksum.json"
            if checksum_path.is_symlink() or not checksum_path.is_file():
                refuse("existing vendor package checksum is missing or linked")
            try:
                checksum_value = json.loads(checksum_path.read_text(encoding="utf-8"))
            except (OSError, ValueError, json.JSONDecodeError):
                refuse("existing vendor package checksum is malformed")
            if checksum_value != {"files": reviewed_files, "package": dependency["checksum"]}:
                refuse("existing vendor package checksum differs from the reviewed source")
            archive_files = reviewed_files
        packages.append(
            {
                "name": name,
                "version": version,
                "archive_sha256": dependency["checksum"],
                "file_count": len(archive_files),
                "tree_sha256": sha256(
                    json.dumps(archive_files, sort_keys=True, separators=(",", ":")).encode()
                ),
                "vendor_path": f"build/private-cpu-diagnostic/cargo-vendor/{name}-{version}",
            }
        )

    report = {
        "schema": "t64.cpu-offline-vendor.v1",
        "status": "PASS_EXACT_REVIEWED_ARCHIVES_MATERIALIZED",
        "source_lock_sha256": receipt["project"]["lockfile"]["sha256"],
        "source_package_audit": "build/private-cpu-diagnostic/package-registry-audit/qualification-source-review.json",
        "packages": packages,
    }
    if mode == "--verify-existing":
        actual_package_dirs = {
            item.name for item in vendor_root.iterdir() if item.is_dir() and not item.is_symlink()
        }
        if actual_package_dirs != expected_package_dirs or any(
            item.is_symlink() or not item.is_dir() for item in vendor_root.iterdir()
        ):
            refuse("existing private vendor tree has missing or unexpected package directories")
        try:
            existing_report = json.loads(report_path.read_text(encoding="utf-8"))
        except (OSError, ValueError, json.JSONDecodeError):
            refuse("existing private Cargo vendor receipt is malformed")
        expected_keys = ("schema", "source_lock_sha256", "source_package_audit", "packages")
        if any(existing_report.get(key) != report.get(key) for key in expected_keys):
            refuse("existing vendor receipt differs from the verified private vendor tree")
        if existing_report.get("status") not in {
            "PASS_EXACT_REVIEWED_ARCHIVES_MATERIALIZED",
            "PASS_EXISTING_EXACT_REVIEWED_ARCHIVES_VERIFIED",
        }:
            refuse("existing private Cargo vendor receipt has an unexpected status")
        print("PASS: existing offline Cargo vendor tree matches all reviewed archives and checksums")
        return 0
    temporary_report = private_root / f"vendor-manifest.json.tmp.{os.getpid()}"
    with temporary_report.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, sort_keys=True, indent=2)
        stream.write("\n")
    os.chmod(temporary_report, 0o600)
    if mode == "--prepare":
        os.replace(stage_root, vendor_root)
    os.replace(temporary_report, report_path)
    print("PASS: all locked registry archives match the reviewed source trees and are privately vendored")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, Refusal, ValueError, TypeError, KeyError, tarfile.TarError) as error:
        print(f"refused: {error}", file=sys.stderr)
        raise SystemExit(4)
