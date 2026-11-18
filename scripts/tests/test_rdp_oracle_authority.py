#!/usr/bin/env python3
"""Exercise the actual phase07 oracle-authority CLI entry points."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
from datetime import datetime, timedelta, timezone


ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "scripts/check-rdp-oracle-authority.py"
PRIVATE_TEST_ROOT = ROOT / "build/gate/07-12-task1/authority-repair-tests"
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


def ensure_private_test_root(root: Path = PRIVATE_TEST_ROOT) -> None:
    # Never chmod an existing path: a symlink must not change an unrelated target.
    relative = root.relative_to(ROOT)
    if not relative.parts or relative.parts[0] != "build":
        raise AssertionError("private test root must be beneath the ignored build directory")
    current = ROOT
    for component in relative.parts:
        current = current / component
        try:
            current.mkdir(mode=0o700)
        except FileExistsError:
            pass
        info = current.lstat()
        if not stat.S_ISDIR(info.st_mode) or current.resolve() != current:
            raise AssertionError("private test root contains a symlink or non-directory")
    if stat.S_IMODE(info.st_mode) != 0o700:
        raise AssertionError("private test root must already have mode 0700")


def private_temporary_directory(prefix: str) -> tempfile.TemporaryDirectory[str]:
    ensure_private_test_root()
    return tempfile.TemporaryDirectory(prefix=prefix, dir=PRIVATE_TEST_ROOT)


def verify_private_test_root() -> None:
    with private_temporary_directory("t64-oracle-test-root-") as tmp:
        scratch = Path(tmp)
        target = scratch / "unrelated-target"
        target.mkdir(mode=0o755)
        before_mode = stat.S_IMODE(target.stat().st_mode)
        link = scratch / "linked-root"
        link.symlink_to(target, target_is_directory=True)
        for candidate in (link, link / "child"):
            try:
                ensure_private_test_root(candidate)
            except AssertionError:
                pass
            else:
                raise AssertionError("symlink test root was accepted")
        if stat.S_IMODE(target.stat().st_mode) != before_mode or (target / "child").exists():
            raise AssertionError("refused test root changed its symlink target")


def invoke_forged_approval(*, publication: bool = False) -> subprocess.CompletedProcess[str]:
    """Run the real CLI against a complete mirror with self-declared grants."""
    with private_temporary_directory("t64-oracle-authority-test-") as tmp:
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
                "black-box-execution-on-a-future-ready-exported-corpus",
            ],
            "corpus_sha256": canonical_sha256(record["request"]["corpus"]),
            "rights_audit_sha256": canonical_sha256(record["rights_audit"]),
            "request_sha256": canonical_sha256(record["request"]),
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
                "request_sha256": canonical_sha256(record["request"]),
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


def verify_resolved_storage_root() -> None:
    spec = importlib.util.spec_from_file_location(
        "phase07_oracle_authority_checker", CHECKER,
    )
    if spec is None or spec.loader is None:
        raise AssertionError("could not load the authority checker for path controls")
    checker = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = checker
    spec.loader.exec_module(checker)
    with private_temporary_directory("t64-oracle-path-control-") as scratch_name:
        scratch = Path(scratch_name)
        worktree = scratch / "synthetic-worktree"
        inside = worktree / "private-oracle"
        outside = scratch / "synthetic-external-root"
        inside.mkdir(parents=True)
        outside.mkdir()
        link = outside / "redirect-to-worktree"
        link.symlink_to(inside, target_is_directory=True)
        resolved_sibling = checker._ensure_outside_worktree(
            outside / "oracle-checkout", worktree, "synthetic sibling root",
        )
        if resolved_sibling == outside.resolve():
            raise AssertionError("sibling checkout root was not resolved")
        try:
            checker._ensure_outside_worktree(
                link / "oracle-checkout", worktree, "synthetic symlink root",
            )
        except checker.Refusal as error:
            if "inside the Tendies64 worktree" not in str(error):
                raise
        else:
            raise AssertionError("symlink resolving into synthetic worktree was admitted")


def main() -> int:
    selftest = invoke("--selftest")
    self_output = selftest.stdout + selftest.stderr
    require(selftest.returncode == 0, "checker selftests did not pass", selftest)
    expected_controls = (
        "blocked-record",
        "out-of-tree-private-root",
        "nested-private-path-refusal",
        "duplicate-record-key",
        "missing-authority",
        "approved-scoped-synthetic",
        "local-request-revision-binding",
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
        "publication-request-revision-binding",
        "incomplete-rights-audit-refusal",
        "request-revision-pin",
    )
    actual_controls = tuple(
        line.removeprefix("PASS ")
        for line in self_output.splitlines()
        if line.startswith("PASS ")
    )
    require(
        actual_controls == expected_controls
        and f"authority checker selftest passed ({len(expected_controls)} controls)" in self_output,
        "checker control inventory differs from the exact required set/count",
        selftest,
    )

    verify_private_test_root()
    try:
        verify_resolved_storage_root()
    except Exception as error:
        raise AssertionError(f"resolved checkout/build root control failed: {error}") from error

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
