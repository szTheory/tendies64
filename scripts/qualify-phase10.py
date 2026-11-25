#!/usr/bin/env python3
"""Phase 10 qualifier: a thin delta over scripts/qualify-phase09.py.

Provenance: the Refusal class, the bounded file readers, the git helper, the
source snapshot (clean tree, sorted input manifest with SHA-256 per file), the
private-root handling (mode 0700, no symlink) and the one-writer record file are
reduced copies of the helpers of scripts/qualify-phase09.py (kept unmodified),
and the bounded process-group runner is a reduced copy of _run_bounded of
scripts/qualify-rdp-vi.py. Nothing from the closed Phase 7, 8 or 9 qualifiers is
imported, edited or re-verified here. Standard library only. Licensed under the
repository license (Apache-2.0).

What this qualifier binds is only what Phase 10 changed:

  * the Wave 0 base and every later wave base, read as literal 40-hex values
    from the "- <label>: <40-hex>" lines of PIN-IMPACT.md (a ref name, HEAD, a
    short hash, a duplicate or a missing line is refused; never HEAD);
  * the bound source inputs: every file changed since the Wave 0 base plus the
    Phase 10 tools below, excluding planning and CI-ledger metadata
    (.planning/ and docs/ci-reviews/LOG.md), which cannot change a build;
  * the Phase 10 test inventory: the PHASE10_TESTS block of
    scripts/check-test-inventory.sh against the ctest -N -L phase10 selection;
  * the tested artifact identity: the build ids embedded in the libraries and
    runners the stages actually exercised (core, runner, ASan core);
  * the stage table: the CI scanner, the inventory check, the owner-gated
    oracle authority check (blocked, not approved), the full gate, the Phase 10
    JUnit, the ASan selection, the out-of-tree mutant driver and the digest
    sweep against the Wave 0 base.

Evidence tier recorded: "unit-tested, authored scope". A record makes no claim of
hardware accuracy and no claim of oracle comparison; it states that the authored
Phase 10 tests, mutants and sweep passed on the recorded source and artifacts.

Modes:
  --native [--rehearsal]    run every stage and write one private record
  --verify RECORD [--rehearsal]
                            re-check a record against the current tree

The record, its per-stage logs and the JUnit files stay under
build/gate/10-qualification-private; raw logs contain local paths and are never
committed. Only a sanitized summary of the final record is committed (plan
10-19). A rehearsal record is labelled and is refused as final evidence.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import signal
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
PRIVATE_SUBPATH = "build/gate/10-qualification-private"
PIN_IMPACT_SUBPATH = ".planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md"
INVENTORY_SUBPATH = "scripts/check-test-inventory.sh"
SCHEMA = "t64.phase10-qualification"
SCHEMA_VERSION = 1
RECORD_NAME = "record.json"
TIER = "unit-tested, authored scope"
CLAIMS = {"hardware_accuracy": False, "oracle_comparison": False}
REHEARSAL_STATEMENT = "this result is a rehearsal and not final evidence"
HEX40 = re.compile(r"[0-9a-f]{40}")
SHA256_RE = re.compile(r"[0-9a-f]{64}")
CORE_BUILD_RE = re.compile(rb"t64-build-v1-[0-9a-f]{64}")
RUNNER_BUILD_RE = re.compile(rb"t64-runner-v1-[0-9a-f]{64}")
EMPTY_SHA256 = hashlib.sha256(b"").hexdigest()
RECORD_MAX_BYTES = 16 * 1024 * 1024
JUNIT_MAX_BYTES = 8 * 1024 * 1024

# Wave base labels of PIN-IMPACT.md section 5. The Wave 0 base is required by
# every record; a final record also requires every later label (the
# qualification base is the audited head that plan 10-18 records).
WAVE0_LABEL = "Wave 0 base"
BASE_LABELS = (WAVE0_LABEL, "Wave 1 base", "Wave 2 base", "Wave 3 base",
               "Qualification base")
QUALIFICATION_LABEL = "Qualification base"

# Planning and CI-ledger metadata: committed after the audited head, never an
# input of any build or test, hence excluded from the bound inputs and from the
# dirty check (the OWNER_METADATA_PATHS idea of the Phase 8 qualifier).
EXCLUDED_PREFIXES = (".planning/",)
EXCLUDED_FILES = ("docs/ci-reviews/LOG.md",)

# The Phase 10 tools and stage entry points: bound even when they are unchanged
# since the Wave 0 base, because a stage result is only as good as its tool.
TOOL_PATHS = (
    "CMakeLists.txt", "CMakePresets.json", ".github/workflows/ci.yml",
    "scripts/gate.sh", "scripts/check-ci-yml.sh", "scripts/check-test-inventory.sh",
    "scripts/check-rdp-oracle-authority.py", "scripts/p10-mutants.py",
    "scripts/digest-sweep.py", "tools/digest-sweep/t64_digest_sweep.c",
    "scripts/preflight-phase10.sh", "scripts/preflight-phase10-regexes.txt",
    "scripts/qualify-phase10.py", "scripts/tests/test_phase10_qualification.py",
)

# Artifact files whose embedded build id is the tested artifact identity
# (T64_BUILD_ID hashes production content and build policy; AUTONOMY section 2).
ARTIFACTS = (
    ("core", "build/gate/core/libt64_core.a", CORE_BUILD_RE),
    ("runner_real", "build/gate/runner/t64-runner-real", RUNNER_BUILD_RE),
    ("runner_test", "build/gate/runner/t64-runner-test", RUNNER_BUILD_RE),
    ("asan_core", "build/asan/core/libt64_core.a", CORE_BUILD_RE),
)

# Measured limits: see _stage_table for the derivation of each limit.
PYTHON = "python3"
# The scanner selftest had 138 cases when ci.yml, check-ci-yml.sh and gate.sh
# became final for Phase 10 (PIN-IMPACT section 10.3); it may only grow.
CI_SELFTEST_MIN_CASES = 138
# Lines the digest sweep compares between the 21 shipped ids at the Wave 0 base
# and at HEAD (PIN-IMPACT sections 2.1, 2.2 and 7): 1599 shared lines. A smaller
# number would mean the sweep silently covers fewer ROMs, ids or checkpoints.
SWEEP_SHARED_LINES = 1599


class Refusal(ValueError):
    """Raised when evidence is missing, changed or unbound."""


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise Refusal(message)


def _sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _read(path: Path, limit: int = 64 * 1024 * 1024) -> bytes:
    try:
        metadata = path.lstat()
    except OSError as error:
        raise Refusal(f"required file is unavailable: {path.name}") from error
    if path.is_symlink() or not path.is_file() or metadata.st_size > limit:
        raise Refusal(f"file is not a bounded regular file: {path.name}")
    return path.read_bytes()


def _json(path: Path, limit: int = RECORD_MAX_BYTES) -> Any:
    try:
        return json.loads(_read(path, limit))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise Refusal(f"JSON is malformed: {path.name}") from error


def _under(path: Path, parent: Path) -> bool:
    try:
        path.resolve().relative_to(parent.resolve())
        return True
    except (OSError, ValueError):
        return False


def _private_root() -> Path:
    return ROOT / PRIVATE_SUBPATH


def _git(args: list[str]) -> bytes:
    try:
        result = subprocess.run(["git", *args], cwd=ROOT, capture_output=True,
                                timeout=60, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise Refusal("cannot inspect the selected source inputs") from error
    if result.returncode != 0:
        raise Refusal("cannot inspect the selected source inputs")
    return result.stdout


def _git_ok(args: list[str]) -> bool:
    try:
        return subprocess.run(["git", *args], cwd=ROOT, capture_output=True,
                              timeout=60, check=False).returncode == 0
    except (OSError, subprocess.TimeoutExpired):
        return False


def _head() -> str:
    head = _git(["rev-parse", "HEAD"]).decode("ascii").strip()
    _require(HEX40.fullmatch(head) is not None, "HEAD is not a 40-hex commit id")
    return head


def _excluded(path: str) -> bool:
    return path.startswith(EXCLUDED_PREFIXES) or path in EXCLUDED_FILES


# ---- wave bases ---------------------------------------------------------------

def read_bases(pin_impact: Path, *, final: bool) -> dict[str, str]:
    """The literal 40-hex wave bases of PIN-IMPACT.md; never HEAD, never a ref.

    The Wave 0 base is always required. A final record also requires the later
    labels. Every present label must occur on exactly one line of the exact form
    "- <label>: <40 lowercase hex>" and name a commit that is an ancestor of HEAD.
    """
    text = _read(pin_impact, 4 * 1024 * 1024).decode("utf-8", errors="strict")
    head = _head()
    required = BASE_LABELS if final else (WAVE0_LABEL,)
    bases: dict[str, str] = {}
    checked: set[str] = set()
    for label in BASE_LABELS:
        lines = [line for line in text.splitlines() if line.startswith(f"- {label}:")]
        if not lines:
            _require(label not in required, f"PIN-IMPACT.md has no '- {label}:' line")
            continue
        _require(len(lines) == 1,
                 f"PIN-IMPACT.md has {len(lines)} '- {label}:' lines; exactly one is required")
        found = re.fullmatch(rf"- {re.escape(label)}: ([0-9a-f]{{40}})", lines[0])
        _require(found is not None,
                 f"the '- {label}:' line is not a 40 lowercase hex literal "
                 "(HEAD, a ref name or a short hash is refused)")
        assert found is not None
        sha = found.group(1)
        if sha not in checked:
            _require(_git_ok(["cat-file", "-e", sha + "^{commit}"]),
                     f"the '{label}' value is not a commit of this repository")
            _require(_git_ok(["merge-base", "--is-ancestor", sha, head]),
                     f"the '{label}' commit is not an ancestor of HEAD")
            checked.add(sha)
        bases[label] = sha
    return bases


def _changed_between(first: str, second: str) -> list[str]:
    raw = _git(["diff", "--name-only", "-z", "--no-renames", first, second])
    return sorted(item.decode("utf-8", errors="strict") for item in raw.split(b"\0") if item)


# ---- source snapshot ----------------------------------------------------------

def source_snapshot(base: str) -> dict[str, Any]:
    """The clean-tree manifest of the bound inputs: changed since base plus tools."""
    _require(HEX40.fullmatch(base) is not None, "the snapshot base is not a 40-hex literal")
    head = _head()
    status = _git(["status", "--porcelain=v1", "-z", "--untracked-files=all", "--", ".",
                   *(f":(exclude){item.rstrip('/')}" for item in
                     (*EXCLUDED_PREFIXES, *EXCLUDED_FILES))])
    _require(not status,
             "the working tree has uncommitted or untracked source inputs; commit them first")
    raw = _git(["diff", "--name-status", "-z", "--no-renames", base, head])
    fields = [item.decode("utf-8", errors="strict") for item in raw.split(b"\0") if item]
    _require(len(fields) % 2 == 0, "source delta listing is malformed")
    changed: set[str] = set()
    deleted: set[str] = set()
    for code, path in zip(fields[0::2], fields[1::2]):
        if _excluded(path):
            continue
        (deleted if code.startswith("D") else changed).add(path)
    bound = sorted((changed | set(TOOL_PATHS)) - deleted)
    entries = []
    for relative in bound:
        path = ROOT / relative
        try:
            metadata = path.lstat()
        except OSError as error:
            raise Refusal(f"bound source input is absent: {relative}") from error
        _require(path.is_file() and not path.is_symlink(),
                 f"bound source input is not a regular file: {relative}")
        data = _read(path)
        entries.append({
            "path": relative,
            "mode": "100755" if metadata.st_mode & 0o111 else "100644",
            "size_bytes": len(data),
            "sha256": _sha(data),
        })
    encoded = json.dumps(entries, sort_keys=True, separators=(",", ":")).encode()
    return {
        "base": base,
        "excluded": {"prefixes": list(EXCLUDED_PREFIXES), "files": list(EXCLUDED_FILES)},
        "input_count": len(entries),
        "input_manifest_sha256": _sha(encoded),
        "inputs": entries,
        "deleted": sorted(deleted),
        "dirty_status_sha256": _sha(status),
    }


def qualification_drift(bases: dict[str, str]) -> list[str]:
    """Non-planning paths changed between the qualification base and HEAD."""
    base = bases.get(QUALIFICATION_LABEL)
    if base is None:
        return []
    return [path for path in _changed_between(base, _head()) if not _excluded(path)]


# ---- test inventory -----------------------------------------------------------

def phase10_tests() -> list[str]:
    """The names of the PHASE10_TESTS block of scripts/check-test-inventory.sh."""
    text = _read(ROOT / INVENTORY_SUBPATH, 4 * 1024 * 1024).decode("utf-8", errors="strict")
    found = re.search(r"^PHASE10_TESTS='([^']*)'", text, flags=re.MULTILINE)
    _require(found is not None, "no PHASE10_TESTS block in scripts/check-test-inventory.sh")
    assert found is not None
    names = [line for line in found.group(1).split("\n") if line]
    _require(bool(names) and len(names) == len(set(names)),
             "the PHASE10_TESTS block is empty or lists a name twice")
    _require(all(re.fullmatch(r"[A-Za-z0-9._-]+", name) is not None for name in names),
             "the PHASE10_TESTS block holds a malformed test name")
    return names


def label_names() -> list[str]:
    """The names ctest -N -L phase10 selects in the gate build."""
    try:
        done = subprocess.run(
            ["ctest", "--test-dir", "build/gate", "-N", "-L", "phase10"], cwd=ROOT,
            capture_output=True, timeout=60, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise Refusal("cannot list the phase10 CTest selection") from error
    _require(done.returncode == 0, "ctest -N -L phase10 failed; configure build/gate first")
    text = done.stdout.decode("utf-8", errors="replace")
    names = re.findall(r"^\s*Test\s+#\d+:\s+(\S+)\s*$", text, flags=re.MULTILINE)
    total = re.search(r"^Total Tests: ([0-9]+)$", text, flags=re.MULTILINE)
    _require(total is not None and int(total.group(1)) == len(names),
             "ctest -N -L phase10 output is inconsistent")
    return sorted(names)


def inventory_snapshot() -> dict[str, Any]:
    """The PHASE10_TESTS block against the label selection; they must agree."""
    names = phase10_tests()
    labelled = label_names()
    _require(sorted(names) == labelled,
             "the CTest label phase10 selects different names than the PHASE10_TESTS block")
    return {
        "names": sorted(names),
        "count": len(names),
        "label_count": len(labelled),
        "names_sha256": _sha("\n".join(sorted(names)).encode() + b"\n"),
    }


# ---- artifact identity --------------------------------------------------------

def artifact_identity(names: list[str]) -> dict[str, Any]:
    """The build ids embedded in the named artifacts, which the stages exercised."""
    known = {name for name, _, _ in ARTIFACTS}
    _require(set(names) <= known, "a stage names an unknown artifact")
    found: dict[str, Any] = {}
    for name, relative, pattern in ARTIFACTS:
        if name not in names:
            continue
        data = _read(ROOT / relative, 512 * 1024 * 1024)
        ids = sorted({item.decode("ascii") for item in pattern.findall(data)})
        _require(len(ids) == 1, f"artifact {relative} does not embed exactly one build id")
        found[name] = {"path": relative, "build_id": ids[0]}
    return found


# ---- stage table --------------------------------------------------------------

def _command(argv: list[str], limit: int, measured: float, *, expect: int = 0,
             marker: str | None = None, check: str | None = None,
             env: dict[str, str] | None = None) -> dict[str, Any]:
    return {"argv": argv, "expect_exit": expect, "limit_seconds": limit,
            "measured_seconds": measured, "marker": marker, "check": check,
            "env": dict(env or {})}


def _stage(name: str, derivation: str, commands: list[dict[str, Any]],
           artifacts: tuple[str, ...] = ()) -> dict[str, Any]:
    """A stage; artifacts names the ARTIFACTS entries whose identity it produces."""
    return {"name": name, "derivation": derivation, "commands": commands,
            "artifacts": list(artifacts),
            "limit_seconds": sum(item["limit_seconds"] for item in commands)}


def _gate_env() -> dict[str, str]:
    """The environment scripts/gate.sh exports before its CTest step."""
    return {
        "T64_REQUIRE_FIXTURES": "0",
        "T64_BOOT_FIXTURES_DIR": "{root}/build/fixtures",
        "T64_BOOT_CONTRACT_PATH": "{root}/fixtures/boot-contract.json",
        "T64_TEST_IPL3_PATH": "{root}/build/fixtures/ipl3_compat.z64",
    }


def _stage_table(bases: dict[str, str]) -> list[dict[str, Any]]:
    """Every stage with its commands, markers and limits (cheap stages first).

    A limit is a kill deadline: at least 2 times the largest measured seconds of
    the command, native or under the sanitizer, on the shared host (the F-108
    and F-112 lessons; a limit under the real cost fails every qualification
    closed, as the 600 s wrapper of Phase 7 once did). The measured value sits
    beside each limit, in the record as well, and the hermetic tests enforce the
    factor, so a limit cannot drift below its derivation unnoticed. Measurements:
    plan 10-17 Task 2, commit 48c928e, 1-minute host load 7.4 to
    13.7 on a shared 18-core host: gate.sh 143 s (CTest wall 84.9 s, 254 names),
    ctest -L phase10 7.2 s, ASan configure 1 s, ASan build 16 s, ASan ctest -L
    phase10 120.9 s wall (import-v26-hostile alone 120.9 s; the 30 ASan test
    seconds sum to 278.5 s), mutant run 97 s, digest sweep 41 s (60.6 s in
    PIN-IMPACT section 7), CI scanner 0.03 s and its selftest 1.8 to 2 s,
    inventory check 0.6 to 1 s, oracle check 0.09 s. Plan 10-20 refreshed the
    gate, asan and mutants figures (see their derivations and PIN-IMPACT
    section 16); the other stages keep the plan 10-17 figures above.
    """
    wave0 = bases[WAVE0_LABEL]
    gate_env = _gate_env()
    asan_env = {**gate_env, "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
                "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}
    ctest = ["ctest", "--preset", "gate", "-L", "phase10", "--no-tests=error",
             "--output-junit", "{record}/phase10-junit.xml"]
    asan_ctest = ["ctest", "--preset", "asan", "-L", "phase10", "--no-tests=error",
                  "--output-junit", "{record}/asan-phase10-junit.xml"]
    return [
        _stage(
            "ci-scanner",
            "CI workflow scanner and its 138-case selftest (F-115): measured 0.03 s and 2 s natively; limits of 30 s and 60 s cover interpreter and shell start on a loaded host",
            [
                _command(["sh", "scripts/check-ci-yml.sh"], 30, 0.03,
                         marker=r"^check-ci-yml: ok$"),
                _command(["sh", "scripts/check-ci-yml.sh", "--selftest"], 60,
                         2.0, check="ci-selftest",
                         marker=r"^check-ci-yml selftest: cases=[0-9]+ failures=0$"),
            ]),
        _stage(
            "inventory",
            "exact test inventory of the gate preset (the PHASE10_TESTS block is part of it): measured 0.6 to 1 s natively; limit 60 s",
            [_command(["sh", "scripts/check-test-inventory.sh", "--preset", "gate"],
                      60, 1.0, check="inventory",
                      marker=r"^test inventory PASS preset=gate count=[0-9]+$")]),
        _stage(
            "oracle",
            "owner-gated oracle request stays blocked (PIN-IMPACT section 8): measured "
            "0.09 s per run natively, limit 30 s covers interpreter start on a loaded host",
            [
                _command([PYTHON, "-B", "scripts/check-rdp-oracle-authority.py",
                          "--expect-blocked"], 30, 0.09, expect=0,
                         marker=r"blocked: no oracle operation is authorized"),
                _command([PYTHON, "-B", "scripts/check-rdp-oracle-authority.py",
                          "--require-approved"], 30, 0.09, expect=2,
                         marker=r"no pinned competent owner/legal decision"),
            ]),
        _stage(
            "gate",
            "the full gate (scripts/gate.sh): measured 129.41 s in the passing plan 10-19 qualification record (PIN-IMPACT section 15.1; 1-minute load 16.1 at the start and 6.8 at the end), so 600 s is 4.6 times; the plan 10-17 figure of 143 s (warm build, load 7.4) is superseded, and no newer measurement of the whole stage existed when plan 10-20 refreshed this text; a cold Binutils fixture build adds about 186 s (F-111) and stays inside; the Phase 9 gate stage took 136 to 220 s (F-110, F-112)",
            [_command(["sh", "scripts/gate.sh"], 600, 129,
                      marker=r"^\[gate\] PASS$", check="gate")],
            artifacts=("core", "runner_real", "runner_test")),
        _stage(
            "phase10-junit",
            "the 30 Phase 10 CTest names with JUnit on the gate preset: measured 7.2 s wall (22 s summed) at load 13.7, so 120 s is 16 times",
            [_command(ctest, 120, 7.2, env=gate_env,
                      marker=r"^100% tests passed", check="junit:phase10-junit.xml")]),
        _stage(
            "asan",
            "ASan and UBSan configure, build and the Phase 10 selection: measured 0.19 s, 7.71 s and 129.05 s wall in the passing plan 10-19 qualification record (PIN-IMPACT section 15.1; 1-minute load 16.1 at the start and 6.8 at the end; the 30 names sum to 305.08 s with import-v26-hostile alone 129.03 s), so the 600 s CTest limit is 4.6 times the wall and 1.97 times the serial sum (the four-job preset overlaps the names, and the longest name alone is 129.03 s), and the 600 s build limit covers an incremental rebuild after core edits; the plan 10-17 figures (1 s, 16 s, 120.9 s wall and a 278.5 s sum) are superseded, and no newer measurement of the whole selection existed when plan 10-20 refreshed this text",
            [
                _command(["cmake", "--preset", "asan"], 120, 0.2),
                _command(["cmake", "--build", "--preset", "asan", "--parallel", "1"],
                         600, 7.7),
                _command(asan_ctest, 600, 129, env=asan_env,
                         marker=r"^100% tests passed", check="junit:asan-phase10-junit.xml"),
            ],
            artifacts=("asan_core",)),
        _stage(
            "mutants",
            "out-of-tree mutant driver (26 mutants and 2 controls, 28 rows): measured 147 s at 1-minute load 10.7 to 13.5 (plan 10-20 Task 3, all 28 rows as expected), so 600 s is 4.1 times; the previous measurement was 97 s for 27 rows at load 8 (plan 10-17) and 100.72 s in the passing plan 10-19 record",
            [_command([PYTHON, "-B", "scripts/p10-mutants.py", "--run"], 600,
                      147, check="mutants")]),
        _stage(
            "sweep",
            "digest sweep of the 21 shipped ids against the Wave 0 base (two tree builds and two sweeps in parallel): measured 41 s here and 60.6 s at load 8.5 in PIN-IMPACT section 7, so 300 s is 4.9 times the larger",
            [_command([PYTHON, "-B", "scripts/digest-sweep.py", "--base", wave0, "--head",
                       "HEAD"], 300, 60.6, check="sweep",
                      marker=r"^sweep: [0-9]+ shared lines")]),
    ]


def _artifact_names(table: list[dict[str, Any]]) -> list[str]:
    return sorted({name for stage in table for name in stage["artifacts"]})


# ---- bounded process runner ---------------------------------------------------

def _group_exists(group: int) -> bool:
    try:
        os.killpg(group, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def _signal_group(group: int, selected: int) -> None:
    try:
        os.killpg(group, selected)
    except ProcessLookupError:
        pass


def _wait_group_empty(group: int, seconds: float) -> bool:
    deadline = time.monotonic() + seconds
    while _group_exists(group):
        if time.monotonic() >= deadline:
            return False
        time.sleep(0.01)
    return True


def run_bounded(argv: list[str], *, timeout_seconds: float,
                env_delta: dict[str, str] | None = None) -> dict[str, Any]:
    """Run one command in its own process group with a hard deadline."""
    _require(os.name == "posix", "bounded process-group cleanup requires a POSIX host")
    environment = os.environ.copy()
    environment["PYTHONDONTWRITEBYTECODE"] = "1"
    environment.update(env_delta or {})
    started = time.monotonic()
    timed_out = False
    return_code: int | None = None
    stdout = b""
    stderr = b""
    group_empty = True
    process: subprocess.Popen[bytes] | None = None
    try:
        process = subprocess.Popen(argv, cwd=ROOT, env=environment, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, start_new_session=True)
        try:
            stdout, stderr = process.communicate(timeout=timeout_seconds)
        except subprocess.TimeoutExpired:
            timed_out = True
            _signal_group(process.pid, signal.SIGTERM)
            try:
                stdout, stderr = process.communicate(timeout=0.5)
            except subprocess.TimeoutExpired:
                _signal_group(process.pid, signal.SIGKILL)
                try:
                    stdout, stderr = process.communicate(timeout=2.0)
                except subprocess.TimeoutExpired as error:
                    stdout = error.stdout or b""
                    stderr = error.stderr or b""
            group_empty = _wait_group_empty(process.pid, 0.5)
            if not group_empty:
                _signal_group(process.pid, signal.SIGKILL)
                group_empty = _wait_group_empty(process.pid, 1.0)
        else:
            return_code = process.returncode
            if _group_exists(process.pid):
                _signal_group(process.pid, signal.SIGTERM)
                group_empty = _wait_group_empty(process.pid, 0.2)
                if not group_empty:
                    _signal_group(process.pid, signal.SIGKILL)
                    group_empty = _wait_group_empty(process.pid, 1.0)
    except OSError as error:
        stderr = str(error).encode("utf-8", errors="replace")
        return_code = 127
    if process is not None and return_code is None:
        try:
            process.wait(timeout=0.5)
        except subprocess.TimeoutExpired:
            pass
        return_code = process.returncode
    if not group_empty:
        stderr += b"process group remained after bounded termination\n"
    return {"return_code": return_code, "timed_out": timed_out,
            "seconds": round(time.monotonic() - started, 2),
            "stdout": stdout, "stderr": stderr, "process_group_empty": group_empty}


# ---- per-command checks -------------------------------------------------------

def junit_problems(data: bytes, expected: list[str]) -> dict[str, Any]:
    """Every expected CTest name ran once and passed; the count equals the block."""
    try:
        root = ET.fromstring(data)
    except ET.ParseError as error:
        raise Refusal("JUnit is malformed") from error
    cases = list(root.iter("testcase"))
    names = [case.attrib.get("name", "") for case in cases]
    _require(len(names) == len(set(names)), "JUnit lists a test case twice")
    missing = sorted(set(expected) - set(names))
    extra = sorted(set(names) - set(expected))
    _require(not missing, "JUnit lacks the Phase 10 test(s): " + ", ".join(missing[:4]))
    _require(not extra, "JUnit lists a test outside the Phase 10 inventory: " + ", ".join(extra[:4]))
    _require(len(names) == len(expected),
             f"JUnit has {len(names)} testcases, the PHASE10_TESTS block names {len(expected)}")
    seconds: dict[str, float] = {}
    for case in cases:
        name = case.attrib["name"]
        _require(case.attrib.get("status") == "run" and
                 not any(child.tag in {"failure", "error", "skipped"} for child in case),
                 f"JUnit records {name} as failed, skipped or not run")
        try:
            seconds[name] = round(float(case.attrib.get("time", "")), 3)
        except ValueError as error:
            raise Refusal(f"JUnit time of {name} is not a number") from error
    return {"testcases": len(names), "seconds": dict(sorted(seconds.items())),
            "seconds_sum": round(sum(seconds.values()), 2)}


def gate_details(stdout: bytes) -> dict[str, Any]:
    """The per-step seconds of the gate summary and the CTest wall seconds."""
    text = stdout.decode("utf-8", errors="replace")
    _require(re.search(r"^\[gate\] PASS$", text, flags=re.MULTILINE) is not None,
             "the gate did not print its PASS line")
    steps = {name: int(sec) for name, sec in
             re.findall(r"^\[gate\]   ([a-z0-9-]+) ([0-9]+)s$", text, flags=re.MULTILINE)}
    for required in ("build", "test-inventory", "ctest", "core-lint"):
        _require(required in steps, f"the gate summary lacks the {required} step")
    total = re.findall(r"^\[gate\] total ([0-9]+)s$", text, flags=re.MULTILINE)
    wall = re.findall(r"^Total Test time \(real\) =\s+([0-9.]+) sec$", text, flags=re.MULTILINE)
    _require(len(total) == 1 and len(wall) == 1, "the gate output lacks its total or CTest wall")
    return {"steps": dict(sorted(steps.items())), "gate_total_seconds": int(total[0]),
            "ctest_wall_seconds": float(wall[0])}


def mutant_problems(stdout: bytes, listed: dict[str, str]) -> dict[str, Any]:
    """Every listed mutant and control ended as the table expects (KILLED, SURVIVED, INVALID)."""
    text = stdout.decode("utf-8", errors="replace")
    rows = re.findall(r"^(\S+) (KILLED|SURVIVED|INVALID) (KILLED|SURVIVED|INVALID) [0-9.]+ ",
                      text, flags=re.MULTILINE)
    seen = {name: (got, want) for name, got, want in rows}
    _require(len(seen) == len(rows), "the mutant output names a mutant twice")
    _require(set(seen) == set(listed), "the mutant output does not cover exactly the listed mutants")
    for name, (got, want) in seen.items():
        _require(want == listed[name] and got == want,
                 f"mutant {name} ended {got}, the table expects {listed[name]}")
    counts = {kind: sum(1 for got, _ in seen.values() if got == kind)
              for kind in ("KILLED", "SURVIVED", "INVALID")}
    _require(counts["KILLED"] > 0, "no mutant was killed")
    return {"mutants": len(seen), **{kind.lower(): count for kind, count in counts.items()}}


def listed_mutants() -> dict[str, str]:
    """The mutant ids and expected outcomes of the driver's own --list output."""
    run = run_bounded([PYTHON, "-B", "scripts/p10-mutants.py", "--list"], timeout_seconds=60)
    _require(run["return_code"] == 0, "the mutant driver cannot list its mutants")
    rows = re.findall(r"^(\S+) (KILLED|SURVIVED|INVALID) ", run["stdout"].decode(),
                      flags=re.MULTILINE)
    _require(bool(rows) and len({name for name, _ in rows}) == len(rows),
             "the mutant driver lists no mutant or a duplicate")
    return dict(rows)


