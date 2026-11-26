#!/usr/bin/env python3
"""Phase 11 qualifier: a thin delta over scripts/qualify-phase10.py.

Provenance: everything but the constants, the stage table, the artifact list and
the freeze-record binding below is a copy of scripts/qualify-phase10.py (kept
unmodified), which reduces the helpers of scripts/qualify-phase09.py and the
bounded process-group runner of scripts/qualify-rdp-vi.py: the Refusal class, the
bounded file readers, the git helper, the source snapshot (clean tree, sorted
input manifest with SHA-256 per file), the private-root handling (mode 0700, no
symlink) and the one-writer record file. Nothing from a closed Phase 7 to 10
qualifier is imported, edited or re-verified here. Standard library only.
Licensed under the repository license (Apache-2.0).

What this qualifier binds is only what Phase 11 changed:

  * the Wave 0 base and every later base, read as literal 40-hex values from the
    "- <label>: <40-hex>" lines of PIN-IMPACT.md (Wave 0 base, Install base,
    Freeze base, Qualification base; a ref name, HEAD, a short hash, a duplicate
    or a missing line is refused; never HEAD);
  * the bound source inputs: every file changed since the Wave 0 base plus the
    stage tools below, excluding planning and CI-ledger metadata (.planning/ and
    docs/ci-reviews/LOG.md) and the two Phase 11 evidence documents
    (docs/claims/phase11.md and docs/evidence/phase11-qualification.md), which
    cite the record and are written after it, so a verify after their commit
    stays valid (checker W-9);
  * the Phase 11 test inventory: the PHASE11_TESTS block of
    scripts/check-test-inventory.sh against the ctest -N -L phase11 selection;
  * the tested artifact identity: the build ids embedded in the libraries and
    runners the stages exercised (core, runner, ASan core) and in the archive
    that a fresh `git archive` build installed, with the consumer that linked it;
  * the freeze candidate: the four golden hashes of docs/abi/FREEZE-CANDIDATE.md
    (header manifest, public symbols, layout pins, status classes) as the record
    block states them and as the golden files hash today;
  * the stage table: the CI scanner, the inventory check, the owner-gated oracle
    authority check (blocked, not approved), the cheap ABI and install names, the
    installed suite with JUnit, the full gate, the sanitizer lanes against the
    shipped and the instrumented library, the two-thread TSan lane, the fresh
    archive lane, the digest sweep against the Wave 0 base and the freeze check.

Evidence tier recorded: "unit-tested, authored scope". A record makes no claim of
hardware accuracy, no claim of oracle comparison, no claim of a final ABI and no
claim of a release; it states that the authored Phase 11 tests and lanes passed on
the recorded source and artifacts of a software-interface candidate.

Modes:
  --native [--rehearsal]    run every stage and write one private record
  --verify RECORD [--rehearsal]
                            re-check a record against the current tree

The record, its per-stage logs and the JUnit files stay under
build/gate/11-qualification-private; raw logs contain local paths and are never
committed. Only a sanitized summary of the final record is committed (plan
11-25). A rehearsal record is labelled and is refused as final evidence.
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
PRIVATE_SUBPATH = "build/gate/11-qualification-private"
PIN_IMPACT_SUBPATH = (".planning/phases/11-installed-consumer-and-freeze-candidate/"
                      "PIN-IMPACT.md")
INVENTORY_SUBPATH = "scripts/check-test-inventory.sh"
CASES_SUBPATH = "tests/installed-consumer/cases.txt"
FREEZE_RECORD_SUBPATH = "docs/abi/FREEZE-CANDIDATE.md"
SCHEMA = "t64.phase11-qualification"
SCHEMA_VERSION = 1
RECORD_NAME = "record.json"
TIER = "unit-tested, authored scope"
# Phase 11 adds two claims that stay false: the freeze candidate is a software-interface
# candidate (ABI 0.1, package 0.1.0), never a final ABI and never a release.
CLAIMS = {"hardware_accuracy": False, "oracle_comparison": False, "final_abi": False,
          "release": False}
REHEARSAL_STATEMENT = "this result is a rehearsal and not final evidence"
HEX40 = re.compile(r"[0-9a-f]{40}")
SHA256_RE = re.compile(r"[0-9a-f]{64}")
CORE_BUILD_RE = re.compile(rb"t64-build-v1-[0-9a-f]{64}")
RUNNER_BUILD_RE = re.compile(rb"t64-runner-v1-[0-9a-f]{64}")
EMPTY_SHA256 = hashlib.sha256(b"").hexdigest()
RECORD_MAX_BYTES = 16 * 1024 * 1024
JUNIT_MAX_BYTES = 8 * 1024 * 1024

# Wave base labels of PIN-IMPACT.md "Wave bases". The Wave 0 base is required by
# every record; a final record also requires every later label (the install base
# is recorded by plan 11-15, the freeze base by 11-21 and the qualification base,
# the audited head, by plan 11-25).
WAVE0_LABEL = "Wave 0 base"
BASE_LABELS = (WAVE0_LABEL, "Install base", "Freeze base", "Qualification base")
QUALIFICATION_LABEL = "Qualification base"

# Planning and CI-ledger metadata: committed after the audited head, never an
# input of any build or test, hence excluded from the bound inputs and from the
# dirty check (the OWNER_METADATA_PATHS idea of the Phase 8 qualifier). The two
# Phase 11 evidence documents join them: they cite the record, so they are written
# after it and cannot be bound by it (W-9); they are the only addition to the
# Phase 10 exclusions, and no build or test reads them.
EVIDENCE_DOCS = ("docs/claims/phase11.md", "docs/evidence/phase11-qualification.md")
EXCLUDED_PREFIXES = (".planning/",)
EXCLUDED_FILES = ("docs/ci-reviews/LOG.md", *EVIDENCE_DOCS)

# The Phase 11 tools and stage entry points: bound even when they are unchanged
# since the Wave 0 base, because a stage result is only as good as its tool. The
# freeze record and its four goldens are listed so that they stay bound even if a
# later plan reverts them to their Wave 0 content.
TOOL_PATHS = (
    "CMakeLists.txt", "CMakePresets.json", ".github/workflows/ci.yml",
    "scripts/gate.sh", "scripts/check-ci-yml.sh", "scripts/check-test-inventory.sh",
    "scripts/check-rdp-oracle-authority.py", "scripts/check-install.sh",
    "scripts/check-abi.py", "scripts/digest-sweep.py", "tools/digest-sweep/t64_digest_sweep.c",
    "tests/installed-consumer/cases.txt",
    "docs/abi/FREEZE-CANDIDATE.md", "docs/abi/header.sha256", "docs/abi/public-symbols.txt",
    "docs/abi/layout.c", "docs/abi/status-classes.txt",
    "scripts/preflight-phase11.sh", "scripts/preflight-phase11-regexes.txt",
    "scripts/qualify-phase11.py", "scripts/tests/test_phase11_qualification.py",
)

# The four goldens of the freeze record: block key, golden file (PLAN 11-23).
FREEZE_GOLDENS = (
    ("header_sha256", "docs/abi/header.sha256"),
    ("public_symbols_sha256", "docs/abi/public-symbols.txt"),
    ("layout_sha256", "docs/abi/layout.c"),
    ("status_classes_sha256", "docs/abi/status-classes.txt"),
)
FRESH_PREFIX = "build/gate/fresh-archive/prefix with space"

# Artifact files whose embedded build id is the tested artifact identity
# (T64_BUILD_ID hashes production content and build policy; AUTONOMY section 2).
# installed_core is the archive that cmake --install put into the prefix of the
# fresh-archive lane (a `git archive HEAD` tree built outside the repository) and
# installed_consumer the independent consumer that linked it: the identity of what
# was installed and run (PATTERNS assignment 27).
ARTIFACTS = (
    ("core", "build/gate/core/libt64_core.a", CORE_BUILD_RE),
    ("runner_real", "build/gate/runner/t64-runner-real", RUNNER_BUILD_RE),
    ("runner_test", "build/gate/runner/t64-runner-test", RUNNER_BUILD_RE),
    ("asan_core", "build/asan/core/libt64_core.a", CORE_BUILD_RE),
    ("installed_core", f"{FRESH_PREFIX}/lib/libt64_core.a", CORE_BUILD_RE),
    ("installed_consumer", "build/gate/fresh-archive/consumer/t64c", CORE_BUILD_RE),
)
# Artifacts that must carry the same build id when a record names both: the fresh
# archive was configured exactly as the gate (T64_BUILD_ID hashes production
# content and build policy, not paths), and the consumer links the installed archive.
SAME_BUILD_ID = (("core", "installed_core"), ("installed_core", "installed_consumer"))

# Measured limits: see _stage_table for the derivation of each limit.
PYTHON = "python3"
# The scanner selftest had 138 cases when ci.yml, check-ci-yml.sh and gate.sh
# became final for Phase 10 (Phase 10 PIN-IMPACT section 10.3); no Phase 11 plan
# edited them, and it may only grow.
CI_SELFTEST_MIN_CASES = 138
# Lines the digest sweep compares between the 21 shipped ids at the Phase 11 Wave 0
# base and at HEAD (Phase 10 PIN-IMPACT sections 2.1, 2.2 and 7, the Phase 11 sweeps
# of plans 11-13, 11-14 and 11-23 print the same count): 1599 shared lines. A smaller
# number would mean the sweep silently covers fewer ROMs, ids or checkpoints.
SWEEP_SHARED_LINES = 1599
# The sanitizer lanes run the call-sequence generator for this many steps (the
# default of check-install.sh sanitize; PIN-IMPACT plan 11-20 Task 2).
SANITIZE_STEPS = 20000
# The composite digest every thread of the TSan lane must reach: a runner-recorded
# pin (Phase 10 PIN-IMPACT section 11.3), not an independent value (PIN-IMPACT plan 11-18).
COMPOSITE_DIGEST = "97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c"


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

def phase11_tests() -> list[str]:
    """The names of the PHASE11_TESTS block of scripts/check-test-inventory.sh."""
    text = _read(ROOT / INVENTORY_SUBPATH, 4 * 1024 * 1024).decode("utf-8", errors="strict")
    found = re.search(r"^PHASE11_TESTS='([^']*)'", text, flags=re.MULTILINE)
    _require(found is not None, "no PHASE11_TESTS block in scripts/check-test-inventory.sh")
    assert found is not None
    names = [line for line in found.group(1).split("\n") if line]
    _require(bool(names) and len(names) == len(set(names)),
             "the PHASE11_TESTS block is empty or lists a name twice")
    _require(all(re.fullmatch(r"[A-Za-z0-9._-]+", name) is not None for name in names),
             "the PHASE11_TESTS block holds a malformed test name")
    return names


def label_names() -> list[str]:
    """The names ctest -N -L phase11 selects in the gate build (fixture setups included)."""
    try:
        done = subprocess.run(
            ["ctest", "--test-dir", "build/gate", "-N", "-L", "phase11"], cwd=ROOT,
            capture_output=True, timeout=60, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise Refusal("cannot list the phase11 CTest selection") from error
    _require(done.returncode == 0, "ctest -N -L phase11 failed; configure build/gate first")
    text = done.stdout.decode("utf-8", errors="replace")
    names = re.findall(r"^\s*Test\s+#\d+:\s+(\S+)\s*$", text, flags=re.MULTILINE)
    total = re.search(r"^Total Tests: ([0-9]+)$", text, flags=re.MULTILINE)
    _require(total is not None and int(total.group(1)) == len(names),
             "ctest -N -L phase11 output is inconsistent")
    return sorted(names)


def inventory_snapshot() -> dict[str, Any]:
    """The PHASE11_TESTS block against the label selection; they must agree."""
    names = phase11_tests()
    labelled = label_names()
    _require(sorted(names) == labelled,
             "the CTest label phase11 selects different names than the PHASE11_TESTS block")
    return {
        "names": sorted(names),
        "count": len(names),
        "label_count": len(labelled),
        "names_sha256": _sha("\n".join(sorted(names)).encode() + b"\n"),
    }


# ---- artifact identity --------------------------------------------------------

def artifact_identity(names: list[str]) -> dict[str, Any]:
    """The build ids embedded in the named artifacts, which the stages exercised.

    Two artifacts of SAME_BUILD_ID that are both named must carry the same id: the
    archive that a fresh export installed equals the gate's archive (the build id
    hashes production content and build policy, not paths), and the consumer that
    linked it carries the same id.
    """
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
    for first, second in SAME_BUILD_ID:
        if first in found and second in found:
            _require(found[first]["build_id"] == found[second]["build_id"],
                     f"artifacts {first} and {second} carry different build ids")
    return found


# ---- freeze candidate -----------------------------------------------------------

def freeze_binding() -> dict[str, Any]:
    """The machine-read block of the freeze record and the hashes of its four goldens.

    Each golden file must hash to the value the block states (the same compare as
    repo.abi-freeze-record), so the record binds the goldens that the bound
    inputs hold. A malformed, missing or doubled block is refused.
    """
    raw = _read(ROOT / FREEZE_RECORD_SUBPATH, 1024 * 1024)
    text = raw.decode("utf-8", errors="strict")
    blocks = re.findall(r"^```freeze-record\n(.*?)^```$", text, flags=re.MULTILINE | re.DOTALL)
    _require(len(blocks) == 1, "the freeze record does not hold exactly one machine-read block")
    block: dict[str, str] = {}
    holes: list[str] = []
    for line in blocks[0].splitlines():
        key, separator, value = line.partition(": ")
        _require(separator == ": " and bool(key) and bool(value) and
                 re.fullmatch(r"[a-z0-9_]+", key) is not None,
                 "the freeze record block holds a malformed line")
        if key == "padding_hole":
            holes.append(value)
            continue
        _require(key not in block, f"the freeze record block states {key} twice")
        block[key] = value
    goldens: dict[str, Any] = {}
    for key, relative in FREEZE_GOLDENS:
        stated = block.get(key, "")
        _require(SHA256_RE.fullmatch(stated) is not None,
                 f"the freeze record block has no SHA-256 for {key}")
        actual = _sha(_read(ROOT / relative))
        _require(actual == stated,
                 f"the golden {relative} hashes to {actual}, the freeze record block states {stated}")
        goldens[key] = {"path": relative, "sha256": actual}
    for key in ("abi_version", "package_version", "amendments", "public_function_count",
                "status_count", "stop_reason_count", "layout_structs", "layout_members",
                "layout_constants", "padding_holes"):
        _require(key in block, f"the freeze record block lacks {key}")
    _require(re.fullmatch(r"[0-9]+", block["padding_holes"]) is not None and
             int(block["padding_holes"]) == len(holes),
             "the freeze record block states a padding hole count that its rows do not match")
    return {"record": FREEZE_RECORD_SUBPATH, "record_sha256": _sha(raw), "block": block,
            "goldens": goldens, "padding_hole_rows": sorted(holes)}


def cross_checks(commit: str, stages: list[dict[str, Any]], identity: dict[str, Any]) -> None:
    """Facts that tie a lane's own output to the record: the archived commit and the build id."""
    for stage in stages:
        if stage.get("name") != "fresh-archive":
            continue
        details = stage.get("details", {}).get("0", {})
        _require(details.get("archive_commit") == commit,
                 "the fresh-archive lane archived a commit other than the recorded one")
        if "installed_core" in identity:
            _require(details.get("build_id") == identity["installed_core"]["build_id"],
                     "the fresh-archive lane's build id differs from the installed archive's")


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
    factor, so a limit cannot drift below its derivation unnoticed. The CI scanner,
    inventory and oracle figures are the Phase 10 measurements (0.03 s and 2 s,
    0.6 to 1 s, 0.09 s; plan 10-17), no Phase 11 plan edited those tools. The
    others are Phase 11 measurements named in each derivation: PIN-IMPACT "Install
    steps" (plans 11-16 to 11-20, 1-minute host load 5 to 19) and "Freeze steps"
    (plans 11-22 to 11-24, load 3 to 7), and the native seconds of this plan's
    Task 2 (see PIN-IMPACT "Freeze steps", plan 11-24).
    """
    wave0 = bases[WAVE0_LABEL]
    gate_env = _gate_env()
    abi_regex = r"^(repo\.abi|install\.|api04\.)"
    abi_ctest = ["ctest", "--preset", "gate", "-R", abi_regex, "--no-tests=error"]
    installed_ctest = ["ctest", "--preset", "gate", "-L", "phase11", "--no-tests=error",
                       "--output-junit", "{record}/phase11-junit.xml"]
    return [
        _stage(
            "inventory",
            "exact test inventory of the gate preset (the PHASE11_TESTS block is part of it): measured 0.6 to 1 s natively (328 names); limit 60 s",
            [_command(["sh", "scripts/check-test-inventory.sh", "--preset", "gate"],
                      60, 1.0, check="inventory",
                      marker=r"^test inventory PASS preset=gate count=[0-9]+$")]),
        _stage(
            "fresh-archive",
            "git archive of HEAD built outside the repository as the gate preset configures it, only t64_core, installed into a prefix with a space, the consumer smoke and composite cases against it (check-install.sh fresh-archive; plan 11-24 Task 1): measured 3.3 s and 4 s in all (library 3 s, consumer under 1 s) at load 3, so 120 s is 30 times and covers a loaded host",
            [_command(["sh", "scripts/check-install.sh", "fresh-archive"], 120, 4,
                      marker=r"^check-install: fresh-archive ok: ", check="fresh-archive")],
            artifacts=("installed_core", "installed_consumer")),
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
    _require(not missing, "JUnit lacks the Phase 11 test(s): " + ", ".join(missing[:4]))
    _require(not extra, "JUnit lists a test outside the Phase 11 inventory: " + ", ".join(extra[:4]))
    _require(len(names) == len(expected),
             f"JUnit has {len(names)} testcases, the PHASE11_TESTS block names {len(expected)}")
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


def _cases_listed() -> list[str]:
    """The case names of tests/installed-consumer/cases.txt (lines that start with a letter)."""
    text = _read(ROOT / CASES_SUBPATH, 1024 * 1024).decode("utf-8", errors="strict")
    cases = sorted(line for line in text.splitlines()
                   if re.fullmatch(r"[a-z][a-z0-9-]*", line) is not None)
    _require(bool(cases) and len(cases) == len(set(cases)),
             "tests/installed-consumer/cases.txt lists no case or a case twice")
    return cases


def _sanitize_details(stdout: bytes, mode: str) -> dict[str, Any]:
    """The sanitizer lane of check-install.sh: every case ok, the instrumented side proven."""
    _require(mode in ("shipped", "instrumented"), "unknown sanitizer lane mode")
    text = stdout.decode("utf-8", errors="replace")
    _require(re.search(r"^check-install: sanitize [a-z]+: case [a-z0-9-]+ FAILED", text,
                       flags=re.MULTILINE) is None, f"the {mode} sanitizer lane reports a failed case")
    cases = re.findall(rf"^check-install: sanitize {mode}: case ([a-z0-9-]+) ok, ", text,
                       flags=re.MULTILINE)
    _require(sorted(cases) == _cases_listed(),
             f"the {mode} sanitizer lane did not run exactly the cases of cases.txt")
    refs = re.findall(rf"^check-install: sanitize {mode}: AddressSanitizer runtime references: "
                      r"archive ([0-9]+), consumer ([0-9]+)$", text, flags=re.MULTILINE)
    _require(len(refs) == 1, f"the {mode} sanitizer lane printed no runtime reference line")
    archive, consumer = (int(value) for value in refs[0])
    _require(consumer > 0, f"the {mode} lane's consumer is not instrumented")
    if mode == "shipped":
        _require(archive == 0, "the shipped lane's archive is instrumented, not as shipped")
    else:
        _require(archive > 0, "the instrumented lane's archive is not instrumented")
    ok = re.findall(rf"^check-install: sanitize {mode} ok: every case of cases.txt, sequence "
                    r"with --steps ([0-9]+) --digest-stride ([0-9]+), [0-9]+ s in all$", text,
                    flags=re.MULTILINE)
    _require(len(ok) == 1, f"the {mode} sanitizer lane printed no clean summary line")
    steps, stride = (int(value) for value in ok[0])
    _require(steps == SANITIZE_STEPS,
             f"the {mode} sanitizer lane ran {steps} sequence steps, the pinned run is {SANITIZE_STEPS}")
    return {"cases": len(cases), "archive_asan_references": archive,
            "consumer_asan_references": consumer, "sequence_steps": steps,
            "digest_stride": stride}


def _tsan_details(stdout: bytes) -> dict[str, Any]:
    text = stdout.decode("utf-8", errors="replace")
    refs = re.findall(r"^check-install: tsan: ThreadSanitizer runtime references: "
                      r"archive ([0-9]+), consumer ([0-9]+)$", text, flags=re.MULTILINE)
    _require(len(refs) == 1, "the TSan lane printed no runtime reference line")
    archive, consumer = (int(value) for value in refs[0])
    _require(archive > 0 and consumer > 0,
             "the TSan lane's library and consumer must both be instrumented")
    digests = re.findall(r"^check-install: tsan ok: .* both digests ([0-9a-f]{64});", text,
                         flags=re.MULTILINE)
    _require(len(digests) == 1 and digests[0] == COMPOSITE_DIGEST,
             "the TSan lane's thread digests differ from the pinned composite digest")
    return {"archive_tsan_references": archive, "consumer_tsan_references": consumer,
            "digest": digests[0]}


def _fresh_archive_details(stdout: bytes) -> dict[str, Any]:
    text = stdout.decode("utf-8", errors="replace")
    archived = re.findall(r"^check-install: fresh-archive: archive of ([0-9a-f]{40}) "
                          r"\(([0-9]+) files\)", text, flags=re.MULTILINE)
    _require(len(archived) == 1, "the fresh-archive lane did not name the archived commit")
    ids = re.findall(r"^check-install: fresh-archive: installed archive build id "
                     r"(t64-build-v1-[0-9a-f]{64})$", text, flags=re.MULTILINE)
    _require(len(ids) == 1, "the fresh-archive lane printed no installed build id")
    cases = sorted(re.findall(r"^check-install: fresh-archive: case ([a-z0-9-]+) ok \(", text,
                              flags=re.MULTILINE))
    _require(cases == ["composite", "smoke"],
             "the fresh-archive lane did not run exactly the smoke and composite cases")
    return {"archive_commit": archived[0][0], "archive_files": int(archived[0][1]),
            "build_id": ids[0], "cases": cases}


def _freeze_details(stdout: bytes) -> dict[str, Any]:
    """The freeze check's summary line must equal the machine-read block of the record."""
    text = stdout.decode("utf-8", errors="replace")
    found = re.findall(r"^abi-freeze: headers=([0-9]+) abi=([0-9.]+) package=([0-9.]+) "
                       r"symbols=([0-9]+) layout=([0-9]+)/([0-9]+)/([0-9]+) holes=([0-9]+) "
                       r"statuses=([0-9]+) stop_reasons=([0-9]+)$", text, flags=re.MULTILINE)
    _require(len(found) == 1, "the freeze check printed no summary line")
    headers, abi, package, symbols, structs, members, constants, holes, statuses, stops = found[0]
    block = freeze_binding()["block"]
    printed = {"abi_version": abi, "package_version": package,
               "public_function_count": symbols, "layout_structs": structs,
               "layout_members": members, "layout_constants": constants,
               "padding_holes": holes, "status_count": statuses, "stop_reason_count": stops}
    for key, value in printed.items():
        _require(block.get(key) == value,
                 f"the freeze check measured {key} {value}, the record block states {block.get(key)}")
    _require(headers == "1", f"the installed tree holds {headers} headers, the candidate has one")
    return {"printed": printed, "headers": int(headers)}


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
    if check == "sweep":
        return _sweep_details(stdout)
    if check.startswith("sanitize:"):
        return _sanitize_details(stdout, check.partition(":")[2])
    if check == "tsan":
        return _tsan_details(stdout)
    if check == "fresh-archive":
        return _fresh_archive_details(stdout)
    if check == "freeze":
        return _freeze_details(stdout)
    if check.startswith("junit:"):
        data = _read(directory / check.partition(":")[2], JUNIT_MAX_BYTES)
        return {**junit_problems(data, phase11_tests()), "junit_sha256": _sha(data)}
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
    freeze = freeze_binding()
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
        print(f"qualify-phase11: stage {stage['name']} ok "
              f"({stage_record['seconds']} s of {stage['limit_seconds']} s)", flush=True)
    identity = artifact_identity(_artifact_names(table))
    cross_checks(head, stages, identity)
    _require(freeze_binding() == freeze, "the freeze record or a golden changed during the run")
    record = {
        "schema": SCHEMA, "schema_version": SCHEMA_VERSION, "status": "complete",
        "record_id": directory.name, "rehearsal": bool(rehearsal),
        "statement": REHEARSAL_STATEMENT if rehearsal else None,
        "tier": TIER, "claims": dict(CLAIMS),
        "commit_sha": head, "wave_bases": bases,
        "qualification_drift_paths": drift,
        "source_snapshot": snapshot, "inventory": inventory,
        "freeze_record": freeze,
        "artifact_identity": identity,
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
             "the record is not a complete Phase 11 qualification record")
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
             "the Phase 11 test inventory differs from the recorded one")
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
    identity = artifact_identity(_artifact_names(table))
    _require(record.get("artifact_identity") == identity,
             "the tested artifact identity differs from the recorded build ids")
    cross_checks(commit, stages, identity)
    _require(record.get("freeze_record") == freeze_binding(),
             "the freeze record or one of its four goldens differs from the recorded binding")
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
            print("qualify-phase11: native qualification passed; private record at "
                  f"{_relative(record)}")
        else:
            verify_record(args.verify, rehearsal=args.rehearsal)
            print("qualify-phase11: record verified against the current tree: "
                  f"{_relative(args.verify)}")
        if args.rehearsal:
            print(f"qualify-phase11: {REHEARSAL_STATEMENT}")
        return 0
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, Refusal, KeyError,
            TypeError, ValueError, AssertionError, subprocess.TimeoutExpired) as error:
        print(f"qualify-phase11: refused original evidence: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
