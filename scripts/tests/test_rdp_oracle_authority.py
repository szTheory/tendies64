#!/usr/bin/env python3
"""Exercise the actual phase07 oracle-authority CLI entry points."""

from __future__ import annotations

import contextlib
import copy
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
from collections.abc import Callable, Iterator
from datetime import datetime, timedelta, timezone
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "scripts/check-rdp-oracle-authority.py"
PRIVATE_TEST_ROOT = ROOT / "build/gate/07-12-task1/authority-repair-tests"
RECORD_RELATIVE = "docs/evidence/phase07-oracle-authority.json"
CHECKER_RELATIVE = "scripts/check-rdp-oracle-authority.py"
MANIFEST_RELATIVE = "fixtures/manifest.json"
# Scratch mirrors must stay complete after the manifest leaves BOUND_FILES
# (GATE-02 binds its rows individually, not the whole file).
MIRROR_EXTRA = (MANIFEST_RELATIVE,)
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


def populate_mirror(mirror: Path) -> None:
    """Copy everything the real CLI reads into a scratch root; mutate only the copy."""
    copied: set[str] = set()
    for relative in (*BOUND_FILES, *MIRROR_EXTRA, RECORD_RELATIVE, CHECKER_RELATIVE):
        if relative in copied:
            continue
        copied.add(relative)
        destination = mirror / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / relative, destination)


def invoke_forged_approval(*, publication: bool = False) -> subprocess.CompletedProcess[str]:
    """Run the real CLI against a complete mirror with self-declared grants."""
    with private_temporary_directory("t64-oracle-authority-test-") as tmp:
        mirror = Path(tmp)
        populate_mirror(mirror)

        record_path = mirror / RECORD_RELATIVE
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


class SetupError(Exception):
    """The scratch mirror or its mutation is invalid, so a case result would mean nothing."""


BOUND_IDS = ("rdp-fill", "rdp-fill-payload")
PROBE_ROW_ID = "phase10-binding-probe"
MANIFEST_TAIL = "\n  ]\n}\n"
BLOCKED_TEXT = "blocked: no oracle operation is authorized"
PIN_REFUSAL_TEXT = "no pinned competent owner/legal decision"


@contextlib.contextmanager
def scratch_mirror() -> Iterator[Path]:
    # A private directory per case: parallel CTest jobs never share a mirror and
    # nothing here writes to the repository.
    with private_temporary_directory("t64-oracle-binding-") as tmp:
        mirror = Path(tmp)
        populate_mirror(mirror)
        yield mirror


def run_in(mirror: Path, flag: str) -> subprocess.CompletedProcess[str]:
    return invoke(flag, checker=mirror / CHECKER_RELATIVE, cwd=mirror)


def load_manifest(mirror: Path) -> dict[str, Any]:
    data = json.loads((mirror / MANIFEST_RELATIVE).read_text(encoding="utf-8"))
    if not isinstance(data, dict) or not isinstance(data.get("entries"), list):
        raise SetupError("scratch manifest has no entries list")
    return data


def store_manifest(mirror: Path, data: dict[str, Any], *, indent: int = 2) -> None:
    text = json.dumps(data, indent=indent, ensure_ascii=True) + "\n"
    (mirror / MANIFEST_RELATIVE).write_text(text, encoding="utf-8")


def find_row(data: dict[str, Any], row_id: str) -> dict[str, Any]:
    matches = [row for row in data["entries"] if row.get("id") == row_id]
    if len(matches) != 1:
        raise SetupError(f"scratch manifest does not hold exactly one row {row_id!r}")
    return matches[0]


def splice_rows(mirror: Path, rows: list[dict[str, Any]]) -> None:
    """Append rows by text so every existing byte of the manifest stays as it was."""
    path = mirror / MANIFEST_RELATIVE
    before = len(load_manifest(mirror)["entries"])
    text = path.read_text(encoding="utf-8")
    if not text.endswith(MANIFEST_TAIL):
        raise SetupError("manifest layout changed; cannot append a row by text")
    chunks = []
    for row in rows:
        body = json.dumps(row, indent=2, ensure_ascii=True)
        chunks.append(",\n" + "\n".join("    " + line for line in body.splitlines()))
    path.write_text(text[: -len(MANIFEST_TAIL)] + "".join(chunks) + MANIFEST_TAIL,
                    encoding="utf-8")
    if len(load_manifest(mirror)["entries"]) != before + len(rows):
        raise SetupError("appended manifest rows were not parsed back")


def probe_row(mirror: Path) -> dict[str, Any]:
    """An unrelated authored row, shaped like the Phase 9 rows already in the manifest."""
    row = copy.deepcopy(find_row(load_manifest(mirror), "phase09-save-roundtrip-s32k"))
    row["id"] = PROBE_ROW_ID
    return row


