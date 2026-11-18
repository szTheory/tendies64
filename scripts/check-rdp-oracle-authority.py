#!/usr/bin/env python3
"""Fail-closed admission for the prepared Phase07 oracle authority record."""

from __future__ import annotations

import argparse
from copy import deepcopy
from datetime import datetime, timedelta, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
RECORD_PATH = ROOT / "docs/evidence/phase07-oracle-authority.json"
PINNED_REQUEST_SHA256 = "deeacb7ae7fe4b7699bc4c9d4d1a1618e9caea0d57a2c51d136ae9798c9debdd"
PINNED_RIGHTS_AUDIT_SHA256 = "7506efecad2511fc2c535ed93b2f65b99dfc2e013508ba8ef760242251f1ff37"

# These are deliberately unset. A JSON field that says "approved" is not
# authority; a future competent decision requires a reviewed source change that
# pins the exact decision object here. Publication has a distinct pin.
TRUSTED_LOCAL_DECISION_SHA256: str | None = None
TRUSTED_PUBLICATION_DECISION_SHA256: str | None = None

EXPECTED_TOP_LEVEL_KEYS = {
    "schema", "schema_version", "record_id", "metadata_observed_at_utc",
    "request_sha256", "request", "rights_audit", "local_decision",
    "publication_decision",
}
EXPECTED_CORPUS_PATHS = {
    "docs/spec/rdp-vi-cases.json",
    "fixtures/manifest.json",
    "fixtures/rdp/fill.S",
    "scripts/check-rdp-vi-cases.py",
    "core/tests/test_rdp.c",
    "core/tests/test_rdp_copy.c",
    "core/tests/test_rdp_triangles.c",
    "core/tests/test_rdp_texture.c",
    "core/tests/test_rdp_combiner.c",
    "core/tests/test_rdp_blender.c",
    "core/tests/test_rdp_depth.c",
    "core/tests/test_vi.c",
    "core/tests/test_video_output.c",
    "runner/tests/test_rdp_video.c",
}
EXPECTED_ORACLES = (
    ("angrylion-plus", "3744ec766ed8d2c15fd5cbb4ac3c241b852b6e69"),
    ("parallel-rdp-standalone", "388d70f5835b352d841d9d9e5a08c5de01470f41"),
)
EXPECTED_OPERATIONS = [
    "out-of-tree-acquisition",
    "bounded-build",
    "black-box-execution-on-the-authored-corpus",
]
EXPECTED_AUDIT_IDS = [
    ("angrylion-plus", "MAME License.txt"),
    ("angrylion-plus", "src/**, msvc/**, and generated build inputs"),
    ("parallel-rdp-standalone", "root LICENSE"),
    ("parallel-rdp-standalone", "parallel-rdp/**"),
    ("parallel-rdp-standalone", "parallel-rdp/shaders/** including slangmosh.hpp and slangmosh.json"),
    ("parallel-rdp-standalone", "vulkan/**, util/**, volk/**, vulkan-headers/** and all build/include closure"),
    ("parallel-rdp-standalone", "upstream rdp-conformance, vi-conformance, fuzz vectors and expected data"),
]
EXPECTED_DECISION_KEYS = {
    "status", "decision_domain", "authority_role", "reference",
    "evidence_sha256", "issued_at_utc", "expires_at_utc", "approved_oracles",
    "approved_operations", "corpus_sha256", "rights_audit_sha256",
}
EXPECTED_PUBLICATION_KEYS = {
    "status", "decision_domain", "authority_role", "reference",
    "evidence_sha256", "issued_at_utc", "expires_at_utc",
    "approved_oracles", "approved_operations", "corpus_sha256",
    "rights_audit_sha256", "publication_scope",
}
BLOCKED_PUBLICATION_DECISION = {
    "status": "not-authorized",
    "decision_domain": "none",
    "authority_role": "none",
    "reference": None,
    "evidence_sha256": None,
    "issued_at_utc": None,
    "expires_at_utc": None,
    "approved_oracles": [],
    "approved_operations": [],
    "corpus_sha256": None,
    "rights_audit_sha256": None,
    "publication_scope": None,
}
EXPECTED_PUBLICATION_OPERATIONS = ["publish-oracle-derived-bytes-and-expectations"]
EXPECTED_PUBLICATION_SCOPE = "oracle-derived-bytes-and-expectations"
HEX_64 = re.compile(r"^[0-9a-f]{64}$")
HEX_40 = re.compile(r"^[0-9a-f]{40}$")


