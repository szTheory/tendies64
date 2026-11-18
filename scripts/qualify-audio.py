#!/usr/bin/env python3
"""Verify private, original records from the authored audio capture tests."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shlex
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "docs/spec/audio-cases.json"
EXPECTED_INVENTORY_SHA256 = "42dfe5f51b8129479bf809d1a5dc53a5e7947cd8c9aecdf034b2fe1d14cadd1b"
PROFILE_ID = "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"
PROFILE_SHA256 = "eaa400aa23bfa3d0c9ba6d475d8eba7a60302151e22b6278ff6a4991d08bb5bf"
FIXTURE_ID = "audio-pcm-literal"
FIXTURE_SHA256 = "cd9556444480b8b02b043207013e7ce5bf5697a1c040058bcde1348e55e80b99"
INPUT_SHA256 = "6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d"
PCM = bytes.fromhex(
    "00007fff8000ffff1234edcc0001fffe"
    "80007fffffff000055aaaa5500008000"
)
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
    "scripts/qualify-rdp-vi.py", "scripts/collect-boot-evidence.sh",
    "fixtures/manifest.json", "fixtures/manifest.schema.json",
    "tools/manifest", "tools/rights",
    "scripts/tests/test_rsp_no_hle.py",
    "docs/spec/audio-cases.json", ".github/workflows/ci.yml",
)


class Refusal(ValueError):
    """Raised when an original record is missing, changed, or unbound."""


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
    value = _public_projection(json.loads(data), expected)
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
    paths = re.findall(r"^private audio runner evidence retained at (.+)$", section,
                       flags=re.MULTILINE)
    _require(len(paths) == 3, "race original does not retain its baseline and two contender records")
    records = [Path(value).resolve() for value in paths]
    _require(all(_under(path, build) for path in records),
             "race record escapes the explicit build directory")
    contenders: list[tuple[Path, list[str], dict[str, Any]]] = []
    baseline: tuple[Path, list[str], dict[str, Any]] | None = None
    for record in records:
        argv = _read(record / "command.args", 16 * 1024).decode("utf-8").splitlines()
        result = _producer_result(record)
        _require(argv and argv[0] == expected[2], "race record is not owned by the test supervisor")
        _require(argv.count("--raw-capture-dir") == 1,
                 "race record does not identify one raw target")
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
        "private_records": [str(path) for path in records],
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


def _capture_record(record: Path, build: Path, last_test_path: Path,
                    junit_path: Path) -> dict[str, Any]:
    record = record.resolve()
    build = build.resolve()
    _require(_under(record, build), "private original record is outside the explicit build directory")
    _require(_under(last_test_path, build) and _under(junit_path, build),
             "CTest originals are outside the explicit build directory")
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
    _require(len(audio_observer) > 0, "original PCM observation record is empty")

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
    mode.add_argument("--verify-evidence", type=Path, metavar="PRIVATE_RUN")
    mode.add_argument("--scan-pii", action="store_true")
    parser.add_argument("--build", type=Path, help="explicit normal or ASAN build directory")
    parser.add_argument("--last-test", type=Path, help="exact owning CTest LastTest.log")
    parser.add_argument("--junit", type=Path, help="exact owning CTest JUnit XML")
    parser.add_argument("--job", choices=("local", "gate-linux", "gate-macos"))
    parser.add_argument("--sha", help="exact workflow source SHA; local smoke may omit")
    parser.add_argument("--run", help="exact workflow run ID")
    parser.add_argument("--attempt", help="exact workflow attempt")
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
        if args.verify_evidence is not None:
            if args.build is None or args.last_test is None or args.junit is None:
                parser.error("--verify-evidence requires --build, --last-test, and --junit")
            summary = _capture_record(args.verify_evidence, args.build,
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
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, Refusal) as error:
        print(f"qualify-audio: refused original evidence: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
