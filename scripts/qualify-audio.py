#!/usr/bin/env python3
"""Verify private, original records from the authored audio capture tests."""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import importlib.util
import json
import os
import re
import shlex
import struct
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "docs/spec/audio-cases.json"
EXPECTED_INVENTORY_SHA256 = "2f68e71e9f68f2869dad61b5054f19c9057c5ea6c7a82d06120672d5717874a8"
PROFILE_ID = "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"
PROFILE_SHA256 = "eaa400aa23bfa3d0c9ba6d475d8eba7a60302151e22b6278ff6a4991d08bb5bf"
FIXTURE_ID = "audio-pcm-literal"
FIXTURE_SHA256 = "cd9556444480b8b02b043207013e7ce5bf5697a1c040058bcde1348e55e80b99"
INPUT_SHA256 = "6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d"
PCM = bytes.fromhex(
    "00007fff8000ffff1234edcc0001fffe"
    "80007fffffff000055aaaa5500008000"
)
# Authored expectations for fixtures/audio/pcm.S, kept independent of any retained
# record: eight CPU stores from the copy_pcm loop at 0x8000045c into RDRAM 0x3400,
# seven AI register stores in program order, the guest-owned MI snapshot store to
# 0x3210, and the final hello-transcript flush store as the last retired instruction.
AUDIO_PCM_WORDS = (
    0x00007FFF, 0x8000FFFF, 0x1234EDCC, 0x0001FFFE,
    0x80007FFF, 0xFFFF0000, 0x55AAAA55, 0x00008000,
)
AUDIO_PCM_STORE = (0x8000045C, 0xAD2B0000)
AUDIO_AI_WRITES = (
    (0x04500008, 1), (0x04500010, 1103), (0x04500014, 15),
    (0x04500000, 0x3400), (0x04500004, 16), (0x04500000, 0x3410),
    (0x04500004, 16),
)
AUDIO_AI_STORES = (
    (0x8000047C, 0xAD090008), (0x80000484, 0xAD090010),
    (0x8000048C, 0xAD090014), (0x80000494, 0xAD090000),
    (0x8000049C, 0xAD090004), (0x800004A4, 0xAD090000),
    (0x800004AC, 0xAD090004),
)
AUDIO_MI_SNAPSHOT_ADDRESS = 0x3210
AUDIO_LAST_CPU_PC = 0x800005E4
TRACE_KIND_BUS_WRITE32 = 4
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
SHA1_RE = re.compile(r"^[0-9a-f]{40}$")
DECIMAL_RE = re.compile(r"^[0-9]+$")
PUBLIC_SCHEMA = "t64.audio-qualification"
PUBLIC_KEYS = {
    "schema", "schema_version", "evidence_mode", "workflow_sha", "source_revision",
    "job_name", "run_id", "run_attempt", "platform", "profile", "fixture",
    "producer", "capture", "execution", "inventory", "platform_witness",
}
SOURCE_PATHS = (
    "CMakeLists.txt", "core", "runner", "cmake",
    "scripts/check-audio-cases.py", "scripts/check-rsp-no-hle.py",
    "scripts/rights-scan.sh", "scripts/pii-scan.sh",
    "scripts/check-provenance.sh", "scripts/check-claims.sh",
    "scripts/qualify-audio.py", "scripts/tests/test_audio_qualification.py",
    "scripts/gate.sh",
    "scripts/qualify-rdp-vi.py", "scripts/collect-boot-evidence.sh",
    "fixtures/manifest.json", "fixtures/manifest.schema.json",
    "tools/manifest", "tools/rights",
    "scripts/tests/test_rsp_no_hle.py",
    "docs/spec/audio-cases.json", ".github/workflows/ci.yml",
)
OWNER_METADATA_PATHS = ("AGENTS.md", ".planning/config.json", ".planning/AUTONOMY.md")


class Refusal(ValueError):
    """Raised when an original record is missing, changed, or unbound."""


class _TAudioPacketInfo(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("sequence", ctypes.c_uint64),
        ("first_sample_index", ctypes.c_uint64),
        ("first_tick", ctypes.c_uint64),
        ("last_tick", ctypes.c_uint64),
        ("frame_count", ctypes.c_uint32),
        ("rate_numerator", ctypes.c_uint64),
        ("rate_denominator", ctypes.c_uint64),
        ("channels", ctypes.c_uint32),
        ("bits_per_sample", ctypes.c_uint32),
        ("encoding", ctypes.c_uint32),
        ("byte_length", ctypes.c_size_t),
        ("profile_id", ctypes.c_char * 64),
        ("build_id", ctypes.c_char * 128),
    ]


class _TAudioPacketObservation(ctypes.Structure):
    _fields_ = [
        ("info", _TAudioPacketInfo),
        ("bytes", ctypes.c_uint8 * 16),
    ]


class _TAudioBusWrite(ctypes.Structure):
    _fields_ = [
        ("tick", ctypes.c_uint64),
        ("address", ctypes.c_uint32),
        ("value", ctypes.c_uint32),
        ("kind", ctypes.c_uint32),
    ]


class _TAudioSwRetirement(ctypes.Structure):
    _fields_ = [
        ("tick", ctypes.c_uint64),
        ("pc", ctypes.c_uint32),
        ("opcode", ctypes.c_uint32),
    ]


class _TAudioYieldObservation(ctypes.Structure):
    _fields_ = [
        ("target_tick", ctypes.c_uint64),
        ("before_tick", ctypes.c_uint64),
        ("reached_tick", ctypes.c_uint64),
        ("video_before", ctypes.c_uint32),
        ("video_after", ctypes.c_uint32),
        ("audio_before", ctypes.c_uint32),
        ("audio_after", ctypes.c_uint32),
    ]


class _TAudioObservation(ctypes.Structure):
    _fields_ = [
        ("magic", ctypes.c_uint32),
        ("version", ctypes.c_uint32),
        ("packet_count", ctypes.c_uint32),
        ("bus_write_count", ctypes.c_uint32),
        ("sw_retirement_count", ctypes.c_uint32),
        ("trace_overflow", ctypes.c_uint32),
        ("mi_ai_read_seen", ctypes.c_uint32),
        ("guest_mi_snapshot_seen", ctypes.c_uint32),
        ("guest_mi_snapshot", ctypes.c_uint32),
        ("trace_records", ctypes.c_uint64),
        ("mi_ai_read_tick", ctypes.c_uint64),
        ("guest_last_cpu_tick", ctypes.c_uint64),
        ("guest_stop_tick", ctypes.c_uint64),
        ("guest_first_escape_tick", ctypes.c_uint64),
        ("guest_first_exception_tick", ctypes.c_uint64),
        ("guest_first_exception_epc", ctypes.c_uint64),
        ("guest_first_exception_preceding_tick", ctypes.c_uint64),
        ("guest_first_exception_state", ctypes.c_uint64),
        ("guest_first_exception_mi", ctypes.c_uint64),
        ("guest_last_cpu_pc", ctypes.c_uint32),
        ("guest_first_escape_pc", ctypes.c_uint32),
        ("guest_first_exception_preceding_pc", ctypes.c_uint32),
        ("guest_first_exception_seen", ctypes.c_uint32),
        ("guest_stop_reason", ctypes.c_uint32),
        ("yield_count", ctypes.c_uint32),
        ("packets", _TAudioPacketObservation * 2),
        ("bus_writes", _TAudioBusWrite * 16),
        ("sw_retirements", _TAudioSwRetirement * 16),
        ("yields", _TAudioYieldObservation * 65),
    ]


def _read(path: Path, limit: int = 64 * 1024 * 1024) -> bytes:
    try:
        metadata = path.lstat()
    except OSError as error:
        raise Refusal(f"required original file is unavailable: {path.name}") from error
    if path.is_symlink() or not path.is_file() or metadata.st_size < 0 or metadata.st_size > limit:
        raise Refusal(f"original file is not a bounded regular file: {path.name}")
    return path.read_bytes()


def _json(path: Path, limit: int = 1024 * 1024) -> Any:
    try:
        return json.loads(_read(path, limit))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise Refusal(f"original JSON is malformed: {path.name}") from error


def _sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _file_sha(path: Path) -> str:
    return _sha(_read(path))


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise Refusal(message)


def _under(path: Path, parent: Path) -> bool:
    try:
        path.resolve().relative_to(parent.resolve())
        return True
    except (OSError, ValueError):
        return False


def _test_section(last_test: str, name: str) -> str:
    sections = re.split(r"(?=^\d+/\d+ Testing: )", last_test, flags=re.MULTILINE)
    matches = [section for section in sections if re.search(
        rf"^\d+/\d+ Testing: {re.escape(name)}$", section, flags=re.MULTILINE
    )]
    _require(len(matches) == 1, f"LastTest.log does not contain one {name} original")
    section = matches[0]
    _require(re.search(r"^Test Passed\.$", section, flags=re.MULTILINE) is not None,
             f"CTest did not record {name} as passed")
    return section


def _junit_passed(path: Path, name: str) -> None:
    try:
        root = ET.fromstring(_read(path, 8 * 1024 * 1024))
    except ET.ParseError as error:
        raise Refusal("original JUnit is malformed") from error
    tests = [element for element in root.iter("testcase") if element.attrib.get("name") == name]
    _require(len(tests) == 1, f"JUnit does not contain one {name} case")
    case = tests[0]
    _require(not any(child.tag in {"failure", "error", "skipped"} for child in case),
             f"JUnit records {name} as failed or skipped")


def _parse_command(section: str) -> list[str]:
    match = re.search(r"^Command: (.+)$", section, flags=re.MULTILINE)
    _require(match is not None, "LastTest.log is missing the original CTest command")
    try:
        return shlex.split(match.group(1))
    except ValueError as error:
        raise Refusal("original CTest command cannot be parsed") from error


def _git(args: list[str]) -> bytes:
    result = subprocess.run(["git", *args], cwd=ROOT, capture_output=True,
                            timeout=20, check=False)
    if result.returncode != 0:
        raise Refusal("cannot inspect the selected source inputs")
    return result.stdout


def _source_snapshot(commit_sha: str, *, require_clean: bool) -> dict[str, Any]:
    _require(SHA1_RE.fullmatch(commit_sha) is not None, "source SHA is malformed")
    selected = _git(["rev-parse", "HEAD"]).decode("ascii").strip()
    _require(selected == commit_sha, "checked-out source SHA differs from the selected run")
    status = _git([
        "status", "--porcelain=v1", "-z", "--untracked-files=all", "--",
        *SOURCE_PATHS,
    ])
    _require(not status or not require_clean,
             "hosted source inputs differ from the selected clean commit")
    listed = _git([
        "ls-files", "--cached", "--others", "--exclude-standard", "-z", "--",
        *SOURCE_PATHS,
    ])
    paths = sorted({value.decode("utf-8", errors="strict")
                    for value in listed.split(b"\0") if value})
    _require(bool(paths), "source input inventory is empty")
    entries = []
    for relative in paths:
        path = ROOT / relative
        metadata = path.lstat()
        _require(path.is_file() and not path.is_symlink(),
                 "source input is not a regular file")
        data = _read(path, 64 * 1024 * 1024)
        entries.append({
            "path": relative,
            "mode": "100755" if metadata.st_mode & 0o111 else "100644",
            "size_bytes": len(data),
            "sha256": _sha(data),
        })
    encoded = json.dumps(entries, sort_keys=True, separators=(",", ":")).encode()
    return {
        "commit_sha": selected,
        "dirty_status_sha256": _sha(status),
        "input_count": len(entries),
        "input_manifest_sha256": _sha(encoded),
        "inputs": entries,
    }


def _owner_metadata_snapshot() -> list[dict[str, Any]]:
    """Record known non-build owner inputs without treating them as source code."""
    entries = []
    _validate_owner_exclusion_scope(list(OWNER_METADATA_PATHS))
    for relative in OWNER_METADATA_PATHS:
        path = ROOT / relative
        if not path.exists() and not path.is_symlink():
            entries.append({"path": relative, "present": False, "dirty_status": ""})
            continue
        _require(path.is_file() and not path.is_symlink(),
                 "excluded owner metadata is not a regular file")
        status = _git(["status", "--porcelain=v1", "-z", "--untracked-files=all",
                       "--", relative]).decode("utf-8", errors="strict")
        data = _read(path, 4 * 1024 * 1024)
        entries.append({
            "path": relative,
            "present": True,
            "dirty_status": status,
            "size_bytes": len(data),
            "sha256": _sha(data),
        })
    return entries


def _validate_owner_exclusion_scope(paths: list[str]) -> list[str]:
    _require(isinstance(paths, list) and paths and
             all(path in OWNER_METADATA_PATHS for path in paths),
             "only named non-build owner metadata may be excluded from local source binding")
    return paths


def _validate_inventory(data: bytes, expected_sha256: str) -> dict[str, Any]:
    _require(_sha(data) == expected_sha256,
             "audio case denominator differs from the independently pinned inventory")
    try:
        inventory = json.loads(data)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise Refusal("audio case inventory is malformed") from error
    _require(isinstance(inventory, dict) and
             inventory.get("schema") == "t64.audio-case-inventory" and
             inventory.get("schema_version") == 1,
             "audio case inventory schema is unsupported")
    suites = inventory.get("suites")
    _require(isinstance(suites, dict) and bool(suites),
             "audio case inventory has no suites")
    ctests: set[str] = set()
    case_pairs: set[tuple[str, str]] = set()
    for suite_name, suite in suites.items():
        _require(isinstance(suite_name, str) and suite_name and isinstance(suite, dict),
                 "audio suite entry is malformed")
        suite_ctests = suite.get("ctest")
        _require(isinstance(suite_ctests, list) and bool(suite_ctests) and
                 all(isinstance(value, str) and value for value in suite_ctests) and
                 len(suite_ctests) == len(set(suite_ctests)),
                 f"{suite_name} has an empty or duplicate CTest denominator")
        ctests.update(suite_ctests)
        binaries = suite.get("binaries")
        _require(isinstance(binaries, list) and bool(binaries),
                 f"{suite_name} has no case-list binary")
        paths: set[str] = set()
        for entry in binaries:
            _require(isinstance(entry, dict) and isinstance(entry.get("path"), str),
                     f"{suite_name} has a malformed binary entry")
            relative = Path(entry["path"])
            _require(not relative.is_absolute() and ".." not in relative.parts and
                     "." not in relative.parts and relative.as_posix() not in paths,
                     f"{suite_name} contains an unsafe or duplicate binary path")
            paths.add(relative.as_posix())
            case_labels = entry.get("cases")
            _require(isinstance(case_labels, list) and bool(case_labels) and
                     all(isinstance(value, str) and value for value in case_labels) and
                     len(case_labels) == len(set(case_labels)),
                     f"{suite_name} has an empty or duplicate case denominator")
            for case in case_labels:
                case_pairs.add((relative.as_posix(), case))
    _require(case_pairs and ctests,
             "audio case inventory denominator is empty")
    return {
        "snapshot_sha256": _sha(data),
        "case_count": len(case_pairs),
        "ctest_count": len(ctests),
    }


