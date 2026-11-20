#!/usr/bin/env python3
"""Phase 9 qualifier: authored pad-poll rows, hosted emission and public scan.

Provenance: a reduced copy of the hosted functions of scripts/qualify-audio.py
(kept unmodified): the helper set, the retained-suite reader, the public
projection, the emitter and the exact public member allowlist. Differences:
the observed values are result records of supervised pad-poll runs matched to
authored rows (HOSTED_ROWS), there is no capture bundle and no race part, and
the native, hosted-collection and final modes (--native, --collect-hosted,
--verify-evidence) are the reduced copies of the Phase 8 modes with the
platform-invariant pair comparison of this phase. Licensed under the repository
license (Apache-2.0).

Every observed row field is checked against authored data; no value of a row
is taken from the record alone.

The bound source tuple below is data: 09-03.1 finalizes it with every file
that 09-04 to 09-09 will create, because a later logic edit to a frozen file
costs a native plus hosted qualification cycle.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import re
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "docs/spec/phase09-cases.json"
INVENTORY_SCHEMA = "t64.phase09-case-inventory"
# No inventory hash pin here: the literal pins are data rows added in 09-11.
PUBLIC_SCHEMA = "t64.phase09-qualification"
PUBLIC_MANIFEST_MAX = 16 * 1024
PUBLIC_KEYS = frozenset({
    "schema", "schema_version", "evidence_mode", "workflow_sha", "source_revision",
    "job_name", "run_id", "run_attempt", "platform", "producer", "rows", "timing",
    "execution", "inventory",
})
HOSTED_SUITE = "hosted"
# Private originals never leave the machine and never enter a public upload.
PRIVATE_ROOT = ROOT / "build/gate/09-11-phase09-private"
RESULT_SCHEMA = "t64.result"
# Parent of the first Phase 9 commit that created core/tests/test_si_profile.c:
#   git log --diff-filter=A --format=%H -- core/tests/test_si_profile.c | tail -1
#   -> c173a5e23e8a8cc09a1c7d118c06d1cf282ad2af; its parent is the base below.
PHASE09_SOURCE_BASE = "cfe4d34bb2f6090359bcf0251bbdff93938fb1ff"
REHEARSAL_STATEMENT = "this result is a rehearsal and not final evidence"
# Hard limits of the shared native stage and of its verification (plan 09-10,
# L4 CR-01 and IN-01). The stage limits themselves live in qualify-rdp-vi.py
# (NATIVE_*_TIMEOUT_SECONDS); the derivation is in 09-REVIEW-FIX.md and the pins
# in scripts/tests/test_phase09_qualification.py.
# The outer limit of the whole shared stage is the sum of its five stage limits
# plus the cheap preflight; the record bound is the largest single stage limit.
NATIVE_SHARED_STAGE_TIMEOUT_SECONDS = 4200
NATIVE_SHARED_RECORD_MAX_TIMEOUT_SECONDS = 2400
# Upload allowlist: exactly one regular file with this name, nothing else.
PUBLIC_MEMBER = "manifest.json"
# Private artifact suffixes: state, replay and save media never leave the machine.
PRIVATE_SUFFIXES = (".sta", ".rpl", ".sav", ".eep", ".sra", ".z64")
STATE_MAGIC = b"T64STATE"
LOCAL_PATH_PATTERN = re.compile(rb"(?i)(?:/(?:users|home|private/var/folders)/[^\s\"']+|"
                                rb"\b[a-z]:\\{1,2}(?:users|documents and settings)\\{1,2}[^\s\"']+)")
EMAIL_PATTERN = re.compile(rb"(?i)[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,}")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
SHA1_RE = re.compile(r"^[0-9a-f]{40}$")
DECIMAL_RE = re.compile(r"^[0-9]+$")
CORE_BUILD_RE = re.compile(r"^t64-build-v1-[0-9a-f]{64}$")
RUNNER_BUILD_RE = re.compile(r"^t64-runner-v1-[0-9a-f]{64}$")
EMPTY_SHA256 = hashlib.sha256(b"").hexdigest()
# Named policy literals of 09-03 (software-profile policy, not hardware claims):
# consecutive poll starts are at least one handshake plus the read tail apart,
# and the data is latched one handshake after a poll starts.
POLL_PAIR_MIN_TICKS = 21067
POLL_HANDSHAKE_TICKS = 16971
POLL_COUNT = 4
GUEST_TICK_BOUND = 1_000_000
RETAINED_RUN_MARKER = re.compile(r"^private phase09 runner evidence retained at (.+)$",
                                 re.MULTILINE)
TIMING_MARKER = re.compile(
    r"^case reached: pad-poll-timing starts S1=([0-9]+) S2=([0-9]+) S3=([0-9]+) S4=([0-9]+)$",
    re.MULTILINE)
ACCEPTED_MARKER = re.compile(
    r"^case reached: pad-poll-accepted completion_tick=([0-9]+)$", re.MULTILINE)
CONTROL_MARKER = re.compile(
    r"^case reached: ([a-z0-9-]+) outcome=([a-z-]+) detail=([a-z-]+) guest_tick=([0-9]+)$",
    re.MULTILINE)
ROW_KEYS = ("row_id", "ctest", "fixture_id", "fixture_sha256", "profile_id",
            "profile_hash", "input_sha256", "expected_outcome", "expected_detail",
            "log_bytes", "log_sha256")
OBSERVED_ROW_KEYS = ("guest_tick", "state_digest")
_STEM_PROFILE = "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
_STEM_PROFILE_HASH = "661b161dfee9a10857efe3f98d840c09f05cbf0492dfde59e8ec44190dabb6d5"
_BASELINE_FIXTURE = "phase09-pad-poll-si-v1"
_BASELINE_INPUT = "e70838a961dfa08c13d9baf4cbf48fb8e3d14029c348a6e0181771b93ca8d1ba"
# Authored rows (data). Every value was derived from an independent source: the
# transcripts from fixtures/pad/poll_oracle.py, the fixture hashes from the
# fixture manifest and the built files, the profile hash from the runner table,
# and the input hashes from the retained input scripts with their stamps
# derived from the measured poll starts. Later plans append rows only.
HOSTED_ROWS: tuple[dict[str, Any], ...] = (
    {
        "row_id": "accepted-baseline",
        "ctest": "runner.pad-poll",
        "fixture_id": _BASELINE_FIXTURE,
        "fixture_sha256": "29a005be19b2f86e866d00cfb3ab2f8aac39e376d85a5e5d24e78c113a80dab6",
        "profile_id": _STEM_PROFILE,
        "profile_hash": _STEM_PROFILE_HASH,
        "input_sha256": _BASELINE_INPUT,
        "expected_outcome": "pass",
        "expected_detail": "positive-match",
        "log_bytes": 68,
        "log_sha256": "715aa3854b455dc9fd8e038e1aa27e5d200ea311acbfd032930cdc760498d3f6",
    },
    {
        "row_id": "shifted-script",
        "ctest": "controls.real.pad-wrong-value",
        "fixture_id": _BASELINE_FIXTURE,
        "fixture_sha256": "29a005be19b2f86e866d00cfb3ab2f8aac39e376d85a5e5d24e78c113a80dab6",
        "profile_id": _STEM_PROFILE,
        "profile_hash": _STEM_PROFILE_HASH,
        "input_sha256": "4c830af94b3e98080f2e1b5da75581599a2523fe2df60d45b06beabc0f35c979",
        "expected_outcome": "guest-fail",
        "expected_detail": "wrong-transcript",
        "log_bytes": 68,
        "log_sha256": "4c78899114ba1e1d4111c02a84a059e3943d6c3d00208c533bbc3e7faeb07910",
    },
    {
        "row_id": "neutral-script",
        "ctest": "controls.real.pad-wrong-value",
        "fixture_id": _BASELINE_FIXTURE,
        "fixture_sha256": "29a005be19b2f86e866d00cfb3ab2f8aac39e376d85a5e5d24e78c113a80dab6",
        "profile_id": _STEM_PROFILE,
        "profile_hash": _STEM_PROFILE_HASH,
        "input_sha256": "6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d",
        "expected_outcome": "guest-fail",
        "expected_detail": "wrong-transcript",
        "log_bytes": 68,
        "log_sha256": "caf55c819271e372f4e4185520a49ed74c36d79c870bfa30d1dbc064c9f9f137",
    },
    {
        "row_id": "wrong-rom",
        "ctest": "controls.real.pad-wrong-value",
        "fixture_id": "phase09-pad-poll-wrong-si-v1",
        "fixture_sha256": "84bacc101384d52b109a4c41e4b4fac340143a89ad79a4fc4460c5890923a8f6",
        "profile_id": _STEM_PROFILE,
        "profile_hash": _STEM_PROFILE_HASH,
        "input_sha256": "330be83d0a0bf653375cdb71a5332ac82a6bc2f077e3c63a38b634d91b0a56f2",
        "expected_outcome": "guest-fail",
        "expected_detail": "wrong-transcript",
        "log_bytes": 68,
        "log_sha256": "07de0176dbe67a8c313b1871e019b588d00793e8a4002fae4ce3f6cecf333315",
    },
    {
        # The baseline ROM with its last byte flipped: not a registered fixture,
        # so the supervisor reports no fixture identity and runs no guest.
        "row_id": "replacement-refused",
        "ctest": "controls.real.pad-wrong-value",
        "fixture_id": "phase09-pad-poll-si-v1-altered-copy",
        "fixture_sha256": "96e34dfa34819ee022f83f1e0a9e265f5ffb719ee32cbf2c25408568905cc760",
        "profile_id": _STEM_PROFILE,
        "profile_hash": _STEM_PROFILE_HASH,
        "input_sha256": _BASELINE_INPUT,
        "expected_outcome": "harness-error",
        "expected_detail": "refusal",
        "log_bytes": 0,
        "log_sha256": EMPTY_SHA256,
    },
)
SOURCE_PATHS = (
    "CMakeLists.txt", "core", "runner",
    "tools/manifest", "tools/rights", "fixtures",
    "scripts/check-phase09-cases.py", "scripts/check-rsp-no-hle.py",
    "scripts/check-test-inventory.sh", "scripts/check-ci-yml.sh",
    "scripts/rights-scan.sh", "scripts/pii-scan.sh",
    "scripts/check-provenance.sh", "scripts/check-claims.sh",
    "scripts/gate.sh",
    "scripts/list-membership-sites.py", "docs/spec/profile-membership-sites.txt",
    "scripts/qualify-phase09.py", "scripts/tests/test_phase09_qualification.py",
    "scripts/tests/test_rsp_no_hle.py",
    "docs/spec/phase09-cases.json", ".github/workflows/ci.yml",
    "scripts/build-phase09-admit-fixture.sh", "scripts/build-pad-fixture.sh",
    "scripts/collect-boot-evidence.sh", "scripts/qualify-rdp-vi.py",
    "scripts/qualify-audio.py", "scripts/check-audio-cases.py",
    "scripts/build-boot-fixtures.sh",
)
# Files later plans create, bound by the final source tuple. A native or hosted
# record made before they exist lists them as absent and is only a rehearsal.
PENDING_SOURCE_PATHS: dict[str, str] = {
    "scripts/build-save-fixtures.sh": "09-04",
    "scripts/build-replay-fixture.sh": "09-07",
    "scripts/check-state-field-use.py": "09-09",
    "scripts/k5-mutants.py": "09-06",
    "docs/spec/state-field-rules.txt": "09-09.1",
    "docs/spec/state-mutants.txt": "09-09.1",
}
OWNER_METADATA_PATHS = ("AGENTS.md", ".planning/config.json", ".planning/AUTONOMY.md")


class Refusal(ValueError):
    """Raised when evidence is missing, changed or unbound."""


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise Refusal(message)


def _read(path: Path, limit: int = 64 * 1024 * 1024) -> bytes:
    try:
        metadata = path.lstat()
    except OSError as error:
        raise Refusal(f"required original file is unavailable: {path.name}") from error
    if path.is_symlink() or not path.is_file() or metadata.st_size < 0 \
            or metadata.st_size > limit:
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


def _under(path: Path, parent: Path) -> bool:
    try:
        path.resolve().relative_to(parent.resolve())
        return True
    except (OSError, ValueError):
        return False


def _unique_nonempty(values: object, label: str) -> list[str]:
    if not isinstance(values, list) or not values or any(
            not isinstance(value, str) or not value for value in values):
        raise Refusal(f"{label} must be a nonempty string list")
    if len(values) != len(set(values)):
        raise Refusal(f"{label} contains duplicate entries")
    return list(values)


def _safe_relative(value: object, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise Refusal(f"{label} must be a nonempty relative path")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts or "." in path.parts:
        raise Refusal(f"{label} must stay inside the build directory")
    return path.as_posix()


def _host_platform() -> str:
    if sys.platform == "darwin":
        return "macos"
    if sys.platform.startswith("linux"):
        return "linux"
    raise Refusal("projection host platform is unsupported")


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
    _require(not any(child.tag in {"failure", "error", "skipped"} for child in tests[0]),
             f"JUnit records {name} as failed or skipped")


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
    absent_pending = sorted(name for name in PENDING_SOURCE_PATHS
                            if not (ROOT / name).exists() and not (ROOT / name).is_symlink())
    bound = [*SOURCE_PATHS, *(name for name in PENDING_SOURCE_PATHS
                              if name not in absent_pending)]
    status = _git(["status", "--porcelain=v1", "-z", "--untracked-files=all", "--", *bound])
    _require(not status or not require_clean,
             "hosted source inputs differ from the selected clean commit")
    listed = _git(["ls-files", "--cached", "--others", "--exclude-standard", "-z", "--",
                   *bound])
    paths = sorted({value.decode("utf-8", errors="strict")
                    for value in listed.split(b"\0") if value})
    _require(bool(paths), "source input inventory is empty")
    entries = []
    for relative in paths:
        path = ROOT / relative
        metadata = path.lstat()
        _require(path.is_file() and not path.is_symlink(),
                 "source input is not a regular file")
        data = _read(path)
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
        "absent_pending": absent_pending,
    }


def _owner_metadata_snapshot() -> list[dict[str, Any]]:
    """Record known non-build owner inputs without treating them as source code."""
    entries = []
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
        entries.append({"path": relative, "present": True, "dirty_status": status,
                        "size_bytes": len(data), "sha256": _sha(data)})
    return entries


# ---- case inventory ---------------------------------------------------------

def _parse_inventory(data: bytes) -> dict[str, Any]:
    try:
        inventory = json.loads(data)
    except (json.JSONDecodeError, UnicodeDecodeError) as error:
        raise Refusal(f"case inventory is not valid JSON: {error}") from error
    _require(isinstance(inventory, dict), "case inventory must be a JSON object")
    _require(inventory.get("schema") == INVENTORY_SCHEMA
             and inventory.get("schema_version") == 1,
             "unsupported case inventory schema")
    suites = inventory.get("suites")
    _require(isinstance(suites, dict), "case inventory suites must be an object")
    _require(len(suites) > 0, "case inventory has zero suites")
    cases_total = 0
    for name, suite in suites.items():
        _require(isinstance(name, str) and bool(name) and isinstance(suite, dict),
                 "suite entries must be named objects")
        _unique_nonempty(suite.get("ctest"), f"suite {name} ctest names")
        binaries = suite.get("binaries")
        _require(isinstance(binaries, list) and len(binaries) > 0,
                 f"suite {name} binaries must be a nonempty list")
        seen: set[str] = set()
        for entry in binaries:
            _require(isinstance(entry, dict), f"suite {name} binary entries must be objects")
            relative = _safe_relative(entry.get("path"), f"suite {name} binary path")
            _require(relative not in seen,
                     f"suite {name} contains duplicate binary path {relative}")
            seen.add(relative)
            cases_total += len(_unique_nonempty(entry.get("cases"), f"{relative} cases"))
    _require(cases_total > 0, "case inventory has zero cases")
    return inventory


def validate_case_inventory(path: Path) -> dict[str, int]:
    """Accept only a nonempty, well-formed case inventory; zero cases are refused."""
    inventory = _parse_inventory(_read(Path(path), 1024 * 1024))
    suites = inventory["suites"]
    ctests = sum(len(suite["ctest"]) for suite in suites.values())
    cases = sum(len(entry["cases"]) for suite in suites.values()
                for entry in suite["binaries"])
    return {"suites": len(suites), "ctest": ctests, "cases": cases}


def inventory_snapshot(data: bytes, suite: str) -> dict[str, Any]:
    """Hash of the inventory bytes plus the counts of one named suite."""
    inventory = _parse_inventory(data)
    selected = inventory["suites"].get(suite)
    _require(isinstance(selected, dict), f"unknown qualification suite: {suite}")
    return {
        "snapshot_sha256": _sha(data),
        "suite": suite,
        "case_count": sum(len(entry["cases"]) for entry in selected["binaries"]),
        "ctest_count": len(selected["ctest"]),
    }


def _inventory_summary(data: bytes) -> dict[str, Any]:
    """Hash and counts over every suite of the inventory (the native denominator)."""
    suites = _parse_inventory(data)["suites"]
    return {
        "snapshot_sha256": _sha(data),
        "suite_count": len(suites),
        "ctest_count": len({name for suite in suites.values() for name in suite["ctest"]}),
        "case_count": len({(entry["path"], case) for suite in suites.values()
                           for entry in suite["binaries"] for case in entry["cases"]}),
    }


def _rehearsal_gate(absent_pending: list[str], rehearsal: bool) -> str | None:
    """Absent pending files make a record a rehearsal; final evidence needs none."""
    if absent_pending:
        _require(rehearsal,
                 "pending source paths are absent (" + ", ".join(absent_pending) +
                 "); the record is not final evidence, use --rehearsal for a labelled rehearsal")
    return REHEARSAL_STATEMENT if rehearsal else None


# ---- authored rows and observations ------------------------------------------

def _u64(value: object) -> int | None:
    if isinstance(value, str) and DECIMAL_RE.fullmatch(value) is not None and len(value) < 20:
        return int(value)
    return None


def row_observation(result: Any, run_input_sha256: str, row: dict[str, Any]) -> dict[str, Any]:
    """Check one supervised result record against its authored row.

    Returns the model-time observations (guest tick, state digest) and the build
    identity of the record; every other field must equal the authored value.
    """
    row_id = row["row_id"]

    def differs(name: str) -> Refusal:
        return Refusal(f"row {row_id}: result {name} differs from the authored row")

    _require(isinstance(result, dict) and result.get("schema") == RESULT_SCHEMA
             and result.get("schema_version") == 2 and isinstance(result.get("counts"), dict)
             and isinstance(result.get("guest_stop"), dict),
             f"row {row_id}: result record is not the v2 shape")
    if result.get("outcome") != row["expected_outcome"]:
        raise differs("outcome")
    if result.get("detail") != row["expected_detail"]:
        raise differs("detail")
    if _u64(result["counts"].get("log_bytes")) != row["log_bytes"]:
        raise differs("log_bytes")
    tick = _u64(result.get("guest_tick"))
    if tick is None or tick > GUEST_TICK_BOUND:
        raise differs("guest_tick")
    if run_input_sha256 != row["input_sha256"]:
        raise differs("input_sha256")
    if row["expected_outcome"] == "harness-error":
        for name, key in (("fixture_id", "fixture_id"), ("fixture_sha256", "fixture_sha256"),
                          ("profile_id", "profile_id"), ("profile_hash", "profile_digest"),
                          ("input_sha256", "input_digest"), ("log_sha256", "log_sha256"),
                          ("state_digest", "state_digest")):
            if result.get(key) is not None:
                raise differs(name)
        if tick != 0:
            raise differs("guest_tick")
        if result["guest_stop"].get("kind") != "refused":
            raise differs("guest_stop")
        if row["log_sha256"] != EMPTY_SHA256:
            raise differs("log_sha256")
        return {"guest_tick": 0, "state_digest": None, "build_id": None,
                "source_revision": None}
    for name, key, expected in (
            ("fixture_id", "fixture_id", row["fixture_id"]),
            ("fixture_sha256", "fixture_sha256", row["fixture_sha256"]),
            ("profile_id", "profile_id", row["profile_id"]),
            ("profile_hash", "profile_digest", row["profile_hash"]),
            ("input_sha256", "input_digest", row["input_sha256"]),
            ("log_sha256", "log_sha256", row["log_sha256"])):
        if result.get(key) != expected:
            raise differs(name)
    if tick == 0:
        raise differs("guest_tick")
    if result["guest_stop"].get("kind") != "authored-completion":
        raise differs("guest_stop")
    state = result.get("state_digest")
    if not isinstance(state, str) or SHA256_RE.fullmatch(state) is None:
        raise differs("state_digest")
    build_id = result.get("build_id")
    if not isinstance(build_id, str) or CORE_BUILD_RE.fullmatch(build_id) is None:
        raise differs("build_id")
    revision = result.get("source_revision")
    if not isinstance(revision, str) or SHA1_RE.fullmatch(revision) is None:
        raise differs("source_revision")
    return {"guest_tick": tick, "state_digest": state, "build_id": build_id,
            "source_revision": revision}


def _run_option(argv: list[str], name: str) -> str:
    _require(argv.count(name) == 1, f"retained run command does not carry exactly one {name}")
    index = argv.index(name)
    _require(index + 1 < len(argv), f"retained run command has no value for {name}")
    return argv[index + 1]


def _run_record(directory: Path, build: Path) -> dict[str, Any]:
    """Read one retained supervised run: result, exact command and input script."""
    directory = Path(directory)
    _require(directory.is_dir() and not directory.is_symlink(),
             "retained run directory is missing or unsafe")
    result = _json(directory / "result.json", 16 * 1024)
    argv = _read(directory / "command.args", 16 * 1024).decode("utf-8").splitlines()
    input_bytes = _read(directory / "input.json", 16 * 1024)
    supervisor = str((build / "runner/t64-supervisor").resolve())
    _require(bool(argv) and argv[0] == supervisor,
             "retained run command does not name the production supervisor of the owning build")
    _require(Path(_run_option(argv, "--result")).resolve() ==
             (directory / "result.json").resolve() and
             Path(_run_option(argv, "--work-dir")).resolve() == directory.resolve(),
             "retained run command was not produced for this directory")
    input_sha = _sha(input_bytes)
    _require(_run_option(argv, "--input-sha256") == input_sha,
             "retained input script differs from the digest the run declared")
    fixture = Path(_run_option(argv, "--fixture")).resolve()
    _require(_under(fixture, ROOT / "build"),
             "retained run fixture is outside the build tree")
    try:
        events = json.loads(input_bytes)["events"]
    except (UnicodeDecodeError, json.JSONDecodeError, KeyError, TypeError) as error:
        raise Refusal("retained input script is not a valid schedule") from error
    _require(isinstance(events, list), "retained input script has no event list")
    return {"result": result, "input_sha256": input_sha, "events": events,
            "fixture_sha256": _file_sha(fixture), "argv": argv,
            "directory": str(directory.resolve())}


def _match_runs(runs: list[dict[str, Any]],
                rows: tuple[dict[str, Any], ...]) -> list[dict[str, Any]]:
    """Each retained run matches exactly one row and every row is matched once."""
    keys = {(row["fixture_sha256"], row["input_sha256"]): row for row in rows}
    _require(len(keys) == len(rows) and
             len({row["row_id"] for row in rows}) == len(rows),
             "authored rows are not unique by fixture and input")
    matched: dict[str, dict[str, Any]] = {}
    for run in runs:
        row = keys.get((run["fixture_sha256"], run["input_sha256"]))
        _require(row is not None, "a retained run matches no authored row")
        assert row is not None
        _require(row["row_id"] not in matched,
                 f"row {row['row_id']} is matched by more than one retained run")
        _require(row["ctest"] == run["ctest"],
                 f"row {row['row_id']} was observed under the wrong CTest")
        matched[row["row_id"]] = row_observation(run["result"], run["input_sha256"], row)
    missing = [row["row_id"] for row in rows if row["row_id"] not in matched]
    _require(not missing, "authored rows without a retained run: " + ", ".join(missing))
    return [matched[row["row_id"]] for row in rows]


def _timing(section: str, baseline_tick: int,
            baseline_events: list[Any]) -> dict[str, Any]:
    starts = TIMING_MARKER.findall(section)
    accepted = ACCEPTED_MARKER.findall(section)
    _require(len(starts) == 1 and len(accepted) == 1,
             "poll timing needs exactly one timing line and one accepted line")
    ticks = [int(value) for value in starts[0]]
    _require(len(ticks) == POLL_COUNT and
             all(later - earlier >= POLL_PAIR_MIN_TICKS
                 for earlier, later in zip(ticks, ticks[1:])),
             "poll starts are not strictly increasing by at least one poll period")
    completion = int(accepted[0])
    _require(completion == baseline_tick and
             ticks[-1] + POLL_PAIR_MIN_TICKS <= completion <= GUEST_TICK_BOUND,
             "completion tick differs from the accepted run or leaves its range")
    stamps = [ticks[0] + POLL_HANDSHAKE_TICKS - 1, ticks[1] + POLL_HANDSHAKE_TICKS,
              ticks[2] + POLL_HANDSHAKE_TICKS + 1]
    _require([_u64(event.get("tick")) if isinstance(event, dict) else None
              for event in baseline_events] == stamps,
             "baseline input stamps differ from the ones derived from the measured starts")
    return {"poll_starts": ticks, "completion_tick": completion}


# ---- retained suite ----------------------------------------------------------

def _observe_sections(build: Path, sections: dict[str, str], case_names: set[str],
                      rows: tuple[dict[str, Any], ...] = HOSTED_ROWS) -> dict[str, Any]:
    """Match the retained runs named in passed CTest sections to the authored rows."""
    runs: list[dict[str, Any]] = []
    seen: set[str] = set()
    for ctest, section in sections.items():
        for value in RETAINED_RUN_MARKER.findall(section):
            directory = Path(value).resolve()
            _require(_under(directory, build) and str(directory) not in seen,
                     "retained run directory is outside the owning build or repeated")
            seen.add(str(directory))
            record = _run_record(directory, build)
            record["ctest"] = ctest
            runs.append(record)
    observations = _match_runs(runs, rows)
    by_row = {row["row_id"]: (row, observed) for row, observed in zip(rows, observations)}
    baseline_row, baseline_observed = by_row["accepted-baseline"]
    baseline_run = next(run for run in runs
                        if (run["fixture_sha256"], run["input_sha256"]) ==
                        (baseline_row["fixture_sha256"], baseline_row["input_sha256"]))
    timing = _timing(sections[baseline_row["ctest"]], baseline_observed["guest_tick"],
                     baseline_run["events"])
    # Every control run prints one outcome line; they equal the observations.
    controls = sorted(
        (row["expected_outcome"], row["expected_detail"], str(observed["guest_tick"]))
        for row, observed in zip(rows, observations) if row["expected_outcome"] != "pass")
    printed = CONTROL_MARKER.findall("\n".join(sections.values()))
    _require(all(name in case_names for name, *_ in printed),
             "a control line names a case outside the hosted inventory")
    _require(sorted((outcome, detail, tick) for _, outcome, detail, tick in printed) == controls,
             "control outcome lines differ from the observed retained runs")
    real = [item for item in observations if item["build_id"] is not None]
    _require(bool(real) and all(
        item["build_id"] == real[0]["build_id"] and
        item["source_revision"] == real[0]["source_revision"] for item in real),
        "accepted runs disagree on the core build or source revision")
    binaries = {name: _file_sha(build / name) for name in (
        "runner/t64-runner-real", "runner/t64-supervisor", "runner/t64-result-check",
        "runner/test_pad_poll", "core/libt64_core.a")}
    runner_bytes = _read(build / "runner/t64-runner-real")
    runner_ids = {match.decode("ascii") for match in
                  re.findall(rb"t64-runner-v1-[0-9a-f]{64}", runner_bytes)}
    _require(len(runner_ids) == 1, "real runner binary does not carry exactly one build identity")
    _require(real[0]["build_id"].encode("ascii") in runner_bytes,
             "core build identity is not present in the real runner binary")
    return {
        "observations": [{key: item[key] for key in OBSERVED_ROW_KEYS} for item in observations],
        "timing": timing,
        "source_revision": real[0]["source_revision"],
        "producer": {
            "source_revision": real[0]["source_revision"],
            "core_build_id": real[0]["build_id"],
            "runner_build_id": next(iter(runner_ids)),
            "real_runner_sha256": binaries["runner/t64-runner-real"],
            "supervisor_sha256": binaries["runner/t64-supervisor"],
            "checker_sha256": binaries["runner/t64-result-check"],
            "test_binary_sha256": binaries["runner/test_pad_poll"],
            "core_library_sha256": binaries["core/libt64_core.a"],
        },
        "runs": runs,
        "binary_names": sorted(binaries),
    }


def _artifact(build: Path, prefix: str, suite: str, suffix: str) -> Path:
    pattern = re.compile(re.escape(f"{prefix}-{suite}-") + r"[0-9]+-[0-9]+-" + re.escape(suffix))
    matches = sorted(path for path in build.iterdir() if pattern.fullmatch(path.name))
    _require(len(matches) == 1,
             f"explicit {suite} artifact prefix does not select exactly one {suffix}")
    return matches[0]


def _suite_records(build: Path, suite: str, prefix: str,
                   rows: tuple[dict[str, Any], ...] = HOSTED_ROWS) -> dict[str, Any]:
    """Read the retained originals of one hosted suite run (no capture, no race)."""
    _require(suite == HOSTED_SUITE, f"unknown authored qualification suite: {suite}")
    inventory_data = _read(INVENTORY, 1024 * 1024)
    snapshot = inventory_snapshot(inventory_data, suite)
    selected = _parse_inventory(inventory_data)["suites"][suite]
    expected_ctests = selected["ctest"]
    case_names = {case for entry in selected["binaries"] for case in entry["cases"]}
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
    _require(_json(build_command_path, 16 * 1024) ==
             ["cmake", "--build", str(build), "--parallel", "1"],
             "owning checker build command differs from the explicit build tree")
    discovery = _json(discovery_path, 8 * 1024 * 1024)
    _require(isinstance(discovery, dict) and isinstance(discovery.get("tests"), list),
             "original CTest discovery record is malformed")
    discovered = [entry.get("name") for entry in discovery["tests"] if isinstance(entry, dict)]
    _require(len(discovered) == len(set(discovered)) and
             set(expected_ctests).issubset(set(discovered)),
             "registered authored CTest denominator was absent from original discovery")
    _require(_json(source_command_path, 16 * 1024) ==
             [sys.executable, "-B", str((ROOT / "scripts/check-rsp-no-hle.py").resolve()),
              "--build", str(build)] and
             _read(source_stdout_path, 4 * 1024 * 1024).startswith(b"PASS: rsp-no-hle"),
             "original source/body/object gate is absent or failed")
    last_bytes = _read(last_path, 32 * 1024 * 1024)
    last_text = last_bytes.decode("utf-8", errors="strict")
    sections = {name: _test_section(last_text, name) for name in expected_ctests}
    try:
        junit_root = ET.fromstring(_read(junit_path, 8 * 1024 * 1024))
    except ET.ParseError as error:
        raise Refusal("original CTest JUnit is malformed") from error
    junit_names = [entry.attrib.get("name") for entry in junit_root.iter("testcase")]
    _require(len(junit_names) == len(set(junit_names)) and
             set(junit_names) == set(expected_ctests),
             "original JUnit denominator differs from the exact selected CTest suite")
    for name in expected_ctests:
        _junit_passed(junit_path, name)
    observed = _observe_sections(build, sections, case_names, rows)
    command_sha = _sha(json.dumps(suite_command, separators=(",", ":")).encode())
    runs = observed["runs"]
    return {
        "platform": _host_platform(),
        "source_revision": observed["source_revision"],
        "observations": observed["observations"],
        "timing": observed["timing"],
        "producer": observed["producer"],
        "execution": {
            "suite": suite, "ctests": list(expected_ctests), "command_sha256": command_sha,
            "last_test_sha256": _sha(last_bytes), "junit_sha256": _file_sha(junit_path),
            "return_code": 0, "status": "pass", "timed_out": False,
        },
        "inventory": snapshot,
        "private": {
            "last_test_path": str(last_path), "junit_path": str(junit_path),
            "run_directories": [run["directory"] for run in runs],
            "fixture_paths": sorted({_run_option(run["argv"], "--fixture") for run in runs}),
            "checker_artifact_paths": [str(path) for path in (
                command_path, exit_path, discovery_path, source_command_path,
                source_stdout_path, build_command_path)],
            "binary_paths": observed["binary_names"],
        },
    }


# ---- public projection -------------------------------------------------------

def _projection(summary: dict[str, Any], *, job: str, workflow_sha: str | None,
                run_id: str | None, attempt: str | None) -> dict[str, Any]:
    _require(len(summary["observations"]) == len(HOSTED_ROWS),
             "projection observations differ in count from the authored rows")
    rows = []
    for row, observed in zip(HOSTED_ROWS, summary["observations"]):
        public = dict(row)
        public.update({key: observed[key] for key in OBSERVED_ROW_KEYS})
        rows.append(public)
    return {
        "schema": PUBLIC_SCHEMA,
        "schema_version": 1,
        "evidence_mode": "entry-path-validation" if job == "local" else "hosted-gate",
        "workflow_sha": workflow_sha,
        "source_revision": summary["source_revision"],
        "job_name": job,
        "run_id": run_id,
        "run_attempt": attempt,
        "platform": summary["platform"],
        "producer": dict(summary["producer"]),
        "rows": rows,
        "timing": dict(summary["timing"]),
        "execution": dict(summary["execution"]),
        "inventory": dict(summary["inventory"]),
    }


def _is_sha256(value: object) -> bool:
    return isinstance(value, str) and SHA256_RE.fullmatch(value) is not None


def public_projection(value: Any, expected: dict[str, Any] | None = None, *,
                      rows: tuple[dict[str, Any], ...] | None = None) -> dict[str, Any]:
    """Validate one public manifest against the closed key set and its context."""
    authored = HOSTED_ROWS if rows is None else rows
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
    platform = value.get("platform")
    _require(platform in {"linux", "macos"}, "projection host platform is unsupported")
    if job != "local":
        _require(platform == ("linux" if job == "gate-linux" else "macos"),
                 "hosted platform does not match the exact workflow job")
    public_rows = value.get("rows")
    _require(isinstance(public_rows, list), "projection rows must be a list")
    _require(len(public_rows) > 0, "projection has zero rows")
    _require(len(public_rows) == len(authored),
             "projection rows differ from the authored rows")
    for public, row in zip(public_rows, authored):
        _require(isinstance(public, dict) and
                 set(public) == set(ROW_KEYS) | set(OBSERVED_ROW_KEYS) and
                 {key: public[key] for key in ROW_KEYS} == row,
                 "projection rows differ from the authored rows")
        tick = public["guest_tick"]
        _require(type(tick) is int and 0 <= tick <= GUEST_TICK_BOUND,
                 "projection row guest tick is malformed")
        if row["expected_outcome"] == "harness-error":
            _require(tick == 0 and public["state_digest"] is None,
                     "projection refusal row carries a guest tick or state digest")
        else:
            _require(tick > 0 and _is_sha256(public["state_digest"]),
                     "projection row lacks its guest tick or state digest")
    timing = value.get("timing")
    baseline = next((public for public in public_rows
                     if public["row_id"] == "accepted-baseline"), None)
    _require(isinstance(timing, dict) and set(timing) == {"poll_starts", "completion_tick"} and
             isinstance(timing.get("poll_starts"), list) and
             len(timing["poll_starts"]) == POLL_COUNT and
             all(type(tick) is int and tick > 0 for tick in timing["poll_starts"]) and
             all(later - earlier >= POLL_PAIR_MIN_TICKS for earlier, later in
                 zip(timing["poll_starts"], timing["poll_starts"][1:])) and
             type(timing.get("completion_tick")) is int and baseline is not None and
             timing["completion_tick"] == baseline["guest_tick"] and
             timing["poll_starts"][-1] + POLL_PAIR_MIN_TICKS <= timing["completion_tick"]
             <= GUEST_TICK_BOUND,
             "projection poll timing is malformed or disagrees with the accepted row")
    producer = value.get("producer")
    _require(isinstance(producer, dict) and set(producer) == {
        "source_revision", "core_build_id", "runner_build_id", "real_runner_sha256",
        "supervisor_sha256", "checker_sha256", "test_binary_sha256", "core_library_sha256",
    } and all(_is_sha256(producer.get(key)) for key in (
        "real_runner_sha256", "supervisor_sha256", "checker_sha256",
        "test_binary_sha256", "core_library_sha256")) and
        producer.get("source_revision") == source_revision and
        isinstance(producer.get("core_build_id"), str) and
        CORE_BUILD_RE.fullmatch(producer["core_build_id"]) is not None and
        isinstance(producer.get("runner_build_id"), str) and
        RUNNER_BUILD_RE.fullmatch(producer["runner_build_id"]) is not None,
        "projection producer fields differ from the bounded allowlist")
    execution = value.get("execution")
    _require(isinstance(execution, dict) and set(execution) == {
        "suite", "ctests", "command_sha256", "last_test_sha256", "junit_sha256",
        "return_code", "status", "timed_out",
    } and execution.get("suite") == HOSTED_SUITE and
        isinstance(execution.get("ctests"), list) and bool(execution["ctests"]) and
        len(execution["ctests"]) == len(set(execution["ctests"])) and
        all(isinstance(name, str) for name in execution["ctests"]) and
        all(row["ctest"] in execution["ctests"] for row in authored) and
        all(_is_sha256(execution.get(key)) for key in (
            "command_sha256", "last_test_sha256", "junit_sha256")) and
        execution.get("return_code") == 0 and execution.get("status") == "pass" and
        execution.get("timed_out") is False,
        "projection process result is stale, failed, or malformed")
    inventory = value.get("inventory")
    _require(isinstance(inventory, dict) and set(inventory) == {
        "snapshot_sha256", "suite", "case_count", "ctest_count",
    } and _is_sha256(inventory.get("snapshot_sha256")) and
        inventory.get("suite") == HOSTED_SUITE and
        type(inventory.get("case_count")) is int and inventory["case_count"] > 0 and
        type(inventory.get("ctest_count")) is int and inventory["ctest_count"] > 0,
        "projection source denominator is empty or malformed")
    if expected is not None:
        _require(workflow_sha == expected.get("sha") and job == expected.get("job") and
                 run_id == expected.get("run") and attempt == expected.get("attempt"),
                 "public projection differs from the exact supplied run context")
        if expected.get("inventory") is not None:
            _require(inventory == expected["inventory"],
                     "projection denominator differs from the authored inventory")
    return value


def scan_public_members(directory: Path, expected: dict[str, Any] | None = None, *,
                        rows: tuple[dict[str, Any], ...] | None = None) -> dict[str, Any]:
    """Exact upload allowlist: one regular manifest.json and nothing else.

    Refuses nested directories, symlinks, members with a private suffix,
    manifest bytes that begin with the state container magic or contain a local
    path or a personal identifier, and an oversized or non-object manifest. With
    a caller context the manifest must also be a valid public projection; the
    authored rows default to HOSTED_ROWS and can be given for an older record.
    """
    directory = Path(directory)
    _require(directory.is_dir() and not directory.is_symlink(),
             "public evidence directory is absent or unsafe")
    manifest: Path | None = None
    for entry in sorted(directory.iterdir(), key=lambda item: item.name):
        _require(not entry.is_symlink(),
                 f"public upload contains a symlink: {entry.name}")
        _require(not entry.is_dir(),
                 f"public upload contains a nested directory: {entry.name}")
        _require(not entry.name.lower().endswith(PRIVATE_SUFFIXES),
                 f"public upload contains a private-suffix member: {entry.name}")
        _require(entry.name == PUBLIC_MEMBER and entry.is_file(),
                 f"public upload contains an unexpected member: {entry.name}")
        manifest = entry
    _require(manifest is not None, "public upload has no manifest.json")
    assert manifest is not None
    _require(manifest.lstat().st_size <= PUBLIC_MANIFEST_MAX,
             "public manifest exceeds its bounded size")
    data = _read(manifest, PUBLIC_MANIFEST_MAX)
    _require(not data.startswith(STATE_MAGIC),
             "public manifest begins with the private state container magic")
    _require(LOCAL_PATH_PATTERN.search(data) is None,
             "public manifest contains a local path")
    _require(EMAIL_PATTERN.search(data) is None,
             "public manifest contains a personal identifier")
    try:
        value = json.loads(data)
    except (json.JSONDecodeError, UnicodeDecodeError) as error:
        raise Refusal(f"public manifest is not valid JSON: {error}") from error
    _require(isinstance(value, dict), "public manifest must be a JSON object")
    if expected is not None:
        public_projection(value, expected, rows=rows)
    return value


# ---- private originals and emission ------------------------------------------

def _safe_copy(source: Path, destination: Path) -> None:
    _require(not source.is_symlink(), "private original contains a symlink")
    if source.is_dir():
        destination.mkdir(mode=0o700)
        for child in sorted(source.iterdir()):
            _safe_copy(child, destination / child.name)
        return
    data = _read(source)
    destination.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    with destination.open("xb") as stream:
        stream.write(data)
    destination.chmod(source.stat().st_mode & 0o777)


def _retain_originals(private: Path, build: Path, summary: dict[str, Any],
                      source_snapshot: dict[str, Any] | None) -> None:
    originals = private / "originals"
    originals.mkdir(mode=0o700)
    held = summary["private"]
    _safe_copy(Path(held["last_test_path"]), originals / "LastTest.log")
    _safe_copy(Path(held["junit_path"]), originals / "suite-junit.xml")
    for index, directory in enumerate(held["run_directories"]):
        _safe_copy(Path(directory), originals / f"run-{index:02d}")
    for index, path_text in enumerate(held["fixture_paths"]):
        _safe_copy(Path(path_text), originals / f"inputs/{index:02d}-{Path(path_text).name}")
    for name in held["binary_paths"]:
        _safe_copy(build / name, originals / "bin" / Path(name).name)
    for index, path_text in enumerate(held["checker_artifact_paths"]):
        path = Path(path_text)
        if path.is_file() and not path.is_symlink():
            _safe_copy(path, originals / f"checker/{index:02d}-{path.name}")
    binding_name = "source-binding.json" if source_snapshot is not None else "entry-path-original.json"
    binding = source_snapshot if source_snapshot is not None else {
        "evidence_mode": "entry-path-validation",
        "source_revision": summary["source_revision"],
        "core_build_id": summary["producer"]["core_build_id"],
        "runner_build_id": summary["producer"]["runner_build_id"],
        "runner_binary_sha256": summary["producer"]["real_runner_sha256"],
    }
    (private / binding_name).write_text(
        json.dumps(binding, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")
    (private / binding_name).chmod(0o600)


def _emit_hosted(*, job: str, commit_sha: str | None, run_id: str | None,
                 attempt: str | None, build_dir: Path, suite: str,
                 artifact_prefix: str, output_dir: Path) -> dict[str, Any]:
    build = Path(build_dir).resolve()
    output = Path(output_dir).resolve()
    _require(build.is_dir() and _under(build, ROOT / "build"),
             "explicit build directory must be inside the repository build tree")
    _require(re.fullmatch(r"[A-Za-z0-9._-]{1,120}", artifact_prefix) is not None,
             "artifact prefix is unsafe")
    _require(suite == HOSTED_SUITE, "emission requires the exact hosted suite")
    if job == "local":
        _require(run_id is None and attempt is None,
                 "local smoke cannot claim hosted run identifiers")
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
        workflow_sha = commit_sha
        source_before = _source_snapshot(commit_sha, require_clean=True)
    summary = _suite_records(build, suite, artifact_prefix)
    source_revision = summary["source_revision"]
    if job == "local":
        _require(commit_sha is None or commit_sha == source_revision,
                 "local smoke SHA conflicts with the retained producer source revision")
    else:
        _require(source_revision == commit_sha,
                 "hosted build identity names a different source revision than the run")
    _require(summary["platform"] == (
        _host_platform() if job == "local" else "linux" if job == "gate-linux" else "macos"),
        "hosted platform does not match the exact workflow job")
    PRIVATE_ROOT.parent.mkdir(parents=True, exist_ok=True)
    if PRIVATE_ROOT.exists() or PRIVATE_ROOT.is_symlink():
        _require(PRIVATE_ROOT.is_dir() and not PRIVATE_ROOT.is_symlink(),
                 "private original store is unsafe")
    else:
        PRIVATE_ROOT.mkdir(mode=0o700)
    output_binding = _sha(str(output).encode("utf-8"))[:12]
    record_id = (f"{job}-{source_revision[:12]}-{run_id or 'local'}-"
                 f"{attempt or '0'}-{artifact_prefix[:32]}-{output_binding}")
    private = PRIVATE_ROOT / record_id
    _require(not private.exists() and not private.is_symlink(),
             "private original record destination already exists")
    private.mkdir(mode=0o700)
    _retain_originals(private, build, summary, source_before)
    if source_before is not None:
        _require(source_before == _source_snapshot(commit_sha or "", require_clean=True),
                 "source inputs changed during original-record verification")
    projection = _projection(summary, job=job, workflow_sha=workflow_sha,
                             run_id=run_id, attempt=attempt)
    expected = {"job": job, "sha": workflow_sha, "run": run_id, "attempt": attempt,
                "inventory": summary["inventory"]}
    public_projection(projection, expected)
    _require(not output.exists() and not output.is_symlink(),
             "public output directory must be fresh")
    output.mkdir(parents=True, mode=0o755)
    payload = json.dumps(projection, sort_keys=True, separators=(",", ":")).encode() + b"\n"
    _require(len(payload) <= PUBLIC_MANIFEST_MAX, "public projection exceeds the bounded size")
    manifest_path = output / PUBLIC_MEMBER
    with manifest_path.open("xb") as stream:
        stream.write(payload)
    manifest_path.chmod(0o644)
    scan_public_members(output, expected)
    record = {
        "schema": "t64.phase09-private-originals",
        "schema_version": 1,
        "evidence_mode": projection["evidence_mode"],
        "source_revision": source_revision,
        "source_binding_sha256": None if source_before is None else _sha(
            json.dumps(source_before, sort_keys=True, separators=(",", ":")).encode()),
        "public_manifest_sha256": _sha(payload),
        "last_test_path": summary["private"]["last_test_path"],
        "junit_path": summary["private"]["junit_path"],
        "run_directories": summary["private"]["run_directories"],
    }
    (private / "original-record.json").write_text(
        json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")
    (private / "original-record.json").chmod(0o600)
    return {"projection": projection, "private": private}


# ---- platform comparison -------------------------------------------------------

def _first_difference(left: Any, right: Any, path: str) -> str | None:
    """The path of the first value that differs between two JSON-shaped values."""
    if type(left) is not type(right):
        return path or "value"
    if isinstance(left, dict):
        if set(left) != set(right):
            return (path or "value") + " (keys)"
        for key in sorted(left):
            found = _first_difference(left[key], right[key], f"{path}.{key}" if path else key)
            if found is not None:
                return found
        return None
    if isinstance(left, list):
        if len(left) != len(right):
            return (path or "value") + " (length)"
        for index, (first, second) in enumerate(zip(left, right)):
            found = _first_difference(first, second, f"{path}[{index}]")
            if found is not None:
                return found
        return None
    return None if left == right else (path or "value")


def compare_hosted_pair(manifests: dict[str, dict[str, Any]]) -> None:
    """Linux and macOS agree on exactly the platform-invariant fields.

    Compared: the schema, mode, workflow SHA, source revision, run and attempt,
    every row (authored values, state digests and guest ticks, which are model
    time), the poll start ticks and completion tick, the process status fields
    and the inventory. Not compared: job name, platform, producer identities and
    binary hashes, and the command, LastTest and JUnit hashes, because build
    identities and process records differ per platform by design.
    """
    _require(isinstance(manifests, dict) and set(manifests) == {"gate-linux", "gate-macos"},
             "hosted evidence requires both named platform jobs")
    _require(all(isinstance(value, dict) and value.get("job_name") == job
                 for job, value in manifests.items()),
             "the two hosted manifests are not one manifest per platform job")

    def semantics(value: dict[str, Any]) -> dict[str, Any]:
        return {
            "schema": value["schema"], "schema_version": value["schema_version"],
            "evidence_mode": value["evidence_mode"], "workflow_sha": value["workflow_sha"],
            "source_revision": value["source_revision"], "run_id": value["run_id"],
            "run_attempt": value["run_attempt"], "rows": value["rows"],
            "timing": value["timing"],
            "execution": {key: value["execution"][key]
                          for key in ("return_code", "status", "timed_out")},
            "inventory": value["inventory"],
        }

    difference = _first_difference(semantics(manifests["gate-linux"]),
                                   semantics(manifests["gate-macos"]), "")
    _require(difference is None, f"Linux and macOS manifests disagree on {difference}")


# ---- hosted collection ----------------------------------------------------------

def _rdp_helpers() -> Any:
    path = ROOT / "scripts/qualify-rdp-vi.py"
    spec = importlib.util.spec_from_file_location("t64_phase09_rdp_helpers", path)
    _require(spec is not None and spec.loader is not None,
             "shared native qualification helpers are unavailable")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _expected_gcc13_image() -> str:
    """The pinned GCC 13 image identity, imported from the Phase 8 qualifier."""
    path = ROOT / "scripts/qualify-audio.py"
    spec = importlib.util.spec_from_file_location("t64_phase09_audio_pins", path)
    _require(spec is not None and spec.loader is not None, "Phase 8 pins are unavailable")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    image = module.EXPECTED_GCC13_IMAGE
    _require(isinstance(image, str) and re.fullmatch(r"sha256:[0-9a-f]{64}", image) is not None,
             "pinned GCC 13 image identity is malformed")
    return image


EXPECTED_GCC13_IMAGE = _expected_gcc13_image()


def _validate_hosted_context(value: Any, *, repo: str, sha: str, run: str,
                             attempt: str, job: str) -> dict[str, Any]:
    """Bind one hosted manifest to caller-supplied Actions identity."""
    _require(repo == "szTheory/tendies64" and SHA1_RE.fullmatch(sha) is not None and
             DECIMAL_RE.fullmatch(run) is not None and
             DECIMAL_RE.fullmatch(attempt) is not None and
             job in {"gate-linux", "gate-macos"},
             "hosted collection requires an exact repository, SHA, run, attempt, and job")
    return public_projection(value, {"job": job, "sha": sha, "run": run, "attempt": attempt})


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
            _require(artifact["expired"] is False, "selected hosted artifact is expired")
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
            attempt=attempt, job=job)
        with tempfile.TemporaryDirectory(prefix="t64-phase09-zip-check-",
                                         dir=private_root) as temp:
            extracted = Path(temp) / job
            rdp._safe_extract_upload(archive, extracted, job)
            rdp.verify_upload(extracted, expected_sha=sha, expected_job=job,
                              expected_run=run, expected_attempt=attempt)
            zipped = _read(extracted / "phase09" / job / PUBLIC_MEMBER, PUBLIC_MANIFEST_MAX)
            published = _read(combined / "phase09" / job / PUBLIC_MEMBER, PUBLIC_MANIFEST_MAX)
            _require(zipped == published,
                     f"original {job} ZIP phase09 projection differs from collected output")
        archive_bindings.append({
            "job": job, "artifact_id": str(artifact["id"]),
            "archive_bytes": artifact["size_in_bytes"],
            "archive_digest": artifact["digest"],
            "phase09_manifest_sha256": _file_sha(combined / "phase09" / job / PUBLIC_MEMBER),
        })
    return archive_bindings


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
        (target / "phase09").mkdir(mode=0o700)
        _safe_copy(combined / "phase09" / job, target / "phase09" / job)


def _verify_public_pair(directory: Path, *, sha: str, run: str,
                        attempt: str) -> dict[str, dict[str, Any]]:
    _require(directory.is_dir() and not directory.is_symlink() and
             {path.name for path in directory.iterdir()} == {"gate-linux", "gate-macos"} and
             all(path.is_dir() and not path.is_symlink() for path in directory.iterdir()),
             "public phase09 projection does not contain exactly the two hosted jobs")
    manifests = {}
    for job in ("gate-linux", "gate-macos"):
        manifest = scan_public_members(directory / job, {
            "job": job, "sha": sha, "run": run, "attempt": attempt})
        manifests[job] = _validate_hosted_context(
            manifest, repo="szTheory/tendies64", sha=sha, run=run, attempt=attempt, job=job)
    compare_hosted_pair(manifests)
    return manifests


def _single_reported_path(output: bytes, pattern: bytes, what: str) -> Path:
    """Return the one distinct path a collector reports; identical repeats are accepted."""
    found = {match.decode("utf-8", errors="strict") for match in
             re.findall(pattern, output, flags=re.MULTILINE)}
    _require(len(found) == 1, what)
    return Path(next(iter(found)))


def _write_json_private(path: Path, value: Any) -> None:
    payload = json.dumps(value, sort_keys=True, separators=(",", ":")).encode() + b"\n"
    with path.open("xb") as stream:
        stream.write(payload)
    path.chmod(0o600)


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
        data = _read(path)
        total += len(data)
        _require(total <= 512 * 1024 * 1024 and len(entries) < 10000,
                 "private evidence tree exceeds its bounded inventory")
        entries.append({"path": path.relative_to(directory).as_posix(),
                        "size_bytes": metadata.st_size, "sha256": _sha(data)})
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
        data = _read(directory / f"{name}.log")
        _require(len(data) == process.get(f"{name}_bytes") and
                 _sha(data) == process.get(f"{name}_sha256"),
                 "original owning command output was changed or truncated")
    return process


def _run_saved_stage(rdp: Any, directory: Path, command: list[str], timeout_seconds: int,
                     extra_environment: dict[str, str] | None = None) -> dict[str, Any]:
    environment_delta = {"PYTHONDONTWRITEBYTECODE": "1", **(extra_environment or {})}
    environment = os.environ.copy()
    environment.update(environment_delta)
    outcome = rdp._run_bounded(command, cwd=ROOT, environment=environment,
                               timeout_seconds=float(timeout_seconds))
    rdp._save_process(directory, command, ROOT, environment_delta, outcome,
                      float(timeout_seconds))
    _require(outcome.get("process_group_empty") is True and
             outcome.get("timed_out") is False and outcome.get("return_code") == 0,
             f"bounded native stage failed or timed out: {directory.name}")
    return outcome


def _collect_hosted(repo: str, sha: str, run: str, attempt: str, native_path: Path,
                    output_dir: Path, *, rehearsal: bool = False) -> Path:
    _require(repo == "szTheory/tendies64" and SHA1_RE.fullmatch(sha) is not None and
             DECIMAL_RE.fullmatch(run) is not None and DECIMAL_RE.fullmatch(attempt) is not None and
             _git(["rev-parse", "HEAD"]).decode().strip() == sha,
             "hosted collection requires exact repository/SHA/run/attempt on the selected source")
    _require(not native_path.is_symlink(), "native record path is a symlink")
    native_path = native_path.resolve()
    native = _verify_native_record(native_path, sha, rehearsal=rehearsal)
    hosted_private = PRIVATE_ROOT / f"hosted-{sha[:12]}-{run}-{attempt}-{time.time_ns()}"
    _require(not hosted_private.exists() and not hosted_private.is_symlink(),
             "hosted private destination already exists or is unsafe")
    hosted_private.mkdir(parents=True, mode=0o700)
    combined = hosted_private / "combined"
    public_output = output_dir.resolve()
    _require(not output_dir.is_symlink() and not _under(public_output, PRIVATE_ROOT) and
             not public_output.exists() and not public_output.is_symlink(),
             "public phase09 output must be a fresh directory")
    source_snapshot = _source_snapshot(sha, require_clean=True)
    _require(source_snapshot == native["source_snapshot"],
             "hosted collection source inputs differ from the completed native source snapshot")
    rdp = _rdp_helpers()
    collect_command = [sys.executable, "-B", str(ROOT / "scripts/qualify-rdp-vi.py"),
                       "--collect-hosted", "--repo", repo, "--sha", sha,
                       "--run", run, "--attempt", attempt, "--out", str(combined)]
    collect_dir = hosted_private / "collector-command"
    _run_saved_stage(rdp, collect_dir, collect_command, 240)
    output_bytes = _read(collect_dir / "stdout.log")
    collection_path = _single_reported_path(
        output_bytes, rb"^qualify-rdp-vi: private collection records retained at (.+)$",
        "exact hosted collector did not return one private API/ZIP record path")
    _require(_under(collection_path, ROOT / "build/gate/07-qualification") and
             collection_path.is_dir() and not collection_path.is_symlink(),
             "hosted collection receipt is outside its private record store")
    rdp._verify_collected_pair(combined, commit_sha=sha, run_id=run, run_attempt=attempt)
    _require(all((combined / "phase09" / job / PUBLIC_MEMBER).is_file()
                 for job in ("gate-linux", "gate-macos")),
             "selected hosted artifacts do not both contain phase09 qualification manifests")
    manifests = {
        job: _validate_hosted_context(
            _json(combined / "phase09" / job / PUBLIC_MEMBER, PUBLIC_MANIFEST_MAX),
            repo=repo, sha=sha, run=run, attempt=attempt, job=job)
        for job in ("gate-linux", "gate-macos")
    }
    compare_hosted_pair(manifests)
    nested_stdout = _read(collection_path / "collection/stdout.log")
    downloads = _single_reported_path(
        nested_stdout, rb"^collect-boot-evidence: original download records retained at (.+)$",
        "collector did not retain one exact set of API and archive originals")
    _require(_under(downloads, ROOT / "build/gate/07-qualification") and
             not downloads.is_symlink(),
             "hosted archive originals are outside their private build store")
    archive_bindings = _verify_hosted_originals(
        downloads, combined, repo=repo, sha=sha, run=run, attempt=attempt,
        private_root=hosted_private)
    pii_input = hosted_private / "pii-upload-layout"
    _build_pii_upload_layout(combined, pii_input)
    _run_saved_stage(rdp, hosted_private / "combined-pii-scan", [
        sys.executable, "-B", str(ROOT / "scripts/qualify-rdp-vi.py"),
        "--scan-pii", str(pii_input)], 60)
    _run_saved_stage(rdp, hosted_private / "combined-rights-scan", [
        "sh", "scripts/rights-scan.sh", "--staging", str(combined)], 60)
    output = public_output
    output.mkdir(parents=True, mode=0o755)
    for job in ("gate-linux", "gate-macos"):
        target = output / job
        target.mkdir(mode=0o755)
        with (target / PUBLIC_MEMBER).open("xb") as stream:
            stream.write(_read(combined / "phase09" / job / PUBLIC_MEMBER, PUBLIC_MANIFEST_MAX))
        (target / PUBLIC_MEMBER).chmod(0o644)
    published = _verify_public_pair(output, sha=sha, run=run, attempt=attempt)
    _require(_source_snapshot(sha, require_clean=True) == source_snapshot,
             "source inputs changed during exact hosted collection")
    record = {
        "schema": "t64.phase09-hosted-collection",
        "schema_version": 1,
        "evidence_mode": "hosted-gate",
        "status": "complete",
        "rehearsal": bool(rehearsal),
        "repo": repo, "sha": sha, "run": run, "attempt": attempt,
        "jobs": ["gate-linux", "gate-macos"],
        "source_snapshot": source_snapshot,
        "excluded_owner_metadata": _owner_metadata_snapshot(),
        "native_record_path": str(native_path),
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
        "public_manifests": {job: _file_sha(output / job / PUBLIC_MEMBER) for job in published},
    }
    record_path = hosted_private / "hosted-record.json"
    _write_json_private(record_path, record)
    return record_path


def _validate_hosted_record_header(record: Any, *, repo: str, sha: str, run: str,
                                   attempt: str, rehearsal: bool = False) -> dict[str, Any]:
    _require(isinstance(record, dict) and
             record.get("schema") == "t64.phase09-hosted-collection" and
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
                 "pii_input_path", "collector_command_path", "public_output_path")),
             "hosted record is missing exact context, platforms, or private originals")
    absent = record["source_snapshot"].get("absent_pending")
    _require(isinstance(absent, list) and all(isinstance(name, str) for name in absent),
             "hosted record is missing exact context, platforms, or private originals")
    _require(record.get("rehearsal") is bool(rehearsal),
             "hosted record rehearsal label differs from the requested verification")
    _rehearsal_gate(absent, rehearsal)
    return record


def _verify_hosted_record(path: Path, *, repo: str, sha: str, run: str, attempt: str,
                          native_path: Path, rehearsal: bool = False) -> dict[str, Any]:
    _require(not path.is_symlink(), "hosted record path is a symlink")
    path = path.resolve()
    _require(_under(path, PRIVATE_ROOT) and path.name == "hosted-record.json" and
             path.is_file(), "explicit hosted record is outside the private qualification tree")
    record = _validate_hosted_record_header(
        _json(path, 16 * 1024 * 1024), repo=repo, sha=sha, run=run, attempt=attempt,
        rehearsal=rehearsal)
    _require(not native_path.is_symlink(), "native record path is a symlink")
    native_path = native_path.resolve()
    native = _verify_native_record(native_path, sha, rehearsal=rehearsal)
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
    _verify_process_record(collector_dir, collector_command, max_timeout=240,
                           environment={"PYTHONDONTWRITEBYTECODE": "1"})
    collection_stdout = _read(collection / "collection/stdout.log")
    nested_command = ["sh", "scripts/collect-boot-evidence.sh", "--collect", "--repo", repo,
                      "--sha", sha, "--run", run, "--attempt", attempt, "--out", str(combined)]
    _verify_process_record(collection / "collection", nested_command, max_timeout=180,
                           environment={})
    download_matches = re.findall(
        rb"^collect-boot-evidence: original download records retained at (.+)$",
        collection_stdout, flags=re.MULTILINE)
    _require(len(download_matches) == 1 and
             Path(download_matches[0].decode("utf-8", errors="strict")) == downloads,
             "collector no longer names its exact retained API/ZIP originals")
    rdp._verify_collected_pair(combined, commit_sha=sha, run_id=run, run_attempt=attempt)
    bindings = _verify_hosted_originals(
        downloads, combined, repo=repo, sha=sha, run=run, attempt=attempt,
        private_root=hosted_private)
    _require(record.get("archive_bindings") == bindings,
             "hosted archive API identity or phase09 projection binding changed")
    _verify_process_record(hosted_private / "combined-pii-scan", [
        sys.executable, "-B", str(ROOT / "scripts/qualify-rdp-vi.py"), "--scan-pii",
        str(pii_input)], max_timeout=60, environment={"PYTHONDONTWRITEBYTECODE": "1"})
    _verify_process_record(hosted_private / "combined-rights-scan", [
        "sh", "scripts/rights-scan.sh", "--staging", str(combined)],
        max_timeout=60, environment={"PYTHONDONTWRITEBYTECODE": "1"})
    public_manifests = _verify_public_pair(public, sha=sha, run=run, attempt=attempt)
    _require(record.get("public_manifests") == {
        job: _file_sha(public / job / PUBLIC_MEMBER) for job in public_manifests
    } and all(
        _read(public / job / PUBLIC_MEMBER, PUBLIC_MANIFEST_MAX) ==
        _read(combined / "phase09" / job / PUBLIC_MEMBER, PUBLIC_MANIFEST_MAX)
        for job in public_manifests),
        "public hosted projections differ from their exact retained archives")
    return record


# ---- native qualification ---------------------------------------------------------

def _native_command_plan(prefix: str, normal_junit: Path) -> list[dict[str, Any]]:
    """Explicit commands after the shared native gate, for every inventory suite."""
    _require(isinstance(prefix, str) and
             re.fullmatch(r"09-[A-Za-z0-9._-]{1,96}", prefix) is not None and
             isinstance(normal_junit, Path) and normal_junit.is_absolute() and
             ".." not in normal_junit.parts and _under(normal_junit, PRIVATE_ROOT),
             "native artifact prefix is malformed")
    suites = list(_parse_inventory(_read(INVENTORY, 1024 * 1024))["suites"])
    checker = ROOT / "scripts/check-phase09-cases.py"
    builds = {"normal": str((ROOT / "build/gate").resolve()),
              "asan": str((ROOT / "build/asan").resolve())}
    plan: list[dict[str, Any]] = [{
        "label": "shared-native-gates",
        "command": [sys.executable, "-B", str(ROOT / "scripts/qualify-rdp-vi.py"), "--native"],
        "timeout_seconds": NATIVE_SHARED_STAGE_TIMEOUT_SECONDS,
        "environment": {"T64_GATE_JUNIT_OUT": str(normal_junit)},
    }]
    for kind in ("normal", "asan"):
        for suite in suites:
            plan.append({
                "label": f"{kind}-inventory-{suite}",
                "command": [sys.executable, "-B", str(checker), "--build", builds[kind],
                            "--suite", suite, "--artifact-prefix", f"{prefix}-{kind}"],
                "timeout_seconds": 720,
                "environment": {},
            })
    for kind in ("normal", "asan"):
        plan.append({
            "label": f"{kind}-source-body-check",
            "command": [sys.executable, "-B", str(ROOT / "scripts/check-rsp-no-hle.py"),
                        "--build", builds[kind]],
            "timeout_seconds": 120,
            "environment": {},
        })
    return plan


def _required_gcc_units(source_sha: str) -> list[str]:
    """Tracked C units under core, runner and tools/common changed since the Phase 9 base."""
    _require(SHA1_RE.fullmatch(source_sha) is not None, "source SHA is malformed")
    ancestor = subprocess.run(
        ["git", "merge-base", "--is-ancestor", PHASE09_SOURCE_BASE, source_sha], cwd=ROOT,
        capture_output=True, timeout=10, check=False)
    _require(ancestor.returncode == 0,
             "Phase 9 source baseline is not an ancestor of the selected revision")
    output = _git(["diff", "--name-only", "-z", "--diff-filter=AMR",
                   f"{PHASE09_SOURCE_BASE}..{source_sha}", "--", "core", "runner",
                   "tools/common"])
    return sorted({item.decode("utf-8", errors="strict") for item in output.split(b"\0")
                   if item.endswith(b".c")})


def _validate_gcc13_closure(sources: Any, command: Any, source_sha: str) -> list[str]:
    rdp = _rdp_helpers()
    required = set(_required_gcc_units(source_sha))
    _require(isinstance(sources, list) and sources == sorted(set(sources)) and
             required.issubset(set(sources)),
             "GCC13 closure omits required Phase 9 production or test C units")
    _require(isinstance(command, list) and
             command == rdp._gcc13_closure_command(sources, EXPECTED_GCC13_IMAGE),
             "GCC13 compiler command differs from its exact source unit set")
    script = command[command.index("sh") + 2]
    core_flags = next((line for line in script.splitlines() if line.startswith("core_flags=")), "")
    test_flags = next((line for line in script.splitlines() if line.startswith("test_flags=")), "")
    runner_flags = next((line for line in script.splitlines()
                         if line.startswith("runner_flags=")), "")
    _require("-Wframe-larger-than=2048" in core_flags and
             "-Wframe-larger-than=" not in test_flags and
             "-Wframe-larger-than=" not in runner_flags,
             "GCC13 core frame limit or the test and runner frame policy changed")
    _require("-DT64_REAL_RUNNER_TESTING" in script and "-DT64_SUPERVISOR_TESTING" in script,
             "GCC13 real or supervisor testing define is absent")
    return sources


def _verify_rdp_native_receipt(path: Path, source_sha: str, normal_junit: Path) -> dict[str, Any]:
    rdp = _rdp_helpers()
    path = path.resolve()
    _require(_under(path, ROOT / "build/gate") and path.is_dir() and not path.is_symlink(),
             "shared native receipt is outside its explicit private gate tree")
    record = _json(path / "source-binding.json", 8 * 1024 * 1024)
    _require(normal_junit.is_absolute() and _under(normal_junit, PRIVATE_ROOT) and
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
                 "image_id": EXPECTED_GCC13_IMAGE, "platform": "linux/amd64"},
             "shared native receipt lacks exact source or pinned GCC13 identity")
    _require(record.get("source_binding") == rdp._native_source_snapshot(ROOT, source_sha),
             "shared native source/build/fixture input snapshot is stale")
    sources = record.get("phase07_changed_c_sources")
    gcc_script = ROOT / "build/gate/07-10-gcc13-command.sh"
    route = record.get("gcc13_reference_route")
    _require(isinstance(route, dict) and route.get("path") == "build/gate/07-10-gcc13-command.sh" and
             route.get("sha256") == _file_sha(gcc_script),
             "saved GCC13 reference route is missing or changed")
    expected_gcc = rdp._gcc13_closure_command(sources, EXPECTED_GCC13_IMAGE)
    _validate_gcc13_closure(sources, record.get("gcc13_command"), source_sha)
    _require(record.get("gcc13_command") == expected_gcc and
             record.get("native_commands") == [
                 ["sh", "scripts/gate.sh"], ["cmake", "--preset", "asan"],
                 ["cmake", "--build", "--preset", "asan", "--parallel", "1"],
                 ["ctest", "--preset", "asan", "--no-tests=error"]],
             "shared native command plan differs from the reviewed bounded route")
    ctest = ["ctest", "--preset", "asan", "--no-tests=error", "--output-junit",
             str((path / "asan/full-ctest-junit.xml").resolve())]
    commands = {
        "gate": ["sh", "scripts/gate.sh"], "asan-configure": ["cmake", "--preset", "asan"],
        "asan-build": ["cmake", "--build", "--preset", "asan", "--parallel", "1"],
        "asan-full-ctest": ctest, "gcc13": expected_gcc,
    }
    for label, argv in commands.items():
        environment = {"T64_GATE_JUNIT_OUT": str(normal_junit)} if label == "gate" else {}
        if label == "asan-full-ctest":
            environment = {"ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
                           "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}
        _verify_process_record(path / label, argv,
                               max_timeout=NATIVE_SHARED_RECORD_MAX_TIMEOUT_SECONDS,
                               environment=environment)
    _require((path / "asan/full-ctest-junit.xml").is_file() and
             (path / "asan/after-full-ctest-LastTest.log").is_file(),
             "shared ASan full-inventory originals are missing")
    return record


def _discovery_only(text: str) -> bool:
    return (text.startswith("Start testing:") and "End testing:" in text and
            re.search(r"^\d+/\d+ Testing: ", text, flags=re.MULTILINE) is None)


def _verify_discovery_last_test_retention(owning: bytes, before: bytes, after: bytes,
                                          *, first: bool = True) -> None:
    """Require the full-suite log to survive non-running discovery.

    `ctest --show-only=json-v1` rewrites LastTest.log with an empty start/end log,
    so the log a later suite check copies before its own discovery is that empty
    log. The first check of a build must see the owning log byte for byte; every
    later copy is either the owning log or a log with no test sections, so no
    discovery ran a test.
    """
    before_text = before.decode("utf-8", errors="strict")
    after_text = after.decode("utf-8", errors="strict")
    if first:
        _require(before == owning,
                 "owning full-suite LastTest changed before inventory discovery")
    else:
        _require(before == owning or _discovery_only(before_text),
                 "inventory discovery was preceded by an unrecognized LastTest")
    _require(after == owning or _discovery_only(after_text),
             "inventory discovery ran tests or left an unrecognized LastTest")


def _all_sections(last_text: str) -> dict[str, str]:
    parts = re.split(r"(?=^\d+/\d+ Testing: )", last_text, flags=re.MULTILINE)
    sections: dict[str, str] = {}
    for part in parts:
        found = re.match(r"\d+/\d+ Testing: (.+)\n", part)
        if found is None:
            continue
        _require(found.group(1) not in sections, "full owning LastTest inventory is duplicated")
        sections[found.group(1)] = part
    return sections


def _native_suite_records(build: Path, *, prefix: str, last_path: Path,
                          junit_path: Path) -> dict[str, Any]:
    """Bind every authored suite to the full normal or ASan suite originals."""
    data = _read(INVENTORY, 1024 * 1024)
    suites = _parse_inventory(data)["suites"]
    last_bytes = _read(last_path)
    last_text = last_bytes.decode("utf-8", errors="strict")
    sections = _all_sections(last_text)
    last_names = re.findall(r"^\d+/\d+ Testing: (.+)$", last_text, flags=re.MULTILINE)
    _require(bool(sections) and len(last_names) == len(sections),
             "full owning LastTest inventory is empty or duplicated")
    for name, section in sections.items():
        _require(re.search(r"^Test Passed\.$", section, flags=re.MULTILINE) is not None,
                 f"CTest did not record {name} as passed")
    try:
        junit_root = ET.fromstring(_read(junit_path, 16 * 1024 * 1024))
    except ET.ParseError as error:
        raise Refusal("original JUnit is malformed") from error
    cases = [entry for entry in junit_root.iter("testcase")]
    junit_names = [entry.attrib.get("name") for entry in cases]
    _require(bool(junit_names) and len(junit_names) == len(set(junit_names)) and
             set(junit_names) == set(sections),
             "owning full-suite JUnit does not match its exact LastTest inventory")
    _require(all(not any(child.tag in {"failure", "error", "skipped"} for child in case)
                 for case in cases), "owning full-suite JUnit records a failed or skipped case")
    resolved = build.resolve()
    expected_build = ["cmake", "--build", str(resolved), "--parallel", "1"]
    expected_discovery = ["ctest", "--test-dir", str(resolved), "--show-only=json-v1"]
    checked: list[tuple[int, str, Path, Path]] = []
    artifact_paths: list[str] = []
    for suite_name, suite in suites.items():
        _require(set(suite["ctest"]).issubset(set(sections)),
                 f"full native suite did not reach every authored {suite_name} CTest")
        discovery_path = _artifact(build, prefix, suite_name, "discovery-stdout.log")
        token = discovery_path.name.removesuffix("-discovery-stdout.log")
        names = {name: build / f"{token}-{name}" for name in (
            "build-command.json", "discovery-command.json", "before-discovery-LastTest.log",
            "after-discovery-LastTest.log", "build-stdout.log", "build-stderr.log",
            "discovery-stderr.log")}
        _require(all(path.is_file() and not path.is_symlink() for path in names.values()),
                 "non-running authored inventory originals are incomplete")
        _require(_json(names["build-command.json"], 16 * 1024) == expected_build and
                 _json(names["discovery-command.json"], 16 * 1024) == expected_discovery,
                 "inventory build or discovery command changed")
        discovery = _json(discovery_path, 16 * 1024 * 1024)
        _require(isinstance(discovery, dict) and isinstance(discovery.get("tests"), list),
                 "full native CTest discovery record is malformed")
        discovered = [entry.get("name") for entry in discovery["tests"] if isinstance(entry, dict)]
        _require(len(discovered) == len(set(discovered)) and set(discovered) == set(sections),
                 "owning full suite and discovered test inventory differ")
        stamp = int(token.rsplit("-", 2)[1])
        checked.append((stamp, suite_name, names["before-discovery-LastTest.log"],
                        names["after-discovery-LastTest.log"]))
        artifact_paths.extend([str(discovery_path), *(str(path) for path in names.values())])
    for position, (_, _, before_path, after_path) in enumerate(sorted(checked)):
        _verify_discovery_last_test_retention(
            last_bytes, _read(before_path), _read(after_path), first=position == 0)
    hosted_names = suites[HOSTED_SUITE]["ctest"]
    case_names = {case for entry in suites[HOSTED_SUITE]["binaries"] for case in entry["cases"]}
    observed = _observe_sections(build, {name: sections[name] for name in hosted_names},
                                 case_names)
    return {
        "inventory": _inventory_summary(data),
        "artifact_prefix": prefix,
        "inventory_suites": list(suites),
        "full_ctest_names": list(sections),
        "observations": observed["observations"],
        "timing": observed["timing"],
        "producer": observed["producer"],
        "source_revision": observed["source_revision"],
        "last_test_path": str(last_path.resolve()),
        "junit_path": str(junit_path.resolve()),
        "private": {
            "last_test_path": str(last_path.resolve()), "junit_path": str(junit_path.resolve()),
            "run_directories": [run["directory"] for run in observed["runs"]],
            "fixture_paths": sorted({_run_option(run["argv"], "--fixture")
                                     for run in observed["runs"]}),
            "checker_artifact_paths": sorted(artifact_paths),
            "binary_paths": observed["binary_names"],
        },
    }


def _native_qualify(source_sha: str, output_dir: Path, *, rehearsal: bool = False) -> Path:
    _require(SHA1_RE.fullmatch(source_sha) is not None and
             _git(["rev-parse", "HEAD"]).decode().strip() == source_sha,
             "native source SHA must exactly match the checked-out revision")
    PRIVATE_ROOT.parent.mkdir(parents=True, exist_ok=True)
    if PRIVATE_ROOT.exists() or PRIVATE_ROOT.is_symlink():
        _require(PRIVATE_ROOT.is_dir() and not PRIVATE_ROOT.is_symlink(),
                 "native private root is unsafe")
    else:
        PRIVATE_ROOT.mkdir(mode=0o700)
    _require(not output_dir.is_symlink(), "native output path is a symlink")
    output = output_dir.resolve()
    _require(_under(output, PRIVATE_ROOT) and not output.exists() and not output.is_symlink(),
             "native output must be a fresh private directory under build/gate")
    source_before = _source_snapshot(source_sha, require_clean=True)
    _rehearsal_gate(source_before["absent_pending"], rehearsal)
    output.mkdir(parents=True, mode=0o700)
    prefix = f"09-{source_sha[:8]}-{time.time_ns()}"
    normal_junit = (output / "normal-full-ctest-junit.xml").resolve()
    plan = _native_command_plan(prefix, normal_junit)
    rdp = _rdp_helpers()
    steps_dir = output / "steps"
    steps_dir.mkdir(mode=0o700)
    markers: dict[str, bytes] = {}
    for item in plan:
        label = item["label"]
        _run_saved_stage(rdp, steps_dir / label, item["command"], item["timeout_seconds"],
                         item["environment"])
        stdout = _read(steps_dir / label / "stdout.log")
        if label == "shared-native-gates":
            matches = re.findall(
                r"^qualify-rdp-vi: private owning command, process, LastTest, and JUnit records: (.+)$",
                stdout.decode("utf-8", errors="strict"), flags=re.MULTILINE)
            _require(len(matches) == 1,
                     "shared native runner did not return one explicit original receipt")
            rdp_native_path = Path(matches[0]).resolve()
            rdp_record = _verify_rdp_native_receipt(rdp_native_path, source_sha, normal_junit)
        elif "-inventory-" in label:
            suite_name = label.partition("-inventory-")[2]
            _require(f"validated nonempty {suite_name} phase09 case inventory".encode() in stdout,
                     f"{label} did not reach its pass marker")
        else:
            _require(stdout.startswith(b"PASS: rsp-no-hle"),
                     f"{label} native source/body/object check did not pass")
        markers[label] = stdout
        _require(_source_snapshot(source_sha, require_clean=True) == source_before,
                 "source inputs changed during native Phase 9 qualification")
    suite_records: dict[str, Any] = {}
    for kind, build_name, junit_path, last_path in (
            ("normal", "gate", normal_junit, rdp_native_path / "normal/after-gate-LastTest.log"),
            ("asan", "asan", rdp_native_path / "asan/full-ctest-junit.xml",
             rdp_native_path / "asan/after-full-ctest-LastTest.log")):
        build = (ROOT / "build" / build_name).resolve()
        summary = _native_suite_records(build, prefix=f"{prefix}-{kind}", last_path=last_path,
                                        junit_path=junit_path)
        retained = output / kind
        retained.mkdir(mode=0o700)
        _retain_originals(retained, build, summary, source_before)
        _write_json_private(retained / "summary-binding.json", summary)
        suite_records[kind] = {
            "build_dir": str(build), "artifact_prefix": f"{prefix}-{kind}",
            "inventory_suites": summary["inventory_suites"], "summary": summary,
            "retained_path": str(retained), "retained_manifest": _tree_manifest(retained),
        }
    _require(_source_snapshot(source_sha, require_clean=True) == source_before,
             "source inputs changed before native qualification could be sealed")
    record = {
        "schema": "t64.phase09-native-qualification",
        "schema_version": 1,
        "evidence_mode": "native-qualification",
        "status": "complete",
        "rehearsal": bool(rehearsal),
        "source_sha": source_sha,
        "source_snapshot": source_before,
        "excluded_owner_metadata": _owner_metadata_snapshot(),
        "inventory": _inventory_summary(_read(INVENTORY, 1024 * 1024)),
        "normal_full_ctest_junit_path": str(normal_junit),
        "shared_native_path": str(rdp_native_path),
        "shared_native_record_sha256": _file_sha(rdp_native_path / "source-binding.json"),
        "shared_native_tree_manifest": _tree_manifest(rdp_native_path),
        "shared_native_closure": {"units": rdp_record["phase07_changed_c_sources"],
                                  "gcc13_command": rdp_record["gcc13_command"]},
        "steps_path": str(steps_dir),
        "suites": suite_records,
        "output_path": str(output),
    }
    _write_json_private(output / "native-record.json", record)
    return output / "native-record.json"


def _validate_native_record_header(record: Any, expected_sha: str, *,
                                   expected_snapshot: dict[str, Any] | None = None,
                                   rehearsal: bool = False) -> dict[str, Any]:
    _require(isinstance(record, dict) and
             record.get("schema") == "t64.phase09-native-qualification" and
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
    _require(isinstance(closure, dict) and isinstance(closure.get("units"), list) and
             isinstance(closure.get("gcc13_command"), list),
             "native evidence lacks the Phase 9 GCC13 source closure")
    data = _read(INVENTORY, 1024 * 1024)
    _require(record.get("inventory") == _inventory_summary(data),
             "native evidence is missing or reducing the authored case denominator")
    names = list(_parse_inventory(data)["suites"])
    _require(all(isinstance(entry, dict) and isinstance(entry.get("inventory_suites"), list) and
                 sorted(entry["inventory_suites"]) == sorted(names)
                 for entry in record["suites"].values()),
             "native evidence names a suite that is not in the authored inventory "
             "or omits an authored suite")
    if expected_snapshot is not None:
        _require(record["source_snapshot"] == expected_snapshot,
                 "native source inputs differ from the recorded clean snapshot")
    absent = record["source_snapshot"].get("absent_pending")
    _require(isinstance(absent, list) and all(isinstance(name, str) for name in absent),
             "native evidence is smoke-only, incomplete, or missing a required suite")
    _require(record.get("rehearsal") is bool(rehearsal),
             "native record rehearsal label differs from the requested verification")
    _rehearsal_gate(absent, rehearsal)
    return record


def _verify_native_record(path: Path, expected_sha: str, *,
                          rehearsal: bool = False) -> dict[str, Any]:
    _require(not path.is_symlink(), "native record path is a symlink")
    path = path.resolve()
    _require(_under(path, PRIVATE_ROOT) and path.name == "native-record.json" and
             path.is_file() and not path.is_symlink(),
             "explicit native record is outside the private qualification tree")
    current_source = _source_snapshot(expected_sha, require_clean=True)
    record = _validate_native_record_header(
        _json(path, 64 * 1024 * 1024), expected_sha, expected_snapshot=current_source,
        rehearsal=rehearsal)
    output = path.parent
    _require(record.get("output_path") == str(output) and
             record.get("steps_path") == str(output / "steps") and
             record.get("normal_full_ctest_junit_path") ==
             str((output / "normal-full-ctest-junit.xml").resolve()),
             "native record paths differ from its explicit private directory")
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
        rdp_path, expected_sha, Path(record["normal_full_ctest_junit_path"]))
    _require(record["shared_native_closure"] == {
        "units": rdp_record["phase07_changed_c_sources"],
        "gcc13_command": rdp_record["gcc13_command"],
    }, "native GCC13 source and command binding is stale")
    prefix = record["suites"]["normal"].get("artifact_prefix")
    _require(isinstance(prefix, str) and prefix.startswith("09-") and prefix.endswith("-normal") and
             record["suites"]["asan"].get("artifact_prefix") == prefix[:-7] + "-asan",
             "normal and ASan suite prefixes do not belong to the same native attempt")
    plan = _native_command_plan(prefix[:-7], Path(record["normal_full_ctest_junit_path"]))
    for item in plan:
        label = item["label"]
        _verify_process_record(output / "steps" / label, item["command"],
                               max_timeout=item["timeout_seconds"],
                               environment={"PYTHONDONTWRITEBYTECODE": "1",
                                            **item["environment"]})
        stdout = _read(output / "steps" / label / "stdout.log")
        if "-inventory-" in label:
            suite_name = label.partition("-inventory-")[2]
            _require(f"validated nonempty {suite_name} phase09 case inventory".encode() in stdout,
                     f"{label} pass marker is missing")
        elif "source-body-check" in label:
            _require(stdout.startswith(b"PASS: rsp-no-hle"),
                     "native source/body/object pass marker is missing")
    for kind, build_name in (("normal", "gate"), ("asan", "asan")):
        selected = record["suites"][kind]
        build = (ROOT / "build" / build_name).resolve()
        _require(selected.get("build_dir") == str(build) and
                 selected.get("retained_path") == str(output / kind),
                 f"{kind} inventory does not use its explicit owning build directory")
        last_path = rdp_path / ("normal/after-gate-LastTest.log" if kind == "normal"
                                else "asan/after-full-ctest-LastTest.log")
        junit_path = Path(record["normal_full_ctest_junit_path"] if kind == "normal"
                          else rdp_path / "asan/full-ctest-junit.xml")
        summary = _native_suite_records(build, prefix=selected["artifact_prefix"],
                                        last_path=last_path, junit_path=junit_path)
        _require(selected.get("summary") == json.loads(json.dumps(summary)) and
                 selected.get("retained_manifest") == _tree_manifest(output / kind),
                 f"{kind} owning command, result, binaries, or originals changed")
    return record


def _verify_record(run_directory: Path, build: Path, last_test: Path,
                   junit: Path) -> dict[str, Any]:
    """Re-validate one retained run against its owning LastTest section and JUnit."""
    build = Path(build).resolve()
    run = _run_record(Path(run_directory), build)
    last_text = _read(Path(last_test), 32 * 1024 * 1024).decode("utf-8", errors="strict")
    row = next((item for item in HOSTED_ROWS
                if (item["fixture_sha256"], item["input_sha256"]) ==
                (run["fixture_sha256"], run["input_sha256"])), None)
    _require(row is not None, "retained run matches no authored row")
    assert row is not None
    section = _test_section(last_text, row["ctest"])
    named = {str(Path(value).resolve()) for value in RETAINED_RUN_MARKER.findall(section)}
    _require(run["directory"] in named,
             "owning CTest section does not name this retained run directory")
    _junit_passed(Path(junit), row["ctest"])
    return {"row": row, "observation": row_observation(run["result"], run["input_sha256"], row)}


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
    parser.add_argument("--rehearsal", action="store_true",
                        help="label a native or hosted record made before every bound "
                             "source exists; never used for the final record")
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
    parser.add_argument("--suite", choices=(HOSTED_SUITE,))
    parser.add_argument("--artifact-prefix", help="explicit run-bound CTest artifact prefix")
    parser.add_argument("--out", type=Path, help="fresh public projection directory")
    args = parser.parse_args()
    if args.rehearsal and not (args.native or args.collect_hosted or args.verify_evidence):
        parser.error("--rehearsal is only valid with --native, --collect-hosted, "
                     "or --verify-evidence")
    try:
        if args.selftest:
            test_script = ROOT / "scripts/tests/test_phase09_qualification.py"
            result = subprocess.run(
                [sys.executable, "-B", str(test_script), "--selftest"],
                cwd=ROOT, timeout=45, check=False,
            )
            _require(result.returncode == 0, "Phase 9 qualification controls did not pass")
            return 0
        if args.native:
            if args.sha is None or args.out is None:
                parser.error("--native requires exact --sha and fresh private --out")
            native_record = _native_qualify(args.sha, args.out, rehearsal=args.rehearsal)
            print("qualify-phase09: native qualification passed; private originals retained "
                  f"at {native_record}")
            if args.rehearsal:
                print(f"qualify-phase09: {REHEARSAL_STATEMENT}")
            return 0
        if args.collect_hosted:
            if not all((args.repo, args.sha, args.run, args.attempt)) or \
                    args.native_record is None or args.out is None:
                parser.error("--collect-hosted requires --repo, --sha, --run, --attempt, "
                             "--native-record, and --out")
            hosted_record = _collect_hosted(
                args.repo, args.sha, args.run, args.attempt, args.native_record, args.out,
                rehearsal=args.rehearsal)
            print("qualify-phase09: exact hosted artifacts collected; private originals "
                  f"retained at {hosted_record}")
            if args.rehearsal:
                print(f"qualify-phase09: {REHEARSAL_STATEMENT}")
            return 0
        if args.verify_evidence:
            if not all((args.repo, args.sha, args.run, args.attempt)) or \
                    args.native_record is None or args.hosted_record is None:
                parser.error("--verify-evidence requires exact --repo, --sha, --run, "
                             "--attempt, --native-record, and --hosted-record")
            _verify_hosted_record(
                args.hosted_record, repo=args.repo, sha=args.sha, run=args.run,
                attempt=args.attempt, native_path=args.native_record,
                rehearsal=args.rehearsal)
            print("qualify-phase09: complete native, ASan, GCC13, and both hosted platform "
                  "records verified")
            if args.rehearsal:
                print(f"qualify-phase09: {REHEARSAL_STATEMENT}")
            return 0
        if args.verify_record is not None:
            if args.build is None or args.last_test is None or args.junit is None:
                parser.error("--verify-record requires --build, --last-test, and --junit")
            checked = _verify_record(args.verify_record, args.build, args.last_test, args.junit)
            observed = checked["observation"]
            print(f"qualify-phase09: accepted retained run {checked['row']['row_id']} "
                  f"({checked['row']['expected_outcome']}, guest tick {observed['guest_tick']}); "
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
            emitted = _emit_hosted(
                job=args.job, commit_sha=args.sha, run_id=args.run, attempt=args.attempt,
                build_dir=args.build, suite=args.suite, artifact_prefix=args.artifact_prefix,
                output_dir=args.out)
            mode_name = "entry-path validation" if args.job == "local" else "hosted evidence"
            print(f"qualify-phase09: emitted {mode_name}; private originals retained at "
                  f"{emitted['private']}")
            return 0
        if args.scan_pii:
            if args.out is None or args.job is None:
                parser.error("--scan-pii requires --job and --out")
            scan_public_members(args.out, {
                "job": args.job, "sha": args.sha, "run": args.run, "attempt": args.attempt,
                "inventory": inventory_snapshot(_read(INVENTORY, 1024 * 1024), HOSTED_SUITE),
            })
            print("qualify-phase09: strict public allowlist and PII scan passed")
            return 0
        raise Refusal("no supported mode selected")
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, Refusal,
            AttributeError, KeyError, TypeError, ValueError, AssertionError,
            subprocess.TimeoutExpired) as error:
        print(f"qualify-phase09: refused original evidence: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
