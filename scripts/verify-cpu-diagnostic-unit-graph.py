#!/usr/bin/env python3
"""Validate the private frozen Cargo unit graph against audited lock/source inputs."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import stat
import sys
import tomllib
from urllib.parse import unquote, urlparse

sys.dont_write_bytecode = True

REGISTRY_SOURCE = "registry+https://github.com/rust-lang/crates.io-index"
HEX64 = re.compile(r"[0-9a-f]{64}\Z")


class Refusal(Exception):
    pass


def refuse(message: str) -> None:
    raise Refusal(message)


def sha256_path(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def identity(name: object, version: object, label: str) -> tuple[str, str]:
    if not isinstance(name, str) or not name or not isinstance(version, str) or not version:
        refuse(f"{label} package identity is malformed")
    return name, version


def package_id(value: object) -> tuple[str, str, str]:
    if not isinstance(value, str) or value.count("#") != 1:
        refuse("Cargo unit graph package id is malformed")
    source, suffix = value.split("#", 1)
    if "@" in suffix:
        name, version = suffix.rsplit("@", 1)
    elif source.startswith("path+file://") and suffix:
        name, version = path_source(source).name, suffix
    else:
        refuse("Cargo unit graph package id has no version")
    if not name or not version:
        refuse("Cargo unit graph package id has an empty name or version")
    return source, name, version


def path_source(source: str) -> pathlib.Path:
    if not source.startswith("path+file://"):
        refuse("Cargo unit graph contains an unreviewed package source")
    parsed = urlparse(source[len("path+"):])
    if parsed.scheme != "file" or parsed.netloc not in ("", "localhost"):
        refuse("Cargo unit graph path package has an unsupported file URL")
    path = pathlib.Path(unquote(parsed.path))
    if not path.is_absolute():
        refuse("Cargo unit graph path package is not absolute")
    return pathlib.Path(os.path.abspath(path))


def checked_lock_rows(lock: dict, registry: dict, label: str) -> tuple[dict, dict]:
    if not isinstance(lock, dict) or not isinstance(lock.get("package"), list):
        refuse(f"{label} lockfile has no package table")
    registry_rows: dict[tuple[str, str], str] = {}
    path_rows: dict[tuple[str, str], dict] = {}
    for row in lock["package"]:
        if not isinstance(row, dict):
            refuse(f"{label} lockfile contains a malformed package row")
        key = identity(row.get("name"), row.get("version"), label)
        if key in registry_rows or key in path_rows:
            refuse(f"{label} lockfile repeats a package identity")
        source = row.get("source")
        if source is None:
            path_rows[key] = row
        elif source == REGISTRY_SOURCE:
            checksum = row.get("checksum")
            if not isinstance(checksum, str) or HEX64.fullmatch(checksum) is None:
                refuse(f"{label} registry lock checksum is malformed")
            registry_rows[key] = checksum
        else:
            refuse(f"{label} lockfile contains an unreviewed source kind")
    if {key for key in registry if key not in registry_rows}:
        refuse(f"{label} reviewed registry identities are missing from its lockfile")
    if registry_rows != registry:
        refuse(f"{label} lockfile has an unreviewed registry identity or checksum")
    return registry_rows, path_rows


def default_feature_closure(manifest: dict) -> set[str]:
    features = manifest.get("features")
    if not isinstance(features, dict) or not isinstance(features.get("default"), list):
        refuse("pinned project manifest has no explicit default feature list")
    closure = {"default"}
    pending = list(features["default"])
    while pending:
        name = pending.pop()
        if not isinstance(name, str) or name not in features:
            refuse("pinned project default features use an unsupported feature expression")
        if name in closure:
            continue
        closure.add(name)
        members = features[name]
        if not isinstance(members, list):
            refuse("pinned project feature list is malformed")
        pending.extend(members)
    return closure


def package_identity_set(rows: object, checksum_key: str, label: str) -> dict[tuple[str, str], str]:
    if not isinstance(rows, list):
        refuse(f"{label} registry inventory is malformed")
    result: dict[tuple[str, str], str] = {}
    for row in rows:
        if not isinstance(row, dict):
            refuse(f"{label} registry inventory row is malformed")
        key = identity(row.get("name"), row.get("version"), label)
        digest = row.get(checksum_key)
        if not isinstance(digest, str) or HEX64.fullmatch(digest) is None or key in result:
            refuse(f"{label} registry inventory has a duplicate or malformed checksum")
        result[key] = digest
    return result


def dependency_name(value: object) -> str:
    if not isinstance(value, str) or not value:
        refuse("sysroot lock dependency identity is malformed")
    return value.split(" ", 1)[0]


def sysroot_path_closure(path_rows: dict, roots: list[str]) -> set[tuple[str, str]]:
    by_name: dict[str, list[tuple[str, str]]] = {}
    for key in path_rows:
        by_name.setdefault(key[0], []).append(key)
    if not isinstance(roots, list) or not roots or any(not isinstance(x, str) for x in roots):
        refuse("audited build-std roots are malformed")
    pending: list[tuple[str, str]] = []
    for name in roots:
        matches = by_name.get(name, [])
        if len(matches) != 1:
            refuse("audited build-std root is missing or ambiguous in the sysroot lock")
        pending.extend(matches)
    result: set[tuple[str, str]] = set()
    while pending:
        current = pending.pop()
        if current in result:
            continue
        result.add(current)
        row = path_rows[current]
        dependencies = row.get("dependencies", [])
        if not isinstance(dependencies, list):
            refuse("sysroot lock dependency list is malformed")
        for dependency in dependencies:
            matches = by_name.get(dependency_name(dependency), [])
            if len(matches) > 1:
                refuse("sysroot path dependency is ambiguous")
            if matches:
                pending.extend(matches)
    return result


def validate_graph(
    graph: dict,
    project_manifest: dict,
    project_lock: dict,
    sysroot_lock: dict,
    target_spec: dict,
    cargo_config: dict,
    vendor_report: dict,
    project_review: dict,
    rust_src_audit: dict,
    project_root: pathlib.Path,
    sysroot_root: pathlib.Path,
) -> dict:
    if not isinstance(graph, dict) or set(graph) != {"version", "units", "roots"} or type(graph["version"]) is not int or graph["version"] != 1:
        refuse("Cargo unit graph schema is unsupported")
    if target_spec.get("panic-strategy") != "abort":
        refuse("custom target does not declare panic-strategy abort")
    if not isinstance(vendor_report, dict) or vendor_report.get("status") != "PASS_PROJECT_AND_SYSROOT_RESOLUTION_SOURCES_MATERIALIZED":
        refuse("complete offline vendor receipt is not accepted")
    project_registry = package_identity_set(vendor_report.get("project_packages"), "archive_sha256", "project")
    sysroot_registry = package_identity_set(vendor_report.get("sysroot_registry_packages"), "lock_checksum", "sysroot")
    project_lock_registry, project_paths = checked_lock_rows(project_lock, project_registry, "project")
    sysroot_lock_registry, sysroot_paths = checked_lock_rows(sysroot_lock, sysroot_registry, "sysroot")

    project_package = project_manifest.get("package", {})
    root_identity = identity(project_package.get("name"), project_package.get("version"), "project root")
    if set(project_paths) != {root_identity}:
        refuse("project lockfile does not contain exactly its root source-less package")
    if project_package.get("name") != next(iter(project_paths))[0]:
        refuse("project lock root does not match its manifest")
    if project_paths[root_identity].get("dependencies") is None:
        refuse("project lock root dependency list is missing")

    audited_sysroot_paths: set[tuple[str, str]] = set()
    for row in vendor_report.get("sysroot_path_packages", []):
        if not isinstance(row, dict):
            refuse("sysroot path package inventory is malformed")
        key = identity(row.get("name"), row.get("version"), "sysroot path")
        if key in audited_sysroot_paths:
            refuse("sysroot path inventory repeats a package identity")
        audited_sysroot_paths.add(key)
    if set(sysroot_paths) != audited_sysroot_paths:
        refuse("sysroot source-less lock rows differ from the complete path-package audit")
    build_std = cargo_config.get("unstable", {}).get("build-std")
    if build_std != rust_src_audit.get("build_std_requested"):
        refuse("pinned Cargo config differs from the separately audited build-std roots")
    expected_sysroot = {
        identity(row.get("name"), row.get("version"), "audited sysroot compile")
        for row in rust_src_audit.get("source_derived_expected_compile_packages", [])
        if isinstance(row, dict)
    }
    if not expected_sysroot or expected_sysroot != sysroot_path_closure(sysroot_paths, build_std):
        refuse("sysroot path compile closure differs from the reviewed source-derived set")

    if project_review.get("status") != "PASS_INPUT_AUDIT_NO_USE" or project_review.get("external_code_executed") is not False:
        refuse("project source/build-hook audit is not accepted")
    review_packages = project_review.get("packages")
    if not isinstance(review_packages, list):
        refuse("project source/build-hook package audit is malformed")
    expected_hooks: set[tuple[str, str]] = set()
    expected_hook_files: dict[tuple[str, str], dict[str, str]] = {}
    expected_proc_macros: set[tuple[str, str]] = set()
    for row in review_packages:
        key = identity(row.get("name"), row.get("version"), "project source audit")
        if key not in project_registry or row.get("checksum") != project_registry[key]:
            refuse("project hook audit differs from the exact registry package lock")
        hooks = row.get("build_scripts")
        if not isinstance(hooks, list):
            refuse("project build-hook inventory is malformed")
        if hooks:
            expected_hooks.add(key)
            file_hashes: dict[str, str] = {}
            for hook in hooks:
                if not isinstance(hook, dict) or not isinstance(hook.get("path"), str):
                    refuse("project build-hook source record is malformed")
                digest = hook.get("sha256")
                if not isinstance(digest, str) or HEX64.fullmatch(digest) is None:
                    refuse("project build-hook source hash is malformed")
                if hook["path"] in file_hashes:
                    refuse("project build-hook source path is duplicated")
                file_hashes[hook["path"]] = digest
            expected_hook_files[key] = file_hashes
        if row.get("proc_macro_crate") is True:
            expected_proc_macros.add(key)
        elif row.get("proc_macro_crate") is not False:
            refuse("project proc-macro audit flag is malformed")
    if {key for key in (identity(row.get("name"), row.get("version"), "project review") for row in review_packages)} != set(project_registry):
        refuse("project source review does not cover every locked registry package")

    compiled_sysroot_rows = rust_src_audit.get("source_derived_expected_compile_packages")
    if not isinstance(compiled_sysroot_rows, list) or not compiled_sysroot_rows:
        refuse("source-derived sysroot compile audit is missing")
    expected_sysroot_hooks = {
        identity(row.get("name"), row.get("version"), "sysroot source review")
        for row in compiled_sysroot_rows
        if isinstance(row, dict) and isinstance(row.get("build_script_review"), str)
        and row["build_script_review"].startswith("Reviewed build.rs")
    }

    units = graph.get("units")
    roots = graph.get("roots")
    if not isinstance(units, list) or not units or not isinstance(roots, list) or len(roots) != 1:
        refuse("Cargo unit graph has no unique build root")
    if any(type(index) is not int or index < 0 or index >= len(units) for index in roots):
        refuse("Cargo unit graph root index is malformed or outside the graph")
    root_units: list[int] = []
    registry_units: set[tuple[str, str]] = set()
    project_path_units: set[tuple[str, str]] = set()
    sysroot_path_units: set[tuple[str, str]] = set()
    observed_project_hooks: set[tuple[str, str]] = set()
    observed_sysroot_hooks: set[tuple[str, str]] = set()
    observed_proc_macros: set[tuple[str, str]] = set()
    serialized_units: list[dict] = []
    for index, unit in enumerate(units):
        required_unit_keys = {"dependencies", "features", "mode", "pkg_id", "platform", "profile", "target"}
        if not isinstance(unit, dict) or frozenset(unit) not in {frozenset(required_unit_keys), frozenset(required_unit_keys | {"is_std"})}:
            refuse("Cargo unit graph unit schema is unsupported")
        is_std = unit.get("is_std", False)
        if type(is_std) is not bool:
            refuse("Cargo unit graph standard-library marker is malformed")
        source, name, version = package_id(unit.get("pkg_id"))
        key = (name, version)
        if source.startswith("registry+"):
            if source != REGISTRY_SOURCE or key not in project_registry:
                refuse("Cargo unit graph contains an unreviewed registry package")
            registry_units.add(key)
        elif source.startswith("path+file://"):
            source_path = path_source(source)
            if key == root_identity:
                if source_path != project_root:
                    refuse("Cargo unit graph project root path differs from the reviewed overlay")
                project_path_units.add(key)
                root_units.append(index)
            elif key in expected_sysroot and source_path.is_relative_to(sysroot_root):
                sysroot_path_units.add(key)
            else:
                refuse("Cargo unit graph contains an unreviewed path package")
        else:
            refuse("Cargo unit graph contains an unsupported package source")

        target = unit.get("target")
        profile = unit.get("profile")
        target_keys = {"crate_types", "doc", "doctest", "edition", "kind", "name", "src_path", "test"}
        profile_keys = {"codegen_backend", "codegen_units", "debug_assertions", "debuginfo", "incremental", "lto", "name", "opt_level", "overflow_checks", "panic", "rpath", "split_debuginfo", "strip"}
        if not isinstance(target, dict) or set(target) != target_keys or not isinstance(profile, dict) or set(profile) != profile_keys:
            refuse("Cargo unit graph target/profile schema is unsupported")
        if profile.get("name") != "release":
            refuse("Cargo unit graph contains a non-release unit")
        kind = target.get("kind")
        mode = unit.get("mode")
        platform = unit.get("platform")
        panic = profile.get("panic")
        if not isinstance(kind, list) or not kind or mode not in {"build", "run-custom-build"}:
            refuse("Cargo unit graph has a malformed target kind or mode")
        if is_std != (source.startswith("path+file://") and key in expected_sysroot):
            refuse("Cargo unit graph standard-library marker differs from its audited package source")
        if kind == ["custom-build"]:
            if panic != "unwind":
                refuse("Cargo custom-build unit is not using its required host unwind strategy")
            if mode == "build" and platform is not None:
                refuse("Cargo build-script compiler unit is not host-only")
            if mode == "run-custom-build":
                if source == REGISTRY_SOURCE:
                    observed_project_hooks.add(key)
                else:
                    observed_sysroot_hooks.add(key)
        elif mode != "build":
            refuse("Cargo unit graph has a non-build non-hook unit")
        elif platform == str(pathlib.Path(target_spec["__path__"])):
            if panic != "abort":
                refuse("a compiled target unit does not use the audited abort panic strategy")
        elif platform is None:
            if panic != "unwind":
                refuse("a host unit does not use its required unwind panic strategy")
        else:
            refuse("Cargo unit graph contains an unreviewed compilation platform")
        if source == REGISTRY_SOURCE and key in expected_proc_macros and kind == ["proc-macro"]:
            observed_proc_macros.add(key)
        if source == REGISTRY_SOURCE and key in expected_hook_files and kind == ["custom-build"]:
            hook_path = pathlib.Path(target.get("src_path", ""))
            if not hook_path.is_file():
                refuse("reviewed registry build-hook source is missing")
            expected_hash = expected_hook_files[key].get(hook_path.name)
            if expected_hash is None or sha256_path(hook_path) != expected_hash:
                refuse("compiled registry build-hook source differs from its reviewed bytes")
        if name == "compiler_builtins" and ("c" in unit.get("features", []) or "compiler-builtins-c" in unit.get("features", [])):
            refuse("compiler_builtins C feature is enabled without an audited compiler-rt toolchain")
        features = unit.get("features")
        if not isinstance(features, list) or any(not isinstance(feature, str) for feature in features) or len(set(features)) != len(features):
            refuse("Cargo unit graph feature set is malformed")
        if unit.get("dependencies") is not None:
            if not isinstance(unit["dependencies"], list):
                refuse("Cargo unit graph dependency list is malformed")
            for dependency in unit["dependencies"]:
                if not isinstance(dependency, dict) or set(dependency) != {"extern_crate_name", "index", "noprelude", "nounused", "public"}:
                    refuse("Cargo unit graph dependency schema is unsupported")
                dep_index = dependency.get("index")
                if type(dep_index) is not int or not 0 <= dep_index < len(units):
                    refuse("Cargo unit graph dependency index is outside the graph")
        if source.startswith("path+file://") and key == root_identity:
            if kind != ["bin"] or target.get("name") != root_identity[0] or set(features) != default_feature_closure(project_manifest):
                refuse("project root target/features differ from its pinned default profile")
            if profile.get("panic") != "abort" or profile.get("lto") != "fat" or profile.get("opt_level") != "3":
                refuse("project release profile differs from the reviewed target build")
        serialized_units.append({
            "package": f"{name}@{version}",
            "source_kind": "registry" if source == REGISTRY_SOURCE else ("project-path" if key == root_identity else "sysroot-path"),
            "target": target.get("name"),
            "kind": kind,
            "mode": mode,
            "platform": platform,
            "features": sorted(features) if isinstance(features, list) else [],
            "panic": panic,
            "is_std": is_std,
        })

    if registry_units != set(project_registry) or project_path_units != {root_identity} or sysroot_path_units != expected_sysroot:
        refuse("compiled package identities differ from the exact project/sysroot source closure")
    if root_units != roots:
        refuse("Cargo unit graph root does not identify the pinned project binary")
    if observed_project_hooks != expected_hooks or observed_sysroot_hooks != expected_sysroot_hooks:
        refuse("compiled build-hook set differs from the reviewed package hooks")
    if observed_proc_macros != expected_proc_macros:
        refuse("compiled proc-macro set differs from the reviewed package audit")

    return {
        "schema": "t64.cpu-unit-graph-review.v1",
        "status": "PASS_FROZEN_OFFLINE_UNIT_GRAPH",
        "built": False,
        "build_hooks_executed": False,
        "guest_execution_verified": False,
        "target_panic": target_spec["panic-strategy"],
        "project_root_features": sorted(default_feature_closure(project_manifest)),
        "registry_package_identities": sorted(f"{name}@{version}" for name, version in registry_units),
        "compiled_sysroot_path_packages": sorted(f"{name}@{version}" for name, version in sysroot_path_units),
        "project_build_hook_packages": sorted(f"{name}@{version}" for name, version in observed_project_hooks),
        "sysroot_build_hook_packages": sorted(f"{name}@{version}" for name, version in observed_sysroot_hooks),
        "proc_macro_packages": sorted(f"{name}@{version}" for name, version in observed_proc_macros),
        "unit_count": len(units),
        "units": serialized_units,
    }


def reject_duplicate_keys(pairs: list[tuple[str, object]]) -> dict:
    result: dict = {}
    for key, value in pairs:
        if key in result:
            refuse("JSON input contains a duplicate key")
        result[key] = value
    return result


def private_file(path: pathlib.Path, root: pathlib.Path, label: str) -> pathlib.Path:
    try:
        relative = path.relative_to(root)
    except ValueError:
        refuse(f"{label} is outside private diagnostic storage")
    if ".." in relative.parts:
        refuse(f"{label} contains a private path traversal")
    cursor = root
    if cursor.is_symlink():
        refuse("private diagnostic root is linked")
    for part in relative.parts:
        cursor = cursor / part
        if cursor.is_symlink():
            refuse(f"{label} traverses a symbolic link")
    try:
        resolved = path.resolve(strict=True)
        resolved.relative_to(root.resolve(strict=True))
    except (OSError, ValueError):
        refuse(f"{label} resolves outside private diagnostic storage")
    if resolved != path or not resolved.is_file():
        refuse(f"{label} is missing or not a regular file")
    return resolved


def private_output(path: pathlib.Path, root: pathlib.Path, label: str) -> pathlib.Path:
    try:
        relative = path.relative_to(root)
    except ValueError:
        refuse(f"{label} is outside private diagnostic storage")
    if ".." in relative.parts:
        refuse(f"{label} contains a private path traversal")
    cursor = root
    for part in relative.parts[:-1]:
        cursor = cursor / part
        if cursor.is_symlink():
            refuse(f"{label} traverses a symbolic link")
    if path.is_symlink():
        refuse(f"{label} is a symbolic link")
    try:
        resolved_parent = path.parent.resolve(strict=True)
        resolved_parent.relative_to(root.resolve(strict=True))
    except (OSError, ValueError):
        refuse(f"{label} parent is missing or outside private diagnostic storage")
    if not resolved_parent.is_dir():
        refuse(f"{label} parent is not a directory")
    return resolved_parent / path.name


def load_json(path: pathlib.Path, root: pathlib.Path, label: str) -> dict:
    private_file(path, root, label)
    try:
        value = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate_keys)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError):
        refuse(f"{label} is malformed")
    if not isinstance(value, dict):
        refuse(f"{label} is not a JSON object")
    return value


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    for name in (
        "graph", "project-manifest", "project-lock", "sysroot-lock", "target-spec",
        "cargo-config", "cargo-home-config", "vendor-report", "project-review", "rust-src-audit",
        "qualification", "toolchain-install", "source-projection", "source-audit",
        "source-closure", "output",
    ):
        parser.add_argument(f"--{name}", required=True)
    parser.add_argument("--private-root", default="build/private-cpu-diagnostic")
    args = parser.parse_args(argv)
    os.umask(0o077)
    repository = pathlib.Path(__file__).resolve().parents[1]
    private_root = pathlib.Path(args.private_root)
    if ".." in private_root.parts:
        refuse("private diagnostic root contains a path traversal")
    if not private_root.is_absolute():
        private_root = repository / private_root
    private_root = pathlib.Path(os.path.abspath(private_root))
    try:
        private_root.relative_to(repository)
    except ValueError:
        refuse("private diagnostic root is outside the checkout")
    if (
        private_root.is_symlink()
        or not private_root.is_dir()
        or private_root.resolve(strict=True) != private_root
        or stat.S_IMODE(private_root.stat().st_mode) & 0o077
    ):
        refuse("private diagnostic root is missing, linked, or exposed")
    paths = {name: pathlib.Path(getattr(args, name)) for name in (
        "graph", "project_manifest", "project_lock", "sysroot_lock", "target_spec",
        "cargo_config", "cargo_home_config", "vendor_report", "project_review", "rust_src_audit",
        "qualification", "toolchain_install", "source_projection", "source_audit",
        "source_closure",
    )}
    for label, path in paths.items():
        if not path.is_absolute():
            path = repository / path
            paths[label] = path
        private_file(path, private_root, label.replace("_", " "))
    output = pathlib.Path(args.output)
    if not output.is_absolute():
        output = repository / output
    output = private_output(output, private_root, "unit graph review output")
    if output.exists() or output.is_symlink():
        refuse("refusing to overwrite an existing private unit graph review")

    graph = load_json(paths["graph"], private_root, "unit graph")
    vendor_report = load_json(paths["vendor_report"], private_root, "offline vendor report")
    project_review = load_json(paths["project_review"], private_root, "project package source review")
    rust_src_audit = load_json(paths["rust_src_audit"], private_root, "rust-src audit")
    qualification = load_json(paths["qualification"], private_root, "pre-use qualification")
    toolchain_install = load_json(paths["toolchain_install"], private_root, "toolchain installation receipt")
    projection = load_json(paths["source_projection"], private_root, "source projection")
    source_audit = load_json(paths["source_audit"], private_root, "complete source audit")
    source_closure = load_json(paths["source_closure"], private_root, "source closure seal")
    try:
        project_manifest = tomllib.loads(paths["project_manifest"].read_text(encoding="utf-8"))
        project_lock = tomllib.loads(paths["project_lock"].read_text(encoding="utf-8"))
        sysroot_lock = tomllib.loads(paths["sysroot_lock"].read_text(encoding="utf-8"))
        target_spec = load_json(paths["target_spec"], private_root, "custom target")
        cargo_config = tomllib.loads(paths["cargo_config"].read_text(encoding="utf-8"))
        cargo_home_config = tomllib.loads(paths["cargo_home_config"].read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, tomllib.TOMLDecodeError):
        refuse("unit graph source inputs are malformed")

    vendor_path = pathlib.Path(paths["vendor_report"].parent / "cargo-vendor-complete").resolve(strict=True)
    if (
        cargo_home_config.get("source", {}).get("crates-io", {}).get("replace-with") != "t64-reviewed-vendor"
        or cargo_home_config.get("source", {}).get("t64-reviewed-vendor", {}).get("directory") != str(vendor_path)
        or cargo_home_config.get("net", {}).get("offline") is not True
    ):
        refuse("isolated Cargo home does not use the exact reviewed offline vendor source")

    if (
        qualification.get("schema") != "t64.cpu-qualification"
        or qualification.get("schema_version") != 1
        or qualification.get("status") != "qualified"
    ):
        refuse("pre-use qualification receipt is not accepted")
    checks = qualification.get("checks")
    required_checks = {
        "host_toolchain", "rust_source", "project_lock", "registry_dependencies",
        "build_hooks", "target_tools", "prebuild_asset_closure", "source_membership",
    }
    if not isinstance(checks, dict) or set(checks) != required_checks or any(value != "pass" for value in checks.values()):
        refuse("pre-use qualification receipt contains a missing or failed check")
    if toolchain_install.get("status") != "PASS_EXACT_COMPONENTS_INSTALLED":
        refuse("exact installed toolchain receipt is not accepted")
    if (
        sha256_path(paths["project_lock"]) != vendor_report.get("project_lock_sha256")
        or sha256_path(paths["sysroot_lock"]) != vendor_report.get("sysroot_lock_sha256")
        or sha256_path(paths["sysroot_lock"]) != rust_src_audit.get("sysroot_lock_sha256")
        or sha256_path(paths["target_spec"]) != rust_src_audit.get("custom_target_sha256")
    ):
        refuse("Cargo locks or custom target changed after their exact source audits")
    projection_body = {key: value for key, value in projection.items() if key != "projection_sha256"}
    projection_canonical = (json.dumps(projection_body, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode()
    if hashlib.sha256(projection_canonical).hexdigest() != projection.get("projection_sha256"):
        refuse("source projection seal does not match its contents")
    if (
        source_closure.get("status") != "PASS_PREBUILD_SOURCE"
        or source_closure.get("overlay_checkout") != str(paths["project_manifest"].parent)
        or source_closure.get("overlay_sha256") != sha256_path(repository / "tests/cpu-diagnostic/overlay.patch")
        or source_closure.get("source_projection_sha256") != projection.get("projection_sha256")
        or source_closure.get("source_audit_sha256") != sha256_path(paths["source_audit"])
        or source_closure.get("source_revision") != "9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c"
        or set(source_closure.get("excluded_before_compile", []))
        != {"font", "framebuffer", "graphics", "non_cpu_rsp_startup"}
    ):
        refuse("source closure does not bind the reviewed overlay, projection, exclusions, and pinned revision")
    module_files = source_closure.get("module_files")
    if not isinstance(module_files, list) or not module_files:
        refuse("source closure has no compiled module membership")
    seen_module_paths: set[str] = set()
    overlay_root = paths["project_manifest"].parent
    for row in module_files:
        if not isinstance(row, dict) or set(row) != {"path", "sha256"}:
            refuse("source closure module membership row is malformed")
        relative = row.get("path")
        digest = row.get("sha256")
        if not isinstance(relative, str) or relative in seen_module_paths or not isinstance(digest, str) or HEX64.fullmatch(digest) is None:
            refuse("source closure module membership is duplicated or malformed")
        seen_module_paths.add(relative)
        module_path = overlay_root / relative
        if module_path.is_symlink() or not module_path.is_file() or sha256_path(module_path) != digest:
            refuse("compiled module source differs from the prebuild source closure seal")

    project_root = paths["project_manifest"].parent
    sysroot_root = paths["sysroot_lock"].parent
    target_spec["__path__"] = str(paths["target_spec"])
    report = validate_graph(graph, project_manifest, project_lock, sysroot_lock, target_spec,
                            cargo_config, vendor_report, project_review, rust_src_audit,
                            project_root, sysroot_root)
    report.update({
        "inputs": {name: {"path": str(path.relative_to(repository)), "sha256": sha256_path(path)}
                   for name, path in paths.items()},
        "review_tool_sha256": sha256_path(pathlib.Path(__file__).resolve()),
        "output_path": str(output.relative_to(repository)),
    })
    output_parent = output.parent
    temporary = output.with_name(f"{output.name}.tmp.{os.getpid()}")
    with temporary.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, sort_keys=True, indent=2)
        stream.write("\n")
    os.chmod(temporary, 0o600)
    os.replace(temporary, output)
    print("PASS: frozen Cargo units, target panic profile, package closure, and reviewed hooks match private audits")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except (OSError, Refusal, ValueError, KeyError, TypeError, AttributeError) as error:
        print(f"unit-graph review refused: {error}", file=sys.stderr)
        raise SystemExit(4)