def _ci_selftest_details(stdout: bytes) -> dict[str, Any]:
    text = stdout.decode("utf-8", errors="replace")
    found = re.findall(r"^check-ci-yml selftest: cases=([0-9]+) failures=0$", text,
                       flags=re.MULTILINE)
    _require(len(found) == 1, "the CI scanner selftest printed no clean summary line")
    _require(int(found[0]) >= CI_SELFTEST_MIN_CASES,
             f"the CI scanner selftest ran {found[0]} cases, fewer than {CI_SELFTEST_MIN_CASES}")
    return {"cases": int(found[0])}


def _inventory_details(stdout: bytes) -> dict[str, Any]:
    text = stdout.decode("utf-8", errors="replace")
    found = re.findall(r"^test inventory PASS preset=gate count=([0-9]+)$", text,
                       flags=re.MULTILINE)
    _require(len(found) == 1 and int(found[0]) > 0, "the test inventory check did not pass")
    return {"count": int(found[0])}


def _sweep_details(stdout: bytes) -> dict[str, Any]:
    text = stdout.decode("utf-8", errors="replace")
    found = re.findall(r"^sweep: ([0-9]+) shared lines, ([0-9]+) differences, "
                       r"([0-9]+) head-only lines$", text, flags=re.MULTILINE)
    _require(len(found) == 1, "the sweep printed no summary line")
    shared, differences, head_only = (int(value) for value in found[0])
    _require(differences == 0, f"the digest sweep reports {differences} differences")
    _require(head_only == 0, f"the digest sweep reports {head_only} head-only lines")
    _require(shared == SWEEP_SHARED_LINES,
             f"the digest sweep compared {shared} lines, the pinned coverage is {SWEEP_SHARED_LINES}")
    return {"shared_lines": shared, "differences": differences, "head_only_lines": head_only}


