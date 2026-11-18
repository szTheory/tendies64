#!/usr/bin/env python3
"""Qualify authored RDP/VI output and bind records to their original run."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time
import warnings
import zipfile
from io import BytesIO
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PRIVATE_ROOT = ROOT / "build/gate/07-qualification"
PUBLIC_ROOT = ROOT / "build/evidence/combined"
CASE_INVENTORY = ROOT / "docs/spec/rdp-vi-cases.json"
CASE_INVENTORY_SHA256 = "c915cc38ce78aeccc0d6718351f4968368f35126366e4b353a90037abc98385b"
RUNNER = ROOT / "build/gate/runner/test_rdp_video"
SUPERVISOR = ROOT / "build/gate/runner/t64-supervisor"
CHECKER = ROOT / "build/gate/runner/t64-result-check"
GUEST_RUNNER = ROOT / "build/gate/runner/t64-runner-real"
FIXTURE = ROOT / "build/fixtures/rdp-fill.z64"
INPUT = ROOT / "build/fixtures/hello.input.json"
SCRIPT = ROOT / "scripts/qualify-rdp-vi.py"

SCHEMA = "t64.rdp-vi-evidence"
SCHEMA_VERSION = 1
PUBLIC_RAW_SCHEMA = "t64.rdp-vi-public-raw"
FIXTURE_ID = "rdp-vi-fill-diagnostic"
FIXTURE_SHA256 = "b469340e7caa0ac341a2a5b525aa7e3b39c8a47f295ecd10710ae695e46dfc3d"
INPUT_SHA256 = "6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d"
PROFILE_ID = "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2"
PROFILE_SHA256 = "5612696247abb5dae1c2abb6c15d7fd6c5632136210be7b21a1085590af1c94e"
VI_CLOCK_POLICY = "rsp62500000-ntsc48681812-rational-v1"
COMPARATOR_MARKER = (
    "accepted raw pixel comparator reached: authored bytes match t64_video_copy output"
)
WRONG_PIXEL_MARKER = "wrong-pixel comparator reached at (x=0,y=0,channel=0)"
EXPECTED_SOURCE = bytes.fromhex(
    "f80107c1f80107c1f80107c1f80107c1f80107c1f80107c1"
)
EXPECTED_PACKET = bytes.fromhex(
    "ff0000ff00ff00ffff0000ff00ff00ff"
    "ff0000ff00ff00ffff0000ff00ff00ff"
    "ff0000ff00ff00ffff0000ff00ff00ff"
)
BUILD_ID_RE = re.compile(r"t64-build-v1-[0-9a-f]{64}\Z")
SHA256_RE = re.compile(r"[0-9a-f]{64}\Z")
SHA1_RE = re.compile(r"[0-9a-f]{40}\Z")
DECIMAL_RE = re.compile(r"[1-9][0-9]{0,18}\Z")
PHASE07_SOURCE_BASE = "18dcaa0c4849f4c2841d1fb4c14959600e42b257"
NATIVE_INPUT_PATHS = (
    ".gitattributes", ".gitignore", "CMakeLists.txt", "CMakePresets.json",
    "CMakeUserPresets.json",
    "core", "runner", "scripts", "tools", "fixtures", "tests", "docs/spec",
)
MAX_JSON_BYTES = 4 * 1024 * 1024
MAX_RAW_BYTES = 64 * 1024 * 1024
MAX_UPLOAD_BYTES = 1024 * 1024


class Refusal(ValueError):
    """A qualification record is missing, altered, or outside its contract."""


def _sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _canonical(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=True, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def _pairs_no_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise Refusal("duplicate JSON key")
        result[key] = value
    return result


def _read_file(path: Path, maximum: int = MAX_RAW_BYTES) -> bytes:
    if path.is_symlink():
        raise Refusal("symlink input")
    descriptor = -1
    try:
        descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) |
                             getattr(os, "O_NONBLOCK", 0))
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode) or before.st_size < 0 or before.st_size > maximum:
            raise Refusal("input is not a bounded regular file")
        chunks: list[bytes] = []
        total = 0
        while True:
            part = os.read(descriptor, min(1024 * 1024, maximum + 1 - total))
            if not part:
                break
            total += len(part)
            if total > maximum:
                raise Refusal("input exceeds byte bound")
            chunks.append(part)
        after = os.fstat(descriptor)
        if ((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) !=
                (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns) or
                total != before.st_size):
            raise Refusal("input changed while being read")
        return b"".join(chunks)
    except OSError as error:
        raise Refusal("input could not be read safely") from error
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def _read_json(path: Path) -> object:
    try:
        data = _read_file(path, MAX_JSON_BYTES)
        return json.loads(data, object_pairs_hook=_pairs_no_duplicates)
    except (UnicodeError, json.JSONDecodeError) as error:
        raise Refusal("JSON input is malformed") from error


def _write_json(path: Path, value: object) -> None:
    path.write_bytes(_canonical(value))


def _file_sha(path: Path) -> str:
    return _sha(_read_file(path))


def _private_dir(prefix: str) -> Path:
    PRIVATE_ROOT.mkdir(parents=True, exist_ok=True, mode=0o700)
    result = Path(tempfile.mkdtemp(prefix=prefix, dir=PRIVATE_ROOT))
    result.chmod(0o700)
    return result


def _run_bounded(command: list[str], *, cwd: Path, environment: dict[str, str],
                 timeout_seconds: float) -> dict[str, object]:
    if os.name != "posix":
        raise Refusal("bounded process-group cleanup requires a POSIX host")
    started = time.monotonic()
    timed_out = False
    return_code: int | None = None
    stdout = b""
    stderr = b""
    process_group_empty = True
    process: subprocess.Popen[bytes] | None = None
    try:
        process = subprocess.Popen(
            command, cwd=cwd, env=environment, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, start_new_session=True,
        )
        try:
            stdout, stderr = process.communicate(timeout=timeout_seconds)
        except subprocess.TimeoutExpired:
            timed_out = True
            stdout, stderr, process_group_empty = _stop_process_group(process)
        else:
            return_code = process.returncode
            if _process_group_exists(process.pid):
                _send_group_signal(process.pid, signal.SIGTERM)
                process_group_empty = _wait_process_group_empty(process.pid, 0.2)
                if not process_group_empty:
                    _send_group_signal(process.pid, signal.SIGKILL)
                    process_group_empty = _wait_process_group_empty(process.pid, 1.0)
    except OSError as error:
        stderr = str(error).encode("utf-8", errors="replace")
        return_code = 127
    if process is not None:
        try:
            if process.poll() is None:
                process.wait(timeout=0.1)
        except subprocess.TimeoutExpired:
            pass
        if return_code is None:
            return_code = process.returncode
    if not process_group_empty:
        stderr += b"process group remained after bounded termination\n"
        if return_code in (None, 0):
            return_code = 125
    elapsed_ms = int((time.monotonic() - started) * 1000)
    return {
        "return_code": return_code,
        "timed_out": timed_out,
        "elapsed_ms": elapsed_ms,
        "stdout": stdout,
        "stderr": stderr,
        "process_group_empty": process_group_empty,
    }


def _process_group_exists(process_group: int) -> bool:
    try:
        os.killpg(process_group, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def _send_group_signal(process_group: int, selected_signal: int) -> None:
    try:
        os.killpg(process_group, selected_signal)
    except ProcessLookupError:
        pass


def _wait_process_group_empty(process_group: int, timeout_seconds: float) -> bool:
    deadline = time.monotonic() + timeout_seconds
    while _process_group_exists(process_group):
        if time.monotonic() >= deadline:
            return False
        time.sleep(0.01)
    return True


def _stop_process_group(process: subprocess.Popen[bytes]) -> tuple[bytes, bytes, bool]:
    _send_group_signal(process.pid, signal.SIGTERM)
    try:
        stdout, stderr = process.communicate(timeout=0.2)
    except subprocess.TimeoutExpired:
        _send_group_signal(process.pid, signal.SIGKILL)
        try:
            stdout, stderr = process.communicate(timeout=1.0)
        except subprocess.TimeoutExpired as error:
            stdout = error.stdout or b""
            stderr = error.stderr or b""
    if process.poll() is None:
        _send_group_signal(process.pid, signal.SIGKILL)
        try:
            process.wait(timeout=0.2)
        except subprocess.TimeoutExpired:
            pass
    empty = _wait_process_group_empty(process.pid, 0.5)
    if not empty:
        _send_group_signal(process.pid, signal.SIGKILL)
        empty = _wait_process_group_empty(process.pid, 0.5)
    return stdout, stderr, empty


def _save_process(directory: Path, command: list[str], cwd: Path,
                  environment: dict[str, object], outcome: dict[str, object],
                  timeout_seconds: float) -> None:
    if directory.is_symlink():
        raise Refusal("process evidence directory must not be a symlink")
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    stdout = outcome["stdout"]
    stderr = outcome["stderr"]
    assert isinstance(stdout, bytes) and isinstance(stderr, bytes)
    (directory / "stdout.log").write_bytes(stdout)
    (directory / "stderr.log").write_bytes(stderr)
    _write_json(directory / "command.json", {
        "argv": command,
        "cwd": str(cwd),
        "env": environment,
        "timeout_seconds": timeout_seconds,
    })
    _write_json(directory / "process.json", {
        "return_code": outcome["return_code"],
        "timed_out": outcome["timed_out"],
        "elapsed_ms": outcome["elapsed_ms"],
        "timeout_seconds": timeout_seconds,
        "stdout_bytes": len(stdout),
        "stderr_bytes": len(stderr),
        "stdout_sha256": _sha(stdout),
        "stderr_sha256": _sha(stderr),
    })


def _run_recorded(directory: Path, command: list[str], *, cwd: Path,
                  environment: dict[str, str], timeout_seconds: float) -> dict[str, object]:
    outcome = _run_bounded(
        command, cwd=cwd, environment={**os.environ, **environment},
        timeout_seconds=timeout_seconds,
    )
    _save_process(directory, command, cwd, environment, outcome, timeout_seconds)
    if outcome.get("process_group_empty") is not True:
        raise Refusal("bounded command descendants remained after process-group cleanup")
    return outcome


def _archive_last_test(build_dir: Path, destination: Path, label: str,
                       *, required: bool) -> bool:
    last_test = build_dir / "Testing/Temporary/LastTest.log"
    archived = destination / f"{label}-LastTest.log"
    destination.mkdir(parents=True, exist_ok=True, mode=0o700)
    if last_test.is_file() and not last_test.is_symlink():
        shutil.copy2(last_test, archived)
        return True
    archived.write_text("LastTest.log missing at the owning checkpoint\n", encoding="utf-8")
    if required:
        raise Refusal(f"owning CTest LastTest.log is missing for {label}")
    return False


def _run_native() -> tuple[Path, str]:
    source_sha = _git_head()
    source_binding = _native_source_snapshot(ROOT, source_sha)
    changed_c_sources = _phase07_changed_c_sources(source_sha)
    gcc_script = ROOT / "build/gate/07-10-gcc13-command.sh"
    if not gcc_script.is_file() or gcc_script.is_symlink():
        raise Refusal("previously qualified read-only GCC 13 command is unavailable")
    gcc_script_sha = _file_sha(gcc_script)
    gcc_image = _gcc13_image_identity()
    gcc_command = _gcc13_closure_command(changed_c_sources, gcc_image["image_id"])
    evidence = _private_dir(f"native-{source_sha[:12]}-")
    native_record: dict[str, object] = {
        "schema": "t64.phase07-native-source-binding",
        "schema_version": 1,
        "source_sha": source_sha,
        "source_input_pathspecs": list(NATIVE_INPUT_PATHS),
        "excluded_owner_paths": [
            "AGENTS.md", ".planning/config.json", ".planning/AUTONOMY.md",
        ],
        "source_binding": source_binding,
        "phase07_source_base": PHASE07_SOURCE_BASE,
        "phase07_changed_c_sources": changed_c_sources,
        "gcc13_reference_route": {
            "path": str(gcc_script.relative_to(ROOT)),
            "sha256": gcc_script_sha,
            "flags": "Phase07 gcc13-command core, core-test, and runner groups",
        },
        "gcc13_image": gcc_image,
        "gcc13_command": gcc_command,
        "native_commands": [
            ["sh", "scripts/gate.sh"],
            ["cmake", "--preset", "asan"],
            ["cmake", "--build", "--preset", "asan", "--parallel", "1"],
            ["ctest", "--preset", "asan", "--no-tests=error"],
        ],
    }
    _write_json(evidence / "source-binding.json", native_record)
    cheap_passed, cheap_message = _selftest()
    if not cheap_passed:
        raise Refusal("actual acceptance, comparator, deadline, and record controls failed before the full gate")
    _assert_native_source_snapshot(source_binding)
    normal_build = ROOT / "build/gate"
    asan_build = ROOT / "build/asan"
    normal_commands = [
        ("gate", ["sh", "scripts/gate.sh"], 600.0),
        ("asan-configure", ["cmake", "--preset", "asan"], 120.0),
        ("asan-build", ["cmake", "--build", "--preset", "asan", "--parallel", "1"], 600.0),
    ]
    _archive_last_test(normal_build, evidence / "normal", "before-gate", required=False)
    for label, command, timeout in normal_commands:
        _assert_native_source_snapshot(source_binding)
        if _file_sha(gcc_script) != gcc_script_sha:
            raise Refusal("saved GCC 13 reference route changed during native qualification")
        result = _run_recorded(
            evidence / label, command, cwd=ROOT, environment={}, timeout_seconds=timeout,
        )
        _assert_native_source_snapshot(source_binding)
        if label == "gate":
            _archive_last_test(
                normal_build, evidence / "normal", "after-gate",
                required=not result["timed_out"] and result["return_code"] == 0,
            )
        if result["timed_out"] or result["return_code"] != 0:
            raise Refusal(f"native command failed or timed out: {label}")

    _archive_last_test(asan_build, evidence / "asan", "before-full-ctest", required=False)
    junit = evidence / "asan/full-ctest-junit.xml"
    ctest_command = [
        "ctest", "--preset", "asan", "--no-tests=error", "--output-junit", str(junit),
    ]
    _assert_native_source_snapshot(source_binding)
    asan_result = _run_recorded(
        evidence / "asan-full-ctest", ctest_command, cwd=ROOT,
        environment={
            "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
        },
        timeout_seconds=600.0,
    )
    _assert_native_source_snapshot(source_binding)
    _archive_last_test(asan_build, evidence / "asan", "after-full-ctest", required=True)
    if asan_result["timed_out"] or asan_result["return_code"] != 0 or not junit.is_file():
        raise Refusal("full ASan/UBSan CTest inventory failed, timed out, or lacks JUnit")

    _assert_native_source_snapshot(source_binding)
    if _file_sha(gcc_script) != gcc_script_sha or _gcc13_image_identity() != gcc_image:
        raise Refusal("GCC 13 route or local image identity changed during native qualification")
    gcc = _run_recorded(
        evidence / "gcc13", gcc_command, cwd=ROOT, environment={}, timeout_seconds=120.0,
    )
    _assert_native_source_snapshot(source_binding)
    if gcc["timed_out"] or gcc["return_code"] != 0:
        raise Refusal("previously qualified read-only network-disabled GCC 13 check failed")
    native_record.update({
        "cheap_preflight": cheap_message,
        "normal_build_dir": str(normal_build),
        "asan_build_dir": str(asan_build),
        "source_binding_verified_after_all_commands": True,
    })
    _write_json(evidence / "source-binding.json", native_record)
    return evidence, source_sha


def _deadline_control() -> tuple[bool, str]:
    record = _private_dir("deadline-control-")
    grandchild_code = (
        "import os,pathlib,sys,time\n"
        "pathlib.Path(sys.argv[1]).write_text(str(os.getpid()))\n"
        "marker=pathlib.Path(sys.argv[2])\n"
        "while True:\n"
        " with marker.open('a') as stream: stream.write('x')\n"
        " time.sleep(0.02)\n"
    )
    parent_code = (
        "import subprocess,sys,time\n"
        "subprocess.Popen([sys.executable,'-B','-c',sys.argv[1],sys.argv[2],sys.argv[3]])\n"
        "open(sys.argv[4],'w').write('started')\n"
        "print('deadline parent started',flush=True)\n"
        "time.sleep(20)\n"
    )
    pid_path = record / "grandchild.pid"
    marker_path = record / "grandchild-writes.log"
    ready_path = record / "grandchild.ready"
    command = [sys.executable, "-B", "-c", parent_code, grandchild_code,
               str(pid_path), str(marker_path), str(ready_path)]
    outcome = _run_bounded(command, cwd=ROOT, environment=os.environ.copy(),
                            timeout_seconds=0.5)
    _save_process(record, command, ROOT, {}, outcome, 0.5)
    if not ready_path.is_file() or not pid_path.is_file() or not marker_path.is_file():
        _write_json(record / "deadline-control.json", {
            "grandchild_started": False,
            "process_group_empty": outcome.get("process_group_empty"),
        })
        return False, "deadline control failed before observing its child and grandchild"
    try:
        grandchild_pid = int(pid_path.read_text(encoding="ascii"))
    except (OSError, ValueError):
        return False, "deadline control grandchild PID record is malformed"
    writes_at_return = marker_path.stat().st_size
    time.sleep(0.12)
    writes_after_wait = marker_path.stat().st_size
    alive = True
    try:
        os.kill(grandchild_pid, 0)
    except ProcessLookupError:
        alive = False
    except PermissionError:
        alive = True
    _write_json(record / "deadline-control.json", {
        "grandchild_started": True,
        "grandchild_pid": grandchild_pid,
        "writes_at_return": writes_at_return,
        "writes_after_wait": writes_after_wait,
        "grandchild_pid_alive_after_wait": alive,
        "process_group_empty": outcome.get("process_group_empty"),
    })
    elapsed = outcome["elapsed_ms"]
    passed = outcome["timed_out"] is True and isinstance(elapsed, int) and elapsed < 2000 and \
        outcome.get("process_group_empty") is True and not alive and writes_after_wait == writes_at_return
    if passed:
        return True, "actual deadline killed and reaped its child and grandchild before post-deadline writes"
    return False, "deadline control left a descendant alive, writing, or outside its cleanup bound"


def _load_case_source() -> dict[str, object]:
    value = _read_json(CASE_INVENTORY)
    if not isinstance(value, dict) or value.get("schema") != "t64.rdp-vi-case-inventory" or \
            value.get("schema_version") != 1:
        raise Refusal("unsupported source-derived RDP/VI case inventory")
    suites = value.get("suites")
    if not isinstance(suites, dict) or not suites:
        raise Refusal("source-derived case inventory has no suites")
    for suite_name, suite in suites.items():
        if not isinstance(suite_name, str) or not isinstance(suite, dict):
            raise Refusal("case inventory suite is malformed")
        ctest = suite.get("ctest")
        binaries = suite.get("binaries")
        if not isinstance(ctest, list) or not ctest or not isinstance(binaries, list) or not binaries:
            raise Refusal("case inventory suite has an empty CTest or binary list")
        if len(ctest) != len(set(ctest)) or any(not isinstance(item, str) or not item for item in ctest):
            raise Refusal("case inventory CTest names are empty or duplicated")
        for binary in binaries:
            if not isinstance(binary, dict) or not isinstance(binary.get("path"), str):
                raise Refusal("case inventory binary entry is malformed")
            path = Path(binary["path"])
            cases = binary.get("cases")
            if path.is_absolute() or ".." in path.parts or not isinstance(cases, list) or not cases:
                raise Refusal("case inventory binary path or case set is invalid")
            if len(cases) != len(set(cases)) or any(not isinstance(item, str) or not item for item in cases):
                raise Refusal("case inventory names are empty or duplicated")
    qualification = value.get("qualification_ctest")
    if qualification != "harness.rdp-vi-qualification":
        raise Refusal("qualification harness CTest registration is missing")
    return value


def _inventory_snapshot() -> dict[str, object]:
    source = _load_case_source()
    source_sha256 = _file_sha(CASE_INVENTORY)
    if source_sha256 != CASE_INVENTORY_SHA256:
        raise Refusal("source-derived case inventory differs from its reviewed pin")
    return {
        "schema": "t64.rdp-vi-inventory-snapshot",
        "schema_version": 1,
        "manifest_sha256": source_sha256,
        "qualification_ctest": source["qualification_ctest"],
        "suites": source["suites"],
    }


def _inventory_summary(snapshot: dict[str, object]) -> dict[str, object]:
    suites = snapshot.get("suites")
    if not isinstance(suites, dict):
        raise Refusal("inventory snapshot suites are malformed")
    ctests: list[str] = []
    cases: set[tuple[str, str]] = set()
    for suite in suites.values():
        if not isinstance(suite, dict):
            raise Refusal("inventory snapshot suite is malformed")
        for name in suite.get("ctest", []):
            if name not in ctests:
                ctests.append(name)
        for binary in suite.get("binaries", []):
            for case in binary.get("cases", []):
                cases.add((binary["path"], case))
    qualification = snapshot.get("qualification_ctest")
    if not isinstance(qualification, str) or not qualification:
        raise Refusal("qualification CTest name is missing")
    if qualification not in ctests:
        ctests.append(qualification)
    return {"case_count": len(cases), "ctest_names": ctests}


def _normalized_command_hash(runner_sha: str, supervisor_sha: str,
                             checker_sha: str, guest_runner_sha: str,
                             fixture_sha: str,
                             input_sha: str) -> str:
    command_identity = {
        "runner": "runner/test_rdp_video",
        "selector": "accepted-cpu-fill-to-raw-frame",
        "supervisor_sha256": supervisor_sha,
        "checker_sha256": checker_sha,
        "runner_sha256": runner_sha,
        "guest_runner_sha256": guest_runner_sha,
        "fixture_sha256": fixture_sha,
        "input_sha256": input_sha,
        "export_contract": "t64.rdp-vi-observation-v1",
    }
    return _sha(_canonical(command_identity))


def _producer_files(directory: Path) -> dict[str, str]:
    paths = {
        "runner_sha256": directory / "original/bin/test_rdp_video",
        "supervisor_sha256": directory / "original/bin/t64-supervisor",
        "checker_sha256": directory / "original/bin/t64-result-check",
        "guest_runner_sha256": directory / "original/bin/t64-runner-real",
    }
    return {name: _file_sha(path) for name, path in paths.items()}


def _make_manifest(directory: Path, job: str, commit_sha: str,
                   run_id: str | None = None,
                   run_attempt: str | None = None) -> dict[str, object]:
    observation = _read_json(directory / "observation.json")
    if not isinstance(observation, dict):
        raise Refusal("producer observation is not an object")
    inventory = _inventory_snapshot()
    (directory / "inventory.json").write_bytes(_canonical(inventory))
    producer = _producer_files(directory)
    summary = _inventory_summary(inventory)
    raw_exports = []
    for name in ("rdram.bin", "vi-packet.bin"):
        content = _read_file(directory / name)
        raw_exports.append({"path": name, "size_bytes": len(content), "sha256": _sha(content)})
    fixture_sha = _file_sha(FIXTURE)
    input_sha = _file_sha(INPUT)
    execution = {
        "assertion": COMPARATOR_MARKER,
        "command_sha256": _normalized_command_hash(
            producer["runner_sha256"], producer["supervisor_sha256"],
            producer["checker_sha256"], producer["guest_runner_sha256"],
            fixture_sha, input_sha,
        ),
        "return_code": 0,
        "status": "pass",
        "timed_out": False,
    }
    manifest: dict[str, object] = {
        "schema": SCHEMA,
        "schema_version": SCHEMA_VERSION,
        "commit_sha": commit_sha,
        "job_name": job,
        "run_id": run_id,
        "run_attempt": run_attempt,
        "fixture": {
            "fixture_id": FIXTURE_ID,
            "fixture_sha256": fixture_sha,
            "input_sha256": input_sha,
        },
        "profile": {"profile_id": PROFILE_ID, "profile_sha256": PROFILE_SHA256},
        "policy": {"vi_clock_policy": VI_CLOCK_POLICY},
        "producer": {"build_id": observation.get("build_id"), **producer},
        "observation": observation,
        "private_originals": raw_exports,
        "inventory": {
            "snapshot_sha256": _sha(_canonical(inventory)),
            "source_manifest_sha256": inventory["manifest_sha256"],
            **summary,
        },
        "execution": execution,
    }
    public_bytes = _canonical(_make_public_raw(manifest, {
        "rdram.bin": _read_file(directory / "rdram.bin"),
        "vi-packet.bin": _read_file(directory / "vi-packet.bin"),
    }))
    (directory / "raw-export.json").write_bytes(public_bytes)
    manifest["public_serialization"] = {
        "path": "raw-export.json", "byte_length": len(public_bytes),
        "sha256": _sha(public_bytes),
    }
    return manifest


def _make_public_raw(manifest: dict[str, object], exports: dict[str, bytes]) -> dict[str, object]:
    observation = manifest.get("observation")
    fixture = manifest.get("fixture")
    profile = manifest.get("profile")
    policy = manifest.get("policy")
    producer = manifest.get("producer")
    if not all(isinstance(value, dict) for value in (observation, fixture, profile, policy, producer)):
        raise Refusal("cannot serialize raw bytes without complete producer provenance")
    serialized: dict[str, object] = {
        "schema": PUBLIC_RAW_SCHEMA,
        "schema_version": 1,
        "commit_sha": manifest.get("commit_sha"),
        "job_name": manifest.get("job_name"),
        "run_id": manifest.get("run_id"),
        "run_attempt": manifest.get("run_attempt"),
        "fixture": fixture,
        "profile": profile,
        "policy": policy,
        "producer_build_id": producer.get("build_id"),
        "observation_sha256": _sha(_canonical(observation)),
        "rdram": {
            "encoding": "hex-lowercase", "address": observation.get("source_address"),
            "byte_length": len(exports["rdram.bin"]),
            "sha256": _sha(exports["rdram.bin"]),
            "bytes_hex": exports["rdram.bin"].hex(),
        },
        "vi_packet": {
            "encoding": "hex-lowercase", "width": observation.get("width"),
            "height": observation.get("height"), "stride_bytes": observation.get("stride_bytes"),
            "byte_length": len(exports["vi-packet.bin"]),
            "sha256": _sha(exports["vi-packet.bin"]),
            "bytes_hex": exports["vi-packet.bin"].hex(),
        },
    }
    return serialized


def _decode_public_raw(value: object, manifest: dict[str, object]) -> dict[str, bytes]:
    raw = _exact_keys(value, {
        "schema", "schema_version", "commit_sha", "job_name", "run_id", "run_attempt",
        "fixture", "profile", "policy", "producer_build_id", "observation_sha256",
        "rdram", "vi_packet",
    })
    observation = manifest.get("observation")
    producer = manifest.get("producer")
    if not isinstance(observation, dict) or not isinstance(producer, dict):
        raise Refusal("public raw serialization lacks bound producer provenance")
    expected_identity = {
        "schema": PUBLIC_RAW_SCHEMA, "schema_version": 1,
        "commit_sha": manifest.get("commit_sha"), "job_name": manifest.get("job_name"),
        "run_id": manifest.get("run_id"), "run_attempt": manifest.get("run_attempt"),
        "fixture": manifest.get("fixture"), "profile": manifest.get("profile"),
        "policy": manifest.get("policy"), "producer_build_id": producer.get("build_id"),
        "observation_sha256": _sha(_canonical(observation)),
    }
    if any(raw.get(key) != expected for key, expected in expected_identity.items()):
        raise Refusal("public raw serialization provenance differs from its manifest")
    rows = {
        "rdram.bin": _exact_keys(raw.get("rdram"), {
            "encoding", "address", "byte_length", "sha256", "bytes_hex",
        }),
        "vi-packet.bin": _exact_keys(raw.get("vi_packet"), {
            "encoding", "width", "height", "stride_bytes", "byte_length", "sha256", "bytes_hex",
        }),
    }
    observation = manifest["observation"]
    assert isinstance(observation, dict)
    records = manifest.get("private_originals")
    if not isinstance(records, list) or len(records) != 2:
        raise Refusal("private original raw byte bindings are missing")
    expected_records = {record.get("path"): record for record in records if isinstance(record, dict)}
    decoded: dict[str, bytes] = {}
    for name, row in rows.items():
        encoded = row.get("bytes_hex")
        if row.get("encoding") != "hex-lowercase" or not isinstance(encoded, str) or \
                len(encoded) > MAX_RAW_BYTES * 2 or len(encoded) % 2 != 0 or \
                re.fullmatch(r"[0-9a-f]*", encoded) is None:
            raise Refusal("public raw byte encoding is malformed or exceeds its bound")
        try:
            data = bytes.fromhex(encoded)
        except ValueError as error:
            raise Refusal("public raw byte encoding is not hexadecimal") from error
        record = expected_records.get(name)
        if not isinstance(record, dict) or row.get("byte_length") != len(data) or \
                row.get("sha256") != _sha(data) or \
                record.get("size_bytes") != len(data) or record.get("sha256") != _sha(data):
            raise Refusal("public decoded raw bytes differ from their original byte bindings")
        if name == "rdram.bin" and row.get("address") != observation.get("source_address"):
            raise Refusal("public RDRAM byte span differs from the observed source address")
        if name == "vi-packet.bin" and (row.get("width"), row.get("height"),
                row.get("stride_bytes")) != (observation.get("width"), observation.get("height"),
                                               observation.get("stride_bytes")):
            raise Refusal("public VI byte span differs from the observed generated frame")
        decoded[name] = data
    return decoded


def _verify_public_serialization(directory: Path, manifest: dict[str, object],
                                 originals: dict[str, bytes] | None = None) -> dict[str, bytes]:
    binding = _exact_keys(manifest.get("public_serialization"), {
        "path", "byte_length", "sha256",
    })
    if binding.get("path") != "raw-export.json":
        raise Refusal("public raw serialization path differs from the exact contract")
    data = _read_file(directory / "raw-export.json", MAX_UPLOAD_BYTES)
    if binding.get("byte_length") != len(data) or binding.get("sha256") != _sha(data):
        raise Refusal("public raw serialization bytes differ from their manifest binding")
    value = _read_json(directory / "raw-export.json")
    if _canonical(value) != data:
        raise Refusal("public raw serialization is not canonical JSON")
    decoded = _decode_public_raw(value, manifest)
    if originals is not None and decoded != originals:
        raise Refusal("public decoded bytes differ from the retained private original bytes")
    return decoded


def _copy_binary(source: Path, destination: Path) -> None:
    data = _read_file(source)
    destination.write_bytes(data)
    destination.chmod(source.stat().st_mode & 0o755)


def _build_actual_bundle(directory: Path, *, job: str, commit_sha: str,
                         fixture: Path = FIXTURE, run_id: str | None = None,
                         run_attempt: str | None = None) -> dict[str, object]:
    directory.mkdir(parents=True, mode=0o700)
    original = directory / "original"
    binaries = original / "bin"
    binaries.mkdir(parents=True, mode=0o700)
    runner_copy = binaries / "test_rdp_video"
    supervisor_copy = binaries / "t64-supervisor"
    checker_copy = binaries / "t64-result-check"
    guest_runner_copy = binaries / "t64-runner-real"
    _copy_binary(RUNNER, runner_copy)
    _copy_binary(SUPERVISOR, supervisor_copy)
    _copy_binary(CHECKER, checker_copy)
    _copy_binary(GUEST_RUNNER, guest_runner_copy)
    for source, expected in ((fixture, FIXTURE_SHA256), (INPUT, INPUT_SHA256)):
        if _file_sha(source) != expected:
            raise Refusal("authored fixture or input digest changed")
    command = [
        str(runner_copy), "accepted-cpu-fill-to-raw-frame",
        str(supervisor_copy), str(checker_copy), str(fixture), str(INPUT),
    ]
    environment = {"T64_RDP_VI_EXPORT_DIR": str(directory)}
    process = _run_bounded(
        command, cwd=ROOT, environment={**os.environ, **environment},
        timeout_seconds=10.0,
    )
    _save_process(original, command, ROOT, environment, process, 10.0)
    if process["timed_out"] or process["return_code"] != 0:
        raise Refusal("actual accepted authored RDP/VI producer failed")
    stdout = process["stdout"]
    if not isinstance(stdout, bytes) or COMPARATOR_MARKER.encode() not in stdout:
        raise Refusal("actual producer did not report reaching the pixel comparator")
    if _file_sha(fixture) != FIXTURE_SHA256 or _file_sha(INPUT) != INPUT_SHA256:
        raise Refusal("fixture or input changed during actual producer run")
    manifest = _make_manifest(directory, job, commit_sha, run_id, run_attempt)
    _write_json(directory / "manifest.json", manifest)
    return manifest


def _exact_keys(value: object, keys: set[str]) -> dict[str, object]:
    if not isinstance(value, dict) or set(value) != keys:
        raise Refusal("record has missing or unexpected fields")
    return value


def _verify_observation(observation: object, build_id: object) -> dict[str, object]:
    keys = {
        "schema", "schema_version", "fixture_id", "fixture_sha256", "input_sha256",
        "profile_id", "profile_sha256", "build_id", "vi_clock_policy", "width",
        "height", "stride_bytes", "source_address", "source_format", "output_encoding",
        "field", "parity", "control", "burst", "h_sync", "v_sync", "h_start",
        "v_start", "x_scale", "y_scale", "source_bytes", "packet_bytes", "start_tick",
        "end_tick", "sequence", "commands_completed", "fullsync_completed",
        "last_command_word", "fill_command_word", "last_command_byte", "last_opcode_field",
        "fill_command_byte", "fill_opcode_field", "dpc_current", "dpc_end", "semantic_flags",
        "mi_pending_before_clear", "mi_mask_before_clear", "mi_pending_after_clear",
    }
    result = _exact_keys(observation, keys)
    expected = {
        "schema": "t64.rdp-vi-observation", "schema_version": 1,
        "fixture_id": FIXTURE_ID, "fixture_sha256": FIXTURE_SHA256,
        "input_sha256": INPUT_SHA256, "profile_id": PROFILE_ID,
        "profile_sha256": PROFILE_SHA256, "vi_clock_policy": VI_CLOCK_POLICY,
        "width": 4, "height": 3, "stride_bytes": 16,
        "source_address": 0x3000, "source_format": 2, "output_encoding": 1,
        "field": 0, "parity": 0, "control": 2, "burst": 0,
        "h_sync": 4, "v_sync": 2, "h_start": 4, "v_start": 3,
        "x_scale": 0x400, "y_scale": 0x400, "source_bytes": len(EXPECTED_SOURCE),
        "packet_bytes": len(EXPECTED_PACKET), "sequence": 0,
        "commands_completed": 6, "fullsync_completed": 1,
        "last_command_word": 0x2900000000000000,
        "fill_command_word": 0x37000000F80107C1,
        "last_command_byte": 0x29, "last_opcode_field": 0x29,
        "fill_command_byte": 0x37, "fill_opcode_field": 0x37,
        "dpc_current": 0x3130, "dpc_end": 0x3130,
        "semantic_flags": 7, "mi_pending_before_clear": 0x20,
        "mi_mask_before_clear": 0x20, "mi_pending_after_clear": 0,
    }
    for field, value in expected.items():
        if result.get(field) != value:
            raise Refusal("producer observation differs from the authored contract")
    if not isinstance(result.get("start_tick"), int) or not isinstance(result.get("end_tick"), int) or \
            result["end_tick"] - result["start_tick"] != 20:
        raise Refusal("producer observation has an unexpected guest-time interval")
    if not isinstance(build_id, str) or not BUILD_ID_RE.fullmatch(build_id) or \
            result.get("build_id") != build_id:
        raise Refusal("producer build identity is missing or inconsistent")
    return result


def _verify_local_original(directory: Path, manifest: dict[str, object]) -> None:
    original = directory / "original"
    if original.is_symlink() or not original.is_dir():
        raise Refusal("private original process record is missing")
    entries = {entry.name for entry in original.iterdir()}
    if entries != {"bin", "command.json", "process.json", "stdout.log", "stderr.log"}:
        raise Refusal("private original record contains missing or unexpected files")
    binary_dir = original / "bin"
    if binary_dir.is_symlink() or not binary_dir.is_dir() or \
            {entry.name for entry in binary_dir.iterdir()} != {
                "test_rdp_video", "t64-supervisor", "t64-result-check", "t64-runner-real"}:
        raise Refusal("retained producer executable set is incomplete")
    command = _exact_keys(_read_json(original / "command.json"), {
        "argv", "cwd", "env", "timeout_seconds"})
    process = _exact_keys(_read_json(original / "process.json"), {
        "return_code", "timed_out", "elapsed_ms", "timeout_seconds", "stdout_bytes", "stderr_bytes",
        "stdout_sha256", "stderr_sha256",
    })
    argv = command.get("argv")
    environment = command.get("env")
    if not isinstance(argv, list) or len(argv) != 6 or not isinstance(environment, dict) or \
            set(environment) != {"T64_RDP_VI_EXPORT_DIR"}:
        raise Refusal("retained process command is malformed")
    expected_argv = [
        str(binary_dir / "test_rdp_video"), "accepted-cpu-fill-to-raw-frame",
        str(binary_dir / "t64-supervisor"), str(binary_dir / "t64-result-check"),
        str(FIXTURE), str(INPUT),
    ]
    if argv != expected_argv or command.get("cwd") != str(ROOT.resolve()) or \
            command.get("timeout_seconds") != 10.0 or process.get("timeout_seconds") != 10.0 or \
            environment.get("T64_RDP_VI_EXPORT_DIR") != str(directory.resolve()):
        raise Refusal("retained command does not identify the accepted real producer")
    if process.get("return_code") != 0 or process.get("timed_out") is not False:
        raise Refusal("original process did not complete successfully")
    stdout = _read_file(original / "stdout.log")
    stderr = _read_file(original / "stderr.log")
    if process.get("stdout_bytes") != len(stdout) or process.get("stderr_bytes") != len(stderr) or \
            process.get("stdout_sha256") != _sha(stdout) or process.get("stderr_sha256") != _sha(stderr) or \
            COMPARATOR_MARKER.encode() not in stdout:
        raise Refusal("original process output does not prove the comparator was reached")
    producer = manifest.get("producer")
    if not isinstance(producer, dict):
        raise Refusal("producer identity is missing")
    retained = _producer_files(directory)
    for field, digest in retained.items():
        if producer.get(field) != digest:
            raise Refusal("retained executable bytes differ from their binding")
    if _file_sha(FIXTURE) != FIXTURE_SHA256 or _file_sha(INPUT) != INPUT_SHA256:
        raise Refusal("current authored fixture identity differs from the original command")
    if _normalized_command_hash(
            retained["runner_sha256"], retained["supervisor_sha256"],
            retained["checker_sha256"], retained["guest_runner_sha256"],
            FIXTURE_SHA256, INPUT_SHA256) != \
            manifest.get("execution", {}).get("command_sha256"):
        raise Refusal("original command binding does not match the accepted source set")


def verify_evidence(directory: Path, *, expected_sha: str | None = None,
                    expected_job: str | None = None, expected_run: str | None = None,
                    expected_attempt: str | None = None, public: bool = False) -> None:
    if directory.is_symlink() or not directory.is_dir():
        raise Refusal("evidence directory is absent or unsafe")
    manifest = _exact_keys(_read_json(directory / "manifest.json"), {
        "schema", "schema_version", "commit_sha", "job_name", "run_id", "run_attempt",
        "fixture", "profile",
        "policy", "producer", "observation", "private_originals", "public_serialization",
        "inventory", "execution",
    })
    commit_sha = manifest.get("commit_sha")
    job = manifest.get("job_name")
    if manifest.get("schema") != SCHEMA or manifest.get("schema_version") != SCHEMA_VERSION or \
            not isinstance(commit_sha, str) or not SHA1_RE.fullmatch(commit_sha) or \
            job not in {"local", "gate-linux", "gate-macos"}:
        raise Refusal("evidence identity is malformed")
    run_id = manifest.get("run_id")
    run_attempt = manifest.get("run_attempt")
    if job == "local":
        if run_id is not None or run_attempt is not None:
            raise Refusal("local evidence unexpectedly claims a hosted run identity")
    elif not isinstance(run_id, str) or not DECIMAL_RE.fullmatch(run_id) or \
            not isinstance(run_attempt, str) or not DECIMAL_RE.fullmatch(run_attempt):
        raise Refusal("hosted evidence is missing its run and attempt identity")
    if expected_sha is not None and commit_sha != expected_sha or \
            expected_job is not None and job != expected_job or \
            expected_run is not None and run_id != expected_run or \
            expected_attempt is not None and run_attempt != expected_attempt:
        raise Refusal("evidence identity differs from the selected source, job, run, or attempt")
    if manifest.get("fixture") != {
            "fixture_id": FIXTURE_ID, "fixture_sha256": FIXTURE_SHA256,
            "input_sha256": INPUT_SHA256} or \
            manifest.get("profile") != {"profile_id": PROFILE_ID,
                                         "profile_sha256": PROFILE_SHA256} or \
            manifest.get("policy") != {"vi_clock_policy": VI_CLOCK_POLICY}:
        raise Refusal("fixture, profile, or named policy differs from the authored source")
    producer = manifest.get("producer")
    if not isinstance(producer, dict) or set(producer) != {
            "build_id", "runner_sha256", "supervisor_sha256", "checker_sha256",
            "guest_runner_sha256"} or \
            not isinstance(producer.get("build_id"), str) or \
            not BUILD_ID_RE.fullmatch(producer["build_id"]) or \
            any(not isinstance(producer.get(name), str) or
                not SHA256_RE.fullmatch(producer[name]) for name in (
                    "runner_sha256", "supervisor_sha256", "checker_sha256",
                    "guest_runner_sha256")):
        raise Refusal("producer executable binding is malformed")
    observation = _verify_observation(manifest.get("observation"), producer["build_id"])
    if _read_json(directory / "observation.json") != observation:
        raise Refusal("original producer observation differs from its manifest projection")
    raw_records = manifest.get("private_originals")
    if not isinstance(raw_records, list) or len(raw_records) != 2 or any(
            not isinstance(record, dict) or set(record) != {"path", "size_bytes", "sha256"}
            for record in raw_records) or \
            [record.get("path") for record in raw_records] != ["rdram.bin", "vi-packet.bin"]:
        raise Refusal("raw export records do not name the exact two private original byte members")
    if public:
        originals = _verify_public_serialization(directory, manifest)
        if originals["rdram.bin"] != EXPECTED_SOURCE or \
                originals["vi-packet.bin"] != EXPECTED_PACKET:
            raise Refusal("public raw bytes differ from the independently authored output contract")
    else:
        originals = {
            "rdram.bin": _read_file(directory / "rdram.bin"),
            "vi-packet.bin": _read_file(directory / "vi-packet.bin"),
        }
        if originals["rdram.bin"] != EXPECTED_SOURCE or originals["vi-packet.bin"] != EXPECTED_PACKET:
            raise Refusal("private raw producer bytes differ from the authored fill contract")
        actual_records = [
            {"path": name, "size_bytes": len(data), "sha256": _sha(data)}
            for name, data in originals.items()
        ]
        if manifest.get("private_originals") != actual_records:
            raise Refusal("private original bytes differ from their recorded hashes and lengths")
        _verify_public_serialization(directory, manifest, originals)
    snapshot = _inventory_snapshot()
    snapshot_bytes = _canonical(snapshot)
    if _read_file(directory / "inventory.json") != snapshot_bytes:
        raise Refusal("case and CTest snapshots differ from the independently pinned source inventory")
    summary = _inventory_summary(snapshot)
    expected_inventory = {
        "snapshot_sha256": _sha(snapshot_bytes),
        "source_manifest_sha256": snapshot["manifest_sha256"],
        **summary,
    }
    if manifest.get("inventory") != expected_inventory:
        raise Refusal("evidence case and CTest denominator is incomplete or resealed")
    execution = _exact_keys(manifest.get("execution"), {
        "assertion", "command_sha256", "return_code", "status", "timed_out",
    })
    if execution.get("assertion") != COMPARATOR_MARKER or execution.get("return_code") != 0 or \
            execution.get("status") != "pass" or execution.get("timed_out") is not False or \
            not isinstance(execution.get("command_sha256"), str) or \
            not SHA256_RE.fullmatch(execution["command_sha256"]):
        raise Refusal("claimed process result is not an accepted bounded consumer run")
    original = directory / "original"
    if original.exists():
        _verify_local_original(directory, manifest)
    elif job == "local":
        raise Refusal("local evidence is missing its original command and process records")
    allowed = {"manifest.json", "inventory.json", "observation.json", "raw-export.json"}
    if not public:
        allowed.update({"rdram.bin", "vi-packet.bin"})
    if original.exists():
        allowed.add("original")
    extra = {entry.name for entry in directory.iterdir()} - allowed
    if extra:
        raise Refusal("evidence directory contains unexpected files")


def _upload_members(job: str) -> tuple[set[str], set[str]]:
    if job not in {"gate-linux", "gate-macos"}:
        raise Refusal("upload job must be one of the two named CI jobs")
    files = {f"{job}.json"}
    raw_dir = f"rdp-vi/{job}"
    files.update(f"{raw_dir}/{name}" for name in (
        "manifest.json", "inventory.json", "observation.json", "raw-export.json"))
    return files, {"rdp-vi", raw_dir}


def _safe_extract_upload(archive_bytes: bytes, destination: Path, job: str) -> None:
    expected_files, expected_dirs = _upload_members(job)
    if destination.is_symlink() or (destination.exists() and
                                    (not destination.is_dir() or any(destination.iterdir()))):
        raise Refusal("upload extraction destination is unsafe or not empty")
    destination.mkdir(parents=True, exist_ok=True, mode=0o700)
    try:
        upload = zipfile.ZipFile(BytesIO(archive_bytes), "r")
    except (OSError, zipfile.BadZipFile) as error:
        raise Refusal("hosted artifact is not a valid ZIP archive") from error
    with upload:
        infos = upload.infolist()
        names = [info.filename for info in infos]
        if not infos or len(names) != len(set(names)):
            raise Refusal("hosted artifact has duplicate or empty members")
        files: set[str] = set()
        dirs: set[str] = set()
        total_size = 0
        for info in infos:
            name = info.filename
            if not isinstance(name, str) or not name or "\\" in name or "\x00" in name or \
                    name.startswith("/"):
                raise Refusal("hosted artifact member has an unsafe path")
            parts = name.rstrip("/").split("/")
            if any(part in {"", ".", ".."} for part in parts):
                raise Refusal("hosted artifact member has a traversal or empty path component")
            mode = (info.external_attr >> 16) & 0o170000
            if mode == stat.S_IFLNK or info.flag_bits & 1:
                raise Refusal("hosted artifact contains a symlink or encrypted member")
            if info.is_dir():
                if name not in {f"{directory}/" for directory in expected_dirs}:
                    raise Refusal("hosted artifact contains an unexpected directory")
                dirs.add(name[:-1])
                continue
            if name not in expected_files or mode not in {0, stat.S_IFREG}:
                raise Refusal("hosted artifact contains an unexpected or special file")
            files.add(name)
            total_size += info.file_size
            if total_size > MAX_UPLOAD_BYTES or info.file_size > MAX_UPLOAD_BYTES:
                raise Refusal("hosted artifact exceeds the unpacked byte bound")
        if files != expected_files or not dirs.issubset(expected_dirs):
            raise Refusal("hosted artifact members differ from the combined CI upload contract")
        try:
            for info in infos:
                if info.is_dir():
                    continue
                target = destination.joinpath(*info.filename.split("/"))
                target.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
                total = 0
                with upload.open(info, "r") as source, target.open("xb") as output:
                    while True:
                        part = source.read(64 * 1024)
                        if not part:
                            break
                        total += len(part)
                        if total > info.file_size or total > MAX_UPLOAD_BYTES:
                            raise Refusal("hosted artifact member expands beyond its declared byte bound")
                        output.write(part)
                if total != info.file_size:
                    raise Refusal("hosted artifact member length differs from its ZIP record")
                target.chmod(0o600)
        except (OSError, zipfile.BadZipFile, EOFError) as error:
            shutil.rmtree(destination, ignore_errors=True)
            raise Refusal("hosted artifact member could not be extracted safely") from error
        except Refusal:
            shutil.rmtree(destination, ignore_errors=True)
            raise


def verify_upload(directory: Path, *, expected_sha: str, expected_job: str,
                  expected_run: str, expected_attempt: str) -> None:
    if directory.is_symlink() or not directory.is_dir():
        raise Refusal("combined upload directory is absent or unsafe")
    expected_files, expected_dirs = _upload_members(expected_job)
    found_files: set[str] = set()
    found_dirs: set[str] = set()
    for path in directory.rglob("*"):
        relative = path.relative_to(directory).as_posix()
        if path.is_symlink():
            raise Refusal("combined upload contains a symlink")
        if path.is_dir():
            found_dirs.add(relative)
        elif path.is_file():
            found_files.add(relative)
        else:
            raise Refusal("combined upload contains a special filesystem entry")
    if found_files != expected_files or found_dirs != expected_dirs:
        raise Refusal("combined upload contains missing, extra, duplicate, or nested members")
    boot = directory / f"{expected_job}.json"
    boot_record = _exact_keys(_read_json(boot), {
        "schema", "schema_version", "commit_sha", "run_id", "run_attempt", "job_name", "result",
    })
    if boot_record.get("schema") != "t64.boot-evidence" or boot_record.get("schema_version") != 1 or \
            boot_record.get("commit_sha") != expected_sha or boot_record.get("run_id") != expected_run or \
            boot_record.get("run_attempt") != expected_attempt or boot_record.get("job_name") != expected_job:
        raise Refusal("boot evidence in the combined upload has the wrong run identity")
    checker_command = [
        str(CHECKER), "boot-evidence", "--sha", expected_sha, "--run", expected_run,
        "--attempt", expected_attempt, "--job", expected_job, str(boot),
    ]
    checker = _run_bounded(checker_command, cwd=ROOT, environment=os.environ.copy(), timeout_seconds=10.0)
    if checker["timed_out"] or checker["return_code"] != 0:
        raise Refusal("boot evidence failed its existing strict C consumer")
    verify_evidence(
        directory / "rdp-vi" / expected_job, expected_sha=expected_sha,
        expected_job=expected_job, expected_run=expected_run, expected_attempt=expected_attempt,
        public=True,
    )


def _emit_layout_selftest(destination: Path, *, job: str, commit_sha: str,
                          run_id: str, run_attempt: str) -> None:
    if job not in {"gate-linux", "gate-macos"} or not SHA1_RE.fullmatch(commit_sha) or \
            not DECIMAL_RE.fullmatch(run_id) or not DECIMAL_RE.fullmatch(run_attempt):
        raise Refusal("layout selftest identity is malformed")
    if destination.is_symlink() or destination.exists():
        raise Refusal("layout selftest destination already exists or is unsafe")
    private_root = _private_dir(f"layout-original-{job}-")
    private = private_root / "original-run"
    _build_actual_bundle(
        private, job=job, commit_sha=commit_sha, run_id=run_id, run_attempt=run_attempt,
    )
    verify_evidence(
        private, expected_sha=commit_sha, expected_job=job,
        expected_run=run_id, expected_attempt=run_attempt,
    )
    destination.mkdir(parents=True, mode=0o700)
    for name in ("manifest.json", "inventory.json", "observation.json", "raw-export.json"):
        shutil.copyfile(private / name, destination / name)
        (destination / name).chmod(0o600)


def _package_upload(directory: Path, archive_path: Path, *, job: str,
                    commit_sha: str, run_id: str, run_attempt: str) -> None:
    verify_upload(
        directory, expected_sha=commit_sha, expected_job=job,
        expected_run=run_id, expected_attempt=run_attempt,
    )
    if archive_path.is_symlink() or archive_path.exists():
        raise Refusal("upload archive destination already exists or is unsafe")
    expected_files, expected_dirs = _upload_members(job)
    with zipfile.ZipFile(archive_path, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for relative in sorted(expected_dirs):
            archive.writestr(relative + "/", b"")
        for relative in sorted(expected_files):
            archive.write(directory / relative, arcname=relative)
    archive_path.chmod(0o600)
    if archive_path.stat().st_size > MAX_UPLOAD_BYTES:
        archive_path.unlink()
        raise Refusal("combined upload ZIP exceeds its compressed byte bound")


def _mutate_upload_archive(source: Path, destination: Path, job: str,
                           mutation: str) -> None:
    if mutation not in {"extra", "duplicate", "traversal", "raw-data", "raw-resealed"}:
        raise Refusal("unsupported upload archive mutation")
    source_bytes = _read_file(source, MAX_UPLOAD_BYTES)
    expected_files, _expected_dirs = _upload_members(job)
    if destination.is_symlink() or destination.exists():
        raise Refusal("mutated archive destination already exists or is unsafe")
    with zipfile.ZipFile(BytesIO(source_bytes), "r") as incoming:
        entries = [(info.filename, incoming.read(info)) for info in incoming.infolist()]
    if mutation == "raw-data":
        raw_name = f"rdp-vi/{job}/raw-export.json"
        raw_bytes = next(data for name, data in entries if name == raw_name)
        raw_value = json.loads(raw_bytes, object_pairs_hook=_pairs_no_duplicates)
        encoded = raw_value["rdram"]["bytes_hex"]
        raw_value["rdram"]["bytes_hex"] = ("0" if encoded[0] != "0" else "1") + encoded[1:]
        forged = (json.dumps(raw_value, ensure_ascii=True, sort_keys=True,
                             separators=(",", ":")) + "\n").encode("utf-8")
        entries = [(name, forged if name == raw_name else data) for name, data in entries]
    elif mutation == "raw-resealed":
        raw_name = f"rdp-vi/{job}/raw-export.json"
        manifest_name = f"rdp-vi/{job}/manifest.json"
        raw_value = json.loads(
            next(data for name, data in entries if name == raw_name),
            object_pairs_hook=_pairs_no_duplicates,
        )
        manifest = json.loads(
            next(data for name, data in entries if name == manifest_name),
            object_pairs_hook=_pairs_no_duplicates,
        )
        source_row = raw_value["rdram"]
        forged_source = bytearray.fromhex(source_row["bytes_hex"])
        forged_source[0] ^= 1
        forged_bytes = bytes(forged_source)
        source_row["bytes_hex"] = forged_bytes.hex()
        source_row["byte_length"] = len(forged_bytes)
        source_row["sha256"] = _sha(forged_bytes)
        raw_bytes = _canonical(raw_value)
        original_row = next(
            record for record in manifest["private_originals"]
            if record["path"] == "rdram.bin"
        )
        original_row["size_bytes"] = len(forged_bytes)
        original_row["sha256"] = _sha(forged_bytes)
        manifest["public_serialization"] = {
            "path": "raw-export.json",
            "byte_length": len(raw_bytes),
            "sha256": _sha(raw_bytes),
        }
        manifest_bytes = _canonical(manifest)
        entries = [
            (name, raw_bytes if name == raw_name else
             manifest_bytes if name == manifest_name else data)
            for name, data in entries
        ]
    with zipfile.ZipFile(destination, "x", compression=zipfile.ZIP_DEFLATED) as outgoing:
        for name, data in entries:
            outgoing.writestr(name, data)
        if mutation == "extra":
            outgoing.writestr("unexpected.txt", b"unexpected")
        elif mutation == "duplicate":
            duplicate_name = f"{job}.json"
            duplicate_data = next(data for name, data in entries if name == duplicate_name)
            with warnings.catch_warnings():
                warnings.filterwarnings("ignore", message="Duplicate name:.*", category=UserWarning)
                outgoing.writestr(duplicate_name, duplicate_data)
        elif mutation == "traversal":
            outgoing.writestr("../escape.bin", b"traversal")
    destination.chmod(0o600)


def _verify_collected_pair(directory: Path, *, commit_sha: str, run_id: str,
                          run_attempt: str) -> dict[str, dict[str, object]]:
    if directory.is_symlink() or not directory.is_dir():
        raise Refusal("collected artifact pair is absent or unsafe")
    expected_files = {"gate-linux.json", "gate-macos.json"}
    expected_dirs = {"rdp-vi", "rdp-vi/gate-linux", "rdp-vi/gate-macos"}
    for job in ("gate-linux", "gate-macos"):
        expected_files.update(f"rdp-vi/{job}/{name}" for name in (
            "manifest.json", "inventory.json", "observation.json", "raw-export.json"))
    found_files: set[str] = set()
    found_dirs: set[str] = set()
    for path in directory.rglob("*"):
        relative = path.relative_to(directory).as_posix()
        if path.is_symlink():
            raise Refusal("collected pair contains a symlink")
        if path.is_dir():
            found_dirs.add(relative)
        elif path.is_file():
            found_files.add(relative)
        else:
            raise Refusal("collected pair contains a special filesystem entry")
    if found_files != expected_files or found_dirs != expected_dirs:
        raise Refusal("collected pair contains missing or unexpected members")
    manifests: dict[str, dict[str, object]] = {}
    for job in ("gate-linux", "gate-macos"):
        boot = directory / f"{job}.json"
        checker_command = [
            str(CHECKER), "boot-evidence", "--sha", commit_sha, "--run", run_id,
            "--attempt", run_attempt, "--job", job, str(boot),
        ]
        checker = _run_bounded(
            checker_command, cwd=ROOT, environment=os.environ.copy(), timeout_seconds=10.0,
        )
        if checker["timed_out"] or checker["return_code"] != 0:
            raise Refusal(f"collected {job} boot record failed the strict C consumer")
        raw_dir = directory / "rdp-vi" / job
        verify_evidence(
            raw_dir, expected_sha=commit_sha, expected_job=job,
            expected_run=run_id, expected_attempt=run_attempt, public=True,
        )
        manifest = _read_json(raw_dir / "manifest.json")
        assert isinstance(manifest, dict)
        manifests[job] = manifest
    linux, macos = manifests["gate-linux"], manifests["gate-macos"]
    linux_raw = _verify_public_serialization(
        directory / "rdp-vi/gate-linux", manifests["gate-linux"])
    macos_raw = _verify_public_serialization(
        directory / "rdp-vi/gate-macos", manifests["gate-macos"])
    if linux_raw != macos_raw:
        raise Refusal("the two host raw RDRAM/VI exports differ")
    def semantic_projection(manifest: dict[str, object]) -> dict[str, object]:
        projection = dict(manifest)
        projection.pop("job_name", None)
        projection.pop("producer", None)
        projection.pop("public_serialization", None)
        observation = dict(projection["observation"])
        observation.pop("build_id", None)
        projection["observation"] = observation
        execution = dict(projection["execution"])
        execution.pop("command_sha256", None)
        projection["execution"] = execution
        return projection
    if semantic_projection(linux) != semantic_projection(macos):
        raise Refusal("the two hosts disagree on the authored RDP/VI semantic metadata")
    return manifests


def _collect_hosted(repo: str, commit_sha: str, run_id: str,
                    run_attempt: str, output: Path) -> tuple[Path, Path]:
    if repo != "szTheory/tendies64" or not SHA1_RE.fullmatch(commit_sha) or \
            not DECIMAL_RE.fullmatch(run_id) or not DECIMAL_RE.fullmatch(run_attempt):
        raise Refusal("hosted collection identity is malformed")
    if output.is_symlink() or output.exists():
        raise Refusal("hosted collection destination already exists or is unsafe")
    process_dir = _private_dir(f"collect-hosted-{commit_sha[:12]}-")
    command = [
        "sh", "scripts/collect-boot-evidence.sh", "--collect", "--repo", repo,
        "--sha", commit_sha, "--run", run_id, "--attempt", run_attempt,
        "--out", str(output),
    ]
    process = _run_recorded(
        process_dir / "collection", command, cwd=ROOT, environment={}, timeout_seconds=180.0,
    )
    stdout = process["stdout"]
    stderr = process["stderr"]
    assert isinstance(stdout, bytes) and isinstance(stderr, bytes)
    if process["timed_out"] or process["return_code"] != 0:
        raise Refusal("exact hosted run/job/artifact collection failed or timed out")
    manifests = _verify_collected_pair(
        output, commit_sha=commit_sha, run_id=run_id, run_attempt=run_attempt,
    )
    comparison = {
        "schema": "t64.rdp-vi-hosted-pair",
        "schema_version": 1,
        "commit_sha": commit_sha,
        "run_id": run_id,
        "run_attempt": run_attempt,
        "jobs": {job: {
            "producer_build_id": manifests[job]["producer"]["build_id"],
            "producer_executable_sha256": {
                key: value for key, value in manifests[job]["producer"].items()
                if key != "build_id"
            },
        } for job in ("gate-linux", "gate-macos")},
        "semantic_raw_exports_equal": True,
        "public_serialization_sha256": {
            job: _file_sha(output / "rdp-vi" / job / "raw-export.json")
            for job in ("gate-linux", "gate-macos")
        },
        "rdram_sha256": manifests["gate-linux"]["private_originals"][0]["sha256"],
        "vi_packet_sha256": manifests["gate-linux"]["private_originals"][1]["sha256"],
        "boot_linux_sha256": _file_sha(output / "gate-linux.json"),
        "boot_macos_sha256": _file_sha(output / "gate-macos.json"),
        "collection_stdout_sha256": _sha(stdout),
        "collection_stderr_sha256": _sha(stderr),
    }
    _write_json(process_dir / "comparison.json", comparison)
    print(f"qualify-rdp-vi: exact hosted artifacts verified for {commit_sha} run {run_id} attempt {run_attempt}")
    print("qualify-rdp-vi: Linux/macOS authored RDRAM, VI bytes, profile, policy, and timing metadata agree")
    print(f"qualify-rdp-vi: private collection records retained at {process_dir}")
    return output, process_dir


def _verify_cli(directory: Path, label: str, record_dir: Path) -> bool:
    command = [sys.executable, "-B", str(SCRIPT), "--verify-evidence", str(directory)]
    outcome = _run_bounded(command, cwd=ROOT, environment=os.environ.copy(), timeout_seconds=5.0)
    stdout = outcome["stdout"]
    stderr = outcome["stderr"]
    assert isinstance(stdout, bytes) and isinstance(stderr, bytes)
    _write_json(record_dir / f"{label}.json", {
        "argv": command,
        "return_code": outcome["return_code"],
        "timed_out": outcome["timed_out"],
        "elapsed_ms": outcome["elapsed_ms"],
        "stdout_sha256": _sha(stdout),
        "stderr_sha256": _sha(stderr),
    })
    return not outcome["timed_out"] and outcome["return_code"] == 0


def _run_wrong_pixel(bundle: Path, record_dir: Path) -> None:
    bin_dir = bundle / "original/bin"
    command = [
        str(bin_dir / "test_rdp_video"), "same-setup-wrong-pixel",
        str(bin_dir / "t64-supervisor"), str(bin_dir / "t64-result-check"),
        str(FIXTURE), str(INPUT),
    ]
    outcome = _run_bounded(command, cwd=ROOT, environment=os.environ.copy(),
                           timeout_seconds=10.0)
    stdout = outcome["stdout"]
    stderr = outcome["stderr"]
    assert isinstance(stdout, bytes) and isinstance(stderr, bytes)
    _save_process(record_dir, command, ROOT, {}, outcome, 10.0)
    if outcome["timed_out"] or outcome["return_code"] != 0 or \
            WRONG_PIXEL_MARKER.encode() not in stdout:
        raise Refusal("same-setup wrong-pixel comparator did not reach its changed literal")


def _clone_bundle(source: Path, destination: Path) -> Path:
    shutil.copytree(source, destination)
    command_path = destination / "original/command.json"
    command = _read_json(command_path)
    assert isinstance(command, dict)
    command["env"]["T64_RDP_VI_EXPORT_DIR"] = str(destination.resolve())
    for index, name in ((0, "test_rdp_video"), (2, "t64-supervisor"),
                        (3, "t64-result-check")):
        command["argv"][index] = str(destination / "original/bin" / name)
    _write_json(command_path, command)
    return destination


def _mutate_inventory(directory: Path, *, empty: bool) -> None:
    snapshot = _read_json(directory / "inventory.json")
    assert isinstance(snapshot, dict) and isinstance(snapshot.get("suites"), dict)
    if empty:
        for suite in snapshot["suites"].values():
            suite["ctest"] = []
            for binary in suite["binaries"]:
                binary["cases"] = []
        snapshot["qualification_ctest"] = ""
    else:
        filters = snapshot["suites"]["vi-filters"]
        target = next(binary for binary in filters["binaries"]
                      if binary["path"] == "core/test_vi")
        target["cases"].remove("vi-aa-next-row-inside-rdram")
    snapshot_bytes = _canonical(snapshot)
    (directory / "inventory.json").write_bytes(snapshot_bytes)
    manifest = _read_json(directory / "manifest.json")
    assert isinstance(manifest, dict) and isinstance(manifest.get("inventory"), dict)
    summary = {"case_count": 0, "ctest_names": []} if empty else _inventory_summary(snapshot)
    manifest["inventory"] = {
        "snapshot_sha256": _sha(snapshot_bytes),
        "source_manifest_sha256": snapshot.get("manifest_sha256"),
        **summary,
    }
    _write_json(directory / "manifest.json", manifest)


def _save_refused_fixture_attempt(bundle: Path, failed: Path) -> None:
    failed.mkdir(parents=True, mode=0o700)
    original = failed / "original"
    binary_dir = original / "bin"
    binary_dir.mkdir(parents=True, mode=0o700)
    for name in ("test_rdp_video", "t64-supervisor", "t64-result-check", "t64-runner-real"):
        _copy_binary(bundle / "original/bin" / name, binary_dir / name)
    corrupt_fixture = failed / "replaced.z64"
    fixture_bytes = bytearray(_read_file(FIXTURE))
    fixture_bytes[0] ^= 1
    corrupt_fixture.write_bytes(fixture_bytes)
    command = [
        str(binary_dir / "test_rdp_video"), "accepted-cpu-fill-to-raw-frame",
        str(binary_dir / "t64-supervisor"), str(binary_dir / "t64-result-check"),
        str(corrupt_fixture), str(INPUT),
    ]
    environment = {"T64_RDP_VI_EXPORT_DIR": str(failed)}
    outcome = _run_bounded(command, cwd=ROOT, environment={**os.environ, **environment},
                           timeout_seconds=10.0)
    _save_process(original, command, ROOT, environment, outcome, 10.0)
    if outcome["timed_out"] or outcome["return_code"] == 0:
        raise Refusal("forged-report control did not produce an original failed process outcome")


def _native_source_control() -> str:
    control = _private_dir("native-source-control-")
    repo = control / "repo"
    repo.mkdir()
    subprocess.run(["git", "init", "--quiet", str(repo)], check=True,
                   capture_output=True, timeout=10)
    subprocess.run(["git", "-C", str(repo), "config", "user.name", "Tendies64 source control"],
                   check=True, capture_output=True, timeout=10)
    subprocess.run(["git", "-C", str(repo), "config", "user.email", "source-control@example.invalid"],
                   check=True, capture_output=True, timeout=10)
    (repo / "core").mkdir()
    (repo / ".planning").mkdir()
    source = _read_file(ROOT / "core/t64_vi.c")
    (repo / "core/t64_vi.c").write_bytes(source)
    (repo / ".gitignore").write_text("core/ignored.c\n", encoding="utf-8")
    (repo / "AGENTS.md").write_text("owner instructions\n", encoding="utf-8")
    (repo / ".planning/config.json").write_text("owner config\n", encoding="utf-8")
    (repo / ".planning/AUTONOMY.md").write_text("owner policy\n", encoding="utf-8")
    subprocess.run(["git", "-C", str(repo), "add", "--", "."], check=True,
                   capture_output=True, timeout=10)
    subprocess.run(["git", "-C", str(repo), "commit", "--quiet", "-m", "source-control fixture"],
                   check=True, capture_output=True, timeout=10)
    baseline = _native_source_snapshot(repo)

    def must_refuse(label: str) -> None:
        try:
            _native_source_snapshot(repo)
        except Refusal:
            return
        raise Refusal(f"native source preflight accepted {label}")

    source_path = repo / "core/t64_vi.c"
    source_path.write_bytes(source + b"\n/* relevant tracked mutation */\n")
    must_refuse("a modified relevant tracked input")
    try:
        _assert_native_source_snapshot(baseline)
    except Refusal:
        pass
    else:
        raise Refusal("native source post-run guard accepted a relevant tracked mutation")
    source_path.write_bytes(source)
    (repo / "AGENTS.md").write_text("changed owner instructions\n", encoding="utf-8")
    (repo / ".planning/config.json").write_text("changed owner config\n", encoding="utf-8")
    (repo / ".planning/AUTONOMY.md").write_text("changed owner policy\n", encoding="utf-8")
    owner_edit_snapshot = _native_source_snapshot(repo)
    if owner_edit_snapshot != baseline:
        raise Refusal("native source binding included unrelated owner instruction or policy edits")
    (repo / "core/untracked.c").write_text("int new_source;\n", encoding="utf-8")
    must_refuse("an untracked relevant input")
    (repo / "core/untracked.c").unlink()
    (repo / "core/ignored.c").write_text("int ignored_source;\n", encoding="utf-8")
    must_refuse("an ignored but newly created relevant input")
    (repo / "core/ignored.c").unlink()
    (repo / "core/alias.c").symlink_to("t64_vi.c")
    subprocess.run(["git", "-C", str(repo), "add", "--", "core/alias.c"], check=True,
                   capture_output=True, timeout=10)
    subprocess.run(["git", "-C", str(repo), "commit", "--quiet", "-m", "symlink control"],
                   check=True, capture_output=True, timeout=10)
    must_refuse("a committed relevant symlink")
    source_record = next(
        item for item in baseline["input_paths"]
        if isinstance(item, dict) and item.get("path") == "core/t64_vi.c"
    )
    _write_json(control / "control.json", {
        "schema": "t64.native-source-drift-control",
        "schema_version": 1,
        "synthetic_repo_source_sha": baseline["source_sha"],
        "relevant_source": source_record,
        "input_manifest_sha256": baseline["input_manifest_sha256"],
        "refused": [
            "modified relevant tracked source at startup",
            "modified relevant tracked source during a run",
            "new untracked relevant source",
            "new ignored relevant source",
            "committed relevant symlink",
        ],
        "owner_edits_allowed": [
            "AGENTS.md", ".planning/config.json", ".planning/AUTONOMY.md",
        ],
        "owner_edits_preserved_source_identity": owner_edit_snapshot == baseline,
    })
    return "relevant tracked/untracked/ignored/symlink drift refused; owner-file edits allowed"


def _selftest() -> tuple[bool, str]:
    source_control_message = _native_source_control()
    deadline_ok, deadline_message = _deadline_control()
    if not deadline_ok:
        raise Refusal(deadline_message)
    for path in (RUNNER, SUPERVISOR, CHECKER, GUEST_RUNNER, FIXTURE, INPUT):
        if not path.is_file():
            raise Refusal("gate build and authored fixture are required for qualification selftest")
    run_root = _private_dir("selftest-")
    valid = run_root / "accepted-original"
    _build_actual_bundle(valid, job="local", commit_sha=_git_head())
    if not _verify_cli(valid, "accepted-original", run_root):
        raise Refusal("final CLI verifier refused the actual accepted original")
    _run_wrong_pixel(valid, run_root / "wrong-pixel")

    expected_controls = {
        "missing-packet": lambda path: (path / "vi-packet.bin").unlink(),
        "swapped-binding": lambda path: _change_producer_binding(path),
        "zero-test-resealed-snapshots": lambda path: _mutate_inventory(path, empty=True),
        "missing-v9-case-resealed-snapshots": lambda path: _mutate_inventory(path, empty=False),
    }
    for label, mutation in expected_controls.items():
        variant = _clone_bundle(valid, run_root / label)
        mutation(variant)
        if _verify_cli(variant, label, run_root):
            raise Refusal(f"final CLI verifier accepted mutation control: {label}")

    failed = run_root / "failed-original"
    _save_refused_fixture_attempt(valid, failed)
    forged = _clone_bundle(valid, run_root / "forged-success-report")
    for name in ("command.json", "process.json", "stdout.log", "stderr.log"):
        shutil.copyfile(failed / "original" / name, forged / "original" / name)
    command = _read_json(forged / "original/command.json")
    assert isinstance(command, dict)
    command["argv"][0] = str(forged / "original/bin/test_rdp_video")
    command["argv"][2] = str(forged / "original/bin/t64-supervisor")
    command["argv"][3] = str(forged / "original/bin/t64-result-check")
    command["env"]["T64_RDP_VI_EXPORT_DIR"] = str(forged.resolve())
    _write_json(forged / "original/command.json", command)
    if _verify_cli(forged, "forged-success-report", run_root):
        raise Refusal("final CLI verifier accepted a success report over an original refusal")
    emitter = _run_recorded(
        run_root / "hosted-emitter-entry",
        [sys.executable, "-B", str(ROOT / "scripts/tests/test_rdp_vi_qualification.py"),
         "RdpViQualificationTests.test_actual_hosted_emitter_entrypoint"],
        cwd=ROOT, environment={"PYTHONDONTWRITEBYTECODE": "1"}, timeout_seconds=30.0,
    )
    if emitter["timed_out"] or emitter["return_code"] != 0:
        raise Refusal("actual hosted emitter entrypoint control failed before qualification")
    return True, (
        "actual accepted producer, reached wrong-pixel comparator, child/grandchild subprocess deadline, "
        "actual hosted emitter entrypoint, "
        f"native source binding control ({source_control_message}), "
        "missing/swapped/zero/missing-v9/forged original-record controls passed"
    )


def _change_producer_binding(directory: Path) -> None:
    manifest = _read_json(directory / "manifest.json")
    assert isinstance(manifest, dict) and isinstance(manifest.get("producer"), dict)
    manifest["producer"]["runner_sha256"] = manifest["producer"]["checker_sha256"]
    _write_json(directory / "manifest.json", manifest)


def _git_head(root: Path = ROOT) -> str:
    result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root,
                            capture_output=True, text=True, timeout=5, check=False)
    if result.returncode != 0:
        raise Refusal("cannot bind evidence to the current source revision")
    value = result.stdout.strip()
    if not SHA1_RE.fullmatch(value):
        raise Refusal("current source revision is malformed")
    return value


def _git_bytes(root: Path, arguments: list[str]) -> bytes:
    result = subprocess.run(
        ["git", *arguments], cwd=root, capture_output=True, timeout=10, check=False,
    )
    if result.returncode != 0:
        raise Refusal("cannot inspect the selected native source inputs")
    return result.stdout


def _native_source_snapshot(root: Path, source_sha: str | None = None) -> dict[str, object]:
    root = root.resolve()
    selected_sha = source_sha or _git_head(root)
    if not SHA1_RE.fullmatch(selected_sha) or _git_head(root) != selected_sha:
        raise Refusal("native source HEAD changed from the selected commit")
    pathspecs = list(NATIVE_INPUT_PATHS)
    status = _git_bytes(root, [
        "status", "--porcelain=v1", "-z", "--untracked-files=all", "--ignored=matching",
        "--", *pathspecs,
    ])
    if status:
        raise Refusal("relevant native source, build, fixture, or test inputs differ from the selected commit")
    tree = _git_bytes(root, ["ls-tree", "-r", "-z", selected_sha, "--", *pathspecs])
    entries: list[dict[str, object]] = []
    for record in tree.split(b"\0"):
        if not record:
            continue
        try:
            metadata, path_bytes = record.split(b"\t", 1)
            mode_bytes, object_type, object_id = metadata.split(b" ", 2)
            relative = path_bytes.decode("utf-8", errors="strict")
            mode = mode_bytes.decode("ascii")
            git_blob = object_id.decode("ascii")
        except (ValueError, UnicodeError) as error:
            raise Refusal("native source inventory contains an unparseable path") from error
        if object_type != b"blob" or mode not in {"100644", "100755"} or \
                not re.fullmatch(r"[0-9a-f]{40,64}", git_blob):
            raise Refusal("native source inventory contains a symlink, gitlink, or special file")
        path = root / relative
        try:
            metadata = path.lstat()
        except OSError as error:
            raise Refusal("a tracked native source input is absent") from error
        if stat.S_ISLNK(metadata.st_mode) or not stat.S_ISREG(metadata.st_mode):
            raise Refusal("a native source input is a symlink or special file")
        executable = bool(metadata.st_mode & 0o111)
        if executable != (mode == "100755"):
            raise Refusal("native source executable mode differs from the selected commit")
        content = _read_file(path)
        actual_blob = subprocess.run(
            ["git", "hash-object", "--no-filters", "--", relative], cwd=root,
            capture_output=True, text=True, timeout=10, check=False,
        )
        if actual_blob.returncode != 0 or actual_blob.stdout.strip() != git_blob:
            raise Refusal("native source bytes differ from the selected commit")
        entries.append({
            "path": relative,
            "git_blob_id": git_blob,
            "git_mode": mode,
            "sha256": _sha(content),
        })
    entries.sort(key=lambda item: str(item["path"]))
    if not entries:
        raise Refusal("selected native source input inventory is empty")
    return {
        "source_sha": selected_sha,
        "input_paths": entries,
        "input_manifest_sha256": _sha(_canonical(entries)),
    }


def _assert_native_source_snapshot(expected: dict[str, object]) -> None:
    source_sha = expected.get("source_sha")
    if not isinstance(source_sha, str):
        raise Refusal("native source binding is malformed")
    current = _native_source_snapshot(ROOT, source_sha)
    if current != expected:
        raise Refusal("native source inputs changed while qualification was running")


def _phase07_changed_c_sources(source_sha: str, root: Path = ROOT) -> list[str]:
    base = PHASE07_SOURCE_BASE
    ancestor = subprocess.run(
        ["git", "merge-base", "--is-ancestor", base, source_sha], cwd=root,
        capture_output=True, timeout=10, check=False,
    )
    if ancestor.returncode != 0:
        raise Refusal("Phase07 GCC source baseline is not an ancestor of the selected revision")
    output = _git_bytes(root, [
        "diff", "--name-only", "-z", "--diff-filter=ACMR", f"{base}..{source_sha}",
        "--", "core", "runner", "tools/common",
    ])
    paths = sorted({
        item.decode("utf-8", errors="strict") for item in output.split(b"\0")
        if item.endswith(b".c")
    })
    if "runner/tests/test_rdp_video.c" not in paths or not any(
            path.startswith("core/") and "/tests/" not in path for path in paths):
        raise Refusal("Phase07 GCC source closure misses production C or the owning RDP/VI consumer")
    return paths


def _gcc13_image_identity() -> dict[str, str]:
    result = subprocess.run(
        ["docker", "image", "inspect", "--format", "{{.Id}} {{.Os}}/{{.Architecture}}", "gcc:13"],
        cwd=ROOT, capture_output=True, text=True, timeout=5, check=False,
    )
    fields = result.stdout.strip().split()
    if result.returncode != 0 or len(fields) != 2 or \
            not re.fullmatch(r"sha256:[0-9a-f]{64}", fields[0]) or fields[1] != "linux/amd64":
        raise Refusal("the existing local read-only GCC 13 linux/amd64 image is unavailable")
    return {"image_id": fields[0], "platform": fields[1]}


def _gcc13_closure_command(source_paths: list[str], image_id: str) -> list[str]:
    script = r'''set -eu
set -f
warnings="-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wvla -Wcast-qual -Wformat=2 -Wwrite-strings -Werror -Wdouble-promotion"
core_flags="-std=c17 -O3 -DNDEBUG -DSOFTFLOAT_FAST_INT64=1 -Icore/include -Itools/common -Icore/softfloat -Icore $warnings -Wframe-larger-than=2048"
test_flags="-std=c17 -O3 -DNDEBUG -Icore/include -Icore -Itests $warnings"
runner_flags="-std=c17 -O3 -DNDEBUG -D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Irunner -Itools/common -Icore/include $warnings"
index=0
for source_file in "$@"; do
  index=$((index + 1))
  case "$source_file" in
    core/tests/*.c) gcc $test_flags -c "$source_file" -o "/tmp/phase07-$index.o" ;;
    core/*.c) gcc $core_flags -c "$source_file" -o "/tmp/phase07-$index.o" ;;
    runner/*.c|runner/tests/*.c) gcc $runner_flags -c "$source_file" -o "/tmp/phase07-$index.o" ;;
    tools/common/*.c) gcc -std=c17 -O3 -DNDEBUG -Itools/common $warnings -c "$source_file" -o "/tmp/phase07-$index.o" ;;
    *) echo "outside the Phase07 GCC closure: $source_file" >&2; exit 2 ;;
  esac
  echo "compiled $source_file"
  if [ "$source_file" = runner/t64_real.c ]; then
    gcc $runner_flags -DT64_REAL_RUNNER_TESTING -c "$source_file" -o "/tmp/phase07-$index-real-test.o"
  elif [ "$source_file" = runner/t64_supervisor.c ]; then
    gcc $runner_flags -DT64_SUPERVISOR_TESTING -c "$source_file" -o "/tmp/phase07-$index-supervisor-test.o"
  fi
done
'''
    if not source_paths or any(
            not re.fullmatch(r"[A-Za-z0-9_./-]+\.c", path) for path in source_paths):
        raise Refusal("Phase07 GCC source closure contains an unsafe path")
    return [
        "docker", "run", "--rm", "--pull=never", "--platform", "linux/amd64",
        "--network", "none", "--read-only", "--tmpfs", "/tmp:size=64m",
        "-v", f"{ROOT}:/src:ro", "-w", "/src", image_id,
        "sh", "-ec", script, "phase07-gcc13", *source_paths,
    ]


def _emit_hosted(job: str, commit_sha: str) -> None:
    if job not in {"gate-linux", "gate-macos"} or not SHA1_RE.fullmatch(commit_sha):
        raise Refusal("hosted emitter identity is invalid")
    if os.environ.get("GITHUB_JOB") != job or os.environ.get("GITHUB_SHA") != commit_sha:
        raise Refusal("hosted emitter arguments differ from the actual CI job identity")
    run_id = os.environ.get("GITHUB_RUN_ID")
    run_attempt = os.environ.get("GITHUB_RUN_ATTEMPT")
    if not isinstance(run_id, str) or not DECIMAL_RE.fullmatch(run_id) or \
            not isinstance(run_attempt, str) or not DECIMAL_RE.fullmatch(run_attempt):
        raise Refusal("hosted emitter requires exact workflow run and attempt identities")
    if _git_head() != commit_sha:
        raise Refusal("checked-out source SHA differs from the hosted workflow SHA")
    private = _private_dir(f"hosted-{job}-") / "original-run"
    _build_actual_bundle(
        private, job=job, commit_sha=commit_sha, run_id=run_id, run_attempt=run_attempt,
    )
    verify_evidence(
        private, expected_sha=commit_sha, expected_job=job,
        expected_run=run_id, expected_attempt=run_attempt,
    )
    public = PUBLIC_ROOT / job / "rdp-vi" / job
    if public.exists() or public.is_symlink():
        raise Refusal("public evidence destination already exists")
    public.mkdir(parents=True, mode=0o700)
    for name in ("manifest.json", "inventory.json", "observation.json", "raw-export.json"):
        shutil.copyfile(private / name, public / name)
    for path in public.iterdir():
        path.chmod(0o600)
    print(f"qualify-rdp-vi: emitted authored raw evidence for {job} ({commit_sha})")


def _scan_pii(public_root: Path = PUBLIC_ROOT) -> None:
    if public_root.is_symlink() or not public_root.is_dir():
        raise Refusal("combined public evidence directory is absent or unsafe")
    stage = ROOT / ".t64-rdp-vi-evidence-pii"
    if stage.exists() or stage.is_symlink():
        raise Refusal("stale or unsafe PII staging directory exists")
    stage.mkdir(mode=0o700)
    copied = 0
    try:
        for job in ("gate-linux", "gate-macos"):
            source = public_root / job
            if not source.exists():
                continue
            if source.is_symlink() or not source.is_dir():
                raise Refusal(f"combined {job} upload directory is unsafe")
            boot = _exact_keys(_read_json(source / f"{job}.json"), {
                "schema", "schema_version", "commit_sha", "run_id", "run_attempt", "job_name", "result",
            })
            sha, run, attempt = boot.get("commit_sha"), boot.get("run_id"), boot.get("run_attempt")
            if boot.get("schema") != "t64.boot-evidence" or boot.get("schema_version") != 1 or \
                    boot.get("job_name") != job or not isinstance(sha, str) or \
                    not SHA1_RE.fullmatch(sha) or not isinstance(run, str) or \
                    not DECIMAL_RE.fullmatch(run) or not isinstance(attempt, str) or \
                    not DECIMAL_RE.fullmatch(attempt):
                raise Refusal(f"combined {job} upload has an invalid boot identity")
            verify_upload(
                source, expected_sha=sha, expected_job=job,
                expected_run=run, expected_attempt=attempt,
            )
            destination = stage / job
            shutil.copytree(source, destination)
            verify_upload(
                destination, expected_sha=sha, expected_job=job,
                expected_run=run, expected_attempt=attempt,
            )
            copied += 1
        if copied == 0:
            raise Refusal("no combined upload directories were available for PII scanning")
        result = subprocess.run(["sh", "scripts/pii-scan.sh", "--tree"],
                                cwd=ROOT, check=False)
        if result.returncode != 0:
            raise Refusal("combined upload files failed the repository PII scan")
    finally:
        shutil.rmtree(stage, ignore_errors=True)
    print(f"qualify-rdp-vi: {copied} exact combined upload(s) passed the PII scan")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--selftest", action="store_true")
    group.add_argument("--deadline-selftest", action="store_true")
    group.add_argument("--native", action="store_true")
    group.add_argument("--emit-hosted", action="store_true")
    group.add_argument("--scan-pii", nargs="?", const=PUBLIC_ROOT, type=Path, metavar="COMBINED_ROOT")
    group.add_argument("--collect-hosted", action="store_true")
    group.add_argument("--verify-evidence", type=Path, metavar="DIR")
    group.add_argument("--verify-upload", type=Path, metavar="DIR")
    group.add_argument("--extract-upload", type=Path, metavar="ZIP")
    group.add_argument("--emit-layout-selftest", type=Path, metavar="DIR")
    group.add_argument("--package-upload", type=Path, metavar="DIR")
    group.add_argument("--mutate-upload-archive", type=Path, metavar="ZIP")
    parser.add_argument("--job", choices=("gate-linux", "gate-macos"))
    parser.add_argument("--repo", default="szTheory/tendies64")
    parser.add_argument("--sha")
    parser.add_argument("--run")
    parser.add_argument("--attempt")
    parser.add_argument("--out", type=Path)
    parser.add_argument("--mutation", choices=(
        "extra", "duplicate", "traversal", "raw-data", "raw-resealed",
    ))
    args = parser.parse_args()
    try:
        if args.deadline_selftest:
            passed, message = _deadline_control()
            if not passed:
                raise Refusal(message)
            print("qualify-rdp-vi: " + message)
            return 0
        if args.selftest:
            passed, message = _selftest()
            if not passed:
                raise Refusal("qualification selftest did not pass")
            print("qualify-rdp-vi: " + message)
            return 0
        if args.verify_evidence is not None:
            verify_evidence(
                args.verify_evidence.absolute(), expected_sha=args.sha,
                expected_job=args.job, expected_run=args.run, expected_attempt=args.attempt,
            )
            print("qualify-rdp-vi: original evidence records and raw bytes verified")
            return 0
        if args.verify_upload is not None:
            if not all((args.sha, args.job, args.run, args.attempt)):
                raise Refusal("combined upload verification requires SHA, job, run, and attempt")
            verify_upload(
                args.verify_upload.absolute(), expected_sha=args.sha, expected_job=args.job,
                expected_run=args.run, expected_attempt=args.attempt,
            )
            print(f"qualify-rdp-vi: exact combined upload members verified for {args.job}")
            return 0
        if args.extract_upload is not None:
            if args.job is None or args.out is None:
                raise Refusal("archive extraction requires job and fresh output directory")
            archive_bytes = _read_file(args.extract_upload, MAX_UPLOAD_BYTES)
            _safe_extract_upload(archive_bytes, args.out.absolute(), args.job)
            print(f"qualify-rdp-vi: exact upload archive members extracted for {args.job}")
            return 0
        if args.emit_layout_selftest is not None:
            if not all((args.job, args.sha, args.run, args.attempt)):
                raise Refusal("layout selftest requires SHA, job, run, and attempt")
            _emit_layout_selftest(
                args.emit_layout_selftest.absolute(), job=args.job, commit_sha=args.sha,
                run_id=args.run, run_attempt=args.attempt,
            )
            print(f"qualify-rdp-vi: actual raw producer files emitted for {args.job} layout control")
            return 0
        if args.package_upload is not None:
            if not all((args.job, args.sha, args.run, args.attempt)) or args.out is None:
                raise Refusal("upload packaging requires SHA, job, run, attempt, and ZIP output")
            _package_upload(
                args.package_upload.absolute(), args.out.absolute(), job=args.job,
                commit_sha=args.sha, run_id=args.run, run_attempt=args.attempt,
            )
            print(f"qualify-rdp-vi: combined {args.job} artifact packaged with upload-root paths")
            return 0
        if args.mutate_upload_archive is not None:
            if args.job is None or args.out is None or args.mutation is None:
                raise Refusal("upload archive mutation requires job, mutation, and fresh ZIP output")
            _mutate_upload_archive(
                args.mutate_upload_archive.absolute(), args.out.absolute(), args.job, args.mutation,
            )
            print(f"qualify-rdp-vi: created {args.mutation} archive control for {args.job}")
            return 0
        if args.emit_hosted:
            job = args.job or os.environ.get("GITHUB_JOB")
            sha = args.sha or os.environ.get("GITHUB_SHA")
            if not isinstance(job, str) or not isinstance(sha, str):
                raise Refusal("hosted emission requires job and source SHA")
            _emit_hosted(job, sha)
            return 0
        if args.scan_pii is not None:
            _scan_pii(args.scan_pii.absolute())
            return 0
        if args.native:
            evidence, source_sha = _run_native()
            print(f"qualify-rdp-vi: full normal, ASan/UBSan, and qualified GCC 13 runs passed for {source_sha}")
            print(f"qualify-rdp-vi: private owning command, process, LastTest, and JUnit records: {evidence}")
            return 0
        if args.collect_hosted:
            if not all((args.sha, args.run, args.attempt)) or args.out is None:
                raise Refusal("hosted collection requires SHA, run, attempt, and fresh output directory")
            output, evidence = _collect_hosted(
                args.repo, args.sha, args.run, args.attempt, args.out.absolute(),
            )
            print(f"qualify-rdp-vi: exact combined boot/raw artifact pair collected to {output}")
            print(f"qualify-rdp-vi: private collection records retained at {evidence}")
            return 0
    except (OSError, Refusal, KeyError, TypeError, ValueError, AssertionError) as error:
        print(f"qualify-rdp-vi: refused: {error}", file=sys.stderr)
        return 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