def _public_projection(value: Any, expected: dict[str, Any] | None = None) -> dict[str, Any]:
    _require(isinstance(value, dict) and set(value) == PUBLIC_KEYS,
             "public projection has unknown or missing top-level fields")
    _require(value.get("schema") == PUBLIC_SCHEMA and value.get("schema_version") == 1,
             "public projection schema is unsupported")
    workflow_sha = value.get("workflow_sha")
    source_revision = value.get("source_revision")
    job = value.get("job_name")
    evidence_mode = value.get("evidence_mode")
    _require((workflow_sha is None or
              isinstance(workflow_sha, str) and SHA1_RE.fullmatch(workflow_sha) is not None) and
             isinstance(source_revision, str) and SHA1_RE.fullmatch(source_revision) is not None and
             job in {"local", "gate-linux", "gate-macos"},
             "public projection context is malformed")
    run_id = value.get("run_id")
    attempt = value.get("run_attempt")
    if job == "local":
        _require(run_id is None and attempt is None and workflow_sha is None and
                 evidence_mode == "entry-path-validation",
                 "local projection unexpectedly claims a hosted run")
    else:
        _require(evidence_mode == "hosted-gate" and isinstance(workflow_sha, str) and
                 isinstance(run_id, str) and DECIMAL_RE.fullmatch(run_id) is not None and
                 isinstance(attempt, str) and DECIMAL_RE.fullmatch(attempt) is not None,
                 "hosted projection is missing its exact run and attempt")
    _require(value.get("platform") in {"linux", "macos"},
             "projection host platform is unsupported")
    profile = value.get("profile")
    fixture = value.get("fixture")
    producer = value.get("producer")
    capture = value.get("capture")
    execution = value.get("execution")
    inventory = value.get("inventory")
    witness = value.get("platform_witness")
    _require(isinstance(profile, dict) and set(profile) == {
        "profile_id", "profile_sha256", "descriptor_version",
        "machine_model_version", "state_serialization_version",
    } and profile == {
        "profile_id": PROFILE_ID, "profile_sha256": PROFILE_SHA256,
        "descriptor_version": 11, "machine_model_version": 19,
        "state_serialization_version": 24,
    }, "projection profile tuple differs from the authored profile")
    _require(isinstance(fixture, dict) and set(fixture) == {
        "fixture_id", "fixture_sha256", "fixture_size", "input_sha256", "input_size",
    } and fixture == {
        "fixture_id": FIXTURE_ID, "fixture_sha256": FIXTURE_SHA256,
        "fixture_size": 4728, "input_sha256": INPUT_SHA256, "input_size": 54,
    }, "projection fixture or input identity differs")
    _require(isinstance(producer, dict) and set(producer) == {
        "source_revision", "core_build_id", "runner_build_id", "real_runner_sha256",
        "supervisor_sha256", "checker_sha256", "test_runner_sha256",
        "test_supervisor_sha256", "test_binary_sha256", "core_library_sha256",
    }, "projection producer fields differ from the bounded allowlist")
    for key in ("real_runner_sha256", "supervisor_sha256", "checker_sha256",
                "test_runner_sha256", "test_supervisor_sha256", "test_binary_sha256",
                "core_library_sha256"):
        _require(isinstance(producer.get(key), str) and
                 SHA256_RE.fullmatch(producer[key]) is not None,
                 "projection executable identity is malformed")
    _require(producer.get("source_revision") == source_revision and
             isinstance(producer.get("core_build_id"), str) and
             re.fullmatch(r"t64-build-v1-[0-9a-f]{64}", producer["core_build_id"]) and
             isinstance(producer.get("runner_build_id"), str) and
             re.fullmatch(r"t64-runner-v1-[0-9a-f]{64}", producer["runner_build_id"]),
             "projection build identities are malformed")
    _require(isinstance(capture, dict) and set(capture) == {
        "manifest_sha256", "video_packets", "audio_packets", "total_bytes",
        "pcm_literal_sha256", "video_observer_sha256", "audio_observer_sha256",
    } and capture.get("manifest_sha256") and capture.get("video_packets") == 29 and
             capture.get("audio_packets") == 2 and capture.get("total_bytes") == 16581 and
             all(isinstance(capture.get(key), str) and SHA256_RE.fullmatch(capture[key])
                 for key in ("manifest_sha256", "pcm_literal_sha256",
                             "video_observer_sha256", "audio_observer_sha256")),
             "projection capture summary is malformed")
    _require(capture.get("pcm_literal_sha256") == _sha(PCM),
             "projection PCM identity differs from the independent authored literal")
    _require(isinstance(execution, dict) and set(execution) == {
        "capture_ctest", "race_ctest", "command_sha256", "race_command_sha256",
        "last_test_sha256", "junit_sha256", "return_code", "status", "timed_out",
    } and execution.get("capture_ctest") == "runner.raw-capture" and
             execution.get("race_ctest") == "controls.real.raw-capture-race" and
             execution.get("return_code") == 0 and execution.get("status") == "pass" and
             execution.get("timed_out") is False and
             all(isinstance(execution.get(key), str) and SHA256_RE.fullmatch(execution[key])
                 for key in ("command_sha256", "race_command_sha256",
                             "last_test_sha256", "junit_sha256")),
             "projection process result is stale, failed, or malformed")
    _require(isinstance(inventory, dict) and set(inventory) == {
        "snapshot_sha256", "case_count", "ctest_count",
    } and isinstance(inventory.get("snapshot_sha256"), str) and
             SHA256_RE.fullmatch(inventory["snapshot_sha256"]) is not None and
             isinstance(inventory.get("case_count"), int) and inventory["case_count"] > 0 and
             isinstance(inventory.get("ctest_count"), int) and inventory["ctest_count"] > 0,
             "projection source denominator is empty or malformed")
    pinned_inventory = _validate_inventory(_read(INVENTORY, 128 * 1024),
                                           EXPECTED_INVENTORY_SHA256)
    _require(inventory == pinned_inventory,
             "projection denominator is incomplete, empty, or differs from the authored inventory")
    _require(isinstance(witness, dict) and set(witness) == {
        "platform", "test_binary_sha256", "runner_binary_sha256",
        "supervisor_binary_sha256", "checker_binary_sha256",
        "ctest_command_sha256", "contender_command_sha256", "outcomes",
    } and witness.get("platform") == value.get("platform") and
             witness.get("outcomes") == [
                 "harness-error:capture-no-replace-commit", "pass:positive-match"
             ] and isinstance(witness.get("contender_command_sha256"), list) and
             len(witness["contender_command_sha256"]) == 2 and
             all(isinstance(item, str) and SHA256_RE.fullmatch(item)
                 for item in witness["contender_command_sha256"]) and
             all(isinstance(witness.get(key), str) and SHA256_RE.fullmatch(witness[key])
                 for key in ("test_binary_sha256", "runner_binary_sha256",
                             "supervisor_binary_sha256", "checker_binary_sha256",
                             "ctest_command_sha256")),
             "hosted platform no-replace witness is missing or malformed")
    if expected is not None:
        _require(workflow_sha == expected.get("sha") and job == expected.get("job") and
                 run_id == expected.get("run") and attempt == expected.get("attempt"),
                 "public projection differs from the exact supplied run context")
    return value


def _validate_hosted_context(value: Any, *, repo: str, sha: str, run: str,
                            attempt: str, job: str) -> dict[str, Any]:
    """Bind one hosted manifest to caller-supplied Actions identity."""
    _require(repo == "szTheory/tendies64" and SHA1_RE.fullmatch(sha) is not None and
             DECIMAL_RE.fullmatch(run) is not None and
             DECIMAL_RE.fullmatch(attempt) is not None and
             job in {"gate-linux", "gate-macos"},
             "hosted collection requires an exact repository, SHA, run, attempt, and job")
    manifest = _public_projection(value, {
        "job": job, "sha": sha, "run": run, "attempt": attempt,
    })
    expected_platform = "linux" if job == "gate-linux" else "macos"
    _require(manifest.get("platform") == expected_platform and
             manifest.get("platform_witness", {}).get("platform") == expected_platform,
             "hosted platform does not match the exact workflow job")
    return manifest


def _verify_artifact_download(artifact: Any, binding: Any, archive: bytes, *,
                              repo: str, sha: str, run: str, attempt: str,
                              job: str) -> dict[str, Any]:
    expected_name = f"t64-authored-evidence-{sha}-{run}-{attempt}-{job}"
    _require(isinstance(artifact, dict) and artifact.get("name") == expected_name and
             artifact.get("expired") is False and
             type(artifact.get("id")) is int and artifact["id"] > 0 and
             type(artifact.get("size_in_bytes")) is int and
             0 < artifact["size_in_bytes"] <= 1024 * 1024 and
             isinstance(artifact.get("digest"), str) and
             re.fullmatch(r"sha256:[0-9a-f]{64}", artifact["digest"]) is not None and
             isinstance(artifact.get("workflow_run"), dict) and
             str(artifact["workflow_run"].get("id")) == run and
             artifact["workflow_run"].get("head_sha") == sha,
             "selected hosted artifact API identity differs from the exact attempt")
    _require(isinstance(binding, dict) and set(binding) == {
        "artifact_id", "commit_sha", "run_id", "run_attempt", "job_name",
        "archive_digest", "archive_bytes",
    } and binding == {
        "artifact_id": str(artifact["id"]), "commit_sha": sha, "run_id": run,
        "run_attempt": attempt, "job_name": job,
        "archive_digest": artifact["digest"], "archive_bytes": len(archive),
    }, "retained artifact API binding differs from its original ZIP")
    _require(len(archive) == artifact["size_in_bytes"] and
             f"sha256:{_sha(archive)}" == artifact["digest"],
             "original hosted ZIP length or API digest differs")
    return artifact


def _verify_hosted_originals(downloads: Path, combined: Path, *, repo: str, sha: str,
                             run: str, attempt: str, private_root: Path) -> list[dict[str, Any]]:
    _require(downloads.is_dir() and not downloads.is_symlink(),
             "retained hosted API/ZIP originals are missing or unsafe")
    expected_members = {
        "run.json", "attempt.json", "jobs.json", "artifacts.json",
        "gate-linux.zip", "gate-linux-download-exit.txt", "gate-linux-binding.json",
        "gate-macos.zip", "gate-macos-download-exit.txt", "gate-macos-binding.json",
    }
    _require({path.name for path in downloads.iterdir()} == expected_members,
             "retained hosted API/ZIP directory has missing or unexpected members")
    rdp = _rdp_helpers()
    run_record = rdp._read_json(downloads / "run.json")
    attempt_record = rdp._read_json(downloads / "attempt.json")
    jobs_record = rdp._read_json(downloads / "jobs.json")
    artifacts_record = rdp._read_json(downloads / "artifacts.json")
    repository_record = run_record.get("repository") if isinstance(run_record, dict) else None
    _require(isinstance(run_record, dict) and isinstance(repository_record, dict) and
             str(run_record.get("id")) == run and
             repository_record.get("full_name") == repo and
             run_record.get("path") == ".github/workflows/ci.yml" and
             run_record.get("event") == "push" and run_record.get("head_sha") == sha and
             str(run_record.get("run_attempt")) == attempt and
             run_record.get("status") == "completed" and run_record.get("conclusion") == "success",
             "retained run API record differs from the exact successful push context")
    _require(isinstance(attempt_record, dict) and str(attempt_record.get("id")) == run and
             attempt_record.get("path") == ".github/workflows/ci.yml" and
             attempt_record.get("head_sha") == sha and
             str(attempt_record.get("run_attempt")) == attempt and
             attempt_record.get("status") == "completed" and
             attempt_record.get("conclusion") == "success",
             "retained attempt API record differs from the exact successful context")
    jobs = jobs_record.get("jobs") if isinstance(jobs_record, dict) else None
    _require(isinstance(jobs_record, dict) and jobs_record.get("total_count") == 2 and
             isinstance(jobs, list) and len(jobs) == 2 and
             {job.get("name") for job in jobs if isinstance(job, dict)} ==
             {"gate-linux", "gate-macos"} and
             all(str(job.get("run_id")) == run and str(job.get("run_attempt")) == attempt and
                 job.get("head_sha") == sha and job.get("status") == "completed" and
                 job.get("conclusion") == "success" for job in jobs if isinstance(job, dict)),
             "retained hosted job API record is not exactly the two successful selected jobs")
    artifacts = artifacts_record.get("artifacts") if isinstance(artifacts_record, dict) else None
    _require(isinstance(artifacts_record, dict) and
             artifacts_record.get("total_count") == len(artifacts or []) and
             isinstance(artifacts, list), "retained artifact enumeration is malformed")
    selected: dict[str, dict[str, Any]] = {}
    seen_names: set[str] = set()
    seen_ids: set[str] = set()
    proof_prefix = f"t64-authored-evidence-{sha}-{run}-"
    for artifact in artifacts:
        _require(isinstance(artifact, dict), "retained artifact API list contains a malformed row")
        name = artifact.get("name")
        if not isinstance(name, str) or not name.startswith(proof_prefix):
            continue
        match = re.fullmatch(re.escape(proof_prefix) + r"([1-9][0-9]*)-(gate-linux|gate-macos)", name)
        artifact_id = str(artifact.get("id"))
        workflow = artifact.get("workflow_run")
        _require(match is not None and isinstance(workflow, dict) and
                 name not in seen_names and artifact_id not in seen_ids and
                 isinstance(artifact.get("expired"), bool) and
                 int(match.group(1)) <= int(attempt) and
                 str(workflow.get("id")) == run and workflow.get("head_sha") == sha,
                 "retained proof artifact has unknown, duplicate, expired, or stale context")
        seen_names.add(name)
        seen_ids.add(artifact_id)
        if match.group(1) == attempt:
            _require(artifact["expired"] is False,
                     "selected hosted artifact is expired")
            selected[match.group(2)] = artifact
    _require(set(selected) == {"gate-linux", "gate-macos"},
             "retained API record does not contain exactly both current hosted artifacts")
    archive_bindings = []
    for job in ("gate-linux", "gate-macos"):
        archive = _read(downloads / f"{job}.zip", 1024 * 1024)
        _require(_read(downloads / f"{job}-download-exit.txt", 32).strip() == b"0",
                 f"original {job} archive download did not exit successfully")
        binding = rdp._read_json(downloads / f"{job}-binding.json")
        artifact = _verify_artifact_download(
            selected[job], binding, archive, repo=repo, sha=sha, run=run,
            attempt=attempt, job=job,
        )
        with tempfile.TemporaryDirectory(prefix="t64-audio-zip-check-", dir=private_root) as temp:
            extracted = Path(temp) / job
            rdp._safe_extract_upload(archive, extracted, job)
            rdp.verify_upload(extracted, expected_sha=sha, expected_job=job,
                               expected_run=run, expected_attempt=attempt)
            zipped_audio = _read(extracted / "audio" / job / "manifest.json", 16 * 1024)
            published_audio = _read(combined / "audio" / job / "manifest.json", 16 * 1024)
            _require(zipped_audio == published_audio,
                     f"original {job} ZIP audio projection differs from collected output")
        archive_bindings.append({
            "job": job, "artifact_id": str(artifact["id"]),
            "archive_bytes": artifact["size_in_bytes"],
            "archive_digest": artifact["digest"],
            "audio_manifest_sha256": _file_sha(combined / "audio" / job / "manifest.json"),
        })
    return archive_bindings


def _compare_hosted_pair(manifests: dict[str, dict[str, Any]]) -> None:
    _require(set(manifests) == {"gate-linux", "gate-macos"},
             "hosted audio evidence requires both named platform jobs")
    def semantics(value: dict[str, Any]) -> dict[str, Any]:
        execution = value["execution"]
        witness = value["platform_witness"]
        return {
            "schema": value["schema"],
            "schema_version": value["schema_version"],
            "evidence_mode": value["evidence_mode"],
            "workflow_sha": value["workflow_sha"],
            "source_revision": value["source_revision"],
            "run_id": value["run_id"],
            "run_attempt": value["run_attempt"],
            "profile": value["profile"],
            "fixture": value["fixture"],
            "capture": value["capture"],
            "execution": {key: execution[key] for key in (
                "capture_ctest", "race_ctest", "return_code", "status", "timed_out",
            )},
            "inventory": value["inventory"],
            "platform_witness_outcomes": witness["outcomes"],
        }
    _require(semantics(manifests["gate-linux"]) == semantics(manifests["gate-macos"]),
             "Linux and macOS audio manifests disagree on authored semantic results")


def _build_pii_upload_layout(combined: Path, destination: Path) -> None:
    _require(not destination.exists() and not destination.is_symlink(),
             "private PII input directory must be fresh")
    destination.mkdir(parents=True, mode=0o700)
    for job in ("gate-linux", "gate-macos"):
        target = destination / job
        target.mkdir(mode=0o700)
        _safe_copy(combined / f"{job}.json", target / f"{job}.json")
        (target / "rdp-vi").mkdir(mode=0o700)
        _safe_copy(combined / "rdp-vi" / job, target / "rdp-vi" / job)
        (target / "audio").mkdir(mode=0o700)
        _safe_copy(combined / "audio" / job, target / "audio" / job)