def command_details(spec: dict[str, Any], stdout: bytes, stderr: bytes,
                    directory: Path) -> dict[str, Any] | None:
    """Pass marker and special checks of one command; the details it adds."""
    if spec["marker"] is not None:
        _require(re.search(spec["marker"].encode(), stdout + b"\n" + stderr,
                           flags=re.MULTILINE) is not None,
                 f"the command output lacks its pass marker: {spec['marker']}")
    check = spec["check"]
    if check is None:
        return None
    if check == "gate":
        return gate_details(stdout)
    if check == "ci-selftest":
        return _ci_selftest_details(stdout)
    if check == "inventory":
        return _inventory_details(stdout)
    if check == "mutants":
        return mutant_problems(stdout, listed_mutants())
    if check == "sweep":
        return _sweep_details(stdout)
    if check.startswith("junit:"):
        data = _read(directory / check.partition(":")[2], JUNIT_MAX_BYTES)
        return {**junit_problems(data, phase10_tests()), "junit_sha256": _sha(data)}
    raise Refusal(f"unknown command check: {check}")


# ---- records ------------------------------------------------------------------

def _expand_text(value: str, directory: Path) -> str:
    return value.replace("{record}", str(directory)).replace("{root}", str(ROOT))


def _expand(argv: list[str], directory: Path) -> list[str]:
    return [_expand_text(item, directory) for item in argv]


