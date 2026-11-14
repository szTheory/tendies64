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
        "verification_scope",
    }
    binding = object_with_keys(binding, expected, "current RSP pre-use binding v4")
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
                row["path"] not in (fix["path"], fix["regression_path"], extraction["path"]) and
                row["sha256"] != prior[row["path"]]):
            refuse("production core changed beyond the single CTC1 predicate fix")
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
    if normal_build["path"] != "build/private-rsp-diagnostic/plan04/task3-postcommit-gate-build.log":
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


def verify_rebuilt_core_binding(binding_path: pathlib.Path, qualification_sha: str, build_identity: dict[str, object]) -> dict[str, object]:
    binding_path = private_path(binding_path, "current core identity binding")
    if stat.S_IMODE(binding_path.stat().st_mode) != 0o600:
        refuse("current core identity binding is not owner-only")
    binding = read_json(binding_path)
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


def verify_current_core_identity(build_identity: dict[str, object], qualification_sha: str) -> None:
    identity_exe = ROOT / "build/gate/runner/t64-cpu-diagnostic"
    current_sha = private_digest(identity_exe, "current core identity executable")
    v4_binding = ROOT / "build/private-rsp-diagnostic/current-core-identity-binding-v4.json"
    v3_binding = ROOT / "build/private-rsp-diagnostic/current-core-identity-binding-v3.json"
    v2_binding = ROOT / "build/private-rsp-diagnostic/current-core-identity-binding-v2.json"
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
    verify_current_core_identity(build_identity, digest(qualification_path.read_bytes()))
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