def _verify_public_pair(directory: Path, *, sha: str, run: str,
                        attempt: str) -> dict[str, dict[str, Any]]:
    _require(directory.is_dir() and not directory.is_symlink() and
             {path.name for path in directory.iterdir()} == {"gate-linux", "gate-macos"} and
             all(path.is_dir() and not path.is_symlink() for path in directory.iterdir()),
             "public audio projection does not contain exactly the two hosted jobs")
    manifests = {}
    for job in ("gate-linux", "gate-macos"):
        manifests[job] = _scan_public(directory / job, {
            "job": job, "sha": sha, "run": run, "attempt": attempt,
        })
    _compare_hosted_pair(manifests)
    return manifests


def _collect_hosted(repo: str, sha: str, run: str, attempt: str,
                    native_path: Path, output_dir: Path) -> Path:
    _require(repo == "szTheory/tendies64" and SHA1_RE.fullmatch(sha) is not None and
             DECIMAL_RE.fullmatch(run) is not None and
             DECIMAL_RE.fullmatch(attempt) is not None and
             _git(["rev-parse", "HEAD"]).decode().strip() == sha,
             "hosted collection requires exact repository/SHA/run/attempt on the selected source")
    _require(not native_path.is_symlink(), "native record path is a symlink")
    native_path = native_path.resolve()
    native = _verify_native_record(native_path, sha)
    private_root = ROOT / "build/gate/08-05-audio-private"
    hosted_private = private_root / f"hosted-{sha[:12]}-{run}-{attempt}-{time.time_ns()}"
    _require(not hosted_private.exists() and not hosted_private.is_symlink(),
             "hosted private destination already exists or is unsafe")
    hosted_private.mkdir(parents=True, mode=0o700)
    combined = hosted_private / "combined"
    public_output = output_dir.resolve()
    _require(not output_dir.is_symlink() and not _under(public_output, private_root) and
             not public_output.exists() and
             not public_output.is_symlink(),
             "public audio output must be a fresh directory")
    source_snapshot = _source_snapshot(sha, require_clean=True)
    _require(source_snapshot == native["source_snapshot"],
             "hosted collection source inputs differ from the completed native source snapshot")
    rdp = _rdp_helpers()
    collect_command = [sys.executable, "-B", str(ROOT / "scripts/qualify-rdp-vi.py"),
                       "--collect-hosted", "--repo", repo, "--sha", sha,
                       "--run", run, "--attempt", attempt, "--out", str(combined)]
    collect_dir = hosted_private / "collector-command"
    _run_saved_stage(rdp, collect_dir, collect_command, 240)
    output_bytes = _read(collect_dir / "stdout.log", 4 * 1024 * 1024)
    collection_matches = re.findall(
        rb"^qualify-rdp-vi: private collection records retained at (.+)$",
        output_bytes, flags=re.MULTILINE,
    )
    _require(len(collection_matches) == 1,
             "exact hosted collector did not return one private API/ZIP record path")
    collection_path = Path(collection_matches[0].decode("utf-8", errors="strict"))
    _require(_under(collection_path, ROOT / "build/gate/07-qualification") and
             collection_path.is_dir() and not collection_path.is_symlink(),
             "hosted collection receipt is outside its private record store")
    manifests = rdp._verify_collected_pair(
        combined, commit_sha=sha, run_id=run, run_attempt=attempt,
    )
    _require(all((combined / "audio" / job / "manifest.json").is_file()
                 for job in ("gate-linux", "gate-macos")),
             "selected hosted artifacts do not both contain audio qualification manifests")
    audio_manifests = {
        job: _validate_hosted_context(
            _json(combined / "audio" / job / "manifest.json", 16 * 1024),
            repo=repo, sha=sha, run=run, attempt=attempt, job=job,
        ) for job in ("gate-linux", "gate-macos")
    }
    _compare_hosted_pair(audio_manifests)
    nested_stdout = _read(collection_path / "collection/stdout.log", 8 * 1024 * 1024)
    download_matches = re.findall(
        rb"^collect-boot-evidence: original download records retained at (.+)$",
        nested_stdout, flags=re.MULTILINE,
    )
    _require(len(download_matches) == 1,
             "collector did not retain one exact set of API and archive originals")
    downloads = Path(download_matches[0].decode("utf-8", errors="strict"))
    _require(_under(downloads, ROOT / "build/gate/07-qualification") and
             not downloads.is_symlink(),
             "hosted archive originals are outside their private build store")
    archive_bindings = _verify_hosted_originals(
        downloads, combined, repo=repo, sha=sha, run=run, attempt=attempt,
        private_root=hosted_private,
    )
    pii_input = hosted_private / "pii-upload-layout"
    _build_pii_upload_layout(combined, pii_input)
    _run_saved_stage(rdp, hosted_private / "combined-pii-scan", [
        sys.executable, "-B", str(ROOT / "scripts/qualify-rdp-vi.py"),
        "--scan-pii", str(pii_input),
    ], 60)
    _run_saved_stage(rdp, hosted_private / "combined-rights-scan", [
        "sh", "scripts/rights-scan.sh", "--staging", str(combined),
    ], 60)
    output = public_output
    output.mkdir(parents=True, mode=0o755)
    for job in ("gate-linux", "gate-macos"):
        target = output / job
        target.mkdir(mode=0o755)
        manifest_data = _read(combined / "audio" / job / "manifest.json", 16 * 1024)
        with (target / "manifest.json").open("xb") as stream:
            stream.write(manifest_data)
        (target / "manifest.json").chmod(0o644)
    published = _verify_public_pair(output, sha=sha, run=run, attempt=attempt)
    _require(_source_snapshot(sha, require_clean=True) == source_snapshot,
             "source inputs changed during exact hosted collection")
    record = {
        "schema": "t64.audio-hosted-collection",
        "schema_version": 1,
        "evidence_mode": "hosted-gate",
        "status": "complete",
        "repo": repo,
        "sha": sha,
        "run": run,
        "attempt": attempt,
        "jobs": ["gate-linux", "gate-macos"],
        "source_snapshot": source_snapshot,
        "excluded_owner_metadata": _owner_metadata_snapshot(),
        "native_record_path": str(native_path.resolve()),
        "native_record_sha256": _file_sha(native_path),
        "collection_path": str(collection_path),
        "collection_tree_manifest": _tree_manifest(collection_path),
        "downloads_path": str(downloads),
        "downloads_tree_manifest": _tree_manifest(downloads),
        "archive_bindings": archive_bindings,
        "combined_path": str(combined),
        "combined_tree_manifest": _tree_manifest(combined),
        "pii_input_path": str(pii_input),
        "pii_input_tree_manifest": _tree_manifest(pii_input),
        "collector_command_path": str(collect_dir),
        "collector_process": _json(collect_dir / "process.json", 16 * 1024),
        "public_output_path": str(output),
        "public_manifests": {job: _file_sha(output / job / "manifest.json")
                              for job in published},
    }
    record_path = hosted_private / "hosted-record.json"
    _write_json_private(record_path, record)
    return record_path


def _validate_hosted_record_header(record: Any, *, repo: str, sha: str,
                                   run: str, attempt: str) -> dict[str, Any]:
    _require(isinstance(record, dict) and
             record.get("schema") == "t64.audio-hosted-collection" and
             record.get("schema_version") == 1 and
             record.get("evidence_mode") == "hosted-gate" and
             record.get("status") == "complete" and
             record.get("repo") == repo and record.get("sha") == sha and
             record.get("run") == run and record.get("attempt") == attempt and
             record.get("jobs") == ["gate-linux", "gate-macos"] and
             isinstance(record.get("source_snapshot"), dict) and
             isinstance(record.get("excluded_owner_metadata"), list) and
             isinstance(record.get("native_record_path"), str) and
             isinstance(record.get("native_record_sha256"), str) and
             SHA256_RE.fullmatch(record["native_record_sha256"]) is not None and
             isinstance(record.get("archive_bindings"), list) and
             len(record["archive_bindings"]) == 2 and
             all(isinstance(record.get(key), str) for key in (
                 "collection_path", "downloads_path", "combined_path",
                 "pii_input_path", "collector_command_path", "public_output_path",
             )),
             "hosted record is missing exact context, platforms, or private originals")
    return record


def _verify_hosted_record(path: Path, *, repo: str, sha: str, run: str,
                          attempt: str, native_path: Path) -> dict[str, Any]:
    _require(not path.is_symlink(), "hosted record path is a symlink")
    path = path.resolve()
    private_root = ROOT / "build/gate/08-05-audio-private"
    _require(_under(path, private_root) and path.name == "hosted-record.json" and
             path.is_file(), "explicit hosted record is outside the private qualification tree")
    record = _validate_hosted_record_header(
        _json(path, 8 * 1024 * 1024), repo=repo, sha=sha, run=run, attempt=attempt,
    )
    _require(not native_path.is_symlink(), "native record path is a symlink")
    native_path = native_path.resolve()
    native = _verify_native_record(native_path, sha)
    _require(record.get("native_record_path") == str(native_path) and
             record.get("native_record_sha256") == _file_sha(native_path) and
             record.get("source_snapshot") == native["source_snapshot"] and
             record.get("source_snapshot") == _source_snapshot(sha, require_clean=True) and
             record.get("excluded_owner_metadata") == _owner_metadata_snapshot(),
             "hosted collection is stale against its exact native source binding")
    rdp = _rdp_helpers()
    hosted_private = path.parent
    collection = Path(record["collection_path"])
    downloads = Path(record["downloads_path"])
    combined = Path(record["combined_path"])
    pii_input = Path(record["pii_input_path"])
    collector_dir = Path(record["collector_command_path"])
    public = Path(record["public_output_path"])
    _require(collection.is_dir() and not collection.is_symlink() and
             downloads.is_dir() and not downloads.is_symlink() and
             _under(collection, ROOT / "build/gate/07-qualification") and
             _under(downloads, ROOT / "build/gate/07-qualification") and
             combined == hosted_private / "combined" and
             pii_input == hosted_private / "pii-upload-layout" and
             collector_dir == hosted_private / "collector-command" and
             record.get("collection_tree_manifest") == _tree_manifest(collection) and
             record.get("downloads_tree_manifest") == _tree_manifest(downloads) and
             record.get("combined_tree_manifest") == _tree_manifest(combined),
             "hosted API, archive, command, or extracted originals changed")
    _require(record.get("pii_input_tree_manifest") == _tree_manifest(pii_input),
             "strict PII scanner input layout changed")
    collector_command = [sys.executable, "-B", str(ROOT / "scripts/qualify-rdp-vi.py"),
                         "--collect-hosted", "--repo", repo, "--sha", sha,
                         "--run", run, "--attempt", attempt, "--out", str(combined)]
    _verify_process_record(
        collector_dir, collector_command, max_timeout=240,
        environment={"PYTHONDONTWRITEBYTECODE": "1"},
    )
    collection_stdout = _read(collection / "collection/stdout.log", 8 * 1024 * 1024)
    nested_command = ["sh", "scripts/collect-boot-evidence.sh", "--collect", "--repo", repo,
                      "--sha", sha, "--run", run, "--attempt", attempt,
                      "--out", str(combined)]
    _verify_process_record(collection / "collection", nested_command,
                           max_timeout=180, environment={})
    download_matches = re.findall(
        rb"^collect-boot-evidence: original download records retained at (.+)$",
        collection_stdout, flags=re.MULTILINE,
    )
    _require(len(download_matches) == 1 and
             Path(download_matches[0].decode("utf-8", errors="strict")) == downloads,
             "collector no longer names its exact retained API/ZIP originals")
    rdp._verify_collected_pair(combined, commit_sha=sha, run_id=run, run_attempt=attempt)
    bindings = _verify_hosted_originals(
        downloads, combined, repo=repo, sha=sha, run=run, attempt=attempt,
        private_root=hosted_private,
    )
    _require(record.get("archive_bindings") == bindings,
             "hosted archive API identity or audio projection binding changed")
    _verify_process_record(
        hosted_private / "combined-pii-scan", [
            sys.executable, "-B", str(ROOT / "scripts/qualify-rdp-vi.py"),
            "--scan-pii", str(pii_input),
        ], max_timeout=60, environment={"PYTHONDONTWRITEBYTECODE": "1"},
    )
    _verify_process_record(
        hosted_private / "combined-rights-scan", [
            "sh", "scripts/rights-scan.sh", "--staging", str(combined),
        ], max_timeout=60, environment={"PYTHONDONTWRITEBYTECODE": "1"},
    )
    public_manifests = _verify_public_pair(public, sha=sha, run=run, attempt=attempt)
    _require(record.get("public_manifests") == {
        job: _file_sha(public / job / "manifest.json") for job in public_manifests
    } and all(
        _read(public / job / "manifest.json", 16 * 1024) ==
        _read(combined / "audio" / job / "manifest.json", 16 * 1024)
        for job in public_manifests
    ), "public hosted projections differ from their exact retained archives")
    return record


def _native_command_plan(prefix: str, normal_junit: Path) -> list[dict[str, Any]]:
    """Construct explicit normal/ASan audio checks after the shared native gate."""
    _require(re.fullmatch(r"08-05-[A-Za-z0-9._-]{1,96}", prefix) is not None and
             normal_junit.is_absolute() and
             _under(normal_junit, ROOT / "build/gate/08-05-audio-private"),
             "native artifact prefix is malformed")
    rdp = ROOT / "scripts/qualify-rdp-vi.py"
    checker = ROOT / "scripts/check-audio-cases.py"
    return [
        {
            "label": "shared-native-gates",
            "command": [sys.executable, "-B", str(rdp), "--native"],
            "timeout_seconds": 2400,
            "environment": {"T64_GATE_JUNIT_OUT": str(normal_junit)},
        },
        {
            "label": "normal-audio-qualification",
            "command": [sys.executable, "-B", str(checker), "--build",
                        str((ROOT / "build/gate").resolve()), "--suite", "qualification",
                        "--artifact-prefix", f"{prefix}-normal"],
            "timeout_seconds": 720,
            "environment": {},
        },
        {
            "label": "asan-audio-qualification",
            "command": [sys.executable, "-B", str(checker), "--build",
                        str((ROOT / "build/asan").resolve()), "--suite", "qualification",
                        "--artifact-prefix", f"{prefix}-asan"],
            "timeout_seconds": 720,
            "environment": {},
        },
        {
            "label": "normal-source-body-check",
            "command": [sys.executable, "-B", str(ROOT / "scripts/check-rsp-no-hle.py"),
                        "--build", str((ROOT / "build/gate").resolve())],
            "timeout_seconds": 120,
            "environment": {},
        },
        {
            "label": "asan-source-body-check",
            "command": [sys.executable, "-B", str(ROOT / "scripts/check-rsp-no-hle.py"),
                        "--build", str((ROOT / "build/asan").resolve())],
            "timeout_seconds": 120,
            "environment": {},
        },
    ]


PHASE08_GCC_REQUIRED_UNITS = {
    "core/t64_ai.c", "core/tests/test_ai.c", "core/tests/test_isviewer.c",
    "runner/t64_raw_capture.c",
    "runner/tests/test_audio_capture.c", "runner/t64_real.c",
    "runner/t64_supervisor.c", "runner/tests/test_rdp_video.c",
}
EXPECTED_GCC13_IMAGE = "sha256:ad13696c6b2464e813639c0479ec2f66877516ec18a183c85282378720376786"


def _rdp_helpers() -> Any:
    path = ROOT / "scripts/qualify-rdp-vi.py"
    spec = importlib.util.spec_from_file_location("t64_audio_rdp_helpers", path)
    _require(spec is not None and spec.loader is not None,
             "shared native qualification helpers are unavailable")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _tree_manifest(directory: Path) -> list[dict[str, Any]]:
    _require(directory.is_dir() and not directory.is_symlink(),
             "retained private evidence directory is missing or unsafe")
    entries: list[dict[str, Any]] = []
    total = 0
    for path in sorted(directory.rglob("*")):
        metadata = path.lstat()
        _require(not path.is_symlink() and (path.is_dir() or path.is_file()),
                 "private evidence tree contains a symlink or special file")
        if not path.is_file():
            continue
        data = _read(path, 64 * 1024 * 1024)
        total += len(data)
        _require(total <= 512 * 1024 * 1024 and len(entries) < 10000,
                 "private evidence tree exceeds its bounded inventory")
        entries.append({
            "path": path.relative_to(directory).as_posix(),
            "size_bytes": metadata.st_size,
            "sha256": _sha(data),
        })
    _require(bool(entries), "private evidence tree is empty")
    return entries