def _expand_env(env: dict[str, str], directory: Path) -> dict[str, str]:
    return {name: _expand_text(value, directory) for name, value in env.items()}


def _private_root_ready() -> Path:
    root = _private_root()
    root.parent.mkdir(parents=True, exist_ok=True)
    if root.exists() or root.is_symlink():
        _require(root.is_dir() and not root.is_symlink(), "the private root is unsafe")
        _require(root.lstat().st_mode & 0o077 == 0,
                 "the private root is accessible to other users")
    else:
        root.mkdir(mode=0o700)
        root.chmod(0o700)
    return root


def _write_private(path: Path, payload: bytes) -> None:
    with path.open("xb") as stream:
        stream.write(payload)
    path.chmod(0o600)


def _utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _load() -> list[float]:
    try:
        return [round(value, 2) for value in os.getloadavg()]
    except OSError:
        return []


def _platform() -> str:
    if sys.platform == "darwin":
        return "macos"
    if sys.platform.startswith("linux"):
        return "linux"
    raise Refusal("the host platform is unsupported")


def native_qualify(*, rehearsal: bool) -> Path:
    """Run every stage in order and write one private record (never on failure)."""
    final = not rehearsal
    bases = read_bases(ROOT / PIN_IMPACT_SUBPATH, final=final)
    head = _head()
    snapshot = source_snapshot(bases[WAVE0_LABEL])
    drift = qualification_drift(bases) if final else []
    _require(not drift, "bound paths changed since the qualification base: " + ", ".join(drift[:4]))
    inventory = inventory_snapshot()
    table = _stage_table(bases)
    root = _private_root_ready()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    directory = root / f"{stamp}-{head[:8]}"
    _require(not directory.exists() and not directory.is_symlink(),
             "the record directory already exists")
    directory.mkdir(mode=0o700)
    steps_dir = directory / "steps"
    steps_dir.mkdir(mode=0o700)
    started = _utc_now()
    load_start = _load()
    started_clock = time.monotonic()
    stages: list[dict[str, Any]] = []
    for stage in table:
        stage_record: dict[str, Any] = {
            "name": stage["name"], "derivation": stage["derivation"],
            "artifacts": stage["artifacts"],
            "limit_seconds": stage["limit_seconds"], "commands": [], "details": {}}
        for index, spec in enumerate(stage["commands"]):
            outcome = run_bounded(_expand(spec["argv"], directory),
                                  timeout_seconds=spec["limit_seconds"],
                                  env_delta=_expand_env(spec["env"], directory))
            stem = f"{stage['name']}.{index}"
            _write_private(steps_dir / f"{stem}.stdout.log", outcome["stdout"])
            _write_private(steps_dir / f"{stem}.stderr.log", outcome["stderr"])
            _require(outcome["timed_out"] is False and outcome["process_group_empty"] is True,
                     f"stage {stage['name']} command {index} timed out or left a process group "
                     f"(limit {spec['limit_seconds']} s)")
            _require(outcome["return_code"] == spec["expect_exit"],
                     f"stage {stage['name']} command {index} exited {outcome['return_code']}, "
                     f"expected {spec['expect_exit']} (see {PRIVATE_SUBPATH}/{directory.name}/steps)")
            details = command_details(spec, outcome["stdout"], outcome["stderr"], directory)
            if details is not None:
                stage_record["details"][str(index)] = details
            stage_record["commands"].append({
                "argv": spec["argv"], "env": spec["env"], "expect_exit": spec["expect_exit"],
                "limit_seconds": spec["limit_seconds"],
                "measured_seconds": spec["measured_seconds"], "marker": spec["marker"],
                "check": spec["check"], "return_code": outcome["return_code"],
                "timed_out": False, "seconds": outcome["seconds"],
                "stdout_bytes": len(outcome["stdout"]), "stdout_sha256": _sha(outcome["stdout"]),
                "stderr_bytes": len(outcome["stderr"]), "stderr_sha256": _sha(outcome["stderr"]),
            })
            _require(outcome["seconds"] <= spec["limit_seconds"],
                     f"stage {stage['name']} command {index} ran over its limit")
        stage_record["seconds"] = round(sum(c["seconds"] for c in stage_record["commands"]), 2)
        stages.append(stage_record)
        _require(source_snapshot(bases[WAVE0_LABEL]) == snapshot,
                 f"source inputs changed during stage {stage['name']}")
        print(f"qualify-phase10: stage {stage['name']} ok "
              f"({stage_record['seconds']} s of {stage['limit_seconds']} s)", flush=True)
    record = {
        "schema": SCHEMA, "schema_version": SCHEMA_VERSION, "status": "complete",
        "record_id": directory.name, "rehearsal": bool(rehearsal),
        "statement": REHEARSAL_STATEMENT if rehearsal else None,
        "tier": TIER, "claims": dict(CLAIMS),
        "commit_sha": head, "wave_bases": bases,
        "qualification_drift_paths": drift,
        "source_snapshot": snapshot, "inventory": inventory,
        "artifact_identity": artifact_identity(_artifact_names(table)),
        "stages": stages,
        "host": {"platform": _platform(), "load_start": load_start, "load_end": _load()},
        "started_utc": started, "finished_utc": _utc_now(),
        "total_seconds": round(time.monotonic() - started_clock, 2),
    }
    _require(source_snapshot(bases[WAVE0_LABEL]) == snapshot,
             "source inputs changed before the record could be sealed")
    payload = json.dumps(record, sort_keys=True, separators=(",", ":")).encode() + b"\n"
    _write_private(directory / RECORD_NAME, payload)
    return directory / RECORD_NAME


