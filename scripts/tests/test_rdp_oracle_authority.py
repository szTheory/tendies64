#!/usr/bin/env python3
"""Exercise the actual phase07 oracle-authority CLI entry points."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime, timedelta, timezone


ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "scripts/check-rdp-oracle-authority.py"
BOUND_FILES = (
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
)


def invoke(
    flag: str,
    *,
    checker: Path = CHECKER,
    cwd: Path = ROOT,
) -> subprocess.CompletedProcess[str]:
    environment = os.environ.copy()
    environment["PYTHONDONTWRITEBYTECODE"] = "1"
    return subprocess.run(
        [sys.executable, "-B", str(checker), flag],
        cwd=cwd,
        env=environment,
        text=True,
        capture_output=True,
        timeout=20,
        check=False,
    )


def canonical_sha256(value: object) -> str:
    encoded = json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=True,
        allow_nan=False,
    ).encode("ascii")
    return hashlib.sha256(encoded).hexdigest()


def invoke_forged_approval(*, publication: bool = False) -> subprocess.CompletedProcess[str]:
    """Run the real CLI against a complete mirror with self-declared grants."""
    with tempfile.TemporaryDirectory(prefix="t64-oracle-authority-test-") as tmp:
        mirror = Path(tmp)
        copy_paths = (*BOUND_FILES, "docs/evidence/phase07-oracle-authority.json",
                      "scripts/check-rdp-oracle-authority.py")
        for relative in copy_paths:
            destination = mirror / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, destination)

        record_path = mirror / "docs/evidence/phase07-oracle-authority.json"
        record = json.loads(record_path.read_text(encoding="utf-8"))
        now = datetime.now(timezone.utc)
        utc = lambda value: value.isoformat(timespec="seconds").replace("+00:00", "Z")
        record["local_decision"] = {
            "status": "approved",
            "decision_domain": "owner-legal",
            "authority_role": "owner",
            "reference": "self-declared; no competent decision exists",
            "evidence_sha256": "a" * 64,
            "issued_at_utc": utc(now - timedelta(minutes=1)),
            "expires_at_utc": utc(now + timedelta(days=1)),
            "approved_oracles": [
                {"id": item["id"], "revision_sha1": item["revision_sha1"]}
                for item in record["request"]["oracles"]
            ],
            "approved_operations": [
                "out-of-tree-acquisition",
                "bounded-build",
                "black-box-execution-on-the-authored-corpus",
            ],
            "corpus_sha256": canonical_sha256(record["request"]["corpus"]),
            "rights_audit_sha256": canonical_sha256(record["rights_audit"]),
        }
        if publication:
            record["publication_decision"] = {
                "status": "approved",
                "decision_domain": "owner-legal",
                "authority_role": "owner",
                "reference": "self-declared publication grant; no competent decision exists",
                "evidence_sha256": "c" * 64,
                "issued_at_utc": utc(now - timedelta(minutes=1)),
                "expires_at_utc": utc(now + timedelta(days=1)),
                "approved_oracles": [
                    {"id": item["id"], "revision_sha1": item["revision_sha1"]}
                    for item in record["request"]["oracles"]
                ],
                "approved_operations": ["publish-oracle-derived-bytes-and-expectations"],
                "corpus_sha256": canonical_sha256(record["request"]["corpus"]),
                "rights_audit_sha256": canonical_sha256(record["rights_audit"]),
                "publication_scope": "oracle-derived-bytes-and-expectations",
            }
        record_path.write_text(
            json.dumps(record, indent=2, ensure_ascii=True) + "\n",
            encoding="utf-8",
        )
        return invoke(
            "--require-approved",
            checker=mirror / "scripts/check-rdp-oracle-authority.py",
            cwd=mirror,
        )


def require(condition: bool, message: str, result: subprocess.CompletedProcess[str]) -> None:
    if condition:
        return
    detail = (result.stdout + result.stderr).strip()
    raise AssertionError(f"{message}; exit={result.returncode}; output={detail!r}")


def main() -> int:
    selftest = invoke("--selftest")
    self_output = selftest.stdout + selftest.stderr
    require(selftest.returncode == 0, "checker selftests did not pass", selftest)
    for control in (
        "blocked-record",
        "duplicate-record-key",
        "approved-scoped-synthetic",
        "missing-authority",
        "partial-scope",
        "revision-mismatch",
        "expired-decision",
        "trusted-expired-local-refusal",
        "altered-local-pin-refusal",
        "synthetic-live-refusal",
        "publication-separated",
        "publication-partial-scope",
        "publication-expiry",
        "trusted-expired-publication-refusal",
        "altered-publication-pin-refusal",
        "publication-synthetic-live-refusal",
    ):
        require(control in self_output, f"selftest omitted {control}", selftest)

    blocked = invoke("--expect-blocked")
    require(
        blocked.returncode == 0
        and "blocked: no oracle operation is authorized" in blocked.stdout,
        "actual checker did not accept the known blocked authority record",
        blocked,
    )

    live = invoke("--require-approved")
    require(
        live.returncode == 2
        and "no pinned competent owner/legal decision" in live.stderr,
        "live pre-use gate did not refuse the absent competent authority pin",
        live,
    )

    forged = invoke_forged_approval()
    require(
        forged.returncode == 2
        and "no pinned competent owner/legal decision" in forged.stderr,
        "actual live CLI accepted a self-declared approval record",
        forged,
    )
    forged_publication = invoke_forged_approval(publication=True)
    require(
        forged_publication.returncode == 2
        and "cannot carry publication permission" in forged_publication.stderr,
        "local live CLI accepted a self-declared publication grant",
        forged_publication,
    )
    print("rdp-oracle-authority: blocked, forged local, and forged publication records refused")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
