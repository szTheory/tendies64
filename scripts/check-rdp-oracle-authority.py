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
import tempfile
from typing import Any
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
RECORD_PATH = ROOT / "docs/evidence/phase07-oracle-authority.json"
PINNED_REQUEST_SHA256 = "75f2b651394825ade3a2661950b889522a59b03e2fbc21c65c70f271adda2779"
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
    "black-box-execution-on-a-future-ready-exported-corpus",
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
    "request_sha256",
}
EXPECTED_PUBLICATION_KEYS = {
    "status", "decision_domain", "authority_role", "reference",
    "evidence_sha256", "issued_at_utc", "expires_at_utc",
    "approved_oracles", "approved_operations", "corpus_sha256",
    "rights_audit_sha256", "publication_scope", "request_sha256",
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
    "request_sha256": None,
}
EXPECTED_PUBLICATION_OPERATIONS = ["publish-oracle-derived-bytes-and-expectations"]
EXPECTED_PUBLICATION_SCOPE = "oracle-derived-bytes-and-expectations"
HEX_64 = re.compile(r"^[0-9a-f]{64}$")
HEX_40 = re.compile(r"^[0-9a-f]{40}$")
PRIVATE_PATH = re.compile(
    r"(?i)(?:^|[^A-Z0-9])/(?:Users|home|private)(?:/|$)"
    r"|(?:^|[^A-Z0-9])[A-Z]:[\\/](?:Users|Documents and Settings|home|private)(?:[\\/]|$)"
)


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
        if PRIVATE_PATH.search(value):
            raise Refusal("authority record contains a workstation-specific path")
        return
    if isinstance(value, list):
        for item in value:
            _refuse_private_paths(item)
    elif isinstance(value, dict):
        for item in value.values():
            _refuse_private_paths(item)


def _ensure_outside_worktree(candidate: Path, worktree: Path, label: str) -> Path:
    """Resolve existing symlinks without creating paths, and reject in-tree roots."""
    resolved_candidate = candidate.resolve(strict=False)
    resolved_worktree = worktree.resolve(strict=False)
    if resolved_candidate == resolved_worktree or \
            resolved_worktree in resolved_candidate.parents:
        raise Refusal(f"{label} resolves inside the Tendies64 worktree")
    return resolved_candidate


def _current_temporary_root() -> Path:
    configured = os.environ.get("TMPDIR")
    temporary_root = Path(configured) if configured else Path(tempfile.gettempdir())
    if not temporary_root.is_absolute():
        raise Refusal("TMPDIR must resolve to an absolute private storage root")
    return temporary_root.resolve(strict=False)


def _validate_storage_proposal(request: dict[str, Any]) -> None:
    storage = request.get("storage_and_output_limits")
    if not isinstance(storage, dict):
        raise Refusal("proposed private storage limits are missing")
    if storage.get("directory_mode") != "0700":
        raise Refusal("proposed private storage directory mode must be 0700")
    template = storage.get("private_root_template")
    prefix = "${TMPDIR}/"
    if not isinstance(template, str) or not template.startswith(prefix):
        raise Refusal("proposed private root must be relative to TMPDIR")
    relative = template[len(prefix):].rstrip("/")
    relative_path = Path(relative)
    if not relative or relative_path.is_absolute() or ".." in relative_path.parts or \
            "\\" in relative:
        raise Refusal("proposed private root template is not a safe relative path")
    if relative_path.parts[-3:] != ("<oracle-id>", "<revision>", "<run-id>"):
        raise Refusal("proposed private root must bind oracle, revision, and run identity")
    root_prefix = Path(*relative_path.parts[:-3])
    temporary_root = _current_temporary_root()
    for oracle_id, revision in EXPECTED_ORACLES:
        _ensure_outside_worktree(
            temporary_root / root_prefix / oracle_id / revision / "admission-check",
            ROOT,
            "proposed checkout/build root",
        )

    caps = (
        "checkout_plus_build_max_bytes", "outputs_max_bytes_total",
        "output_max_bytes_per_case", "case_count_max",
    )
    for cap in caps:
        value = storage.get(cap)
        if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
            raise Refusal(f"proposed storage cap is missing or invalid: {cap}")
    wall_caps = storage.get("wall_clock_caps_seconds")
    if not isinstance(wall_caps, dict) or set(wall_caps) != {
        "acquisition", "build", "per_case", "aggregate",
    } or any(not isinstance(value, int) or isinstance(value, bool) or value <= 0
             for value in wall_caps.values()):
        raise Refusal("proposed acquisition/build/case/aggregate wall caps are invalid")
    cleanup = storage.get("process_group_cleanup")
    if cleanup != {
        "on_timeout": "SIGTERM the complete isolated process group",
        "grace_period_seconds": 5,
        "after_grace": "SIGKILL the complete process group and verify it exited",
        "result": "record timeout and refuse without retry",
    }:
        raise Refusal("proposed process-group timeout cleanup policy is incomplete")
    if storage.get("limits_status") != "proposed-only-no-window-authorized":
        raise Refusal("proposed limits must not be represented as an active run window")


