#!/usr/bin/env python3
"""Fail-closed pre-use check for the private CPU diagnostic receipt."""

from __future__ import annotations

import hashlib
import json
import os
import pathlib
import re
import shutil
import stat
import subprocess
import sys
import tomllib

SOURCE_REVISION = "9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c"
CHANNEL_MANIFEST_SHA256 = "808b3fb16540d28b08fc8f9db812a129a13129d5413cdb30607a12ec7fa09632"
SOURCE_PROJECTION_SHA256 = "2382f2b434b4fbda80e73e26ad9ca226eb88a221817fae2e6eba35002d8469d0"
SOURCE_GROUPS = [
    "address_error_exception", "arithmetic", "cache", "cop0", "cop1",
    "cop_unusable", "endian_re", "exception_instructions", "jumps",
    "overflow_exception", "privilege", "tlb", "tlb64", "traps",
]
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
REQUIRED_ARCHIVES = {"rustc", "cargo", "rust-std", "rust-src"}
REQUIRED_ASSETS = {"font", "framebuffer", "graphics", "non_cpu_rsp_startup"}
REQUIRED_TARGET_TOOLS = {"as", "ld", "objcopy", "objdump"}
HEX64 = re.compile(r"[0-9a-f]{64}\Z")


class Refusal(Exception):
    pass


def refuse(message: str) -> None:
    raise Refusal(message)


def object_with_keys(value: object, keys: set[str], label: str) -> dict:
    if not isinstance(value, dict) or set(value) != keys:
        refuse(f"{label} fields do not match the closed qualification schema")
    return value