def _verify_stage(stage: Any, expected: dict[str, Any], directory: Path) -> None:
    name = expected["name"]
    _require(isinstance(stage, dict) and stage.get("name") == name and
             stage.get("limit_seconds") == expected["limit_seconds"] and
             stage.get("derivation") == expected["derivation"] and
             stage.get("artifacts") == expected["artifacts"] and
             isinstance(stage.get("commands"), list) and
             len(stage["commands"]) == len(expected["commands"]) and
             isinstance(stage.get("details"), dict),
             f"stage {name} differs from the stage table")
    seconds = 0.0
    for index, (got, spec) in enumerate(zip(stage["commands"], expected["commands"])):
        _require(isinstance(got, dict) and got.get("argv") == spec["argv"] and
                 got.get("env") == spec["env"] and got.get("expect_exit") == spec["expect_exit"] and
                 got.get("limit_seconds") == spec["limit_seconds"] and
                 got.get("measured_seconds") == spec["measured_seconds"] and
                 got.get("marker") == spec["marker"] and got.get("check") == spec["check"],
                 f"stage {name} command {index} differs from the stage table")
        _require(got.get("return_code") == spec["expect_exit"] and got.get("timed_out") is False,
                 f"stage {name} command {index} failed or timed out")
        took = got.get("seconds")
        _require(isinstance(took, (int, float)) and not isinstance(took, bool) and
                 0 <= took <= spec["limit_seconds"],
                 f"stage {name} command {index} ran over its limit")
        seconds += took
        stem = f"{name}.{index}"
        for stream in ("stdout", "stderr"):
            data = _read(directory / "steps" / f"{stem}.{stream}.log")
            _require(got.get(f"{stream}_bytes") == len(data) and
                     got.get(f"{stream}_sha256") == _sha(data),
                     f"stage {name} command {index} {stream} log was changed or truncated")
        stdout = _read(directory / "steps" / f"{stem}.stdout.log")
        stderr = _read(directory / "steps" / f"{stem}.stderr.log")
        details = command_details(spec, stdout, stderr, directory)
        _require(stage["details"].get(str(index)) == details if details is not None
                 else str(index) not in stage["details"],
                 f"stage {name} command {index} details differ from its retained output")
    _require(stage.get("seconds") == round(seconds, 2),
             f"stage {name} seconds differ from the sum of its commands")