def _validate_differential_preparation(request: dict[str, Any]) -> None:
    preparation = request.get("differential_preparation")
    if not isinstance(preparation, dict) or set(preparation) != {
        "inventory_kind", "inventory_suites", "inventory_rows",
        "case_classification", "case_mapping", "exported_corpus",
        "replay_adapter", "execution_status",
    }:
        raise Refusal("differential preparation and replay readiness are missing")
    if preparation.get("inventory_kind") != "suite-scoped-native-membership-rows" or \
            preparation.get("inventory_suites") != 11 or \
            preparation.get("inventory_rows") != 216:
        raise Refusal("216-row inventory must remain identified as native membership")
    if preparation.get("execution_status") not in {
        "blocked-not-prepared", "ready",
    }:
        raise Refusal("differential execution readiness status is invalid")
    for key in ("case_classification", "case_mapping", "exported_corpus", "replay_adapter"):
        row = preparation.get(key)
        if not isinstance(row, dict) or set(row) != {"status", "id", "sha256", "case_count"}:
            raise Refusal(f"differential {key} identity is malformed")
        if row["status"] not in {"blocked", "missing", "ready"}:
            raise Refusal(f"differential {key} readiness is invalid")
        if row["status"] == "ready":
            if not isinstance(row["id"], str) or not row["id"].strip() or \
                    not HEX_64.fullmatch(str(row["sha256"])):
                raise Refusal(f"ready differential {key} lacks a pinned identity")
            if row["case_count"] is not None and (
                not isinstance(row["case_count"], int) or
                isinstance(row["case_count"], bool) or
                row["case_count"] <= 0 or row["case_count"] > 216
            ):
                raise Refusal(f"ready differential {key} has an invalid case count")
        elif row["id"] is not None or row["sha256"] is not None or row["case_count"] is not None:
            raise Refusal(f"blocked differential {key} must not claim an identity")
    statuses = [preparation[key]["status"] for key in (
        "case_classification", "case_mapping", "exported_corpus", "replay_adapter",
    )]
    if preparation["execution_status"] == "ready" and any(
        status != "ready" for status in statuses
    ):
        raise Refusal("differential execution is marked ready with blocked inputs")
    if preparation["execution_status"] == "blocked-not-prepared" and any(
        status == "ready" for status in statuses
    ):
        raise Refusal("differential readiness summary disagrees with component rows")
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
    if set(fixture_manifest) != {"path", "bound_rows"} or \
            fixture_manifest.get("path") != "fixtures/manifest.json":
        raise Refusal("authored fixture manifest identity is malformed")
    fixture_path = _safe_repo_path(fixture_manifest.get("path"))
    fixture_bytes = fixture_path.read_bytes()
    bound_rows = fixture_manifest.get("bound_rows")
    if not isinstance(bound_rows, list) or not bound_rows:
        raise Refusal("fixture manifest bound row set is empty")

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
    # GATE-02: each manifest row present at the rebind is pinned by its own
    # canonical hash and looked up by id (never by position). Rows appended
    # later are outside this request and need no rebind. A missing, duplicated
    # or changed bound row, or an empty bound set, refuses.
    bound_ids: list[Any] = []
    for bound in bound_rows:
        if not isinstance(bound, dict) or set(bound) != {"id", "row_sha256"} or \
                not isinstance(bound["id"], str) or not bound["id"] or \
                not HEX_64.fullmatch(str(bound["row_sha256"])):
            raise Refusal("fixture manifest bound row is malformed")
        bound_ids.append(bound["id"])
    if len(bound_ids) != len(set(bound_ids)):
        raise Refusal("fixture manifest bound rows are duplicated")
    for bound in bound_rows:
        matches = [
            entry for entry in fixture_rows
            if isinstance(entry, dict) and entry.get("id") == bound["id"]
        ]
        if len(matches) != 1:
            raise Refusal(
                f"fixture manifest bound row is missing or duplicated: {bound['id']}"
            )
        if _canonical_sha256(matches[0]) != bound["row_sha256"]:
            raise Refusal(f"fixture manifest bound row changed: {bound['id']}")
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