def _verify_process_record(directory: Path, command: list[str], *, max_timeout: int,
                           cwd: Path = ROOT,
                           environment: dict[str, str] | None = None) -> dict[str, Any]:
    command_record = _json(directory / "command.json", 64 * 1024)
    process = _json(directory / "process.json", 16 * 1024)
    _require(isinstance(command_record, dict) and
             command_record.get("argv") == command and
             command_record.get("cwd") == str(cwd) and
             command_record.get("env") == (environment or {}) and
             isinstance(command_record.get("timeout_seconds"), (int, float)) and
             0 < command_record["timeout_seconds"] <= max_timeout and
             isinstance(process, dict) and process.get("return_code") == 0 and
             process.get("timed_out") is False and
             process.get("timeout_seconds") == command_record["timeout_seconds"],
             "original owning command or outcome is absent, stale, or failed")
    for name in ("stdout", "stderr"):
        data = _read(directory / f"{name}.log", 64 * 1024 * 1024)
        _require(len(data) == process.get(f"{name}_bytes") and
                 _sha(data) == process.get(f"{name}_sha256"),
                 "original owning command output was changed or truncated")
    return process


def _validate_gcc13_closure(sources: Any, command: Any) -> list[str]:
    rdp = _rdp_helpers()
    _require(isinstance(sources, list) and sources == sorted(set(sources)) and
             PHASE08_GCC_REQUIRED_UNITS.issubset(set(sources)),
             "GCC13 closure omits required Phase08 production or test C units")
    _require(isinstance(command, list) and
             command == rdp._gcc13_closure_command(sources, EXPECTED_GCC13_IMAGE),
             "GCC13 compiler command differs from its exact source unit set")
    script = command[command.index("sh") + 2]
    core_flags = next((line for line in script.splitlines() if line.startswith("core_flags=")), "")
    test_flags = next((line for line in script.splitlines() if line.startswith("test_flags=")), "")
    runner_flags = next((line for line in script.splitlines() if line.startswith("runner_flags=")), "")
    _require("-Wframe-larger-than=2048" in core_flags and
             "-Wframe-larger-than=" not in test_flags and
             "-Wframe-larger-than=" not in runner_flags and
             "-DT64_REAL_RUNNER_TESTING" in script and
             "-DT64_SUPERVISOR_TESTING" in script,
             "GCC13 core frame policy or real/test runner compile variants changed")
    return sources


def _verify_rdp_native_receipt(path: Path, source_sha: str,
                               normal_junit: Path) -> dict[str, Any]:
    rdp = _rdp_helpers()
    path = path.resolve()
    _require(_under(path, ROOT / "build/gate") and path.is_dir() and not path.is_symlink(),
             "shared native receipt is outside its explicit private gate tree")
    record_path = path / "source-binding.json"
    record = _json(record_path, 8 * 1024 * 1024)
    _require(normal_junit.is_absolute() and
             _under(normal_junit, ROOT / "build/gate/08-05-audio-private") and
             normal_junit.is_file() and not normal_junit.is_symlink() and
             _read(normal_junit, 16 * 1024 * 1024),
             "owning normal CTest JUnit is missing or outside the explicit private path")
    _require(isinstance(record, dict) and
             record.get("schema") == "t64.phase07-native-source-binding" and
             record.get("schema_version") == 1 and record.get("source_sha") == source_sha and
             record.get("source_binding_verified_after_all_commands") is True and
             record.get("normal_full_ctest_junit_path") == str(normal_junit) and
             record.get("phase07_source_base") == rdp.PHASE07_SOURCE_BASE and
             record.get("gcc13_image") == {
                 "image_id": EXPECTED_GCC13_IMAGE, "platform": "linux/amd64",
             }, "shared native receipt lacks exact source or pinned GCC13 identity")
    _require(record.get("source_binding") == rdp._native_source_snapshot(ROOT, source_sha),
             "shared native source/build/fixture input snapshot is stale")
    sources = record.get("phase07_changed_c_sources")
    gcc_script = ROOT / "build/gate/07-10-gcc13-command.sh"
    route = record.get("gcc13_reference_route")
    _require(isinstance(route, dict) and route.get("path") == "build/gate/07-10-gcc13-command.sh" and
             route.get("sha256") == _file_sha(gcc_script),
             "saved GCC13 reference route is missing or changed")
    expected_gcc = rdp._gcc13_closure_command(sources, EXPECTED_GCC13_IMAGE)
    _validate_gcc13_closure(sources, record.get("gcc13_command"))
    _require(record.get("gcc13_command") == expected_gcc and
             record.get("native_commands") == [
                 ["sh", "scripts/gate.sh"],
                 ["cmake", "--preset", "asan"],
                 ["cmake", "--build", "--preset", "asan", "--parallel", "1"],
                 ["ctest", "--preset", "asan", "--no-tests=error"],
             ], "shared native command plan differs from the reviewed bounded route")
    gcc_cmd = expected_gcc
    ctest = ["ctest", "--preset", "asan", "--no-tests=error", "--output-junit",
             str((path / "asan/full-ctest-junit.xml").resolve())]
    commands = {
        "gate": ["sh", "scripts/gate.sh"],
        "asan-configure": ["cmake", "--preset", "asan"],
        "asan-build": ["cmake", "--build", "--preset", "asan", "--parallel", "1"],
        "asan-full-ctest": ctest,
        "gcc13": gcc_cmd,
    }
    for label, argv in commands.items():
        environment = {"T64_GATE_JUNIT_OUT": str(normal_junit)} if label == "gate" else {}
        if label == "asan-full-ctest":
            environment = {
                "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
                "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
            }
        _verify_process_record(path / label, argv, max_timeout=2400,
                               environment=environment)
    _require((path / "asan/full-ctest-junit.xml").is_file() and
             (path / "asan/after-full-ctest-LastTest.log").is_file(),
             "shared ASan full-inventory originals are missing")
    return record


def _run_saved_stage(rdp: Any, directory: Path, command: list[str],
                     timeout_seconds: int,
                     extra_environment: dict[str, str] | None = None) -> dict[str, Any]:
    environment_delta = {"PYTHONDONTWRITEBYTECODE": "1", **(extra_environment or {})}
    environment = os.environ.copy()
    environment.update(environment_delta)
    outcome = rdp._run_bounded(
        command, cwd=ROOT, environment=environment,
        timeout_seconds=float(timeout_seconds),
    )
    rdp._save_process(directory, command, ROOT, environment_delta, outcome,
                      float(timeout_seconds))
    _require(outcome.get("process_group_empty") is True and
             outcome.get("timed_out") is False and outcome.get("return_code") == 0,
             f"bounded native stage failed or timed out: {directory.name}")
    return outcome


def _select_actual_capture_records(capture_section: str,
                                   build: Path) -> tuple[list[Path], list[Path]]:
    """Split runner.raw-capture's retained records into production and test-runner originals.

    The CTest retains one record per scenario: several run the production
    t64-supervisor, and the drain/resume pair runs t64-supervisor-test. Every
    production record is qualified; every other record must be positively
    identified as a test-supervisor original so none is silently ignored.
    """
    retained = [Path(value).resolve() for value in re.findall(
        r"^private audio runner evidence retained at (.+)$", capture_section,
        flags=re.MULTILINE,
    )]
    _require(bool(retained) and len(retained) == len(set(retained)),
             "full native suite does not retain distinct actual capture originals")
    production = str((build / "runner/t64-supervisor").resolve())
    test_runner = str((build / "runner/t64-supervisor-test").resolve())
    actual: list[Path] = []
    excluded: list[Path] = []
    for path in retained:
        _require(_under(path, build) and (path / "raw-bundle/manifest.json").is_file(),
                 "retained capture original is missing or outside the owning build")
        try:
            result = _json(path / "result.json", 16 * 1024)
            argv = _read(path / "command.args", 16 * 1024).decode("utf-8").splitlines()
        except (OSError, UnicodeDecodeError, json.JSONDecodeError, Refusal) as error:
            raise Refusal("retained capture original has no readable result or command") from error
        _require(bool(argv), "retained capture original has an empty command")
        if argv[0] == production and result.get("outcome") == "pass":
            actual.append(path)
        else:
            _require(argv[0] == test_runner,
                     "retained capture original is neither a production pass nor a test-runner record")
            excluded.append(path)
    _require(bool(actual), "original capture CTest has no accepted production-runner bundle")
    return actual, excluded


def _verify_discovery_last_test_retention(owning: bytes, before: bytes,
                                          after: bytes) -> None:
    """Require the full-suite log to survive discovery, which may replace LastTest.log.

    `ctest --show-only=json-v1` rewrites LastTest.log with an empty start/end log,
    so the after-discovery copy cannot equal the owning log. The owning log must be
    retained byte-for-byte before discovery, and discovery must not have run tests:
    the after copy is either that same log or a log with no test sections.
    """
    _require(before == owning,
             "owning full-suite LastTest changed before inventory discovery")
    text = after.decode("utf-8", errors="strict")
    discovery_only = (text.startswith("Start testing:") and "End testing:" in text and
                      re.search(r"^\d+/\d+ Testing: ", text, flags=re.MULTILINE) is None)
    _require(after == owning or discovery_only,
             "inventory discovery ran tests or left an unrecognized LastTest")


def _native_suite_records(build: Path, *, prefix: str, last_path: Path,
                          junit_path: Path) -> dict[str, Any]:
    """Bind exact authored cases to the full normal/ASan suite originals."""
    inventory_data = _read(INVENTORY, 128 * 1024)
    inventory = _validate_inventory(inventory_data, EXPECTED_INVENTORY_SHA256)
    authored = json.loads(inventory_data)
    selected = authored["suites"]["qualification"]
    expected_audio_ctests = selected["ctest"]
    last_bytes = _read(last_path, 32 * 1024 * 1024)
    last_text = last_bytes.decode("utf-8", errors="strict")
    last_names = re.findall(r"^\d+/\d+ Testing: (.+)$", last_text, flags=re.MULTILINE)
    _require(bool(last_names) and len(last_names) == len(set(last_names)) and
             all(re.search(rf"^\d+/\d+ Testing: {re.escape(name)}$", last_text,
                           flags=re.MULTILINE) for name in last_names),
             "full owning LastTest inventory is empty or duplicated")
    for name in last_names:
        _test_section(last_text, name)

    junit_root = ET.fromstring(_read(junit_path, 16 * 1024 * 1024))
    junit_names = [entry.attrib.get("name") for entry in junit_root.iter("testcase")]
    _require(junit_names and len(junit_names) == len(set(junit_names)) and
             set(junit_names) == set(last_names),
             "owning full-suite JUnit does not match its exact LastTest inventory")
    for name in junit_names:
        _junit_passed(junit_path, str(name))
    _require(set(expected_audio_ctests).issubset(set(last_names)),
             "full native suite did not reach every authored audio qualification CTest")

    discovery_paths = list(build.glob(f"{prefix}-qualification-*-discovery-stdout.log"))
    _require(len(discovery_paths) == 1,
             "non-running audio inventory check did not retain one exact discovery record")
    token = discovery_paths[0].name.removesuffix("-discovery-stdout.log")
    discovery_path = build / f"{token}-discovery-stdout.log"
    discovery_command_path = build / f"{token}-discovery-command.json"
    build_command_path = build / f"{token}-build-command.json"
    before_last = build / f"{token}-before-discovery-LastTest.log"
    after_last = build / f"{token}-after-discovery-LastTest.log"
    _require(all(path.is_file() and not path.is_symlink() for path in (
        discovery_path, discovery_command_path, build_command_path, before_last, after_last,
        build / f"{token}-build-stdout.log", build / f"{token}-build-stderr.log",
        build / f"{token}-discovery-stderr.log",
    )), "non-running authored inventory originals are incomplete")
    expected_build_command = ["cmake", "--build", str(build.resolve()), "--parallel", "1"]
    expected_discovery_command = ["ctest", "--test-dir", str(build.resolve()),
                                  "--show-only=json-v1"]
    _require(_json(build_command_path, 16 * 1024) == expected_build_command and
             _json(discovery_command_path, 16 * 1024) == expected_discovery_command,
             "inventory build or discovery command changed")
    _verify_discovery_last_test_retention(
        last_bytes, _read(before_last, 32 * 1024 * 1024),
        _read(after_last, 32 * 1024 * 1024),
    )
    discovery = _json(discovery_path, 16 * 1024 * 1024)
    _require(isinstance(discovery, dict) and isinstance(discovery.get("tests"), list),
             "full native CTest discovery record is malformed")
    discovered = [entry.get("name") for entry in discovery["tests"]
                  if isinstance(entry, dict)]
    _require(len(discovered) == len(set(discovered)) and
             set(discovered) == set(last_names) and
             set(expected_audio_ctests).issubset(set(discovered)),
             "owning full suite and discovered test inventory differ")

    capture_section = _test_section(last_text, "runner.raw-capture")
    capture_records, excluded_records = _select_actual_capture_records(
        capture_section, build.resolve())
    _require("case reached: actual-av-bundle" in capture_section and
             "case reached: literal-pcm-files" in capture_section and
             "case reached: capture-drain-resume" in capture_section,
             "accepted consumer, independent sample, or drain assertions were not reached")
    summaries = [_capture_record(path, build, last_path, junit_path)
                 for path in capture_records]
    _require(all(item["capture_manifest_sha256"] == summaries[0]["capture_manifest_sha256"] and
                 item["result"].get("state_digest") == summaries[0]["result"].get("state_digest")
                 for item in summaries),
             "accepted production capture records disagree on packets or canonical state")
    summary = summaries[0]
    summary.update({
        "capture_records": [item["private_record"] for item in summaries],
        "excluded_test_runner_records": [str(path) for path in excluded_records],
        "inventory": inventory,
        "suite": "qualification",
        "artifact_prefix": prefix,
        "artifact_token": token,
        "last_test_path": str(last_path.resolve()),
        "junit_path": str(junit_path.resolve()),
        "full_ctest_names": last_names,
        "discovered_ctest_names": discovered,
        "checker_artifact_paths": [str(path) for path in (
            build_command_path, discovery_command_path, discovery_path,
            before_last, after_last, build / f"{token}-build-stdout.log",
            build / f"{token}-build-stderr.log",
            build / f"{token}-discovery-stderr.log",
        )],
    })
    return summary