def verify_record(path: Path, *, rehearsal: bool) -> dict[str, Any]:
    """Re-check a record against the current tree and its own retained logs."""
    _require(not path.is_symlink(), "the record path is a symlink")
    path = path.resolve()
    _require(_under(path, _private_root()) and path.name == RECORD_NAME and path.is_file(),
             "the record is outside the private qualification root")
    record = _json(path)
    _require(isinstance(record, dict) and record.get("schema") == SCHEMA and
             record.get("schema_version") == SCHEMA_VERSION and
             record.get("status") == "complete" and
             record.get("record_id") == path.parent.name,
             "the record is not a complete Phase 10 qualification record")
    recorded_rehearsal = record.get("rehearsal")
    _require(isinstance(recorded_rehearsal, bool), "the record lacks its rehearsal label")
    _require(not (recorded_rehearsal and not rehearsal),
             "a rehearsal record is not final evidence; it is refused as final")
    _require(not (rehearsal and not recorded_rehearsal),
             "a final record was presented as a rehearsal")
    _require(record.get("statement") == (REHEARSAL_STATEMENT if rehearsal else None),
             "the record statement differs from its rehearsal label")
    _require(record.get("tier") == TIER and record.get("claims") == CLAIMS,
             f"the record tier or claims differ from '{TIER}' without hardware or oracle claims")
    commit = record.get("commit_sha")
    _require(isinstance(commit, str) and HEX40.fullmatch(commit) is not None and
             _git_ok(["merge-base", "--is-ancestor", commit, _head()]),
             "the recorded commit is malformed or not an ancestor of HEAD")
    bases = read_bases(ROOT / PIN_IMPACT_SUBPATH, final=not rehearsal)
    _require(record.get("wave_bases") == bases, "the recorded wave bases differ from PIN-IMPACT.md")
    drift = qualification_drift(bases) if not rehearsal else []
    _require(not drift, "bound paths changed since the qualification base: " + ", ".join(drift[:4]))
    _require(record.get("qualification_drift_paths") == drift,
             "the recorded qualification drift differs from the current tree")
    snapshot = source_snapshot(bases[WAVE0_LABEL])
    _require(record.get("source_snapshot") == snapshot,
             "bound source inputs differ from the recorded snapshot")
    _require(record.get("inventory") == inventory_snapshot(),
             "the Phase 10 test inventory differs from the recorded one")
    table = _stage_table(bases)
    stages = record.get("stages")
    _require(isinstance(stages, list), "the record lacks its stage list")
    recorded_names = [item.get("name") if isinstance(item, dict) else None for item in stages]
    wanted = [item["name"] for item in table]
    missing = [name for name in wanted if name not in recorded_names]
    _require(not missing, "the record lacks stage(s): " + ", ".join(missing))
    _require(recorded_names == wanted, "the record stage list differs from the stage table")
    for stage, expected in zip(stages, table):
        _verify_stage(stage, expected, path.parent)
    _require(record.get("artifact_identity") == artifact_identity(_artifact_names(table)),
             "the tested artifact identity differs from the recorded build ids")
    return record


# ---- command line -------------------------------------------------------------

def _relative(path: Path) -> str:
    try:
        return path.resolve().relative_to(ROOT.resolve()).as_posix()
    except ValueError:
        return path.name


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--native", action="store_true",
                      help="run every stage and write one private record")
    mode.add_argument("--verify", type=Path, metavar="RECORD",
                      help="re-check a record against the current tree")
    parser.add_argument("--rehearsal", action="store_true",
                        help="label a rehearsal record (native) or accept one (verify); "
                             "a rehearsal is never final evidence")
    args = parser.parse_args()
    try:
        if args.native:
            record = native_qualify(rehearsal=args.rehearsal)
            print("qualify-phase10: native qualification passed; private record at "
                  f"{_relative(record)}")
        else:
            verify_record(args.verify, rehearsal=args.rehearsal)
            print("qualify-phase10: record verified against the current tree: "
                  f"{_relative(args.verify)}")
        if args.rehearsal:
            print(f"qualify-phase10: {REHEARSAL_STATEMENT}")
        return 0
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, Refusal, KeyError,
            TypeError, ValueError, AssertionError, subprocess.TimeoutExpired) as error:
        print(f"qualify-phase10: refused original evidence: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