class Refusal(Exception):
    """Expected fail-closed admission refusal."""


def _no_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise Refusal(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _canonical_sha256(value: Any) -> str:
    encoded = json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=True,
        allow_nan=False,
    ).encode("ascii")
    return hashlib.sha256(encoded).hexdigest()


def _exact_keys(value: Any, keys: set[str], label: str) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != keys:
        raise Refusal(f"{label} has missing, unknown, or non-object fields")
    return value


def _refuse_private_paths(value: Any) -> None:
    if isinstance(value, str):
        if value.startswith(("/Users/", "/home/", "/private/")):
            raise Refusal("authority record contains a workstation-specific path")
        return
    if isinstance(value, list):
        for item in value:
            _refuse_private_paths(item)
    elif isinstance(value, dict):
        for item in value.values():
            _refuse_private_paths(item)


def _read_record() -> dict[str, Any]:
    if RECORD_PATH.is_symlink() or not RECORD_PATH.is_file():
        raise Refusal("versioned oracle authority record is missing or unsafe")
    try:
        record = json.loads(
            RECORD_PATH.read_text(encoding="utf-8"),
            object_pairs_hook=_no_duplicate_keys,
            parse_constant=lambda item: (_ for _ in ()).throw(
                Refusal(f"invalid JSON constant: {item}")
            ),
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise Refusal("oracle authority record cannot be parsed") from error
    return _exact_keys(record, EXPECTED_TOP_LEVEL_KEYS, "authority record")


def _safe_repo_path(relative: Any) -> Path:
    if not isinstance(relative, str) or not relative:
        raise Refusal("corpus path is missing")
    candidate = Path(relative)
    if candidate.is_absolute() or ".." in candidate.parts or "\\" in relative:
        raise Refusal("corpus path is not a safe repository-relative path")
    path = ROOT / candidate
    cursor = ROOT
    for part in candidate.parts:
        cursor = cursor / part
        if cursor.is_symlink():
            raise Refusal(f"bound authored corpus path traverses a symlink: {relative}")
    if not path.is_file() or not path.resolve().is_relative_to(ROOT.resolve()):
        raise Refusal(f"bound authored corpus file is missing or unsafe: {relative}")
    return path


def _verify_corpus(request: dict[str, Any]) -> str:
    corpus = request.get("corpus")
    if not isinstance(corpus, dict):
        raise Refusal("authored corpus identity is missing")
    manifest = corpus.get("case_manifest")
    if not isinstance(manifest, dict):
        raise Refusal("authored case manifest identity is missing")
    case_path = _safe_repo_path(manifest.get("path"))
    case_bytes = case_path.read_bytes()
    if hashlib.sha256(case_bytes).hexdigest() != manifest.get("sha256"):
        raise Refusal("authored case manifest SHA-256 changed")

    fixture_manifest = corpus.get("fixture_manifest")
    if not isinstance(fixture_manifest, dict):
        raise Refusal("authored fixture manifest identity is missing")
    fixture_path = _safe_repo_path(fixture_manifest.get("path"))
    fixture_bytes = fixture_path.read_bytes()
    if hashlib.sha256(fixture_bytes).hexdigest() != fixture_manifest.get("sha256"):
        raise Refusal("fixture manifest SHA-256 changed")

    files = corpus.get("bound_authored_inputs")
    if not isinstance(files, list) or not files:
        raise Refusal("bound authored corpus file set is empty")
    paths: list[str] = []
    for item in files:
        if not isinstance(item, dict) or set(item) != {
            "path", "sha256", "git_blob_id_sha1",
        }:
            raise Refusal("authored corpus input binding is malformed")
        if not HEX_64.fullmatch(str(item["sha256"])) or \
                not HEX_40.fullmatch(str(item["git_blob_id_sha1"])):
            raise Refusal("authored corpus digest uses an invalid hash domain")
        path = _safe_repo_path(item["path"])
        if hashlib.sha256(path.read_bytes()).hexdigest() != item["sha256"]:
            raise Refusal(f"authored corpus SHA-256 changed: {item['path']}")
        paths.append(item["path"])
    if len(paths) != len(set(paths)) or set(paths) != EXPECTED_CORPUS_PATHS:
        raise Refusal("authored corpus input inventory is incomplete or duplicated")
    try:
        blob_output = subprocess.run(
            ["git", "hash-object", "--", *paths],
            cwd=ROOT,
            env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
            check=True,
            capture_output=True,
            text=True,
            timeout=10,
        ).stdout.splitlines()
    except (OSError, subprocess.SubprocessError) as error:
        raise Refusal("Git blob identity check could not complete") from error
    if len(blob_output) != len(paths):
        raise Refusal("Git blob identity output is incomplete")
    by_path = dict(zip(paths, blob_output, strict=True))
    for item in files:
        if by_path[item["path"]] != item["git_blob_id_sha1"]:
            raise Refusal(f"Git blob ID changed: {item['path']}")

    try:
        cases = json.loads(case_bytes)
        fixture_data = json.loads(fixture_bytes)
    except json.JSONDecodeError as error:
        raise Refusal("authored case or fixture manifest is invalid JSON") from error
    suites = cases.get("suites") if isinstance(cases, dict) else None
    if not isinstance(suites, dict):
        raise Refusal("authored case suites are missing")
    case_pairs: list[tuple[str, str, str]] = []
    for suite_id, suite in suites.items():
        if not isinstance(suite, dict) or not isinstance(suite.get("binaries"), list):
            raise Refusal("authored case suite is malformed")
        for binary in suite["binaries"]:
            if not isinstance(binary, dict) or not isinstance(binary.get("path"), str) or \
                    not isinstance(binary.get("cases"), list):
                raise Refusal("authored case binary inventory is malformed")
            for case in binary["cases"]:
                if not isinstance(case, str) or not case:
                    raise Refusal("authored case name is empty or invalid")
                case_pairs.append((suite_id, binary["path"], case))
    if len(case_pairs) != len(set(case_pairs)):
        raise Refusal("authored case manifest has duplicate binary/case entries")
    if len(suites) != manifest.get("suites") or \
            len(case_pairs) != manifest.get("cases") or \
            cases.get("qualification_ctest") != "harness.rdp-vi-qualification":
        raise Refusal("authored case denominator differs from its bound identity")

    fixture_rows = fixture_data.get("entries") if isinstance(fixture_data, dict) else None
    if not isinstance(fixture_rows, list):
        raise Refusal("fixture manifest rows are missing")
    rows = {entry.get("id"): entry for entry in fixture_rows if isinstance(entry, dict)}
    fixture_ids = corpus.get("authored_fixture_ids")
    if fixture_ids != ["rdp-fill", "rdp-fill-payload"]:
        raise Refusal("authored fixture set differs from the prepared request")
    for expected in corpus.get("fixture_rights", []):
        row = rows.get(expected.get("id"))
        if not isinstance(row, dict):
            raise Refusal("authored fixture rights row is missing")
        if row.get("source_rights") != "authored-here" or \
                row.get("binary_rights") != "cleared" or \
                row.get("distribution") != ["build-only"] or \
                row.get("sha256") != expected.get("built_fixture_sha256") or \
                row.get("size") != expected.get("built_fixture_size_bytes"):
            raise Refusal("authored fixture rights or identity changed")
    return _canonical_sha256(corpus)


def _validate_record(record: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any], str]:
    if record.get("schema") != "t64.phase07.oracle-authority" or \
            record.get("schema_version") != 1 or \
            record.get("record_id") != "phase07-vid04-local-oracle-request-v1":
        raise Refusal("oracle authority record schema or identity mismatch")
    _refuse_private_paths(record)
    request = record.get("request")
    rights_audit = record.get("rights_audit")
    if not isinstance(request, dict) or not isinstance(rights_audit, dict):
        raise Refusal("authority request or rights audit is missing")
    actual_request_sha = _canonical_sha256(request)
    actual_audit_sha = _canonical_sha256(rights_audit)
    if actual_request_sha != PINNED_REQUEST_SHA256 or \
            record.get("request_sha256") != PINNED_REQUEST_SHA256:
        raise Refusal("authority request does not match the reviewed source pin")
    if actual_audit_sha != PINNED_RIGHTS_AUDIT_SHA256:
        raise Refusal("rights audit does not match the reviewed source pin")
    _parse_utc(record.get("metadata_observed_at_utc"), "metadata observation time")
    if request.get("purpose") != "Prepare a bounded local-only emulator comparison request for the Phase07 authored RDP/VI corpus." or \
            request.get("evidence_label") != "emulator-to-emulator differential only; never hardware truth":
        raise Refusal("authority request purpose or evidence label changed")
    oracles = request.get("oracles")
    if not isinstance(oracles, list) or len(oracles) != len(EXPECTED_ORACLES):
        raise Refusal("both pinned oracles are required")
    for oracle, (expected_id, expected_revision) in zip(oracles, EXPECTED_ORACLES, strict=True):
        if not isinstance(oracle, dict) or oracle.get("id") != expected_id or \
                oracle.get("revision_sha1") != expected_revision or \
                not HEX_40.fullmatch(str(oracle.get("revision_sha1"))):
            raise Refusal("oracle repository or pinned revision mismatch")
    local = _exact_keys(record.get("local_decision"), EXPECTED_DECISION_KEYS, "local decision")
    publication = _exact_keys(
        record.get("publication_decision"), EXPECTED_PUBLICATION_KEYS,
        "publication decision",
    )
    corpus_sha = _verify_corpus(request)
    audit_items = rights_audit.get("items")
    if rights_audit.get("complete") is not False or not isinstance(audit_items, list):
        raise Refusal("incomplete rights audit must remain explicitly blocked")
    actual_audit_ids = [(item.get("oracle_id"), item.get("path_scope"))
                        for item in audit_items if isinstance(item, dict)]
    if actual_audit_ids != EXPECTED_AUDIT_IDS:
        raise Refusal("rights audit path inventory is incomplete or reordered")
    return request, rights_audit, corpus_sha