def reject_duplicate_keys(pairs: list[tuple[str, object]]) -> dict:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            refuse(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def lexical_path(path: pathlib.Path, base: pathlib.Path) -> pathlib.Path:
    if not path.is_absolute():
        path = base / path
    return pathlib.Path(os.path.abspath(path))


def reject_symlink_chain(path: pathlib.Path, boundary: pathlib.Path, label: str) -> None:
    try:
        relative = path.relative_to(boundary)
    except ValueError:
        refuse(f"{label} escapes its permitted directory")
    cursor = boundary
    if cursor.is_symlink():
        refuse(f"{label} traverses a symlink")
    for component in relative.parts:
        cursor = cursor / component
        if cursor.is_symlink():
            refuse(f"{label} traverses a symlink")


def sha256_path(path: pathlib.Path, label: str) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    except OSError:
        refuse(f"{label} cannot be read")
    return digest.hexdigest()


def checked_file(
    repository: pathlib.Path,
    value: object,
    label: str,
    private_root: pathlib.Path | None = None,
) -> str:
    row = object_with_keys(value, {"path", "sha256"}, label)
    path_text = row["path"]
    digest = row["sha256"]
    if not isinstance(path_text, str) or not path_text:
        refuse(f"{label} path is missing")
    if not isinstance(digest, str) or HEX64.fullmatch(digest) is None:
        refuse(f"{label} SHA-256 is malformed")
    path = lexical_path(pathlib.Path(path_text), repository)
    try:
        reject_symlink_chain(path, repository, label)
        if private_root is not None:
            reject_symlink_chain(path, private_root, label)
        resolved = path.resolve(strict=True)
        resolved.relative_to(repository)
        if not stat.S_ISREG(resolved.stat().st_mode):
            refuse(f"{label} is not a regular private file")
    except (OSError, ValueError):
        refuse(f"{label} is missing or outside the repository")
    actual = sha256_path(resolved, label)
    if actual != digest:
        refuse(f"{label} SHA-256 does not match the qualified bytes")
    return actual


def checked_private_directory(
    repository: pathlib.Path, private_root: pathlib.Path, text: object, label: str
) -> pathlib.Path:
    if not isinstance(text, str) or not text:
        refuse(f"{label} path is missing")
    path = lexical_path(pathlib.Path(text), repository)
    reject_symlink_chain(path, private_root, label)
    try:
        resolved = path.resolve(strict=True)
        resolved.relative_to(private_root)
        if not resolved.is_dir():
            refuse(f"{label} is not a private directory")
    except (OSError, ValueError):
        refuse(f"{label} is missing or outside the private qualification root")
    return resolved


def executable_candidate(repository: pathlib.Path, value: str, label: str) -> pathlib.Path:
    if not value or any(character.isspace() for character in value):
        refuse(f"{label} must name one executable path without wrapper arguments")
    path = pathlib.Path(value)
    if path.is_absolute() or "/" in value:
        path = lexical_path(path, repository)
    else:
        found = shutil.which(value)
        if found is None:
            refuse(f"{label} is unavailable on PATH")
        path = pathlib.Path(os.path.abspath(found))
    try:
        resolved = path.resolve(strict=True)
        if not resolved.is_file() or not os.access(resolved, os.X_OK):
            refuse(f"{label} is not executable")
    except OSError:
        refuse(f"{label} is unavailable")
    return resolved


def read_json_file(path: pathlib.Path, label: str, maximum: int = 16 * 1024 * 1024) -> object:
    try:
        if path.stat().st_size > maximum:
            refuse(f"{label} exceeds its size limit")
        return json.loads(
            path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate_keys
        )
    except Refusal:
        raise
    except (OSError, ValueError, json.JSONDecodeError):
        refuse(f"{label} is missing or malformed")


def check_tools(
    repository: pathlib.Path, value: object, private_root: pathlib.Path
) -> dict:
    tools = object_with_keys(value, {"host_c_compiler", "target_binutils"}, "build tools")
    compiler = object_with_keys(
        tools["host_c_compiler"], {"path", "sha256"}, "host C compiler"
    )
    compiler_path_text = compiler["path"]
    if not isinstance(compiler_path_text, str) or not pathlib.Path(
        compiler_path_text
    ).is_absolute():
        refuse("host C compiler must have an absolute reviewed path")
    compiler_path = pathlib.Path(compiler_path_text)
    if compiler_path.is_symlink() or not compiler_path.is_file() or not os.access(
        compiler_path, os.X_OK
    ):
        refuse("host C compiler path is not an executable regular file")
    try:
        canonical_compiler = compiler_path.resolve(strict=True)
    except OSError:
        refuse("host C compiler path is unavailable")
    if canonical_compiler != compiler_path:
        refuse("host C compiler path is not canonical")
    if not isinstance(compiler["sha256"], str) or HEX64.fullmatch(
        compiler["sha256"]
    ) is None:
        refuse("host C compiler SHA-256 is malformed")
    if sha256_path(compiler_path, "host C compiler") != compiler["sha256"]:
        refuse("host C compiler SHA-256 does not match the reviewed executable")
    # The receipt's canonical path is the executable the builder will use. If
    # the caller explicitly selects CC, require it to resolve to that exact
    # file; when CC is absent, don't silently substitute PATH's `cc` for the
    # receipt-bound compiler.
    selected_compiler = os.environ.get("CC")
    if selected_compiler is not None:
        requested_compiler = executable_candidate(
            repository, selected_compiler, "selected host C compiler"
        )
        if requested_compiler != compiler_path:
            refuse("selected host C compiler path differs from the reviewed executable")

    binutils = tools["target_binutils"]
    if not isinstance(binutils, list):
        refuse("target binutils review is missing")
    records: dict[str, str] = {}
    record_hashes: dict[str, str] = {}
    env_names = {
        "as": "T64_MIPS_AS",
        "ld": "T64_MIPS_LD",
        "objcopy": "T64_MIPS_OBJCOPY",
        "objdump": "T64_MIPS_OBJDUMP",
    }
    for entry in binutils:
        row = object_with_keys(entry, {"name", "path", "sha256"}, "target binutil")
        name = row["name"]
        if not isinstance(name, str) or name not in REQUIRED_TARGET_TOOLS or name in records:
            refuse("target binutils names are missing, unknown, or duplicated")
        binutil_lexical = lexical_path(pathlib.Path(row["path"]), repository)
        try:
            binutil_lexical.relative_to(private_root)
        except ValueError:
            try:
                binutil_lexical.relative_to(repository / "build/tools/install")
            except ValueError:
                refuse(f"target {name} path is outside the reviewed tool prefixes")
        digest = checked_file(
            repository,
            {"path": row["path"], "sha256": row["sha256"]},
            f"target {name}",
        )
        recorded_path = lexical_path(pathlib.Path(row["path"]), repository).resolve(
            strict=True
        )
        if not os.access(recorded_path, os.X_OK):
            refuse(f"target {name} is not executable")
        default_path = f"build/tools/install/bin/mips64-elf-{name}"
        requested_path = executable_candidate(
            repository, os.environ.get(env_names[name], default_path), f"target {name}"
        )
        if requested_path != recorded_path:
            refuse(f"selected target {name} path differs from the reviewed executable")
        if digest != row["sha256"]:
            refuse(f"target {name} SHA-256 differs from the reviewed executable")
        records[name] = str(recorded_path)
        record_hashes[name] = digest
    if set(records) != REQUIRED_TARGET_TOOLS:
        refuse("target binutils review is incomplete")
    return {
        "host_c_compiler": str(compiler_path),
        "host_c_compiler_sha256": compiler["sha256"],
        "target_binutils": records,
        "target_binutils_sha256": record_hashes,
    }


def lock_registry_packages(
    lock: object, root_package: tuple[str, str]
) -> set[tuple[str, str, str]]:
    if not isinstance(lock, dict) or not isinstance(lock.get("package"), list):
        refuse("packaged Cargo.lock has no valid package list")
    if (
        not isinstance(root_package, tuple)
        or len(root_package) != 2
        or any(not isinstance(part, str) or not part for part in root_package)
    ):
        refuse("packaged root package identity is malformed")
    packages: set[tuple[str, str, str]] = set()
    root_seen = False
    identities: set[tuple[str, str, str]] = set()
    registry_source = "registry+https://github.com/rust-lang/crates.io-index"
    for row in lock["package"]:
        if not isinstance(row, dict):
            refuse("packaged Cargo.lock contains a malformed package row")
        if set(row) - {"name", "version", "source", "checksum", "dependencies"}:
            refuse("packaged Cargo.lock contains an unsupported package field")
        name = row.get("name")
        version = row.get("version")
        if not isinstance(name, str) or not name or not isinstance(version, str) or not version:
            refuse("packaged Cargo.lock package identity is malformed")

        # Cargo's sole source-less package must be the exact root package from
        # the already hash-checked Cargo.toml. Path/workspace dependencies are
        # intentionally unsupported: their source closure is not recursively
        # accepted by this receipt.
        if "source" not in row:
            if (name, version) != root_package or root_seen:
                refuse("Cargo.lock contains an unreviewed source-less/path package")
            if "checksum" in row:
                refuse("source-less root package unexpectedly has a checksum")
            root_seen = True
            continue
        source = row["source"]
        if not isinstance(source, str) or not source.startswith("registry+"):
            refuse("Cargo.lock contains an unreviewed git or non-registry source")
        if source != registry_source:
            refuse("Cargo.lock contains an unreviewed registry source")
        checksum = row.get("checksum")
        if (
            not isinstance(checksum, str)
            or HEX64.fullmatch(checksum) is None
        ):
            refuse("Cargo.lock registry package identity or checksum is malformed")
        package = (name, version, checksum)
        identity = (name, version, source)
        if package in packages or identity in identities:
            refuse("Cargo.lock contains a duplicate registry package")
        packages.add(package)
        identities.add(identity)
    if not root_seen:
        refuse("Cargo.lock does not contain its exact root package")
    return packages


def verify_source_projection(
    repository: pathlib.Path,
    private_root: pathlib.Path,
    source: dict,
    projection: object,
) -> tuple[pathlib.Path, pathlib.Path]:
    projection = object_with_keys(
        projection,
        {
            "empty_required_groups",
            "prebuild_gate",
            "projection_sha256",
            "registrations",
            "required_group_coverage",
            "required_group_order",
            "reviewed_source_file_sha256",
            "schema",
            "schema_version",
            "source_revision",
            "source_value_identity",
            "testlist_sha256",
        },
        "prebuild source projection",
    )
    if (
        projection["schema"] != "t64.cpu-source-projection"
        or projection["schema_version"] != 2
        or projection["source_revision"] != SOURCE_REVISION
        or projection["prebuild_gate"] != "ready-for-qualified-build"
        or projection["projection_sha256"] != SOURCE_PROJECTION_SHA256
        or projection["required_group_order"] != SOURCE_GROUPS
        or projection["empty_required_groups"] != []
        or projection["source_value_identity"]
        != "registration ordinal and ordered Test::values index, bound by source file and values-method SHA-256"
    ):
        refuse("source projection differs from the reviewed source-derived seal")

    body = {key: value for key, value in projection.items() if key != "projection_sha256"}
    canonical = (
        json.dumps(body, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
    ).encode()
    if hashlib.sha256(canonical).hexdigest() != SOURCE_PROJECTION_SHA256:
        refuse("source projection digest does not bind its canonical contents")

    checkout = checked_private_directory(
        repository, private_root, source["checkout"], "pinned source checkout"
    )
    overlay_checkout = checked_private_directory(
        repository,
        private_root,
        source["overlay_checkout"],
        "authored overlay checkout",
    )
    git = shutil.which("git")
    if git is None:
        refuse("git is unavailable for the pinned source check")
    for checkout_path, label in (
        (checkout, "pinned source"),
        (overlay_checkout, "authored overlay"),
    ):
        try:
            result = subprocess.run(
                [git, "-C", str(checkout_path), "rev-parse", "HEAD"],
                check=True,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env={"PATH": os.environ.get("PATH", "/usr/bin:/bin"), "LC_ALL": "C"},
            )
        except (OSError, subprocess.SubprocessError):
            refuse(f"{label} checkout revision cannot be verified")
        if result.stdout.strip() != SOURCE_REVISION:
            refuse(f"{label} checkout is not based on the audited revision")

    file_hashes = projection["reviewed_source_file_sha256"]
    if not isinstance(file_hashes, dict) or not file_hashes:
        refuse("source projection has no reviewed source file hashes")
    for relative, expected_sha in file_hashes.items():
        if (
            not isinstance(relative, str)
            or not relative.startswith("src/tests/")
            or pathlib.PurePosixPath(relative).is_absolute()
            or ".." in pathlib.PurePosixPath(relative).parts
            or not isinstance(expected_sha, str)
            or HEX64.fullmatch(expected_sha) is None
        ):
            refuse("source projection contains a malformed source file binding")
        checked_file(
            repository,
            {"path": str(checkout / relative), "sha256": expected_sha},
            f"pinned {relative}",
            private_root,
        )
    testlist_sha = projection["testlist_sha256"]
    if not isinstance(testlist_sha, str) or HEX64.fullmatch(testlist_sha) is None:
        refuse("source projection testlist SHA-256 is malformed")
    checked_file(
        repository,
        {"path": str(checkout / "src/tests/testlist.rs"), "sha256": testlist_sha},
        "pinned CPU test registration list",
        private_root,
    )

    rows = projection["registrations"]
    if not isinstance(rows, list) or len(rows) != 1085:
        refuse("source projection does not contain every pinned registration")
    names: set[str] = set()
    name_counts: dict[str, int] = {}
    for row in rows:
        if not isinstance(row, dict) or not isinstance(row.get("source_name"), str):
            refuse("source projection contains a malformed registration")
        name_counts[row["source_name"]] = name_counts.get(row["source_name"], 0) + 1
    coverage = {group: {"registrations": 0, "values": 0} for group in SOURCE_GROUPS}
    level_lanes = {
        "Timing": "timing",
        "Cycle": "cycle",
        "COP0Hazard": "cop0-hazard",
        "PoorlyUnderstoodQuirk": "quirk",
        "StressTest": "stress",
    }
    required_levels = {"BasicFunctionality", "RarelyUsed", "Weird"}
    for index, row in enumerate(rows):
        object_with_keys(
            row,
            {
                "constructor", "effective_value_count", "execution", "group", "index",
                "lane", "level", "name", "reason", "source_file", "source_file_sha256",
                "source_group", "source_name", "source_value_count", "source_value_ids",
                "value_count_rule", "values", "values_method_sha256",
            },
            "source registration",
        )
        if row["index"] != index:
            refuse("source registration order is not exact")
        expected_name = row["source_name"] + (
            f" [source registration {index}]" if name_counts[row["source_name"]] > 1 else ""
        )
        if row["name"] != expected_name or not isinstance(row["name"], str):
            refuse("duplicate source names lack stable ordinal qualification")
        if row["name"] in names or len(row["name"].encode("utf-8")) > 256:
            refuse("source registration names are duplicated or exceed the protocol limit")
        names.add(row["name"])
        source_count = row["source_value_count"]
        effective_count = row["effective_value_count"]
        if (
            not isinstance(source_count, int)
            or isinstance(source_count, bool)
            or source_count < 0
            or not isinstance(effective_count, int)
            or isinstance(effective_count, bool)
            or effective_count != max(1, source_count)
            or row["source_file"] not in file_hashes
            or file_hashes[row["source_file"]] != row["source_file_sha256"]
        ):
            refuse("source registration count or source file binding is inconsistent")
        if row["lane"] == "required-base":
            if (
                row["group"] not in SOURCE_GROUPS
                or row["level"] not in required_levels
                or row["reason"] != "required"
            ):
                refuse("required-base lane does not follow the pinned default profile")
            coverage[row["group"]]["registrations"] += 1
            coverage[row["group"]]["values"] += effective_count
        elif row["lane"] == "skipped-non-cpu":
            if row["group"] != "non-cpu" or row["execution"] != "omitted" or row["values"] != []:
                refuse("non-CPU source registration is not explicitly omitted")
        else:
            if (
                row["group"] not in SOURCE_GROUPS
                or level_lanes.get(row["level"]) != row["lane"]
                or not isinstance(row["reason"], str)
                or not row["reason"]
            ):
                refuse("nonrequired CPU lane changes the pinned source level")
        source_ids = row["source_value_ids"]
        if source_count == 0:
            expected_ids = ["once"]
        else:
            expected_ids = [f"v{value_index:05d}" for value_index in range(source_count)]
        if not isinstance(source_ids, list) or source_ids != expected_ids:
            refuse("source value identities or ordering are incomplete")
        values = row["values"]
        if row["lane"] == "skipped-non-cpu":
            continue
        if not isinstance(values, list) or len(values) != effective_count:
            refuse("CPU source registration has an incomplete value list")
        if row["execution"] != ("once" if source_count == 0 else "values"):
            refuse("source zero-value execution semantics changed")
        for value_index, value in enumerate(values):
            object_with_keys(value, {"id", "index", "selector"}, "source value")
            if (
                value["id"] != expected_ids[value_index]
                or value["index"] != value_index
                or value["selector"] != f"r{index:04d}v{value_index:05d}"
            ):
                refuse("source value identity or selector ordering changed")
    if coverage != projection["required_group_coverage"] or any(
        item["values"] == 0 for item in coverage.values()
    ):
        refuse("required source group membership is incomplete")
    return checkout, overlay_checkout


def validate(receipt_path: pathlib.Path, repository: pathlib.Path) -> dict:
    private_candidate = repository / "build/private-cpu-diagnostic"
    reject_symlink_chain(private_candidate, repository, "private qualification root")
    try:
        private_root = private_candidate.resolve(strict=True)
        private_root.relative_to(repository)
        if not private_root.is_dir():
            refuse("private qualification root is not a directory")
    except (OSError, ValueError):
        refuse("private qualification root is missing or outside the checkout")

    receipt_path = lexical_path(receipt_path, repository)
    reject_symlink_chain(receipt_path, private_root, "qualification receipt")
    try:
        receipt_path = receipt_path.resolve(strict=True)
        receipt_path.relative_to(private_root)
        receipt_info = receipt_path.stat()
        if not stat.S_ISREG(receipt_info.st_mode) or receipt_info.st_size > 1_048_576:
            refuse("qualification receipt is not a bounded private file")
        mode = receipt_info.st_mode & 0o777
        if mode & 0o077:
            refuse("qualification receipt permissions expose private metadata")
        data = read_json_file(receipt_path, "qualification receipt", 1_048_576)
    except (OSError, ValueError):
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
            "tools",
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
        repository, toolchain["manifest"], "dated Rust channel manifest", private_root
    )
    if manifest_sha != CHANNEL_MANIFEST_SHA256:
        refuse("dated Rust channel manifest SHA-256 differs from the audited manifest")
    try:
        manifest_path = lexical_path(
            pathlib.Path(toolchain["manifest"]["path"]), repository
        )
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
        expected_target = "*" if name == "rust-src" else toolchain["host_target"]
        if row["target"] != expected_target:
            refuse(f"{name} target does not match the audited toolchain")
        try:
            manifest_target = manifest["pkg"][name]["target"][row["target"]]
        except (KeyError, TypeError):
            refuse(f"{name} archive component is absent from the dated manifest")
        official_sha = manifest_target.get("xz_hash")
        if row["manifest_sha256"] != official_sha:
            refuse(f"{name} archive SHA-256 differs from the dated manifest")
        archive_rows.append((name, row, official_sha))
    if archive_names != REQUIRED_ARCHIVES:
        refuse("required Rust component archives are incomplete")

    build_tools = check_tools(repository, data["tools"], private_root)

    project = object_with_keys(
        data["project"],
        {"manifest", "lockfile", "resolution_sha256", "locked"},
        "project package",
    )
    checked_file(repository, project["manifest"], "packaged Cargo.toml", private_root)
    manifest_path = lexical_path(
        pathlib.Path(project["manifest"]["path"]), repository
    )
    try:
        project_manifest = tomllib.loads(manifest_path.read_text(encoding="utf-8"))
        root_package = project_manifest["package"]
        root_identity = (root_package["name"], root_package["version"])
    except (OSError, KeyError, TypeError, tomllib.TOMLDecodeError):
        refuse("packaged Cargo.toml root package identity cannot be parsed")
    if (
        not isinstance(root_identity[0], str)
        or not root_identity[0]
        or not isinstance(root_identity[1], str)
        or not root_identity[1]
        or "workspace" in project_manifest
    ):
        refuse("packaged Cargo.toml is not one exact root package")
    lock_sha = checked_file(
        repository, project["lockfile"], "packaged Cargo.lock", private_root
    )
    if project["locked"] is not True:
        refuse("project build is not pinned with --locked")
    if project["resolution_sha256"] != lock_sha:
        refuse("resolved dependency graph differs from the packaged Cargo.lock")

    lock_path = lexical_path(pathlib.Path(project["lockfile"]["path"]), repository)
    try:
        lock = tomllib.loads(lock_path.read_text(encoding="utf-8"))
    except (OSError, tomllib.TOMLDecodeError):
        refuse("packaged Cargo.lock cannot be parsed")
    locked_packages = lock_registry_packages(lock, root_identity)

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
            repository, row["archive"], f"{row['name']} registry archive", private_root
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
    exclusions = assets["excluded_before_compile"]
    if (
        not isinstance(exclusions, list)
        or any(not isinstance(value, str) for value in exclusions)
        or set(exclusions) != REQUIRED_ASSETS
    ):
        refuse("uncleared font, graphics, framebuffer, or non-CPU RSP startup remains")
    closure_sha = checked_file(
        repository,
        assets["closure_report"],
        "prebuild asset-closure report",
        private_root,
    )
    try:
        closure_path = lexical_path(
            pathlib.Path(assets["closure_report"]["path"]), repository
        )
        closure = read_json_file(closure_path, "prebuild asset-closure report")
    except (KeyError, TypeError):
        refuse("prebuild asset-closure report is malformed")
    closure_exclusions = closure.get("excluded_before_compile", []) if isinstance(closure, dict) else None
    if (
        not isinstance(closure, dict)
        or closure.get("status") != "PASS_PREBUILD_SOURCE"
        or closure.get("source_revision") != SOURCE_REVISION
        or not isinstance(closure_exclusions, list)
        or any(not isinstance(value, str) for value in closure_exclusions)
        or set(closure_exclusions) != REQUIRED_ASSETS
    ):
        refuse("prebuild asset-closure report has unresolved inputs")

    source = object_with_keys(
        data["source"],
        {
            "membership_sealed",
            "checkout",
            "overlay_checkout",
            "projection",
            "overlay",
            "overlay_sha256",
        },
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
        private_root,
    )
    overlay_sha = checked_file(repository, source["overlay"], "authored source overlay")
    try:
        projection_path = lexical_path(
            pathlib.Path(source["projection"]["path"]), repository
        )
        projection = read_json_file(projection_path, "prebuild source projection")
    except (KeyError, TypeError):
        refuse("prebuild source projection is malformed")
    if projection_sha != projection_info["sha256"]:
        refuse("source projection file digest differs from its receipt")
    if projection_info["projection_sha256"] != SOURCE_PROJECTION_SHA256:
        refuse("source projection digest differs from the source-derived prebuild seal")
    checkout, overlay_checkout = verify_source_projection(
        repository, private_root, source, projection
    )
    try:
        manifest_path = lexical_path(
            pathlib.Path(project["manifest"]["path"]), repository
        ).resolve(strict=True)
        lock_path = lexical_path(
            pathlib.Path(project["lockfile"]["path"]), repository
        ).resolve(strict=True)
    except (OSError, ValueError):
        refuse("packaged Cargo files are missing or cannot be resolved")
    if manifest_path != overlay_checkout / "Cargo.toml" or lock_path != overlay_checkout / "Cargo.lock":
        refuse("packaged Cargo files are not the exact files in the reviewed overlay checkout")
    if source["overlay_sha256"] != overlay_sha:
        refuse("authored overlay SHA-256 differs from its qualified bytes")
    if (
        closure.get("overlay_sha256") != overlay_sha
        or closure.get("overlay_checkout") != str(overlay_checkout)
    ):
        refuse("prebuild asset closure is not bound to the authored overlay")
    changed_files = closure.get("changed_files")
    if not isinstance(changed_files, list) or not changed_files:
        refuse("prebuild asset closure does not enumerate the overlay files")
    seen_changed_paths: set[str] = set()
    for item in changed_files:
        row = object_with_keys(item, {"path", "sha256"}, "overlay output file")
        path_text = row["path"]
        if (
            not isinstance(path_text, str)
            or pathlib.PurePosixPath(path_text).is_absolute()
            or ".." in pathlib.PurePosixPath(path_text).parts
            or path_text in seen_changed_paths
        ):
            refuse("overlay closure contains an invalid or duplicate path")
        seen_changed_paths.add(path_text)
        checked_file(
            repository,
            {"path": str(overlay_checkout / path_text), "sha256": row["sha256"]},
            f"overlay output {path_text}",
            private_root,
        )
    git = shutil.which("git")
    try:
        status = subprocess.run(
            [git, "-C", str(overlay_checkout), "status", "--porcelain=v1", "-z", "--untracked-files=all"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env={"PATH": os.environ.get("PATH", "/usr/bin:/bin"), "LC_ALL": "C"},
        ).stdout
    except (OSError, subprocess.SubprocessError):
        refuse("authored overlay changed-file set cannot be verified")
    working_paths: set[str] = set()
    for record in status.split(b"\0"):
        if not record:
            continue
        if len(record) < 4 or record[2:3] != b" ":
            refuse("authored overlay has an unrecognized working-tree status")
        state = record[:2]
        try:
            changed_path = record[3:].decode("utf-8")
        except UnicodeDecodeError:
            refuse("authored overlay contains a non-UTF-8 path")
        if state == b"??":
            pass
        elif state in {b" M", b"M ", b"MM", b" A", b"A ", b"AM"}:
            pass
        else:
            refuse("authored overlay contains a rename, deletion, or unsupported file state")
        working_paths.add(changed_path)
    if working_paths != seen_changed_paths:
        refuse("prebuild asset closure does not enumerate the complete overlay diff")

    for name, row, official_sha in archive_rows:
        archive_sha = checked_file(
            repository, row["archive"], f"{name} archive", private_root
        )
        if archive_sha != official_sha:
            refuse(f"{name} archive bytes differ from the dated manifest")
    return build_tools


def main() -> int:
    compiler_path_mode = len(sys.argv) == 4 and sys.argv[1] == "--compiler-path"
    tool_paths_json_mode = len(sys.argv) == 4 and sys.argv[1] == "--tool-paths-json"
    if len(sys.argv) != (4 if compiler_path_mode or tool_paths_json_mode else 3):
        print(
            "usage: verify-cpu-diagnostic-qualification.py [--compiler-path|--tool-paths-json] RECEIPT REPOSITORY",
            file=sys.stderr,
        )
        return 2
    try:
        if compiler_path_mode or tool_paths_json_mode:
            receipt_arg, repository_arg = sys.argv[2], sys.argv[3]
        else:
            receipt_arg, repository_arg = sys.argv[1], sys.argv[2]
        tools = validate(
            pathlib.Path(receipt_arg), pathlib.Path(repository_arg).resolve(strict=True)
        )
    except Refusal as error:
        print(f"pre-use qualification refused: {error}", file=sys.stderr)
        return 4
    if compiler_path_mode:
        print(tools["host_c_compiler"])
    elif tool_paths_json_mode:
        print(json.dumps(tools, sort_keys=True, separators=(",", ":")))
    else:
        print("pre-use qualification accepted")
    return 0


if __name__ == "__main__":
    sys.exit(main())