def _native_qualify(source_sha: str, output_dir: Path) -> Path:
    _require(SHA1_RE.fullmatch(source_sha) is not None and
             _git(["rev-parse", "HEAD"]).decode().strip() == source_sha,
             "native source SHA must exactly match the checked-out revision")
    private_root = ROOT / "build/gate/08-05-audio-private"
    if private_root.exists() or private_root.is_symlink():
        _require(private_root.is_dir() and not private_root.is_symlink(),
                 "native private root is unsafe")
    else:
        private_root.mkdir(parents=True, mode=0o700)
    _require(not output_dir.is_symlink(), "native output path is a symlink")
    output = output_dir.resolve()
    _require(_under(output, private_root) and not output.exists() and not output.is_symlink(),
             "native output must be a fresh private directory under build/gate")
    output.mkdir(parents=True, mode=0o700)
    source_before = _source_snapshot(source_sha, require_clean=True)
    prefix = f"08-05-{source_sha[:8]}-{time.time_ns()}"
    normal_junit = (output / "normal-full-ctest-junit.xml").resolve()
    plan = _native_command_plan(prefix, normal_junit)
    rdp = _rdp_helpers()
    steps_dir = output / "steps"
    steps_dir.mkdir(mode=0o700)
    _run_saved_stage(rdp, steps_dir / "shared-native-gates", plan[0]["command"],
                     plan[0]["timeout_seconds"], plan[0]["environment"])
    native_stdout = _read(steps_dir / "shared-native-gates/stdout.log", 8 * 1024 * 1024).decode()
    matches = re.findall(
        r"^qualify-rdp-vi: private owning command, process, LastTest, and JUnit records: (.+)$",
        native_stdout, flags=re.MULTILINE,
    )
    _require(len(matches) == 1, "shared native runner did not return one explicit original receipt")
    rdp_native_path = Path(matches[0]).resolve()
    rdp_record = _verify_rdp_native_receipt(rdp_native_path, source_sha, normal_junit)
    suite_records: dict[str, Any] = {}
    for inventory_plan, source_plan, build_name, artifact_suffix, junit_path, last_path in (
        (plan[1], plan[3], "gate", "normal", normal_junit,
         rdp_native_path / "normal/after-gate-LastTest.log"),
        (plan[2], plan[4], "asan", "asan",
         rdp_native_path / "asan/full-ctest-junit.xml",
         rdp_native_path / "asan/after-full-ctest-LastTest.log"),
    ):
        _run_saved_stage(rdp, steps_dir / inventory_plan["label"],
                         inventory_plan["command"], inventory_plan["timeout_seconds"])
        inventory_stdout = _read(
            steps_dir / inventory_plan["label"] / "stdout.log", 8 * 1024 * 1024,
        )
        _require(b"validated nonempty qualification audio case inventory" in inventory_stdout,
                 f"{artifact_suffix} authored inventory check did not reach its pass marker")
        build = (ROOT / "build" / build_name).resolve()
        _run_saved_stage(rdp, steps_dir / source_plan["label"], source_plan["command"],
                         source_plan["timeout_seconds"])
        body_stdout = _read(steps_dir / source_plan["label"] / "stdout.log", 4 * 1024 * 1024)
        _require(body_stdout.startswith(b"PASS: rsp-no-hle"),
                 f"{artifact_suffix} native source/body/object check did not pass")
        summary = _native_suite_records(
            build, prefix=f"{prefix}-{artifact_suffix}", last_path=last_path,
            junit_path=junit_path,
        )
        retained = output / artifact_suffix
        _require(not retained.exists() and not retained.is_symlink(),
                 "native retained originals destination already exists")
        retained.mkdir(mode=0o700)
        _retain_originals(retained, build, last_path, junit_path, summary, source_before)
        _write_json_private(retained / "summary-binding.json", summary)
        suite_records[artifact_suffix] = {
            "build_dir": str(build),
            "artifact_prefix": f"{prefix}-{artifact_suffix}",
            "summary": summary,
            "retained_path": str(retained),
            "retained_manifest": _tree_manifest(retained),
        }
        _require(_source_snapshot(source_sha, require_clean=True) == source_before,
                 "source inputs changed during native audio qualification")
    _require(_source_snapshot(source_sha, require_clean=True) == source_before,
             "source inputs changed before native qualification could be sealed")
    record = {
        "schema": "t64.audio-native-qualification",
        "schema_version": 1,
        "evidence_mode": "native-qualification",
        "status": "complete",
        "source_sha": source_sha,
        "source_snapshot": source_before,
        "excluded_owner_metadata": _owner_metadata_snapshot(),
        "inventory": _validate_inventory(_read(INVENTORY, 128 * 1024),
                                          EXPECTED_INVENTORY_SHA256),
        "normal_full_ctest_junit_path": str(normal_junit),
        "shared_native_path": str(rdp_native_path),
        "shared_native_record_sha256": _file_sha(rdp_native_path / "source-binding.json"),
        "shared_native_tree_manifest": _tree_manifest(rdp_native_path),
        "shared_native_closure": {
            "units": rdp_record["phase07_changed_c_sources"],
            "gcc13_command": rdp_record["gcc13_command"],
        },
        "steps_path": str(steps_dir),
        "suites": suite_records,
        "output_path": str(output),
    }
    _write_json_private(output / "native-record.json", record)
    return output / "native-record.json"


def _write_json_private(path: Path, value: Any) -> None:
    payload = json.dumps(value, sort_keys=True, separators=(",", ":")).encode() + b"\n"
    with path.open("xb") as stream:
        stream.write(payload)
    path.chmod(0o600)


def _validate_native_record_header(record: Any, expected_sha: str) -> dict[str, Any]:
    _require(isinstance(record, dict) and
             record.get("schema") == "t64.audio-native-qualification" and
             record.get("schema_version") == 1 and
             record.get("evidence_mode") == "native-qualification" and
             record.get("status") == "complete" and
             record.get("source_sha") == expected_sha and
             isinstance(record.get("source_snapshot"), dict) and
             isinstance(record.get("excluded_owner_metadata"), list) and
             isinstance(record.get("normal_full_ctest_junit_path"), str) and
             isinstance(record.get("shared_native_path"), str) and
             isinstance(record.get("shared_native_record_sha256"), str) and
             SHA256_RE.fullmatch(record["shared_native_record_sha256"]) is not None and
             isinstance(record.get("shared_native_tree_manifest"), list) and
             isinstance(record.get("steps_path"), str) and
             isinstance(record.get("suites"), dict) and
             set(record["suites"]) == {"normal", "asan"},
             "native evidence is smoke-only, incomplete, or missing a required suite")
    closure = record.get("shared_native_closure")
    _require(isinstance(closure, dict) and
             isinstance(closure.get("units"), list) and
             PHASE08_GCC_REQUIRED_UNITS.issubset(set(closure["units"])) and
             isinstance(closure.get("gcc13_command"), list),
             "native evidence lacks the Phase08 GCC13 source closure")
    inventory = record.get("inventory")
    expected_inventory = _validate_inventory(_read(INVENTORY, 128 * 1024),
                                             EXPECTED_INVENTORY_SHA256)
    _require(inventory == expected_inventory,
             "native evidence is missing or reducing the authored case denominator")
    return record


def _verify_native_record(path: Path, expected_sha: str) -> dict[str, Any]:
    _require(not path.is_symlink(), "native record path is a symlink")
    path = path.resolve()
    private_root = ROOT / "build/gate/08-05-audio-private"
    _require(_under(path, private_root) and path.name == "native-record.json" and
             path.is_file() and not path.is_symlink(),
             "explicit native record is outside the private qualification tree")
    record = _validate_native_record_header(_json(path, 16 * 1024 * 1024), expected_sha)
    output = path.parent
    _require(record.get("output_path") == str(output) and
             record.get("steps_path") == str(output / "steps") and
             record.get("normal_full_ctest_junit_path") ==
             str((output / "normal-full-ctest-junit.xml").resolve()),
             "native record paths differ from its explicit private directory")
    current_source = _source_snapshot(expected_sha, require_clean=True)
    _require(record.get("source_snapshot") == current_source,
             "native source inputs differ from the recorded clean snapshot")
    _require(record.get("excluded_owner_metadata") == _owner_metadata_snapshot() and
             [entry.get("path") for entry in record["excluded_owner_metadata"]] ==
             list(OWNER_METADATA_PATHS),
             "known excluded owner metadata changed or has an unexpected path")
    rdp_path = Path(record["shared_native_path"])
    _require(not rdp_path.is_symlink(), "shared native record path is a symlink")
    rdp_path = rdp_path.resolve()
    _require(record.get("shared_native_record_sha256") ==
             _file_sha(rdp_path / "source-binding.json") and
             record.get("shared_native_tree_manifest") == _tree_manifest(rdp_path),
             "shared native command/original record files changed")
    rdp_record = _verify_rdp_native_receipt(
        rdp_path, expected_sha, Path(record["normal_full_ctest_junit_path"]),
    )
    _require(record["shared_native_closure"] == {
        "units": rdp_record["phase07_changed_c_sources"],
        "gcc13_command": rdp_record["gcc13_command"],
    }, "native GCC13 source and command binding is stale")
    prefix = record["suites"]["normal"].get("artifact_prefix")
    _require(isinstance(prefix, str) and prefix.startswith("08-05-") and
             prefix.endswith("-normal") and
             record["suites"]["asan"].get("artifact_prefix") == prefix[:-7] + "-asan",
             "normal and ASan suite prefixes do not belong to the same native attempt")
    plan = _native_command_plan(prefix[:-7], Path(record["normal_full_ctest_junit_path"]))
    for index, key in enumerate((
        "shared-native-gates", "normal-audio-qualification", "asan-audio-qualification",
        "normal-source-body-check", "asan-source-body-check",
    )):
        item = plan[index]
        _verify_process_record(output / "steps" / key, item["command"],
                               max_timeout=item["timeout_seconds"],
                               environment={"PYTHONDONTWRITEBYTECODE": "1",
                                            **item["environment"]})
        stdout = _read(output / "steps" / key / "stdout.log", 16 * 1024 * 1024)
        if "audio-qualification" in key:
            _require(b"validated nonempty qualification audio case inventory" in stdout,
                     "native audio inventory pass marker is missing")
        if "source-body-check" in key:
            _require(stdout.startswith(b"PASS: rsp-no-hle"),
                     "native source/body/object pass marker is missing")
    for suite_name, build_name, artifact_suffix in (
        ("normal", "gate", "normal"), ("asan", "asan", "asan"),
    ):
        selected = record["suites"][suite_name]
        build = (ROOT / "build" / build_name).resolve()
        _require(selected.get("build_dir") == str(build) and
                 selected.get("retained_path") == str(output / artifact_suffix),
                 f"{suite_name} inventory does not use its explicit owning build directory")
        last_path = Path(rdp_path / (
            "normal/after-gate-LastTest.log" if suite_name == "normal" else
            "asan/after-full-ctest-LastTest.log"
        ))
        junit_path = Path(record["normal_full_ctest_junit_path"] if suite_name == "normal" else
                          rdp_path / "asan/full-ctest-junit.xml")
        # The JUnit is emitted by the owning full CTest invocation; no nested suite is run here.
        summary = _native_suite_records(
            build, prefix=selected["artifact_prefix"], last_path=last_path,
            junit_path=junit_path,
        )
        _require(selected.get("summary") == summary and
                 selected.get("retained_manifest") == _tree_manifest(output / artifact_suffix),
                 f"{suite_name} owning command, result, binaries, or originals changed")
    return record


def _scan_public(directory: Path, expected: dict[str, Any] | None = None) -> dict[str, Any]:
    _require(directory.is_dir() and not directory.is_symlink(),
             "public evidence directory is absent or unsafe")
    entries = list(directory.iterdir())
    _require(len(entries) == 1 and entries[0].name == "manifest.json" and
             entries[0].is_file() and not entries[0].is_symlink(),
             "public upload contains an unexpected, raw, or missing member")
    data = _read(entries[0], 16 * 1024)
    patterns = (
        re.compile(rb"(?i)/(?:users|home|private/var/folders)/[^\s\"']+"),
        re.compile(rb"(?i)[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,}"),
        re.compile(rb"(?i)(?:rom|pcm|frame|packet)[^\" ]*\.?(?:z64|s16be|rgba8|bin)"),
    )
    _require(not any(pattern.search(data) for pattern in patterns),
             "public projection contains a local path, personal identifier, or raw artifact name")
    projection = json.loads(data)
    if expected is not None and expected.get("job") in {"gate-linux", "gate-macos"}:
        value = _validate_hosted_context(
            projection, repo="szTheory/tendies64", sha=expected.get("sha"),
            run=expected.get("run"), attempt=expected.get("attempt"),
            job=expected.get("job"),
        )
    else:
        value = _public_projection(projection, expected)
    _require(len(data) <= 16 * 1024, "public projection exceeds its bounded size")
    return value


def _producer_result(record: Path) -> dict[str, Any]:
    result = _json(record / "result.json", 16 * 1024)
    producer = _json(record / "producer-report.json", 16 * 1024)
    _reports_agree(result, producer)
    return result


def _reports_agree(result: dict[str, Any], producer: dict[str, Any]) -> None:
    for field in ("outcome", "detail", "guest_tick", "state_digest"):
        _require(result.get(field) == producer.get(field),
                 f"original producer and supervisor reports disagree on {field}")


def _verify_pcm_literal(data: bytes) -> None:
    _require(len(data) == len(PCM) and data == PCM,
             "captured samples differ from the independently authored PCM literals")


def _packet_file(path: Path, expected_size: int) -> bytes:
    data = _read(path, max(expected_size, 1))
    _require(expected_size > 0 and len(data) == expected_size,
             f"packet file is zero, truncated, or has an unexpected size: {path.name}")
    return data