def _expect_blocked(record: dict[str, Any]) -> None:
    _validate_record(record)
    decision = record["local_decision"]
    if decision != {
        "status": "blocked",
        "decision_domain": "none",
        "authority_role": "none",
        "reference": None,
        "evidence_sha256": None,
        "issued_at_utc": None,
        "expires_at_utc": None,
        "approved_oracles": [],
        "approved_operations": [],
        "corpus_sha256": None,
        "rights_audit_sha256": None,
    }:
        raise Refusal("default local decision is not the exact no-authority blocked record")
    publication = record["publication_decision"]
    if publication != BLOCKED_PUBLICATION_DECISION:
        raise Refusal("publication decision must remain separately unauthorized")
    if any(item.get("status") in {"cleared", "approved"}
           for item in record["rights_audit"]["items"]):
        raise Refusal("an unresolved upstream audit row was treated as cleared")


def _parse_utc(value: Any, label: str) -> datetime:
    if not isinstance(value, str) or not value.endswith("Z"):
        raise Refusal(f"{label} must be an explicit UTC timestamp")
    try:
        parsed = datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as error:
        raise Refusal(f"{label} is not a valid UTC timestamp") from error
    if parsed.tzinfo is None:
        raise Refusal(f"{label} has no timezone")
    return parsed.astimezone(timezone.utc)