def flip_hex(value: str) -> str:
    return ("1" if value[0] == "0" else "0") + value[1:]


def change_field(
    row_id: str, field: str, change: Callable[[Any], Any],
) -> Callable[[dict[str, Any]], None]:
    def apply(data: dict[str, Any]) -> None:
        row = find_row(data, row_id)
        row[field] = change(row[field])
    return apply


def remove_row(row_id: str) -> Callable[[dict[str, Any]], None]:
    def apply(data: dict[str, Any]) -> None:
        find_row(data, row_id)
        data["entries"] = [row for row in data["entries"] if row.get("id") != row_id]
    return apply


def expect_valid_blocked(result: subprocess.CompletedProcess[str], what: str) -> None:
    require(result.returncode == 0 and BLOCKED_TEXT in result.stdout, what, result)


def expect_refused(result: subprocess.CompletedProcess[str], what: str) -> None:
    # A refusal is the checker's own controlled exit 2, never a crash.
    require(
        result.returncode == 2
        and result.stderr.startswith("oracle-authority:")
        and "Traceback" not in result.stderr,
        what,
        result,
    )


def refuse_after(
    label: str, mutate: Callable[[dict[str, Any]], None], what: str,
) -> None:
    with scratch_mirror() as mirror:
        data = load_manifest(mirror)
        mutate(data)
        store_manifest(mirror, data)
        expect_refused(run_in(mirror, "--expect-blocked"), f"{what}: {label}")


def verify_mirror_baseline() -> None:
    """An unchanged mirror must be valid, or no case below can be trusted."""
    with scratch_mirror() as mirror:
        result = run_in(mirror, "--expect-blocked")
        if result.returncode != 0 or BLOCKED_TEXT not in result.stdout:
            raise SetupError(
                "an unmodified scratch mirror is not accepted as valid-and-blocked; "
                f"exit={result.returncode}; output={(result.stdout + result.stderr).strip()!r}"
            )


def case_append_unrelated_row() -> None:
    with scratch_mirror() as mirror:
        splice_rows(mirror, [probe_row(mirror)])
        expect_valid_blocked(
            run_in(mirror, "--expect-blocked"),
            "appending an unrelated authored row to the manifest changed the blocked verdict",
        )
        approval = run_in(mirror, "--require-approved")
        require(
            approval.returncode == 2 and PIN_REFUSAL_TEXT in approval.stderr,
            "an appended row changed the live pre-use refusal",
            approval,
        )


def case_old_whole_file_mutation() -> None:
    with scratch_mirror() as mirror:
        path = mirror / MANIFEST_RELATIVE
        original = path.read_bytes()
        data = load_manifest(mirror)
        row_hashes = [canonical_sha256(row) for row in data["entries"]]
        store_manifest(mirror, data, indent=4)
        if path.read_bytes() == original:
            raise SetupError("the reformatted manifest is byte-identical to the original")
        if row_hashes != [canonical_sha256(row) for row in load_manifest(mirror)["entries"]]:
            raise SetupError("the reformat changed a row's canonical bytes")
        expect_valid_blocked(
            run_in(mirror, "--expect-blocked"),
            "a whitespace-only reformat that keeps every row's canonical bytes was refused",
        )


def case_bound_row_changed() -> None:
    changes: tuple[tuple[str, Callable[[dict[str, Any]], None]], ...] = (
        ("rdp-fill sha256", change_field("rdp-fill", "sha256", flip_hex)),
        ("rdp-fill size", change_field("rdp-fill", "size", lambda value: value + 1)),
        ("rdp-fill source_rights",
         change_field("rdp-fill", "source_rights", lambda value: "pending")),
        ("rdp-fill binary_rights",
         change_field("rdp-fill", "binary_rights", lambda value: "pending")),
        ("rdp-fill distribution",
         change_field("rdp-fill", "distribution", lambda value: ["local-only"])),
        ("rdp-fill-payload sha256", change_field("rdp-fill-payload", "sha256", flip_hex)),
    )
    for label, mutate in changes:
        refuse_after(label, mutate, "a changed bound row was accepted")


def case_existing_row_changed() -> None:
    # Rows the oracle request does not consume lose no protection: the whole-file
    # hash covered them, so their own row hashes must.
    changes: tuple[tuple[str, Callable[[dict[str, Any]], None]], ...] = (
        ("hello-openipl3 sha256", change_field("hello-openipl3", "sha256", flip_hex)),
        ("hello-openipl3 rights_evidence",
         change_field("hello-openipl3", "rights_evidence", lambda value: value + " ")),
        ("hello-openipl3 row removed", remove_row("hello-openipl3")),
    )
    for label, mutate in changes:
        refuse_after(label, mutate, "a changed existing row was accepted")