def _verify_race(build: Path, last_test: str, junit: Path,
                 accepted_digest: str) -> dict[str, Any]:
    section = _test_section(last_test, "controls.real.raw-capture-race")
    _junit_passed(junit, "controls.real.raw-capture-race")
    _require("case reached: two actual runners crossed the shared precommit barrier" in section and
             "one published the baseline bundle and one reported capture-no-replace-commit" in section,
             "native no-replace consumer case was not reached")
    command = _parse_command(section)
    expected = [
        str((build / "runner/test_audio_capture").resolve()), "capture-race",
        str((build / "runner/t64-supervisor-test").resolve()),
        str((build / "runner/t64-result-check").resolve()),
        str((ROOT / "build/fixtures/audio-pcm.z64").resolve()),
        str((ROOT / "build/fixtures/hello.input.json").resolve()),
    ]
    _require(command == expected, "no-replace CTest argv differs from the authored race command")
    stage_match = re.search(
        r"^case reached: source staging-container substitution preserved the accepted bundle; "
        r"baseline=(.+) target=(.+) marker=(.+) moved=(.+)$",
        section, flags=re.MULTILINE,
    )
    _require(stage_match is not None,
             "source staging-container substitution control was not reached")
    stage_baseline_target = Path(stage_match.group(1)).resolve()
    stage_target = Path(stage_match.group(2)).resolve()
    stage_marker = Path(stage_match.group(3)).resolve()
    moved_stage = Path(stage_match.group(4)).resolve()
    paths = re.findall(r"^private audio runner evidence retained at (.+)$", section,
                       flags=re.MULTILINE)
    _require(len(paths) == 5,
             "race original does not retain the finalization race and stage-substitution records")
    records = [Path(value).resolve() for value in paths]
    _require(all(_under(path, build) for path in records) and
             all(_under(path, build) for path in (
                 stage_baseline_target, stage_target, stage_marker, moved_stage,
             )),
             "race record escapes the explicit build directory")
    record_data: list[tuple[Path, list[str], dict[str, Any], Path]] = []
    stage_data: dict[str, tuple[Path, list[str], dict[str, Any]]] = {}
    race_records: list[Path] = []
    for record in records:
        argv = _read(record / "command.args", 16 * 1024).decode("utf-8").splitlines()
        result = _producer_result(record)
        _require(argv and argv[0] == expected[2],
                 "race record is not owned by the test supervisor")
        _require(argv.count("--raw-capture-dir") == 1 and
                 argv.index("--raw-capture-dir") + 1 < len(argv),
                 "race record does not identify one raw target")
        target = Path(argv[argv.index("--raw-capture-dir") + 1]).resolve()
        if target in {stage_baseline_target, stage_target}:
            _require(str(target) not in stage_data,
                     "stage-substitution original has a duplicate producer record")
            stage_data[str(target)] = (record, argv, result)
        else:
            race_records.append(record)
            record_data.append((record, argv, result, target))
    _require(len(stage_data) == 2 and
             set(stage_data) == {str(stage_baseline_target), str(stage_target)} and
             len(race_records) == 3,
             "race original does not partition into one stage-substitution pair and one finalization race")
    stage_baseline = stage_data[str(stage_baseline_target)]
    stage_publisher = stage_data[str(stage_target)]
    _require(stage_baseline[1].count("--test-fault") == 0 and
             stage_publisher[1].count("--test-fault") == 1 and
             stage_publisher[1].index("--test-fault") + 1 < len(stage_publisher[1]) and
             stage_publisher[1][stage_publisher[1].index("--test-fault") + 1] ==
             "capture-precommit-barrier" and
             all(item[2].get("outcome") == "pass" and
                 item[2].get("detail") == "positive-match" and
                 item[2].get("guest_tick") == "551001" and
                 item[2].get("state_digest") == accepted_digest
                 for item in (stage_baseline, stage_publisher)),
             "stage-substitution records are not two accepted complete guest runs")

    def bundle_files(bundle: Path) -> dict[str, bytes]:
        _require(bundle.is_dir() and not bundle.is_symlink(),
                 "stage-substitution bundle is missing or unsafe")
        result: dict[str, bytes] = {}
        total = 0
        for path in sorted(bundle.rglob("*")):
            metadata = path.lstat()
            _require(not path.is_symlink() and (path.is_dir() or path.is_file()),
                     "stage-substitution bundle contains a symlink or special file")
            if not path.is_file():
                continue
            data = _read(path, 64 * 1024 * 1024)
            total += len(data)
            _require(total <= 64 * 1024 * 1024 and len(result) < 10000,
                     "stage-substitution bundle exceeds its bounded file inventory")
            result[path.relative_to(bundle).as_posix()] = data
        return result

    _require(bundle_files(stage_baseline_target) == bundle_files(stage_target),
             "source stage-entry replacement did not publish the original accepted bundle")
    marker_status = stage_marker.lstat()
    _require(stage_marker.is_file() and not stage_marker.is_symlink() and
             marker_status.st_mode & 0o777 == 0o600 and
             _read(stage_marker, 64) == b"FOREIGN-STAGE",
             "foreign stage replacement marker was removed or changed")
    _require(moved_stage.is_dir() and not moved_stage.is_symlink() and
             not any(moved_stage.iterdir()),
             "renamed private staging parent did not remain empty after publication")

    contenders: list[tuple[Path, list[str], dict[str, Any]]] = []
    baseline: tuple[Path, list[str], dict[str, Any]] | None = None
    for record, argv, result, _target in record_data:
        fault_count = argv.count("--test-fault")
        if fault_count == 0:
            baseline = (record, argv, result)
        else:
            _require(fault_count == 1 and argv.index("--test-fault") + 1 < len(argv) and
                     argv[argv.index("--test-fault") + 1] == "capture-precommit-barrier",
                     "race contender command does not use the actual precommit barrier")
            contenders.append((record, argv, result))
    _require(baseline is not None and len(contenders) == 2,
             "race did not retain exactly one baseline and two barrier contenders")
    contender_targets = [
        Path(argv[argv.index("--raw-capture-dir") + 1]).resolve()
        for _, argv, _ in contenders
    ]
    _require(len(set(map(str, contender_targets))) == 1,
             "race contenders did not target the same final path")
    outcomes = sorted((item[2].get("outcome"), item[2].get("detail")) for item in contenders)
    _require(outcomes == [("harness-error", "capture-no-replace-commit"), ("pass", "positive-match")],
             "race result pair is not exactly one publish and one no-replace refusal")
    for _, _, result in contenders:
        _require(result.get("guest_tick") == "551001" and
                 result.get("state_digest") == accepted_digest,
                 "race result differs from the accepted canonical completion boundary")
    _require(baseline[2].get("outcome") == "pass" and
             baseline[2].get("state_digest") == accepted_digest,
             "race baseline differs from the accepted canonical completion boundary")
    final_target = contender_targets[0]
    baseline_target = Path(
        baseline[1][baseline[1].index("--raw-capture-dir") + 1]
    ).resolve()
    _require(_under(baseline_target, build) and baseline_target.is_dir() and
             not baseline_target.is_symlink(),
             "no-replace pre-race baseline bundle is missing or unsafe")
    _require(final_target.is_dir() and not final_target.is_symlink(),
             "no-replace race winner did not retain its final target")
    winner_manifest = _read(final_target / "manifest.json", 1024 * 1024)
    _require(_sha(winner_manifest) == _sha(_read(baseline_target / "manifest.json", 1024 * 1024)),
             "no-replace winner differs from the retained completed baseline bundle")
    marker = re.search(r"with digest ([0-9a-f]{64})$", section, flags=re.MULTILINE)
    _require(marker is not None and marker.group(1) == accepted_digest,
             "race output digest differs from its original contender reports")
    _require(sys.platform in {"linux", "darwin"},
             "no-replace witness came from an unsupported host platform")
    binary_hashes = {
        name: _file_sha(build / name) for name in (
            "runner/test_audio_capture", "runner/t64-runner-test",
            "runner/t64-supervisor-test", "runner/t64-result-check",
        )
    }
    contender_hashes = sorted(
        _sha("\n".join(argv).encode("utf-8")) for _, argv, _ in contenders
    )
    return {
        "platform": "linux" if sys.platform == "linux" else "macos",
        "ctest_command_sha256": _sha(json.dumps(command, separators=(",", ":")).encode()),
        "test_binary_sha256": binary_hashes["runner/test_audio_capture"],
        "runner_binary_sha256": binary_hashes["runner/t64-runner-test"],
        "supervisor_binary_sha256": binary_hashes["runner/t64-supervisor-test"],
        "checker_binary_sha256": binary_hashes["runner/t64-result-check"],
        "contender_command_sha256": contender_hashes,
        "outcomes": ["harness-error:capture-no-replace-commit", "pass:positive-match"],
        "private_records": [str(path) for path in race_records],
        "stage_replacement_records": [
            str(stage_baseline[0]), str(stage_publisher[0]),
        ],
        "stage_replacement_baseline": str(stage_baseline_target),
        "stage_replacement_target": str(stage_target),
        "stage_replacement_marker": str(stage_marker),
        "stage_replacement_moved_parent": str(moved_stage),
        "private_target": str(final_target),
    }


def _json_string(value: Any, expected: str, label: str) -> None:
    _require(isinstance(value, str) and value == expected, f"{label} identity differs")


def _verify_capture_identities(result: dict[str, Any], manifest: dict[str, Any]) -> None:
    _json_string(result.get("profile_id"), PROFILE_ID, "profile")
    _json_string(result.get("profile_digest"), PROFILE_SHA256, "profile digest")
    _json_string(result.get("fixture_id"), FIXTURE_ID, "fixture")
    _json_string(result.get("fixture_sha256"), FIXTURE_SHA256, "fixture digest")
    _json_string(result.get("input_digest"), INPUT_SHA256, "input digest")
    _json_string(manifest.get("profile_id"), PROFILE_ID, "bundle profile")
    _json_string(manifest.get("profile_sha256"), PROFILE_SHA256, "bundle profile digest")
    _json_string(manifest.get("fixture_id"), FIXTURE_ID, "bundle fixture")
    _json_string(manifest.get("fixture_sha256"), FIXTURE_SHA256, "bundle fixture digest")
    _json_string(manifest.get("input_digest"), INPUT_SHA256, "bundle input digest")
    _require(isinstance(result.get("source_revision"), str) and
             SHA1_RE.fullmatch(result["source_revision"]) is not None and
             manifest.get("source_revision") == result["source_revision"] and
             isinstance(result.get("build_id"), str) and
             re.fullmatch(r"t64-build-v1-[0-9a-f]{64}", result["build_id"]) is not None and
             manifest.get("core_build_id") == result.get("build_id"),
             "producer source revision or semantic core build identity differs")
    _require(manifest.get("runner_build_id") is not None and
             re.fullmatch(r"t64-runner-v1-[0-9a-f]{64}",
                          str(manifest.get("runner_build_id"))) is not None,
             "runner semantic build identity is malformed")


def _audio_observation_native_size() -> int:
    _require((sys.platform.startswith("linux") or sys.platform == "darwin") and
             sys.byteorder == "little" and
             ctypes.sizeof(ctypes.c_void_p) == 8 and
             ctypes.sizeof(ctypes.c_size_t) == 8,
             "audio observation native representation is unsupported")
    _require(ctypes.sizeof(_TAudioPacketInfo) == 280 and
             ctypes.sizeof(_TAudioObservation) == 3976,
             "audio observation version 2 native layout differs")
    return ctypes.sizeof(_TAudioObservation)


def _audio_observation_text(info: _TAudioPacketInfo, field: str,
                            expected: str) -> bool:
    descriptor = getattr(_TAudioPacketInfo, field)
    actual = ctypes.string_at(ctypes.addressof(info) + descriptor.offset,
                              descriptor.size)
    encoded = expected.encode("ascii")
    return (len(encoded) < len(actual) and actual[:len(encoded)] == encoded and
            actual[len(encoded):] == bytes(len(actual) - len(encoded)))


def _audio_observation_uint(value: Any, name: str) -> int:
    _require(isinstance(value, str) and len(value) <= 20 and
             re.fullmatch(r"[0-9]+", value) is not None,
             f"audio observation {name} is not an unsigned decimal literal")
    parsed = int(value)
    _require(parsed <= 0xFFFFFFFFFFFFFFFF,
             f"audio observation {name} exceeds its native field width")
    return parsed


def _verify_audio_observation(data: bytes, manifest: dict[str, Any],
                              audio_entries: list[dict[str, Any]],
                              packet_data: list[bytes],
                              result: dict[str, Any]) -> None:
    expected_size = _audio_observation_native_size()
    _require(len(data) == expected_size,
             "original PCM observation has the wrong native record size")
    observation = _TAudioObservation.from_buffer_copy(data)
    _require(observation.magic == 0x54363441 and observation.version == 2,
             "original PCM observation schema or version differs")
    _require(observation.packet_count == 2 and
             observation.bus_write_count == 16 and
             observation.sw_retirement_count == 15 and
             observation.trace_overflow == 0 and
             observation.mi_ai_read_seen == 1 and
             observation.guest_mi_snapshot_seen == 1 and
             observation.guest_stop_reason == 8 and
             observation.yield_count <= 65,
             "original PCM observation counters or completion metadata differ")
    stop_tick = _audio_observation_uint(result.get("guest_tick"),
                                        "result guest tick")
    _require(stop_tick == 551001 and observation.guest_stop_tick == stop_tick and
             observation.guest_last_cpu_tick <= stop_tick and
             observation.mi_ai_read_tick <= stop_tick,
             "original PCM observation completion tick differs from its result")
    _require(len(audio_entries) == 2 and len(packet_data) == 2,
             "original PCM observation packet denominator differs")
    build_id = manifest.get("core_build_id")
    _require(isinstance(build_id, str) and
             re.fullmatch(r"t64-build-v1-[0-9a-f]{64}", build_id) is not None,
             "original PCM observation build identity is malformed")
    expected_samples = (0, 4)
    expected_ticks: list[tuple[int, int]] = []
    for index, (entry, raw_packet) in enumerate(zip(audio_entries, packet_data)):
        _require(isinstance(entry, dict),
                 "raw audio packet metadata is malformed")
        expected = {
            "sequence": index + 1,
            "first_sample_index": expected_samples[index],
            "frame_count": 4,
            "rate_numerator": 48681812,
            "rate_denominator": 1104,
            "channels": 2,
            "bits_per_sample": 16,
            "byte_length": 16,
        }
        info = observation.packets[index].info
        _require(info.struct_size == ctypes.sizeof(_TAudioPacketInfo) and
                 info.sequence == expected["sequence"] and
                 info.first_sample_index == expected["first_sample_index"] and
                 info.frame_count == expected["frame_count"] and
                 info.rate_numerator == expected["rate_numerator"] and
                 info.rate_denominator == expected["rate_denominator"] and
                 info.channels == expected["channels"] and
                 info.bits_per_sample == expected["bits_per_sample"] and
                 info.encoding == 1 and info.byte_length == expected["byte_length"],
                 f"PCM observation packet {index} metadata differs from the authored literal")
        for field, value in expected.items():
            _require(_audio_observation_uint(entry.get(field), field) == value,
                     f"raw audio packet {index} {field} differs from its independent literal")
        first_tick = _audio_observation_uint(entry.get("first_tick"), "first tick")
        last_tick = _audio_observation_uint(entry.get("last_tick"), "last tick")
        _require(info.first_tick == first_tick and info.last_tick == last_tick and
                 first_tick <= last_tick < stop_tick and
                 entry.get("encoding") == "s16be-interleaved-stereo",
                 f"PCM observation packet {index} tick or encoding metadata differs from its raw bundle")
        expected_ticks.append((first_tick, last_tick))
        _require(len(raw_packet) == 16 and
                 bytes(observation.packets[index].bytes) == raw_packet and
                 raw_packet == PCM[index * 16:(index + 1) * 16],
                 f"PCM observation packet {index} bytes differ from the accepted raw bundle or literal")
        _require(_audio_observation_text(info, "profile_id", PROFILE_ID) and
                 _audio_observation_text(info, "build_id", build_id),
                 f"PCM observation packet {index} profile or build identity differs")
    _require(expected_ticks[0][1] < expected_ticks[1][0],
             "PCM observation packet tick ranges overlap or are out of order")
    _verify_audio_observation_tail(observation, stop_tick, expected_ticks, result)


def _audio_region_is_zero(value: Any) -> bool:
    return ctypes.string_at(ctypes.addressof(value), ctypes.sizeof(value)) == \
        bytes(ctypes.sizeof(value))


def _verify_audio_observation_tail(observation: _TAudioObservation, stop_tick: int,
                                   packet_ticks: list[tuple[int, int]],
                                   result: dict[str, Any]) -> None:
    """Bind the trace, retirement, yield and guest-owned fields to the authored fixture."""
    _require(b"".join(word.to_bytes(4, "big") for word in AUDIO_PCM_WORDS) == PCM,
             "authored PCM store words differ from the authored PCM literal")
    _require(observation.guest_first_escape_pc == 0 and
             observation.guest_first_escape_tick == 0 and
             observation.guest_first_exception_seen == 0 and
             observation.guest_first_exception_tick == 0 and
             observation.guest_first_exception_epc == 0 and
             observation.guest_first_exception_preceding_tick == 0 and
             observation.guest_first_exception_preceding_pc == 0 and
             observation.guest_first_exception_state == 0 and
             observation.guest_first_exception_mi == 0,
             "original PCM observation records a guest escape or exception")
    snapshot = observation.guest_mi_snapshot
    _require((snapshot & 0x04) != 0 and (snapshot & ~0x3F) == 0,
             "original PCM observation MI snapshot lacks the AI interrupt or is not a register value")

    expected_writes = [(0x3400 + 4 * index, word)
                       for index, word in enumerate(AUDIO_PCM_WORDS)]
    expected_writes.extend(AUDIO_AI_WRITES)
    previous_tick = 0
    for index, (address, value) in enumerate(expected_writes):
        write = observation.bus_writes[index]
        _require(write.kind == TRACE_KIND_BUS_WRITE32 and write.address == address and
                 write.value == value and write.tick > previous_tick,
                 f"PCM observation bus write {index} differs from the authored fixture")
        previous_tick = write.tick
    snapshot_write = observation.bus_writes[len(expected_writes)]
    _require(snapshot_write.kind == TRACE_KIND_BUS_WRITE32 and
             snapshot_write.address == AUDIO_MI_SNAPSHOT_ADDRESS and
             snapshot_write.value == snapshot and snapshot_write.tick > previous_tick,
             "PCM observation MI snapshot write differs from the authored fixture")

    expected_stores = [AUDIO_PCM_STORE] * len(AUDIO_PCM_WORDS) + list(AUDIO_AI_STORES)
    for index, (pc, opcode) in enumerate(expected_stores):
        store = observation.sw_retirements[index]
        _require(store.pc == pc and store.opcode == opcode and
                 store.tick == observation.bus_writes[index].tick,
                 f"PCM observation store retirement {index} differs from the authored fixture")
    _require(_audio_region_is_zero(observation.sw_retirements[len(expected_stores)]),
             "PCM observation has a store retirement beyond its authored count")

    ai_queued_tick = observation.bus_writes[len(expected_writes) - 1].tick
    _require(packet_ticks[0][0] > ai_queued_tick and
             snapshot_write.tick > packet_ticks[1][1] and
             observation.mi_ai_read_tick >= packet_ticks[1][1] and
             observation.guest_last_cpu_tick >= max(observation.mi_ai_read_tick,
                                                    snapshot_write.tick) and
             observation.guest_last_cpu_tick <= stop_tick,
             "PCM observation guest progress ticks differ from the authored fixture order")
    _require(observation.guest_last_cpu_pc == AUDIO_LAST_CPU_PC,
             "PCM observation last retired instruction differs from the authored fixture")
    instruction_count = _audio_observation_uint(result.get("instruction_count"),
                                                "result instruction count")
    _require(observation.trace_records >= instruction_count + observation.bus_write_count,
             "PCM observation trace record count is below its retired instructions and writes")

    for index in range(len(observation.yields)):
        entry = observation.yields[index]
        if index >= observation.yield_count:
            _require(_audio_region_is_zero(entry),
                     "PCM observation has a resource yield beyond its recorded count")
            continue
        _require(0 < entry.target_tick and entry.before_tick <= entry.reached_tick and
                 entry.reached_tick <= entry.target_tick and
                 entry.reached_tick <= stop_tick and
                 entry.video_before <= entry.video_after and
                 entry.audio_before <= entry.audio_after <= 2,
                 f"PCM observation resource yield {index} is out of bounds")