def _validate_approved_decision(
    decision: dict[str, Any],
    request: dict[str, Any],
    rights_audit: dict[str, Any],
    *,
    now: datetime,
    allowed_domain: str,
    test_only: bool,
) -> None:
    _exact_keys(decision, EXPECTED_DECISION_KEYS, "approval decision")
    if decision.get("status") != "approved" or \
            decision.get("decision_domain") != allowed_domain:
        raise Refusal("decision is not an approval from the required authority domain")
    allowed_roles = {"owner", "legal-counsel"}
    if test_only:
        allowed_roles = {"authored-test"}
    if decision.get("authority_role") not in allowed_roles:
        raise Refusal("decision authority role is missing or not competent")
    reference = decision.get("reference")
    if not isinstance(reference, str) or not reference.strip():
        raise Refusal("decision reference is missing")
    _refuse_private_paths(reference)
    if not HEX_64.fullmatch(str(decision.get("evidence_sha256"))):
        raise Refusal("competent decision evidence digest is missing")
    issued = _parse_utc(decision.get("issued_at_utc"), "decision issue time")
    expires = _parse_utc(decision.get("expires_at_utc"), "decision expiry")
    if issued > now or expires <= now or expires <= issued:
        raise Refusal("decision is expired or has an invalid time range")
    if expires - issued > timedelta(days=366):
        raise Refusal("decision expiry exceeds the one-year authority window")

    expected_oracles = [
        {"id": item["id"], "revision_sha1": item["revision_sha1"]}
        for item in request["oracles"]
    ]
    if decision.get("approved_oracles") != expected_oracles:
        raise Refusal("decision omits an oracle or mismatches a pinned revision")
    if decision.get("approved_operations") != EXPECTED_OPERATIONS:
        raise Refusal("decision operation scope is partial or broader than requested")
    corpus_sha = _canonical_sha256(request["corpus"])
    if decision.get("corpus_sha256") != corpus_sha:
        raise Refusal("decision is not bound to the exact authored corpus")
    audit_sha = _canonical_sha256(rights_audit)
    if decision.get("rights_audit_sha256") != audit_sha:
        raise Refusal("decision is not bound to the current per-file rights audit")
    if rights_audit.get("complete") is not True or any(
        item.get("status") != "cleared" for item in rights_audit.get("items", [])
    ):
        raise Refusal("a required per-file or transitive rights row remains unresolved")
    if any(oracle.get("build_recipe", {}).get("status") != "ready"
           for oracle in request["oracles"]):
        raise Refusal("a pinned oracle lacks a reviewed, exact build recipe")
    publication = request.get("publication_scope", {})
    if publication.get("requested") is not False or \
            publication.get("status") != "not-authorized" or \
            publication.get("separate_competent_decision_required") is not True:
        raise Refusal("local execution cannot absorb publication permission")


