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


class Refusal(ValueError):
    pass


def refuse(message: str) -> None:
    raise Refusal(message)


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


def private_digest(path: pathlib.Path, label: str) -> str:
    path = pathlib.Path(path)
    if path.is_symlink() or not path.is_file():
        refuse(f"{label} is missing or linked")
    return digest(path.read_bytes())


def verify_rebuilt_core_binding(binding_path: pathlib.Path, qualification_sha: str, build_identity: dict[str, object]) -> None:
    binding_path = private_path(binding_path, "current core identity binding")
    if stat.S_IMODE(binding_path.stat().st_mode) != 0o600:
        refuse("current core identity binding is not owner-only")
    binding = read_json(binding_path)
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


def verify_preuse(path: pathlib.Path) -> None:
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
    if not isinstance(build_identity, dict) or build_identity.get("profile_id") != "ntsc-cart-4m-openipl3-rsp-functional-v1" or build_identity.get("core_model") != 6 or build_identity.get("core_state_schema") != 12:
        refuse("RSP build profile or core state identity differs")
    expected_profile = {"id": "ntsc-cart-4m-openipl3-rsp-functional-v1", "region": "NTSC", "media": "cartridge", "rdram_bytes": 4 * 1024 * 1024, "boot_asset_sha256": BOOT_SHA, "component": "rsp-functional-diagnostic", "expansion": "none", "peripherals": "none"}
    if build_identity.get("profile_sha256") != digest(canonical(expected_profile)):
        refuse("RSP profile digest does not match its private profile fields")
    identity_exe = ROOT / "build/gate/runner/t64-cpu-diagnostic"
    current_executable_sha = private_digest(identity_exe, "current core identity executable")
    identity = subprocess.run([str(identity_exe), "--compat-v2", "--runtime-identity-json"], check=True, capture_output=True, text=True)
    core_json = json.loads(identity.stdout)
    if core_json.get("core_build_sha256") != build_identity.get("core_build_sha256") or not isinstance(core_json.get("source_revision"), str) or re.fullmatch(r"[0-9a-f]{40}", core_json["source_revision"]) is None:
        refuse("current production core build identity differs from the qualified RSP core")
    if current_executable_sha == build_identity.get("core_identity_executable_sha256"):
        if core_json["source_revision"] != build_identity.get("core_identity_source_revision"):
            refuse("unchanged production adapter reports a different source revision")
    else:
        verify_rebuilt_core_binding(ROOT / "build/private-rsp-diagnostic/current-core-identity-binding-v2.json", digest(qualification_path.read_bytes()), build_identity)
    print("PASS: independent RSP source, assets, tools, locks, and build identity verified before use")


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
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--workspace", type=pathlib.Path)
    args = parser.parse_args()
    try:
        if args.verify is not None:
            if args.verify_fixture is not None or args.verify_closure_fixture is not None or args.selftest or args.workspace is not None:
                refuse("--verify cannot be combined with fixture or selftest options")
            verify_preuse(args.verify)
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
