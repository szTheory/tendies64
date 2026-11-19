#!/usr/bin/env python3
"""Phase 9 qualifier stub: refuses every evidence mode until 09-03.

Provenance: a reduced copy of scripts/qualify-audio.py (kept unmodified). It
keeps the constants, the Refusal class, the exact public member allowlist and
the mode set of the audio qualifier, and drops every collector and verifier
body. Until the Phase 9 collector and verifier land in 09-03, each mode other
than --selftest refuses with a stated reason and exits 1, so no step can pass
with evidence it never collected. Licensed under the repository license
(Apache-2.0).

The bound source tuple below is data: 09-03 must finalize it with every file
that 09-04 to 09-09 will create, because a later logic edit to a frozen file
costs a native plus hosted qualification cycle.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "docs/spec/phase09-cases.json"
INVENTORY_SCHEMA = "t64.phase09-case-inventory"
# No inventory hash pin here: the literal pins are data rows added in 09-11.
PUBLIC_SCHEMA = "t64.phase09-qualification"
PUBLIC_MANIFEST_MAX = 16 * 1024
# Upload allowlist: exactly one regular file with this name, nothing else.
PUBLIC_MEMBER = "manifest.json"
# Private artifact suffixes: state, replay and save media never leave the machine.
PRIVATE_SUFFIXES = (".sta", ".rpl", ".sav", ".eep", ".sra", ".z64")
STATE_MAGIC = b"T64STATE"
LOCAL_PATH_PATTERN = re.compile(rb"(?i)(?:/(?:users|home|private/var/folders)/[^\s\"']+|"
                                rb"\b[a-z]:\\{1,2}(?:users|documents and settings)\\{1,2}[^\s\"']+)")
EMAIL_PATTERN = re.compile(rb"(?i)[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,}")
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
)
NOT_PRESENT = ("the Phase 9 collector and verifier are not yet present "
               "(they are provided in 09-03); no evidence mode exits 0 without evidence")


class Refusal(ValueError):
    """Raised when evidence is missing, changed or unbound."""


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise Refusal(message)


def _read(path: Path, limit: int) -> bytes:
    try:
        metadata = path.lstat()
    except OSError as error:
        raise Refusal(f"required file is unavailable: {path.name}") from error
    if path.is_symlink() or not path.is_file() or metadata.st_size < 0 \
            or metadata.st_size > limit:
        raise Refusal(f"file is not a bounded regular file: {path.name}")
    return path.read_bytes()


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


def validate_case_inventory(path: Path) -> dict[str, int]:
    """Accept only a nonempty, well-formed case inventory; zero cases are refused."""
    try:
        inventory = json.loads(_read(Path(path), 1024 * 1024))
    except (json.JSONDecodeError, UnicodeDecodeError) as error:
        raise Refusal(f"case inventory is not valid JSON: {error}") from error
    _require(isinstance(inventory, dict), "case inventory must be a JSON object")
    _require(inventory.get("schema") == INVENTORY_SCHEMA
             and inventory.get("schema_version") == 1,
             "unsupported case inventory schema")
    suites = inventory.get("suites")
    _require(isinstance(suites, dict), "case inventory suites must be an object")
    _require(len(suites) > 0, "case inventory has zero suites")
    ctests = 0
    cases_total = 0
    for name, suite in suites.items():
        _require(isinstance(name, str) and bool(name) and isinstance(suite, dict),
                 "suite entries must be named objects")
        ctests += len(_unique_nonempty(suite.get("ctest"), f"suite {name} ctest names"))
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
    return {"suites": len(suites), "ctest": ctests, "cases": cases_total}


def scan_public_members(directory: Path) -> dict[str, Any]:
    """Exact upload allowlist: one regular manifest.json and nothing else.

    Refuses nested directories, symlinks, members with a private suffix,
    manifest bytes that begin with the state container magic or contain a local
    path or a personal identifier, and an oversized or non-object manifest.
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
    return value


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
    args = parser.parse_args()
    try:
        if args.selftest:
            test_script = ROOT / "scripts/tests/test_phase09_qualification.py"
            result = subprocess.run(
                [sys.executable, "-B", str(test_script), "--selftest"],
                cwd=ROOT, timeout=45, check=False,
            )
            _require(result.returncode == 0, "Phase 9 qualification controls did not pass")
            return 0
        raise Refusal(NOT_PRESENT)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, Refusal,
            AttributeError, KeyError, TypeError, ValueError, AssertionError,
            subprocess.TimeoutExpired) as error:
        print(f"qualify-phase09: refused original evidence: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