def _validate_publication_decision(
    decision: dict[str, Any],
    request: dict[str, Any],
    rights_audit: dict[str, Any],
    *,
    now: datetime,
    test_only: bool,
) -> None:
    _exact_keys(decision, EXPECTED_PUBLICATION_KEYS, "publication decision")
    expected_domain = "authored-test" if test_only else "owner-legal"
    expected_roles = {"authored-test"} if test_only else {"owner", "legal-counsel"}
    if decision.get("status") != "approved" or \
            decision.get("decision_domain") != expected_domain:
        raise Refusal("publication decision is not a grant from the required authority domain")
    if decision.get("authority_role") not in expected_roles:
        raise Refusal("publication decision authority role is missing or not competent")
    reference = decision.get("reference")
    if not isinstance(reference, str) or not reference.strip():
        raise Refusal("publication decision reference is missing")
    _refuse_private_paths(reference)
    if not HEX_64.fullmatch(str(decision.get("evidence_sha256"))):
        raise Refusal("publication decision evidence digest is missing")
    issued = _parse_utc(decision.get("issued_at_utc"), "publication issue time")
    expires = _parse_utc(decision.get("expires_at_utc"), "publication expiry")
    if issued > now or expires <= now or expires <= issued:
        raise Refusal("publication decision is expired or has an invalid time range")
    if expires - issued > timedelta(days=366):
        raise Refusal("publication decision expiry exceeds the one-year authority window")
    expected_oracles = [
        {"id": item["id"], "revision_sha1": item["revision_sha1"]}
        for item in request["oracles"]
    ]
    if decision.get("approved_oracles") != expected_oracles:
        raise Refusal("publication decision omits an oracle or mismatches a pinned revision")
    if decision.get("approved_operations") != EXPECTED_PUBLICATION_OPERATIONS:
        raise Refusal("publication decision scope is partial or broader than requested")
    if decision.get("publication_scope") != EXPECTED_PUBLICATION_SCOPE:
        raise Refusal("publication decision omits the exact derived-output scope")
    if decision.get("corpus_sha256") != _canonical_sha256(request["corpus"]):
        raise Refusal("publication decision is not bound to the exact authored corpus")
    if decision.get("rights_audit_sha256") != _canonical_sha256(rights_audit):
        raise Refusal("publication decision is not bound to the current rights audit")
    if rights_audit.get("complete") is not True or any(
        item.get("status") != "cleared" for item in rights_audit.get("items", [])
    ):
        raise Refusal("publication requires a complete cleared per-file rights audit")
    scope = request.get("publication_scope", {})
    if scope.get("requested") is not True or scope.get("status") != "requested":
        raise Refusal("publication was not requested in the pinned authority scope")