def _validate_record(
    record: dict[str, Any], *, test_only: bool = False,
) -> tuple[dict[str, Any], dict[str, Any], str]:
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
    if request.get("purpose") != "Prepare a bounded local-only comparison request pending a rights-cleared exported corpus and replay adapter." or \
            request.get("evidence_label") != "emulator-to-emulator differential only; never hardware truth":
        raise Refusal("authority request purpose or evidence label changed")
    _validate_storage_proposal(request)
    _validate_differential_preparation(request)
    local_operations = request.get("local_operations")
    if not isinstance(local_operations, dict) or \
            local_operations.get("proposed_only") != EXPECTED_OPERATIONS or \
            local_operations.get("authorized_now") != [
                "public-repository-license-and-source-metadata-inspection-only",
            ]:
        raise Refusal("local oracle operation proposal or current authorization changed")
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
    audit_complete = rights_audit.get("complete")
    if not isinstance(audit_complete, bool) or \
            (audit_complete and not test_only) or \
            not isinstance(audit_items, list):
        raise Refusal("complete rights audit is permitted only in authored test context")
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
        "request_sha256": None,
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
    if decision.get("request_sha256") != _canonical_sha256(request):
        raise Refusal("decision is not bound to the complete authority request")
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
    _validate_differential_preparation(request)
    if request["differential_preparation"]["execution_status"] != "ready":
        raise Refusal(
            "exported corpus, case mapping, classification, and replay adapter must be ready "
            "before live execution approval"
        )
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
    if decision.get("request_sha256") != _canonical_sha256(request):
        raise Refusal("publication decision is not bound to the complete authority request")
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
    request, rights_audit, _corpus_sha = _validate_record(record, test_only=test_only)
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
    request, rights_audit, _corpus_sha = _validate_record(record, test_only=test_only)
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
        "request_sha256": _canonical_sha256(request),
    }
    if overrides:
        decision.update(overrides)
    return decision


