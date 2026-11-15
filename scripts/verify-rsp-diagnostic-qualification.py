#!/usr/bin/env python3
"""Verify authored RSP qualification controls and private source receipts."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import pathlib
import re
import stat
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
BOOT_SHA = "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068"
CORE_BASE_REVISION = "d7479bcc854348c1384cc199815954c662ffe9be"
PLAN10_TESTED_SOURCE_REVISION = "cefbba2a35f0aa1e0ce9b1d64cfb5df3d41f44c5"
PLAN10_BINDING_BASE_REVISION = "88274a0748e2bb38fd7a7d3477914c534d993791"
PLAN10_V6_CORE_SOURCE_REVISION = "578e86a71752b74e65a4cb7dd9afbf545b0fc0fd"
PRODUCTION_SOURCE_PREFIXES = ("core", "runner", "tools/common")
V6_HOST_SOURCE_PATHS = frozenset({
    "scripts/check-rsp-evidence.py",
    "scripts/prepare-rsp-diagnostic.py",
    "scripts/verify-rsp-diagnostic-qualification.py",
    "scripts/run-rsp-diagnostic.py",
    "scripts/tests/test_rsp_evidence.py",
    "scripts/tests/test_rsp_qualification.py",
    "scripts/tests/test_rsp_run.py",
})
V6_BUILD_POLICY_PATHS = ("CMakeLists.txt", "CMakePresets.json")
V6_NORMAL_EVIDENCE_PATHS = ("build/gate/05-10-final-gate.log",)
V6_ASAN_EVIDENCE_PATHS = (
    "build/asan/05-10-final-build.log",
    "build/asan/05-10-final-inventory.log",
    "build/asan/05-10-final-ctest.log",
)
V6_ARCHIVE_RELATIVE = pathlib.PurePosixPath(
    "build/private-rsp-diagnostic/plan10-v6-executable-archive")
V6_REFRESH1_ARCHIVE_RELATIVE = pathlib.PurePosixPath(
    "build/private-rsp-diagnostic/plan10-v6-refresh1-executable-archive")
V6_BINDING_PATHS = {
    "v6": pathlib.PurePosixPath(
        "build/private-rsp-diagnostic/current-core-identity-binding-v6.json"),
    "v6-refresh1": pathlib.PurePosixPath(
        "build/private-rsp-diagnostic/current-core-identity-binding-v6-refresh1.json"),
}
V6_ARCHIVE_PATHS = {
    "v6": V6_ARCHIVE_RELATIVE,
    "v6-refresh1": V6_REFRESH1_ARCHIVE_RELATIVE,
}
V6_BINDING_SCHEMAS = {
    "v6": "t64.rsp-core-identity-binding.v6",
    "v6-refresh1": "t64.rsp-core-identity-binding.v6-refresh1",
}
V6_ARCHIVE_SCHEMAS = {
    "v6": "t64.rsp-plan10-v6-executable-archive.v1",
    "v6-refresh1": "t64.rsp-plan10-v6-refresh1-executable-archive.v1",
}
HEX40 = re.compile(r"[0-9a-f]{40}\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
RSP_PROFILE_ID = "ntsc-cart-4m-openipl3-rsp-functional-v1"
V5_ADAPTER_SOURCE_PROVENANCE = (
    "t64.rsp.adapter-source-scope-hashes-distinct-from-core-build-annotation.v1"
)
V5_ADAPTER_SOURCE_PATHS = frozenset({
    "runner/t64_rsp_diagnostic.c",
    "runner/t64_rsp_diag_protocol.c",
    "runner/t64_rsp_diag_protocol.h",
    "runner/tests/test_rsp_diag_protocol.c",
})


class Refusal(ValueError):
    pass


def refuse(message: str) -> None:
    raise Refusal(message)


def verify_v5_adapter_source_provenance(refresh: dict[str, object],
                                        source_annotation: str) -> None:
    if (refresh.get("source_provenance") != V5_ADAPTER_SOURCE_PROVENANCE or
            refresh.get("core_source_annotation") != source_annotation):
        refuse("refreshed adapter source scope and core build annotation are not separately identified")


def verify_v5_adapter_annotation_delta(paths: object,
                                       refresh_enabled: bool) -> None:
    if not isinstance(paths, (set, frozenset, list, tuple)) or any(not isinstance(path, str) for path in paths):
        refuse("core/adapter annotation delta paths are malformed")
    allowed = V5_ADAPTER_SOURCE_PATHS if refresh_enabled else frozenset()
    if set(paths) - allowed:
        refuse("core/compile-policy annotation delta exceeds the exact adapter source scope")


def verify_v5_adapter_archive_receipt(value: object) -> dict[str, object]:
    expected = {
        "adapter_path", "adapter_sha256", "binding_matches_adapter",
        "binding_path", "binding_sha256", "schema",
    }
    if not isinstance(value, dict) or set(value) != expected:
        refuse("pre-rebuild adapter archive receipt fields do not match the closed schema")
    if (value.get("schema") != "t64.rsp-plan10-pre-json-bound-validator-fix-adapter-archive.v1" or
            value.get("binding_matches_adapter") is not True):
        refuse("pre-rebuild adapter archive receipt does not bind the archived adapter")
    return value


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode()


def reject_duplicate_keys(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            refuse("duplicate JSON key")
        result[key] = value
    return result


def object_with_keys(value: object, keys: set[str], label: str) -> dict[str, object]:
    if not isinstance(value, dict) or set(value) != keys:
        refuse(f"{label} fields do not match the closed schema")
    return value


def read_json(path: pathlib.Path) -> object:
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 4 * 1024 * 1024:
        refuse("qualification fixture is linked, missing, or exceeds its size limit")
    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate_keys)
    except (OSError, json.JSONDecodeError, UnicodeDecodeError):
        refuse("qualification fixture is malformed")


def derive_rsp_membership(source_rows: object) -> list[dict[str, object]]:
    helper_path = ROOT / "scripts/prepare-rsp-diagnostic.py"
    spec = importlib.util.spec_from_file_location("t64_rsp_membership", helper_path)
    if spec is None or spec.loader is None:
        refuse("RSP source partition helper cannot be loaded")
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    try:
        return helper.derive_rsp_membership(source_rows)
    except (TypeError, ValueError) as error:
        refuse(f"RSP source registration partition is malformed: {error}")


def authored_closure_fixture() -> dict[str, object]:
    helper_path = ROOT / "scripts/prepare-rsp-diagnostic.py"
    spec = importlib.util.spec_from_file_location("t64_rsp_closure_fixture", helper_path)
    if spec is None or spec.loader is None:
        refuse("RSP authored closure helper cannot be loaded")
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    source_rows = helper.authored_rows()
    audit_rows = json.loads(json.dumps(source_rows))
    inventory_rows = derive_rsp_membership(source_rows)
    tools = [{"name": "authored-rustc", "sha256": "a" * 64}, {"name": "authored-objcopy", "sha256": "b" * 64}]
    locks = {"project": "c" * 64, "sysroot": "d" * 64, "vendor": "e" * 64}
    assets = {"boot_sha256": "f" * 64, "font_graphics_disposition": "excluded-unqualified"}
    return {
        "schema": "t64.rsp-authored-closure-fixture.v1",
        "component": "rsp",
        "source": {"rows": source_rows, "audit_rows": audit_rows, "source_sha256": digest(canonical(source_rows)), "audit_sha256": digest(canonical(audit_rows)), "inventory_rows": inventory_rows, "inventory_sha256": digest(canonical(inventory_rows))},
        "tools": {"files": tools, "manifest_sha256": digest(canonical(tools))},
        "locks": {"files": locks, "partition_sha256": digest(canonical(locks))},
        "assets": {**assets, "closure_sha256": digest(canonical(assets))},
    }


def verify_authored_closure_fixture(value: object) -> None:
    fixture = object_with_keys(value, {"assets", "component", "locks", "schema", "source", "tools"}, "authored RSP closure fixture")
    if fixture["schema"] != "t64.rsp-authored-closure-fixture.v1" or fixture["component"] != "rsp":
        refuse("authored closure fixture is not an RSP component")
    source = object_with_keys(fixture["source"], {"audit_rows", "audit_sha256", "inventory_rows", "inventory_sha256", "rows", "source_sha256"}, "authored source closure")
    if source["source_sha256"] != digest(canonical(source["rows"])) or source["audit_sha256"] != digest(canonical(source["audit_rows"])):
        refuse("authored source/audit closure digest changed")
    if source["rows"] != source["audit_rows"]:
        refuse("authored source registration differs from its source audit")
    expected_inventory = derive_rsp_membership(source["rows"])
    if source["inventory_rows"] != expected_inventory or source["inventory_sha256"] != digest(canonical(source["inventory_rows"])):
        refuse("authored source registration/value partition is incomplete")
    tools = object_with_keys(fixture["tools"], {"files", "manifest_sha256"}, "authored tool closure")
    if not isinstance(tools["files"], list) or not tools["files"] or tools["manifest_sha256"] != digest(canonical(tools["files"])):
        refuse("authored tool closure digest changed")
    seen_tools: set[str] = set()
    for row in tools["files"]:
        row = object_with_keys(row, {"name", "sha256"}, "authored tool closure row")
        if not isinstance(row["name"], str) or not row["name"] or row["name"] in seen_tools or re.fullmatch(r"[0-9a-f]{64}", str(row["sha256"])) is None:
            refuse("authored tool closure row is malformed or duplicated")
        seen_tools.add(row["name"])
    locks = object_with_keys(fixture["locks"], {"files", "partition_sha256"}, "authored lock partition")
    if not isinstance(locks["files"], dict) or set(locks["files"]) != {"project", "sysroot", "vendor"} or locks["partition_sha256"] != digest(canonical(locks["files"])) or any(re.fullmatch(r"[0-9a-f]{64}", str(row)) is None for row in locks["files"].values()):
        refuse("authored lock partition digest changed or is incomplete")
    assets = object_with_keys(fixture["assets"], {"boot_sha256", "closure_sha256", "font_graphics_disposition"}, "authored asset closure")
    asset_body = {key: assets[key] for key in ("boot_sha256", "font_graphics_disposition")}
    if assets["closure_sha256"] != digest(canonical(asset_body)) or assets["font_graphics_disposition"] != "excluded-unqualified" or re.fullmatch(r"[0-9a-f]{64}", str(assets["boot_sha256"])) is None:
        refuse("authored asset closure digest changed or retains an unqualified asset")
    print("PASS: authored source, tool, lock, and asset closure verified")


def private_path(path: pathlib.Path, label: str) -> pathlib.Path:
    path = pathlib.Path(os.path.abspath(path))
    try:
        path.relative_to(ROOT / "build/private-rsp-diagnostic")
    except ValueError:
        refuse(f"{label} escapes the private RSP root")
    cursor = ROOT
    for part in path.relative_to(ROOT).parts:
        cursor /= part
        if cursor.is_symlink():
            refuse(f"{label} traverses a symlink")
    if not path.is_file() or path.stat().st_size > 16 * 1024 * 1024:
        refuse(f"{label} is missing or exceeds its size limit")
    return path


def project_path(path: pathlib.Path, label: str, maximum: int = 64 * 1024 * 1024) -> pathlib.Path:
    path = pathlib.Path(os.path.abspath(path))
    try:
        path.relative_to(ROOT)
    except ValueError:
        refuse(f"{label} escapes the project root")
    cursor = ROOT
    for part in path.relative_to(ROOT).parts:
        cursor /= part
        if cursor.is_symlink():
            refuse(f"{label} traverses a symlink")
    if not path.is_file() or path.stat().st_size > maximum:
        refuse(f"{label} is missing or exceeds its size limit")
    return path


def private_digest(path: pathlib.Path, label: str) -> str:
    path = pathlib.Path(path)
    if path.is_symlink() or not path.is_file():
        refuse(f"{label} is missing or linked")
    return digest(path.read_bytes())


def validate_scoped_source_rows(rows: object, expected_paths: set[str]) -> dict[str, str]:
    if not isinstance(rows, list) or not rows:
        refuse("current RSP source scope is empty")
    found: dict[str, str] = {}
    for row in rows:
        row = object_with_keys(row, {"path", "sha256"}, "current RSP source row")
        relative = pathlib.PurePosixPath(str(row["path"]))
        if (relative.is_absolute() or any(part in ("", ".", "..", ".git") for part in relative.parts)
                or relative.as_posix() != row["path"] or row["path"] in found
                or not isinstance(row["sha256"], str)
                or re.fullmatch(r"[0-9a-f]{64}", row["sha256"]) is None):
            refuse("current RSP source scope path or digest is malformed")
        found[row["path"]] = row["sha256"]
    if set(found) != expected_paths:
        refuse("current RSP source scope does not match the complete reviewed source delta")
    return found


def git_names(*arguments: str) -> list[str]:
    result = subprocess.run(["git", "-C", str(ROOT), *arguments], check=True,
                            capture_output=True, text=True, timeout=30)
    return result.stdout.splitlines()


def reviewed_plan10_source_delta(baseline: str, tested: str) -> list[str]:
    if baseline != PLAN10_BINDING_BASE_REVISION or tested != PLAN10_TESTED_SOURCE_REVISION:
        refuse("current RSP source delta does not use the reviewed Plan10 source range")
    lines = git_names("diff", "--name-status", "--no-renames", baseline, tested,
                      "--", *PRODUCTION_SOURCE_PREFIXES)
    paths: list[str] = []
    for line in lines:
        fields = line.split("\t")
        if len(fields) != 2 or fields[0] not in {"A", "M"}:
            refuse("reviewed RSP source delta contains an unsupported file change")
        relative = pathlib.PurePosixPath(fields[1])
        if (relative.is_absolute() or relative.as_posix() != fields[1]
                or not any(fields[1].startswith(prefix + "/") for prefix in PRODUCTION_SOURCE_PREFIXES)):
            refuse("reviewed RSP source delta escapes the production source scope")
        paths.append(fields[1])
    if len(paths) != len(set(paths)):
        refuse("reviewed RSP source delta contains duplicate paths")
    return paths


def verify_v5_identity_receipts(binding: dict[str, object],
                                qualification_closure_sha: str,
                                identity: dict[str, object], runtime: dict[str, object],
                                source_annotation: str) -> dict[str, object]:
    attempt_relative = pathlib.PurePosixPath(str(binding["identity_attempt_dir"]))
    if (attempt_relative.is_absolute() or ".." in attempt_relative.parts or
            not attempt_relative.as_posix().startswith("build/private-rsp-diagnostic/plan10-current-binding-v5-")):
        refuse("v5 identity attempt escapes the private RSP root")
    attempt_dir = ROOT / attempt_relative
    if attempt_dir.is_symlink() or not attempt_dir.is_dir() or stat.S_IMODE(attempt_dir.stat().st_mode) != 0o700:
        refuse("v5 identity attempt directory is missing or not owner-only")

    closure_ref = object_with_keys(binding["preuse_source_closure"], {"path", "sha256"},
                                   "v5 source/tool/asset pre-use closure record")
    closure_command_path = private_path(ROOT / closure_ref["path"], "v5 source closure command")
    if (closure_ref["path"] != (attempt_relative / "source-closure-command.json").as_posix() or
            private_digest(closure_command_path, "v5 source closure command") != closure_ref["sha256"]):
        refuse("v5 source closure command record changed")
    closure_command = object_with_keys(read_json(closure_command_path),
                                       {"argv", "exit", "source_closure_sha256", "stderr_path",
                                        "stderr_sha256", "stdout_path", "stdout_sha256"},
                                       "v5 source closure command")
    closure_stdout_path = private_path(ROOT / closure_command["stdout_path"], "v5 source closure output")
    closure_stderr_path = private_path(ROOT / closure_command["stderr_path"], "v5 source closure diagnostics")
    closure_stdout = closure_stdout_path.read_bytes()
    closure_stderr = closure_stderr_path.read_bytes()
    if (closure_command["exit"] != 0 or
            closure_command["source_closure_sha256"] != qualification_closure_sha or
            closure_command["argv"] != ["python3", "-B", "scripts/verify-rsp-diagnostic-qualification.py",
                                         "--verify-source-closure", "build/private-rsp-diagnostic/qualification.json"] or
            closure_command["stdout_path"] != (attempt_relative / "source-closure.stdout.log").as_posix() or
            closure_command["stderr_path"] != (attempt_relative / "source-closure.stderr.log").as_posix() or
            digest(closure_stdout) != closure_command["stdout_sha256"] or
            digest(closure_stderr) != closure_command["stderr_sha256"] or
            b"PASS: independent RSP source, assets, tools, and locks verified" not in closure_stdout):
        refuse("v5 source/tool/asset/lock closure did not pass before identity use")

    hash_record_path = private_path(attempt_dir / "identity-executable-hashes.json",
                                    "v5 pre-identity executable hashes")
    hash_record = object_with_keys(read_json(hash_record_path),
                                   {"adapter_path", "adapter_sha256_before_use", "core_reader_path",
                                    "core_reader_sha256_before_use", "source_closure_command"},
                                   "v5 pre-identity executable hashes")
    receipts = binding["identity_command_receipts"]
    if not isinstance(receipts, list) or len(receipts) != 2:
        refuse("v5 identity command receipts are incomplete")
    core_identity_json: dict[str, object] | None = None
    for index, (executable, arguments) in enumerate((
            ("build/gate/runner/t64-rsp-diagnostic", ["--runtime-identity-json"]),
            ("build/gate/runner/t64-cpu-diagnostic", ["--compat-v2", "--runtime-identity-json"]))):
        receipt = object_with_keys(receipts[index],
                                   {"argv", "deadline_seconds", "elapsed_seconds", "exit",
                                    "process_group_empty", "stderr", "stderr_sha256", "stdout",
                                    "stdout_sha256", "timed_out"}, "v5 identity command receipt")
        if (receipt["argv"] != [str(ROOT / executable), *arguments] or receipt["exit"] != 0 or
                receipt["deadline_seconds"] != 10 or receipt["timed_out"] is not False or
                receipt["process_group_empty"] is not True):
            refuse("v5 current identity command was not bounded and reaped")
        stdout_path = private_path(attempt_dir / receipt["stdout"], "v5 identity command output")
        stderr_path = private_path(attempt_dir / receipt["stderr"], "v5 identity command diagnostics")
        stdout = stdout_path.read_bytes()
        stderr = stderr_path.read_bytes()
        if (len(stdout) > 4096 or len(stderr) > 4096 or digest(stdout) != receipt["stdout_sha256"] or
                digest(stderr) != receipt["stderr_sha256"]):
            refuse("v5 identity output differs from its bounded command receipt")
        command_identity = json.loads(stdout)
        if index == 0:
            if command_identity != runtime:
                refuse("v5 adapter identity output differs from its sealed receipt")
        else:
            core_identity_json = command_identity

    adapter_record = object_with_keys(binding["rsp_adapter_identity"],
                                      {"executable_sha256", "path"}, "current RSP adapter")
    adapter_path = project_path(ROOT / "build/gate/runner/t64-rsp-diagnostic",
                                "current RSP adapter", 512 * 1024 * 1024)
    core_path = project_path(ROOT / identity["path"], "current core identity reader",
                             512 * 1024 * 1024)
    if (adapter_record["path"] != adapter_path.relative_to(ROOT).as_posix() or
            digest(adapter_path.read_bytes()) != adapter_record["executable_sha256"] or
            digest(core_path.read_bytes()) != identity["executable_sha256"]):
        refuse("current v5 executable bytes differ from the pre-use binding")
    if (runtime["core_build_sha256"] != identity["core_build_sha256"] or
            runtime["core_source_revision"] != source_annotation or
            runtime["profile_id"] != binding["profile_id"] or
            not isinstance(runtime["profile_descriptor_sha256"], str) or
            re.fullmatch(r"[0-9a-f]{64}", runtime["profile_descriptor_sha256"]) is None):
        refuse("current RSP runtime identity differs from the pre-use binding")

    if (hash_record["adapter_path"] != adapter_path.relative_to(ROOT).as_posix() or
            hash_record["core_reader_path"] != identity["path"] or
            hash_record["adapter_sha256_before_use"] != adapter_record["executable_sha256"] or
            hash_record["core_reader_sha256_before_use"] != identity["executable_sha256"] or
            hash_record["source_closure_command"] != "passed-before-identity-use" or
            core_identity_json is None or
            core_identity_json.get("core_build_sha256") != runtime["core_build_sha256"] or
            core_identity_json.get("source_revision") != source_annotation):
        refuse("v5 executable hashes or CPU identity were not bound after source closure")
    return core_identity_json


def verify_v6_profile_identity(qualification: dict[str, object],
                               rsp_identity: dict[str, object],
                               cpu_identity: dict[str, object]) -> None:
    build_identity = qualification.get("build_identity")
    if (qualification.get("status") != "qualified" or
            qualification.get("component") != "rsp" or
            qualification.get("profile_id") != RSP_PROFILE_ID or
            not isinstance(build_identity, dict) or
            build_identity.get("profile_id") != RSP_PROFILE_ID or
            build_identity.get("core_model") != 6 or
            build_identity.get("core_state_schema") != 12 or
            rsp_identity.get("profile_id") != RSP_PROFILE_ID or
            not isinstance(rsp_identity.get("core_build_sha256"), str) or
            re.fullmatch(r"[0-9a-f]{64}", rsp_identity["core_build_sha256"]) is None or
            not isinstance(rsp_identity.get("profile_descriptor_sha256"), str) or
            re.fullmatch(r"[0-9a-f]{64}", rsp_identity["profile_descriptor_sha256"]) is None or
            cpu_identity.get("core_build_sha256") != rsp_identity.get("core_build_sha256") or
            cpu_identity.get("source_revision") != rsp_identity.get("core_source_revision") or
            rsp_identity.get("core_build_sha256") == build_identity.get("core_build_sha256")):
        refuse("v6 current identity does not match the exact functional RSP profile and changed core")


def verify_v6_host_source_paths(paths: object) -> None:
    if (not isinstance(paths, (list, tuple, set, frozenset)) or
            any(not isinstance(path, str) for path in paths) or
            set(paths) != V6_HOST_SOURCE_PATHS):
        refuse("v6 host-source delta differs from its exact allowlist")


def verify_v6_no_production_delta(paths: object) -> None:
    if not isinstance(paths, (list, tuple, set, frozenset)) or paths:
        refuse("v6 core or build-policy source differs from the exact reviewed baseline")


def verify_v6_ctest_proofs(normal_text: str, asan_text: str,
                           source_root: pathlib.Path = ROOT) -> int:
    helper_path = source_root / "scripts/check-rsp-evidence.py"
    spec = importlib.util.spec_from_file_location("t64_v6_native_evidence_verifier", helper_path)
    if spec is None or spec.loader is None:
        refuse("v6 strict native evidence helper cannot be loaded")
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    try:
        native_required = helper.native_gate_tests(source_root)
        helper.validate_ctest_log(normal_text, native_required)
        _, phase_tests, _ = helper.owned_tables(source_root)
        asan_required = (set(phase_tests) | {"runner.cpu-diagnostic-protocol",
                                            "runner.cpu-diagnostic-inventory",
                                            "fuzz.rom-admission",
                                            "sanitizer.negative-control"})
        allowed = native_required | {"fuzz.rom-admission", "sanitizer.negative-control"}
        helper.validate_ctest_log(asan_text, asan_required, allowed=allowed)
    except helper.Refusal:
        refuse("v6 saved native or sanitizer CTest rows fail strict inventory validation")
    return len(native_required)


def verify_v6_native_evidence(value: object) -> dict[str, list[dict[str, str]]]:
    evidence = object_with_keys(value, {"asan", "normal"}, "v6 saved native evidence")
    checked: dict[str, list[dict[str, str]]] = {"normal": [], "asan": []}
    normal_rows = evidence["normal"]
    if not isinstance(normal_rows, list) or len(normal_rows) != len(V6_NORMAL_EVIDENCE_PATHS):
        refuse("v6 canonical normal evidence set is incomplete")
    normal_content: bytes | None = None
    for row, expected_path in zip(normal_rows, V6_NORMAL_EVIDENCE_PATHS):
        row = object_with_keys(row, {"path", "sha256"}, "v6 normal evidence row")
        if row["path"] != expected_path:
            refuse("v6 normal evidence selects an unexpected report")
        path = project_path(ROOT / expected_path, "v6 normal evidence", 16 * 1024 * 1024)
        content = path.read_bytes()
        if digest(content) != row["sha256"]:
            refuse("v6 canonical normal evidence changed")
        normal_content = content
        checked["normal"].append({"path": expected_path, "sha256": digest(content)})
    if normal_content is None:
        refuse("v6 canonical normal gate evidence is missing")
    normal_inventory = re.search(rb"test inventory PASS preset=gate count=([0-9]+)", normal_content)
    normal_ctest = re.search(rb"100% tests passed out of ([0-9]+)", normal_content)
    normal_text = normal_content.decode("utf-8", errors="strict")

    asan_rows = evidence["asan"]
    if not isinstance(asan_rows, list) or len(asan_rows) != len(V6_ASAN_EVIDENCE_PATHS):
        refuse("v6 sanitizer evidence set is incomplete")
    asan_content: list[bytes] = []
    for row, expected_path in zip(asan_rows, V6_ASAN_EVIDENCE_PATHS):
        row = object_with_keys(row, {"path", "sha256"}, "v6 sanitizer evidence row")
        if row["path"] != expected_path:
            refuse("v6 sanitizer evidence selects an unexpected report")
        path = project_path(ROOT / expected_path, "v6 sanitizer evidence", 16 * 1024 * 1024)
        content = path.read_bytes()
        if digest(content) != row["sha256"]:
            refuse("v6 sanitizer evidence changed")
        checked["asan"].append({"path": expected_path, "sha256": digest(content)})
        asan_content.append(content)
    build, inventory, ctest = asan_content
    expected_native_count = verify_v6_ctest_proofs(
        normal_text, ctest.decode("utf-8", errors="strict"))
    if (normal_inventory is None or normal_ctest is None or
            normal_inventory.group(1) != normal_ctest.group(1) or
            int(normal_inventory.group(1)) != expected_native_count):
        refuse("v6 saved normal gate does not prove its complete native inventory")
    if (build.count(b"T64_ALL_TARGET_BUILD_EXIT=0\n") != 1 or
            b"Linking C executable runner/t64-cpu-diagnostic" not in build or
            b"Linking C executable runner/t64-rsp-diagnostic" not in build or
            re.search(rb"test inventory PASS preset=asan count=[0-9]+", inventory) is None or
            re.search(rb"100% tests passed out of [0-9]+", ctest) is None):
        refuse("v6 saved sanitizer inventory, all-target build, or owning tests did not pass")
    if re.search(rb"test inventory PASS preset=asan count=[0-9]+", inventory) is None:
        refuse("v6 saved sanitizer inventory is missing")
    return checked


def verify_v6_source_scope(binding: dict[str, object], prior: dict[str, object],
                           source_annotation: str) -> tuple[dict[str, str], dict[str, str]]:
    delta = object_with_keys(binding["reviewed_source_delta"], {
        "baseline_revision", "changed_paths", "core_tested_revision",
        "host_paths", "host_source_revision",
    }, "v6 reviewed source delta")
    baseline = prior.get("current_identity", {}).get("source_revision")
    host_revision = delta["host_source_revision"]
    tested = delta["core_tested_revision"]
    for revision in (baseline, host_revision, tested, source_annotation):
        if not isinstance(revision, str) or not re.fullmatch(r"[0-9a-f]{40}", revision):
            refuse("v6 reviewed source revision is malformed")
    if (baseline != PLAN10_BINDING_BASE_REVISION or tested != PLAN10_V6_CORE_SOURCE_REVISION or
            git_names("rev-parse", "--verify", f"{tested}^{{commit}}") != [tested] or
            git_names("rev-parse", "--verify", f"{host_revision}^{{commit}}") != [host_revision] or
            git_names("rev-parse", "--verify", f"{source_annotation}^{{commit}}") != [source_annotation] or
            not git_is_ancestor(tested, host_revision) or
            not git_is_ancestor(host_revision, source_annotation) or
            not git_is_ancestor(source_annotation, "HEAD")):
        refuse("v6 source binding does not pin its exact reviewed core and host revisions")

    prior_rows = prior.get("core_source_scope")
    if not isinstance(prior_rows, list) or not prior_rows:
        refuse("historical v4 production source scope is empty")
    prior_paths: set[str] = set()
    for row in prior_rows:
        row = object_with_keys(row, {"path", "sha256"}, "historical v4 production source row")
        relative = row["path"]
        if not isinstance(relative, str) or relative in prior_paths:
            refuse("historical v4 production source scope contains duplicate paths")
        old = subprocess.run(["git", "-C", str(ROOT), "show", f"{baseline}:{relative}"],
                             check=True, capture_output=True, timeout=10).stdout
        if digest(old) != row["sha256"]:
            refuse("historical v4 source row differs from its reviewed baseline")
        prior_paths.add(relative)

    changed: list[str] = []
    for line in git_names("diff", "--name-status", "--no-renames", baseline, tested,
                          "--", *PRODUCTION_SOURCE_PREFIXES):
        fields = line.split("\t")
        if len(fields) != 2 or fields[0] not in {"A", "M"}:
            refuse("v6 reviewed production delta contains an unsupported change")
        relative = pathlib.PurePosixPath(fields[1])
        if (relative.is_absolute() or relative.as_posix() != fields[1] or
                not any(fields[1].startswith(prefix + "/") for prefix in PRODUCTION_SOURCE_PREFIXES)):
            refuse("v6 reviewed production delta escapes the exact source scope")
        changed.append(fields[1])
    if len(changed) != len(set(changed)) or delta["changed_paths"] != changed:
        refuse("v6 production source delta omits or adds reviewed paths")
    expected_core_paths = prior_paths | set(changed)
    core_rows = validate_scoped_source_rows(binding["core_source_scope"], expected_core_paths)
    for relative, expected_sha in core_rows.items():
        source = project_path(ROOT / relative, "v6 reviewed production source").read_bytes()
        tested_bytes = subprocess.run(["git", "-C", str(ROOT), "show", f"{tested}:{relative}"],
                                      check=True, capture_output=True, timeout=10).stdout
        annotation_bytes = subprocess.run(["git", "-C", str(ROOT), "show", f"{source_annotation}:{relative}"],
                                          check=True, capture_output=True, timeout=10).stdout
        if source != tested_bytes or source != annotation_bytes or digest(source) != expected_sha:
            refuse("v6 production source differs from the pinned reviewed bytes")

    if delta["host_paths"] != sorted(V6_HOST_SOURCE_PATHS):
        refuse("v6 host source path set differs from its exact allowlist")
    source_scripts = git_names("diff", "--name-only", tested, host_revision,
                               "--", "scripts")
    head_scripts = git_names("diff", "--name-only", tested, "HEAD", "--", "scripts")
    verify_v6_host_source_paths(source_scripts)
    verify_v6_host_source_paths(head_scripts)
    if (git_names("diff", "--name-only", host_revision, source_annotation, "--", "scripts") or
            git_names("diff", "--name-only", host_revision, "HEAD", "--", "scripts")):
        refuse("v6 host provenance contains an unexpected script change")
    source_status = subprocess.run(
        ["git", "-C", str(ROOT), "status", "--porcelain=v1", "--untracked-files=all",
         "--", "core", "runner", "tools", *V6_BUILD_POLICY_PATHS, "scripts"],
        check=True, capture_output=True, text=True, timeout=10).stdout.splitlines()
    if source_status:
        refuse("v6 source or host tools have uncommitted changes")
    verify_v6_no_production_delta(git_names(
        "diff", "--name-only", tested, "HEAD", "--",
        "core", "runner", "tools", *V6_BUILD_POLICY_PATHS))
    verify_v6_no_production_delta(git_names(
        "diff", "--name-only", tested, source_annotation, "--",
        "core", "runner", "tools", *V6_BUILD_POLICY_PATHS))

    host_rows = validate_scoped_source_rows(
        binding["host_source_scope"], set(V6_HOST_SOURCE_PATHS))
    for relative, expected_sha in host_rows.items():
        path = project_path(ROOT / relative, "v6 host source", 2 * 1024 * 1024)
        committed = subprocess.run(["git", "-C", str(ROOT), "show", f"{host_revision}:{relative}"],
                                   check=True, capture_output=True, timeout=10).stdout
        if path.read_bytes() != committed or digest(committed) != expected_sha:
            refuse("v6 host source differs from its exact committed bytes")
    return core_rows, host_rows


def verify_v6_executable_archive(binding: dict[str, object],
                                 binding_bytes: bytes,
                                 binding_version: str = "v6") -> None:
    if binding_version not in V6_ARCHIVE_PATHS:
        refuse("unsupported RSP v6 binding context")
    expected_archive = V6_ARCHIVE_PATHS[binding_version]
    archive_relative = pathlib.PurePosixPath(str(binding.get("binary_archive_path", "")))
    if archive_relative != expected_archive:
        refuse("v6 binding selects an unexpected executable archive")
    archive_root = private_path(ROOT / archive_relative / "archive-receipt.json",
                                "v6 executable archive receipt").parent
    if archive_root.is_symlink() or not archive_root.is_dir() or stat.S_IMODE(archive_root.stat().st_mode) != 0o700:
        refuse("v6 executable archive is missing or not owner-only")
    receipt_path = archive_root / "archive-receipt.json"
    archived_paths = (receipt_path, archive_root / "identity-binding.json",
                      archive_root / "adapter.bin", archive_root / "core-reader.bin")
    if any(path.is_symlink() or not path.is_file() or stat.S_IMODE(path.stat().st_mode) != 0o600
           for path in archived_paths):
        refuse("v6 executable archive files are missing or not owner-only")
    receipt = object_with_keys(read_json(receipt_path), {
        "adapter_path", "adapter_sha256", "binding_path", "binding_sha256",
        "core_reader_path", "core_reader_sha256", "schema",
    }, "v6 executable archive receipt")
    if (receipt["schema"] != V6_ARCHIVE_SCHEMAS[binding_version] or
            receipt["adapter_path"] != (expected_archive / "adapter.bin").as_posix() or
            receipt["core_reader_path"] != (expected_archive / "core-reader.bin").as_posix() or
            receipt["binding_path"] != (expected_archive / "identity-binding.json").as_posix()):
        refuse("v6 executable archive receipt selects unexpected paths")
    for key in ("adapter_sha256", "binding_sha256", "core_reader_sha256"):
        if not isinstance(receipt[key], str) or not HEX64.fullmatch(receipt[key]):
            refuse("v6 executable archive digest is malformed")
    archived_binding = private_path(archive_root / "identity-binding.json",
                                    "archived v6 binding").read_bytes()
    archived_adapter = private_path(archive_root / "adapter.bin", "archived v6 adapter").read_bytes()
    archived_core = private_path(archive_root / "core-reader.bin", "archived v6 core reader").read_bytes()
    if (archived_binding != binding_bytes or digest(archived_binding) != receipt["binding_sha256"] or
            digest(archived_adapter) != receipt["adapter_sha256"] or
            digest(archived_core) != receipt["core_reader_sha256"] or
            receipt["adapter_sha256"] != binding["rsp_adapter_identity"].get("executable_sha256") or
            receipt["core_reader_sha256"] != binding["current_identity"].get("executable_sha256")):
        refuse("v6 executable archive does not preserve the exact bound binaries and binding")


def verify_v6_identity_receipts(binding: dict[str, object], qualification: dict[str, object],
                                identity: dict[str, object], runtime: dict[str, object],
                                binding_version: str = "v6") -> None:
    if binding_version not in V6_BINDING_PATHS:
        refuse("unsupported RSP v6 binding context")
    attempt_relative = pathlib.PurePosixPath(str(binding.get("identity_attempt_dir", "")))
    attempt_prefix = ("plan10-current-binding-v6-" if binding_version == "v6" else
                      "plan10-current-binding-v6-refresh1-")
    if (attempt_relative.is_absolute() or ".." in attempt_relative.parts or
            not attempt_relative.as_posix().startswith(
                f"build/private-rsp-diagnostic/{attempt_prefix}")):
        refuse("v6 identity attempt escapes the private RSP root")
    attempt_dir = ROOT / attempt_relative
    if attempt_dir.is_symlink() or not attempt_dir.is_dir() or stat.S_IMODE(attempt_dir.stat().st_mode) != 0o700:
        refuse("v6 identity attempt is missing or not owner-only")
    closure_ref = object_with_keys(binding["preuse_source_closure"], {"path", "sha256"},
                                   "v6 source closure record")
    closure_path = private_path(ROOT / closure_ref["path"], "v6 source closure command")
    if (stat.S_IMODE(closure_path.stat().st_mode) != 0o600 or
            closure_ref["path"] != (attempt_relative / "source-closure-command.json").as_posix() or
            private_digest(closure_path, "v6 source closure command") != closure_ref["sha256"]):
        refuse("v6 source closure command record changed")
    closure_command = object_with_keys(read_json(closure_path), {
        "argv", "exit", "source_closure_sha256", "stderr_path", "stderr_sha256",
        "stdout_path", "stdout_sha256",
    }, "v6 source closure command")
    stdout_path = private_path(ROOT / closure_command["stdout_path"], "v6 source closure output")
    stderr_path = private_path(ROOT / closure_command["stderr_path"], "v6 source closure diagnostics")
    if any(stat.S_IMODE(path.stat().st_mode) != 0o600 for path in (stdout_path, stderr_path)):
        refuse("v6 source closure logs are not owner-only")
    stdout, stderr = stdout_path.read_bytes(), stderr_path.read_bytes()
    closure_sha = qualification.get("closure", {}).get("sha256")
    if (closure_command["exit"] != 0 or closure_command["source_closure_sha256"] != closure_sha or
            closure_command["argv"] != ["python3", "-B", "scripts/verify-rsp-diagnostic-qualification.py",
                                         "--verify-source-closure", "build/private-rsp-diagnostic/qualification.json"] or
            closure_command["stdout_path"] != (attempt_relative / "source-closure.stdout.log").as_posix() or
            closure_command["stderr_path"] != (attempt_relative / "source-closure.stderr.log").as_posix() or
            digest(stdout) != closure_command["stdout_sha256"] or digest(stderr) != closure_command["stderr_sha256"] or
            b"PASS: independent RSP source, assets, tools, and locks verified" not in stdout):
        refuse("v6 source/tool/asset/lock closure did not pass before identity use")

    hash_path = private_path(attempt_dir / "identity-executable-hashes.json",
                             "v6 pre-identity executable hashes")
    if stat.S_IMODE(hash_path.stat().st_mode) != 0o600:
        refuse("v6 pre-identity executable hashes are not owner-only")
    hash_record = object_with_keys(
        read_json(hash_path),
        {"adapter_path", "adapter_sha256_before_use", "core_reader_path",
         "core_reader_sha256_before_use", "source_closure_command"},
        "v6 pre-identity executable hashes")
    receipts = binding["identity_command_receipts"]
    if not isinstance(receipts, list) or len(receipts) != 2:
        refuse("v6 identity command receipts are incomplete")
    cpu_json: dict[str, object] | None = None
    for index, (relative, arguments) in enumerate((
            ("build/gate/runner/t64-rsp-diagnostic", ["--runtime-identity-json"]),
            ("build/gate/runner/t64-cpu-diagnostic", ["--compat-v2", "--runtime-identity-json"]))):
        receipt = object_with_keys(receipts[index], {
            "argv", "deadline_seconds", "elapsed_seconds", "exit", "process_group_empty",
            "stderr", "stderr_sha256", "stdout", "stdout_sha256", "timed_out",
        }, "v6 identity command receipt")
        if (receipt["argv"] != [str(ROOT / relative), *arguments] or
                receipt["deadline_seconds"] != 10 or receipt["exit"] != 0 or
                receipt["timed_out"] is not False or receipt["process_group_empty"] is not True):
            refuse("v6 current identity command was not bounded and reaped")
        label = "rsp-runtime-identity" if index == 0 else "cpu-runtime-identity"
        if (receipt["stdout"] != f"{label}.stdout.log" or
                receipt["stderr"] != f"{label}.stderr.log"):
            refuse("v6 identity output paths differ from their fixed command labels")
        out_path = private_path(attempt_dir / receipt["stdout"], "v6 identity output")
        err_path = private_path(attempt_dir / receipt["stderr"], "v6 identity diagnostics")
        if any(stat.S_IMODE(path.stat().st_mode) != 0o600 for path in (out_path, err_path)):
            refuse("v6 identity command receipts are not owner-only")
        out, err = out_path.read_bytes(), err_path.read_bytes()
        if (len(out) > 4096 or len(err) > 4096 or digest(out) != receipt["stdout_sha256"] or
                digest(err) != receipt["stderr_sha256"]):
            refuse("v6 identity output differs from its bounded receipt")
        command_identity = json.loads(out)
        if index == 0:
            if command_identity != runtime:
                refuse("v6 RSP identity differs from its sealed receipt")
        else:
            cpu_json = command_identity

    adapter = project_path(ROOT / "build/gate/runner/t64-rsp-diagnostic", "v6 RSP adapter", 512 * 1024 * 1024)
    core = project_path(ROOT / str(identity.get("path", "")), "v6 CPU identity reader", 512 * 1024 * 1024)
    adapter_sha = digest(adapter.read_bytes())
    core_sha = digest(core.read_bytes())
    adapter_record = object_with_keys(binding["rsp_adapter_identity"],
                                      {"executable_sha256", "path"}, "v6 adapter identity")
    if (adapter_record["path"] != adapter.relative_to(ROOT).as_posix() or
            adapter_sha != adapter_record["executable_sha256"] or
            core_sha != identity.get("executable_sha256") or
            hash_record["adapter_path"] != adapter_record["path"] or
            hash_record["adapter_sha256_before_use"] != adapter_sha or
            hash_record["core_reader_path"] != identity.get("path") or
            hash_record["core_reader_sha256_before_use"] != core_sha or
            hash_record["source_closure_command"] != "passed-before-identity-use" or
            cpu_json is None or cpu_json.get("core_build_sha256") != runtime.get("core_build_sha256") or
            cpu_json.get("source_revision") != runtime.get("core_source_revision")):
        refuse("v6 executable hashes or CPU identity were not bound after source closure")
    verify_v6_profile_identity(qualification, runtime, cpu_json)


def verify_rsp_core_binding_v6(binding: dict[str, object], qualification_sha: str,
                               build_identity: dict[str, object],
                               binding_version: str = "v6") -> dict[str, object]:
    if binding_version not in V6_BINDING_PATHS:
        refuse("unsupported RSP v6 binding context")
    expected = {
        "authorized_for_future_guest_use", "binary_archive_path", "component",
        "core_model", "core_source_scope", "core_state_schema", "current_identity",
        "guest_execution_performed", "historical_first_use", "host_source_scope",
        "identity_attempt_dir", "identity_command_receipts", "native_evidence",
        "original_identity", "preuse_source_closure", "profile_id", "qualified_build_sha256",
        "qualified_rom_sha256", "qualification_sha256", "reviewed_source_delta",
        "rsp_adapter_identity", "rsp_runtime_identity", "schema", "status",
        "supersedes_binding_v4", "verification_scope",
    }
    binding = object_with_keys(binding, expected, "current RSP pre-use binding v6")
    if (binding["schema"] != V6_BINDING_SCHEMAS[binding_version] or
            binding["status"] != "preuse-verified-current-core-for-qualified-rsp-image" or
            binding["component"] != "rsp" or binding["qualification_sha256"] != qualification_sha or
            binding["profile_id"] != RSP_PROFILE_ID or binding["core_model"] != 6 or
            binding["core_state_schema"] != 12 or binding["profile_id"] != build_identity.get("profile_id") or
            binding["core_model"] != build_identity.get("core_model") or
            binding["core_state_schema"] != build_identity.get("core_state_schema") or
            binding["guest_execution_performed"] is not False or
            binding["authorized_for_future_guest_use"] is not True):
        refuse("current v6 RSP binding does not match the sealed functional profile")

    qualification_path = private_path(ROOT / "build/private-rsp-diagnostic/qualification.json",
                                      "original RSP qualification")
    if private_digest(qualification_path, "original RSP qualification") != qualification_sha:
        refuse("v6 binding changed the original qualification")
    qualification = read_json(qualification_path)
    build_path = private_path(ROOT / "build/private-rsp-diagnostic/qualified-build.json",
                              "original qualified RSP build")
    qualified_build = read_json(build_path)
    if (private_digest(build_path, "original qualified RSP build") != binding["qualified_build_sha256"] or
            qualified_build.get("qualification_sha256") != qualification_sha or
            qualified_build.get("guest_rom_executed") is not False):
        refuse("v6 binding changed the original unexecuted composed build")
    rom_record = qualified_build.get("artifacts", {}).get("rom", {})
    if rom_record.get("sha256") != binding["qualified_rom_sha256"]:
        refuse("v6 binding changed the qualified ROM identity")
    rom_path = private_path(ROOT / str(rom_record.get("path", "")), "original qualified RSP ROM")
    if private_digest(rom_path, "original qualified RSP ROM") != binding["qualified_rom_sha256"]:
        refuse("original qualified RSP ROM bytes changed")

    prior_ref = object_with_keys(binding["supersedes_binding_v4"], {"path", "sha256"},
                                 "historical v4 RSP binding")
    expected_prior_path = "build/private-rsp-diagnostic/current-core-identity-binding-v4.json"
    if prior_ref["path"] != expected_prior_path:
        refuse("v6 binding does not preserve the historical v4 receipt")
    prior_path = private_path(ROOT / prior_ref["path"], "historical v4 RSP binding")
    prior_bytes = prior_path.read_bytes()
    prior = read_json(prior_path)
    if (digest(prior_bytes) != prior_ref["sha256"] or
            prior.get("schema") != "t64.rsp-core-identity-binding.v4" or
            prior.get("qualification_sha256") != qualification_sha or
            prior.get("original_identity") != binding["original_identity"]):
        refuse("historical v4 binding is not an intact predecessor")

    identity = object_with_keys(binding["current_identity"], {
        "core_build_sha256", "executable_sha256", "path", "source_revision",
    }, "v6 current core identity")
    if (identity["path"] != "build/gate/runner/t64-cpu-diagnostic" or
            not isinstance(identity["core_build_sha256"], str) or
            re.fullmatch(r"[0-9a-f]{64}", identity["core_build_sha256"]) is None or
            not isinstance(identity["executable_sha256"], str) or
            re.fullmatch(r"[0-9a-f]{64}", identity["executable_sha256"]) is None or
            not isinstance(identity["source_revision"], str) or
            not HEX40.fullmatch(str(identity["source_revision"]))):
        refuse("v6 current core artifact identity is malformed")
    core_rows, host_rows = verify_v6_source_scope(binding, prior, identity["source_revision"])
    verify_v6_native_evidence(binding["native_evidence"])
    binding_path = private_path(ROOT / V6_BINDING_PATHS[binding_version],
                                "current v6 binding")
    verify_v6_executable_archive(binding, binding_path.read_bytes(), binding_version)

    history = object_with_keys(binding["historical_first_use"], {
        "adapter_exit", "classification", "historical_files",
        "original_launcher_failure_preserved", "outcome", "status",
    }, "preserved historical RSP first use")
    if (history != prior.get("historical_first_use") or history.get("outcome") != "refused" or
            history.get("classification") != "refused" or
            history.get("original_launcher_failure_preserved") is not True):
        refuse("v6 binding rewrote the historical refusal")
    for row in history["historical_files"]:
        row = object_with_keys(row, {"path", "sha256"}, "historical RSP first-use file")
        path = private_path(ROOT / row["path"], "historical RSP first-use file")
        if private_digest(path, "historical RSP first-use file") != row["sha256"]:
            refuse("historical RSP first-use evidence changed")

    runtime = object_with_keys(binding["rsp_runtime_identity"], {
        "core_build_sha256", "core_source_revision", "profile_descriptor_sha256", "profile_id",
    }, "v6 RSP runtime identity")
    if (runtime["core_build_sha256"] != identity["core_build_sha256"] or
            runtime["core_source_revision"] != identity["source_revision"] or
            runtime["profile_id"] != RSP_PROFILE_ID or
            not HEX64.fullmatch(str(runtime["profile_descriptor_sha256"])) or
            runtime["core_build_sha256"] == build_identity.get("core_build_sha256")):
        refuse("v6 RSP runtime identity differs from the exact current binding")
    adapter_record = object_with_keys(binding["rsp_adapter_identity"],
                                      {"executable_sha256", "path"}, "v6 current RSP adapter")
    if (adapter_record["path"] != "build/gate/runner/t64-rsp-diagnostic" or
            not HEX64.fullmatch(str(adapter_record["executable_sha256"]))):
        refuse("v6 RSP adapter identity is malformed")
    verify_v6_identity_receipts(binding, qualification, identity, runtime, binding_version)

    adapter_path = project_path(ROOT / adapter_record["path"], "current v6 RSP adapter", 512 * 1024 * 1024)
    core_path = project_path(ROOT / identity["path"], "current v6 core identity reader", 512 * 1024 * 1024)
    if (private_digest(adapter_path, "current v6 RSP adapter") != adapter_record["executable_sha256"] or
            private_digest(core_path, "current v6 core identity reader") != identity["executable_sha256"]):
        refuse("current v6 executable bytes differ from the pre-use binding")
    if private_digest(adapter_path, "v6 adapter before runtime identity") != adapter_record["executable_sha256"]:
        refuse("current v6 RSP adapter changed before its identity invocation")
    current_runtime = subprocess.run([str(adapter_path), "--runtime-identity-json"],
                                     cwd=ROOT, stdin=subprocess.DEVNULL,
                                     capture_output=True, timeout=10, check=True)
    if private_digest(core_path, "v6 core reader before runtime identity") != identity["executable_sha256"]:
        refuse("current v6 core identity reader changed before its identity invocation")
    current_cpu = subprocess.run([str(core_path), "--compat-v2", "--runtime-identity-json"],
                                 cwd=ROOT, stdin=subprocess.DEVNULL,
                                 capture_output=True, timeout=10, check=True)
    if (len(current_runtime.stdout) > 4096 or len(current_runtime.stderr) > 4096 or
            len(current_cpu.stdout) > 4096 or len(current_cpu.stderr) > 4096 or
            json.loads(current_runtime.stdout) != runtime or
            json.loads(current_cpu.stdout).get("core_build_sha256") != runtime["core_build_sha256"] or
            json.loads(current_cpu.stdout).get("source_revision") != identity["source_revision"] or
            private_digest(adapter_path, "current v6 RSP adapter after identity") != adapter_record["executable_sha256"] or
            private_digest(core_path, "current v6 core after identity") != identity["executable_sha256"]):
        refuse("current v6 core or adapter identity changed after its source and executable checks")
    return json.loads(current_runtime.stdout)


def verify_v5_adapter_refresh(binding: dict[str, object],
                              source_annotation: str) -> None:
    refresh = object_with_keys(binding.get("adapter_refresh"), {
        "adapter_source_scope", "archive_receipt", "asan_evidence",
        "core_source_annotation", "normal_build_test_receipt",
        "predecessor_binding", "source_provenance",
        "source_files_differ_from_annotation",
    }, "v5 refreshed adapter source binding")
    verify_v5_adapter_source_provenance(refresh, source_annotation)

    predecessor_ref = object_with_keys(refresh["predecessor_binding"], {"path", "sha256"},
                                       "previous v5 adapter binding")
    if predecessor_ref["path"] != "build/private-rsp-diagnostic/current-core-identity-binding-v5-refresh2.json":
        refuse("refreshed binding does not identify its archived v5 predecessor")
    predecessor_path = private_path(ROOT / predecessor_ref["path"], "previous v5 adapter binding")
    predecessor_bytes = predecessor_path.read_bytes()
    predecessor = read_json(predecessor_path)
    if (digest(predecessor_bytes) != predecessor_ref["sha256"] or
            predecessor.get("schema") != "t64.rsp-core-identity-binding.v5" or
            predecessor.get("qualification_sha256") != binding["qualification_sha256"]):
        refuse("previous v5 adapter binding changed or belongs to another qualification")

    archive_ref = object_with_keys(refresh["archive_receipt"], {"path", "sha256"},
                                   "pre-rebuild adapter archive receipt")
    expected_archive_path = "build/private-rsp-diagnostic/plan10-pre-json-bound-validator-fix-adapter/archive-receipt.json"
    if archive_ref["path"] != expected_archive_path:
        refuse("refreshed adapter binding selects an unexpected predecessor archive")
    archive_path = private_path(ROOT / archive_ref["path"], "pre-rebuild adapter archive receipt")
    archive_bytes = archive_path.read_bytes()
    archive = verify_v5_adapter_archive_receipt(read_json(archive_path))
    archived_adapter_path = private_path(archive_path.parent / "adapter.bin",
                                         "pre-rebuild adapter bytes")
    archived_binding_path = private_path(archive_path.parent / "identity-binding.json",
                                         "pre-rebuild adapter binding bytes")
    if (digest(archive_bytes) != archive_ref["sha256"] or
            archive["schema"] != "t64.rsp-plan10-pre-json-bound-validator-fix-adapter-archive.v1" or
            archive["adapter_path"] != "build/gate/runner/t64-rsp-diagnostic" or
            archive["binding_path"] != predecessor_ref["path"] or
            archive["adapter_sha256"] != predecessor.get("rsp_adapter_identity", {}).get("executable_sha256") or
            archive["binding_sha256"] != predecessor_ref["sha256"] or
            digest(archived_adapter_path.read_bytes()) != archive["adapter_sha256"] or
            archived_binding_path.read_bytes() != predecessor_bytes):
        refuse("pre-rebuild adapter or its exact predecessor binding changed")

    source_rows = refresh["adapter_source_scope"]
    expected_sources = {
        "runner/t64_rsp_diagnostic.c",
        "runner/t64_rsp_diag_protocol.c",
        "runner/t64_rsp_diag_protocol.h",
        "runner/tests/test_rsp_diag_protocol.c",
    }
    if not isinstance(source_rows, list):
        refuse("refreshed adapter source scope is not a list")
    seen_sources: set[str] = set()
    changed_from_annotation: list[str] = []
    for row in source_rows:
        row = object_with_keys(row, {"path", "sha256"}, "current adapter source row")
        relative = row["path"]
        if (relative not in expected_sources or relative in seen_sources or
                re.fullmatch(r"[0-9a-f]{64}", str(row["sha256"])) is None):
            refuse("refreshed adapter source scope is malformed or duplicated")
        seen_sources.add(relative)
        source_path = project_path(ROOT / relative, "current adapter source")
        source_bytes = source_path.read_bytes()
        if digest(source_bytes) != row["sha256"]:
            refuse("current adapter bytes differ from their private source binding")
        annotated_bytes = subprocess.run(
            ["git", "-C", str(ROOT), "show", f"{source_annotation}:{relative}"],
            check=True, capture_output=True, timeout=10).stdout
        if source_bytes != annotated_bytes:
            changed_from_annotation.append(relative)
    if (seen_sources != expected_sources or
            refresh["source_files_differ_from_annotation"] != sorted(changed_from_annotation) or
            not changed_from_annotation):
        refuse("refreshed adapter bytes do not explicitly distinguish adapter source from core annotation")

    normal_ref = object_with_keys(refresh["normal_build_test_receipt"], {"path", "sha256"},
                                  "validator-fix2 normal build and test receipt")
    if normal_ref["path"] != "build/private-rsp-diagnostic/plan10-validator-fix2-build-test-receipt-v2.json":
        refuse("refreshed adapter binding selects unexpected normal build evidence")
    normal_path = private_path(ROOT / normal_ref["path"], "validator-fix2 normal build and test receipt")
    normal_bytes = normal_path.read_bytes()
    normal = object_with_keys(read_json(normal_path), {"commands", "schema"},
                              "validator-fix2 normal build and test receipt")
    if (digest(normal_bytes) != normal_ref["sha256"] or
            normal["schema"] != "t64.rsp.validator-fix2-build-test-receipt.v1" or
            set(normal["commands"]) != {"configure", "normal_build", "owning_ctest"}):
        refuse("validator-fix2 normal build and test receipt changed")
    normal_configure = object_with_keys(normal["commands"]["configure"],
        {"argv", "exit", "output"}, "validator-fix2 normal configuration")
    normal_configure_output_ref = object_with_keys(normal_configure["output"],
        {"path", "sha256"}, "validator-fix2 normal configuration output")
    normal_configure_path = private_path(ROOT / normal_configure_output_ref["path"],
                                         "validator-fix2 normal configuration output")
    normal_configure_output = normal_configure_path.read_bytes()
    if (normal_configure["argv"] != ["cmake", "--preset", "gate"] or
            normal_configure["exit"] != 0 or
            normal_configure_output_ref["path"] != "build/private-rsp-diagnostic/plan10-validator-fix2-normal-configure.log" or
            digest(normal_configure_output) != normal_configure_output_ref["sha256"]):
        refuse("validator-fix2 normal configuration evidence changed")
    normal_build = object_with_keys(normal["commands"]["normal_build"],
        {"argv", "exit", "outputs", "scope"}, "validator-fix2 all-target normal build")
    expected_normal_build_paths = [
        "build/private-rsp-diagnostic/plan10-json-bound-validator-fix-normal-build.log",
        "build/private-rsp-diagnostic/plan10-validator-fix2-normal-build.log",
    ]
    if (normal_build["argv"] != ["cmake", "--build", "--preset", "gate"] or
            normal_build["exit"] != 0 or normal_build["scope"] != "all-targets" or
            not isinstance(normal_build["outputs"], list) or
            len(normal_build["outputs"]) != len(expected_normal_build_paths)):
        refuse("validator-fix2 normal all-target build evidence is malformed")
    normal_build_rows = [object_with_keys(row, {"path", "sha256"},
        "validator-fix2 normal build output") for row in normal_build["outputs"]]
    if [row["path"] for row in normal_build_rows] != expected_normal_build_paths:
        refuse("validator-fix2 normal all-target build output paths changed")
    for row, marker in zip(normal_build["outputs"], (
            b"Linking C executable runner/t64-rsp-diagnostic",
            b"Linking C executable runner/test_rsp_diag_protocol")):
        row = object_with_keys(row, {"path", "sha256"}, "validator-fix2 normal build output")
        output_path = private_path(ROOT / row["path"], "validator-fix2 normal build output")
        output = output_path.read_bytes()
        if digest(output) != row["sha256"] or marker not in output:
            refuse("validator-fix2 normal all-target build output changed or omitted an owning target")
    normal_test = object_with_keys(normal["commands"]["owning_ctest"],
        {"argv", "exit", "output"}, "validator-fix2 owning normal CTest")
    normal_test_output_ref = object_with_keys(normal_test["output"],
        {"path", "sha256"}, "validator-fix2 owning normal CTest output")
    normal_test_path = private_path(ROOT / normal_test_output_ref["path"],
                                    "validator-fix2 owning normal CTest output")
    normal_test_output = normal_test_path.read_bytes()
    if (normal_test["argv"] != ["ctest", "--preset", "gate", "-R",
            "^unit\\.rsp-diag-protocol$", "--output-on-failure"] or
            normal_test["exit"] != 0 or
            normal_test_output_ref["path"] != "build/private-rsp-diagnostic/plan10-validator-fix2-normal-ctest.log" or
            digest(normal_test_output) != normal_test_output_ref["sha256"] or
            b"100% tests passed" not in normal_test_output):
        refuse("validator-fix2 owning normal CTest evidence changed")

    asan_ref = object_with_keys(refresh["asan_evidence"], {"path", "sha256"},
                                "validator-fix ASan evidence")
    if asan_ref["path"] != "build/private-rsp-diagnostic/plan10-validator-fix2-asan-evidence-v2.json":
        refuse("refreshed adapter binding selects unexpected ASan evidence")
    asan_path = private_path(ROOT / asan_ref["path"], "validator-fix ASan evidence")
    asan_bytes = asan_path.read_bytes()
    asan = object_with_keys(read_json(asan_path), {"build", "configure", "schema", "test"},
                            "validator-fix2 ASan evidence")
    asan_test = object_with_keys(asan["test"],
        {"argv", "environment", "exit", "output"},
        "validator-fix2 ASan owning test")
    asan_test_output_ref = object_with_keys(asan_test["output"],
        {"path", "sha256"}, "validator-fix2 ASan owning test output")
    asan_output_path = private_path(ROOT / asan_test_output_ref["path"],
                                    "validator-fix2 ASan owning test output")
    asan_output = asan_output_path.read_bytes()
    if (digest(asan_bytes) != asan_ref["sha256"] or
            asan["schema"] != "t64.rsp.validator-fix2-asan-evidence.v1"):
        refuse("validator-fix2 ASan evidence receipt changed")
    asan_configure = object_with_keys(asan["configure"], {"argv", "exit", "output"},
                                     "validator-fix2 ASan configuration")
    asan_configure_output_ref = object_with_keys(asan_configure["output"],
        {"path", "sha256"}, "validator-fix2 ASan configuration output")
    asan_configure_path = private_path(ROOT / asan_configure_output_ref["path"],
                                       "validator-fix2 ASan configuration output")
    asan_configure_output = asan_configure_path.read_bytes()
    if (asan_configure["argv"] != ["cmake", "--preset", "asan"] or
            asan_configure["exit"] != 0 or
            asan_configure_output_ref["path"] != "build/private-rsp-diagnostic/plan10-validator-fix2-asan-configure.log" or
            digest(asan_configure_output) != asan_configure_output_ref["sha256"]):
        refuse("validator-fix2 ASan configuration evidence changed")
    asan_build = object_with_keys(asan["build"], {"argv", "exit", "outputs", "scope"},
                                 "validator-fix2 ASan all-target build")
    expected_asan_build_paths = [
        "build/private-rsp-diagnostic/plan10-json-bound-validator-fix-asan-build.log",
        "build/private-rsp-diagnostic/plan10-validator-fix2-asan-build.log",
    ]
    if (asan_build["argv"] != ["cmake", "--build", "--preset", "asan"] or
            asan_build["exit"] != 0 or asan_build["scope"] != "all-targets" or
            not isinstance(asan_build["outputs"], list) or
            len(asan_build["outputs"]) != len(expected_asan_build_paths)):
        refuse("validator-fix2 ASan all-target build evidence is malformed")
    asan_build_rows = [object_with_keys(row, {"path", "sha256"},
        "validator-fix2 ASan build output") for row in asan_build["outputs"]]
    if [row["path"] for row in asan_build_rows] != expected_asan_build_paths:
        refuse("validator-fix2 ASan all-target build output paths changed")
    for row, marker in zip(asan_build["outputs"], (
            b"Linking C executable runner/t64-rsp-diagnostic",
            b"Linking C executable runner/test_rsp_diag_protocol")):
        row = object_with_keys(row, {"path", "sha256"}, "validator-fix2 ASan build output")
        output_path = private_path(ROOT / row["path"], "validator-fix2 ASan build output")
        output = output_path.read_bytes()
        if digest(output) != row["sha256"] or marker not in output:
            refuse("validator-fix2 ASan all-target output changed or omitted an owning target")
    if (asan_test["argv"] != ["ctest", "--preset", "asan", "-R",
            "^unit\\.rsp-diag-protocol$", "--output-on-failure"] or
            asan_test["environment"] != {
                "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
                "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"} or
            asan_test["exit"] != 0 or
            asan_test_output_ref["path"] != "build/private-rsp-diagnostic/plan10-validator-fix2-asan-ctest.log" or
            digest(asan_output) != asan_test_output_ref["sha256"] or
            b"100% tests passed" not in asan_output):
        refuse("validator-fix2 all-target ASan build or owning regression did not pass")


def git_is_ancestor(ancestor: str, descendant: str) -> bool:
    result = subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor",
                             ancestor, descendant], check=False, capture_output=True,
                            timeout=10)
    if result.returncode not in (0, 1):
        refuse("reviewed source ancestry could not be verified")
    return result.returncode == 0


def verify_rsp_profile_policy(source_text: str, profile_id: str) -> None:
    symbol = "ntsc_cart_4m_openipl3_rsp_functional_profile"
    marker = symbol + " = {"
    start = source_text.find(marker)
    end = source_text.find("\n};", start)
    if start < 0 or end < 0:
        refuse("reviewed RSP functional profile descriptor is absent")
    fields: dict[str, str] = {}
    for line in source_text[start + len(marker):end].splitlines():
        match = re.fullmatch(r"\s*\.([a-z_]+)\s*=\s*(.+?),\s*", line)
        if match is None:
            continue
        if match.group(1) in fields:
            refuse("reviewed RSP functional profile descriptor repeats a field")
        fields[match.group(1)] = match.group(2)
    expected = {
        "profile_id": f'"{RSP_PROFILE_ID}"',
        "console_qualification": '"unqualified-target-software-seed"',
        "region": "T64_REGION_US",
        "media_kind": "T64_MEDIA_CARTRIDGE",
        "rdram_bytes": "T64_BASE_RAM_BYTES",
        "expansion_bytes": "0u",
        "peripheral_flags": "0u",
        "save_kind": "T64_SAVE_NONE",
        "boot_route": '"hle-pif+open-ipl3"',
        "boot_status": '"prepared-rsp-functional-entry-unqualified"',
        "guest_tick_unit": '"model-rsp-cycle"',
        "timing_tier": '"functional-rsp-issue-model-policy-v1"',
        "sp_model_version": '"rsp-functional-issue-dma-divider-integer-v1"',
        "sp_register_policy": '"rsp-issue-status-dma-policy-v1"',
        "dma_visibility_policy": '"rsp-one-beat-per-issue-v1"',
        "descriptor_version": "3u",
        "machine_model_version": "6u",
        "state_serialization_version": "12u",
    }
    if profile_id != RSP_PROFILE_ID or any(fields.get(key) != value for key, value in expected.items()):
        refuse("reviewed RSP profile identity, model, state, or execution policy changed")


def verify_rsp_core_binding_v3(binding: dict[str, object], qualification_sha: str, build_identity: dict[str, object]) -> dict[str, object]:
    expected = {"component", "core_model", "core_source_scope", "core_state_schema", "cpu_protocol_extraction", "current_identity", "historical_reader", "original_identity", "production_core_baseline_revision", "profile_id", "qualification_sha256", "reused_build_evidence", "rsp_adapter_identity", "rsp_runtime_identity", "schema", "source_delta", "status", "supersedes_binding", "verification_scope"}
    binding = object_with_keys(binding, expected, "current RSP core identity binding v3")
    if binding["schema"] != "t64.rsp-core-identity-binding.v3" or binding["status"] != "verified-current-core-artifact" or binding["component"] != "rsp" or binding["qualification_sha256"] != qualification_sha or binding["profile_id"] != build_identity.get("profile_id") or binding["core_model"] != build_identity.get("core_model") or binding["core_state_schema"] != build_identity.get("core_state_schema"):
        refuse("current v3 binding does not descend from the original RSP qualification")
    supersedes = object_with_keys(binding["supersedes_binding"], {"path", "sha256"}, "superseded core binding")
    if supersedes["path"] != "build/private-rsp-diagnostic/current-core-identity-binding-v2.json":
        refuse("current v3 binding does not preserve the immutable v2 identity receipt")
    prior_path = private_path(ROOT / supersedes["path"], "immutable v2 core binding")
    prior = read_json(prior_path)
    if private_digest(prior_path, "immutable v2 core binding") != supersedes["sha256"] or prior.get("schema") != "t64.rsp-core-identity-binding.v2":
        refuse("immutable v2 core binding changed")
    original = object_with_keys(binding["original_identity"], {"core_build_sha256", "executable_sha256", "source_revision"}, "original core identity")
    if original != {"core_build_sha256": build_identity.get("core_build_sha256"), "executable_sha256": build_identity.get("core_identity_executable_sha256"), "source_revision": build_identity.get("core_identity_source_revision")}:
        refuse("v3 binding changed the original qualified core identity")
    historical = object_with_keys(binding["historical_reader"], {"executable_sha256", "path", "role", "source_revision"}, "historical identity reader")
    archived = private_path(ROOT / historical["path"], "archived Plan 03 identity reader")
    if historical["path"] != "build/private-rsp-diagnostic/archived-plan03-identity-reader" or historical["role"] != "historical-plan03-identity-snapshot-only" or private_digest(archived, "archived Plan 03 identity reader") != historical["executable_sha256"] or historical["executable_sha256"] != prior.get("current_identity", {}).get("executable_sha256") or historical["source_revision"] != prior.get("current_identity", {}).get("source_revision"):
        refuse("historical Plan 03 identity reader is mislabeled or changed")

    current = object_with_keys(binding["current_identity"], {"core_build_sha256", "executable_sha256", "path", "source_revision"}, "current core identity")
    current_exe = ROOT / "build/gate/runner/t64-cpu-diagnostic"
    if current["path"] != "build/gate/runner/t64-cpu-diagnostic" or private_digest(current_exe, "current production identity reader") != current["executable_sha256"] or current["core_build_sha256"] != build_identity.get("core_build_sha256"):
        refuse("current production identity reader differs from its v3 binding")
    if not isinstance(current["source_revision"], str) or re.fullmatch(r"[0-9a-f]{40}", current["source_revision"]) is None:
        refuse("current production source annotation is malformed")
    resolved = subprocess.run(["git", "-C", str(ROOT), "rev-parse", current["source_revision"] + "^{commit}"], check=True, capture_output=True, text=True).stdout.strip()
    if resolved != current["source_revision"] or subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", current["source_revision"], "HEAD"], check=False).returncode != 0:
        refuse("current core source annotation is not in the checkout history")
    current_runtime = subprocess.run([str(current_exe), "--compat-v2", "--runtime-identity-json"], check=True, capture_output=True, text=True, timeout=10)
    current_json = json.loads(current_runtime.stdout)
    if current_json.get("core_build_sha256") != current["core_build_sha256"] or current_json.get("source_revision") != current["source_revision"]:
        refuse("current core identity reader no longer reports the v3 binding")
    if binding["production_core_baseline_revision"] != CORE_BASE_REVISION or subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", CORE_BASE_REVISION, current["source_revision"]], check=False).returncode != 0:
        refuse("v3 binding does not descend from the reviewed production core baseline")

    scope = binding["core_source_scope"]
    if not isinstance(scope, list) or not scope:
        refuse("v3 production core source scope is empty")
    seen: set[str] = set()
    for row in scope:
        row = object_with_keys(row, {"path", "sha256"}, "v3 production source row")
        relative = pathlib.PurePosixPath(str(row["path"]))
        if relative.is_absolute() or any(part in ("", ".", "..", ".git") for part in relative.parts) or row["path"] in seen:
            refuse("v3 production source scope path is malformed or duplicated")
        seen.add(row["path"])
        file_path = ROOT.joinpath(*relative.parts)
        if private_digest(file_path, "current production source") != row["sha256"]:
            refuse("current production source differs from the v3 binding")
        baseline = subprocess.run(["git", "-C", str(ROOT), "show", f"{CORE_BASE_REVISION}:{relative.as_posix()}"], check=True, capture_output=True).stdout
        if digest(baseline) != row["sha256"]:
            refuse("current production source differs from the reviewed baseline")

    extraction = object_with_keys(binding["cpu_protocol_extraction"], {"baseline_sha256", "commit", "current_sha256", "equivalence_evidence", "path", "status"}, "CPU protocol extraction")
    if extraction["path"] != "runner/t64_cpu_diag_protocol.c" or extraction["status"] != "tested-canonical-output-preserving-shared-helper-extraction":
        refuse("v3 CPU protocol extraction exception is not the tested shared-helper change")
    source_path = ROOT / extraction["path"]
    if source_path.is_symlink() or not source_path.is_file():
        refuse("current CPU diagnostic protocol source is missing or linked")
    source_bytes = source_path.read_bytes()
    if digest(source_bytes) != extraction["current_sha256"] or digest(source_bytes) == extraction["baseline_sha256"]:
        refuse("CPU protocol extraction source hash is invalid")
    extracted = subprocess.run(["git", "-C", str(ROOT), "show", f"{extraction['commit']}:{extraction['path']}"], check=True, capture_output=True).stdout
    if source_bytes != extracted:
        refuse("CPU protocol source differs from the exact reviewed extraction commit")
    baseline_path = ROOT / "build/private-rsp-diagnostic/plan04/cpu-protocol-source-baseline.sha256"
    baseline_rows = [line.split() for line in baseline_path.read_text(encoding="utf-8").splitlines() if line.endswith(" " + extraction["path"])]
    if len(baseline_rows) != 1 or baseline_rows[0][0] != extraction["baseline_sha256"]:
        refuse("CPU protocol pre-extraction source digest changed")
    evidence = extraction["equivalence_evidence"]
    if not isinstance(evidence, list) or len(evidence) != 2:
        refuse("CPU protocol normal and ASan equivalence evidence is incomplete")
    for row in evidence:
        row = object_with_keys(row, {"path", "sha256"}, "CPU protocol equivalence evidence")
        report_path = private_path(ROOT / row["path"], "CPU protocol equivalence report")
        report = report_path.read_bytes()
        if digest(report) != row["sha256"] or b"3/3 Test" not in report or b"Total Test time" not in report:
            refuse("CPU protocol equivalence report is stale or incomplete")

    adapter = object_with_keys(binding["rsp_adapter_identity"], {"executable_sha256", "path"}, "RSP execution adapter identity")
    adapter_path = ROOT / "build/gate/runner/t64-rsp-diagnostic"
    if adapter["path"] != "build/gate/runner/t64-rsp-diagnostic" or private_digest(adapter_path, "RSP execution adapter") != adapter["executable_sha256"]:
        refuse("RSP execution adapter differs from the v3 binding")
    runtime = object_with_keys(binding["rsp_runtime_identity"], {"core_build_sha256", "core_source_revision", "profile_descriptor_sha256", "profile_id"}, "RSP runtime identity")
    actual_runtime = subprocess.run([str(adapter_path), "--runtime-identity-json"], check=True, capture_output=True, text=True, timeout=10)
    if json.loads(actual_runtime.stdout) != runtime or runtime["core_build_sha256"] != current["core_build_sha256"] or runtime["core_source_revision"] != current["source_revision"] or runtime["profile_id"] != build_identity.get("profile_id"):
        refuse("RSP adapter runtime identity differs from the v3 binding")

    reused = binding["reused_build_evidence"]
    expected_evidence = {"build/private-rsp-diagnostic/plan04/task1-postbuild.log", "build/private-rsp-diagnostic/plan04/task1-postbuild-asan.log", "build/private-rsp-diagnostic/plan04/task2-adapter-build-03.log"}
    if not isinstance(reused, list) or {row.get("path") for row in reused if isinstance(row, dict)} != expected_evidence:
        refuse("v3 binding does not preserve the bounded normal, ASan, and adapter build evidence")
    for row in reused:
        row = object_with_keys(row, {"path", "sha256"}, "reused build evidence")
        evidence_path = private_path(ROOT / row["path"], "reused current build evidence")
        if private_digest(evidence_path, "reused current build evidence") != row["sha256"]:
            refuse("reused current build evidence changed")
    return current_json


def verify_rsp_core_binding_v4(binding: dict[str, object], qualification_sha: str, build_identity: dict[str, object]) -> dict[str, object]:
    expected = {
        "authorized_for_future_guest_use", "component", "core_model",
        "core_source_scope", "core_state_schema", "current_identity",
        "current_normal_build", "guest_execution_performed",
        "historical_batch_validation", "historical_first_use", "model6_ctc1_fix",
        "native_evidence", "original_identity", "production_core_baseline_revision",
        "profile_id", "qualified_build_sha256", "qualified_rom_sha256",
        "qualification_sha256", "rsp_adapter_identity", "rsp_runtime_identity",
        "schema", "status", "supersedes_binding_v3", "supersedes_interim_context",
        "verification_scope", "runner_safety_fix",
    }
    binding = object_with_keys(binding, expected, "current RSP pre-use binding v4")
    helper_spec = importlib.util.spec_from_file_location("rsp_runner_safety", ROOT / "scripts/prepare-rsp-diagnostic.py")
    helper = importlib.util.module_from_spec(helper_spec)
    helper_spec.loader.exec_module(helper)
    if binding["runner_safety_fix"] != helper.runner_safety_fix_record():
        refuse("current runner safety binding changed")
    safety_revision = binding["runner_safety_fix"]["commit"]
    if (binding["schema"] != "t64.rsp-core-identity-binding.v4" or
            binding["status"] != "preuse-verified-current-core-for-qualified-rsp-image" or
            binding["component"] != "rsp" or
            binding["qualification_sha256"] != qualification_sha or
            binding["profile_id"] != build_identity.get("profile_id") or
            binding["core_model"] != build_identity.get("core_model") or
            binding["core_state_schema"] != build_identity.get("core_state_schema") or
            binding["guest_execution_performed"] is not False or
            binding["authorized_for_future_guest_use"] is not True or
            binding["production_core_baseline_revision"] != CORE_BASE_REVISION):
        refuse("current v4 RSP binding does not match the original qualification and approved profile")

    supersedes_v3 = object_with_keys(binding["supersedes_binding_v3"], {"path", "sha256"}, "historical v3 execution binding")
    supersedes_interim = object_with_keys(binding["supersedes_interim_context"], {"path", "sha256"}, "interim future context")
    if (supersedes_v3["path"] != "build/private-rsp-diagnostic/current-core-identity-binding-v3.json" or
            supersedes_interim["path"] != "build/private-rsp-diagnostic/current-rsp-future-execution-context-v1.json"):
        refuse("current v4 binding does not preserve both prior execution contexts")
    v3_path = private_path(ROOT / supersedes_v3["path"], "immutable v3 RSP execution binding")
    interim_path = private_path(ROOT / supersedes_interim["path"], "immutable interim future context")
    v3 = read_json(v3_path)
    interim = read_json(interim_path)
    if (private_digest(v3_path, "immutable v3 RSP execution binding") != supersedes_v3["sha256"] or
            v3.get("schema") != "t64.rsp-core-identity-binding.v3" or
            private_digest(interim_path, "immutable interim future context") != supersedes_interim["sha256"] or
            interim.get("schema") != "t64.rsp-future-execution-context.v1" or
            interim.get("status") != "future-requalification-required"):
        refuse("historical v3 or interim context changed")
    v2_path = private_path(ROOT / "build/private-rsp-diagnostic/current-core-identity-binding-v2.json", "immutable v2 RSP identity binding")
    v2 = read_json(v2_path)
    if v2.get("schema") != "t64.rsp-core-identity-binding.v2":
        refuse("immutable v2 identity binding is malformed")
    if binding["original_identity"] != v2.get("current_identity"):
        refuse("v4 binding changed the exact original qualified core identity")

    qualified_build_path = private_path(ROOT / "build/private-rsp-diagnostic/qualified-build.json", "original qualified RSP build")
    qualified_build = read_json(qualified_build_path)
    if private_digest(qualified_build_path, "original qualified RSP build") != binding["qualified_build_sha256"]:
        refuse("original qualified RSP build receipt changed")
    rom_record = qualified_build.get("artifacts", {}).get("rom", {})
    if (qualified_build.get("qualification_sha256") != qualification_sha or
            qualified_build.get("guest_rom_executed") is not False or
            rom_record.get("sha256") != binding["qualified_rom_sha256"]):
        refuse("current binding does not preserve the original unexecuted qualified ROM")
    rom_path = private_path(ROOT / str(rom_record.get("path", "")), "original qualified RSP ROM")
    if private_digest(rom_path, "original qualified RSP ROM") != binding["qualified_rom_sha256"]:
        refuse("original qualified RSP ROM bytes changed")
    if build_identity.get("core_build_sha256") == binding["rsp_runtime_identity"].get("core_build_sha256"):
        refuse("v4 binding is not distinct from the original qualified core build")

    fix = object_with_keys(binding["model6_ctc1_fix"], {
        "baseline_sha256", "commit", "current_sha256", "path",
        "regression_path", "regression_sha256", "subject",
    }, "model6 CTC1 dispatch fix")
    fix_commit = subprocess.run(["git", "-C", str(ROOT), "rev-parse", fix["commit"] + "^{commit}"], check=True, capture_output=True, text=True).stdout.strip()
    subject = subprocess.run(["git", "-C", str(ROOT), "show", "-s", "--format=%s", fix_commit], check=True, capture_output=True, text=True).stdout.strip()
    changed = subprocess.run(["git", "-C", str(ROOT), "diff-tree", "--no-commit-id", "--name-only", "-r", fix_commit], check=True, capture_output=True, text=True).stdout.splitlines()
    if (subject != "fix(05-04): dispatch model6 CTC1 through VR4300 core" or
            fix["subject"] != subject or
            set(changed) != {"core/t64_internal.h", "core/tests/test_cop1.c"} or
            fix["path"] != "core/t64_internal.h" or
            fix["regression_path"] != "core/tests/test_cop1.c"):
        refuse("current core deviation is not the bounded model6 CTC1 predicate fix")
    baseline_core = subprocess.run(["git", "-C", str(ROOT), "show", f"{CORE_BASE_REVISION}:{fix['path']}"], check=True, capture_output=True).stdout
    fixed_core = subprocess.run(["git", "-C", str(ROOT), "show", f"{fix_commit}:{fix['path']}"], check=True, capture_output=True).stdout
    current_core = project_path(ROOT / fix["path"], "current model6 CTC1 predicate").read_bytes()
    regression = project_path(ROOT / fix["regression_path"], "current model6 CTC1 regression").read_bytes()
    if (digest(baseline_core) != fix["baseline_sha256"] or
            digest(fixed_core) != fix["current_sha256"] or current_core != fixed_core or
            digest(regression) != fix["regression_sha256"]):
        refuse("model6 CTC1 fix source or regression differs from its tested commit")

    current = object_with_keys(binding["current_identity"], {"core_build_sha256", "executable_sha256", "path", "source_revision"}, "current production core identity")
    current_reader_path = project_path(ROOT / current["path"], "current production identity reader", 512 * 1024 * 1024)
    if (current["path"] != "build/gate/runner/t64-cpu-diagnostic" or
            current["core_build_sha256"] != binding["rsp_runtime_identity"].get("core_build_sha256") or
            current["executable_sha256"] != digest(current_reader_path.read_bytes()) or
            not isinstance(current["source_revision"], str) or re.fullmatch(r"[0-9a-f]{40}", current["source_revision"]) is None):
        refuse("current production identity reader differs from the v4 binding")
    source_revision = subprocess.run(["git", "-C", str(ROOT), "rev-parse", current["source_revision"] + "^{commit}"], check=True, capture_output=True, text=True).stdout.strip()
    if source_revision != current["source_revision"] or subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", current["source_revision"], "HEAD"], check=False).returncode != 0:
        refuse("current source annotation is not in checkout history")
    if subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", CORE_BASE_REVISION, current["source_revision"]], check=False).returncode != 0:
        refuse("current source annotation does not descend from the reviewed core baseline")

    rows = binding["core_source_scope"]
    if not isinstance(rows, list) or not rows:
        refuse("v4 current core source scope is empty")
    prior_rows = v3.get("core_source_scope")
    if not isinstance(prior_rows, list) or not prior_rows:
        refuse("historical v3 production source scope is empty")
    prior = {row.get("path"): row.get("sha256") for row in prior_rows if isinstance(row, dict)}
    if len(prior) != len(prior_rows):
        refuse("historical v3 production source scope is malformed")
    extraction = object_with_keys(v3.get("cpu_protocol_extraction"), {"baseline_sha256", "commit", "current_sha256", "equivalence_evidence", "path", "status"}, "tested CPU protocol extraction")
    if extraction["path"] != "runner/t64_cpu_diag_protocol.c":
        refuse("shared CPU protocol extraction path changed")
    seen: set[str] = set()
    for row in rows:
        row = object_with_keys(row, {"path", "sha256"}, "v4 current core source row")
        relative = pathlib.PurePosixPath(str(row["path"]))
        if relative.is_absolute() or any(part in ("", ".", "..", ".git") for part in relative.parts) or row["path"] in seen:
            refuse("v4 current core source path is malformed or duplicated")
        seen.add(row["path"])
        current_path = ROOT.joinpath(*relative.parts)
        source_bytes = project_path(current_path, "v4 current core source").read_bytes()
        if digest(source_bytes) != row["sha256"]:
            refuse("current core source differs from its v4 binding")
        committed = subprocess.run(["git", "-C", str(ROOT), "show", f"{current['source_revision']}:{relative.as_posix()}"], check=True, capture_output=True).stdout
        if source_bytes != committed:
            refuse("current core source differs from its annotated build revision")
        if (row["path"] in prior and
                row["path"] not in (fix["path"], fix["regression_path"], extraction["path"], "runner/t64_cpu_diagnostic.c") and
                row["sha256"] != prior[row["path"]]):
            refuse("production core changed beyond the single CTC1 predicate fix")
        if row["path"] == "runner/t64_cpu_diagnostic.c":
            reviewed = subprocess.run(["git", "-C", str(ROOT), "show", safety_revision + ":" + row["path"]], check=True, capture_output=True).stdout
            if source_bytes != reviewed:
                refuse("CPU runner differs from its reviewed safety fix")
    if (fix["path"] not in seen or fix["regression_path"] not in seen or
            extraction["path"] not in seen or not set(prior).issubset(seen)):
        refuse("v4 source scope omits the CTC1 fix, regression, or shared protocol extraction")
    extraction_bytes = subprocess.run(
        ["git", "-C", str(ROOT), "show", f"{extraction['commit']}^{{commit}}:{extraction['path']}"],
        check=True, capture_output=True).stdout
    if project_path(ROOT / extraction["path"], "current CPU protocol source").read_bytes() != extraction_bytes:
        refuse("CPU protocol source differs from its exact shared-helper extraction commit")

    adapter = object_with_keys(binding["rsp_adapter_identity"], {"executable_sha256", "path"}, "current RSP adapter identity")
    adapter_path = project_path(ROOT / "build/gate/runner/t64-rsp-diagnostic", "current RSP adapter", 512 * 1024 * 1024)
    if adapter["path"] != adapter_path.relative_to(ROOT).as_posix() or digest(adapter_path.read_bytes()) != adapter["executable_sha256"]:
        refuse("current RSP adapter bytes differ from the v4 binding")
    runtime = object_with_keys(binding["rsp_runtime_identity"], {"core_build_sha256", "core_source_revision", "profile_descriptor_sha256", "profile_id"}, "current RSP runtime identity")
    if runtime["core_build_sha256"] != current["core_build_sha256"] or runtime["core_source_revision"] != current["source_revision"] or runtime["profile_id"] != build_identity.get("profile_id") or runtime["profile_descriptor_sha256"] != v3.get("rsp_runtime_identity", {}).get("profile_descriptor_sha256"):
        refuse("current RSP runtime identity differs from the pre-use binding")
    current_runtime = subprocess.run([str(adapter_path), "--runtime-identity-json"], check=True, capture_output=True, text=True, timeout=10)
    current_cpu_runtime = subprocess.run([str(ROOT / current["path"]), "--compat-v2", "--runtime-identity-json"], check=True, capture_output=True, text=True, timeout=10)
    if json.loads(current_runtime.stdout) != runtime or json.loads(current_cpu_runtime.stdout).get("core_build_sha256") != runtime["core_build_sha256"] or json.loads(current_cpu_runtime.stdout).get("source_revision") != runtime["core_source_revision"]:
        refuse("current core and RSP runtime identity readers disagree with v4 binding")

    evidence = binding["native_evidence"]
    expected_evidence = {
        "build/private-rsp-diagnostic/plan04/task2-core-regressions-gate.log",
        "build/private-rsp-diagnostic/plan04/task2-core-regressions-asan.log",
        "build/private-rsp-diagnostic/plan04/task2-terminal-gate-tests.log",
        "build/private-rsp-diagnostic/plan04/task2-terminal-asan-tests.log",
        "build/private-rsp-diagnostic/plan04/task2-legacycompat-gate-test.log",
        "build/private-rsp-diagnostic/plan04/task2-legacycompat-asan-test.log",
        "build/private-rsp-diagnostic/plan04/task3-gate-inventory.log",
        "build/private-rsp-diagnostic/plan04/task3-asan-inventory.log",
    }
    if not isinstance(evidence, list) or {row.get("path") for row in evidence if isinstance(row, dict)} != expected_evidence:
        refuse("v4 pre-use native evidence set is incomplete")
    for row in evidence:
        row = object_with_keys(row, {"path", "sha256"}, "v4 pre-use native evidence")
        evidence_path = private_path(ROOT / row["path"], "v4 pre-use native evidence")
        content = evidence_path.read_bytes()
        if digest(content) != row["sha256"] or (b"100% tests passed" not in content and b"test inventory PASS" not in content):
            refuse("v4 pre-use native evidence is stale or nonpassing")
    normal_build = object_with_keys(binding["current_normal_build"], {"path", "sha256"}, "v4 all-target build evidence")
    if normal_build["path"] != "build/private-rsp-diagnostic/runner-safety-review/current-gate-build.log":
        refuse("v4 all-target build evidence path changed")
    normal_build_bytes = private_path(ROOT / normal_build["path"], "v4 all-target build log").read_bytes()
    if digest(normal_build_bytes) != normal_build["sha256"] or b"t64-rsp-diagnostic" not in normal_build_bytes:
        refuse("v4 all-target normal build evidence is stale")
    validator = object_with_keys(binding["historical_batch_validation"], {"classification", "members", "outcome", "path", "sha256"}, "current historical batch validation")
    validator_path = private_path(ROOT / validator["path"], "current historical batch validator report")
    validator_bytes = validator_path.read_bytes()
    if validator["path"] != "build/private-rsp-diagnostic/plan04/task2-current-validator-original-batch-absolute.log" or digest(validator_bytes) != validator["sha256"] or b"classification=validated outcome=refused members=1" not in validator_bytes or (validator["classification"], validator["outcome"], validator["members"]) != ("validated", "refused", 1):
        refuse("current C validator no longer retains the historical refused batch")

    history = object_with_keys(binding["historical_first_use"], {"adapter_exit", "classification", "historical_files", "original_launcher_failure_preserved", "outcome", "status"}, "v4 historical first-use evidence")
    if (history["status"] != "selector-bound-refusal-authenticated" or history["outcome"] != "refused" or history["classification"] != "refused" or history["adapter_exit"] != 2 or history["original_launcher_failure_preserved"] is not True):
        refuse("v4 binding rewrites the original actual refusal or launcher failure")
    historical = history["historical_files"]
    if not isinstance(historical, list) or not historical:
        refuse("v4 historical first-use evidence is empty")
    for row in historical:
        row = object_with_keys(row, {"path", "sha256"}, "v4 immutable historical evidence")
        path = private_path(ROOT / row["path"], "v4 immutable historical evidence")
        if private_digest(path, "v4 immutable historical evidence") != row["sha256"]:
            refuse("v4 historical refusal or binding artifact changed")
    return json.loads(current_runtime.stdout)


def verify_rsp_core_binding_v5(binding: dict[str, object], qualification_sha: str,
                               build_identity: dict[str, object]) -> dict[str, object]:
    expected = {
        "authorized_for_future_guest_use", "component", "core_model",
        "core_source_scope", "core_state_schema", "current_identity",
        "current_normal_evidence", "guest_execution_performed",
        "historical_first_use", "identity_attempt_dir", "identity_command_receipts",
        "original_identity", "preuse_source_closure", "profile_id",
        "qualified_build_sha256", "qualified_rom_sha256",
        "qualification_sha256", "reviewed_source_delta",
        "rsp_adapter_identity", "rsp_runtime_identity", "schema", "status",
        "supersedes_binding_v4", "verification_scope",
    }
    if isinstance(binding, dict) and "adapter_refresh" in binding:
        expected.add("adapter_refresh")
    binding = object_with_keys(binding, expected, "current RSP pre-use binding v5")
    if (binding["schema"] != "t64.rsp-core-identity-binding.v5" or
            binding["status"] != "preuse-verified-current-core-for-qualified-rsp-image" or
            binding["component"] != "rsp" or
            binding["qualification_sha256"] != qualification_sha or
            binding["profile_id"] != build_identity.get("profile_id") or
            binding["core_model"] != build_identity.get("core_model") or
            binding["core_state_schema"] != build_identity.get("core_state_schema") or
            binding["guest_execution_performed"] is not False or
            binding["authorized_for_future_guest_use"] is not True):
        refuse("current v5 RSP binding does not match the sealed qualification")

    prior_ref = object_with_keys(binding["supersedes_binding_v4"], {"path", "sha256"},
                                 "historical v4 RSP binding")
    if prior_ref["path"] != "build/private-rsp-diagnostic/current-core-identity-binding-v4.json":
        refuse("current v5 binding does not preserve the historical v4 receipt")
    prior_path = private_path(ROOT / prior_ref["path"], "historical v4 RSP binding")
    prior_bytes = prior_path.read_bytes()
    if digest(prior_bytes) != prior_ref["sha256"]:
        refuse("historical v4 RSP binding changed")
    prior = read_json(prior_path)
    if (prior.get("schema") != "t64.rsp-core-identity-binding.v4" or
            prior.get("status") != "preuse-verified-current-core-for-qualified-rsp-image" or
            prior.get("component") != "rsp" or
            prior.get("qualification_sha256") != qualification_sha or
            prior.get("original_identity") != binding["original_identity"]):
        refuse("historical v4 RSP binding is not an intact predecessor")

    qualification_path = private_path(ROOT / "build/private-rsp-diagnostic/qualification.json",
                                      "original RSP qualification")
    if private_digest(qualification_path, "original RSP qualification") != qualification_sha:
        refuse("current v5 binding changed the original qualification")
    v2_path = private_path(ROOT / "build/private-rsp-diagnostic/current-core-identity-binding-v2.json",
                           "immutable v2 RSP identity binding")
    v2 = read_json(v2_path)
    if (v2.get("schema") != "t64.rsp-core-identity-binding.v2" or
            binding["original_identity"] != v2.get("current_identity")):
        refuse("current v5 binding changed the immutable v2 core identity")

    qualified_build_path = private_path(ROOT / "build/private-rsp-diagnostic/qualified-build.json",
                                        "original qualified RSP build")
    qualified_build = read_json(qualified_build_path)
    if (private_digest(qualified_build_path, "original qualified RSP build") !=
            binding["qualified_build_sha256"] or
            qualified_build.get("qualification_sha256") != qualification_sha or
            qualified_build.get("guest_rom_executed") is not False):
        refuse("current v5 binding changed the original unexecuted build")
    rom_record = qualified_build.get("artifacts", {}).get("rom", {})
    if rom_record.get("sha256") != binding["qualified_rom_sha256"]:
        refuse("current v5 binding changed the qualified ROM identity")
    rom_path = private_path(ROOT / str(rom_record.get("path", "")), "original qualified RSP ROM")
    if private_digest(rom_path, "original qualified RSP ROM") != binding["qualified_rom_sha256"]:
        refuse("original qualified RSP ROM bytes changed")

    v4_identity = object_with_keys(prior.get("current_identity"),
                                   {"core_build_sha256", "executable_sha256", "path", "source_revision"},
                                   "historical v4 core identity")
    baseline = v4_identity["source_revision"]
    source_delta = object_with_keys(binding["reviewed_source_delta"],
                                    {"baseline_revision", "changed_paths", "tested_source_revision"},
                                    "reviewed RSP source delta")
    if source_delta["baseline_revision"] != baseline or source_delta["tested_source_revision"] != PLAN10_TESTED_SOURCE_REVISION:
        refuse("current v5 binding does not pin the reviewed Plan10 source interval")
    changed_paths = reviewed_plan10_source_delta(baseline, source_delta["tested_source_revision"])
    if source_delta["changed_paths"] != changed_paths:
        refuse("current v5 source delta omits or adds reviewed source changes")

    for row in prior.get("core_source_scope", []):
        if not isinstance(row, dict) or set(row) != {"path", "sha256"}:
            refuse("historical v4 source scope is malformed")
        old_bytes = subprocess.run(["git", "-C", str(ROOT), "show",
                                    f"{baseline}:{row['path']}"], check=True,
                                   capture_output=True, timeout=10).stdout
        if digest(old_bytes) != row["sha256"]:
            refuse("historical v4 source row does not match its reviewed source revision")
    prior_paths = {row["path"] for row in prior.get("core_source_scope", [])}
    if len(prior_paths) != len(prior.get("core_source_scope", [])):
        refuse("historical v4 source scope contains duplicate paths")
    expected_paths = prior_paths | set(changed_paths)
    if "adapter_refresh" in binding:
        expected_paths -= {
            "runner/t64_rsp_diagnostic.c",
            "runner/t64_rsp_diag_protocol.c",
            "runner/t64_rsp_diag_protocol.h",
            "runner/tests/test_rsp_diag_protocol.c",
        }
    rows = validate_scoped_source_rows(binding["core_source_scope"], expected_paths)

    tested = source_delta["tested_source_revision"]
    if not git_is_ancestor(tested, "HEAD"):
        refuse("reviewed source revision is not in the current checkout history")
    identity = object_with_keys(binding["current_identity"],
                                {"core_build_sha256", "executable_sha256", "path", "source_revision"},
                                "current production core identity")
    source_annotation = identity["source_revision"]
    if (identity["path"] != "build/gate/runner/t64-cpu-diagnostic" or
            not isinstance(source_annotation, str) or re.fullmatch(r"[0-9a-f]{40}", source_annotation) is None or
            not isinstance(identity["core_build_sha256"], str) or
            re.fullmatch(r"[0-9a-f]{64}", identity["core_build_sha256"]) is None):
        refuse("current v5 core artifact identity is malformed")
    if "adapter_refresh" in binding:
        verify_v5_adapter_refresh(binding, source_annotation)
    if not git_is_ancestor(source_annotation, "HEAD"):
        refuse("current core source annotation is not in checkout history")
    adapter_refresh_enabled = "adapter_refresh" in binding
    allowed_adapter_paths = V5_ADAPTER_SOURCE_PATHS if adapter_refresh_enabled else frozenset()
    annotation_paths = set(git_names("diff", "--name-only", tested, source_annotation,
                                    "--", *PRODUCTION_SOURCE_PREFIXES))
    verify_v5_adapter_annotation_delta(annotation_paths, adapter_refresh_enabled)
    later_committed_paths = set(git_names("diff", "--name-only", tested, "HEAD",
                                         "--", *PRODUCTION_SOURCE_PREFIXES))
    worktree_status = subprocess.run(
        ["git", "-C", str(ROOT), "status", "--porcelain=v1", "--untracked-files=all",
         "--", *PRODUCTION_SOURCE_PREFIXES], check=True, capture_output=True,
        text=True, timeout=10).stdout.splitlines()
    worktree_paths = {line[3:] for line in worktree_status if len(line) >= 4}
    verify_v5_adapter_annotation_delta(later_committed_paths | worktree_paths,
                                       adapter_refresh_enabled)
    for relative, expected_sha in rows.items():
        source = project_path(ROOT / relative, "current reviewed RSP source").read_bytes()
        tested_bytes = subprocess.run(["git", "-C", str(ROOT), "show", f"{tested}:{relative}"],
                                      check=True, capture_output=True, timeout=10).stdout
        annotated_bytes = subprocess.run(["git", "-C", str(ROOT), "show", f"{source_annotation}:{relative}"],
                                         check=True, capture_output=True, timeout=10).stdout
        if source != tested_bytes or source != annotated_bytes or digest(source) != expected_sha:
            refuse("current RSP production source differs from the pinned reviewed bytes")

    evidence_expected = {
        "build/gate/05-09-final-normal-configure.log",
        "build/gate/05-09-final-normal-build.log",
        "build/gate/05-09-final-gate.log",
        "build/gate/05-09-review-final-ctest.log",
    }
    evidence = binding["current_normal_evidence"]
    if not isinstance(evidence, list) or {row.get("path") for row in evidence if isinstance(row, dict)} != evidence_expected:
        refuse("current v5 normal build evidence is incomplete")
    for row in evidence:
        row = object_with_keys(row, {"path", "sha256"}, "current v5 normal build evidence")
        path = project_path(ROOT / row["path"], "current v5 normal build evidence", 16 * 1024 * 1024)
        content = path.read_bytes()
        if digest(content) != row["sha256"]:
            refuse("current v5 normal build evidence changed")
        if row["path"].endswith("final-gate.log") and b"100% tests passed" not in content:
            refuse("current v5 final native gate evidence is incomplete")
        if row["path"].endswith("review-final-ctest.log") and b"100% tests passed" not in content:
            refuse("current v5 owning native regression evidence is incomplete")

    history = object_with_keys(binding["historical_first_use"],
                               {"adapter_exit", "classification", "historical_files",
                                "original_launcher_failure_preserved", "outcome", "status"},
                               "preserved historical RSP first use")
    if (history != prior.get("historical_first_use") or history["outcome"] != "refused" or
            history["classification"] != "refused" or history["original_launcher_failure_preserved"] is not True):
        refuse("v5 binding did not preserve the original refused first use")
    for row in history["historical_files"]:
        row = object_with_keys(row, {"path", "sha256"}, "historical RSP first-use file")
        path = private_path(ROOT / row["path"], "historical RSP first-use file")
        if private_digest(path, "historical RSP first-use file") != row["sha256"]:
            refuse("historical RSP first-use evidence changed")

    runtime = object_with_keys(binding["rsp_runtime_identity"],
                               {"core_build_sha256", "core_source_revision",
                                "profile_descriptor_sha256", "profile_id"},
                               "current RSP runtime identity")
    if (runtime["core_build_sha256"] != identity["core_build_sha256"] or
            runtime["core_source_revision"] != source_annotation or
            runtime["profile_id"] != build_identity.get("profile_id") or
            not isinstance(runtime["profile_descriptor_sha256"], str) or
            re.fullmatch(r"[0-9a-f]{64}", runtime["profile_descriptor_sha256"]) is None or
            identity["core_build_sha256"] == build_identity.get("core_build_sha256")):
        refuse("current RSP runtime identity differs from the pre-use binding")

    core_identity_json = verify_v5_identity_receipts(
        binding, read_json(qualification_path)["closure"]["sha256"],
        identity, runtime, source_annotation)
    adapter_record = object_with_keys(binding["rsp_adapter_identity"],
                                      {"executable_sha256", "path"}, "current RSP adapter")
    adapter_path = project_path(ROOT / "build/gate/runner/t64-rsp-diagnostic",
                                "current RSP adapter", 512 * 1024 * 1024)
    core_path = project_path(ROOT / identity["path"], "current core identity reader",
                             512 * 1024 * 1024)

    # Executable hashes and all source/input receipts have been checked above.
    current_runtime = subprocess.run([str(adapter_path), "--runtime-identity-json"],
                                     cwd=ROOT, stdin=subprocess.DEVNULL,
                                     capture_output=True, timeout=10, check=True)
    current_cpu = subprocess.run([str(core_path), "--compat-v2", "--runtime-identity-json"],
                                 cwd=ROOT, stdin=subprocess.DEVNULL,
                                 capture_output=True, timeout=10, check=True)
    if len(current_runtime.stdout) > 4096 or len(current_runtime.stderr) > 4096 or len(current_cpu.stdout) > 4096 or len(current_cpu.stderr) > 4096:
        refuse("current identity output exceeds its capture limit")
    if (json.loads(current_runtime.stdout) != runtime or
            json.loads(current_cpu.stdout).get("core_build_sha256") != runtime["core_build_sha256"] or
            json.loads(current_cpu.stdout).get("source_revision") != source_annotation or
            core_identity_json is None or
            core_identity_json.get("core_build_sha256") != runtime["core_build_sha256"] or
            core_identity_json.get("source_revision") != source_annotation or
            digest(adapter_path.read_bytes()) != adapter_record["executable_sha256"] or
            digest(core_path.read_bytes()) != identity["executable_sha256"]):
        refuse("current RSP and CPU identity readers disagree with the verified v5 binding")
    return runtime


def verify_rebuilt_core_binding(binding_path: pathlib.Path, qualification_sha: str, build_identity: dict[str, object]) -> dict[str, object]:
    binding_path = private_path(binding_path, "current core identity binding")
    if stat.S_IMODE(binding_path.stat().st_mode) != 0o600:
        refuse("current core identity binding is not owner-only")
    binding = read_json(binding_path)
    if isinstance(binding, dict) and binding.get("schema") == "t64.rsp-core-identity-binding.v5":
        return verify_rsp_core_binding_v5(binding, qualification_sha, build_identity)
    if isinstance(binding, dict) and binding.get("schema") == "t64.rsp-core-identity-binding.v4":
        return verify_rsp_core_binding_v4(binding, qualification_sha, build_identity)
    if isinstance(binding, dict) and binding.get("schema") == "t64.rsp-core-identity-binding.v3":
        return verify_rsp_core_binding_v3(binding, qualification_sha, build_identity)
    binding = object_with_keys(binding, {"cmake_cache_sha256", "command_receipt_sha256", "commands", "component", "core_model", "core_source_scope", "core_state_schema", "current_identity", "evidence_dir", "original_identity", "production_core_baseline_revision", "profile_id", "qualification_sha256", "schema", "status", "supersedes_binding", "verification_scope"}, "current core identity binding")
    if binding["schema"] != "t64.rsp-core-identity-binding.v2" or binding["status"] != "verified-current-core-artifact" or binding["component"] != "rsp" or binding["qualification_sha256"] != qualification_sha or binding["profile_id"] != build_identity.get("profile_id") or binding["core_model"] != build_identity.get("core_model") or binding["core_state_schema"] != build_identity.get("core_state_schema"):
        refuse("current core identity binding does not descend from the original RSP qualification")
    supersedes = object_with_keys(binding["supersedes_binding"], {"path", "sha256"}, "superseded core identity binding")
    superseded_path = private_path(ROOT / supersedes["path"], "superseded core identity binding")
    if supersedes["path"] != "build/private-rsp-diagnostic/current-core-identity-binding.json" or private_digest(superseded_path, "superseded core identity binding") != supersedes["sha256"]:
        refuse("current core binding does not preserve the original supplemental binding")
    original = object_with_keys(binding["original_identity"], {"core_build_sha256", "executable_sha256", "source_revision"}, "original core identity")
    if original != {"core_build_sha256": build_identity.get("core_build_sha256"), "executable_sha256": build_identity.get("core_identity_executable_sha256"), "source_revision": build_identity.get("core_identity_source_revision")}:
        refuse("current binding changed the original pre-use core identity")
    current = object_with_keys(binding["current_identity"], {"core_build_sha256", "executable_sha256", "path", "source_revision"}, "rebuilt core identity")
    expected_binary = ROOT / "build/gate/runner/t64-cpu-diagnostic"
    if current["path"] != "build/gate/runner/t64-cpu-diagnostic" or current["core_build_sha256"] != build_identity.get("core_build_sha256") or private_digest(expected_binary, "rebuilt production identity reader") != current["executable_sha256"]:
        refuse("rebuilt production identity reader differs from the supplemental binding")
    if not isinstance(current["source_revision"], str) or re.fullmatch(r"[0-9a-f]{40}", current["source_revision"]) is None:
        refuse("rebuilt production identity reader source revision is malformed")
    source_revision = subprocess.run(["git", "-C", str(ROOT), "rev-parse", current["source_revision"] + "^{commit}"], check=True, capture_output=True, text=True).stdout.strip()
    if source_revision != current["source_revision"] or subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", current["source_revision"], "HEAD"], check=False).returncode != 0:
        refuse("rebuilt core source revision is not in the current history")
    if binding["production_core_baseline_revision"] != CORE_BASE_REVISION or subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", CORE_BASE_REVISION, current["source_revision"]], check=False).returncode != 0:
        refuse("rebuilt core binding does not descend from the reviewed production baseline")

    scope = binding["core_source_scope"]
    if not isinstance(scope, list) or not scope:
        refuse("rebuilt core source verification scope is empty")
    seen_paths: set[str] = set()
    for row in scope:
        row = object_with_keys(row, {"path", "sha256"}, "rebuilt core source scope row")
        relative = pathlib.PurePosixPath(str(row["path"]))
        if relative.is_absolute() or any(part in ("", ".", "..", ".git") for part in relative.parts) or row["path"] in seen_paths:
            refuse("rebuilt core source scope path is malformed or duplicated")
        seen_paths.add(row["path"])
        file_path = ROOT.joinpath(*relative.parts)
        if private_digest(file_path, "rebuilt core source") != row["sha256"]:
            refuse("rebuilt core source bytes changed after the all-target build")
        baseline = subprocess.run(["git", "-C", str(ROOT), "show", f"{CORE_BASE_REVISION}:{relative.as_posix()}"], check=True, capture_output=True).stdout
        if digest(baseline) != row["sha256"]:
            refuse("rebuilt core source scope differs from the reviewed production baseline")

    evidence_dir = ROOT / str(binding["evidence_dir"])
    try:
        evidence_dir.relative_to(ROOT / "build/private-rsp-diagnostic")
    except ValueError:
        refuse("core binding command evidence escapes the private RSP root")
    if evidence_dir.is_symlink() or not evidence_dir.is_dir() or stat.S_IMODE(evidence_dir.stat().st_mode) != 0o700:
        refuse("core binding command evidence directory is missing or not owner-only")
    commands = object_with_keys(binding["commands"], {"all_targets_build", "configure", "gate_inventory"}, "core binding commands")
    expected_argv = {"configure": ["cmake", "--preset", "gate"], "all_targets_build": ["cmake", "--build", "--preset", "gate"], "gate_inventory": ["sh", "scripts/check-test-inventory.sh", "--preset", "gate"]}
    receipt_hashes = binding["command_receipt_sha256"]
    if not isinstance(receipt_hashes, dict) or set(receipt_hashes) != set(expected_argv):
        refuse("core binding command receipt hashes are incomplete")
    labels = {"configure": "cmake-configure", "all_targets_build": "cmake-build-all", "gate_inventory": "gate-inventory"}
    for name, argv in expected_argv.items():
        command_path = evidence_dir / f"{labels[name]}.command.json"
        if private_digest(command_path, "core binding command receipt") != receipt_hashes[name]:
            refuse("core binding command receipt changed")
        command = read_json(command_path)
        if command != commands[name] or command.get("argv") != argv or command.get("exit") != 0 or command.get("timed_out") is not False or command.get("process_group_empty") is not True:
            refuse(f"core binding command did not complete successfully: {name}")
        for field in ("stdout", "stderr"):
            log = evidence_dir / str(command[field])
            if private_digest(log, "core binding build log") != command.get(field + "_sha256"):
                refuse("core binding build log changed")
    inventory_log = (evidence_dir / commands["gate_inventory"]["stdout"]).read_text(encoding="utf-8")
    if "test inventory PASS preset=gate count=115" not in inventory_log:
        refuse("core binding does not include the complete gate inventory check")
    if private_digest(ROOT / "build/gate/CMakeCache.txt", "current gate CMake cache") != binding["cmake_cache_sha256"]:
        refuse("current gate build configuration differs from the verified identity binding")
    runtime = subprocess.run([str(expected_binary), "--compat-v2", "--runtime-identity-json"], check=True, capture_output=True, text=True)
    identity_json = json.loads(runtime.stdout)
    if identity_json.get("core_build_sha256") != current["core_build_sha256"] or identity_json.get("source_revision") != current["source_revision"]:
        refuse("rebuilt production identity reader no longer reports its bound artifact identity")
    return identity_json


def verify_current_core_identity(build_identity: dict[str, object], qualification_sha: str,
                                 binding_version: str = "v5") -> None:
    identity_exe = ROOT / "build/gate/runner/t64-cpu-diagnostic"
    current_sha = private_digest(identity_exe, "current core identity executable")
    if binding_version in V6_BINDING_PATHS:
        v6_binding = ROOT / V6_BINDING_PATHS[binding_version]
        if (not v6_binding.is_file() or v6_binding.is_symlink() or
                stat.S_IMODE(v6_binding.stat().st_mode) != 0o600):
            refuse("current v6 RSP source binding is missing")
        verify_rsp_core_binding_v6(read_json(v6_binding), qualification_sha,
                                   build_identity, binding_version)
        return
    if binding_version != "v5":
        refuse("unsupported RSP binding version")
    v5_refresh = ROOT / "build/private-rsp-diagnostic/current-core-identity-binding-v5-refresh3.json"
    v5_binding = ROOT / "build/private-rsp-diagnostic/current-core-identity-binding-v5.json"
    v4_binding = ROOT / "build/private-rsp-diagnostic/current-core-identity-binding-v4.json"
    v3_binding = ROOT / "build/private-rsp-diagnostic/current-core-identity-binding-v3.json"
    v2_binding = ROOT / "build/private-rsp-diagnostic/current-core-identity-binding-v2.json"
    if v5_refresh.is_file() and not v5_refresh.is_symlink():
        verify_rebuilt_core_binding(v5_refresh, qualification_sha, build_identity)
        return
    if v5_binding.is_file() and not v5_binding.is_symlink():
        verify_rebuilt_core_binding(v5_binding, qualification_sha, build_identity)
        return
    if v4_binding.is_file() and not v4_binding.is_symlink():
        # The v4 verifier checks the reader hash and source provenance before
        # invoking it, then validates the intentional current-core rebind.
        verify_rebuilt_core_binding(v4_binding, qualification_sha, build_identity)
        return
    if current_sha != build_identity.get("core_identity_executable_sha256"):
        # The supplemental verifier checks exact executable bytes and provenance
        # before invoking the replacement. Never query an unbound executable.
        core_json = verify_rebuilt_core_binding(
            v3_binding if v3_binding.is_file() and not v3_binding.is_symlink() else v2_binding,
            qualification_sha, build_identity)
    else:
        identity = subprocess.run([str(identity_exe), "--compat-v2", "--runtime-identity-json"],
                                  check=True, capture_output=True, text=True, timeout=10)
        core_json = json.loads(identity.stdout)
        if core_json.get("source_revision") != build_identity.get("core_identity_source_revision"):
            refuse("unchanged production adapter reports a different source revision")
    if core_json.get("core_build_sha256") != build_identity.get("core_build_sha256"):
        refuse("current production core build identity differs from the qualified RSP core")


def verify_preuse(path: pathlib.Path, *, verify_current_identity: bool = True,
                  binding_version: str = "v5") -> None:
    root = ROOT / "build/private-rsp-diagnostic"
    if root.is_symlink() or not root.is_dir() or stat.S_IMODE(root.stat().st_mode) != 0o700:
        refuse("private RSP root is missing or not owner-only")
    qualification_path = private_path(path, "RSP qualification receipt")
    if stat.S_IMODE(qualification_path.stat().st_mode) != 0o600:
        refuse("RSP qualification receipt is not owner-only")
    qualification = read_json(qualification_path)
    qualification = object_with_keys(
        qualification,
        {"assets", "build_identity", "closure", "component", "locks", "membership", "overlay", "profile_id", "schema", "schema_version", "source_revision", "status", "tools"},
        "RSP qualification receipt",
    )
    if qualification["schema"] != "t64.rsp-qualification.v1" or type(qualification["schema_version"]) is not int or qualification["schema_version"] != 1 or qualification["component"] != "rsp" or qualification["status"] != "qualified":
        refuse("RSP qualification receipt is not the sealed pinned-source component")
    closure_ref = object_with_keys(qualification["closure"], {"path", "sha256"}, "RSP closure reference")
    closure_path = private_path(ROOT / str(closure_ref["path"]), "RSP pre-use source closure")
    if private_digest(closure_path, "RSP pre-use source closure") != closure_ref["sha256"] or stat.S_IMODE(closure_path.stat().st_mode) != 0o600:
        refuse("RSP pre-use source closure digest or permissions changed")
    closure = read_json(closure_path)
    if closure.get("schema") != "t64.rsp-preuse-source-closure.v1" or closure.get("component") != "rsp" or closure.get("status") != "PASS_PREBUILD_RSP" or closure.get("source_revision") != qualification["source_revision"]:
        refuse("RSP source closure is not accepted")
    if qualification["membership"] != closure.get("membership") or qualification["overlay"] != closure.get("overlay") or qualification["build_identity"] != closure.get("build_identity") or qualification["locks"] != closure.get("locks") or qualification["tools"] != closure.get("tools") or qualification["assets"] != closure.get("assets"):
        refuse("RSP qualification fields differ from the independently sealed closure")

    private_cpu = ROOT / "build/private-cpu-diagnostic"
    source_root = private_cpu / "nemu64-test"
    cpu_receipt_path = private_cpu / "qualification.json"
    cpu_module = importlib.util.spec_from_file_location("t64_cpu_rsp_verify", ROOT / "scripts/verify-cpu-diagnostic-qualification.py")
    if cpu_module is None or cpu_module.loader is None:
        refuse("accepted CPU qualification validator cannot be loaded")
    cpu_verifier = importlib.util.module_from_spec(cpu_module)
    cpu_module.loader.exec_module(cpu_verifier)
    cpu_verifier.validate(cpu_receipt_path, ROOT)
    cpu_receipt = json.loads(cpu_receipt_path.read_text(encoding="utf-8"))
    if cpu_receipt.get("source_revision") != qualification["source_revision"]:
        refuse("RSP qualification no longer shares the accepted pinned source context")
    projection_path = private_cpu / "source-projection.json"
    projection = json.loads(projection_path.read_text(encoding="utf-8"))
    expected = derive_rsp_membership(projection.get("registrations"))
    membership = closure.get("membership")
    if not isinstance(membership, dict) or membership.get("path") != "build/private-rsp-diagnostic/rsp-source-projection.json":
        refuse("RSP membership receipt path differs")
    membership_path = private_path(ROOT / membership["path"], "RSP membership projection")
    if private_digest(membership_path, "RSP membership projection") != membership.get("sha256"):
        refuse("RSP membership projection bytes changed")
    rsp_projection = json.loads(membership_path.read_text(encoding="utf-8"))
    if rsp_projection.get("registrations") != expected or rsp_projection.get("component") != "rsp" or membership.get("registrations") != len(expected) or membership.get("values") != sum(len(row["values"]) for row in expected):
        refuse("RSP membership no longer exactly partitions pinned source registrations and values")

    source_context = closure.get("source_context")
    if not isinstance(source_context, dict):
        refuse("RSP source context is missing")
    source_inputs = {
        "cpu_qualification_sha256": cpu_receipt_path,
        "source_audit_sha256": private_cpu / "source-audit-full.json",
        "source_summary_sha256": private_cpu / "source-audit-summary.json",
        "cpu_source_projection_sha256": projection_path,
    }
    for field, file_path in source_inputs.items():
        if source_context.get(field) != private_digest(file_path, field):
            refuse(f"RSP source context input changed: {field}")
    cpu_closure_path = ROOT / cpu_receipt["assets"]["closure_report"]["path"]
    graph_path = private_cpu / "unit-graph-review-handler-snapshot-fix.json"
    if source_context.get("cpu_closure_sha256") != private_digest(cpu_closure_path, "accepted CPU source closure") or source_context.get("unit_graph_review_sha256") != private_digest(graph_path, "current accepted CPU unit graph"):
        refuse("RSP source context no longer binds the current accepted closure and graph")
    projection_inputs = rsp_projection.get("inputs")
    if not isinstance(projection_inputs, dict):
        refuse("RSP source projection omits its pinned source input digests")
    for field, file_path in (("source_testlist_sha256", source_root / "src/tests/testlist.rs"), ("source_manifest_sha256", source_root / "Cargo.toml"), ("source_lock_sha256", source_root / "Cargo.lock")):
        if projection_inputs.get(field) != private_digest(file_path, field):
            refuse(f"RSP default selection input changed: {field}")
    source_root_head = subprocess.run(["git", "-C", str(source_root), "rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()
    source_root_status = subprocess.run(["git", "-C", str(source_root), "status", "--porcelain=v1", "--untracked-files=all"], check=True, capture_output=True, text=True).stdout
    if source_root_head != qualification["source_revision"] or source_root_status:
        refuse("pinned source checkout changed after RSP qualification")

    overlay = closure.get("overlay")
    if not isinstance(overlay, dict) or overlay.get("checkout") != "build/private-rsp-diagnostic/overlay-checkout-rsp-v1":
        refuse("RSP overlay checkout identity differs")
    overlay_root = ROOT / overlay["checkout"]
    if overlay_root.is_symlink() or not overlay_root.is_dir():
        refuse("RSP overlay checkout is missing or linked")
    overlay_head = subprocess.run(["git", "-C", str(overlay_root), "rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()
    overlay_status = subprocess.run(["git", "-C", str(overlay_root), "status", "--porcelain=v1", "--untracked-files=all"], check=True, capture_output=True, text=True).stdout
    if overlay_head != qualification["source_revision"] or any(line.startswith("??") for line in overlay_status.splitlines()):
        refuse("RSP source overlay revision changed or contains unreviewed files")
    diff = subprocess.run(["git", "-C", str(overlay_root), "diff", "--binary"], check=True, capture_output=True).stdout
    if digest(diff) != overlay.get("sha256"):
        refuse("RSP source overlay changed after qualification")
    overlay_files = overlay.get("files")
    if not isinstance(overlay_files, list) or len(overlay_files) != overlay.get("file_count"):
        refuse("RSP overlay module manifest is malformed")
    for row in overlay_files:
        if not isinstance(row, dict) or set(row) != {"path", "sha256"}:
            refuse("RSP overlay module row is malformed")
        relative = pathlib.PurePosixPath(str(row["path"]))
        if relative.is_absolute() or any(part in ("", ".", "..", ".git") for part in relative.parts):
            refuse("RSP overlay module path escapes its checkout")
        file_path = overlay_root.joinpath(*relative.parts)
        source_bytes = file_path.read_bytes() if file_path.is_file() and not file_path.is_symlink() else b""
        if not source_bytes or digest(source_bytes) != row["sha256"]:
            refuse("RSP overlay module bytes changed")
        if relative.as_posix().startswith(("src/rsp/", "src/tests/rsp/")):
            source_text = source_bytes.decode("utf-8", errors="strict").lower()
            if any(token in source_text for token in ("font", "bitmap", "include_bytes!", "include_str!", "ipl3", "romdata")):
                refuse("selected RSP module now references unqualified font/data/boot bytes")

    assets = closure.get("assets")
    font_graphics = assets.get("font_graphics", {}).get("status") if isinstance(assets, dict) else None
    if not isinstance(font_graphics, str) or not font_graphics.startswith("excluded from compiled default RSP closure;") or "unqualified" not in font_graphics:
        refuse("RSP font/data rights disposition is unresolved")
    boot = assets.get("ipl3")
    if not isinstance(boot, dict) or boot.get("path") != "build/fixtures/ipl3_compat.z64" or private_digest(ROOT / boot["path"], "RSP open IPL3 fixture") != boot.get("sha256"):
        refuse("RSP open IPL3 fixture changed")
    if boot["sha256"] != "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068":
        refuse("RSP open IPL3 fixture is not the accepted byte identity")

    locks = closure.get("locks")
    expected_locks = {
        "project_cargo_lock_sha256": source_root / "Cargo.lock",
        "sysroot_cargo_lock_sha256": private_cpu / "toolchain-prefix/lib/rustlib/src/rust/library/Cargo.lock",
        "vendor_partition_sha256": private_cpu / "vendor-manifest-complete.json",
        "unit_graph_review_sha256": private_cpu / "unit-graph-review-handler-snapshot-fix.json",
    }
    if not isinstance(locks, dict):
        refuse("RSP lock partition receipt is missing")
    for field, file_path in expected_locks.items():
        if locks.get(field) != private_digest(file_path, field):
            refuse(f"RSP offline lock/tool partition changed: {field}")
    tools = closure.get("tools")
    if not isinstance(tools, dict) or tools.get("cpu_qualification_sha256") != private_digest(cpu_receipt_path, "accepted CPU qualification"):
        refuse("RSP build tool receipt is not bound to the current accepted CPU qualification")
    tool_files = tools.get("tool_files")
    runtime_files = tools.get("runtime_files")
    if not isinstance(tool_files, list) or not isinstance(runtime_files, list) or not tool_files or not runtime_files:
        refuse("RSP tool/runtime closure is incomplete")
    for row in [*tool_files, *runtime_files]:
        if not isinstance(row, dict) or not isinstance(row.get("path"), str) or not isinstance(row.get("sha256"), str) or re.fullmatch(r"[0-9a-f]{64}", row["sha256"]) is None:
            refuse("RSP tool/runtime closure row is malformed")
        tool_path = pathlib.Path(row["path"])
        if not tool_path.is_absolute():
            tool_path = ROOT / tool_path
        if private_digest(tool_path, "RSP build tool/runtime") != row["sha256"]:
            refuse("RSP build tool/runtime bytes changed")
    build_identity = closure.get("build_identity")
    if not isinstance(build_identity, dict) or build_identity.get("profile_id") != RSP_PROFILE_ID or build_identity.get("core_model") != 6 or build_identity.get("core_state_schema") != 12:
        refuse("RSP build profile or core state identity differs")
    expected_profile = {"id": "ntsc-cart-4m-openipl3-rsp-functional-v1", "region": "NTSC", "media": "cartridge", "rdram_bytes": 4 * 1024 * 1024, "boot_asset_sha256": BOOT_SHA, "component": "rsp-functional-diagnostic", "expansion": "none", "peripherals": "none"}
    if build_identity.get("profile_sha256") != digest(canonical(expected_profile)):
        refuse("RSP profile digest does not match its private profile fields")
    profile_source = project_path(ROOT / "core/t64_machine.c", "current RSP functional profile source")
    verify_rsp_profile_policy(profile_source.read_text(encoding="utf-8"), build_identity["profile_id"])
    if verify_current_identity:
        verify_current_core_identity(build_identity, digest(qualification_path.read_bytes()),
                                     binding_version)
        print("PASS: independent RSP source, assets, tools, locks, and build identity verified before use")
    else:
        print("PASS: independent RSP source, assets, tools, and locks verified; no identity executable was invoked")


def verify_fixture(path: pathlib.Path) -> None:
    fixture = read_json(path)
    fixture = object_with_keys(
        fixture,
        {"component", "declared_counts", "inventory_rows", "inventory_sha256", "schema", "schema_version", "source_rows", "source_sha256"},
        "RSP qualification fixture",
    )
    if fixture["schema"] != "t64.rsp-qualification-fixture" or type(fixture["schema_version"]) is not int or fixture["schema_version"] != 1 or fixture["component"] != "rsp":
        refuse("qualification fixture is not an RSP component receipt")
    source_rows = fixture["source_rows"]
    inventory_rows = fixture["inventory_rows"]
    counts = object_with_keys(fixture["declared_counts"], {"registrations", "values"}, "declared counts")
    if not isinstance(source_rows, list) or not isinstance(inventory_rows, list) or not isinstance(counts["registrations"], int) or not isinstance(counts["values"], int):
        refuse("qualification fixture membership or counts are malformed")
    if fixture["source_sha256"] != digest(canonical(source_rows)) or fixture["inventory_sha256"] != digest(canonical(inventory_rows)):
        refuse("qualification fixture digest does not match its bytes")
    expected_inventory = derive_rsp_membership(source_rows)
    if inventory_rows != expected_inventory:
        refuse("source registration/value partition is incomplete")
    if counts["registrations"] != len(inventory_rows) or counts["values"] != sum(len(row.get("values", [])) for row in inventory_rows if isinstance(row, dict)):
        refuse("qualification fixture declared counts differ from the inventory")
    seen: set[int] = set()
    for row in inventory_rows:
        if not isinstance(row, dict) or type(row.get("index")) is not int or row["index"] in seen:
            refuse("qualification fixture contains a malformed or duplicate registration")
        seen.add(row["index"])
        if not isinstance(row.get("values"), list) or not row["values"]:
            refuse("qualification fixture contains an empty value identity list")
        for index, value in enumerate(row["values"]):
            if not isinstance(value, dict) or value.get("index") != index or not isinstance(value.get("id"), str) or not isinstance(value.get("selector"), str):
                refuse("qualification fixture contains a malformed source value")
    print("PASS: RSP qualification fixture integrity verified")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify-fixture", type=pathlib.Path)
    parser.add_argument("--verify-closure-fixture", type=pathlib.Path)
    parser.add_argument("--verify", type=pathlib.Path)
    parser.add_argument("--verify-source-closure", type=pathlib.Path)
    parser.add_argument("--binding-version", choices=("v5", "v6", "v6-refresh1"), default="v5")
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--workspace", type=pathlib.Path)
    args = parser.parse_args()
    try:
        if args.verify is not None:
            if args.verify_source_closure is not None or args.verify_fixture is not None or args.verify_closure_fixture is not None or args.selftest or args.workspace is not None:
                refuse("--verify cannot be combined with fixture or selftest options")
            verify_preuse(args.verify, binding_version=args.binding_version)
        elif args.verify_source_closure is not None:
            if (args.verify_fixture is not None or args.verify_closure_fixture is not None or
                    args.selftest or args.workspace is not None or args.binding_version != "v5"):
                refuse("--verify-source-closure cannot be combined with fixture or selftest options")
            verify_preuse(args.verify_source_closure, verify_current_identity=False)
        elif args.selftest:
            if args.workspace is None or args.verify_fixture is not None or args.verify_closure_fixture is not None:
                refuse("authored selftest requires --workspace")
            fixture = pathlib.Path(args.workspace) / "fixture.json"
            verify_fixture(fixture)
            closure_path = pathlib.Path(args.workspace) / "closure-fixture.json"
            if not closure_path.exists():
                with closure_path.open("x", encoding="utf-8") as stream:
                    json.dump(authored_closure_fixture(), stream, sort_keys=True, separators=(",", ":"))
                    stream.write("\n")
                closure_path.chmod(0o600)
            verify_authored_closure_fixture(read_json(closure_path))
            print("PASS: clean authored RSP pre-use qualification selftest")
        elif args.verify_closure_fixture is not None and args.workspace is None:
            verify_authored_closure_fixture(read_json(args.verify_closure_fixture))
        elif args.verify_fixture is not None and args.workspace is None:
            verify_fixture(args.verify_fixture)
        else:
            refuse("select --verify, --verify-fixture, or --selftest --workspace")
    except (OSError, Refusal, ValueError) as error:
        print(f"refused: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
