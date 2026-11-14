#!/usr/bin/env python3
"""Run privately sealed RSP selectors through the local bounded adapter."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import selectors
import signal
import stat
import subprocess
import sys
import time
from datetime import datetime, timezone

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "build/private-rsp-diagnostic"
PROFILE_ID = "ntsc-cart-4m-openipl3-rsp-functional-v1"
CORE_BINDING = PRIVATE / "current-core-identity-binding-v3.json"
FUTURE_EXECUTION_CONTEXT = PRIVATE / "current-rsp-future-execution-context-v1.json"
INVENTORY_LIMIT = 1_048_576
ROM_LIMIT = 0x00401000
TICK_LIMIT = 10_000_000
STEP_TICKS = 16
CHILD_SECONDS = 10
OUTER_SECONDS = 12
CAPTURE_BYTES = 4096
ADAPTER_LIMIT = 512 * 1024 * 1024
HEX40 = re.compile(r"[0-9a-f]{40}\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
SELECTOR = re.compile(r"r([0-9]{4})v([0-9]{5})\Z")


class Refusal(ValueError):
    pass


def fail(message: str) -> None:
    raise Refusal(message)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def no_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            fail("metadata contains a duplicate JSON key")
        result[key] = value
    return result


def parse_json(data: bytes, label: str):
    try:
        return json.loads(data, object_pairs_hook=no_duplicate_keys)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        fail(f"{label} is malformed JSON: {error}")


def closed_object(value, keys: set[str], label: str) -> dict:
    if not isinstance(value, dict) or set(value) != keys:
        fail(f"{label} fields do not match the closed schema")
    return value


def checked_file(path: pathlib.Path, maximum: int, label: str,
                 *, private_root: pathlib.Path | None = None) -> bytes:
    path = pathlib.Path(path)
    if path.is_symlink() or not path.is_file():
        fail(f"{label} is missing or linked")
    if private_root is not None:
        try:
            path.resolve(strict=True).relative_to(private_root.resolve(strict=True))
        except (OSError, ValueError):
            fail(f"{label} escapes the private root")
    info = path.stat()
    if info.st_size <= 0 or info.st_size > maximum:
        fail(f"{label} is empty or oversized")
    data = path.read_bytes()
    if len(data) != info.st_size:
        fail(f"{label} changed while reading")
    return data


def read_json_file(path: pathlib.Path, maximum: int, label: str,
                   *, private_root: pathlib.Path | None = None) -> tuple[dict, bytes]:
    data = checked_file(path, maximum, label, private_root=private_root)
    value = parse_json(data, label)
    if not isinstance(value, dict):
        fail(f"{label} is not an object")
    return value, data


def write_new(path: pathlib.Path, data: bytes, mode: int = 0o600) -> None:
    if path.exists() or path.is_symlink():
        fail(f"refusing to overwrite private evidence: {path.name}")
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL |
                 getattr(os, "O_NOFOLLOW", 0), mode)
    with os.fdopen(fd, "wb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())


def write_json(path: pathlib.Path, value: object) -> None:
    data = (json.dumps(value, sort_keys=True, separators=(",", ":"),
                       ensure_ascii=False) + "\n").encode("utf-8")
    write_new(path, data)


def private_directory(path: pathlib.Path, *, create: bool = False) -> pathlib.Path:
    if PRIVATE.is_symlink() or not PRIVATE.is_dir() or stat.S_IMODE(PRIVATE.stat().st_mode) != 0o700:
        fail("owner-only RSP private root is unavailable")
    candidate = pathlib.Path(os.path.abspath(path))
    try:
        relative = candidate.relative_to(PRIVATE)
    except ValueError:
        fail("run directory must stay below the owner-only RSP private root")
    cursor = PRIVATE
    for part in relative.parts:
        if part in ("", ".", ".."):
            fail("run directory path is not canonical")
        cursor = cursor / part
        if cursor.exists() or cursor.is_symlink():
            if cursor.is_symlink() or not cursor.is_dir():
                fail("run directory traverses a non-directory or link")
            if stat.S_IMODE(cursor.stat().st_mode) != 0o700:
                fail("run directory is not owner-only")
        elif create:
            cursor.mkdir(mode=0o700)
    if create and not candidate.exists():
        candidate.mkdir(mode=0o700)
    return candidate


def adapter_path(path: pathlib.Path) -> pathlib.Path:
    expected = ROOT / "build/gate/runner/t64-rsp-diagnostic"
    candidate = pathlib.Path(os.path.abspath(path))
    if candidate != expected or candidate.is_symlink() or not candidate.is_file() or not os.access(candidate, os.X_OK):
        fail("adapter path is not the built local RSP diagnostic executable")
    info = candidate.stat()
    if info.st_size <= 0 or info.st_size > ADAPTER_LIMIT:
        fail("RSP adapter is empty or oversized")
    return candidate


def adapter_hash(path: pathlib.Path) -> str:
    return digest(checked_file(path, ADAPTER_LIMIT, "RSP adapter"))


def run_capture(command: list[str], *, timeout: float,
                maximum: int = 65_536) -> tuple[int, bytes, bytes]:
    try:
        result = subprocess.run(command, cwd=ROOT, stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                timeout=timeout, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        fail(f"bounded pre-use command failed: {error}")
    if len(result.stdout) + len(result.stderr) > maximum:
        fail("bounded pre-use command exceeded its capture limit")
    return result.returncode, result.stdout, result.stderr


def verify_source_baseline(binding: dict) -> None:
    current = closed_object(
        binding.get("current_identity"),
        {"core_build_sha256", "executable_sha256", "path", "source_revision"},
        "current core binding identity")
    baseline = binding.get("production_core_baseline_revision")
    rows = binding.get("core_source_scope")
    if not isinstance(baseline, str) or not HEX40.fullmatch(baseline) or not isinstance(rows, list) or not rows:
        fail("current core binding source baseline is malformed")
    head_result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                 stdin=subprocess.DEVNULL, capture_output=True,
                                 text=True, timeout=10, check=True)
    head = head_result.stdout.strip()
    if current["source_revision"] != head:
        fail("current core binding source revision differs from the built checkout")
    if current["path"] != "build/gate/runner/t64-cpu-diagnostic" or not HEX64.fullmatch(current["core_build_sha256"]):
        fail("current core binding executable identity is malformed")
    reader_path = ROOT / current["path"]
    if digest(checked_file(reader_path, ADAPTER_LIMIT, "bound core identity reader")) != current.get("executable_sha256"):
        fail("current production identity reader bytes differ from the explicit binding")
    supersedes = closed_object(binding.get("supersedes_binding"), {"path", "sha256"}, "superseded core binding")
    if supersedes["path"] != "build/private-rsp-diagnostic/current-core-identity-binding-v2.json":
        fail("current RSP binding does not supersede the immutable v2 binding")
    old_binding = checked_file(ROOT / supersedes["path"], 2 * 1024 * 1024,
                               "immutable v2 core binding", private_root=PRIVATE)
    if digest(old_binding) != supersedes["sha256"]:
        fail("immutable v2 core binding changed")
    for row in rows:
        row = closed_object(row, {"path", "sha256"}, "core source binding row")
        relative = pathlib.PurePosixPath(row["path"])
        if relative.is_absolute() or any(part in ("", ".", "..", ".git") for part in relative.parts) or not HEX64.fullmatch(row["sha256"]):
            fail("core source binding path or digest is malformed")
        path = ROOT.joinpath(*relative.parts)
        data = checked_file(path, 64 * 1024 * 1024, "core source file")
        if digest(data) != row["sha256"]:
            fail("current production core source differs from its bound bytes")
        baseline_result = subprocess.run(
            ["git", "show", f"{baseline}:{relative.as_posix()}"], cwd=ROOT,
            stdin=subprocess.DEVNULL, capture_output=True, timeout=10, check=True)
        if digest(baseline_result.stdout) != row["sha256"]:
            fail("current core binding does not match the reviewed production baseline")
    extraction = closed_object(
        binding.get("cpu_protocol_extraction"),
        {"baseline_sha256", "commit", "current_sha256", "equivalence_evidence", "path", "status"},
        "CPU protocol extraction binding")
    if extraction["path"] != "runner/t64_cpu_diag_protocol.c" or extraction["status"] != "tested-canonical-output-preserving-shared-helper-extraction":
        fail("CPU protocol extraction exception is not the exact tested shared-helper change")
    source_path = ROOT / extraction["path"]
    source = checked_file(source_path, 4 * 1024 * 1024, "bound CPU protocol source")
    if digest(source) != extraction["current_sha256"]:
        fail("bound CPU protocol source bytes changed")
    commit_source = subprocess.run(
        ["git", "show", f"{extraction['commit']}:{extraction['path']}"],
        cwd=ROOT, stdin=subprocess.DEVNULL, capture_output=True,
        timeout=10, check=True).stdout
    if source != commit_source:
        fail("CPU protocol source differs from its reviewed extraction commit")
    baseline_rows = [line.split() for line in
                     (PRIVATE / "plan04/cpu-protocol-source-baseline.sha256").read_text().splitlines()
                     if line.endswith(" " + extraction["path"])]
    if len(baseline_rows) != 1 or baseline_rows[0][0] != extraction["baseline_sha256"]:
        fail("CPU protocol pre-extraction baseline evidence changed")
    evidence = extraction["equivalence_evidence"]
    if not isinstance(evidence, list) or len(evidence) != 2:
        fail("CPU protocol equivalence evidence is incomplete")
    for record in evidence:
        record = closed_object(record, {"path", "sha256"}, "CPU protocol equivalence report")
        report = checked_file(ROOT / record["path"], 65_536,
                              "CPU protocol equivalence report", private_root=PRIVATE)
        if digest(report) != record["sha256"] or b"3/3 Test" not in report or b"Total Test time" not in report:
            fail("CPU protocol equivalence report is stale or incomplete")
    adapter_record = closed_object(binding.get("rsp_adapter_identity"),
                                   {"executable_sha256", "path"},
                                   "RSP execution adapter binding")
    if adapter_record["path"] != "build/gate/runner/t64-rsp-diagnostic" or adapter_record["executable_sha256"] != adapter_hash(ROOT / adapter_record["path"]):
        fail("RSP adapter executable differs from the explicit execution binding")


def verify_future_execution_context(adapter: pathlib.Path,
                                    runtime: dict) -> None:
    context, context_bytes = read_json_file(
        FUTURE_EXECUTION_CONTEXT, 1_048_576,
        "future RSP execution context", private_root=PRIVATE)
    if stat.S_IMODE(FUTURE_EXECUTION_CONTEXT.stat().st_mode) != 0o600:
        fail("future RSP execution context is not owner-only")
    expected = {
        "authorized_for_guest_execution", "component", "current_cpu_identity_reader",
        "current_normal_build", "current_rsp_adapter", "current_runtime_identity", "guest_execution_performed",
        "historical_first_use", "model6_ctc1_fix", "qualification_sha256",
        "qualified_core_build_sha256", "qualified_rom_sha256", "reason",
        "schema", "source_artifacts", "status", "verification_evidence",
        "verification_scope",
    }
    context = closed_object(context, expected, "future RSP execution context")
    if (context["schema"] != "t64.rsp-future-execution-context.v1" or
            context["status"] != "future-requalification-required" or
            context["component"] != "rsp" or
            context["authorized_for_guest_execution"] is not False or
            context["guest_execution_performed"] is not False):
        fail("future RSP context is malformed or incorrectly authorizes execution")
    qualification_bytes = checked_file(
        PRIVATE / "qualification.json", 1_048_576,
        "original RSP qualification", private_root=PRIVATE)
    qualification = parse_json(qualification_bytes, "original RSP qualification")
    if digest(qualification_bytes) != context["qualification_sha256"]:
        fail("future RSP context no longer matches the preserved qualification")
    first_use_bytes = checked_file(
        PRIVATE / "first-use-plan04-03/first-use-receipt.json", 1_048_576,
        "historical first-use receipt", private_root=PRIVATE)
    first_use = parse_json(first_use_bytes, "historical first-use receipt")
    if (qualification.get("build_identity", {}).get("core_build_sha256") !=
            context["qualified_core_build_sha256"] or
            first_use.get("base_rom", {}).get("sha256") !=
            context["qualified_rom_sha256"] or
            context["qualified_core_build_sha256"] == runtime.get("core_build_sha256")):
        fail("future RSP context does not distinguish the current core from the qualified build")
    runtime = closed_object(runtime, {
        "core_build_sha256", "core_source_revision",
        "profile_descriptor_sha256", "profile_id",
    }, "post-fix RSP runtime identity")
    if runtime != context["current_runtime_identity"] or runtime["profile_id"] != PROFILE_ID:
        fail("post-fix RSP runtime differs from the future context")
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                          stdin=subprocess.DEVNULL, capture_output=True,
                          text=True, timeout=10, check=True).stdout.strip()
    if runtime["core_source_revision"] != head:
        fail("post-fix adapter was not rebuilt from the current committed source")

    normal_build = closed_object(context["current_normal_build"],
                                 {"path", "sha256"},
                                 "post-fix normal build evidence")
    if normal_build["path"] != "build/private-rsp-diagnostic/plan04/task3-postcommit-gate-build.log":
        fail("post-fix normal build evidence path is unexpected")
    normal_build_bytes = checked_file(ROOT / normal_build["path"], 65_536,
                                      "post-fix normal build evidence",
                                      private_root=PRIVATE)
    if (digest(normal_build_bytes) != normal_build["sha256"] or
            b"t64-rsp-diagnostic" not in normal_build_bytes):
        fail("post-fix normal build does not bind the current RSP adapter target")

    adapter_record = closed_object(context["current_rsp_adapter"],
                                   {"executable_sha256", "path"},
                                   "post-fix RSP adapter")
    if (adapter_record["path"] != adapter.relative_to(ROOT).as_posix() or
            adapter_hash(adapter) != adapter_record["executable_sha256"]):
        fail("post-fix RSP adapter bytes differ from the future context")
    cpu_record = closed_object(context["current_cpu_identity_reader"],
                               {"executable_sha256", "path"},
                               "post-fix CPU identity reader")
    cpu_path = ROOT / cpu_record["path"]
    if (cpu_record["path"] != "build/gate/runner/t64-cpu-diagnostic" or
            adapter_hash(cpu_path) != cpu_record["executable_sha256"]):
        fail("post-fix CPU identity reader bytes differ from the future context")

    fix = closed_object(context["model6_ctc1_fix"],
                        {"commit", "regression_sha256", "source_sha256", "subject"},
                        "model6 CTC1 dispatch fix")
    fix_commit = subprocess.run(
        ["git", "rev-parse", f"{fix['commit']}^{{commit}}"], cwd=ROOT,
        stdin=subprocess.DEVNULL, capture_output=True, text=True,
        timeout=10, check=True).stdout.strip()
    subject = subprocess.run(
        ["git", "show", "-s", "--format=%s", fix_commit], cwd=ROOT,
        stdin=subprocess.DEVNULL, capture_output=True, text=True,
        timeout=10, check=True).stdout.strip()
    if (subject != fix["subject"] or
            subject != "fix(05-04): dispatch model6 CTC1 through VR4300 core" or
            digest(checked_file(ROOT / "core/t64_internal.h", 4_194_304,
                                "model6 CTC1 predicate")) != fix["source_sha256"] or
            digest(checked_file(ROOT / "core/tests/test_cop1.c", 4_194_304,
                                "model6 CTC1 regression")) != fix["regression_sha256"]):
        fail("bounded model6 CTC1 fix source differs from the future context")

    source_rows = context["source_artifacts"]
    if not isinstance(source_rows, list) or not source_rows:
        fail("future RSP source artifact list is empty")
    seen_sources: set[str] = set()
    for row in source_rows:
        row = closed_object(row, {"path", "sha256"}, "future RSP source artifact")
        relative = pathlib.PurePosixPath(row["path"])
        if (relative.is_absolute() or any(part in ("", ".", "..", ".git")
                                          for part in relative.parts) or
                row["path"] in seen_sources or not HEX64.fullmatch(row["sha256"])):
            fail("future RSP source artifact path or digest is malformed")
        seen_sources.add(row["path"])
        if digest(checked_file(ROOT.joinpath(*relative.parts), 64 * 1024 * 1024,
                               "future RSP source artifact")) != row["sha256"]:
            fail("future RSP source artifact bytes differ from the recorded context")

    history = closed_object(context["historical_first_use"], {
        "adapter_exit", "classification", "historical_execution_identity",
        "historical_files", "original_launcher_failure_preserved", "outcome",
        "status",
    }, "historical RSP first use")
    if (history["status"] != "selector-bound-refusal-authenticated" or
            history["outcome"] != "refused" or
            history["classification"] != "refused" or
            history["adapter_exit"] != 2 or
            history["original_launcher_failure_preserved"] is not True):
        fail("pre-fix RSP refusal or original launcher failure was rewritten")
    historical_files = history["historical_files"]
    if not isinstance(historical_files, list) or not historical_files:
        fail("historical RSP run evidence list is empty")
    seen_history: set[str] = set()
    for row in historical_files:
        row = closed_object(row, {"path", "sha256"}, "historical RSP evidence")
        relative = pathlib.PurePosixPath(row["path"])
        if (relative.is_absolute() or any(part in ("", ".", "..", ".git")
                                          for part in relative.parts) or
                row["path"] in seen_history or not HEX64.fullmatch(row["sha256"])):
            fail("historical RSP evidence path or digest is malformed")
        seen_history.add(row["path"])
        if digest(checked_file(ROOT.joinpath(*relative.parts), 64 * 1024 * 1024,
                               "historical RSP evidence", private_root=PRIVATE)) != row["sha256"]:
            fail("historical RSP run evidence was changed")

    evidence_rows = context["verification_evidence"]
    if not isinstance(evidence_rows, list) or not evidence_rows:
        fail("future RSP context lacks saved native evidence")
    for row in evidence_rows:
        row = closed_object(row, {"path", "sha256"}, "future RSP verification evidence")
        data = checked_file(ROOT / row["path"], 65_536,
                            "future RSP verification evidence", private_root=PRIVATE)
        if digest(data) != row["sha256"]:
            fail("future RSP verification evidence bytes changed")
    del context_bytes


def verify_runtime_binding(adapter: pathlib.Path, runtime: dict,
                           run_dir: pathlib.Path) -> dict:
    if FUTURE_EXECUTION_CONTEXT.exists() and not FUTURE_EXECUTION_CONTEXT.is_symlink():
        verify_future_execution_context(adapter, runtime)
        fail("future execution context is valid, but the preserved qualification predates the current core; requalification is required before guest execution")
    binding, binding_bytes = read_json_file(CORE_BINDING, 2 * 1024 * 1024,
                                            "current core identity binding",
                                            private_root=PRIVATE)
    if binding.get("schema") != "t64.rsp-core-identity-binding.v3" or binding.get("status") != "verified-current-core-artifact" or binding.get("component") != "rsp":
        fail("a current v3 RSP core identity binding is required before execution")
    qualification, qualification_bytes = read_json_file(
        PRIVATE / "qualification.json", 1_048_576,
        "RSP qualification receipt", private_root=PRIVATE)
    if binding.get("qualification_sha256") != digest(qualification_bytes):
        fail("current core identity binding does not descend from this RSP qualification")
    if binding.get("profile_id") != PROFILE_ID or binding.get("core_model") != 6 or binding.get("core_state_schema") != 12:
        fail("current core binding profile/model differs from the RSP qualification")
    verify_source_baseline(binding)
    current = binding["current_identity"]
    required = {"core_build_sha256", "core_source_revision", "profile_descriptor_sha256", "profile_id"}
    runtime = closed_object(runtime, required, "RSP adapter runtime identity")
    if runtime["profile_id"] != PROFILE_ID or runtime["core_build_sha256"] != current["core_build_sha256"] or runtime["core_source_revision"] != current["source_revision"]:
        fail("RSP adapter runtime identity differs from the explicitly rebound production core")
    if not HEX64.fullmatch(runtime["profile_descriptor_sha256"]):
        fail("RSP runtime profile descriptor hash is malformed")
    if binding.get("rsp_runtime_identity") != runtime:
        fail("RSP runtime identity differs from the v3 explicit execution binding")
    if not isinstance(binding.get("rsp_adapter_identity"), dict) or binding["rsp_adapter_identity"].get("executable_sha256") != adapter_hash(adapter):
        fail("RSP adapter bytes differ from the v3 explicit execution binding")
    if qualification.get("build_identity", {}).get("core_build_sha256") != runtime["core_build_sha256"]:
        fail("RSP runtime core build differs from the qualified source build")

    identity_path = PRIVATE / "plan04" / "runtime-adapter-binding.json"
    record = {
        "adapter_path": adapter.relative_to(ROOT).as_posix(),
        "adapter_sha256": adapter_hash(adapter),
        "core_binding_path": CORE_BINDING.relative_to(ROOT).as_posix(),
        "core_binding_sha256": digest(binding_bytes),
        "core_build_sha256": runtime["core_build_sha256"],
        "core_source_revision": runtime["core_source_revision"],
        "profile_descriptor_sha256": runtime["profile_descriptor_sha256"],
        "profile_id": PROFILE_ID,
        "schema": "t64.rsp-current-execution-binding.v1",
        "source_baseline_revision": binding["production_core_baseline_revision"],
        "status": "verified-current-rsp-execution-adapter",
    }
    if not identity_path.exists():
        write_json(identity_path, record)
    else:
        saved, _ = read_json_file(identity_path, 65_536,
                                  "existing RSP execution binding",
                                  private_root=PRIVATE)
        if saved != record:
            fail("existing RSP execution binding differs from the current adapter")
    return record


def verify_preuse(run_dir: pathlib.Path) -> tuple[dict, dict, dict, bytes, pathlib.Path]:
    qualification, qualification_bytes = read_json_file(
        PRIVATE / "qualification.json", 1_048_576,
        "RSP qualification receipt", private_root=PRIVATE)
    if qualification.get("schema") != "t64.rsp-qualification.v1" or qualification.get("component") != "rsp" or qualification.get("status") != "qualified" or qualification.get("profile_id") != PROFILE_ID:
        fail("RSP qualification receipt is not accepted for this profile")
    closure_ref = closed_object(qualification.get("closure"), {"path", "sha256"}, "RSP closure reference")
    closure_path = PRIVATE / "rsp-preuse-closure.json"
    if closure_ref["path"] != closure_path.relative_to(ROOT).as_posix():
        fail("RSP qualification selects a different source closure")
    closure_bytes = checked_file(closure_path, 16 * 1024 * 1024,
                                 "RSP pre-use closure", private_root=PRIVATE)
    if digest(closure_bytes) != closure_ref["sha256"]:
        fail("RSP pre-use closure digest changed")
    closure = parse_json(closure_bytes, "RSP pre-use closure")
    if closure.get("schema") != "t64.rsp-preuse-source-closure.v1" or closure.get("component") != "rsp" or closure.get("status") != "PASS_PREBUILD_RSP":
        fail("RSP source closure is not accepted")
    if any(qualification.get(key) != closure.get(key) for key in
           ("membership", "overlay", "build_identity", "locks", "tools", "assets")):
        fail("RSP qualification fields differ from its sealed closure")
    projection_path = PRIVATE / "rsp-source-projection.json"
    projection_bytes = checked_file(projection_path, 16 * 1024 * 1024,
                                     "RSP source projection", private_root=PRIVATE)
    if digest(projection_bytes) != qualification.get("membership", {}).get("sha256"):
        fail("RSP source projection digest differs from its pre-use seal")
    projection = parse_json(projection_bytes, "RSP source projection")
    if projection.get("component") != "rsp" or projection.get("status") != "membership-prepared" or projection.get("source_revision") != qualification.get("source_revision"):
        fail("RSP source projection is not bound to the qualified source revision")

    build, build_bytes = read_json_file(PRIVATE / "qualified-build.json", 1_048_576,
                                        "qualified RSP build receipt",
                                        private_root=PRIVATE)
    if build.get("schema") != "t64.rsp-qualified-build.v1" or build.get("status") != "qualified-build-composed" or build.get("component") != "rsp" or build.get("guest_rom_executed") is not False:
        fail("qualified RSP build receipt is missing or already consumed")
    if build.get("qualification_sha256") != digest(qualification_bytes) or build.get("preuse_closure_sha256") != closure_ref["sha256"] or build.get("source_revision") != qualification.get("source_revision"):
        fail("qualified RSP build receipt identity differs from pre-use")
    artifacts = closed_object(build.get("artifacts"), {"composition", "elf", "payload", "rom"}, "RSP build artifacts")
    rom_record = closed_object(artifacts["rom"], {"path", "sha256", "size"}, "RSP base ROM artifact")
    rom_path = ROOT / pathlib.PurePosixPath(rom_record["path"])
    rom_bytes = checked_file(rom_path, ROM_LIMIT, "qualified RSP ROM", private_root=PRIVATE)
    if len(rom_bytes) != rom_record["size"] or digest(rom_bytes) != rom_record["sha256"]:
        fail("qualified RSP ROM bytes differ from their build seal")
    composition_record = closed_object(artifacts["composition"], {"path", "sha256", "size"}, "RSP composition artifact")
    composition_path = ROOT / pathlib.PurePosixPath(composition_record["path"])
    composition_bytes = checked_file(composition_path, 1_048_576,
                                     "RSP composition report", private_root=PRIVATE)
    if digest(composition_bytes) != composition_record["sha256"]:
        fail("RSP composition report bytes differ from the build receipt")
    composition = parse_json(composition_bytes, "RSP composition report")
    if composition.get("status") != "PASS_POSTBUILD_COMPOSITION" or composition.get("component") != "rsp" or composition.get("rom_sha256") != rom_record["sha256"] or composition.get("qualification_sha256") != build.get("qualification_sha256"):
        fail("RSP post-build composition is not bound to this ROM and qualification")
    layout = composition.get("layout")
    if not isinstance(layout, dict) or layout.get("selector_bytes") != 4 or composition.get("selector_bytes") != 4 or composition.get("selector_symbol") != "T64_RSP_SELECTOR":
        fail("RSP composition has no bounded four-byte selector")
    try:
        entry = int(layout["entry"], 16)
        selector_address = int(layout["selector_address"], 16)
        loaded_bytes = layout["loaded_bytes"]
    except (KeyError, TypeError, ValueError):
        fail("RSP composition selector address is malformed")
    if entry != 0xFFFFFFFF80000400 or type(loaded_bytes) is not int or loaded_bytes != len(rom_bytes) - 4096 or selector_address % 4 != 0 or not entry <= selector_address <= entry + loaded_bytes - 4:
        fail("RSP composition selector address is outside the sealed loaded payload")
    # N64 images contain a 4 KiB header/boot prefix followed by the inspected
    # payload; derive the file offset from the authenticated symbol address.
    layout["selector_rom_offset"] = 0x1000 + selector_address - entry
    if layout["selector_rom_offset"] + 4 > len(rom_bytes) or rom_bytes[layout["selector_rom_offset"]:layout["selector_rom_offset"] + 4] != bytes(4):
        fail("qualified RSP base ROM selector is not zero-filled")
    return qualification, projection, {**build, "composition": composition}, rom_bytes, rom_path


def build_inventory(qualification: dict, projection: dict, build: dict,
                    base_rom: bytes, runtime: dict) -> tuple[dict, bytes]:
    rows = projection.get("registrations")
    if not isinstance(rows, list) or not rows:
        fail("sealed RSP source projection is empty")
    layout = build["composition"]["layout"]
    offset = layout["selector_rom_offset"]
    count_values = 0
    registrations = []
    for source in rows:
        if not isinstance(source, dict):
            fail("source projection registration is malformed")
        index = source.get("index")
        values = source.get("values")
        if type(index) is not int or not 0 <= index <= 0xffff or not isinstance(values, list) or not values:
            fail("source projection registration index or value set is malformed")
        selected_values = []
        for value_index, source_value in enumerate(values):
            if not isinstance(source_value, dict) or source_value.get("index") != value_index:
                fail("source projection value ordering is malformed")
            selector = source_value.get("selector")
            match = SELECTOR.fullmatch(selector) if isinstance(selector, str) else None
            expected_selector = f"r{index:04d}v{value_index:05d}"
            expected_id = "once" if source.get("source_value_count") == 0 else f"v{value_index:05d}"
            if match is None or selector != expected_selector or source_value.get("id") != expected_id:
                fail("source projection selector or value id differs from exact membership")
            variant = bytearray(base_rom)
            variant[offset:offset + 4] = index.to_bytes(2, "big") + value_index.to_bytes(2, "big")
            selected_values.append({
                "id": source_value["id"],
                "index": value_index,
                "selected_rom_sha256": digest(variant),
                "selector": selector,
            })
            count_values += 1
        registrations.append({
            key: source[key]
            for key in ("constructor", "execution", "group", "index", "lane",
                        "level", "name", "source_file", "source_file_sha256",
                        "source_name", "source_value_count", "values_method_sha256")
        } | {"values": selected_values})
    identity = {
        "boot_sha256": qualification["assets"]["ipl3"]["sha256"],
        "closure_sha256": qualification["closure"]["sha256"],
        "core_build_sha256": runtime["core_build_sha256"],
        "core_source_revision": runtime["core_source_revision"],
        "overlay_sha256": qualification["overlay"]["sha256"],
        "profile_descriptor_sha256": runtime["profile_descriptor_sha256"],
        "profile_id": PROFILE_ID,
        "qualification_profile_sha256": qualification["build_identity"]["profile_sha256"],
        "qualification_sha256": digest((PRIVATE / "qualification.json").read_bytes()),
        "rom_sha256": build["artifacts"]["rom"]["sha256"],
        "source_projection_sha256": digest((PRIVATE / "rsp-source-projection.json").read_bytes()),
        "source_revision": qualification["source_revision"],
    }
    inventory = {
        "component": "rsp",
        "declared_counts": {"registrations": len(registrations),
                            "required_values": count_values,
                            "values": count_values},
        "identity": identity,
        "registrations": registrations,
        "schema": "t64.rsp.inventory",
        "schema_version": 1,
    }
    encoded = (json.dumps(inventory, sort_keys=True, separators=(",", ":"),
                          ensure_ascii=False) + "\n").encode("utf-8")
    if len(encoded) > INVENTORY_LIMIT:
        fail("RSP source projection inventory exceeds its byte cap")
    return inventory, encoded


def write_selector_rom(run_dir: pathlib.Path, base_rom: bytes, offset: int,
                       member: dict) -> pathlib.Path:
    registration = member["registration_index"]
    value = member["value_index"]
    variant = bytearray(base_rom)
    variant[offset:offset + 4] = registration.to_bytes(2, "big") + value.to_bytes(2, "big")
    if digest(variant) != member["selected_rom_sha256"]:
        fail("selector-only ROM bytes differ from the exact inventory member hash")
    path = run_dir / f"member-{member['selector']}.z64"
    write_new(path, bytes(variant))
    return path


def inventory_members(inventory: dict):
    for registration in inventory["registrations"]:
        for value in registration["values"]:
            yield {
                "registration_index": registration["index"],
                "value_index": value["index"],
                "selector": value["selector"],
                "selected_rom_sha256": value["selected_rom_sha256"],
            }


def run_guest(adapter: pathlib.Path, inventory_path: pathlib.Path,
              rom_path: pathlib.Path, rom_size: int, member: dict,
              batch_path: pathlib.Path, run_dir: pathlib.Path) -> dict:
    command = [str(adapter), "--run-local", "--local-only", str(inventory_path),
               str(rom_path), str(rom_size), member["selected_rom_sha256"],
               str(TICK_LIMIT), str(member["registration_index"]),
               str(member["value_index"]), str(batch_path)]
    started = datetime.now(timezone.utc).isoformat()
    start = time.monotonic()
    process = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.DEVNULL,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               start_new_session=True)
    captures = {process.stdout: bytearray(), process.stderr: bytearray()}
    overflow = False
    expired = False
    cleanup_signal_denied = False
    done_pipes = set()
    with selectors.DefaultSelector() as selector:
        for stream in captures:
            os.set_blocking(stream.fileno(), False)
            selector.register(stream, selectors.EVENT_READ)
        while selector.get_map():
            remaining = OUTER_SECONDS - (time.monotonic() - start)
            if remaining <= 0:
                expired = True
                break
            for key, _ in selector.select(min(remaining, 0.2)):
                try:
                    block = os.read(key.fd, 8192)
                except BlockingIOError:
                    continue
                if not block:
                    selector.unregister(key.fileobj)
                    done_pipes.add(key.fileobj)
                    continue
                current = captures[key.fileobj]
                if sum(len(item) for item in captures.values()) + len(block) > CAPTURE_BYTES:
                    overflow = True
                    current.extend(block[:max(0, CAPTURE_BYTES - sum(len(item) for item in captures.values()))])
                    break
                current.extend(block)
            if overflow:
                break
    if expired or overflow:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        except PermissionError:
            cleanup_signal_denied = True
    try:
        exit_code = process.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        except PermissionError:
            cleanup_signal_denied = True
        exit_code = process.wait(timeout=2.0)
    group_empty = False
    try:
        os.killpg(process.pid, 0)
    except ProcessLookupError:
        group_empty = True
    except PermissionError:
        group_empty = False
        cleanup_signal_denied = True
    finally:
        if process.stdout is not None:
            process.stdout.close()
        if process.stderr is not None:
            process.stderr.close()
    stdout = bytes(captures[process.stdout])
    stderr = bytes(captures[process.stderr])
    output_path = run_dir / f"attempt-{member['selector']}.stdout.log"
    error_path = run_dir / f"attempt-{member['selector']}.stderr.log"
    write_new(output_path, stdout)
    write_new(error_path, stderr)
    classification = "timeout" if expired else "harness-error" if overflow or not group_empty else "harness-error"
    first_line = stdout.decode("ascii", errors="replace").splitlines()[:1]
    if classification == "harness-error" and first_line:
        match = re.search(r"classification=(refused|incomplete|timeout|pass|guest-fail|harness-error)", first_line[0])
        if match:
            classification = match.group(1)
    batch_sha = None
    if batch_path.exists() and not batch_path.is_symlink():
        batch_data = checked_file(batch_path, 4096, "RSP member batch",
                                  private_root=PRIVATE)
        batch_sha = digest(batch_data)
    receipt = {
        "adapter_argv": command,
        "adapter_exit": exit_code,
        "batch_path": batch_path.relative_to(ROOT).as_posix() if batch_path.exists() else None,
        "batch_sha256": batch_sha,
        "capture_bytes": len(stdout) + len(stderr),
        "capture_limit": CAPTURE_BYTES,
        "child_wall_seconds": CHILD_SECONDS,
        "cleanup_group_empty": group_empty,
        "cleanup_signal_denied": cleanup_signal_denied,
        "classification": classification,
        "outer_deadline_expired": expired,
        "outer_wall_seconds": OUTER_SECONDS,
        "output_overflow": overflow,
        "registration_index": member["registration_index"],
        "selector": member["selector"],
        "started_utc": started,
        "step_ticks": STEP_TICKS,
        "tick_limit": TICK_LIMIT,
        "value_index": member["value_index"],
    }
    write_json(run_dir / f"attempt-{member['selector']}.command.json", receipt)
    if classification == "harness-error" or batch_sha is None:
        fail("bounded RSP attempt did not produce a classified authenticated adapter batch")
    return receipt


def invoke_adapter(adapter: pathlib.Path, *args: str) -> tuple[int, bytes, bytes]:
    return run_capture([str(adapter), *args], timeout=10.0, maximum=65_536)


def run_mode(mode: str, adapter_arg: pathlib.Path,
             run_dir_arg: pathlib.Path) -> int:
    if os.environ.get("CI", "").lower() in {"true", "1"} or os.environ.get("T64_REQUIRE_FIXTURES") == "1":
        fail("RSP guest execution is local-only")
    adapter = adapter_path(adapter_arg)
    run_dir = private_directory(run_dir_arg, create=True)
    batch_dir = private_directory(run_dir / "batches", create=True)

    code, identity_stdout, identity_stderr = run_capture(
        [str(adapter), "--runtime-identity-json"], timeout=10.0, maximum=4096)
    write_new(run_dir / "runtime-identity.stdout.json", identity_stdout)
    write_new(run_dir / "runtime-identity.stderr.log", identity_stderr)
    if code != 0:
        fail("RSP adapter runtime identity refused before any guest use")
    runtime = parse_json(identity_stdout, "RSP runtime identity")
    execution_binding = verify_runtime_binding(adapter, runtime, run_dir)
    write_json(run_dir / "execution-binding.json", execution_binding)

    qual_code, qual_stdout, qual_stderr = run_capture(
        [sys.executable, "-B", "scripts/verify-rsp-diagnostic-qualification.py",
         "--verify", "build/private-rsp-diagnostic/qualification.json"],
        timeout=120.0, maximum=65_536)
    write_new(run_dir / "preuse.stdout.log", qual_stdout)
    write_new(run_dir / "preuse.stderr.log", qual_stderr)
    if qual_code != 0:
        fail("fresh RSP pre-use verification refused before guest use")

    qualification, projection, build, base_rom, rom_path = verify_preuse(run_dir)
    inventory, inventory_bytes = build_inventory(qualification, projection,
                                                 build, base_rom, runtime)
    inventory_path = run_dir / "rsp-inventory.json"
    write_new(inventory_path, inventory_bytes)
    validate_code, validate_stdout, validate_stderr = invoke_adapter(
        adapter, "--validate-inventory", str(inventory_path))
    write_new(run_dir / "inventory-validation.stdout.log", validate_stdout)
    write_new(run_dir / "inventory-validation.stderr.log", validate_stderr)
    if validate_code != 0:
        fail("production RSP adapter rejected the sealed exact inventory")
    selector_offset = build["composition"]["layout"]["selector_rom_offset"]

    if mode == "verify-complete":
        code, stdout, stderr = invoke_adapter(
            adapter, "--verify-complete-json", str(inventory_path), str(batch_dir))
        write_new(run_dir / "complete-verification.stdout.log", stdout)
        write_new(run_dir / "complete-verification.stderr.log", stderr)
        return 0 if code == 0 else 3

    members = list(inventory_members(inventory))
    selected = members if mode == "run-all" else members[:1]
    for member in selected:
        rom_variant = write_selector_rom(run_dir, base_rom, selector_offset, member)
        batch_path = batch_dir / f"{member['selector']}.json"
        attempt = run_guest(adapter, inventory_path, rom_variant, len(base_rom),
                            member, batch_path, run_dir)
        validate_batch_code, batch_stdout, batch_stderr = invoke_adapter(
            adapter, "--validate-batch-json", str(inventory_path), str(batch_path))
        write_new(run_dir / f"batch-validation-{member['selector']}.stdout.log", batch_stdout)
        write_new(run_dir / f"batch-validation-{member['selector']}.stderr.log", batch_stderr)
        if validate_batch_code != 0:
            fail("RSP adapter rejected its own member result batch")
        if mode == "run-one":
            write_json(run_dir / "first-use-receipt.json", {
                "adapter_sha256": execution_binding["adapter_sha256"],
                "attempt": attempt,
                "base_rom_path": rom_path.relative_to(ROOT).as_posix(),
                "base_rom_sha256": build["artifacts"]["rom"]["sha256"],
                "component": "rsp",
                "inventory_sha256": digest(inventory_bytes),
                "limits": {"capture_bytes": CAPTURE_BYTES,
                           "child_wall_seconds": CHILD_SECONDS,
                           "outer_wall_seconds": OUTER_SECONDS,
                           "step_ticks": STEP_TICKS,
                           "tick_limit": TICK_LIMIT},
                "profile_descriptor_sha256": runtime["profile_descriptor_sha256"],
                "qualification_sha256": digest((PRIVATE / "qualification.json").read_bytes()),
                "schema": "t64.rsp-first-use.v1",
                "source_revision": qualification["source_revision"],
                "status": "actual-selector-bound-attempt-recorded",
            })
            print(f"RSP selector attempt retained: {attempt['classification']}")
            return 0

    complete_code, complete_stdout, complete_stderr = invoke_adapter(
        adapter, "--verify-complete-json", str(inventory_path), str(batch_dir))
    write_new(run_dir / "complete-verification.stdout.log", complete_stdout)
    write_new(run_dir / "complete-verification.stderr.log", complete_stderr)
    if complete_code != 0:
        print("RSP run-all finished with incomplete or nonpassing coverage")
        return 3
    print("RSP run-all complete")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_mutually_exclusive_group(required=True)
    actions.add_argument("--run-one", action="store_true")
    actions.add_argument("--run-all", action="store_true")
    actions.add_argument("--verify-complete", action="store_true")
    actions.add_argument("--verify-future-context", action="store_true")
    parser.add_argument("--adapter", type=pathlib.Path,
                        default=ROOT / "build/gate/runner/t64-rsp-diagnostic")
    parser.add_argument("--run-dir", type=pathlib.Path)
    args = parser.parse_args()
    try:
        if args.verify_future_context:
            adapter = adapter_path(args.adapter)
            code, stdout, stderr = run_capture(
                [str(adapter), "--runtime-identity-json"], timeout=10.0,
                maximum=4096)
            if code != 0:
                fail("current RSP adapter refused future-context identity inspection")
            runtime = parse_json(stdout, "post-fix RSP runtime identity")
            verify_future_execution_context(adapter, runtime)
            print("future RSP context verified; guest execution remains unauthorized pending requalification")
            return 0
        if args.run_dir is None:
            fail("--run-dir is required for run-one, run-all, and verify-complete")
        mode = "run-one" if args.run_one else "run-all" if args.run_all else "verify-complete"
        return run_mode(mode, args.adapter, args.run_dir)
    except (OSError, Refusal, subprocess.SubprocessError, ValueError, KeyError,
            TypeError, OverflowError) as error:
        print(f"refused: {error}", file=sys.stderr)
        try:
            if args.run_dir is None:
                raise OSError("no run directory requested")
            run_dir = pathlib.Path(os.path.abspath(args.run_dir))
            if run_dir.exists() and run_dir.is_dir() and not run_dir.is_symlink():
                path = run_dir / "runner-error.json"
                if not path.exists():
                    write_json(path, {"error": str(error), "schema": "t64.rsp-runner-error.v1"})
        except OSError:
            pass
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