def _capture_record(record: Path, build: Path, last_test_path: Path,
                    junit_path: Path) -> dict[str, Any]:
    record = record.resolve()
    build = build.resolve()
    _require(_under(record, build), "private original record is outside the explicit build directory")
    # The shared native runner retains both suites' CTest originals under build/gate,
    # so the ASan originals are not under build/asan; they must still be in the repo build tree.
    _require(_under(last_test_path, ROOT / "build") and _under(junit_path, ROOT / "build"),
             "CTest originals are outside the repository build directory")
    last_test_bytes = _read(last_test_path, 32 * 1024 * 1024)
    last_test = last_test_bytes.decode("utf-8", errors="strict")
    _junit_passed(junit_path, "runner.raw-capture")
    section = _test_section(last_test, "runner.raw-capture")
    _require(str(record) in section, "selected original record is absent from its owning CTest output")
    _require("case reached: actual-av-bundle" in section,
             "the actual capture consumer case was not reached")
    command = _parse_command(section)
    expected_command = [
        str((build / "runner/test_audio_capture").resolve()),
        "actual-av-bundle",
        str((build / "runner/t64-supervisor").resolve()),
        str((build / "runner/t64-supervisor-test").resolve()),
        str((build / "runner/t64-result-check").resolve()),
        str((ROOT / "build/fixtures/audio-pcm.z64").resolve()),
        str((ROOT / "build/fixtures/hello.input.json").resolve()),
        str((ROOT / "build/fixtures/rdp-fill.z64").resolve()),
    ]
    _require(command == expected_command, "actual capture CTest argv differs from its authored command")

    argv = [line for line in _read(record / "command.args", 16 * 1024).decode().splitlines()]
    _require(len(argv) >= 2 and argv[0] == expected_command[2] and argv[1] == "--real",
             "retained producer command is not the actual real supervisor")
    _require(argv.count("--raw-capture-dir") == 1,
             "retained producer command does not have one explicit raw target")
    raw_index = argv.index("--raw-capture-dir")
    _require(raw_index + 1 < len(argv) and
             Path(argv[raw_index + 1]).resolve() == (record / "raw-bundle").resolve(),
             "retained producer command raw target differs from the captured bundle")
    _require("--test-fault" not in argv and "--raw-capture-max-bytes" not in argv,
             "accepted capture command unexpectedly used a fault or alternate cap")
    for name, expected in (("--fixture", ROOT / "build/fixtures/audio-pcm.z64"),
                           ("--input", ROOT / "build/fixtures/hello.input.json")):
        _require(argv.count(name) == 1 and argv.index(name) + 1 < len(argv) and
                 Path(argv[argv.index(name) + 1]).resolve() == expected.resolve(),
                 f"retained producer command {name} differs from the admitted input")

    result = _producer_result(record)
    producer = _json(record / "producer-report.json", 16 * 1024)
    _require(result.get("outcome") == "pass" and result.get("detail") == "positive-match",
             "captured consumer result is not the authored positive match")
    _require(isinstance(result.get("state_digest"), str) and
             SHA256_RE.fullmatch(result["state_digest"]) is not None,
             "captured canonical state digest is malformed")
    _require(result.get("guest_tick") == "551001", "accepted consumer tick differs")
    _require((record / "raw-bundle").is_dir() and not (record / "raw-bundle").is_symlink(),
             "original A/V bundle is missing or unsafe")
    bundle = record / "raw-bundle"
    manifest_bytes = _read(bundle / "manifest.json", 1024 * 1024)
    manifest = json.loads(manifest_bytes)
    _require(manifest.get("schema") == "t64.raw-av-capture" and
             manifest.get("schema_version") == 1,
             "raw bundle schema differs from the authored capture")
    _verify_capture_identities(result, manifest)
    _require(manifest.get("descriptor_version") == "11" and
             manifest.get("machine_model_version") == "19" and
             manifest.get("state_serialization_version") == "24",
             "profile version tuple differs from the authored tuple")
    _require(manifest.get("terminal_guest_tick") == "551001" and
             manifest.get("terminal_outcome") == "pass" and
             manifest.get("terminal_stop_kind") == "authored-completion",
             "bundle terminal state differs from the accepted producer result")
    video = manifest.get("video")
    audio = manifest.get("audio")
    _require(isinstance(video, list) and len(video) == 29 and
             isinstance(audio, list) and len(audio) == 2,
             "bundle packet denominator differs from the authored capture")
    _require(manifest.get("video_count") == "29" and manifest.get("audio_count") == "2" and
             manifest.get("total_bytes") == "16581" and
             manifest.get("manifest_bytes") == str(len(manifest_bytes)),
             "bundle byte accounting differs from the authored capture")

    expected_files = {"manifest.json"}
    video_bytes = bytearray()
    for index, entry in enumerate(video):
        name = f"frame-{index:06d}.rgba8"
        _require(isinstance(entry, dict) and entry.get("file") == name and
                 entry.get("sequence") == str(index) and
                 entry.get("width") == "12" and entry.get("height") == "7" and
                 entry.get("stride_bytes") == "48" and entry.get("byte_length") == "336",
                 "video packet metadata differs from the authored 12x7 stream")
        data = _packet_file(bundle / "video" / name, 336)
        video_bytes.extend(data)
        expected_files.add(f"video/{name}")
    audio_bytes = bytearray()
    audio_packet_data: list[bytes] = []
    for index, entry in enumerate(audio):
        name = f"packet-{index:06d}.s16be"
        _require(isinstance(entry, dict) and entry.get("file") == name and
                 entry.get("sequence") == str(index + 1) and
                 entry.get("frame_count") == "4" and
                 entry.get("byte_length") == "16" and
                 entry.get("rate_numerator") == "48681812" and
                 entry.get("rate_denominator") == "1104" and
                 entry.get("channels") == "2" and
                 entry.get("bits_per_sample") == "16",
                 "audio packet metadata differs from the authored PCM stream")
        data = _packet_file(bundle / "audio" / name, 16)
        audio_bytes.extend(data)
        audio_packet_data.append(data)
        expected_files.add(f"audio/{name}")
    _verify_pcm_literal(bytes(audio_bytes))
    actual_files = {
        path.relative_to(bundle).as_posix()
        for path in bundle.rglob("*")
        if path.is_file() and not path.is_symlink()
    }
    _require(actual_files == expected_files, "raw bundle has missing or unexpected members")
    observer = _read(record / "video-observation.bin", 4096)
    packet_offset = struct.calcsize("@27I7Q4I64s96s64s168s")
    observer_profile = observer[184:248].split(b"\0", 1)[0].decode("ascii", errors="strict")
    observer_build = observer[248:344].split(b"\0", 1)[0].decode("ascii", errors="strict")
    _require(len(observer) == struct.calcsize("@27I7Q4I64s96s64s168s336s") and
             observer_profile == PROFILE_ID and
             observer_build == manifest.get("core_build_id") and
             bytes(video_bytes[:336]) == observer[packet_offset:packet_offset + 336],
             "video packet or observer profile/build identity differs from its original record")
    audio_observer = _read(record / "audio-observation.bin", 16 * 1024)
    _verify_audio_observation(audio_observer, manifest, audio,
                              audio_packet_data, result)

    fixture = _read(ROOT / "build/fixtures/audio-pcm.z64", 1024 * 1024)
    input_data = _read(ROOT / "build/fixtures/hello.input.json", 64 * 1024)
    _require(len(fixture) == 4728 and _sha(fixture) == FIXTURE_SHA256,
             "actual fixture bytes differ from the authored fixture identity")
    _require(len(input_data) == 54 and _sha(input_data) == INPUT_SHA256,
             "actual input bytes differ from the authored input identity")
    binary_names = (
        "runner/test_audio_capture", "runner/t64-runner-real",
        "runner/t64-runner-test", "runner/t64-supervisor",
        "runner/t64-supervisor-test", "runner/t64-result-check", "core/libt64_core.a",
    )
    binaries = {name: _file_sha(build / name) for name in binary_names}
    race = _verify_race(build, last_test, junit_path, result["state_digest"])
    return {
        "ctest_command": command,
        "ctest_command_sha256": _sha(json.dumps(command, separators=(",", ":")).encode()),
        "ctest_last_test_sha256": _sha(last_test_bytes),
        "ctest_junit_sha256": _file_sha(junit_path),
        "result": result,
        "producer": producer,
        "capture_manifest_sha256": _sha(manifest_bytes),
        "video_packets": len(video),
        "audio_packets": len(audio),
        "capture_total_bytes": int(manifest["total_bytes"]),
        "video_observation_sha256": _sha(observer),
        "audio_observation_sha256": _sha(audio_observer),
        "audio_literal_sha256": _sha(PCM),
        "binaries": binaries,
        "race": race,
        "private_record": str(record),
    }


def _artifact(build: Path, prefix: str, suite: str, suffix: str) -> Path:
    matches = list(build.glob(f"{prefix}-{suite}-*-{suffix}"))
    _require(len(matches) == 1, f"explicit {suite} artifact prefix does not select exactly one {suffix}")
    return matches[0]


def _suite_records(build: Path, suite: str, prefix: str) -> tuple[Path, Path, dict[str, Any]]:
    inventory_data = _read(INVENTORY, 128 * 1024)
    inventory = _validate_inventory(inventory_data, EXPECTED_INVENTORY_SHA256)
    authored = json.loads(inventory_data)
    selected = authored["suites"].get(suite)
    _require(isinstance(selected, dict), f"unknown authored qualification suite: {suite}")
    expected_ctests = selected["ctest"]
    last_path = _artifact(build, prefix, suite, "after-suite-LastTest.log")
    token = last_path.name.removesuffix("-after-suite-LastTest.log")
    junit_path = build / f"{token}-{suite}-junit.xml"
    command_path = build / f"{token}-suite-command.json"
    exit_path = build / f"{token}-suite-exit-status.txt"
    discovery_path = build / f"{token}-discovery-stdout.log"
    source_command_path = build / f"{token}-source-body-check-command.json"
    source_stdout_path = build / f"{token}-source-body-check-stdout.log"
    build_command_path = build / f"{token}-build-command.json"
    _require(all(path.is_file() and not path.is_symlink() for path in (
        junit_path, command_path, exit_path, discovery_path, source_command_path,
        source_stdout_path, build_command_path,
    )), "exact checker command/result originals are incomplete")
    _require(_read(exit_path, 64).strip() == b"0", "original authored CTest suite did not exit zero")
    suite_command = _json(command_path, 16 * 1024)
    regex = "^(" + "|".join(re.escape(name) for name in expected_ctests) + ")$"
    expected_command = [
        "ctest", "--test-dir", str(build), "--no-tests=error", "--parallel", "4",
        "--output-on-failure", "-R", regex, "--output-junit", str(junit_path.resolve()),
    ]
    _require(suite_command == expected_command,
             "original CTest command or selected case denominator is stale")
    build_command = _json(build_command_path, 16 * 1024)
    _require(build_command == ["cmake", "--build", str(build), "--parallel", "1"],
             "owning checker build command differs from the explicit build tree")
    discovery = _json(build / f"{token}-discovery-stdout.log", 8 * 1024 * 1024)
    _require(isinstance(discovery, dict) and isinstance(discovery.get("tests"), list),
             "original CTest discovery record is malformed")
    discovered = [entry.get("name") for entry in discovery["tests"] if isinstance(entry, dict)]
    _require(len(discovered) == len(set(discovered)) and
             set(expected_ctests).issubset(set(discovered)),
             "registered authored CTest denominator was absent from original discovery")
    source_command = _json(source_command_path, 16 * 1024)
    expected_source_command = [
        sys.executable, "-B", str((ROOT / "scripts/check-rsp-no-hle.py").resolve()),
        "--build", str(build),
    ]
    _require(source_command == expected_source_command and
             _read(source_stdout_path, 4 * 1024 * 1024).startswith(b"PASS: rsp-no-hle"),
             "original source/body/object gate is absent or failed")
    last_text = _read(last_path, 32 * 1024 * 1024).decode("utf-8", errors="strict")
    _require(all(re.search(rf"^\d+/\d+ Testing: {re.escape(name)}$", last_text,
                           flags=re.MULTILINE) for name in expected_ctests),
             "original LastTest does not contain every authored CTest case")
    try:
        junit_root = ET.fromstring(_read(junit_path, 8 * 1024 * 1024))
    except ET.ParseError as error:
        raise Refusal("original CTest JUnit is malformed") from error
    junit_names = [entry.attrib.get("name") for entry in junit_root.iter("testcase")]
    _require(len(junit_names) == len(set(junit_names)) and set(junit_names) == set(expected_ctests),
             "original JUnit denominator differs from the exact selected CTest suite")
    for name in expected_ctests:
        _junit_passed(junit_path, name)
    capture_section = _test_section(last_text, "runner.raw-capture")
    retained = [Path(value).resolve() for value in re.findall(
        r"^private audio runner evidence retained at (.+)$", capture_section,
        flags=re.MULTILINE,
    )]
    candidates = []
    for path in retained:
        if not _under(path, build) or not (path / "raw-bundle/manifest.json").is_file():
            continue
        try:
            result = _json(path / "result.json", 16 * 1024)
            argv = _read(path / "command.args", 16 * 1024).decode("utf-8").splitlines()
            if result.get("outcome") == "pass" and argv and argv[0] == str((build / "runner/t64-supervisor").resolve()):
                candidates.append(path)
        except (OSError, UnicodeDecodeError, json.JSONDecodeError, Refusal):
            continue
    _require(bool(candidates), "original capture CTest has no accepted production-runner bundle")
    _require("case reached: actual-av-bundle" in capture_section and
             "case reached: literal-pcm-files" in capture_section and
             "case reached: capture-drain-resume" in capture_section,
             "accepted consumer, independent sample, or drain assertions were not reached")
    summaries = [
        _capture_record(path, build, last_path, junit_path)
        for path in candidates
    ]
    _require(all(item["capture_manifest_sha256"] == summaries[0]["capture_manifest_sha256"] and
                 item["result"].get("state_digest") == summaries[0]["result"].get("state_digest")
                 for item in summaries),
             "accepted production capture records disagree on packets or canonical state")
    summary = summaries[0]
    summary.update({
        "inventory": inventory,
        "suite": suite,
        "artifact_token": token,
        "last_test_path": str(last_path),
        "junit_path": str(junit_path),
        "checker_artifact_paths": [
            str(path) for path in (
                command_path, exit_path, discovery_path, source_command_path,
                source_stdout_path, build_command_path,
                build / f"{token}-build-stdout.log",
                build / f"{token}-build-stderr.log",
                build / f"{token}-suite-stdout.log",
                build / f"{token}-suite-stderr.log",
                build / f"{token}-before-suite-LastTest.log",
            )
        ],
        "capture_records": [item["private_record"] for item in summaries],
    })
    return last_path, junit_path, summary


def _safe_copy(source: Path, destination: Path) -> None:
    _require(not source.is_symlink(), "private original contains a symlink")
    if source.is_dir():
        destination.mkdir(mode=0o700)
        for child in source.iterdir():
            _safe_copy(child, destination / child.name)
        return
    data = _read(source, 64 * 1024 * 1024)
    destination.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    with destination.open("xb") as stream:
        stream.write(data)
    destination.chmod(source.stat().st_mode & 0o777)