def _run_selftests(record: dict[str, Any]) -> list[str]:
    request, rights_audit, _corpus_sha = _validate_record(record)
    _expect_blocked(record)
    passed = ["blocked-record"]

    synthetic_worktree = Path(tempfile.gettempdir()) / "t64-synthetic-worktree"
    _ensure_outside_worktree(
        Path(tempfile.gettempdir()) / "t64-synthetic-oracle",
        synthetic_worktree,
        "synthetic sibling oracle root",
    )
    _expect_refusal(
        lambda: _ensure_outside_worktree(
            synthetic_worktree / "build/private/oracle",
            synthetic_worktree,
            "synthetic checkout/build root",
        ),
        "inside the Tendies64 worktree",
    )
    _validate_storage_proposal(request)
    public_mode_request = deepcopy(request)
    public_mode_request["storage_and_output_limits"]["directory_mode"] = "0777"
    _expect_refusal(
        lambda: _validate_storage_proposal(public_mode_request),
        "directory mode must be 0700",
    )
    passed.append("out-of-tree-private-root")

    # Build examples from fragments so the project PII scanner does not mistake
    # authored test cases for workstation paths in the checked-in source.
    slash = "/"
    backslash = chr(92)
    for private_text in (
        {"reference": "Approval noted in " + slash + "Users/example/decision.pdf"},
        {"nested": {"url": "file:" + slash * 3 + "private/var/approval.json"}},
        {"list": ["See C:" + backslash + "Users" + backslash + "example" + backslash + "approval.pdf"]},
    ):
        _expect_refusal(
            lambda private_text=private_text: _refuse_private_paths(private_text),
            "workstation-specific path",
        )
    passed.append("nested-private-path-refusal")

    _expect_refusal(
        lambda: _no_duplicate_keys([("status", "blocked"), ("status", "approved")]),
        "duplicate JSON key",
    )
    passed.append("duplicate-record-key")

    def _mutated_fixture_manifest(mutate: Any) -> dict[str, Any]:
        changed = deepcopy(request)
        mutate(changed["corpus"]["fixture_manifest"])
        return changed

    for name, mutate, needle in (
        ("fixture-manifest-empty-bound-rows",
         lambda value: value.update({"bound_rows": []}), "bound row set is empty"),
        ("fixture-manifest-duplicate-bound-id",
         lambda value: value["bound_rows"].append(dict(value["bound_rows"][0])),
         "duplicated"),
        ("fixture-manifest-bound-row-changed",
         lambda value: value["bound_rows"][0].update({"row_sha256": "0" * 64}),
         "bound row changed"),
        ("fixture-manifest-bound-row-missing",
         lambda value: value["bound_rows"][0].update({"id": "no-such-row"}),
         "missing or duplicated"),
    ):
        _expect_refusal(
            lambda changed=_mutated_fixture_manifest(mutate): _verify_corpus(changed),
            needle,
        )
        passed.append(name)

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
    for readiness_key in (
        "case_classification", "case_mapping", "exported_corpus", "replay_adapter",
    ):
        synthetic_request["differential_preparation"][readiness_key] = {
            "status": "ready",
            "id": f"AUTHORED_TEST_ONLY:{readiness_key}",
            "sha256": "e" * 64,
            "case_count": 1,
        }
    synthetic_request["differential_preparation"]["execution_status"] = "ready"
    valid_test = _synthetic_decision(
        synthetic_request, synthetic_audit, expires_at=future,
    )
    synthetic_record = deepcopy(record)
    synthetic_record["request"] = synthetic_request
    synthetic_record["request_sha256"] = _canonical_sha256(synthetic_request)
    synthetic_record["rights_audit"] = synthetic_audit
    synthetic_record["local_decision"] = valid_test
    with patch.multiple(
        sys.modules[__name__],
        PINNED_REQUEST_SHA256=synthetic_record["request_sha256"],
        PINNED_RIGHTS_AUDIT_SHA256=_canonical_sha256(synthetic_audit),
    ):
        _require_local_approved(
            synthetic_record,
            trusted_pin=_canonical_sha256(valid_test),
            now=now,
            test_only=True,
        )
    passed.append("approved-scoped-synthetic")

    request_mutators = (
        lambda value: value["storage_and_output_limits"].update({
            "private_root_template": "${TMPDIR}/t64-oracle-private-alt/phase07/<oracle-id>/<revision>/<run-id>/",
        }),
        lambda value: value["storage_and_output_limits"].update({
            "outputs_max_bytes_total": value["storage_and_output_limits"]["outputs_max_bytes_total"] + 1,
        }),
        lambda value: value["oracles"][0]["build_recipe"].update({
            "commands": [["cmake", "--build", "build"]],
        }),
        lambda value: value["local_operations"]["forbidden_until_live_gate_passes"].append(
            "additional-operation-restriction"
        ),
    )
    for mutate_request in request_mutators:
        changed_request = deepcopy(synthetic_request)
        mutate_request(changed_request)
        changed_sha = _canonical_sha256(changed_request)
        changed_record = {
            **synthetic_record,
            "request": changed_request,
            "request_sha256": changed_sha,
        }
        with patch.multiple(
            sys.modules[__name__],
            PINNED_REQUEST_SHA256=changed_sha,
            PINNED_RIGHTS_AUDIT_SHA256=_canonical_sha256(synthetic_audit),
        ):
            _expect_refusal(
                lambda changed_record=changed_record: _require_local_approved(
                    changed_record,
                    trusted_pin=_canonical_sha256(valid_test),
                    now=now,
                    test_only=True,
                ),
                "not bound to the complete authority request",
            )
    passed.append("local-request-revision-binding")

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
    expired_local_record = {
        **synthetic_record,
        "local_decision": trusted_expired_local,
    }
    local_pin = _canonical_sha256(trusted_expired_local)
    with patch.multiple(
        sys.modules[__name__],
        PINNED_REQUEST_SHA256=synthetic_record["request_sha256"],
        PINNED_RIGHTS_AUDIT_SHA256=_canonical_sha256(synthetic_audit),
    ):
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
    live_context_decision = {
        **valid_test,
        "rights_audit_sha256": _canonical_sha256(rights_audit),
    }
    live_context_record = {
        **synthetic_record,
        "rights_audit": rights_audit,
        "local_decision": live_context_decision,
    }
    with patch.multiple(
        sys.modules[__name__],
        PINNED_REQUEST_SHA256=synthetic_record["request_sha256"],
        PINNED_RIGHTS_AUDIT_SHA256=_canonical_sha256(rights_audit),
    ):
        _expect_refusal(
            lambda: _require_local_approved(
                live_context_record,
                trusted_pin=_canonical_sha256(live_context_decision),
                now=now,
                test_only=False,
            ),
            "required authority domain",
        )
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
        "request_sha256": _canonical_sha256(publication_request),
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
    synthetic_publication_record = {
        **synthetic_record,
        "request": publication_request,
        "request_sha256": _canonical_sha256(publication_request),
        "local_decision": record["local_decision"],
        "publication_decision": valid_publication,
    }
    expired_publication_record = {
        **synthetic_publication_record,
        "publication_decision": trusted_expired_publication,
    }
    publication_pin = _canonical_sha256(trusted_expired_publication)
    with patch.multiple(
        sys.modules[__name__],
        PINNED_REQUEST_SHA256=synthetic_publication_record["request_sha256"],
        PINNED_RIGHTS_AUDIT_SHA256=_canonical_sha256(synthetic_audit),
    ):
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
            lambda: _require_publication_approved(
                synthetic_publication_record, now=now, test_only=True,
            ),
            "no separate pinned publication decision",
        )
        _expect_refusal(
            lambda: _require_local_approved({
                **synthetic_publication_record,
                "publication_decision": valid_publication,
            }, trusted_pin=_canonical_sha256(valid_test), now=now, test_only=True),
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

    for mutate_request in request_mutators:
        changed_request = deepcopy(publication_request)
        mutate_request(changed_request)
        changed_sha = _canonical_sha256(changed_request)
        changed_publication_record = {
            **synthetic_publication_record,
            "request": changed_request,
            "request_sha256": changed_sha,
            "local_decision": record["local_decision"],
            "publication_decision": valid_publication,
        }
        with patch.multiple(
            sys.modules[__name__],
            PINNED_REQUEST_SHA256=changed_sha,
            PINNED_RIGHTS_AUDIT_SHA256=_canonical_sha256(synthetic_audit),
        ):
            _expect_refusal(
                lambda changed_publication_record=changed_publication_record: \
                    _require_publication_approved(
                        changed_publication_record,
                        trusted_pin=_canonical_sha256(valid_publication),
                        now=now,
                        test_only=True,
                    ),
                "not bound to the complete authority request",
            )
    passed.append("publication-request-revision-binding")

    incomplete_rights_decision = _synthetic_decision(
        synthetic_request, rights_audit, expires_at=future,
    )
    _expect_refusal(
        lambda: _validate_approved_decision(
            incomplete_rights_decision, synthetic_request, rights_audit,
            now=now, allowed_domain="authored-test", test_only=True,
        ),
        "rights row remains unresolved",
    )
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
