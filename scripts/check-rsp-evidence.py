#!/usr/bin/env python3
"""Check the final public RSP evidence receipt without exposing private results."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import selectors
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
PUBLIC_RECEIPT = pathlib.Path("docs/evidence/phase05-final-rsp-evidence.json")
PRIVATE_ROOT = pathlib.Path("build/private-rsp-diagnostic")
PROFILE_SOURCE = pathlib.Path("scripts/run-rsp-diagnostic.py")
PROFILE_ID_RE = re.compile(r'^PROFILE_ID\s*=\s*"([a-z0-9-]+)"\s*$', re.M)
HEX40 = re.compile(r"[0-9a-f]{40}\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
BUILD_ID = re.compile(r"t64-build-v1-[0-9a-f]{64}\Z")
MAX_RECEIPT = 2 * 1024 * 1024
MAX_PRIVATE_JSON = 16 * 1024 * 1024
MAX_LOG = 32 * 1024 * 1024
MAX_ARTIFACT = 512 * 1024 * 1024
MAX_SOURCE_TOTAL = 128 * 1024 * 1024
MAX_PRIVATE_ARTIFACT = 32 * 1024 * 1024
MAX_PRIVATE_ARTIFACT_TOTAL = 256 * 1024 * 1024
MAX_PRIVATE_MEMBERS = 100_000
FIXED_BUILD_BINARIES = {
    "native": "build/gate/runner/t64-rsp-diagnostic",
    "asan": "build/asan/runner/t64-rsp-diagnostic",
}
NATIVE_TEST_BLOCKS = (
    "BASELINE_CORE", "BASELINE_CONTROLS", "PHASE02_TESTS",
    "PHASE03_TESTS", "PHASE04_TESTS", "PHASE05_TESTS",
)

REQUIREMENT_OWNERS = {
    "RSP-01": ["unit.core.rsp-tracer", "unit.core.rsp-control", "unit.core.rsp-scalar",
               "unit.core.rsp-transfers", "unit.core.rsp-vector", "unit.core.rsp-multiply",
               "unit.core.rsp-compare", "unit.core.rsp-divider"],
    "RSP-02": ["unit.core.rsp-vector", "unit.core.rsp-multiply", "unit.core.rsp-compare",
               "unit.core.rsp-divider", "unit.core.rsp-fuzz"],
    "RSP-03": ["unit.core.rsp-control", "unit.core.rsp-transfers", "unit.core.sp-dma"],
    "RSP-04": ["harness.rsp-qualification", "harness.rsp-build", "harness.rsp-run",
               "integration.rsp-rom-smoke"],
    "RSP-05": ["repo.rsp-no-hle"],
}
OPCODE_OWNERS = {
    "scalar-control": ["unit.core.rsp-control", "unit.core.rsp-scalar"],
    "scalar-transfers": ["unit.core.rsp-scalar", "unit.core.rsp-transfers"],
    "vector-arithmetic": ["unit.core.rsp-vector"],
    "vector-multiply": ["unit.core.rsp-multiply"],
    "vector-compare-merge": ["unit.core.rsp-compare"],
    "vector-divider": ["unit.core.rsp-divider"],
}
OPCODE_HEADINGS = (
    "## Scalar and control ledger",
    "## COP2 scalar transfers and vector memory ledger",
    "## Vector arithmetic and logical ledger",
    "## Vector multiply and accumulator ledger",
    "## Vector comparison, clipping, merge, and no-op ledger",
)

RECEIPT_KEYS = {
    "schema", "source_revision", "content_sha256", "verification_sha256", "profile_id",
    "opcode_ledger_sha256", "requirement_owners", "opcode_owners", "threat_owners",
    "builds", "native_gate", "asan_gate", "hosted", "composition", "reports",
    "differential_claim",
}
PRIVATE_KEYS = {
    "schema", "run_source_revision", "profile_id", "private_build_id",
    "inventory_path", "inventory_sha256", "projection_path", "projection_sha256",
    "partition", "audit_path", "audit_sha256", "c_outputs", "red_evidence",
}
C_OUTPUT_KEYS = {
    "inventory_stdout_path", "inventory_stdout_sha256",
    "inventory_stderr_path", "inventory_stderr_sha256",
    "complete_stdout_path", "complete_stdout_sha256",
    "complete_stderr_path", "complete_stderr_sha256",
}
PARTITION_KEYS = {
    "expected_sha256", "classified_sha256", "expected_rows", "classified_rows",
    "missing_rows", "duplicate_rows", "extra_rows", "unclassified_rows", "dropped_refusal_rows",
    "refusal_rows", "reasoned_refusal_rows", "nonpass_rows", "reasoned_nonpass_rows",
}
INVENTORY_KEYS = {"component", "declared_counts", "identity", "registrations", "schema", "schema_version"}
INVENTORY_IDENTITY_KEYS = {
    "boot_sha256", "closure_sha256", "core_build_sha256", "core_source_revision",
    "overlay_sha256", "profile_descriptor_sha256", "profile_id",
    "qualification_profile_sha256", "qualification_sha256", "rom_sha256",
    "source_projection_sha256", "source_revision",
}
INVENTORY_REGISTRATION_KEYS = {
    "constructor", "execution", "group", "index", "lane", "level", "name",
    "source_file", "source_file_sha256", "source_name", "source_value_count",
    "values", "values_method_sha256",
}
INVENTORY_VALUE_KEYS = {"id", "index", "selected_rom_sha256", "selector"}
INVENTORY_COUNT_KEYS = {"registrations", "required_values", "values"}
SOURCE_PROJECTION_KEYS = {
    "component", "declared_counts", "default_feature_closure", "inputs",
    "preuse_closure", "registrations", "schema", "schema_version",
    "selection", "source_revision", "source_root", "status",
}
SOURCE_PROJECTION_REGISTRATION_KEYS = INVENTORY_REGISTRATION_KEYS - {"values"} | {"effective_value_count", "values"}
SOURCE_PROJECTION_VALUE_KEYS = {"id", "index", "selector"}
OUTCOME_AUDIT_KEYS = {
    "aggregate", "authenticated_batches", "classification", "current_run_path",
    "expected_members", "explicit_verification_dir", "explicit_verify_complete",
    "inventory_sha256", "member_results", "member_set_exact", "outcomes", "schema",
}
OUTCOME_MEMBER_KEYS = {
    "attempt_receipt_path", "attempt_receipt_sha256", "batch_path", "batch_sha256",
    "capture_limit", "child_wall_seconds", "classification", "identity_sha256",
    "outer_wall_seconds", "owner", "reason", "registration_index", "selector",
    "stderr_path", "stderr_sha256", "stdout_path", "stdout_sha256", "step_ticks",
    "terminal_instruction_pair_retired", "tick_limit", "value_index",
}
ATTEMPT_KEYS = {
    "adapter_argv", "adapter_exit", "attempted_batch_path", "batch_path",
    "batch_sha256", "capture_bytes", "capture_limit", "child_wall_seconds",
    "cleanup_group_empty", "cleanup_signal_denied", "classification",
    "outer_deadline_expired", "outer_wall_seconds", "output_overflow",
    "registration_index", "rejected_batch_path", "selector", "started_utc",
    "step_ticks", "tick_limit", "value_index",
}
OUTCOME_CLASSES = {"pass", "guest-fail", "refused"}
RED_KEYS = {"kind", "setup", "path", "sha256", "fetched_opcode", "pc", "expected", "actual"}


class Refusal(ValueError):
    """Generic public-safe evidence refusal."""


def refuse() -> None:
    raise Refusal("evidence is missing or inconsistent")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_json(value) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"),
                        ensure_ascii=False) + "\n").encode("utf-8")


def parse_json(data: bytes) -> dict:
    def no_duplicates(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                refuse()
            result[key] = value
        return result
    try:
        value = json.loads(data, object_pairs_hook=no_duplicates)
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError):
        refuse()
    if not isinstance(value, dict):
        refuse()
    return value


def exact_object(value, keys: set[str]) -> dict:
    if not isinstance(value, dict) or set(value) != keys:
        refuse()
    return value


def bounded_fixed_run(root: pathlib.Path, relative_executable: str,
                      arguments: list[str], *, timeout_seconds: float = 5.0,
                      output_limit: int = 65536) -> tuple[int, bytes, bytes]:
    """Run a fixed project-owned metadata/validation command with bounded output."""
    executable = root / relative_executable
    if executable.is_symlink() or not executable.is_file() or not executable.stat().st_mode & 0o111:
        refuse()
    cursor = root
    for part in pathlib.PurePosixPath(relative_executable).parts:
        cursor = cursor / part
        if cursor.is_symlink():
            refuse()
    stdout = bytearray()
    stderr = bytearray()
    try:
        process = subprocess.Popen(
            [str(executable), *arguments], cwd=root, stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env={"PATH": "/usr/bin:/bin", "LC_ALL": "C", "LANG": "C"},
        )
        assert process.stdout is not None and process.stderr is not None
        streams = {process.stdout: stdout, process.stderr: stderr}
        selector = selectors.DefaultSelector()
        for stream in streams:
            os.set_blocking(stream.fileno(), False)
            selector.register(stream, selectors.EVENT_READ)
        deadline = time.monotonic() + timeout_seconds
        while selector.get_map() or process.poll() is None:
            if time.monotonic() >= deadline:
                process.kill()
                process.wait(timeout=1)
                refuse()
            for key, _ in selector.select(0.05):
                try:
                    block = os.read(key.fd, 4096)
                except BlockingIOError:
                    continue
                if not block:
                    selector.unregister(key.fileobj)
                    continue
                target = streams[key.fileobj]
                if len(stdout) + len(stderr) + len(block) > output_limit:
                    process.kill()
                    process.wait(timeout=1)
                    refuse()
                target.extend(block)
        return process.wait(timeout=1), bytes(stdout), bytes(stderr)
    except (OSError, subprocess.SubprocessError):
        refuse()


def runtime_identity(root: pathlib.Path, binary_relative: str) -> dict:
    status, stdout, stderr = bounded_fixed_run(
        root, binary_relative, ["--runtime-identity-json"], output_limit=16384,
    )
    if status != 0 or stderr:
        refuse()
    identity = parse_json(stdout)
    exact_object(identity, {
        "core_build_sha256", "core_source_revision",
        "profile_descriptor_sha256", "profile_id",
    })
    if (not isinstance(identity["core_build_sha256"], str)
            or not HEX64.fullmatch(identity["core_build_sha256"])
            or not isinstance(identity["core_source_revision"], str)
            or not HEX40.fullmatch(identity["core_source_revision"])
            or not isinstance(identity["profile_descriptor_sha256"], str)
            or not HEX64.fullmatch(identity["profile_descriptor_sha256"])
            or not isinstance(identity["profile_id"], str)
            or not re.fullmatch(r"[a-z0-9-]{1,96}", identity["profile_id"])):
        refuse()
    return identity


def _projection_member_keys(projection: dict) -> frozenset[tuple[int, int, str]]:
    registrations = projection.get("registrations")
    if not isinstance(registrations, list) or not registrations or len(registrations) > MAX_PRIVATE_MEMBERS:
        refuse()
    keys: set[tuple[int, int, str]] = set()
    for registration in registrations:
        if not isinstance(registration, dict):
            refuse()
        registration_index = registration.get("index")
        values = registration.get("values")
        if (type(registration_index) is not int or not 0 <= registration_index <= 0xffff
                or not isinstance(values, list) or not values):
            refuse()
        for value in values:
            if not isinstance(value, dict):
                refuse()
            value_index, selector = value.get("index"), value.get("selector")
            if (type(value_index) is not int or not 0 <= value_index <= 0xffff
                    or not isinstance(selector, str) or len(selector) > 32):
                refuse()
            key = (registration_index, value_index, selector)
            if key in keys:
                refuse()
            keys.add(key)
            if len(keys) > MAX_PRIVATE_MEMBERS:
                refuse()
    return frozenset(keys)


def bounded_source_closure_run(project_root: pathlib.Path) -> tuple[int, bytes, bytes]:
    script = project_root / "scripts/verify-rsp-diagnostic-qualification.py"
    if script.is_symlink() or not script.is_file():
        refuse()
    checked_read(script, MAX_PRIVATE_JSON, root=project_root)
    stdout = bytearray()
    stderr = bytearray()
    try:
        process = subprocess.Popen(
            [sys.executable, "-B", str(script), "--verify-source-closure",
             "build/private-rsp-diagnostic/qualification.json"],
            cwd=project_root, env={"PATH": "/usr/bin:/bin", "LC_ALL": "C", "LANG": "C",
                                  "PYTHONDONTWRITEBYTECODE": "1"},
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            start_new_session=True,
        )
        assert process.stdout is not None and process.stderr is not None
        streams = {process.stdout: stdout, process.stderr: stderr}
        selector = selectors.DefaultSelector()
        for stream in streams:
            os.set_blocking(stream.fileno(), False)
            selector.register(stream, selectors.EVENT_READ)
        deadline = time.monotonic() + 30
        while selector.get_map() or process.poll() is None:
            if time.monotonic() >= deadline:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=1)
                refuse()
            for key, _ in selector.select(0.05):
                try:
                    block = os.read(key.fd, 4096)
                except BlockingIOError:
                    continue
                if not block:
                    selector.unregister(key.fileobj)
                    continue
                target = streams[key.fileobj]
                if len(stdout) + len(stderr) + len(block) > 65536:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=1)
                    refuse()
                target.extend(block)
        return process.wait(timeout=1), bytes(stdout), bytes(stderr)
    except (OSError, subprocess.SubprocessError):
        refuse()


def verified_source_membership(project_root: pathlib.Path) -> tuple[str, frozenset[tuple[int, int, str]]]:
    """Run the existing pinned-source closure verifier, then read its verified projection."""
    status, stdout, stderr = bounded_source_closure_run(project_root)
    expected_stdout = (
        b"PASS: independent RSP source, assets, tools, and locks verified; "
        b"no identity executable was invoked\n"
    )
    if status != 0 or stdout != expected_stdout or stderr:
        refuse()

    qualification_path = project_root / PRIVATE_ROOT / "qualification.json"
    qualification_bytes = checked_read(qualification_path, MAX_PRIVATE_JSON, root=project_root)
    qualification = parse_json(qualification_bytes)
    closure_ref = exact_object(qualification.get("closure"), {"path", "sha256"})
    closure_path = safe_relative(project_root, closure_ref["path"], ("build/private-rsp-diagnostic/",))
    closure_bytes = checked_read(closure_path, MAX_PRIVATE_JSON, root=project_root)
    if not isinstance(closure_ref["sha256"], str) or sha256(closure_bytes) != closure_ref["sha256"]:
        refuse()
    closure = parse_json(closure_bytes)
    membership = closure.get("membership")
    if not isinstance(membership, dict) or membership.get("path") != (
            "build/private-rsp-diagnostic/rsp-source-projection.json"):
        refuse()
    projection_path = project_root / PRIVATE_ROOT / "rsp-source-projection.json"
    projection_bytes = checked_read(projection_path, MAX_PRIVATE_JSON, root=project_root)
    projection_sha = membership.get("sha256")
    if not isinstance(projection_sha, str) or not HEX64.fullmatch(projection_sha) or sha256(projection_bytes) != projection_sha:
        refuse()
    projection = parse_json(projection_bytes)
    return projection_sha, _projection_member_keys(projection)


def checked_read(path: pathlib.Path, maximum: int, *, root: pathlib.Path | None = None,
                 allow_empty: bool = False) -> bytes:
    if path.is_symlink() or not path.is_file():
        refuse()
    if root is not None:
        try:
            relative = path.absolute().relative_to(root.absolute())
            cursor = root
            for part in relative.parts:
                cursor = cursor / part
                if cursor.is_symlink():
                    refuse()
            path.resolve(strict=True).relative_to(root.resolve(strict=True))
        except (OSError, ValueError):
            refuse()
    before = path.stat()
    if (before.st_size <= 0 and not allow_empty) or before.st_size > maximum:
        refuse()
    try:
        data = path.read_bytes()
    except OSError:
        refuse()
    after = path.stat()
    if len(data) != before.st_size or after.st_size != before.st_size or after.st_mtime_ns != before.st_mtime_ns:
        refuse()
    return data


def safe_relative(root: pathlib.Path, value: object, prefixes: tuple[str, ...]) -> pathlib.Path:
    if not isinstance(value, str) or not value or "\\" in value:
        refuse()
    relative = pathlib.PurePosixPath(value)
    if relative.is_absolute() or relative.as_posix() != value or ".." in relative.parts:
        refuse()
    if not any(value.startswith(prefix) for prefix in prefixes):
        refuse()
    path = root.joinpath(*relative.parts)
    cursor = root
    for part in relative.parts:
        cursor = cursor / part
        if cursor.is_symlink():
            refuse()
    try:
        path.resolve(strict=True).relative_to(root.resolve(strict=True))
    except (OSError, ValueError):
        refuse()
    return path


def full_revision(value: object, git_root: pathlib.Path) -> str:
    if not isinstance(value, str) or not HEX40.fullmatch(value):
        refuse()
    try:
        result = subprocess.run(
            ["git", "-C", str(git_root), "rev-parse", "--verify", f"{value}^{{commit}}"],
            check=False, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=5,
        )
    except (OSError, subprocess.TimeoutExpired):
        refuse()
    if result.returncode != 0 or result.stdout.strip() != value:
        refuse()
    return value


def source_content_sha256(root: pathlib.Path) -> str:
    """Hash compiled implementation and build policy; SOURCE_REVISION stays separate."""
    paths: set[pathlib.Path] = set()
    for directory in ("core", "runner", "tools"):
        base = root / directory
        if base.is_symlink() or not base.is_dir():
            refuse()
        for path in base.rglob("*"):
            if path.is_symlink():
                refuse()
            if path.is_file() and (path.suffix in {".c", ".h", ".cmake", ".in", ".tmpl"}
                                   or path.name == "CMakeLists.txt"):
                paths.add(path)
    for relative in ("CMakeLists.txt", "CMakePresets.json"):
        path = root / relative
        if path.is_symlink() or not path.is_file():
            refuse()
        paths.add(path)
    return hash_paths(root, paths)


def verification_inputs_sha256(root: pathlib.Path) -> str:
    """Bind the checker/tests, RSP contract, authored fixtures, provenance and CI workflow."""
    paths: set[pathlib.Path] = set()
    scripts = root / "scripts"
    if scripts.is_symlink() or not scripts.is_dir():
        refuse()
    for path in scripts.rglob("*"):
        if path.is_symlink():
            refuse()
        if path.is_file():
            paths.add(path)
    for directory in ("fixtures/rsp", "docs/spec"):
        base = root / directory
        if base.is_symlink() or not base.is_dir():
            refuse()
        for path in base.rglob("*"):
            if path.is_symlink():
                refuse()
            if path.is_file() and (directory == "fixtures/rsp" or path.name.startswith("rsp-") or path.name == "result-protocol.md"):
                paths.add(path)
    fixed = (
        "fixtures/manifest.json",
        "provenance/copies.json", "LICENSES/ISC.txt",
        ".github/workflows/ci.yml", ".github/workflows/rsp-stress.yml",
    )
    for relative in fixed:
        path = root / relative
        if path.is_symlink() or not path.is_file():
            refuse()
        paths.add(path)
    return hash_paths(root, paths)


def hash_paths(root: pathlib.Path, paths: set[pathlib.Path]) -> str:
    total = 0
    digest = hashlib.sha256()
    for path in sorted(paths, key=lambda item: item.relative_to(root).as_posix()):
        relative = path.relative_to(root).as_posix().encode("utf-8")
        data = checked_read(path, MAX_ARTIFACT, root=root)
        total += len(data)
        if total > MAX_SOURCE_TOTAL:
            refuse()
        digest.update(len(relative).to_bytes(4, "big"))
        digest.update(relative)
        digest.update(len(data).to_bytes(8, "big"))
        digest.update(data)
    return digest.hexdigest()


def phase05_metadata_directory(root: pathlib.Path) -> pathlib.Path:
    """Locate the one approved phase contract across its lifecycle move."""
    root = root.resolve(strict=True)
    matches = []
    for relative in (
        ".planning/phases/05-rsp-low-level-emulation",
        ".planning/milestones/v1.0-phases/05-rsp-low-level-emulation",
    ):
        path = root
        for part in pathlib.PurePosixPath(relative).parts:
            path = path / part
            if path.is_symlink():
                refuse()
        if path.exists():
            if not path.is_dir():
                refuse()
            matches.append(path)
    if len(matches) != 1:
        refuse()
    return matches[0]


def owned_tables(root: pathlib.Path):
    root = root.resolve(strict=True)
    phase_directory = phase05_metadata_directory(root)
    validation = checked_read(phase_directory / "05-VALIDATION.md", 2_000_000, root=root).decode("utf-8")
    requirements = set(re.findall(r"(?m)^\|\s*(RSP-\d{2})\s*\|", validation))
    if requirements != set(REQUIREMENT_OWNERS):
        refuse()
    inventory = checked_read(root / "scripts/check-test-inventory.sh", 1_000_000, root=root).decode("utf-8")
    match = re.search(r"(?ms)^PHASE05_TESTS='(.*?)'", inventory)
    if match is None:
        refuse()
    phase_tests = set(match.group(1).split())
    inventory_tests = set()
    for _, block in re.findall(r"(?ms)^([A-Z][A-Z0-9_]*)='(.*?)'", inventory):
        inventory_tests.update(block.split())
    required = {name for names in REQUIREMENT_OWNERS.values() for name in names}
    required |= {name for names in OPCODE_OWNERS.values() for name in names}
    if not required.issubset(inventory_tests):
        refuse()
    ledger = checked_read(root / "docs/spec/rsp-instructions.md", 1_000_000, root=root).decode("utf-8")
    for heading in OPCODE_HEADINGS:
        if heading not in ledger:
            refuse()
        section = ledger.split(heading, 1)[1].split("\n## ", 1)[0]
        rows = [line for line in section.splitlines() if line.startswith("| ") and not re.match(r"\|\s*(?:---|Encoding|Function)\b", line)]
        if not rows:
            refuse()

    threat_severity: dict[str, str] = {}
    rank = {"low": 0, "medium": 1, "high": 2, "critical": 3}
    plans = phase_directory
    for path in sorted(plans.glob("05-*-PLAN.md")):
        data = checked_read(path, 2_000_000, root=root).decode("utf-8")
        block = re.search(r"(?s)<threat_model>(.*?)</threat_model>", data)
        if block is None:
            continue
        for line in block.group(1).splitlines():
            row = re.match(r"\s*\|\s*(T-05-(?:\d{2}|SC))\s*\|[^|]*\|[^|]*\|\s*(critical|high|medium|low)\s*\|", line, re.I)
            if row:
                threat, severity = row.group(1), row.group(2).lower()
                if threat not in threat_severity or rank[severity] > rank[threat_severity[threat]]:
                    threat_severity[threat] = severity
    if not threat_severity:
        refuse()
    return sha256(ledger.encode()), phase_tests, threat_severity


def native_gate_tests(root: pathlib.Path) -> set[str]:
    inventory = checked_read(root / "scripts/check-test-inventory.sh", 1_000_000, root=root).decode("utf-8")
    blocks = {name: values.split() for name, values in
              re.findall(r"(?ms)^([A-Z][A-Z0-9_]*)='(.*?)'", inventory)}
    if any(name not in blocks for name in NATIVE_TEST_BLOCKS):
        refuse()
    lists = [blocks[name] for name in NATIVE_TEST_BLOCKS]
    tests = [test for block in lists for test in block]
    if len(tests) != 127 or len(set(tests)) != 127:
        refuse()
    return set(tests)


def validate_ctest_log(text: str, required: set[str], *, allowed: set[str] | None = None) -> None:
    summaries = re.findall(r"(?m)^\s*100% tests passed(?:, 0 tests failed)?(?: out of ([0-9]+))?\s*$", text)
    if len(summaries) != 1:
        refuse()
    expected_total = int(summaries[0]) if summaries[0] else None
    rows: dict[str, list[str]] = {}
    for line in text.splitlines():
        match = re.match(
            r"^\s*\d+/\d+ Test\s+#\s*\d+: ([A-Za-z0-9_.-]+)\s+.*?\b(Passed|Failed|Timeout|Skipped|Disabled|Not Run)\b",
            line,
        )
        if match:
            rows.setdefault(match.group(1), []).append(match.group(2))
    allowed = required if allowed is None else allowed
    if (not required.issubset(rows) or not set(rows).issubset(allowed)
            or (expected_total is not None and expected_total != len(rows))
            or any(statuses != ["Passed"] for statuses in rows.values())):
        refuse()


def frontmatter(text: str) -> dict[str, str]:
    match = re.match(r"\A---\s*\n(.*?)\n---\s*(?:\n|\Z)", text, re.S)
    if match is None:
        refuse()
    output = {}
    for line in match.group(1).splitlines():
        item = re.match(r"^([a-z_]+):\s*(.*?)\s*$", line)
        if item:
            output[item.group(1)] = item.group(2).strip('"\'')
    return output


def privacy_scan(data: bytes, *, approved_rom_hash: str | None = None) -> None:
    rom_fields = list(re.finditer(
        rb"(?i)[\"']?(rom_sha256|selected_rom_sha256|rom_id|rom_hash|rom_identity(?:_sha256)?|rom_digest|rom_fingerprint)[\"']?\s*[:=]\s*[\"']?([0-9a-fA-F]{64})[\"']?",
        data,
    ))
    if rom_fields:
        if (approved_rom_hash is None or not HEX64.fullmatch(approved_rom_hash)
                or len(rom_fields) != 1 or rom_fields[0].group(1).lower() != b"rom_sha256"
                or rom_fields[0].group(2).decode("ascii").lower() != approved_rom_hash):
            refuse()
        start, end = rom_fields[0].span()
        data = data[:start] + b" " * (end - start) + data[end:]
    markers = (
        rb"build/private-(?:rsp|cpu)-diagnostic(?:/|\\)",
        rb"\br[0-9]{4}v[0-9]{5}\b",
        rb"(?i)[\"']?classification[\"']?\s*[:=]\s*[\"']?(?:guest-fail|refused|incomplete|timeout|harness-error)[\"']?",
        rb"(?i)[\"']?(?:private_build_id|inventory_sha256|classified_sha256|expected_sha256)[\"']?\s*[:=]\s*[\"']?[0-9a-f]{64}",
        rb"(?i)[\"']?source_name[\"']?\s*[:=]\s*[\"']?\S+",
        rb"(?i)\bprivate\s+(?:external\s+)?(?:member|case|result|count)s?\s*[:=]\s*[0-9]+",
        rb"(?i)[\"']?private_(?:external_)?(?:member|case|result|count)[a-z_]*[\"']?\s*[:=]\s*[0-9]+",
    )
    if any(re.search(marker, data) for marker in markers):
        refuse()


def _check_hosted_run(root: pathlib.Path, hosted: dict, revision: str,
                      content_sha256: str) -> None:
    if set(hosted) != {"run_id", "attempt", "source_revision", "source_sha",
                       "content_sha256", "platforms"}:
        refuse()
    if (hosted["source_revision"] != revision or hosted["source_sha"] != revision[:12]
            or hosted["content_sha256"] != content_sha256):
        refuse()
    if not isinstance(hosted["run_id"], str) or not re.fullmatch(r"[0-9]{6,20}", hosted["run_id"]):
        refuse()
    if not isinstance(hosted["attempt"], str) or not re.fullmatch(r"[1-9][0-9]{0,5}", hosted["attempt"]):
        refuse()
    log = checked_read(root / "docs/ci-reviews/LOG.md", 8_000_000, root=root).decode("utf-8")
    rows = [[field.strip() for field in line.strip().strip("|").split("|")]
            for line in log.splitlines() if line.startswith("|")]
    run = next((row for row in rows if len(row) >= 5 and row[1] == hosted["run_id"]), None)
    if run is None or run[2] != revision[:12] or run[3] != "push" or run[4] != "success":
        refuse()
    if not any(len(row) >= 3 and row[1] == hosted["run_id"] and "Both hosts pass" in row[2]
               for row in rows):
        refuse()
    platforms = exact_object(hosted["platforms"], {"gate-linux", "gate-macos"})
    common_result_profile = None
    for job, value in platforms.items():
        exact_object(value, {
            "record_path", "record_sha256", "artifact_id", "artifact_digest",
            "build_id", "result_profile_id", "result_profile_digest",
        })
        record_path = safe_relative(root, value["record_path"], ("build/gate/05-10-hosted/",))
        if record_path.name != f"{job}.json":
            refuse()
        record_bytes = checked_read(record_path, 65536, root=root)
        if not isinstance(value["record_sha256"], str) or sha256(record_bytes) != value["record_sha256"]:
            refuse()
        privacy_scan(record_bytes)
        if not isinstance(value["artifact_id"], str) or not re.fullmatch(r"[1-9][0-9]{0,19}", value["artifact_id"]):
            refuse()
        if not isinstance(value["artifact_digest"], str) or not re.fullmatch(r"sha256:[0-9a-f]{64}", value["artifact_digest"]):
            refuse()
        if not isinstance(value["build_id"], str) or not BUILD_ID.fullmatch(value["build_id"]):
            refuse()
        if not isinstance(value["result_profile_id"], str) or not re.fullmatch(r"[a-z0-9-]{1,96}", value["result_profile_id"]):
            refuse()
        if not isinstance(value["result_profile_digest"], str) or not HEX64.fullmatch(value["result_profile_digest"]):
            refuse()
        record = parse_json(record_bytes)
        exact_object(record, {"schema", "schema_version", "commit_sha", "run_id", "run_attempt", "job_name", "result"})
        if (record["schema"] != "t64.boot-evidence" or record["schema_version"] != 1
                or record["commit_sha"] != revision or record["run_id"] != hosted["run_id"]
                or record["run_attempt"] != hosted["attempt"] or record["job_name"] != job):
            refuse()
        result = record["result"]
        if not isinstance(result, dict):
            refuse()
        if (result.get("schema") != "t64.result" or result.get("schema_version") != 2
                or result.get("source_revision") != revision
                or result.get("build_id") != value["build_id"]
                or result.get("profile_id") != value["result_profile_id"]
                or result.get("profile_digest") != value["result_profile_digest"]
                or result.get("outcome") != "pass" or result.get("detail") != "positive-match"
                or result.get("fixture_id") != "hello-openipl3"):
            refuse()
        result_profile = (value["result_profile_id"], value["result_profile_digest"])
        if common_result_profile is not None and result_profile != common_result_profile:
            refuse()
        common_result_profile = result_profile


def safe_private_reference(private_root: pathlib.Path, value: object) -> pathlib.Path:
    if not isinstance(value, str) or not value or len(value) > 4096 or "\\" in value:
        refuse()
    relative = pathlib.PurePosixPath(value)
    if relative.is_absolute() or relative.as_posix() != value or any(part in {"", ".", ".."} for part in relative.parts):
        refuse()
    parts = relative.parts
    if parts[:2] == ("build", "private-rsp-diagnostic"):
        parts = parts[2:]
    elif parts and parts[0] == "private-rsp-diagnostic":
        parts = parts[1:]
    if not parts:
        refuse()
    path = private_root.joinpath(*parts)
    cursor = private_root
    for part in parts:
        cursor = cursor / part
        if cursor.is_symlink():
            refuse()
    try:
        path.resolve(strict=True).relative_to(private_root.resolve(strict=True))
    except (OSError, ValueError):
        refuse()
    return path


def private_argument_path(private_root: pathlib.Path, value: object) -> pathlib.Path:
    if not isinstance(value, str) or not value or len(value) > 4096 or "\\" in value:
        refuse()
    candidate = pathlib.Path(value)
    if not candidate.is_absolute():
        return safe_private_reference(private_root, value)
    try:
        relative = candidate.absolute().relative_to(private_root.absolute())
    except ValueError:
        refuse()
    cursor = private_root
    for part in relative.parts:
        cursor = cursor / part
        if cursor.is_symlink():
            refuse()
    try:
        candidate.resolve(strict=True).relative_to(private_root.resolve(strict=True))
    except (OSError, ValueError):
        refuse()
    return candidate


def parse_attempt_stdout(data: bytes, classification: str) -> dict:
    if not data or len(data) > 4096:
        refuse()
    try:
        text = data.decode("ascii")
    except UnicodeDecodeError:
        refuse()
    terminal = re.fullmatch(
        r"classification=(pass|guest-fail) terminal_pc=([0-9a-f]{8}) terminal_opcode=([0-9a-f]{8}) "
        r"delay_slot_pc=([0-9a-f]{8}) delay_slot_opcode=([0-9a-f]{8}) tick=([0-9]+) "
        r"retired=([0-9]+) capture=([0-9]+)\n?", text,
    )
    if terminal:
        if terminal.group(1) != classification:
            refuse()
        return {
            "classification": terminal.group(1),
            "terminal_pc": int(terminal.group(2), 16),
            "terminal_opcode": int(terminal.group(3), 16),
            "delay_slot_pc": int(terminal.group(4), 16),
            "delay_slot_opcode": int(terminal.group(5), 16),
            "retired": int(terminal.group(7)),
        }
    refusal = re.fullmatch(
        r"classification=refused pc=[0-9a-f]{3,8} opcode=[0-9a-f]{8} "
        r"operation=([a-z0-9_-]+) reason=(refusal-code-[0-9]+) tick=[0-9]+ "
        r"retired=([0-9]+) capture=([0-9]+)\n?", text,
    )
    if refusal and classification == "refused":
        return {"classification": "refused", "operation": refusal.group(1),
                "reason": refusal.group(2), "retired": int(refusal.group(3))}
    refuse()


def _inventory_members(inventory: dict, source_revision: str,
                       profile_id: str, private_build_id: str,
                       git_root: pathlib.Path) -> dict[tuple[int, int, str], str]:
    exact_object(inventory, INVENTORY_KEYS)
    if inventory["schema"] != "t64.rsp.inventory" or inventory["schema_version"] != 1 or inventory["component"] != "rsp":
        refuse()
    identity = exact_object(inventory["identity"], INVENTORY_IDENTITY_KEYS)
    if (identity["source_revision"] != source_revision or identity["profile_id"] != profile_id
            or identity["core_build_sha256"] != private_build_id):
        refuse()
    if not isinstance(identity["source_revision"], str) or not HEX40.fullmatch(identity["source_revision"]):
        refuse()
    full_revision(identity["core_source_revision"], git_root)
    for key in INVENTORY_IDENTITY_KEYS - {"profile_id", "source_revision", "core_source_revision"}:
        if not isinstance(identity[key], str) or not HEX64.fullmatch(identity[key]):
            refuse()
    counts = exact_object(inventory["declared_counts"], INVENTORY_COUNT_KEYS)
    if any(type(value) is not int or value < 0 for value in counts.values()):
        refuse()
    registrations = inventory["registrations"]
    if not isinstance(registrations, list) or not registrations or len(registrations) > MAX_PRIVATE_MEMBERS:
        refuse()
    expected: dict[tuple[int, int, str], str] = {}
    registration_indices: set[int] = set()
    value_total = 0
    for registration in registrations:
        exact_object(registration, INVENTORY_REGISTRATION_KEYS)
        registration_index = registration["index"]
        source_value_count = registration["source_value_count"]
        values = registration["values"]
        if (type(registration_index) is not int or not 0 <= registration_index <= 0xffff
                or registration_index in registration_indices
                or type(source_value_count) is not int or source_value_count < 0
                or not isinstance(values, list) or not values):
            refuse()
        registration_indices.add(registration_index)
        if not isinstance(registration["source_file_sha256"], str) or not HEX64.fullmatch(registration["source_file_sha256"]):
            refuse()
        if not isinstance(registration["values_method_sha256"], str) or not HEX64.fullmatch(registration["values_method_sha256"]):
            refuse()
        for value_index, value in enumerate(values):
            exact_object(value, INVENTORY_VALUE_KEYS)
            selector = value["selector"]
            expected_id = "once" if source_value_count == 0 else f"v{value_index:05d}"
            if (type(value["index"]) is not int or value["index"] != value_index
                    or value_index > 0xffff or selector != f"r{registration_index:04d}v{value_index:05d}"
                    or value["id"] != expected_id
                    or not isinstance(value["selected_rom_sha256"], str)
                    or not HEX64.fullmatch(value["selected_rom_sha256"])):
                refuse()
            expected[(registration_index, value_index, selector)] = value["selected_rom_sha256"]
            value_total += 1
            if value_total > MAX_PRIVATE_MEMBERS:
                refuse()
    if (counts["registrations"] != len(registrations) or counts["values"] != value_total
            or counts["required_values"] != value_total or len(expected) != value_total):
        refuse()
    return expected


def _qualified_members(projection: dict, inventory: dict,
                       source_revision: str, profile_id: str,
                       private_build_id: str, git_root: pathlib.Path,
                       qualified_source_keys: frozenset[tuple[int, int, str]] | None = None
                       ) -> dict[tuple[int, int, str], str]:
    inventory_members = _inventory_members(inventory, source_revision,
                                           profile_id, private_build_id, git_root)
    exact_object(projection, SOURCE_PROJECTION_KEYS)
    if (projection["schema"] != "t64.rsp-source-projection"
            or projection["schema_version"] != 1 or projection["component"] != "rsp"
            or projection["source_revision"] != source_revision
            or not isinstance(projection["status"], str) or not projection["status"]):
        refuse()
    projection_counts = exact_object(projection["declared_counts"], {"registrations", "values"})
    if any(type(value) is not int or value < 0 for value in projection_counts.values()):
        refuse()
    projection_rows = projection["registrations"]
    inventory_rows = inventory["registrations"]
    if not isinstance(projection_rows, list) or len(projection_rows) != len(inventory_rows):
        refuse()
    expected: dict[tuple[int, int, str], str] = {}
    value_total = 0
    registration_fields = INVENTORY_REGISTRATION_KEYS - {"values"}
    for source_row, inventory_row in zip(projection_rows, inventory_rows):
        exact_object(source_row, SOURCE_PROJECTION_REGISTRATION_KEYS)
        for field in registration_fields:
            if source_row[field] != inventory_row[field]:
                refuse()
        if type(source_row["effective_value_count"]) is not int or source_row["effective_value_count"] < 0:
            refuse()
        source_values, inventory_values = source_row["values"], inventory_row["values"]
        if not isinstance(source_values, list) or len(source_values) != len(inventory_values):
            refuse()
        for source_value, inventory_value in zip(source_values, inventory_values):
            exact_object(source_value, SOURCE_PROJECTION_VALUE_KEYS)
            if any(source_value[key] != inventory_value[key] for key in SOURCE_PROJECTION_VALUE_KEYS):
                refuse()
            key = (inventory_row["index"], inventory_value["index"], inventory_value["selector"])
            if key in expected or key not in inventory_members:
                refuse()
            expected[key] = inventory_value["selected_rom_sha256"]
            value_total += 1
            if value_total > MAX_PRIVATE_MEMBERS:
                refuse()
    if (projection_counts["registrations"] != len(projection_rows)
            or projection_counts["values"] != value_total
            or set(expected) != set(inventory_members)):
        refuse()
    if qualified_source_keys is not None and set(expected) != set(qualified_source_keys):
        refuse()
    return expected


def derive_private_partition(projection: dict, inventory: dict, audit: dict,
                             private_root: pathlib.Path,
                             source_revision: str,
                             profile_id: str,
                             private_build_id: str,
                             git_root: pathlib.Path,
                             project_root: pathlib.Path,
                             inventory_path: pathlib.Path) -> dict:
    expected_hashes = _qualified_members(projection, inventory, source_revision,
                                         profile_id, private_build_id, git_root)
    expected = set(expected_hashes)
    exact_object(audit, OUTCOME_AUDIT_KEYS)
    inventory_digest = sha256(canonical_json(inventory))
    exact_object(audit["outcomes"], {"guest-fail", "pass", "refused"})
    if (audit["schema"] != "t64.rsp-plan10-current-run-outcome-audit.v1"
            or audit["inventory_sha256"] != inventory_digest
            or type(audit["member_set_exact"]) is not bool
            or audit["member_set_exact"] is not True
            or type(audit["expected_members"]) is not int
            or audit["expected_members"] != len(expected)):
        refuse()
    members = audit["member_results"]
    if not isinstance(members, list) or len(members) > MAX_PRIVATE_MEMBERS:
        refuse()
    expected_outcomes = {"guest-fail": 0, "pass": 0, "refused": 0}
    observed: list[tuple[int, int, str]] = []
    classified_digest_rows = []
    duplicate_count = 0
    unclassified_count = 0
    refusal_count = 0
    reasoned_refusal_count = 0
    nonpass_count = 0
    reasoned_nonpass_count = 0
    batch_paths: set[pathlib.Path] = set()
    total_bytes = 0
    for row in members:
        exact_object(row, OUTCOME_MEMBER_KEYS)
        reg_index, value_index, selector = row["registration_index"], row["value_index"], row["selector"]
        if (type(reg_index) is not int or not 0 <= reg_index <= 0xffff
                or type(value_index) is not int or not 0 <= value_index <= 0xffff
                or not isinstance(selector, str) or len(selector) > 32):
            refuse()
        key = (reg_index, value_index, selector)
        observed.append(key)
        classification, owner, reason = row["classification"], row["owner"], row["reason"]
        if not isinstance(classification, str) or len(classification) > 64:
            refuse()
        if classification not in OUTCOME_CLASSES:
            unclassified_count += 1
        else:
            expected_outcomes[classification] += 1
        if not isinstance(owner, str) or not owner or len(owner) > 4096:
            refuse()
        if not isinstance(reason, str) or len(reason) > 4096:
            refuse()
        if classification == "refused":
            refusal_count += 1
            reasoned_refusal_count += int(bool(reason.strip()))
        if classification != "pass":
            nonpass_count += 1
            reasoned_nonpass_count += int(bool(reason.strip()))
        if type(row["terminal_instruction_pair_retired"]) is not bool:
            refuse()
        if classification == "pass" and row["terminal_instruction_pair_retired"] is not True:
            refuse()
        for key_name in ("identity_sha256", "attempt_receipt_sha256", "batch_sha256", "stdout_sha256", "stderr_sha256"):
            if not isinstance(row[key_name], str) or not HEX64.fullmatch(row[key_name]):
                refuse()
        artifact_bytes_by_key = {}
        artifact_paths_by_key = {}
        for key_name, digest_key, allow_empty in (
            ("attempt_receipt_path", "attempt_receipt_sha256", False),
            ("batch_path", "batch_sha256", False),
            ("stdout_path", "stdout_sha256", True),
            ("stderr_path", "stderr_sha256", True),
        ):
            path = safe_private_reference(private_root, row[key_name])
            file_bytes = checked_read(path, MAX_PRIVATE_ARTIFACT, root=private_root, allow_empty=allow_empty)
            total_bytes += len(file_bytes)
            if total_bytes > MAX_PRIVATE_ARTIFACT_TOTAL or sha256(file_bytes) != row[digest_key]:
                refuse()
            artifact_bytes_by_key[key_name] = file_bytes
            artifact_paths_by_key[key_name] = path
            if key_name == "batch_path":
                batch_paths.add(path.resolve(strict=True))
        attempt_bytes = artifact_bytes_by_key["attempt_receipt_path"]
        attempt = parse_json(attempt_bytes)
        exact_object(attempt, ATTEMPT_KEYS)
        attempt_batch = private_argument_path(private_root, attempt["batch_path"])
        attempted_batch = private_argument_path(private_root, attempt["attempted_batch_path"])
        batch_path = artifact_paths_by_key["batch_path"]
        if (attempt["classification"] != classification
                or attempt["registration_index"] != reg_index
                or attempt["value_index"] != value_index or attempt["selector"] != selector
                or attempt_batch != batch_path or attempted_batch != batch_path
                or attempt["batch_sha256"] != row["batch_sha256"]):
            refuse()
        argv = attempt["adapter_argv"]
        if not isinstance(argv, list) or len(argv) != 11:
            refuse()
        executable_path = pathlib.Path(argv[0]) if isinstance(argv[0], str) else None
        expected_executable = (project_root / FIXED_BUILD_BINARIES["native"]).resolve(strict=True)
        if executable_path is None or executable_path.resolve(strict=True) != expected_executable:
            refuse()
        argv_inventory = private_argument_path(private_root, argv[3])
        argv_rom = private_argument_path(private_root, argv[4])
        argv_batch = private_argument_path(private_root, argv[10])
        if (argv[1:3] != ["--run-local", "--local-only"]
                or argv_inventory != inventory_path
                or argv_rom.parent.resolve(strict=True) != inventory_path.parent.resolve(strict=True)
                or argv_rom.name != f"member-{selector}.z64"
                or not isinstance(argv[5], str) or not argv[5].isdigit()
                or not 0 < int(argv[5]) <= MAX_ARTIFACT
                or argv[6] != expected_hashes[key]
                or argv[7] != "10000000" or argv[8] != str(reg_index)
                or argv[9] != str(value_index) or argv_batch != batch_path):
            refuse()
        limits = {
            "capture_limit": 4096, "child_wall_seconds": 10,
            "outer_wall_seconds": 12, "step_ticks": 16, "tick_limit": 10_000_000,
        }
        for limit, expected_limit in limits.items():
            if type(row[limit]) is not int or row[limit] != expected_limit or attempt[limit] != expected_limit:
                refuse()
        if (type(attempt["capture_bytes"]) is not int or not 0 <= attempt["capture_bytes"] <= 4096
                or type(attempt["adapter_exit"]) is not int
                or not isinstance(attempt["cleanup_group_empty"], bool)
                or not isinstance(attempt["cleanup_signal_denied"], bool)
                or not isinstance(attempt["outer_deadline_expired"], bool)
                or not isinstance(attempt["output_overflow"], bool)):
            refuse()
        if classification == "pass" and (attempt["adapter_exit"] != 0 or attempt["outer_deadline_expired"] or attempt["output_overflow"]):
            refuse()
        # A redundant signal may be denied after natural process exit. The
        # owning wrapper's final empty-group proof, deadline and capture checks
        # determine cleanup success; a denied signal alone is not a live group.
        if (attempt["capture_bytes"] != len(artifact_bytes_by_key["stdout_path"])
                + len(artifact_bytes_by_key["stderr_path"])
                or not attempt["cleanup_group_empty"]
                or attempt["outer_deadline_expired"] or attempt["output_overflow"]):
            refuse()
        expected_exit = {"pass": 0, "guest-fail": 2, "refused": 2}.get(classification)
        if expected_exit is None or attempt["adapter_exit"] != expected_exit:
            refuse()
        output = parse_attempt_stdout(artifact_bytes_by_key["stdout_path"], classification)
        batch = parse_json(artifact_bytes_by_key["batch_path"])
        if not isinstance(batch, dict) or not isinstance(batch.get("identity"), dict):
            refuse()
        batch_identity = batch["identity"]
        batch_member = batch.get("member")
        batch_result = batch.get("result")
        if not isinstance(batch_member, dict) or not isinstance(batch_result, dict):
            refuse()
        if (batch_identity.get("inventory_sha256") != inventory_digest
                or batch_identity.get("source_revision") != source_revision
                or batch_identity.get("profile_id") != profile_id
                or batch_identity.get("core_build_sha256") != private_build_id
                or batch_member.get("registration_index") != reg_index
                or batch_member.get("value_index") != value_index
                or batch_member.get("selector") != selector
                or batch_member.get("selected_rom_sha256") != expected_hashes[key]
                or batch_result.get("outcome") != classification
                or batch_result.get("reason") != reason
                or batch_result.get("terminal_delay_slot_retired") != row["terminal_instruction_pair_retired"]):
            refuse()
        if classification in {"pass", "guest-fail"}:
            if (output["terminal_pc"] != batch_result.get("terminal_pc")
                    or output["terminal_opcode"] != batch_result.get("terminal_opcode")
                    or output["delay_slot_pc"] != batch_result.get("terminal_delay_slot_pc")
                    or output["delay_slot_opcode"] != batch_result.get("terminal_delay_slot_opcode")
                    or output["retired"] != batch_result.get("retired_instructions")):
                refuse()
        elif (output["operation"] != batch_result.get("operation")
              or output["reason"] != batch_result.get("reason")):
            refuse()
        classified_digest_rows.append({
            "key": [reg_index, value_index, selector], "classification": classification,
            "owner": owner, "reason": reason,
            "identity_sha256": row["identity_sha256"],
            "attempt_receipt_sha256": row["attempt_receipt_sha256"],
            "batch_sha256": row["batch_sha256"], "stdout_sha256": row["stdout_sha256"],
            "stderr_sha256": row["stderr_sha256"],
            "terminal_instruction_pair_retired": row["terminal_instruction_pair_retired"],
        })
    observed_set = set(observed)
    duplicate_count += len(observed) - len(observed_set)
    expected_sorted = sorted(expected)
    classified_sorted = sorted(classified_digest_rows, key=lambda item: tuple(item["key"]))
    expected_sha = sha256(canonical_json([
        {"registration_index": row[0], "value_index": row[1], "selector": row[2]}
        for row in expected_sorted
    ]))
    missing_count = len(expected - observed_set)
    extra_count = len(observed_set - expected)
    if audit["outcomes"] != expected_outcomes:
        refuse()
    if type(audit["authenticated_batches"]) is not int or audit["authenticated_batches"] != len(batch_paths):
        refuse()
    return {
        "expected_sha256": expected_sha,
        "classified_sha256": sha256(canonical_json(classified_sorted)),
        "expected_rows": len(expected), "classified_rows": len(members),
        "missing_rows": missing_count, "duplicate_rows": duplicate_count,
        "extra_rows": extra_count, "unclassified_rows": unclassified_count,
        "dropped_refusal_rows": missing_count,
        "refusal_rows": refusal_count, "reasoned_refusal_rows": reasoned_refusal_count,
        "nonpass_rows": nonpass_count, "reasoned_nonpass_rows": reasoned_nonpass_count,
    }


def _check_private_c_outputs(private_root: pathlib.Path, evidence_root: pathlib.Path,
                             sidecar: dict, inventory: dict, inventory_bytes: bytes,
                             audit: dict, git_root: pathlib.Path,
                             qualified_source_anchor: tuple[str, frozenset[tuple[int, int, str]]] | None
                             = None) -> tuple[pathlib.Path, pathlib.Path, dict, frozenset[tuple[int, int, str]]]:
    if qualified_source_anchor is None:
        qualified_source_anchor = verified_source_membership(git_root)
    if (not isinstance(qualified_source_anchor, tuple) or len(qualified_source_anchor) != 2
            or not isinstance(qualified_source_anchor[0], str)
            or not HEX64.fullmatch(qualified_source_anchor[0])
            or not isinstance(qualified_source_anchor[1], frozenset)):
        refuse()
    qualified_projection_sha, qualified_source_keys = qualified_source_anchor
    c_outputs = exact_object(sidecar["c_outputs"], C_OUTPUT_KEYS)
    run_dir = safe_private_reference(private_root, audit["current_run_path"])
    verify_dir = safe_private_reference(private_root, audit["explicit_verification_dir"])
    if not run_dir.is_dir() or not verify_dir.is_dir():
        refuse()
    inventory_path = safe_private_reference(private_root, sidecar["inventory_path"])
    expected_inventory_path = run_dir / "rsp-inventory.json"
    if inventory_path.resolve(strict=True) != expected_inventory_path.resolve(strict=True):
        refuse()
    batches_dir = run_dir / "batches"
    if (batches_dir.is_symlink() or not batches_dir.is_dir()
            or stat.S_IMODE(batches_dir.stat().st_mode) != 0o700):
        refuse()
    expected_output_paths = {
        "inventory_stdout_path": run_dir / "inventory-validation.stdout.log",
        "inventory_stderr_path": run_dir / "inventory-validation.stderr.log",
        "complete_stdout_path": verify_dir / "complete-verification.stdout.log",
        "complete_stderr_path": verify_dir / "complete-verification.stderr.log",
    }
    output_bytes = {}
    for path_key, expected_path in expected_output_paths.items():
        referenced = safe_private_reference(private_root, c_outputs[path_key])
        if referenced.resolve(strict=True) != expected_path.resolve(strict=True):
            refuse()
        hash_key = path_key.replace("_path", "_sha256")
        digest_value = c_outputs[hash_key]
        if not isinstance(digest_value, str) or not HEX64.fullmatch(digest_value):
            refuse()
        data = checked_read(referenced, MAX_LOG, root=private_root,
                            allow_empty=path_key.endswith("stderr_path"))
        if sha256(data) != digest_value:
            refuse()
        output_bytes[path_key] = data

    source_projection_digest = sidecar["projection_sha256"]
    projection_path = private_root / "rsp-source-projection.json"
    projection_bytes = checked_read(projection_path, MAX_PRIVATE_JSON, root=private_root)
    if (not isinstance(source_projection_digest, str)
            or sha256(projection_bytes) != source_projection_digest
            or source_projection_digest != qualified_projection_sha
            or inventory["identity"]["source_projection_sha256"] != source_projection_digest
            or canonical_json(inventory) != inventory_bytes
            or canonical_json(parse_json(projection_bytes)) != projection_bytes):
        refuse()
    projection = parse_json(projection_bytes)
    expected_hashes = _qualified_members(
        projection, inventory, sidecar["run_source_revision"], sidecar["profile_id"],
        sidecar["private_build_id"], git_root, qualified_source_keys,
    )
    inventory_output = output_bytes["inventory_stdout_path"]
    validation_match = re.fullmatch(
        rb"classification=valid registrations=([0-9]+) required_values=([0-9]+) inventory_sha256=([0-9a-f]{64})\n?",
        inventory_output,
    )
    if validation_match is None or output_bytes["inventory_stderr_path"]:
        refuse()
    if (int(validation_match.group(1)) != len(projection["registrations"])
            or int(validation_match.group(2)) != len(expected_hashes)
            or validation_match.group(3).decode("ascii") != sha256(inventory_bytes)):
        refuse()

    inventory_status, inventory_stdout, inventory_stderr = bounded_fixed_run(
        evidence_root, FIXED_BUILD_BINARIES["native"],
        ["--validate-inventory", str(inventory_path)],
    )
    if (inventory_status != 0 or inventory_stdout != inventory_output
            or inventory_stderr != output_bytes["inventory_stderr_path"]):
        refuse()

    completion = output_bytes["complete_stdout_path"]
    try:
        completion_text = completion.decode("ascii").strip()
        explicit_text = audit["explicit_verify_complete"]
    except (UnicodeDecodeError, AttributeError):
        refuse()
    if not isinstance(explicit_text, str) or explicit_text != completion_text:
        refuse()
    complete_match = re.fullmatch(r"classification=complete members=([0-9]+)\n?", completion_text)
    incomplete_match = re.fullmatch(
        r"classification=incomplete status=missing-or-nonpassing-member batches=([0-9]+) members=([0-9]+)\n?",
        completion_text,
    )
    validation_status, validated_stdout, validated_stderr = bounded_fixed_run(
        evidence_root, FIXED_BUILD_BINARIES["native"],
        ["--verify-complete-json", str(inventory_path), str(batches_dir)],
    )
    if (validated_stdout != completion or validated_stderr != output_bytes["complete_stderr_path"]
            or output_bytes["complete_stderr_path"]):
        refuse()
    if complete_match:
        if (validation_status != 0 or int(complete_match.group(1)) != len(expected_hashes)
                or audit["outcomes"] != {"guest-fail": 0, "pass": len(expected_hashes), "refused": 0}):
            refuse()
    elif incomplete_match:
        if (validation_status != 3 or int(incomplete_match.group(1)) != audit["authenticated_batches"]
                or int(incomplete_match.group(2)) != len(expected_hashes)
                or audit["outcomes"].get("guest-fail", 0) + audit["outcomes"].get("refused", 0) == 0):
            refuse()
    else:
        refuse()
    return inventory_path, batches_dir, projection, qualified_source_keys


def _check_private_sidecar(private_root: pathlib.Path, profile_id: str,
                           git_root: pathlib.Path, evidence_root: pathlib.Path,
                           qualified_source_anchor: tuple[str, frozenset[tuple[int, int, str]]] | None
                           = None) -> None:
    if private_root.is_symlink() or not private_root.is_dir() or stat.S_IMODE(private_root.stat().st_mode) != 0o700:
        refuse()
    sidecar = parse_json(checked_read(private_root / "phase05-final-evidence.json", MAX_PRIVATE_JSON, root=private_root))
    exact_object(sidecar, PRIVATE_KEYS)
    if sidecar["schema"] != "t64.rsp.private-final.v3" or sidecar["profile_id"] != profile_id:
        refuse()
    if not isinstance(sidecar["run_source_revision"], str) or not HEX40.fullmatch(sidecar["run_source_revision"]):
        refuse()
    if not isinstance(sidecar["private_build_id"], str) or not HEX64.fullmatch(sidecar["private_build_id"]):
        refuse()
    inventory_path = safe_private_reference(private_root, sidecar["inventory_path"])
    inventory_bytes = checked_read(inventory_path, MAX_PRIVATE_JSON, root=private_root)
    if not isinstance(sidecar["inventory_sha256"], str) or sha256(inventory_bytes) != sidecar["inventory_sha256"]:
        refuse()
    inventory = parse_json(inventory_bytes)
    if canonical_json(inventory) != inventory_bytes:
        refuse()
    if sidecar["projection_path"] != "rsp-source-projection.json":
        refuse()
    audit_path = safe_private_reference(private_root, sidecar["audit_path"])
    audit_bytes = checked_read(audit_path, MAX_PRIVATE_JSON, root=private_root)
    if not isinstance(sidecar["audit_sha256"], str) or sha256(audit_bytes) != sidecar["audit_sha256"]:
        refuse()
    audit = parse_json(audit_bytes)
    inventory_path, _, projection, qualified_source_keys = _check_private_c_outputs(
        private_root, evidence_root, sidecar, inventory, inventory_bytes, audit, git_root,
        qualified_source_anchor,
    )
    derived = derive_private_partition(projection, inventory, audit, private_root,
                                       sidecar["run_source_revision"], profile_id,
                                       sidecar["private_build_id"], git_root,
                                       evidence_root, inventory_path)
    if set(_projection_member_keys(projection)) != set(qualified_source_keys):
        refuse()
    partition = exact_object(sidecar["partition"], PARTITION_KEYS)
    for key, value in partition.items():
        if key in {"expected_sha256", "classified_sha256"}:
            if not isinstance(value, str) or not HEX64.fullmatch(value):
                refuse()
        elif type(value) is not int or value < 0:
            refuse()
    if partition != derived:
        refuse()
    if (derived["expected_rows"] == 0 or derived["expected_rows"] != derived["classified_rows"]
            or derived["missing_rows"] or derived["duplicate_rows"] or derived["extra_rows"]
            or derived["unclassified_rows"] or derived["dropped_refusal_rows"]):
        refuse()
    if (derived["refusal_rows"] != derived["reasoned_refusal_rows"]
            or derived["nonpass_rows"] != derived["reasoned_nonpass_rows"]):
        refuse()

    red_rows = sidecar["red_evidence"]
    if not isinstance(red_rows, list) or len(red_rows) > 128:
        refuse()
    for row in red_rows:
        exact_object(row, RED_KEYS)
        if row["kind"] != "semantic" or row["setup"] != "complete":
            refuse()
        red_path = safe_private_reference(private_root, row["path"])
        red_bytes = checked_read(red_path, MAX_LOG, root=private_root)
        if not isinstance(row["sha256"], str) or sha256(red_bytes) != row["sha256"]:
            refuse()
        if not isinstance(row["fetched_opcode"], str) or not re.fullmatch(r"0x[0-9a-fA-F]{8}", row["fetched_opcode"]):
            refuse()
        if not isinstance(row["pc"], str) or not re.fullmatch(r"0x[0-9a-fA-F]{1,8}", row["pc"]):
            refuse()
        if not isinstance(row["expected"], str) or not isinstance(row["actual"], str) or not row["expected"] or not row["actual"] or row["expected"] == row["actual"]:
            refuse()


def validate(evidence_root: pathlib.Path = ROOT, owner_root: pathlib.Path = ROOT,
             private_root: pathlib.Path | None = None,
             qualified_source_anchor: tuple[str, frozenset[tuple[int, int, str]]] | None = None) -> None:
    evidence_root = evidence_root.resolve()
    owner_root = owner_root.resolve()
    private_root = private_root or (evidence_root / PRIVATE_ROOT)
    private_root = pathlib.Path(private_root)
    receipt_bytes = checked_read(evidence_root / PUBLIC_RECEIPT, MAX_RECEIPT, root=evidence_root)
    receipt = parse_json(receipt_bytes)
    exact_object(receipt, RECEIPT_KEYS)
    if receipt["schema"] != "t64.phase05.final-evidence.v1":
        refuse()
    revision = full_revision(receipt["source_revision"], owner_root)
    content = source_content_sha256(owner_root)
    if receipt["content_sha256"] != content:
        refuse()
    verification = verification_inputs_sha256(owner_root)
    if receipt["verification_sha256"] != verification:
        refuse()
    profile_source = checked_read(owner_root / PROFILE_SOURCE, 2_000_000, root=owner_root).decode("utf-8")
    profile_match = PROFILE_ID_RE.search(profile_source)
    if profile_match is None or receipt["profile_id"] != profile_match.group(1):
        refuse()
    profile_id = profile_match.group(1)

    ledger_sha, phase_tests, threat_severity = owned_tables(owner_root)
    if receipt["opcode_ledger_sha256"] != ledger_sha or receipt["requirement_owners"] != REQUIREMENT_OWNERS or receipt["opcode_owners"] != OPCODE_OWNERS:
        refuse()
    threat_owners = exact_object(receipt["threat_owners"], set(threat_severity))
    for threat, severity in threat_severity.items():
        if threat_owners[threat] not in {"closed", "open-judgment"} or severity in {"high", "critical"} and threat_owners[threat] != "closed":
            refuse()

    builds = exact_object(receipt["builds"], {"native", "asan"})
    build_identities = {}
    for name, binary_path in FIXED_BUILD_BINARIES.items():
        build = exact_object(builds[name], {
            "build_id", "artifact_path", "artifact_sha256", "source_revision",
            "content_sha256", "profile_id",
        })
        if (build["artifact_path"] != binary_path
                or not isinstance(build["build_id"], str) or not BUILD_ID.fullmatch(build["build_id"])
                or not isinstance(build["artifact_sha256"], str) or not HEX64.fullmatch(build["artifact_sha256"])):
            refuse()
        if build["source_revision"] != revision or build["content_sha256"] != content or build["profile_id"] != profile_id:
            refuse()
        artifact = evidence_root / binary_path
        artifact_bytes = checked_read(artifact, MAX_ARTIFACT, root=evidence_root)
        if sha256(artifact_bytes) != build["artifact_sha256"]:
            refuse()
        identity = runtime_identity(evidence_root, binary_path)
        if (identity["core_source_revision"] != revision or identity["profile_id"] != profile_id
                or build["build_id"] != f"t64-build-v1-{identity['core_build_sha256']}"):
            refuse()
        build_identities[name] = identity
    if any(build_identities["native"][key] != build_identities["asan"][key]
           for key in ("core_source_revision", "profile_descriptor_sha256", "profile_id")):
        refuse()

    native_bytes = checked_read(evidence_root / "build/gate/05-10-final-gate.log", MAX_LOG, root=evidence_root)
    asan_bytes = checked_read(evidence_root / "build/asan/05-10-final-ctest.log", MAX_LOG, root=evidence_root)
    for record, data in ((receipt["native_gate"], native_bytes), (receipt["asan_gate"], asan_bytes)):
        exact_object(record, {"sha256"})
        if record["sha256"] != sha256(data):
            refuse()
    native_text = native_bytes.decode("utf-8", errors="strict")
    asan_text = asan_bytes.decode("utf-8", errors="strict")
    if ("[gate] PASS" not in native_text
            or not re.search(r"(?m)^test inventory PASS preset=gate count=127$", native_text)
            or not re.search(r"(?m)^core-lint: files=[0-9]+ violations=0$", native_text)):
        refuse()
    native_required = native_gate_tests(owner_root)
    validate_ctest_log(native_text, native_required)
    asan_required = set(phase_tests) | {"runner.cpu-diagnostic-protocol", "runner.cpu-diagnostic-inventory", "fuzz.rom-admission", "sanitizer.negative-control"}
    # The owning selection may include further registered regressions. Every
    # selected row must pass; accepting extras must not admit invented names.
    validate_ctest_log(
        asan_text, asan_required,
        allowed=native_required | {"fuzz.rom-admission", "sanitizer.negative-control"},
    )

    hosted = receipt["hosted"]
    _check_hosted_run(evidence_root, hosted, revision, content)

    composition = exact_object(receipt["composition"], {"rom_sha256", "build_receipt_sha256"})
    rom_bytes = checked_read(evidence_root / "build/fixtures/rsp-smoke/rsp-smoke.z64", MAX_ARTIFACT, root=evidence_root)
    build_receipt_bytes = checked_read(evidence_root / "build/fixtures/rsp-smoke/rsp-smoke.build.json", MAX_RECEIPT, root=evidence_root)
    if composition["rom_sha256"] != sha256(rom_bytes) or composition["build_receipt_sha256"] != sha256(build_receipt_bytes):
        refuse()
    build_receipt = parse_json(build_receipt_bytes)
    fixture = build_receipt.get("fixture") if build_receipt.get("schema") == "t64.rsp-smoke-build" else None
    if not isinstance(fixture, dict) or fixture.get("variant") != 0 or fixture.get("profile") != profile_id or fixture.get("expected", {}).get("classification") != "pass" or fixture.get("sha256") != sha256(rom_bytes) or fixture.get("size_bytes") != len(rom_bytes):
        refuse()

    reports = exact_object(receipt["reports"], {"native_sha256", "code_sha256", "security_sha256", "authored_sha256"})
    report_paths = {
        "native_sha256": "docs/evidence/phase05-native-rsp.md",
        "code_sha256": "docs/evidence/phase05-review.md",
        "security_sha256": "docs/evidence/phase05-security.md",
        "authored_sha256": "docs/evidence/phase05-authored-rsp.md",
    }
    public_bytes = [receipt_bytes]
    report_content = {}
    for key, relative in report_paths.items():
        data = checked_read(evidence_root / relative, 2_000_000, root=evidence_root)
        if reports[key] != sha256(data):
            refuse()
        public_bytes.append(data)
        report_content[key] = data
    code_meta = frontmatter(report_content["code_sha256"].decode("utf-8"))
    security_meta = frontmatter(report_content["security_sha256"].decode("utf-8"))
    if code_meta.get("status") not in {"complete", "issues_found"} or code_meta.get("unresolved_blockers") != "0":
        refuse()
    if security_meta.get("status") not in {"secured", "open_threats"} or security_meta.get("open_high") != "0":
        refuse()
    privacy_scan(receipt_bytes, approved_rom_hash=composition["rom_sha256"])
    for data in public_bytes[1:]:
        privacy_scan(data)
    if receipt["differential_claim"] != "none":
        refuse()

    _check_private_sidecar(private_root, profile_id, owner_root, evidence_root,
                           qualified_source_anchor)


def latest_tested_source(root: pathlib.Path = ROOT) -> str:
    log = checked_read(root / "docs/ci-reviews/LOG.md", 8_000_000, root=root).decode("utf-8")
    rows = [[field.strip() for field in line.strip().strip("|").split("|")]
            for line in log.splitlines() if line.startswith("|")]
    candidates = [(row[1], row[2]) for row in rows if len(row) >= 5
                  and re.fullmatch(r"[0-9]{6,20}", row[1])
                  and re.fullmatch(r"[0-9a-f]{12}", row[2])
                  and row[3] == "push" and row[4] == "success"]
    # The append-only ledger can gain a recovered historical row after newer
    # findings, so run id (GitHub's monotonically increasing id) is authoritative.
    for run_id, prefix in sorted(candidates, key=lambda item: int(item[0]), reverse=True):
        if not any(len(row) >= 3 and row[1] == run_id and "Both hosts pass" in row[2]
                   for row in rows):
            continue
        try:
            result = subprocess.run(
                ["git", "-C", str(root), "rev-parse", "--verify", f"{prefix}^{{commit}}"],
                check=False, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=5,
            )
        except (OSError, subprocess.TimeoutExpired):
            continue
        revision = result.stdout.strip()
        if result.returncode == 0 and HEX40.fullmatch(revision) and revision.startswith(prefix):
            return revision
    refuse()


def _write_json(path: pathlib.Path, value) -> None:
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    path.write_bytes(canonical_json(value))
    path.chmod(0o600)


def _synthetic_workspace(workspace: pathlib.Path, owner_root: pathlib.Path):
    revision = subprocess.run(["git", "-C", str(owner_root), "rev-parse", "HEAD"], check=True,
                              stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=5).stdout.strip()
    profile_text = checked_read(owner_root / PROFILE_SOURCE, 2_000_000, root=owner_root).decode("utf-8")
    profile_id = PROFILE_ID_RE.search(profile_text).group(1)
    content_digest = source_content_sha256(owner_root)
    verification_digest = verification_inputs_sha256(owner_root)
    ledger_sha, phase_tests, threat_severity = owned_tables(owner_root)

    build_identities = {}
    artifacts = {}
    for name, binary_path in FIXED_BUILD_BINARIES.items():
        source = owner_root / binary_path
        source_bytes = checked_read(source, MAX_ARTIFACT, root=owner_root)
        target = workspace / binary_path
        target.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        target.chmod(0o700)
        artifacts[name] = source_bytes
        build_identities[name] = runtime_identity(workspace, binary_path)
        if (build_identities[name]["core_source_revision"] != revision
                or build_identities[name]["profile_id"] != profile_id):
            refuse()

    native_required = native_gate_tests(owner_root)
    native_total = len(native_required)
    native_rows = "\n".join(
        f"{index}/{native_total} Test # {index}: {name} ................ Passed    0.01 sec"
        for index, name in enumerate(sorted(native_required), start=1)
    )
    native_text = (
        "test inventory PASS preset=gate count=127\ncore-lint: files=86 violations=0\n"
        + native_rows + f"\n100% tests passed out of {native_total}\n[gate] PASS\n"
    )
    asan_names = set(phase_tests) | {"runner.cpu-diagnostic-protocol", "runner.cpu-diagnostic-inventory", "fuzz.rom-admission", "sanitizer.negative-control"}
    asan_total = len(asan_names)
    asan_text = "\n".join(
        f"{index}/{asan_total} Test # {index}: {name} ................ Passed    0.01 sec"
        for index, name in enumerate(sorted(asan_names), start=1)
    ) + f"\n100% tests passed out of {asan_total}\n"
    for relative, text in (("build/gate/05-10-final-gate.log", native_text),
                           ("build/asan/05-10-final-ctest.log", asan_text)):
        path = workspace / relative
        path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    reports_dir = workspace / "docs/evidence"
    reports_dir.mkdir(mode=0o700, parents=True, exist_ok=True)
    reports = {
        "phase05-native-rsp.md": "# Authored final evidence\n\nNo private records are included.\n",
        "phase05-authored-rsp.md": "# Authored controls\n\nNo external inputs are included.\n",
        "phase05-review.md": "---\nstatus: issues_found\nunresolved_blockers: 0\n---\n\nConfigured review; lower flags remain visible.\n",
        "phase05-security.md": "---\nstatus: open_threats\nopen_high: 0\n---\n\nConfigured review; lower flags remain visible.\n",
    }
    for name, text in reports.items():
        (reports_dir / name).write_text(text, encoding="utf-8")

    run_id = "99999999999"
    ci = workspace / "docs/ci-reviews/LOG.md"
    ci.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    ci.write_text(f"| date | run id | sha (12) | event | conclusion |\n|---|---|---|---|---|\n| 2026-11-14 | {run_id} | {revision[:12]} | push | success |\n\n| F-999 | {run_id} | Both hosts pass authored synthetic source {revision[:12]}:127 native checks. | taken | synthetic control |\n", encoding="utf-8")

    rom = b"authored synthetic ROM composition\n"
    rom_dir = workspace / "build/fixtures/rsp-smoke"
    rom_dir.mkdir(mode=0o700, parents=True, exist_ok=True)
    (rom_dir / "rsp-smoke.z64").write_bytes(rom)
    build_receipt = {"schema": "t64.rsp-smoke-build", "fixture": {
        "variant": 0, "profile": profile_id, "expected": {"classification": "pass"},
        "sha256": sha256(rom), "size_bytes": len(rom),
    }}
    _write_json(rom_dir / "rsp-smoke.build.json", build_receipt)

    private_base = workspace / "build/private-rsp-diagnostic"
    private_base.mkdir(mode=0o700, parents=True, exist_ok=True)
    private_base.chmod(0o700)
    private_root = private_base / "synthetic-fixture"
    private_root.mkdir(mode=0o700)
    private_build_id = build_identities["native"]["core_build_sha256"]
    run_dir = private_root / "runs/refresh"
    batches_dir = run_dir / "batches"
    attempts_dir = run_dir / "attempts"
    verify_dir = run_dir / "verification"
    for directory in (batches_dir, attempts_dir, verify_dir):
        directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    registrations = [{
        "constructor": "rsp::synthetic", "execution": "values", "group": "rsp",
        "index": 7, "lane": "required-base", "level": "BasicFunctionality", "name": "synthetic",
        "source_file": "src/tests/rsp/synthetic.c", "source_file_sha256": "4" * 64,
        "source_name": "synthetic", "source_value_count": 3,
        "values_method_sha256": "5" * 64,
        "values": [{"id": f"v{index:05d}", "index": index,
                    "selected_rom_sha256": f"{index + 6:x}" * 64,
                    "selector": f"r0007v{index:05d}"} for index in range(3)],
    }]
    projection_registrations = [{
        **{key: value for key, value in registrations[0].items() if key != "values"},
        "effective_value_count": 3,
        "values": [{key: value[key] for key in SOURCE_PROJECTION_VALUE_KEYS}
                   for value in registrations[0]["values"]],
    }]
    projection = {
        "component": "rsp", "declared_counts": {"registrations": 1, "values": 3},
        "default_feature_closure": {}, "inputs": {}, "preuse_closure": {},
        "registrations": projection_registrations,
        "schema": "t64.rsp-source-projection", "schema_version": 1,
        "selection": {}, "source_revision": revision, "source_root": "synthetic",
        "status": "qualified-synthetic",
    }
    projection_path = private_root / "rsp-source-projection.json"
    _write_json(projection_path, projection)
    projection_sha = sha256(projection_path.read_bytes())
    inventory = {
        "component": "rsp", "declared_counts": {"registrations": 1, "required_values": 3, "values": 3},
        "identity": {
            "boot_sha256": "6" * 64, "closure_sha256": "7" * 64,
            "core_build_sha256": private_build_id, "core_source_revision": revision,
            "overlay_sha256": "8" * 64,
            "profile_descriptor_sha256": build_identities["native"]["profile_descriptor_sha256"],
            "profile_id": profile_id, "qualification_profile_sha256": "a" * 64,
            "qualification_sha256": "b" * 64, "rom_sha256": "c" * 64,
            "source_projection_sha256": projection_sha, "source_revision": revision,
        },
        "registrations": registrations, "schema": "t64.rsp.inventory", "schema_version": 1,
    }
    inventory_path = run_dir / "rsp-inventory.json"
    _write_json(inventory_path, inventory)
    inventory_sha = sha256(inventory_path.read_bytes())
    audit_rows = []
    outcomes = {"guest-fail": 0, "pass": 0, "refused": 0}
    native_executable = str((workspace / FIXED_BUILD_BINARIES["native"]).resolve())
    for index, classification in enumerate(("pass", "refused", "pass")):
        selector = f"r0007v{index:05d}"
        batch_rel = f"runs/refresh/batches/member-{index}.json"
        stdout_rel = f"runs/refresh/attempts/member-{index}.stdout.log"
        stderr_rel = f"runs/refresh/attempts/member-{index}.stderr.log"
        attempt_rel = f"runs/refresh/attempts/member-{index}.command.json"
        rom_path = run_dir / f"member-{selector}.z64"
        rom_data = b"authored synthetic selector ROM\n" + bytes([index])
        rom_path.write_bytes(rom_data)
        rom_hash = registrations[0]["values"][index]["selected_rom_sha256"]
        if classification == "pass":
            operation, reason, terminal = "", "", True
            stdout_data = ("classification=pass terminal_pc=80000420 terminal_opcode=08000108 "
                           "delay_slot_pc=80000424 delay_slot_opcode=00000000 tick=1 retired=2 capture=0\n").encode()
            terminal_pc, terminal_opcode, delay_pc, delay_opcode, retired = 0x80000420, 0x08000108, 0x80000424, 0, 2
        else:
            operation, reason, terminal = "rsp-opcode", "refusal-code-7", False
            stdout_data = ("classification=refused pc=000 opcode=00000000 operation=rsp-opcode "
                           "reason=refusal-code-7 tick=1 retired=0 capture=0\n").encode()
            terminal_pc, terminal_opcode, delay_pc, delay_opcode, retired = 0, 0, 0, 0, 0
        stderr_data = b""
        batch = {
            "component": "rsp",
            "identity": {**inventory["identity"], "inventory_sha256": inventory_sha},
            "member": {"group": "rsp", "registration_index": 7,
                       "registration_name": "synthetic", "selected_rom_sha256": rom_hash,
                       "selector": selector, "source_name": "synthetic",
                       "value_id": f"v{index:05d}", "value_index": index},
            "result": {
                "capture_sha256": "f" * 64, "operation": operation,
                "outcome": classification, "reason": reason, "result_sha256": "e" * 64,
                "retired_instructions": retired, "terminal_delay_slot_opcode": delay_opcode,
                "terminal_delay_slot_pc": delay_pc, "terminal_delay_slot_retired": terminal,
                "terminal_opcode": terminal_opcode, "terminal_pc": terminal_pc,
            },
            "schema": "t64.rsp.batch", "schema_version": 1,
        }
        batch_data = canonical_json(batch)
        for relative, data in ((batch_rel, batch_data), (stdout_rel, stdout_data), (stderr_rel, stderr_data)):
            path = private_root / relative
            path.write_bytes(data)
        batch_sha, stdout_sha, stderr_sha = map(sha256, (batch_data, stdout_data, stderr_data))
        attempt = {
            "adapter_argv": [native_executable, "--run-local", "--local-only",
                             str(inventory_path.resolve()), str(rom_path.resolve()), str(len(rom_data)),
                             rom_hash, "10000000", "7", str(index), str((private_root / batch_rel).resolve())],
            "adapter_exit": 0 if classification == "pass" else 2,
            "attempted_batch_path": str((private_root / batch_rel).resolve()),
            "batch_path": str((private_root / batch_rel).resolve()), "batch_sha256": batch_sha,
            "capture_bytes": len(stdout_data) + len(stderr_data), "capture_limit": 4096,
            "child_wall_seconds": 10, "cleanup_group_empty": True,
            "cleanup_signal_denied": True, "classification": classification,
            "outer_deadline_expired": False, "outer_wall_seconds": 12,
            "output_overflow": False, "registration_index": 7, "rejected_batch_path": None,
            "selector": selector, "started_utc": "2026-01-01T00:00:00+00:00",
            "step_ticks": 16, "tick_limit": 10_000_000, "value_index": index,
        }
        attempt_path = private_root / attempt_rel
        _write_json(attempt_path, attempt)
        outcomes[classification] += 1
        audit_rows.append({
            "attempt_receipt_path": attempt_rel, "attempt_receipt_sha256": sha256(attempt_path.read_bytes()),
            "batch_path": batch_rel, "batch_sha256": batch_sha, "capture_limit": 4096,
            "child_wall_seconds": 10, "classification": classification,
            "identity_sha256": "e" * 64, "outer_wall_seconds": 12,
            "owner": "synthetic-owner", "reason": reason,
            "registration_index": 7, "selector": selector,
            "stderr_path": stderr_rel, "stderr_sha256": stderr_sha,
            "stdout_path": stdout_rel, "stdout_sha256": stdout_sha,
            "step_ticks": 16, "terminal_instruction_pair_retired": classification == "pass",
            "tick_limit": 10_000_000, "value_index": index,
        })
    audit = {
        "aggregate": "synthetic", "authenticated_batches": 3,
        "classification": "synthetic-incomplete", "current_run_path": "runs/refresh",
        "expected_members": 3, "explicit_verification_dir": "runs/refresh/verification",
        "explicit_verify_complete": "classification=incomplete status=missing-or-nonpassing-member batches=3 members=3",
        "inventory_sha256": inventory_sha,
        "member_results": audit_rows, "member_set_exact": True, "outcomes": outcomes,
        "schema": "t64.rsp-plan10-current-run-outcome-audit.v1",
    }
    audit_path = private_root / "runs/outcome-audit-v1/manifest.json"
    _write_json(audit_path, audit)
    inventory_validation_stdout = (
        f"classification=valid registrations=1 required_values=3 inventory_sha256={inventory_sha}\n"
    ).encode()
    inventory_validation_stdout_path = run_dir / "inventory-validation.stdout.log"
    inventory_validation_stderr_path = run_dir / "inventory-validation.stderr.log"
    inventory_validation_stdout_path.write_bytes(inventory_validation_stdout)
    inventory_validation_stderr_path.write_bytes(b"")
    completion_stdout_path = verify_dir / "complete-verification.stdout.log"
    completion_stderr_path = verify_dir / "complete-verification.stderr.log"
    completion_stdout_path.write_text(audit["explicit_verify_complete"] + "\n", encoding="ascii")
    completion_stderr_path.write_bytes(b"")
    c_outputs = {
        "inventory_stdout_path": "runs/refresh/inventory-validation.stdout.log",
        "inventory_stdout_sha256": sha256(inventory_validation_stdout),
        "inventory_stderr_path": "runs/refresh/inventory-validation.stderr.log",
        "inventory_stderr_sha256": sha256(b""),
        "complete_stdout_path": "runs/refresh/verification/complete-verification.stdout.log",
        "complete_stdout_sha256": sha256(completion_stdout_path.read_bytes()),
        "complete_stderr_path": "runs/refresh/verification/complete-verification.stderr.log",
        "complete_stderr_sha256": sha256(b""),
    }
    c_validation_status, c_validation_stdout, c_validation_stderr = bounded_fixed_run(
        workspace, FIXED_BUILD_BINARIES["native"],
        ["--validate-inventory", str(inventory_path.resolve())],
    )
    c_completion_status, c_completion_stdout, c_completion_stderr = bounded_fixed_run(
        workspace, FIXED_BUILD_BINARIES["native"],
        ["--verify-complete-json", str(inventory_path.resolve()), str(batches_dir.resolve())],
    )
    if (c_validation_status != 0 or c_validation_stdout != inventory_validation_stdout
            or c_validation_stderr or c_completion_status != 3
            or c_completion_stdout != completion_stdout_path.read_bytes()
            or c_completion_stderr):
        refuse()
    partition = derive_private_partition(projection, inventory, audit, private_root,
                                         revision, profile_id, private_build_id,
                                         owner_root, workspace, inventory_path)
    red_path = private_root / "red.json"
    red_path.write_text('{"assertion":"fetched mismatch"}\n', encoding="utf-8")
    private = {
        "schema": "t64.rsp.private-final.v3", "run_source_revision": revision,
        "profile_id": profile_id, "private_build_id": private_build_id,
        "inventory_path": "runs/refresh/rsp-inventory.json",
        "inventory_sha256": inventory_sha,
        "projection_path": "rsp-source-projection.json", "projection_sha256": projection_sha,
        "partition": partition,
        "audit_path": "runs/outcome-audit-v1/manifest.json",
        "audit_sha256": sha256(audit_path.read_bytes()), "c_outputs": c_outputs,
        "red_evidence": [{"kind": "semantic", "setup": "complete", "path": "red.json",
                          "sha256": sha256(red_path.read_bytes()), "fetched_opcode": "0x4a0208d4",
                          "pc": "0x000", "expected": "0x1234", "actual": "0x1235"}],
    }
    _write_json(private_root / "phase05-final-evidence.json", private)

    hosted_platforms = {}
    hosted_record_dir = workspace / "build/gate/05-10-hosted"
    hosted_record_dir.mkdir(mode=0o700, parents=True, exist_ok=True)
    result_profile_id = "ntsc-cart-4m-openipl3-warm-v1"
    result_profile_digest = "9" * 64
    for index, job in enumerate(("gate-linux", "gate-macos"), start=1):
        build_id = f"t64-build-v1-{'c' * 64}" if index == 1 else f"t64-build-v1-{'e' * 64}"
        wrapper = {
            "schema": "t64.boot-evidence", "schema_version": 1,
            "commit_sha": revision, "run_id": run_id, "run_attempt": "1", "job_name": job,
            "result": {
                "schema": "t64.result", "schema_version": 2,
                "source_revision": revision, "build_id": build_id,
                "profile_id": result_profile_id, "profile_digest": result_profile_digest,
                "outcome": "pass", "detail": "positive-match", "fixture_id": "hello-openipl3",
            },
        }
        record_path = hosted_record_dir / f"{job}.json"
        _write_json(record_path, wrapper)
        hosted_platforms[job] = {
            "record_path": record_path.relative_to(workspace).as_posix(),
            "record_sha256": sha256(record_path.read_bytes()),
            "artifact_id": str(900000 + index),
            "artifact_digest": f"sha256:{'a' * 64}" if index == 1 else f"sha256:{'b' * 64}",
            "build_id": build_id, "result_profile_id": result_profile_id,
            "result_profile_digest": result_profile_digest,
        }

    receipt = {
        "schema": "t64.phase05.final-evidence.v1", "source_revision": revision,
        "content_sha256": content_digest, "verification_sha256": verification_digest,
        "profile_id": profile_id,
        "opcode_ledger_sha256": ledger_sha, "requirement_owners": REQUIREMENT_OWNERS,
        "opcode_owners": OPCODE_OWNERS, "threat_owners": {key: "closed" for key in threat_severity},
        "builds": {
            "native": {"build_id": f"t64-build-v1-{build_identities['native']['core_build_sha256']}", "artifact_path": FIXED_BUILD_BINARIES["native"], "artifact_sha256": sha256(artifacts["native"]), "source_revision": revision, "content_sha256": content_digest, "profile_id": profile_id},
            "asan": {"build_id": f"t64-build-v1-{build_identities['asan']['core_build_sha256']}", "artifact_path": FIXED_BUILD_BINARIES["asan"], "artifact_sha256": sha256(artifacts["asan"]), "source_revision": revision, "content_sha256": content_digest, "profile_id": profile_id},
        },
        "native_gate": {"sha256": sha256(native_text.encode())},
        "asan_gate": {"sha256": sha256(asan_text.encode())},
        "hosted": {
            "run_id": run_id, "attempt": "1", "source_revision": revision,
            "source_sha": revision[:12], "content_sha256": content_digest,
            "platforms": hosted_platforms,
        },
        "composition": {"rom_sha256": sha256(rom), "build_receipt_sha256": sha256(canonical_json(build_receipt))},
        "reports": {
            "native_sha256": sha256(reports["phase05-native-rsp.md"].encode()),
            "code_sha256": sha256(reports["phase05-review.md"].encode()),
            "security_sha256": sha256(reports["phase05-security.md"].encode()),
            "authored_sha256": sha256(reports["phase05-authored-rsp.md"].encode()),
        },
        "differential_claim": "none",
    }
    _write_json(workspace / PUBLIC_RECEIPT, receipt)
    qualified_source_anchor = (projection_sha, _projection_member_keys(projection))
    return workspace, private_root, receipt, qualified_source_anchor


def _expect_refusal(action) -> None:
    try:
        action()
    except Refusal:
        return
    raise Refusal("an authored evidence mutation was accepted")


def _mutate_receipt(workspace: pathlib.Path, private_root: pathlib.Path,
                    owner_root: pathlib.Path, baseline: dict, mutate,
                    qualified_source_anchor) -> None:
    candidate = json.loads(json.dumps(baseline))
    mutate(candidate)
    _write_json(workspace / PUBLIC_RECEIPT, candidate)
    _expect_refusal(lambda: validate(workspace, owner_root, private_root,
                                     qualified_source_anchor))
    _write_json(workspace / PUBLIC_RECEIPT, baseline)


def _mutate_private(workspace: pathlib.Path, private_root: pathlib.Path,
                    owner_root: pathlib.Path, baseline: dict, mutate,
                    qualified_source_anchor) -> None:
    candidate = json.loads(json.dumps(baseline))
    mutate(candidate)
    _write_json(private_root / "phase05-final-evidence.json", candidate)
    _expect_refusal(lambda: validate(workspace, owner_root, private_root,
                                     qualified_source_anchor))
    _write_json(private_root / "phase05-final-evidence.json", baseline)


def run_selftest(owner_root: pathlib.Path = ROOT) -> None:
    build_root = owner_root / "build/gate"
    if build_root.is_symlink() or not build_root.is_dir():
        refuse()
    with tempfile.TemporaryDirectory(prefix="05-10-evidence-selftest.", dir=build_root) as temporary:
        workspace = pathlib.Path(temporary)
        workspace.chmod(0o700)
        workspace, private_root, baseline, qualified_source_anchor = _synthetic_workspace(workspace, owner_root)
        check_fixture = lambda: validate(workspace, owner_root, private_root, qualified_source_anchor)
        check_fixture()  # Accepted baseline is mandatory.

        for mutate in (
            lambda r: r.__setitem__("schema", "open-schema"),
            lambda r: r.__setitem__("source_revision", "0" * 40),
            lambda r: r.__setitem__("content_sha256", "0" * 64),
            lambda r: r.__setitem__("verification_sha256", "0" * 64),
            lambda r: r.__setitem__("profile_id", "stale-profile"),
            lambda r: r.__setitem__("opcode_ledger_sha256", "0" * 64),
            lambda r: r["requirement_owners"].pop("RSP-05"),
            lambda r: r["opcode_owners"].pop("vector-divider"),
            lambda r: r["threat_owners"].__setitem__(next(k for k in r["threat_owners"] if k != "T-05-SC"), "open-judgment"),
            lambda r: r["builds"]["native"].__setitem__("artifact_sha256", "0" * 64),
            lambda r: r["builds"]["native"].__setitem__("build_id", "t64-build-v1-" + "f" * 64),
            lambda r: r["builds"]["asan"].__setitem__("source_revision", "0" * 40),
            lambda r: r["builds"]["native"].__setitem__("profile_id", "stale-profile"),
            lambda r: r["native_gate"].__setitem__("sha256", "0" * 64),
            lambda r: r["asan_gate"].__setitem__("sha256", "0" * 64),
            lambda r: r["hosted"].__setitem__("attempt", "2"),
            lambda r: r["hosted"]["platforms"]["gate-linux"].__setitem__("artifact_digest", "not-a-digest"),
            lambda r: r["hosted"]["platforms"]["gate-macos"].__setitem__("record_sha256", "0" * 64),
            lambda r: r["composition"].__setitem__("rom_sha256", "0" * 64),
            lambda r: r.pop("composition"),
            lambda r: r["reports"].__setitem__("code_sha256", "0" * 64),
            lambda r: r.__setitem__("differential_claim", "complete-state"),
        ):
            _mutate_receipt(workspace, private_root, owner_root, baseline, mutate,
                            qualified_source_anchor)

        native_artifact = workspace / FIXED_BUILD_BINARIES["native"]
        saved_artifact = native_artifact.read_bytes()
        native_artifact.write_bytes(b"one byte changed after the test\n")
        _expect_refusal(check_fixture)
        native_artifact.write_bytes(saved_artifact)
        asan_log = workspace / "build/asan/05-10-final-ctest.log"
        saved_asan_log = asan_log.read_bytes()
        asan_log.write_bytes(b"")
        _expect_refusal(check_fixture)
        asan_log.write_bytes(saved_asan_log)
        lines = saved_asan_log.decode("utf-8").splitlines()
        required_test = sorted(set(owned_tables(owner_root)[1]) | {
            "runner.cpu-diagnostic-protocol", "runner.cpu-diagnostic-inventory",
            "fuzz.rom-admission", "sanitizer.negative-control",
        })[0]
        matched_line = next(index for index, line in enumerate(lines) if f": {required_test} " in line)
        lines[matched_line] = lines[matched_line].replace("Passed", "Failed", 1)
        forged_log = ("\n".join(lines) + "\n").encode("utf-8")
        asan_log.write_bytes(forged_log)
        forged_receipt = json.loads(json.dumps(baseline))
        forged_receipt["asan_gate"]["sha256"] = sha256(forged_log)
        _write_json(workspace / PUBLIC_RECEIPT, forged_receipt)
        _expect_refusal(check_fixture)
        asan_log.write_bytes(saved_asan_log)
        _write_json(workspace / PUBLIC_RECEIPT, baseline)
        native_log = workspace / "build/gate/05-10-final-gate.log"
        saved_native_log = native_log.read_bytes()
        marker_only = (
            b"test inventory PASS preset=gate count=127\n"
            b"core-lint: files=86 violations=0\n"
            b"100% tests passed out of 127\n[gate] PASS\n"
        )
        native_log.write_bytes(marker_only)
        marker_receipt = json.loads(json.dumps(baseline))
        marker_receipt["native_gate"]["sha256"] = sha256(marker_only)
        _write_json(workspace / PUBLIC_RECEIPT, marker_receipt)
        _expect_refusal(check_fixture)
        native_log.write_bytes(saved_native_log)
        _write_json(workspace / PUBLIC_RECEIPT, baseline)

        composed_rom = workspace / "build/fixtures/rsp-smoke/rsp-smoke.z64"
        saved_rom = composed_rom.read_bytes()
        composed_rom.unlink()
        _expect_refusal(check_fixture)
        composed_rom.write_bytes(saved_rom)

        private_path = private_root / "phase05-final-evidence.json"
        private_baseline = parse_json(private_path.read_bytes())
        for mutate in (
            lambda p: p["partition"].__setitem__("dropped_refusal_rows", 1),
            lambda p: p["partition"].__setitem__("reasoned_refusal_rows", 0),
            lambda p: p["partition"].__setitem__("classified_rows", 2),
            lambda p: p["partition"].__setitem__("classified_sha256", "0" * 64),
            lambda p: p["partition"].__setitem__("extra_rows", 1),
        ):
            _mutate_private(workspace, private_root, owner_root, private_baseline, mutate,
                            qualified_source_anchor)

        inventory_path = safe_private_reference(private_root, private_baseline["inventory_path"])
        original_inventory_bytes = inventory_path.read_bytes()
        inventory = parse_json(original_inventory_bytes)
        projection = parse_json((private_root / "rsp-source-projection.json").read_bytes())
        audit_path = safe_private_reference(private_root, private_baseline["audit_path"])
        original_audit_bytes = audit_path.read_bytes()
        audit = parse_json(original_audit_bytes)
        inventory_validation_path = safe_private_reference(
            private_root, private_baseline["c_outputs"]["inventory_stdout_path"]
        )
        inventory_stderr_path = safe_private_reference(
            private_root, private_baseline["c_outputs"]["inventory_stderr_path"]
        )
        complete_path = safe_private_reference(
            private_root, private_baseline["c_outputs"]["complete_stdout_path"]
        )
        complete_stderr_path = safe_private_reference(
            private_root, private_baseline["c_outputs"]["complete_stderr_path"]
        )
        original_inventory_validation_bytes = inventory_validation_path.read_bytes()
        original_inventory_stderr_bytes = inventory_stderr_path.read_bytes()
        original_complete_bytes = complete_path.read_bytes()
        original_complete_stderr_bytes = complete_stderr_path.read_bytes()

        projection_path = private_root / "rsp-source-projection.json"
        original_projection_bytes = projection_path.read_bytes()
        original_inventory = json.loads(json.dumps(inventory))
        original_projection = json.loads(json.dumps(projection))

        # Shrink the mutable projection and inventory together, then reseal the audit,
        # batches, attempts, C metadata outputs, and summaries. The original injected
        # source-closure anchor must still reject this internally consistent smaller set.
        shrunk_projection = json.loads(json.dumps(projection))
        shrunk_projection["registrations"][0]["source_value_count"] = 2
        shrunk_projection["registrations"][0]["effective_value_count"] = 2
        shrunk_projection["registrations"][0]["values"].pop()
        shrunk_projection["declared_counts"]["values"] = 2
        shrunk_projection_bytes = canonical_json(shrunk_projection)
        shrunk_projection_sha = sha256(shrunk_projection_bytes)
        shrunk_inventory = json.loads(json.dumps(inventory))
        shrunk_inventory["registrations"][0]["source_value_count"] = 2
        shrunk_inventory["registrations"][0]["values"].pop()
        shrunk_inventory["declared_counts"]["required_values"] = 2
        shrunk_inventory["declared_counts"]["values"] = 2
        shrunk_inventory["identity"]["source_projection_sha256"] = shrunk_projection_sha
        shrunk_inventory_bytes = canonical_json(shrunk_inventory)
        shrunk_inventory_sha = sha256(shrunk_inventory_bytes)
        projection_path.write_bytes(shrunk_projection_bytes)
        inventory_path.write_bytes(shrunk_inventory_bytes)
        shrunk_audit = json.loads(json.dumps(audit))
        shrunk_audit["member_results"].pop()
        changed_batches = {}
        changed_attempts = {}
        for row in shrunk_audit["member_results"]:
            batch_path = safe_private_reference(private_root, row["batch_path"])
            attempt_path = safe_private_reference(private_root, row["attempt_receipt_path"])
            changed_batches[batch_path] = batch_path.read_bytes()
            changed_attempts[attempt_path] = attempt_path.read_bytes()
            batch = parse_json(changed_batches[batch_path])
            batch["identity"]["inventory_sha256"] = shrunk_inventory_sha
            batch["identity"]["source_projection_sha256"] = shrunk_projection_sha
            batch_bytes = canonical_json(batch)
            batch_path.write_bytes(batch_bytes)
            attempt = parse_json(changed_attempts[attempt_path])
            attempt["batch_sha256"] = sha256(batch_bytes)
            attempt_bytes = canonical_json(attempt)
            attempt_path.write_bytes(attempt_bytes)
            row["batch_sha256"] = sha256(batch_bytes)
            row["attempt_receipt_sha256"] = sha256(attempt_bytes)
        deleted_batch = safe_private_reference(private_root, audit["member_results"][-1]["batch_path"])
        deleted_batch_bytes = deleted_batch.read_bytes()
        deleted_batch.unlink()
        shrunk_audit["outcomes"] = {name: sum(row["classification"] == name
                                               for row in shrunk_audit["member_results"])
                                    for name in ("guest-fail", "pass", "refused")}
        shrunk_audit["authenticated_batches"] = 2
        shrunk_audit["expected_members"] = 2
        shrunk_audit["inventory_sha256"] = shrunk_inventory_sha

        inventory_status, inventory_stdout, inventory_stderr = bounded_fixed_run(
            workspace, FIXED_BUILD_BINARIES["native"],
            ["--validate-inventory", str(inventory_path)],
        )
        complete_status, complete_stdout, complete_stderr = bounded_fixed_run(
            workspace, FIXED_BUILD_BINARIES["native"],
            ["--verify-complete-json", str(inventory_path), str(deleted_batch.parent)],
        )
        if inventory_status != 0 or inventory_stderr or complete_status != 3 or complete_stderr:
            refuse()
        inventory_validation_path.write_bytes(inventory_stdout)
        inventory_stderr_path.write_bytes(inventory_stderr)
        complete_path.write_bytes(complete_stdout)
        complete_stderr_path.write_bytes(complete_stderr)
        shrunk_audit["explicit_verify_complete"] = complete_stdout.decode("ascii").strip()
        shrunk_audit_bytes = canonical_json(shrunk_audit)
        audit_path.write_bytes(shrunk_audit_bytes)
        shrunk_summary = json.loads(json.dumps(private_baseline))
        shrunk_summary["inventory_sha256"] = shrunk_inventory_sha
        shrunk_summary["projection_sha256"] = shrunk_projection_sha
        shrunk_summary["audit_sha256"] = sha256(shrunk_audit_bytes)
        shrunk_summary["c_outputs"]["inventory_stdout_sha256"] = sha256(inventory_stdout)
        shrunk_summary["c_outputs"]["inventory_stderr_sha256"] = sha256(inventory_stderr)
        shrunk_summary["c_outputs"]["complete_stdout_sha256"] = sha256(complete_stdout)
        shrunk_summary["c_outputs"]["complete_stderr_sha256"] = sha256(complete_stderr)
        shrunk_summary["partition"] = derive_private_partition(
            shrunk_projection, shrunk_inventory, shrunk_audit, private_root,
            shrunk_summary["run_source_revision"], shrunk_summary["profile_id"],
            shrunk_summary["private_build_id"], owner_root, workspace, inventory_path,
        )
        _write_json(private_path, shrunk_summary)
        _expect_refusal(check_fixture)
        # With a deliberately replaced test anchor, the rest of the synthetic record is
        # accepted. This proves the original qualified-membership anchor is the blocker.
        validate(workspace, owner_root, private_root,
                 (shrunk_projection_sha, _projection_member_keys(shrunk_projection)))
        for path, data in changed_batches.items():
            path.write_bytes(data)
        for path, data in changed_attempts.items():
            path.write_bytes(data)
        deleted_batch.write_bytes(deleted_batch_bytes)
        projection_path.write_bytes(original_projection_bytes)
        inventory_path.write_bytes(original_inventory_bytes)
        audit_path.write_bytes(original_audit_bytes)
        inventory_validation_path.write_bytes(original_inventory_validation_bytes)
        inventory_stderr_path.write_bytes(original_inventory_stderr_bytes)
        complete_path.write_bytes(original_complete_bytes)
        complete_stderr_path.write_bytes(original_complete_stderr_bytes)
        _write_json(private_path, private_baseline)

        dropped_audit = json.loads(json.dumps(audit))
        dropped_audit["member_results"].pop()
        dropped_audit["outcomes"] = {name: sum(row["classification"] == name
                                                  for row in dropped_audit["member_results"])
                                       for name in ("guest-fail", "pass", "refused")}
        dropped_audit["authenticated_batches"] = len({row["batch_path"]
                                                       for row in dropped_audit["member_results"]})
        dropped_bytes = canonical_json(dropped_audit)
        audit_path.write_bytes(dropped_bytes)
        dropped_summary = json.loads(json.dumps(private_baseline))
        dropped_summary["audit_sha256"] = sha256(dropped_bytes)
        dropped_summary["partition"] = derive_private_partition(
            projection, inventory, dropped_audit, private_root,
            dropped_summary["run_source_revision"], dropped_summary["profile_id"],
            dropped_summary["private_build_id"], owner_root, workspace, inventory_path,
        )
        if dropped_summary["partition"]["missing_rows"] == 0:
            refuse()
        _write_json(private_path, dropped_summary)
        _expect_refusal(check_fixture)
        audit_path.write_bytes(original_audit_bytes)
        _write_json(private_path, private_baseline)

        # The accepted baseline records a denied redundant signal plus a
        # verified empty group. An actually nonempty group must still refuse,
        # even after its changed attempt receipt is correctly rehashed.
        cleanup_audit = json.loads(json.dumps(audit))
        cleanup_row = cleanup_audit["member_results"][0]
        cleanup_path = safe_private_reference(private_root, cleanup_row["attempt_receipt_path"])
        cleanup_before = cleanup_path.read_bytes()
        cleanup_attempt = parse_json(cleanup_before)
        cleanup_attempt["cleanup_group_empty"] = False
        cleanup_bytes = canonical_json(cleanup_attempt)
        cleanup_path.write_bytes(cleanup_bytes)
        cleanup_row["attempt_receipt_sha256"] = sha256(cleanup_bytes)
        _expect_refusal(lambda: derive_private_partition(
            projection, inventory, cleanup_audit, private_root,
            private_baseline["run_source_revision"], private_baseline["profile_id"],
            private_baseline["private_build_id"], owner_root, workspace, inventory_path,
        ))
        cleanup_path.write_bytes(cleanup_before)

        refusal_audit = json.loads(json.dumps(audit))
        refusal_row = next(row for row in refusal_audit["member_results"]
                           if row["classification"] == "refused")
        refusal_attempt_path = safe_private_reference(private_root, refusal_row["attempt_receipt_path"])
        original_attempt_bytes = refusal_attempt_path.read_bytes()
        refusal_attempt = parse_json(original_attempt_bytes)
        refusal_row["classification"] = "pass"
        refusal_row["reason"] = ""
        refusal_attempt["classification"] = "pass"
        refusal_attempt["adapter_exit"] = 0
        refusal_attempt_bytes = canonical_json(refusal_attempt)
        refusal_attempt_path.write_bytes(refusal_attempt_bytes)
        refusal_row["attempt_receipt_sha256"] = sha256(refusal_attempt_bytes)
        refusal_audit["outcomes"]["refused"] -= 1
        refusal_audit["outcomes"]["pass"] += 1
        refusal_bytes = canonical_json(refusal_audit)
        audit_path.write_bytes(refusal_bytes)
        relabeled_summary = json.loads(json.dumps(private_baseline))
        relabeled_summary["audit_sha256"] = sha256(refusal_bytes)
        _write_json(private_path, relabeled_summary)
        _expect_refusal(check_fixture)
        refusal_attempt_path.write_bytes(original_attempt_bytes)
        audit_path.write_bytes(original_audit_bytes)
        _write_json(private_path, private_baseline)

        complete_path = safe_private_reference(
            private_root, private_baseline["c_outputs"]["complete_stdout_path"]
        )
        original_complete_bytes = complete_path.read_bytes()
        claimed_complete = b"classification=complete members=3\n"
        complete_path.write_bytes(claimed_complete)
        completion_audit = json.loads(json.dumps(audit))
        completion_audit["explicit_verify_complete"] = claimed_complete.decode("ascii").strip()
        completion_audit_bytes = canonical_json(completion_audit)
        audit_path.write_bytes(completion_audit_bytes)
        completion_summary = json.loads(json.dumps(private_baseline))
        completion_summary["audit_sha256"] = sha256(completion_audit_bytes)
        completion_summary["c_outputs"]["complete_stdout_sha256"] = sha256(claimed_complete)
        _write_json(private_path, completion_summary)
        _expect_refusal(check_fixture)
        complete_path.write_bytes(original_complete_bytes)
        audit_path.write_bytes(original_audit_bytes)
        _write_json(private_path, private_baseline)

        batch_path = safe_private_reference(private_root, audit["member_results"][0]["batch_path"])
        original_batch_bytes = batch_path.read_bytes()
        batch_path.write_bytes(original_batch_bytes + b"tampered")
        _expect_refusal(check_fixture)
        batch_path.write_bytes(original_batch_bytes)

        linux_record = workspace / baseline["hosted"]["platforms"]["gate-linux"]["record_path"]
        saved_linux_record = linux_record.read_bytes()
        linux_record.unlink()
        _expect_refusal(check_fixture)
        linux_record.write_bytes(saved_linux_record)
        false_outcome = parse_json(saved_linux_record)
        false_outcome["result"]["outcome"] = "guest-fail"
        false_bytes = canonical_json(false_outcome)
        linux_record.write_bytes(false_bytes)
        candidate = json.loads(json.dumps(baseline))
        candidate["hosted"]["platforms"]["gate-linux"]["record_sha256"] = sha256(false_bytes)
        _write_json(workspace / PUBLIC_RECEIPT, candidate)
        _expect_refusal(check_fixture)
        linux_record.write_bytes(saved_linux_record)
        _write_json(workspace / PUBLIC_RECEIPT, baseline)
        bad_red = json.loads(json.dumps(private_baseline))
        bad_red["red_evidence"][0]["setup"] = "failed"
        _write_json(private_path, bad_red)
        _expect_refusal(check_fixture)
        _write_json(private_path, private_baseline)

        docs = workspace / "docs/evidence/phase05-native-rsp.md"
        old_report = docs.read_bytes()
        authored_rom_hash = baseline["composition"]["rom_sha256"]
        for private_public_text in (
            '{"classification":"refused"}\n',
            '{"rom_sha256":"' + "0" * 64 + '"}\n',
            '{"selected_rom_sha256":"' + authored_rom_hash + '"}\n',
            "private member r0001v00000\n",
        ):
            docs.write_text(private_public_text, encoding="utf-8")
            candidate = json.loads(json.dumps(baseline))
            candidate["reports"]["native_sha256"] = sha256(docs.read_bytes())
            _write_json(workspace / PUBLIC_RECEIPT, candidate)
            _expect_refusal(check_fixture)
        docs.write_bytes(old_report)

        unknown_field = json.loads(json.dumps(baseline))
        unknown_field["private_control"] = {
            "classification": "refused", "rom_sha256": authored_rom_hash,
        }
        _write_json(workspace / PUBLIC_RECEIPT, unknown_field)
        _expect_refusal(check_fixture)
        _write_json(workspace / PUBLIC_RECEIPT, baseline)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_mutually_exclusive_group(required=True)
    actions.add_argument("--selftest", action="store_true")
    actions.add_argument("--check", action="store_true")
    actions.add_argument("--last-tested-source", action="store_true")
    args = parser.parse_args()
    try:
        if args.selftest:
            run_selftest()
            print("PASS: authored RSP final-evidence controls")
            return 0
        if args.check:
            validate()
            print("PASS: final RSP evidence and artifact bindings")
            return 0
        print(latest_tested_source())
        return 0
    except (Refusal, OSError, ValueError, KeyError, TypeError,
            UnicodeDecodeError, subprocess.SubprocessError):
        if args.last_tested_source:
            print("FAIL: no validated saved successful source revision", file=sys.stderr)
        else:
            print("FAIL: final RSP evidence is incomplete or inconsistent", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