def _require_local_approved(
    record: dict[str, Any],
    *,
    trusted_pin: str | None = None,
    now: datetime | None = None,
    test_only: bool = False,
) -> None:
    request, rights_audit, _corpus_sha = _validate_record(record)
    if record["publication_decision"] != BLOCKED_PUBLICATION_DECISION:
        raise Refusal("a local authority grant cannot carry publication permission")
    pin = TRUSTED_LOCAL_DECISION_SHA256 if trusted_pin is None else trusted_pin
    if pin is None:
        raise Refusal("no pinned competent owner/legal decision; oracle operations remain blocked")
    decision = record["local_decision"]
    if _canonical_sha256(decision) != pin:
        raise Refusal("local decision bytes do not match the trusted owner/legal pin")
    _validate_approved_decision(
        decision, request, rights_audit,
        now=now or datetime.now(timezone.utc),
        allowed_domain="authored-test" if test_only else "owner-legal",
        test_only=test_only,
    )


def _require_publication_approved(
    record: dict[str, Any],
    *,
    trusted_pin: str | None = None,
    now: datetime | None = None,
    test_only: bool = False,
) -> None:
    pin = TRUSTED_PUBLICATION_DECISION_SHA256 if trusted_pin is None else trusted_pin
    if pin is None:
        raise Refusal("no separate pinned publication decision; derived data stays local")
    decision = record["publication_decision"]
    request, rights_audit, _corpus_sha = _validate_record(record)
    if _canonical_sha256(decision) != pin:
        raise Refusal("publication decision does not match its separate trusted pin")
    _validate_publication_decision(
        decision, request, rights_audit,
        now=now or datetime.now(timezone.utc), test_only=test_only,
    )


def _expect_refusal(action: Any, phrase: str) -> None:
    try:
        action()
    except Refusal as error:
        if phrase not in str(error):
            raise AssertionError(f"refusal did not identify {phrase!r}: {error}") from error
        return
    raise AssertionError(f"expected refusal containing {phrase!r}")


def _synthetic_decision(
    request: dict[str, Any], rights_audit: dict[str, Any], *, expires_at: str,
    overrides: dict[str, Any] | None = None,
) -> dict[str, Any]:
    decision: dict[str, Any] = {
        "status": "approved",
        "decision_domain": "authored-test",
        "authority_role": "authored-test",
        "reference": "AUTHORED_TEST_ONLY:no external authority",
        "evidence_sha256": "a" * 64,
        "issued_at_utc": "2026-11-18T00:00:00Z",
        "expires_at_utc": expires_at,
        "approved_oracles": [
            {"id": item["id"], "revision_sha1": item["revision_sha1"]}
            for item in request["oracles"]
        ],
        "approved_operations": list(EXPECTED_OPERATIONS),
        "corpus_sha256": _canonical_sha256(request["corpus"]),
        "rights_audit_sha256": _canonical_sha256(rights_audit),
    }
    if overrides:
        decision.update(overrides)
    return decision