def _retain_originals(private: Path, build: Path, last_path: Path,
                      junit_path: Path, summary: dict[str, Any],
                      source_snapshot: dict[str, Any] | None) -> None:
    originals = private / "originals"
    originals.mkdir(mode=0o700)
    _safe_copy(last_path, originals / "LastTest.log")
    _safe_copy(junit_path, originals / "suite-junit.xml")
    for index, record_text in enumerate(summary["capture_records"]):
        _safe_copy(Path(record_text), originals / f"capture-{index:02d}")
    race = summary["race"]
    for index, record_text in enumerate(race["private_records"]):
        _safe_copy(Path(record_text), originals / f"race-record-{index:02d}")
    _safe_copy(Path(race["private_target"]), originals / "race-winner")
    for index, record_text in enumerate(race["stage_replacement_records"]):
        _safe_copy(Path(record_text), originals / f"stage-replacement-record-{index:02d}")
    _safe_copy(Path(race["stage_replacement_baseline"]),
               originals / "stage-replacement-baseline")
    _safe_copy(Path(race["stage_replacement_target"]),
               originals / "stage-replacement-winner")
    _safe_copy(Path(race["stage_replacement_marker"]),
               originals / "stage-replacement-foreign-marker.bin")
    _safe_copy(Path(race["stage_replacement_moved_parent"]),
               originals / "stage-replacement-moved-parent")
    _safe_copy(build / "runner/test_audio_capture", originals / "bin/test_audio_capture")
    for name in (
        "t64-runner-real", "t64-runner-test", "t64-supervisor",
        "t64-supervisor-test", "t64-result-check",
    ):
        _safe_copy(build / "runner" / name, originals / f"bin/{name}")
    _safe_copy(build / "core/libt64_core.a", originals / "bin/libt64_core.a")
    _safe_copy(ROOT / "build/fixtures/audio-pcm.z64", originals / "inputs/audio-pcm.z64")
    _safe_copy(ROOT / "build/fixtures/hello.input.json", originals / "inputs/hello.input.json")
    for index, path_text in enumerate(summary["checker_artifact_paths"]):
        path = Path(path_text)
        if path.is_file() and not path.is_symlink():
            _safe_copy(path, originals / f"checker/{index:02d}-{path.name}")
    binding_name = "source-binding.json" if source_snapshot is not None else "entry-path-original.json"
    binding = source_snapshot if source_snapshot is not None else {
        "evidence_mode": "entry-path-validation",
        "source_revision": summary["result"]["source_revision"],
        "core_build_id": summary["result"]["build_id"],
        "runner_build_id": json.loads(_read(Path(summary["capture_records"][0]) /
                                              "raw-bundle/manifest.json"))["runner_build_id"],
        "runner_binary_sha256": summary["binaries"]["runner/t64-runner-real"],
    }
    (private / binding_name).write_text(
        json.dumps(binding, sort_keys=True, separators=(",", ":")) + "\n",
        encoding="utf-8",
    )
    (private / binding_name).chmod(0o600)


def _projection(summary: dict[str, Any], *, job: str, workflow_sha: str | None,
                run_id: str | None, attempt: str | None) -> dict[str, Any]:
    manifest = json.loads(_read(Path(summary["capture_records"][0]) / "raw-bundle/manifest.json"))
    binaries = summary["binaries"]
    race = summary["race"]
    return {
        "schema": PUBLIC_SCHEMA,
        "schema_version": 1,
        "evidence_mode": "entry-path-validation" if job == "local" else "hosted-gate",
        "workflow_sha": workflow_sha,
        "source_revision": manifest["source_revision"],
        "job_name": job,
        "run_id": run_id,
        "run_attempt": attempt,
        "platform": race["platform"],
        "profile": {
            "profile_id": PROFILE_ID, "profile_sha256": PROFILE_SHA256,
            "descriptor_version": 11, "machine_model_version": 19,
            "state_serialization_version": 24,
        },
        "fixture": {
            "fixture_id": FIXTURE_ID, "fixture_sha256": FIXTURE_SHA256,
            "fixture_size": 4728, "input_sha256": INPUT_SHA256, "input_size": 54,
        },
        "producer": {
            "source_revision": manifest["source_revision"],
            "core_build_id": manifest["core_build_id"],
            "runner_build_id": manifest["runner_build_id"],
            "real_runner_sha256": binaries["runner/t64-runner-real"],
            "supervisor_sha256": binaries["runner/t64-supervisor"],
            "checker_sha256": binaries["runner/t64-result-check"],
            "test_runner_sha256": binaries["runner/t64-runner-test"],
            "test_supervisor_sha256": binaries["runner/t64-supervisor-test"],
            "test_binary_sha256": binaries["runner/test_audio_capture"],
            "core_library_sha256": binaries["core/libt64_core.a"],
        },
        "capture": {
            "manifest_sha256": summary["capture_manifest_sha256"],
            "video_packets": summary["video_packets"],
            "audio_packets": summary["audio_packets"],
            "total_bytes": summary["capture_total_bytes"],
            "pcm_literal_sha256": summary["audio_literal_sha256"],
            "video_observer_sha256": summary["video_observation_sha256"],
            "audio_observer_sha256": summary["audio_observation_sha256"],
        },
        "execution": {
            "capture_ctest": "runner.raw-capture",
            "race_ctest": "controls.real.raw-capture-race",
            "command_sha256": summary["ctest_command_sha256"],
            "race_command_sha256": race["ctest_command_sha256"],
            "last_test_sha256": summary["ctest_last_test_sha256"],
            "junit_sha256": summary["ctest_junit_sha256"],
            "return_code": 0, "status": "pass", "timed_out": False,
        },
        "inventory": {
            "snapshot_sha256": summary["inventory"]["snapshot_sha256"],
            "case_count": summary["inventory"]["case_count"],
            "ctest_count": summary["inventory"]["ctest_count"],
        },
        "platform_witness": {
            "platform": race["platform"],
            "test_binary_sha256": race["test_binary_sha256"],
            "runner_binary_sha256": race["runner_binary_sha256"],
            "supervisor_binary_sha256": race["supervisor_binary_sha256"],
            "checker_binary_sha256": race["checker_binary_sha256"],
            "ctest_command_sha256": race["ctest_command_sha256"],
            "contender_command_sha256": race["contender_command_sha256"],
            "outcomes": race["outcomes"],
        },
    }


def _emit_hosted(*, job: str, commit_sha: str | None, run_id: str | None,
                 attempt: str | None, build_dir: Path, suite: str,
                 artifact_prefix: str, output_dir: Path) -> dict[str, Any]:
    build = build_dir.resolve()
    output = output_dir.resolve()
    _require(build.is_dir() and _under(build, ROOT / "build"),
             "explicit build directory must be inside the repository build tree")
    _require(re.fullmatch(r"[A-Za-z0-9._-]{1,120}", artifact_prefix) is not None,
             "artifact prefix is unsafe")
    if job == "local":
        _require(run_id is None and attempt is None,
                 "local smoke cannot claim hosted run identifiers")
        require_clean = False
        workflow_sha = None
        source_before = None
    else:
        _require(isinstance(commit_sha, str) and SHA1_RE.fullmatch(commit_sha) is not None and
                 _git(["rev-parse", "HEAD"]).decode().strip() == commit_sha and
                 job in {"gate-linux", "gate-macos"} and run_id is not None and
                 attempt is not None and DECIMAL_RE.fullmatch(run_id) is not None and
                 DECIMAL_RE.fullmatch(attempt) is not None,
                 "hosted emitter requires explicit job, run, attempt, and source SHA")
        _require(os.environ.get("GITHUB_JOB") == job and
                 os.environ.get("GITHUB_SHA") == commit_sha and
                 os.environ.get("GITHUB_RUN_ID") == run_id and
                 os.environ.get("GITHUB_RUN_ATTEMPT") == attempt,
                 "emitter arguments differ from the exact workflow environment context")
        require_clean = True
        _require(suite == "qualification",
                 "hosted emission requires the exact qualification suite")
        workflow_sha = commit_sha
        source_before = _source_snapshot(commit_sha, require_clean=require_clean)
    last_path, junit_path, summary = _suite_records(build, suite, artifact_prefix)
    source_revision = summary["result"].get("source_revision")
    _require(isinstance(source_revision, str) and SHA1_RE.fullmatch(source_revision) is not None,
             "actual producer source revision is malformed")
    if job == "local":
        _require(commit_sha is None or commit_sha == source_revision,
                 "local smoke SHA conflicts with the retained producer source revision")
    else:
        _require(summary["inventory"].get("case_count", 0) > 0,
                 "hosted qualification denominator is empty")
    private_parent = build / "08-05-audio-private"
    if private_parent.exists() or private_parent.is_symlink():
        _require(private_parent.is_dir() and not private_parent.is_symlink(),
                 "private original store is unsafe")
    else:
        private_parent.mkdir(mode=0o700)
    output_binding = _sha(str(output).encode("utf-8"))[:12]
    record_id = (f"{job}-{source_revision[:12]}-{run_id or 'local'}-"
                 f"{attempt or '0'}-{artifact_prefix[:32]}-{output_binding}")
    private = private_parent / record_id
    _require(not private.exists() and not private.is_symlink(),
             "private original record destination already exists")
    private.mkdir(mode=0o700)
    _retain_originals(private, build, last_path, junit_path, summary, source_before)
    if source_before is not None:
        source_after = _source_snapshot(commit_sha or "", require_clean=require_clean)
        _require(source_before == source_after,
                 "source inputs changed during original-record verification")
    inventory_data = _read(INVENTORY, 128 * 1024)
    _validate_inventory(inventory_data, EXPECTED_INVENTORY_SHA256)
    projection = _projection(summary, job=job, workflow_sha=workflow_sha,
                              run_id=run_id, attempt=attempt)
    _public_projection(projection, {
        "job": job, "sha": workflow_sha, "run": run_id, "attempt": attempt,
    })
    _require(not output.exists() and not output.is_symlink(),
             "public output directory must be fresh")
    output.mkdir(parents=True, mode=0o755)
    payload = json.dumps(projection, sort_keys=True, separators=(",", ":")).encode() + b"\n"
    _require(len(payload) <= 16 * 1024, "public projection exceeds the bounded size")
    manifest_path = output / "manifest.json"
    with manifest_path.open("xb") as stream:
        stream.write(payload)
    manifest_path.chmod(0o644)
    _scan_public(output, {"job": job, "sha": commit_sha, "run": run_id, "attempt": attempt})
    private_record = {
        "schema": "t64.audio-private-originals",
        "schema_version": 1,
        "evidence_mode": "entry-path-validation" if job == "local" else "hosted-gate",
        "source_revision": source_revision,
        "source_binding_sha256": None if source_before is None else _sha(
            json.dumps(source_before, sort_keys=True, separators=(",", ":")).encode()
        ),
        "public_manifest_sha256": _sha(payload),
        "last_test_path": str(last_path),
        "junit_path": str(junit_path),
        "capture_record": summary["private_record"],
    }
    (private / "original-record.json").write_text(
        json.dumps(private_record, sort_keys=True, separators=(",", ":")) + "\n",
        encoding="utf-8",
    )
    (private / "original-record.json").chmod(0o600)
    return {"projection": projection, "private": private}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--selftest", action="store_true")
    mode.add_argument("--emit-hosted", action="store_true")
    mode.add_argument("--native", action="store_true")
    mode.add_argument("--collect-hosted", action="store_true")
    mode.add_argument("--verify-evidence", action="store_true")
    mode.add_argument("--verify-record", type=Path, metavar="PRIVATE_RUN")
    mode.add_argument("--scan-pii", action="store_true")
    parser.add_argument("--build", type=Path, help="explicit normal or ASAN build directory")
    parser.add_argument("--last-test", type=Path, help="exact owning CTest LastTest.log")
    parser.add_argument("--junit", type=Path, help="exact owning CTest JUnit XML")
    parser.add_argument("--job", choices=("local", "gate-linux", "gate-macos"))
    parser.add_argument("--repo", help="exact hosted repository full name")
    parser.add_argument("--sha", help="exact workflow source SHA; local smoke may omit")
    parser.add_argument("--run", help="exact workflow run ID")
    parser.add_argument("--attempt", help="exact workflow attempt")
    parser.add_argument("--native-record", type=Path, help="exact private native record")
    parser.add_argument("--hosted-record", type=Path, help="exact private hosted collection record")
    parser.add_argument("--suite", choices=("capture-errors", "qualification"))
    parser.add_argument("--artifact-prefix", help="explicit run-bound CTest artifact prefix")
    parser.add_argument("--out", type=Path, help="fresh public projection directory")
    args = parser.parse_args()
    try:
        if args.selftest:
            test_script = ROOT / "scripts/tests/test_audio_qualification.py"
            result = subprocess.run(
                [sys.executable, "-B", str(test_script), "--selftest"],
                cwd=ROOT, timeout=45, check=False,
            )
            _require(result.returncode == 0, "audio qualification controls did not pass")
            return 0
        if args.native:
            if args.sha is None or args.out is None:
                parser.error("--native requires exact --sha and fresh private --out")
            native_record = _native_qualify(args.sha, args.out)
            print(f"qualify-audio: native qualification passed; private originals retained at {native_record}")
            return 0
        if args.collect_hosted:
            if not all((args.repo, args.sha, args.run, args.attempt)) or \
                    args.native_record is None or args.out is None:
                parser.error("--collect-hosted requires --repo, --sha, --run, --attempt, --native-record, and --out")
            hosted_record = _collect_hosted(
                args.repo, args.sha, args.run, args.attempt, args.native_record, args.out,
            )
            print(f"qualify-audio: exact hosted artifacts collected; private originals retained at {hosted_record}")
            return 0
        if args.verify_evidence:
            if not all((args.repo, args.sha, args.run, args.attempt)) or \
                    args.native_record is None or args.hosted_record is None:
                parser.error("--verify-evidence requires exact --repo, --sha, --run, --attempt, --native-record, and --hosted-record")
            _verify_hosted_record(
                args.hosted_record, repo=args.repo, sha=args.sha, run=args.run,
                attempt=args.attempt, native_path=args.native_record,
            )
            print("qualify-audio: complete native, ASan, GCC13, and both hosted platform records verified")
            return 0
        if args.verify_record is not None:
            if args.build is None or args.last_test is None or args.junit is None:
                parser.error("--verify-record requires --build, --last-test, and --junit")
            summary = _capture_record(args.verify_record, args.build,
                                      args.last_test, args.junit)
            print("qualify-audio: accepted retained consumer and race records "
                  f"({summary['video_packets']} video, {summary['audio_packets']} audio, "
                  f"{summary['capture_total_bytes']} bundle bytes); "
                  "this record check makes no hosted or fresh-source qualification claim")
            return 0
        if args.emit_hosted:
            if args.job is None or args.build is None or args.suite is None or \
                    not args.artifact_prefix or args.out is None:
                parser.error("--emit-hosted requires --job, --build, --suite, --artifact-prefix, and --out")
            if args.job == "local":
                if args.run is not None or args.attempt is not None:
                    parser.error("local entry-path validation cannot claim run or attempt IDs")
            elif args.sha is None or args.run is None or args.attempt is None:
                parser.error("hosted emission requires explicit --sha, --run, and --attempt")
            result = _emit_hosted(
                job=args.job, commit_sha=args.sha, run_id=args.run,
                attempt=args.attempt, build_dir=args.build, suite=args.suite,
                artifact_prefix=args.artifact_prefix, output_dir=args.out,
            )
            mode_name = "entry-path validation" if args.job == "local" else "hosted evidence"
            print(f"qualify-audio: emitted {mode_name}; private originals retained at {result['private']}")
            return 0
        if args.scan_pii:
            if args.out is None or args.job is None:
                parser.error("--scan-pii requires --job and --out")
            expected = {
                "job": args.job,
                "sha": args.sha,
                "run": args.run,
                "attempt": args.attempt,
            }
            _scan_public(args.out, expected)
            print("qualify-audio: strict public allowlist and PII scan passed")
            return 0
        raise Refusal("no supported mode selected")
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, Refusal,
            AttributeError, KeyError, TypeError, ValueError, AssertionError,
            subprocess.TimeoutExpired) as error:
        print(f"qualify-audio: refused original evidence: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
