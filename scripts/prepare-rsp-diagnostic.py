#!/usr/bin/env python3
"""Prepare the private, source-derived default RSP membership projection."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import pathlib
import re
import signal
import stat
import subprocess
import sys
import tempfile
import tomllib
import time

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
CORE_BASE_REVISION = "d7479bcc854348c1384cc199815954c662ffe9be"
CPU_PRIVATE = ROOT / "build/private-cpu-diagnostic"
DEFAULT_RSP_PATH = "src/tests/rsp/"
LEVEL_LANES = {
    "BasicFunctionality": "required-base",
    "RarelyUsed": "required-base",
    "Weird": "required-base",
    "Timing": "timing",
    "Cycle": "cycle",
    "COP0Hazard": "cop0-hazard",
    "PoorlyUnderstoodQuirk": "quirk",
    "StressTest": "stress",
    "RDPBasic": "informational",
    "RDPPrecise": "informational",
}


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


def read_json(path: pathlib.Path, label: str, maximum: int = 32 * 1024 * 1024) -> object:
    if path.is_symlink() or not path.is_file() or path.stat().st_size > maximum:
        refuse(f"{label} is linked, missing, or exceeds its size limit")
    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate_keys)
    except (OSError, json.JSONDecodeError, UnicodeDecodeError):
        refuse(f"{label} is malformed")


def owner_only(path: pathlib.Path, *, create: bool = False) -> pathlib.Path:
    path = pathlib.Path(os.path.abspath(path))
    try:
        relative = path.relative_to(ROOT)
    except ValueError:
        refuse("private path escapes the repository")
    cursor = ROOT
    for part in relative.parts:
        cursor /= part
        if cursor.is_symlink():
            refuse("private path traverses a symlink")
    if create:
        path.mkdir(mode=0o700, parents=True, exist_ok=True)
        path.chmod(0o700)
    if not path.is_dir() or stat.S_IMODE(path.stat().st_mode) != 0o700:
        refuse("private directory must exist with owner-only permissions")
    return path.resolve(strict=True)


def write_private(path: pathlib.Path, value: object) -> None:
    parent = owner_only(path.parent)
    if path.is_symlink() or path.exists():
        refuse("refusing to overwrite an existing private report")
    if parent not in path.resolve(strict=False).parents:
        refuse("private report path escapes its directory")
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(path, flags, 0o600)
    with os.fdopen(descriptor, "wb") as stream:
        stream.write(canonical(value))


def level_lane(level: object) -> str:
    if not isinstance(level, str) or level not in LEVEL_LANES:
        refuse("source registration uses an unclassified level")
    return LEVEL_LANES[level]


def derive_rsp_membership(source_rows: object) -> list[dict[str, object]]:
    if not isinstance(source_rows, list) or not source_rows:
        refuse("source registration set is empty or malformed")
    indices: set[int] = set()
    selected: list[dict[str, object]] = []
    for row in source_rows:
        if not isinstance(row, dict):
            refuse("source registration is malformed")
        index = row.get("index")
        source_file = row.get("source_file")
        if type(index) is not int or index < 0 or index > 0xFFFF or index in indices:
            refuse("source registration index is malformed or duplicated")
        indices.add(index)
        if not isinstance(source_file, str) or not source_file.startswith("src/tests/"):
            refuse("source implementation path is malformed")
        if not source_file.startswith(DEFAULT_RSP_PATH):
            continue
        source_count = row.get("source_value_count")
        effective_count = max(1, source_count) if type(source_count) is int and source_count >= 0 else None
        if effective_count is None:
            refuse("source value count is malformed")
        source_name = row.get("source_name")
        constructor = row.get("constructor")
        source_file_sha = row.get("source_file_sha256")
        values_method_sha = row.get("values_method_sha256")
        if (
            not isinstance(source_name, str)
            or not source_name
            or not isinstance(constructor, str)
            or not constructor
            or not isinstance(source_file_sha, str)
            or re.fullmatch(r"[0-9a-f]{64}", source_file_sha) is None
            or not isinstance(values_method_sha, str)
            or re.fullmatch(r"[0-9a-f]{64}", values_method_sha) is None
        ):
            refuse("source registration identity is malformed")
        values = []
        if source_count == 0:
            values.append({"id": "once", "index": 0, "selector": f"r{index:04d}v00000"})
        else:
            for value_index in range(source_count):
                values.append({
                    "id": f"v{value_index:05d}",
                    "index": value_index,
                    "selector": f"r{index:04d}v{value_index:05d}",
                })
        selected.append({
            "constructor": constructor,
            "execution": "once" if source_count == 0 else "values",
            "group": "rsp",
            "index": index,
            "lane": level_lane(row.get("level")),
            "level": row["level"],
            "name": source_name,
            "source_file": source_file,
            "source_file_sha256": source_file_sha,
            "source_name": source_name,
            "source_value_count": source_count,
            "effective_value_count": effective_count,
            "values_method_sha256": values_method_sha,
            "values": values,
        })
    if not selected:
        refuse("default source profile contains no RSP registrations")
    names: dict[str, int] = {}
    for row in selected:
        name = str(row["source_name"])
        names[name] = names.get(name, 0) + 1
    for row in selected:
        if names[str(row["source_name"])] > 1:
            row["name"] = f"{row['source_name']} [source registration {row['index']}]"
    return selected


def authored_rows() -> list[dict[str, object]]:
    return [
        {"index": 0, "source_file": "src/tests/rsp/authored_a.rs", "source_name": "Repeated", "constructor": "super::rsp::a::Alias", "level": "BasicFunctionality", "source_value_count": 0, "source_file_sha256": "a" * 64, "values_method_sha256": "b" * 64},
        {"index": 1, "source_file": "src/tests/rsp/authored_b.rs", "source_name": "Repeated", "constructor": "super::rsp::b::Alias", "level": "RarelyUsed", "source_value_count": 2, "source_file_sha256": "c" * 64, "values_method_sha256": "d" * 64},
        {"index": 2, "source_file": "src/tests/cpu/authored.rs", "source_name": "Outside", "constructor": "super::cpu::Outside", "level": "BasicFunctionality", "source_value_count": 1, "source_file_sha256": "e" * 64, "values_method_sha256": "f" * 64},
        {"index": 3, "source_file": "src/tests/rsp/authored_c.rs", "source_name": "Third", "constructor": "super::rsp::c::Third", "level": "Weird", "source_value_count": 1, "source_file_sha256": "1" * 64, "values_method_sha256": "2" * 64},
        {"index": 4, "source_file": "src/tests/rsp/authored_d.rs", "source_name": "Timing", "constructor": "super::rsp::d::Timing", "level": "Timing", "source_value_count": 1, "source_file_sha256": "3" * 64, "values_method_sha256": "4" * 64},
    ]


def make_fixture(source_rows: list[dict[str, object]]) -> dict[str, object]:
    inventory = derive_rsp_membership(source_rows)
    return {
        "schema": "t64.rsp-qualification-fixture",
        "schema_version": 1,
        "component": "rsp",
        "source_rows": source_rows,
        "inventory_rows": inventory,
        "source_sha256": digest(canonical(source_rows)),
        "inventory_sha256": digest(canonical(inventory)),
        "declared_counts": {
            "registrations": len(inventory),
            "values": sum(len(row["values"]) for row in inventory),
        },
    }


def selftest(workspace: pathlib.Path) -> None:
    root = owner_only(workspace)
    fixture = make_fixture(authored_rows())
    write_private(root / "fixture.json", fixture)
    rows = fixture["inventory_rows"]
    if (
        not isinstance(rows, list)
        or len(rows) != 4
        or [row["index"] for row in rows] != [0, 1, 3, 4]
        or rows[0]["execution"] != "once"
        or rows[0]["values"][0]["id"] != "once"
        or rows[0]["name"] == rows[1]["name"]
        or [value["index"] for value in rows[1]["values"]] != [0, 1]
        or rows[3]["lane"] != "timing"
    ):
        refuse("authored RSP source/value membership control failed")
    print("PASS: authored RSP membership and closure controls")


def import_script(path: pathlib.Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        refuse("required audited helper cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def prepare_source(source_root: pathlib.Path, output: pathlib.Path) -> None:
    private_root = owner_only(CPU_PRIVATE)
    rsp_private = owner_only(ROOT / "build/private-rsp-diagnostic")
    cpu_receipt_path = CPU_PRIVATE / "qualification.json"
    cpu_verifier = import_script(ROOT / "scripts/verify-cpu-diagnostic-qualification.py", "t64_cpu_qualification")
    cpu_verifier.validate(cpu_receipt_path, ROOT)
    cpu_receipt = read_json(cpu_receipt_path, "accepted CPU qualification receipt")
    source_revision = cpu_receipt.get("source_revision") if isinstance(cpu_receipt, dict) else None
    if not isinstance(source_revision, str) or re.fullmatch(r"[0-9a-f]{40}", source_revision) is None:
        refuse("accepted CPU source revision is malformed")
    source_root = source_root.resolve(strict=True)
    try:
        source_root.relative_to(private_root)
    except ValueError:
        refuse("pinned source checkout is outside the accepted private root")
    if source_root.is_symlink() or not source_root.is_dir():
        refuse("pinned source checkout is linked or missing")
    head = subprocess.run(["git", "-C", str(source_root), "rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()
    status = subprocess.run(["git", "-C", str(source_root), "status", "--porcelain=v1", "--untracked-files=all"], check=True, capture_output=True, text=True).stdout
    if head != source_revision or status:
        refuse("pinned source checkout revision or clean state differs")

    if not isinstance(cpu_receipt, dict):
        refuse("accepted CPU qualification receipt is malformed")
    projection_path = CPU_PRIVATE / "source-projection.json"
    audit_path = CPU_PRIVATE / "source-audit-full.json"
    summary_path = CPU_PRIVATE / "source-audit-summary.json"
    closure_path = CPU_PRIVATE / "source-closure-8aa0921-park-fix.json"
    graph_path = CPU_PRIVATE / "unit-graph-review-handler-snapshot-fix.json"
    projection = read_json(projection_path, "accepted CPU source projection")
    audit = read_json(audit_path, "accepted CPU source audit")
    summary = read_json(summary_path, "accepted CPU source audit summary")
    closure = read_json(closure_path, "accepted CPU source closure")
    graph = read_json(graph_path, "current accepted CPU unit graph")
    if not isinstance(projection, dict) or not isinstance(audit, dict) or not isinstance(summary, dict):
        refuse("accepted CPU source inputs are malformed")
    if (
        audit.get("source_revision") != source_revision
        or summary.get("source_revision") != source_revision
        or projection.get("source_revision") != source_revision
        or closure.get("status") != "PASS_PREBUILD_SOURCE"
        or graph.get("status") != "PASS_FROZEN_OFFLINE_UNIT_GRAPH"
        or graph.get("schema") != "t64.cpu-unit-graph-review.v1"
    ):
        refuse("accepted CPU audit context differs from the pinned RSP source")
    graph_inputs = graph.get("inputs")
    if not isinstance(graph_inputs, dict):
        refuse("current accepted CPU graph inputs are malformed")
    for key, path in (("source_audit", audit_path), ("source_projection", projection_path)):
        row = graph_inputs.get(key)
        if not isinstance(row, dict) or row.get("path") != path.relative_to(ROOT).as_posix() or row.get("sha256") != digest(path.read_bytes()):
            refuse(f"accepted CPU {key.replace('_', ' ')} digest or path changed")
    projection_receipt = cpu_receipt.get("source", {}).get("projection")
    if not isinstance(projection_receipt, dict) or projection_receipt.get("sha256") != digest(projection_path.read_bytes()):
        refuse("accepted CPU projection receipt digest changed")
    if graph_inputs.get("qualification", {}).get("sha256") != digest(cpu_receipt_path.read_bytes()):
        refuse("current accepted CPU graph does not bind the qualification receipt")
    if graph_inputs.get("source_closure", {}).get("sha256") != digest(closure_path.read_bytes()):
        refuse("current accepted CPU graph does not bind the current source closure")
    manifest_path = source_root / "Cargo.toml"
    manifest = tomllib.loads(manifest_path.read_text(encoding="utf-8"))
    graph_verifier = import_script(ROOT / "scripts/verify-cpu-diagnostic-unit-graph.py", "t64_cpu_unit_graph")
    features = graph_verifier.default_feature_closure(manifest)
    stress_features = {"vmulf_stress_test", "vmulu_stress_test", "vmulq_stress_test", "vmudl_stress_test", "vmudh_stress_test", "vmudm_stress_test", "vmudn_stress_test", "vmacf_stress_test", "vmacu_stress_test", "vmadl_stress_test", "vmadh_stress_test", "vmadm_stress_test", "vmadn_stress_test", "vrcp32_stress_test", "vrsq32_stress_test", "rcp_rsq_dump"}
    if "quick" not in features or features & stress_features:
        refuse("default feature closure is not the reviewed nonstress RSP profile")
    source_rows = projection.get("registrations")
    audit_rows = audit.get("registrations")
    if not isinstance(source_rows, list) or not isinstance(audit_rows, list) or len(source_rows) != len(audit_rows):
        refuse("accepted source membership rows are malformed")
    for index, (projected, audited) in enumerate(zip(source_rows, audit_rows, strict=True)):
        if not isinstance(projected, dict) or not isinstance(audited, dict):
            refuse("accepted source membership row is malformed")
        if (
            projected.get("index") != index
            or audited.get("index") != index
            or projected.get("constructor") != audited.get("path")
            or projected.get("source_name") != audited.get("upstream_name")
            or projected.get("level") != audited.get("level")
            or projected.get("source_value_count") != audited.get("source_value_count")
            or projected.get("source_file_sha256") != audited.get("source_sha256")
            or projected.get("values_method_sha256") != audited.get("values_method_sha256")
        ):
            refuse("accepted source projection and audit differ")
        source_file = projected.get("source_file")
        if not isinstance(source_file, str):
            refuse("source implementation path is malformed")
        implementation = source_root / source_file
        if implementation.is_symlink() or not implementation.is_file() or digest(implementation.read_bytes()) != projected.get("source_file_sha256"):
            refuse("pinned source implementation bytes differ from the accepted CPU audit")

    inventory = derive_rsp_membership(source_rows)
    output = pathlib.Path(os.path.abspath(output))
    try:
        output.relative_to(rsp_private)
    except ValueError:
        refuse("RSP membership output is outside the private root")
    report = {
        "schema": "t64.rsp-source-projection",
        "schema_version": 1,
        "component": "rsp",
        "status": "membership-prepared",
        "source_revision": source_revision,
        "source_root": "build/private-cpu-diagnostic/nemu64-test",
        "default_feature_closure": sorted(features),
        "selection": {"source_path_prefix": DEFAULT_RSP_PATH, "level_lanes": LEVEL_LANES},
        "registrations": inventory,
        "declared_counts": {
            "registrations": len(inventory),
            "values": sum(len(row["values"]) for row in inventory),
        },
        "inputs": {
            "cpu_qualification_sha256": digest(cpu_receipt_path.read_bytes()),
            "cpu_source_audit_sha256": digest(audit_path.read_bytes()),
            "cpu_source_summary_sha256": digest(summary_path.read_bytes()),
            "cpu_source_projection_sha256": digest(projection_path.read_bytes()),
            "cpu_source_closure_sha256": digest(closure_path.read_bytes()),
            "cpu_unit_graph_sha256": digest(graph_path.read_bytes()),
            "source_testlist_sha256": digest((source_root / "src/tests/testlist.rs").read_bytes()),
            "source_manifest_sha256": digest(manifest_path.read_bytes()),
            "source_lock_sha256": digest((source_root / "Cargo.lock").read_bytes()),
        },
        "preuse_closure": "pending-rsp-overlay-assets-tools-lock-and-build-identity",
    }
    write_private(output, report)
    print("PASS: private RSP source membership projection prepared; pre-use closure remains pending")


def prepare_overlay(source_root: pathlib.Path, projection_path: pathlib.Path, output: pathlib.Path, receipt_path: pathlib.Path) -> None:
    private_root = owner_only(ROOT / "build/private-rsp-diagnostic")
    cpu_receipt_path = CPU_PRIVATE / "qualification.json"
    cpu_verifier = import_script(ROOT / "scripts/verify-cpu-diagnostic-qualification.py", "t64_cpu_overlay_qualification")
    cpu_verifier.validate(cpu_receipt_path, ROOT)
    cpu_receipt = read_json(cpu_receipt_path, "accepted CPU qualification receipt")
    source_revision = cpu_receipt.get("source_revision") if isinstance(cpu_receipt, dict) else None
    if not isinstance(source_revision, str) or re.fullmatch(r"[0-9a-f]{40}", source_revision) is None:
        refuse("accepted CPU source revision is malformed")
    source_root = source_root.resolve(strict=True)
    try:
        source_root.relative_to(CPU_PRIVATE)
    except ValueError:
        refuse("pinned source checkout is outside the accepted private root")
    if source_root.is_symlink() or not source_root.is_dir():
        refuse("pinned source checkout is missing or linked")
    head = subprocess.run(["git", "-C", str(source_root), "rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()
    status = subprocess.run(["git", "-C", str(source_root), "status", "--porcelain=v1", "--untracked-files=all"], check=True, capture_output=True, text=True).stdout
    if head != source_revision or status:
        refuse("pinned source checkout revision or clean state differs")
    projection = read_json(projection_path, "RSP membership projection")
    cpu_projection_path = CPU_PRIVATE / "source-projection.json"
    audit_path = CPU_PRIVATE / "source-audit-full.json"
    cpu_projection = read_json(cpu_projection_path, "accepted CPU source projection")
    audit = read_json(audit_path, "accepted CPU source audit")
    registrations = derive_rsp_membership(cpu_projection.get("registrations") if isinstance(cpu_projection, dict) else None)
    if not isinstance(projection, dict) or projection.get("component") != "rsp" or projection.get("registrations") != registrations:
        refuse("RSP overlay input does not match the exact pinned membership projection")
    output = pathlib.Path(os.path.abspath(output))
    receipt_path = pathlib.Path(os.path.abspath(receipt_path))
    try:
        output.relative_to(private_root)
        receipt_path.relative_to(private_root)
    except ValueError:
        refuse("RSP overlay or receipt destination escapes the private root")
    if output.exists() or output.is_symlink() or receipt_path.exists() or receipt_path.is_symlink():
        refuse("refusing to reuse an existing RSP overlay checkout or receipt")
    subprocess.run(["git", "clone", "--quiet", "--no-hardlinks", str(source_root), str(output)], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    subprocess.run([sys.executable, "-B", str(ROOT / "scripts/apply-cpu-diagnostic-overlay.py"), "--apply", str(output), "tests/cpu-diagnostic/overlay.patch", "--source-projection", str(cpu_projection_path), "--source-audit", str(audit_path)], cwd=ROOT, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    (output / "src/rsp/mod.rs").write_bytes((source_root / "src/rsp/mod.rs").read_bytes())
    test_mod_path = output / "src/tests/mod.rs"
    test_mod = test_mod_path.read_text(encoding="utf-8")
    if "mod rsp;" not in test_mod:
        try:
            start = test_mod.index("mod address_error_exception;")
            end = test_mod.index("mod traps;") + len("mod traps;")
        except ValueError:
            refuse("authored headless test harness module boundary differs")
        test_mod = test_mod[:start] + "mod rsp;\nmod soft_asserts;\nmod testlist;" + test_mod[end:]
    test_mod = test_mod.replace("T64_CPU_SELECTOR", "T64_RSP_SELECTOR").replace("t64.cpu-run", "t64.rsp-run")
    test_mod_path.write_text(test_mod, encoding="utf-8")
    main_path = output / "src/main.rs"
    main_path.write_text(main_path.read_text(encoding="utf-8").replace("T64_CPU_SELECTOR", "T64_RSP_SELECTOR"), encoding="utf-8")

    helper = import_script(ROOT / "scripts/apply-cpu-diagnostic-overlay.py", "t64_rsp_overlay_renderer")
    source_lines = (source_root / "src/tests/testlist.rs").read_text(encoding="utf-8").splitlines()
    audit_rows = audit.get("registrations") if isinstance(audit, dict) else None
    if not isinstance(audit_rows, list):
        refuse("accepted source audit membership rows are malformed")
    rendered = [
        "// Generated privately from the sealed source projection; do not publish this derived file.\n",
        "use alloc::boxed::Box;\nuse alloc::vec;\nuse alloc::vec::Vec;\nuse super::Test;\n\n",
        "pub fn tests() -> Vec<(usize, Box<dyn Test>)> {\n    vec![\n",
    ]
    for row in registrations:
        index = row["index"]
        if index >= len(audit_rows) or not isinstance(audit_rows[index], dict):
            refuse("RSP registration has no accepted source audit row")
        audited = audit_rows[index]
        if row["constructor"] != audited.get("path") or row["source_name"] != audited.get("upstream_name") or pathlib.Path(str(audited.get("source_file"))).relative_to(source_root).as_posix() != row["source_file"]:
            refuse("RSP registration source identity differs from the accepted audit")
        anchor = audited.get("line")
        if type(anchor) is not int or anchor < 0:
            refuse("RSP registration has no pinned constructor anchor")
        expression_start = next((line for line in range(max(0, anchor - 1), min(anchor + 8, len(source_lines))) if source_lines[line].lstrip().startswith("Box::new(") and row["constructor"] in source_lines[line]), None)
        if expression_start is None:
            refuse("pinned RSP constructor expression is missing")
        try:
            expression = helper.render_registration_expression(source_lines, expression_start, row["constructor"])
        except ValueError:
            refuse("pinned RSP constructor expression is not an audited unit-test registration")
        rendered.append(f"        ({index}usize, {expression} as Box<dyn Test>),\n")
    rendered.append("    ]\n}\n")
    (output / "src/tests/testlist.rs").write_text("".join(rendered), encoding="utf-8")

    rsp_mod_path = output / "src/tests/rsp/mod.rs"
    rsp_mod = rsp_mod_path.read_text(encoding="utf-8").replace("pub mod stresstests;\n", "").replace("pub mod stresstests_div;\n", "")
    if "stresstests" in rsp_mod or "stresstests_div" in rsp_mod:
        refuse("nondefault font-dependent stress modules could not be excluded")
    rsp_mod_path.write_text(rsp_mod, encoding="utf-8")
    if "T64_CPU_SELECTOR" in test_mod or "t64.cpu-run" in test_mod or "T64_CPU_SELECTOR" in main_path.read_text(encoding="utf-8"):
        refuse("CPU selector identity remains in RSP overlay")

    overlay_diff = subprocess.run(["git", "-C", str(output), "diff", "--binary"], check=True, capture_output=True).stdout
    manifest = []
    for path in sorted(item for item in output.rglob("*.rs") if item.is_file() and not item.is_symlink()):
        relative = path.relative_to(output).as_posix()
        if relative.startswith("src/rsp/") or relative.startswith("src/tests/rsp/") or relative in ("src/main.rs", "src/tests/mod.rs", "src/tests/testlist.rs", "src/tests/soft_asserts.rs", "src/text_out.rs", "src/print.rs", "src/graphics/mod.rs"):
            manifest.append({"path": relative, "sha256": digest(path.read_bytes())})
    receipt = {
        "schema": "t64.rsp-overlay-preparation.v1",
        "status": "overlay-prepared-closure-pending",
        "source_revision": source_revision,
        "checkout": output.relative_to(ROOT).as_posix(),
        "overlay_sha256": digest(overlay_diff),
        "overlay_files": manifest,
        "selected_registration_count": len(registrations),
        "selected_value_count": sum(len(row["values"]) for row in registrations),
        "excluded_nondefault_stress_modules": ["src/tests/rsp/stresstests.rs", "src/tests/rsp/stresstests_div.rs"],
    }
    write_private(receipt_path, receipt)
    print("PASS: headless RSP overlay and exact source-derived test selector prepared privately")


def checked_private_file(path: pathlib.Path, label: str) -> str:
    path = pathlib.Path(os.path.abspath(path))
    try:
        path.relative_to(ROOT)
    except ValueError:
        refuse(f"{label} escapes the repository")
    cursor = ROOT
    for part in path.relative_to(ROOT).parts:
        cursor /= part
        if cursor.is_symlink():
            refuse(f"{label} traverses a symlink")
    if not path.is_file():
        refuse(f"{label} is missing or not a regular file")
    return digest(path.read_bytes())


def seal_preuse() -> None:
    """Bind the new RSP closure to the already reviewed CPU/toolchain receipts."""
    private_root = owner_only(ROOT / "build/private-rsp-diagnostic")
    cpu_root = owner_only(CPU_PRIVATE)
    source_root = CPU_PRIVATE / "nemu64-test"
    if source_root.is_symlink() or not source_root.is_dir():
        refuse("pinned source checkout is missing or linked")
    projection_path = private_root / "rsp-source-projection.json"
    overlay_receipt_path = private_root / "rsp-overlay-preparation.json"
    qualification_path = private_root / "qualification.json"
    closure_path = private_root / "rsp-preuse-closure.json"
    if qualification_path.exists() or closure_path.exists():
        refuse("refusing to overwrite an existing RSP qualification seal")

    cpu_receipt_path = CPU_PRIVATE / "qualification.json"
    cpu_verifier = import_script(ROOT / "scripts/verify-cpu-diagnostic-qualification.py", "t64_cpu_rsp_qualification")
    cpu_verifier.validate(cpu_receipt_path, ROOT)
    cpu_receipt = read_json(cpu_receipt_path, "accepted CPU qualification receipt")
    source_revision = cpu_receipt.get("source_revision") if isinstance(cpu_receipt, dict) else None
    if not isinstance(source_revision, str) or re.fullmatch(r"[0-9a-f]{40}", source_revision) is None:
        refuse("accepted CPU source revision is malformed")
    source_projection = read_json(CPU_PRIVATE / "source-projection.json", "accepted CPU source projection")
    source_audit = read_json(CPU_PRIVATE / "source-audit-full.json", "accepted CPU source audit")
    source_summary = read_json(CPU_PRIVATE / "source-audit-summary.json", "accepted CPU source summary")
    cpu_closure_path = ROOT / cpu_receipt["assets"]["closure_report"]["path"]
    cpu_closure = read_json(cpu_closure_path, "accepted CPU source closure")
    graph_path = CPU_PRIVATE / "unit-graph-review-handler-snapshot-fix.json"
    graph = read_json(graph_path, "current accepted CPU unit graph review")
    rsp_projection = read_json(projection_path, "RSP source membership projection")
    overlay_receipt = read_json(overlay_receipt_path, "RSP overlay preparation receipt")
    if not all(isinstance(item, dict) for item in (cpu_receipt, source_projection, source_audit, source_summary, cpu_closure, graph, rsp_projection, overlay_receipt)):
        refuse("an accepted source or tool receipt is malformed")

    head = subprocess.run(["git", "-C", str(source_root), "rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()
    status = subprocess.run(["git", "-C", str(source_root), "status", "--porcelain=v1", "--untracked-files=all"], check=True, capture_output=True, text=True).stdout
    if head != source_revision or status:
        refuse("pinned source checkout or accepted CPU source revision changed")

    source_rows = source_projection.get("registrations")
    expected = derive_rsp_membership(source_rows)
    if rsp_projection.get("registrations") != expected or rsp_projection.get("component") != "rsp":
        refuse("RSP source projection does not exactly partition the pinned source registrations and values")
    if rsp_projection.get("declared_counts") != {"registrations": len(expected), "values": sum(len(row["values"]) for row in expected)}:
        refuse("RSP source projection denominator differs from its exact source partition")

    overlay_rel = overlay_receipt.get("checkout")
    if overlay_receipt.get("schema") != "t64.rsp-overlay-preparation.v1" or overlay_receipt.get("source_revision") != source_revision:
        refuse("RSP overlay preparation receipt has the wrong component or source revision")
    overlay_root = ROOT / str(overlay_rel)
    try:
        overlay_root.relative_to(private_root)
    except ValueError:
        refuse("RSP overlay checkout escapes its private root")
    if overlay_root.is_symlink() or not overlay_root.is_dir():
        refuse("RSP overlay checkout is missing or linked")
    overlay_head = subprocess.run(["git", "-C", str(overlay_root), "rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()
    overlay_status = subprocess.run(["git", "-C", str(overlay_root), "status", "--porcelain=v1", "--untracked-files=all"], check=True, capture_output=True, text=True).stdout
    if overlay_head != source_revision or overlay_status:
        # A source overlay is intentionally modified; only an untracked path is forbidden.
        untracked = [line for line in overlay_status.splitlines() if line.startswith("??")]
        if overlay_head != source_revision or untracked:
            refuse("RSP overlay source revision changed or contains unreviewed files")
    overlay_diff = subprocess.run(["git", "-C", str(overlay_root), "diff", "--binary"], check=True, capture_output=True).stdout
    if digest(overlay_diff) != overlay_receipt.get("overlay_sha256"):
        refuse("RSP overlay bytes differ from their private preparation receipt")

    overlay_manifest = overlay_receipt.get("overlay_files")
    if not isinstance(overlay_manifest, list) or not overlay_manifest:
        refuse("RSP overlay source manifest is empty or malformed")
    for row in overlay_manifest:
        if not isinstance(row, dict) or set(row) != {"path", "sha256"}:
            refuse("RSP overlay source manifest row is malformed")
        relative = pathlib.PurePosixPath(str(row["path"]))
        if relative.is_absolute() or any(part in ("", ".", "..", ".git") for part in relative.parts):
            refuse("RSP overlay source manifest path escapes its checkout")
        path = overlay_root.joinpath(*relative.parts)
        if path.is_symlink() or not path.is_file() or digest(path.read_bytes()) != row["sha256"]:
            refuse("RSP overlay source bytes differ from their private manifest")

    selected_sources = {row["source_file"] for row in expected}
    rsp_module = overlay_root / "src/tests/rsp/mod.rs"
    module_bytes = rsp_module.read_text(encoding="utf-8")
    if "stresstests" in module_bytes or "stresstests_div" in module_bytes:
        refuse("nondefault unqualified asset stress modules remain compiled in the RSP overlay")
    declared_test_modules = set(re.findall(r"^pub\s+mod\s+([A-Za-z0-9_]+)\s*;", module_bytes, re.MULTILINE))
    declared_paths = {f"src/tests/rsp/{name}.rs" for name in declared_test_modules} | {"src/tests/rsp/mod.rs"}
    if not selected_sources.issubset(declared_paths):
        refuse("RSP source membership refers to a module absent from the compiled overlay")
    active_paths = {row["path"] for row in cpu_closure.get("module_files", []) if isinstance(row, dict) and isinstance(row.get("path"), str)}
    active_paths -= {"src/tests/mod.rs", "src/tests/testlist.rs"}
    active_paths |= {path for path in declared_paths if (overlay_root / path).is_file()}
    active_paths |= {path.relative_to(overlay_root).as_posix() for path in (overlay_root / "src/rsp").glob("*.rs") if path.is_file()}
    active_paths |= {"src/main.rs", "src/tests/mod.rs", "src/tests/testlist.rs", "src/text_out.rs", "src/print.rs", "src/graphics/mod.rs"}
    active_paths = {relative for relative in active_paths if (overlay_root / relative).is_file()}
    for relative in active_paths:
        path = overlay_root / relative
        source = path.read_text(encoding="utf-8", errors="strict").lower()
        if any(token in source for token in ("font", "bitmap", "include_bytes!", "include_str!", "include!", "ipl3", "romdata")):
            refuse("selected RSP module has an unqualified font/data/boot asset reference")
    rsp_runtime = (overlay_root / "src/rsp/mod.rs").read_text(encoding="utf-8")
    runtime_modules = set(re.findall(r"^pub\s+mod\s+([A-Za-z0-9_]+)\s*;", rsp_runtime, re.MULTILINE))
    runtime_paths = {f"src/rsp/{name}.rs" for name in runtime_modules} | {"src/rsp/mod.rs"}
    if not runtime_paths.issubset(active_paths):
        refuse("RSP runtime module closure is incomplete")

    boot_path = ROOT / "build/fixtures/ipl3_compat.z64"
    boot_sha = checked_private_file(boot_path, "audited open IPL3 fixture")
    if boot_sha != "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068":
        refuse("RSP boot fixture differs from the already reviewed open IPL3 bytes")

    graph_inputs = graph.get("inputs")
    vendor_path = CPU_PRIVATE / "vendor-manifest-complete.json"
    project_lock_path = source_root / "Cargo.lock"
    sysroot_lock_path = CPU_PRIVATE / "toolchain-prefix/lib/rustlib/src/rust/library/Cargo.lock"
    if not isinstance(graph_inputs, dict) or graph.get("status") != "PASS_FROZEN_OFFLINE_UNIT_GRAPH":
        refuse("current accepted Cargo unit graph is not sealed")
    for key, path in (("qualification", cpu_receipt_path), ("source_audit", CPU_PRIVATE / "source-audit-full.json"), ("source_projection", CPU_PRIVATE / "source-projection.json")):
        row = graph_inputs.get(key)
        if not isinstance(row, dict) or row.get("sha256") != checked_private_file(path, key):
            refuse(f"accepted CPU graph no longer binds {key}")
    for path, label in ((vendor_path, "reviewed offline vendor partition"), (project_lock_path, "pinned project Cargo.lock"), (sysroot_lock_path, "pinned sysroot Cargo.lock")):
        checked_private_file(path, label)

    profile = {
        "id": "ntsc-cart-4m-openipl3-rsp-functional-v1",
        "region": "NTSC",
        "media": "cartridge",
        "rdram_bytes": 4 * 1024 * 1024,
        "boot_asset_sha256": boot_sha,
        "component": "rsp-functional-diagnostic",
        "expansion": "none",
        "peripherals": "none",
    }
    profile_sha = digest(canonical(profile))
    identity_exe = ROOT / "build/gate/runner/t64-cpu-diagnostic"
    if identity_exe.is_symlink() or not identity_exe.is_file() or not os.access(identity_exe, os.X_OK):
        refuse("current native core identity executable is missing; build all normal targets first")
    identity_bytes_sha = digest(identity_exe.read_bytes())
    identity_result = subprocess.run([str(identity_exe), "--compat-v2", "--runtime-identity-json"], check=True, capture_output=True, text=True)
    core_identity = json.loads(identity_result.stdout)
    if set(core_identity) != {"core_build_sha256", "profile_sha256", "source_revision"} or core_identity["source_revision"] != "d7479bcc854348c1384cc199815954c662ffe9be":
        refuse("current production core build identity is not the reviewed source")
    tools = cpu_receipt["tools"]
    tool_files = []
    for row in [*tools["target_binutils"], tools["host_c_compiler"]]:
        path = pathlib.Path(row["path"]).resolve(strict=True)
        if not path.is_file() or digest(path.read_bytes()) != row["sha256"]:
            refuse("an accepted host or target build tool changed")
        tool_files.append({"name": row["name"] if "name" in row else "host-c-compiler", "path": str(path), "sha256": row["sha256"]})
    runtime_files = [
        {"path": "build/private-cpu-diagnostic/toolchain-prefix/lib/rustlib/aarch64-apple-darwin/bin/rust-lld", "sha256": "6f44b61e91d6d7b6ba80bb75391587bb4fa832b248281bd67d519516cde43f98"},
        {"path": "build/private-cpu-diagnostic/toolchain-prefix/lib/libLLVM.dylib", "sha256": "216146d4f374dea0eb0df654049025f82454aaa147af6d8388e5b37ff54084e1"},
    ]
    for row in runtime_files:
        if checked_private_file(ROOT / row["path"], "Rust linker runtime library") != row["sha256"]:
            refuse("audited Rust linker runtime library changed")

    active_rows = []
    for relative in sorted(active_paths):
        path = overlay_root / relative
        active_rows.append({"path": relative, "sha256": digest(path.read_bytes())})
    closure = {
        "schema": "t64.rsp-preuse-source-closure.v1",
        "component": "rsp",
        "status": "PASS_PREBUILD_RSP",
        "source_revision": source_revision,
        "membership": {"path": projection_path.relative_to(ROOT).as_posix(), "sha256": digest(projection_path.read_bytes()), "registrations": len(expected), "values": sum(len(row["values"]) for row in expected)},
        "source_context": {"cpu_qualification_sha256": digest(cpu_receipt_path.read_bytes()), "source_audit_sha256": checked_private_file(CPU_PRIVATE / "source-audit-full.json", "source audit"), "source_summary_sha256": checked_private_file(CPU_PRIVATE / "source-audit-summary.json", "source summary"), "cpu_source_projection_sha256": checked_private_file(CPU_PRIVATE / "source-projection.json", "source projection"), "cpu_closure_sha256": checked_private_file(cpu_closure_path, "CPU source closure"), "unit_graph_review_sha256": checked_private_file(graph_path, "current unit graph review")},
        "overlay": {"checkout": overlay_rel, "sha256": digest(overlay_diff), "file_count": len(active_rows), "files": active_rows},
        "assets": {"ipl3": {"path": boot_path.relative_to(ROOT).as_posix(), "sha256": boot_sha, "rights_basis": "same audited open IPL3 fixture; no new boot bytes"}, "font_graphics": {"status": "excluded from compiled default RSP closure; third-party source remains unqualified", "excluded_modules": ["src/tests/rsp/stresstests.rs", "src/tests/rsp/stresstests_div.rs"]}},
        "tools": {"cpu_qualification_sha256": digest(cpu_receipt_path.read_bytes()), "tool_files": tool_files, "runtime_files": runtime_files},
        "locks": {"project_cargo_lock_sha256": checked_private_file(project_lock_path, "project Cargo.lock"), "sysroot_cargo_lock_sha256": checked_private_file(sysroot_lock_path, "sysroot Cargo.lock"), "vendor_partition_sha256": checked_private_file(vendor_path, "offline vendor partition"), "unit_graph_review_sha256": checked_private_file(graph_path, "unit graph")},
        "build_identity": {"profile_id": profile["id"], "profile_sha256": profile_sha, "core_model": 6, "core_state_schema": 12, "core_build_sha256": core_identity["core_build_sha256"], "core_identity_source_revision": core_identity["source_revision"], "core_identity_executable_sha256": identity_bytes_sha},
        "selection": {"default_feature_closure": rsp_projection["default_feature_closure"], "source_path_prefix": DEFAULT_RSP_PATH, "feature_excluded_modules": overlay_receipt["excluded_nondefault_stress_modules"]},
    }
    write_private(closure_path, closure)
    qualification = {"schema": "t64.rsp-qualification.v1", "schema_version": 1, "component": "rsp", "status": "qualified", "source_revision": source_revision, "profile_id": profile["id"], "closure": {"path": closure_path.relative_to(ROOT).as_posix(), "sha256": digest(closure_path.read_bytes())}, "membership": closure["membership"], "overlay": closure["overlay"], "build_identity": closure["build_identity"], "locks": closure["locks"], "tools": closure["tools"], "assets": closure["assets"]}
    write_private(qualification_path, qualification)
    print("PASS: independent RSP source, value, tool, lock, boot, and build-identity closure sealed privately")


def capture_core_command(work: pathlib.Path, label: str, argv: list[str], timeout: int) -> dict[str, object]:
    start = time.monotonic()
    stdout_path, stderr_path = work / f"{label}.stdout.log", work / f"{label}.stderr.log"
    with stdout_path.open("xb") as stdout, stderr_path.open("xb") as stderr:
        process = subprocess.Popen(argv, cwd=ROOT, env=os.environ.copy(), stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr, start_new_session=True)
        timed_out = False
        deadline = start + timeout
        while os.waitid(os.P_PID, process.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is None:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                timed_out = True
                break
            time.sleep(min(0.05, remaining))
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            pass
        exit_code = process.wait(timeout=5)
        group_empty = False
        cleanup_deadline = time.monotonic() + 2
        while time.monotonic() < cleanup_deadline:
            try:
                os.killpg(process.pid, 0)
            except ProcessLookupError:
                group_empty = True
                break
            except PermissionError:
                pass
            time.sleep(0.02)
    receipt = {"argv": argv, "exit": exit_code, "timed_out": timed_out, "process_group_empty": group_empty, "elapsed_seconds": round(time.monotonic() - start, 3), "deadline_seconds": timeout, "stdout": stdout_path.name, "stdout_sha256": digest(stdout_path.read_bytes()), "stderr": stderr_path.name, "stderr_sha256": digest(stderr_path.read_bytes())}
    write_private(work / f"{label}.command.json", receipt)
    if timed_out or exit_code != 0 or not group_empty:
        refuse(f"current core identity verification command failed: {label}")
    return receipt


def bind_current_core() -> None:
    private_root = owner_only(ROOT / "build/private-rsp-diagnostic")
    qualification_path = private_root / "qualification.json"
    binding_path = private_root / "current-core-identity-binding-v2.json"
    prior_binding_path = private_root / "current-core-identity-binding.json"
    if binding_path.exists() or binding_path.is_symlink():
        refuse("refusing to overwrite current core identity binding")
    cpu_verifier = import_script(ROOT / "scripts/verify-cpu-diagnostic-qualification.py", "t64_cpu_rsp_core_binding")
    cpu_verifier.validate(CPU_PRIVATE / "qualification.json", ROOT)
    qualification = read_json(qualification_path, "RSP qualification receipt")
    original_identity = qualification.get("build_identity")
    if not isinstance(original_identity, dict):
        refuse("RSP qualification does not contain its original core identity")
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()
    if subprocess.run(["git", "merge-base", "--is-ancestor", CORE_BASE_REVISION, head], cwd=ROOT, check=False).returncode != 0:
        refuse("current plan source is not based on the reviewed production core")
    core_paths = subprocess.run(["git", "ls-files", "core", "runner/t64_cpu_diagnostic.c", "runner/t64_cpu_diag_protocol.c", "runner/t64_cpu_diag_protocol.h", "runner/t64_verified_input.c", "runner/t64_verified_input.h", "tools/common"], cwd=ROOT, check=True, capture_output=True, text=True).stdout.splitlines()
    if not core_paths:
        refuse("current core source scope is empty")
    source_rows = []
    for relative in core_paths:
        if relative.endswith("CMakeLists.txt"):
            continue
        current = ROOT / relative
        baseline = subprocess.run(["git", "show", f"{CORE_BASE_REVISION}:{relative}"], cwd=ROOT, check=True, capture_output=True).stdout
        current_bytes = current.read_bytes()
        if current_bytes != baseline:
            refuse(f"core identity source scope changed from the reviewed production baseline: {relative}")
        source_rows.append({"path": relative, "sha256": digest(current_bytes)})

    work = pathlib.Path(tempfile.mkdtemp(prefix="core-identity-rebind-", dir=private_root))
    work.chmod(0o700)
    configure = capture_core_command(work, "cmake-configure", ["cmake", "--preset", "gate"], 120)
    build = capture_core_command(work, "cmake-build-all", ["cmake", "--build", "--preset", "gate"], 600)
    inventory = capture_core_command(work, "gate-inventory", ["sh", "scripts/check-test-inventory.sh", "--preset", "gate"], 120)
    identity_exe = ROOT / "build/gate/runner/t64-cpu-diagnostic"
    identity_sha = checked_private_file(identity_exe, "rebuilt current core identity executable")
    identity_result = subprocess.run([str(identity_exe), "--compat-v2", "--runtime-identity-json"], cwd=ROOT, check=True, capture_output=True, text=True)
    current_identity = json.loads(identity_result.stdout)
    if current_identity.get("core_build_sha256") != original_identity.get("core_build_sha256") or current_identity.get("source_revision") != head:
        refuse("rebuilt local identity reader does not bind the unchanged reviewed core to the current plan source")
    if identity_sha == original_identity.get("core_identity_executable_sha256"):
        refuse("current core identity binding is unnecessary because the binary did not change")
    cmake_cache = ROOT / "build/gate/CMakeCache.txt"
    cmake_cache_sha = checked_private_file(cmake_cache, "current gate CMake cache")
    command_receipt_hashes = {"configure": digest((work / "cmake-configure.command.json").read_bytes()), "all_targets_build": digest((work / "cmake-build-all.command.json").read_bytes()), "gate_inventory": digest((work / "gate-inventory.command.json").read_bytes())}

    binding = {
        "schema": "t64.rsp-core-identity-binding.v2",
        "status": "verified-current-core-artifact",
        "component": "rsp",
        "qualification_sha256": digest(qualification_path.read_bytes()),
        "supersedes_binding": {"path": prior_binding_path.relative_to(ROOT).as_posix(), "sha256": checked_private_file(prior_binding_path, "prior current core binding")} if prior_binding_path.exists() else None,
        "profile_id": original_identity["profile_id"],
        "core_model": original_identity["core_model"],
        "core_state_schema": original_identity["core_state_schema"],
        "original_identity": {"executable_sha256": original_identity["core_identity_executable_sha256"], "core_build_sha256": original_identity["core_build_sha256"], "source_revision": original_identity["core_identity_source_revision"]},
        "current_identity": {"path": identity_exe.relative_to(ROOT).as_posix(), "executable_sha256": identity_sha, "core_build_sha256": current_identity["core_build_sha256"], "source_revision": current_identity["source_revision"]},
        "production_core_baseline_revision": CORE_BASE_REVISION,
        "core_source_scope": source_rows,
        "cmake_cache_sha256": cmake_cache_sha,
        "evidence_dir": work.relative_to(ROOT).as_posix(),
        "commands": {"configure": configure, "all_targets_build": build, "gate_inventory": inventory},
        "command_receipt_sha256": command_receipt_hashes,
        "verification_scope": "All gate targets configured and built; CTest inventory checked; core/identity-reader source bytes match the reviewed production baseline; adapter was used only to read build identity, not to execute a ROM or guest.",
    }
    write_private(binding_path, binding)
    print("PASS: rebuilt current core identity bound to the original RSP qualification and verified all-target build evidence")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--workspace", type=pathlib.Path)
    parser.add_argument("--source-root", type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("--prepare-overlay", action="store_true")
    parser.add_argument("--projection", type=pathlib.Path)
    parser.add_argument("--overlay-receipt", type=pathlib.Path)
    parser.add_argument("--seal-preuse", action="store_true")
    parser.add_argument("--bind-current-core", action="store_true")
    args = parser.parse_args()
    try:
        if args.selftest:
            if args.workspace is None or args.source_root is not None or args.output is not None:
                refuse("authored selftest requires only --workspace")
            selftest(args.workspace)
        elif args.prepare_overlay:
            if args.source_root is None or args.projection is None or args.output is None or args.overlay_receipt is None or args.workspace is not None:
                refuse("overlay preparation requires --source-root, --projection, --output, and --overlay-receipt")
            prepare_overlay(args.source_root, args.projection, args.output, args.overlay_receipt)
        elif args.source_root is not None:
            if args.output is None or args.workspace is not None:
                refuse("source preparation requires --source-root and --output")
            prepare_source(args.source_root, args.output)
        elif args.seal_preuse:
            if args.workspace is not None or args.source_root is not None or args.output is not None or args.projection is not None or args.overlay_receipt is not None:
                refuse("pre-use sealing requires no other arguments")
            seal_preuse()
        elif args.bind_current_core:
            if args.workspace is not None or args.source_root is not None or args.output is not None or args.projection is not None or args.overlay_receipt is not None:
                refuse("core identity rebinding requires no other arguments")
            bind_current_core()
        else:
            refuse("select --selftest or --source-root")
    except (OSError, Refusal, subprocess.SubprocessError, tomllib.TOMLDecodeError, ValueError) as error:
        print(f"refused: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