def _run_selftests(record: dict[str, Any]) -> list[str]:
    request, rights_audit, _corpus_sha = _validate_record(record)
    _expect_blocked(record)
    passed = ["blocked-record"]

    _expect_refusal(
        lambda: _no_duplicate_keys([("status", "blocked"), ("status", "approved")]),
        "duplicate JSON key",
    )
    passed.append("duplicate-record-key")

    missing = deepcopy(record)
    missing["local_decision"] = None
    _expect_refusal(lambda: _expect_blocked(missing), "local decision")
    passed.append("missing-authority")

    now = datetime(2026, 11, 18, 4, 0, 40, tzinfo=timezone.utc)
    future = "2026-12-01T00:00:00Z"
    synthetic_audit = deepcopy(rights_audit)
    synthetic_audit["complete"] = True
    for item in synthetic_audit["items"]:
        item["status"] = "cleared"
    synthetic_request = deepcopy(request)
    for oracle in synthetic_request["oracles"]:
        oracle["build_recipe"]["status"] = "ready"
    valid_test = _synthetic_decision(
        synthetic_request, synthetic_audit, expires_at=future,
    )
    _validate_approved_decision(
        valid_test, synthetic_request, synthetic_audit,
        now=now, allowed_domain="authored-test", test_only=True,
    )
    passed.append("approved-scoped-synthetic")

    partial = deepcopy(valid_test)
    partial["approved_oracles"] = partial["approved_oracles"][:1]
    _expect_refusal(
        lambda: _validate_approved_decision(
            partial, synthetic_request, synthetic_audit,
            now=now, allowed_domain="authored-test", test_only=True,
        ),
        "omits an oracle",
    )
    _expect_refusal(
        lambda: _validate_approved_decision(
            {**valid_test, "approved_oracles": [
                {"id": "angrylion-plus", "revision_sha1": "0" * 40},
                valid_test["approved_oracles"][1],
            ]},
            synthetic_request, synthetic_audit,
            now=now, allowed_domain="authored-test", test_only=True,
        ),
        "mismatches a pinned revision",
    )
    passed.extend(["partial-scope", "revision-mismatch"])

    expired = _synthetic_decision(
        synthetic_request, synthetic_audit, expires_at="2026-11-17T00:00:00Z",
    )
    _expect_refusal(
        lambda: _validate_approved_decision(
            expired, synthetic_request, synthetic_audit,
            now=now, allowed_domain="authored-test", test_only=True,
        ),
        "expired",
    )
    passed.append("expired-decision")

    trusted_expired_local = {
        **valid_test,
        "reference": "AUTHORED_TEST_ONLY:trusted bytes with expired time window",
        "issued_at_utc": "2026-11-17T00:00:00Z",
        "expires_at_utc": "2026-11-17T01:00:00Z",
    }
    expired_local_record = {**record, "local_decision": trusted_expired_local}
    local_pin = _canonical_sha256(trusted_expired_local)
    _expect_refusal(
        lambda: _require_local_approved(
            expired_local_record, trusted_pin=local_pin, now=now, test_only=True,
        ),
        "expired",
    )
    _expect_refusal(
        lambda: _require_local_approved(
            {**expired_local_record, "local_decision": {
                **trusted_expired_local,
                "expires_at_utc": "2026-12-01T00:00:00Z",
            }},
            trusted_pin=local_pin,
            now=now,
            test_only=True,
        ),
        "do not match the trusted owner/legal pin",
    )
    passed.extend(["trusted-expired-local-refusal", "altered-local-pin-refusal"])

    _expect_refusal(
        lambda: _validate_approved_decision(
            valid_test, request, rights_audit,
            now=now, allowed_domain="owner-legal", test_only=False,
        ),
        "required authority domain",
    )
    _expect_refusal(lambda: _require_local_approved({**record, "local_decision": valid_test}),
                    "no pinned competent owner/legal decision")
    passed.append("synthetic-live-refusal")

    publication_request = deepcopy(synthetic_request)
    publication_request["publication_scope"] = {
        "requested": True,
        "status": "requested",
        "oracle_derived_bytes_or_expectations": "publish only the exact reviewed output set",
        "separate_competent_decision_required": True,
    }
    valid_publication = {
        "status": "approved",
        "decision_domain": "authored-test",
        "authority_role": "authored-test",
        "reference": "AUTHORED_TEST_ONLY:no publication authority",
        "evidence_sha256": "b" * 64,
        "issued_at_utc": "2026-11-18T00:00:00Z",
        "expires_at_utc": future,
        "approved_oracles": [
            {"id": item["id"], "revision_sha1": item["revision_sha1"]}
            for item in publication_request["oracles"]
        ],
        "approved_operations": list(EXPECTED_PUBLICATION_OPERATIONS),
        "corpus_sha256": _canonical_sha256(publication_request["corpus"]),
        "rights_audit_sha256": _canonical_sha256(synthetic_audit),
        "publication_scope": EXPECTED_PUBLICATION_SCOPE,
    }
    _validate_publication_decision(
        valid_publication, publication_request, synthetic_audit,
        now=now, test_only=True,
    )
    _expect_refusal(
        lambda: _validate_publication_decision(
            {**valid_publication, "approved_operations": []},
            publication_request, synthetic_audit,
            now=now, test_only=True,
        ),
        "scope is partial",
    )
    _expect_refusal(
        lambda: _validate_publication_decision(
            {**valid_publication, "expires_at_utc": "2026-11-17T00:00:00Z"},
            publication_request, synthetic_audit,
            now=now, test_only=True,
        ),
        "expired",
    )
    _expect_refusal(
        lambda: _validate_publication_decision(
            {**valid_publication, "approved_oracles": valid_publication["approved_oracles"][:1]},
            publication_request, synthetic_audit,
            now=now, test_only=True,
        ),
        "omits an oracle",
    )
    trusted_expired_publication = {
        **valid_publication,
        "reference": "AUTHORED_TEST_ONLY:trusted bytes with expired time window",
        "issued_at_utc": "2026-11-17T00:00:00Z",
        "expires_at_utc": "2026-11-17T01:00:00Z",
    }
    expired_publication_record = {
        **record, "publication_decision": trusted_expired_publication,
    }
    publication_pin = _canonical_sha256(trusted_expired_publication)
    _expect_refusal(
        lambda: _require_publication_approved(
            expired_publication_record, trusted_pin=publication_pin, now=now,
            test_only=True,
        ),
        "expired",
    )
    _expect_refusal(
        lambda: _require_publication_approved(
            {**expired_publication_record, "publication_decision": {
                **trusted_expired_publication,
                "expires_at_utc": "2026-12-01T00:00:00Z",
            }},
            trusted_pin=publication_pin,
            now=now,
            test_only=True,
        ),
        "does not match its separate trusted pin",
    )
    _expect_refusal(
        lambda: _require_publication_approved({
            **record, "publication_decision": valid_publication,
        }),
        "no separate pinned publication decision",
    )
    _expect_refusal(
        lambda: _require_local_approved({
            **record,
            "local_decision": valid_test,
            "publication_decision": valid_publication,
        }),
        "cannot carry publication permission",
    )
    passed.extend([
        "publication-separated",
        "publication-partial-scope",
        "publication-expiry",
        "trusted-expired-publication-refusal",
        "altered-publication-pin-refusal",
        "publication-synthetic-live-refusal",
    ])

    _expect_refusal(
        lambda: _validate_approved_decision(
            valid_test, synthetic_request, rights_audit,
            now=now, allowed_domain="authored-test", test_only=True,
        ),
        "rights audit")
    passed.append("incomplete-rights-audit-refusal")

    bad_request = deepcopy(record)
    bad_request["request"]["oracles"][0]["revision_sha1"] = "0" * 40
    _expect_refusal(lambda: _validate_record(bad_request),
                    "authority request does not match the reviewed source pin")
    passed.append("request-revision-pin")
    return passed


def _main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--selftest", action="store_true")
    modes.add_argument("--expect-blocked", action="store_true")
    modes.add_argument("--require-approved", action="store_true")
    args = parser.parse_args(argv)
    try:
        record = _read_record()
        if args.selftest:
            passed = _run_selftests(record)
            for control in passed:
                print(f"PASS {control}")
            print(f"authority checker selftest passed ({len(passed)} controls)")
        elif args.expect_blocked:
            _expect_blocked(record)
            print("authority record valid; blocked: no oracle operation is authorized")
        else:
            _require_local_approved(record)
            print("competent local authority pin validated; publication remains separately gated")
    except (Refusal, AssertionError) as error:
        print(f"oracle-authority: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(_main(sys.argv[1:]))