def case_reorder_bound_rows() -> None:
    def bound_to_end(data: dict[str, Any]) -> None:
        moved = [row for row in data["entries"] if row.get("id") in BOUND_IDS]
        rest = [row for row in data["entries"] if row.get("id") not in BOUND_IDS]
        if len(moved) != len(BOUND_IDS) or data["entries"] == rest + moved:
            raise SetupError("the bound rows were not moved to another position")
        data["entries"] = rest + moved

    def reversed_rows(data: dict[str, Any]) -> None:
        data["entries"] = list(reversed(data["entries"]))

    for label, mutate in (
        ("bound rows moved to the end", bound_to_end),
        ("every row in reverse order", reversed_rows),
    ):
        with scratch_mirror() as mirror:
            data = load_manifest(mirror)
            mutate(data)
            store_manifest(mirror, data)
            expect_valid_blocked(
                run_in(mirror, "--expect-blocked"),
                f"rows are looked up by id, not position, but a reorder was refused: {label}",
            )


def case_duplicate_bound_id() -> None:
    with scratch_mirror() as mirror:
        splice_rows(mirror, [copy.deepcopy(find_row(load_manifest(mirror), "rdp-fill"))])
        expect_refused(
            run_in(mirror, "--expect-blocked"),
            "a duplicated bound id was accepted: identical copy appended",
        )

    def altered_copy_first(data: dict[str, Any]) -> None:
        original = find_row(data, "rdp-fill")
        altered = copy.deepcopy(original)
        altered["sha256"] = flip_hex(altered["sha256"])
        data["entries"].insert(data["entries"].index(original), altered)

    # A lookup that keeps the last row of a duplicated id would accept this.
    refuse_after("altered copy before the original", altered_copy_first,
                 "a duplicated bound id was accepted")


def case_same_bytes_other_id() -> None:
    def other_id(data: dict[str, Any]) -> None:
        original = find_row(data, "rdp-fill")
        clone = copy.deepcopy(original)
        clone["id"] = "rdp-fill-clone"
        data["entries"][data["entries"].index(original)] = clone

    refuse_after("rdp-fill replaced by its own bytes under another id", other_id,
                 "binding by hash alone accepted a row under another id")


def case_empty() -> None:
    with scratch_mirror() as mirror:
        (mirror / MANIFEST_RELATIVE).unlink()
        expect_refused(run_in(mirror, "--expect-blocked"),
                       "a missing manifest passed as valid-and-blocked")

    def empty_rows(data: dict[str, Any]) -> None:
        data["entries"] = []

    def no_rows_key(data: dict[str, Any]) -> None:
        del data["entries"]

    refuse_after("empty entries list", empty_rows, "an empty manifest passed")
    refuse_after("entries key absent", no_rows_key, "a manifest without rows passed")


def case_require_approved() -> None:
    with scratch_mirror() as mirror:
        approval = run_in(mirror, "--require-approved")
    require(
        approval.returncode == 2 and PIN_REFUSAL_TEXT in approval.stderr,
        "the pre-use gate did not refuse the absent competent authority pin",
        approval,
    )
    source = CHECKER.read_text(encoding="utf-8").splitlines()
    for pin in ("TRUSTED_LOCAL_DECISION_SHA256", "TRUSTED_PUBLICATION_DECISION_SHA256"):
        require(
            f"{pin}: str | None = None" in source,
            f"{pin} is no longer unset in the checker source",
            approval,
        )


BINDING_CASES: tuple[tuple[str, Callable[[], None]], ...] = (
    ("append-unrelated-row", case_append_unrelated_row),
    ("old-whole-file-mutation", case_old_whole_file_mutation),
    ("bound-row-changed", case_bound_row_changed),
    ("existing-row-changed", case_existing_row_changed),
    ("reorder-bound-rows", case_reorder_bound_rows),
    ("duplicate-bound-id", case_duplicate_bound_id),
    ("same-bytes-other-id", case_same_bytes_other_id),
    ("empty", case_empty),
    ("require-approved", case_require_approved),
)


def run_binding_cases() -> list[str]:
    """Run every per-row binding case; a failure is named, not allowed to hide later cases."""
    verify_mirror_baseline()
    failed: list[str] = []
    for name, case in BINDING_CASES:
        try:
            case()
        except AssertionError as error:
            failed.append(name)
            print(f"case FAIL {name}: {error}", flush=True)
        else:
            print(f"case PASS {name}", flush=True)
    return failed


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
    failed = run_binding_cases()
    if failed:
        sys.stdout.flush()
        print(
            "rdp-oracle-authority: per-row manifest binding cases failed: "
            + ", ".join(failed),
            file=sys.stderr,
        )
        return 1
    print("rdp-oracle-authority: blocked, forged local, and forged publication records refused")
    print(f"rdp-oracle-authority: {len(BINDING_CASES)} per-row manifest binding cases passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
