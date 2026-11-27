#!/usr/bin/env python3
"""Hermetic controls of the Phase 12 qualifier (scripts/qualify-phase12.py).

No build, no nested CTest and no stage of the real qualification runs here: the
snapshot, base, record and verify logic is exercised in scratch git repositories
under a temporary directory, with stub stage tables of cheap commands, so the
gate cost stays small (the F-113 lesson). Two cases run the real, read-only
oracle and scanner commands once, so a stage command or marker that does not
start cannot hide behind the stubs ("actual startup and deadline control").

Cases are listed with --list-cases, run with --selftest (every case, verbose) or
one at a time with --case NAME.
"""

from __future__ import annotations

import ast
import builtins
import contextlib
import hashlib
import importlib.util
import json
import os
import re
import subprocess
import sys
import tempfile
import time
import unittest
from collections.abc import Iterator
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
QUALIFIER = ROOT / "scripts/qualify-phase12.py"

_SPEC = importlib.util.spec_from_file_location("t64_qualify_phase12", QUALIFIER)
assert _SPEC is not None and _SPEC.loader is not None
qualify = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(qualify)

CASE_NAMES: tuple[str, ...] = (
    "bases-refuse-head-short-duplicate-missing",
    "snapshot-binds-delta-and-tools",
    "dirty-tree-refuses",
    "native-and-verify-accepted",
    "verify-refuses-changed-input",
    "verify-refuses-changed-input-after-qualification-base",
    "verify-refuses-rehearsal-as-final",
    "verify-refuses-outside-private-root",
    "verify-refuses-failed-stage",
    "verify-refuses-missing-stage",
    "native-refuses-failed-stage-and-writes-no-record",
    "verify-refuses-changed-bindings",
    "private-root-unsafe-refuses",
    "bounded-runner-deadline-and-descendant",
    "tier-and-claims-pinned",
    "stage-table-shape",
    "junit-count-rule",
    "gate-sweep-lane-output-rules",
    "tool-paths-and-real-pin-line",
    "real-stage-commands-start",
    "cli-modes-and-help",
    "no-undefined-global-names",
    "claims-pinned",
    "evidence-docs-unbound",
    "artifact-identity-and-freeze-binding",
)

STUB_NAMES = ["t.one", "t.two"]
EXPECTED_STAGES = ["ci-scanner", "inventory", "oracle", "abi-cheap", "installed", "gate",
                   "sanitize-shipped", "sanitize-instrumented", "tsan", "fresh-archive",
                   "sweep", "freeze"]
EVIDENCE_DOCS = ("docs/claims/phase12.md", "docs/evidence/phase12-qualification.md",
                 "docs/evidence/phase12-host-manual.md")
CLAIMS = {"hardware_accuracy": False, "oracle_comparison": False, "release": False,
          "real_time": False, "libfuturo_adapter": False, "final_abi": True}
GOLDEN_FILES = {"header_sha256": "docs/abi/header.sha256",
                "public_symbols_sha256": "docs/abi/public-symbols.txt",
                "layout_sha256": "docs/abi/layout.c",
                "status_classes_sha256": "docs/abi/status-classes.txt"}
PIN_RELATIVE = qualify.PIN_IMPACT_SUBPATH


def _git(root: Path, *args: str) -> str:
    done = subprocess.run(
        ["git", "-c", "user.name=t64-test", "-c", "user.email=noreply@example.invalid",
         "-c", "commit.gpgsign=false", *args],
        cwd=root, capture_output=True, text=True, check=True)
    return done.stdout.strip()


def _commit(root: Path, message: str) -> str:
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", message)
    return _git(root, "rev-parse", "HEAD")


def _write(root: Path, relative: str, text: str) -> None:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _freeze_files(root: Path, *, abi: str = "0.1") -> None:
    """A freeze record with a machine-read block and the four golden files it hashes."""
    contents = {
        "docs/abi/header.sha256": "aa  include/tendies64/t64_machine.h\n",
        "docs/abi/public-symbols.txt": "t64_one\nt64_two\n",
        "docs/abi/layout.c": "/* pins */\n",
        "docs/abi/status-classes.txt": "OK 0 ok\n",
    }
    block = [f"abi_version: {abi}", "package_version: 0.1.0", "amendments: 0"]
    for key, relative in GOLDEN_FILES.items():
        _write(root, relative, contents[relative])
        block.append(f"{key}: {hashlib.sha256(contents[relative].encode()).hexdigest()}")
    block += ["public_function_count: 2", "status_count: 1", "stop_reason_count: 0",
              "layout_structs: 1", "layout_members: 2", "layout_constants: 3", "padding_holes: 2",
              "padding_hole: t64_a struct_size 4 4", "padding_hole: t64_b rule 20 4"]
    _write(root, "docs/abi/FREEZE-CANDIDATE.md",
           "# Freeze candidate\n\n```freeze-record\n" + "\n".join(block) + "\n```\n")


def _pin_text(**labels: str) -> str:
    lines = ["## Wave bases", ""]
    for label, value in labels.items():
        lines.append(f"- {label.replace('_', ' ')}: {value}")
    return "\n".join(lines) + "\n"


@contextlib.contextmanager
def scratch_repo() -> Iterator[dict]:
    """A scratch repository with the qualifier's inputs; ROOT is patched to it.

    Layout: commit A (tools, inventory block, a.c, old.c, a freeze record with its
    four goldens, the two evidence documents), then commit B (a.c changed, b.c
    added, old.c deleted, a planning file, the CI ledger, an edited evidence
    document and the PIN-IMPACT file with the Wave 0, Host and Bind bases at
    A). HEAD is B.
    """
    with tempfile.TemporaryDirectory(prefix="t64-p12q-") as name:
        root = Path(name).resolve()
        _git(root, "init", "-q")
        _write(root, ".gitignore", "build/\n")
        _write(root, "tool_a.txt", "tool a\n")
        _write(root, "scripts/check-test-inventory.sh",
               "PHASE12_TESTS='" + "\n".join(STUB_NAMES) + "'\n")
        _write(root, "a.c", "int a = 1;\n")
        _write(root, "old.c", "int old = 1;\n")
        _freeze_files(root)
        for relative in EVIDENCE_DOCS:
            _write(root, relative, "evidence one\n")
        base = _commit(root, "A")
        _write(root, "a.c", "int a = 2;\n")
        _write(root, "b.c", "int b = 1;\n")
        (root / "old.c").unlink()
        _write(root, ".planning/note.md", "planning\n")
        _write(root, "docs/ci-reviews/LOG.md", "ledger\n")
        _write(root, EVIDENCE_DOCS[0], "evidence two\n")
        _write(root, PIN_RELATIVE, _pin_text(
            Wave_0_base=base, Host_base=base, Bind_base=base))
        head = _commit(root, "B")
        patches = [
            mock.patch.object(qualify, "ROOT", root),
            mock.patch.object(qualify, "TOOL_PATHS",
                              ("tool_a.txt", "scripts/check-test-inventory.sh")),
            mock.patch.object(qualify, "label_names", return_value=sorted(STUB_NAMES)),
        ]
        for item in patches:
            item.start()
        try:
            yield {"root": root, "base": base, "head": head, "pin": root / PIN_RELATIVE}
        finally:
            for item in reversed(patches):
                item.stop()


def _stub_table(bases: dict) -> list[dict]:
    return [
        qualify._stage("alpha", "stub derivation", [
            qualify._command(["python3", "-c", "print('alpha ok')"], 30, 0.05,
                             marker=r"^alpha ok$")]),
        qualify._stage("beta", "stub derivation", [
            qualify._command(["python3", "-c", "import sys; sys.exit(3)"], 30, 0.05,
                             expect=3)]),
    ]


def _add_qualification_base(scratch: dict) -> str:
    """Record the audited head as the qualification base in a planning-only commit."""
    audited = _git(scratch["root"], "rev-parse", "HEAD")
    with scratch["pin"].open("a", encoding="utf-8") as handle:
        handle.write(f"- Qualification base: {audited}\n")
    _commit(scratch["root"], "C planning only")
    return audited


@contextlib.contextmanager
def mutated_record(path: Path, change) -> Iterator[None]:
    """Write a changed copy of the record, then restore the original bytes."""
    original = path.read_bytes()
    record = json.loads(original)
    change(record)
    path.write_bytes(json.dumps(record, sort_keys=True, separators=(",", ":")).encode() + b"\n")
    try:
        yield
    finally:
        path.write_bytes(original)


def _undefined_global_names(path: Path) -> dict[str, int]:
    """Names loaded somewhere in a script that no statement ever binds."""
    tree = ast.parse(path.read_text(encoding="utf-8"))
    bound = set(dir(builtins)) | {"__file__", "__name__"}
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            bound.add(node.name)
            if not isinstance(node, ast.ClassDef):
                args = node.args
                for arg in (*args.posonlyargs, *args.args, *args.kwonlyargs):
                    bound.add(arg.arg)
                if args.vararg:
                    bound.add(args.vararg.arg)
                if args.kwarg:
                    bound.add(args.kwarg.arg)
        elif isinstance(node, ast.Lambda):
            args = node.args
            for arg in (*args.posonlyargs, *args.args, *args.kwonlyargs):
                bound.add(arg.arg)
        elif isinstance(node, ast.Import):
            bound.update((alias.asname or alias.name).split(".")[0] for alias in node.names)
        elif isinstance(node, ast.ImportFrom):
            bound.update(alias.asname or alias.name for alias in node.names)
        elif isinstance(node, ast.ExceptHandler) and node.name:
            bound.add(node.name)
        elif isinstance(node, ast.Name) and isinstance(node.ctx, (ast.Store, ast.Del)):
            bound.add(node.id)
    missing: dict[str, int] = {}
    for node in ast.walk(tree):
        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load) \
                and node.id not in bound:
            missing.setdefault(node.id, node.lineno)
    return missing


def _junit(names: list[str], *, failing: tuple[str, ...] = (), skipped: tuple[str, ...] = (),
           notrun: tuple[str, ...] = (), times: dict[str, str] | None = None) -> bytes:
    """A CTest-shaped JUnit document (testsuite, testcase with status, optional children)."""
    cases = []
    for name in names:
        time = (times or {}).get(name, "0.25")
        status = "run"
        children = ""
        if name in failing:
            status, children = "fail", '<failure message="Failed"/>'
        if name in skipped:
            children = "<skipped/>"
        if name in notrun:
            status = "notrun"
        cases.append(f'<testcase name="{name}" classname="{name}" time="{time}" '
                     f'status="{status}">{children}</testcase>')
    return (f'<?xml version="1.0" encoding="UTF-8"?><testsuite name="(empty)" '
            f'tests="{len(names)}">' + "".join(cases) + "</testsuite>").encode()


class Phase12Qualification(unittest.TestCase):
    def refuses(self, needle: str, function, *args, **kwargs) -> None:
        with self.assertRaises(qualify.Refusal) as caught:
            function(*args, **kwargs)
        self.assertIn(needle, str(caught.exception))

    # -- wave bases ------------------------------------------------------------

    def test_bases_refuse_head_short_duplicate_missing(self) -> None:
        with scratch_repo() as scratch:
            base = scratch["base"]
            pin = scratch["pin"]
            accepted = qualify.read_bases(pin, final=False)
            self.assertEqual(accepted["Wave 0 base"], base)
            self.assertEqual(set(accepted), {"Wave 0 base", "Host base", "Bind base"})
            original = pin.read_text(encoding="utf-8")
            outsider = _git(scratch["root"], "commit-tree", "-m", "side",
                            _git(scratch["root"], "rev-parse", "HEAD^{tree}"))
            variants = {
                "HEAD": ("- Wave 0 base: HEAD", "40 lowercase hex"),
                "a short hash": (f"- Wave 0 base: {base[:7]}", "40 lowercase hex"),
                "a symbolic ref": ("- Wave 0 base: main", "40 lowercase hex"),
                "upper case hex": (f"- Wave 0 base: {base.upper()}", "40 lowercase hex"),
                "trailing space": (f"- Wave 0 base: {base} ", "40 lowercase hex"),
                "41 hex": (f"- Wave 0 base: {base}0", "40 lowercase hex"),
                "an unknown commit": ("- Wave 0 base: " + "0" * 40, "not a commit"),
                "a commit outside the history": (f"- Wave 0 base: {outsider}", "not an ancestor"),
            }
            for label, (line, needle) in variants.items():
                pin.write_text(original.replace(f"- Wave 0 base: {base}", line), encoding="utf-8")
                self.refuses(needle, qualify.read_bases, pin, final=False)
            pin.write_text(original + f"- Wave 0 base: {base}\n", encoding="utf-8")
            self.refuses("exactly one is required", qualify.read_bases, pin, final=False)
            pin.write_text(original.replace(f"- Wave 0 base: {base}\n", ""), encoding="utf-8")
            self.refuses("has no '- Wave 0 base:' line", qualify.read_bases, pin, final=False)
            # The later labels are strict as well: a duplicate, a short hash or a ref.
            for label in ("Host base", "Bind base"):
                pin.write_text(original + f"- {label}: {base}\n", encoding="utf-8")
                self.refuses("exactly one is required", qualify.read_bases, pin, final=False)
                pin.write_text(original.replace(f"- {label}: {base}", f"- {label}: {base[:7]}"),
                               encoding="utf-8")
                self.refuses("40 lowercase hex", qualify.read_bases, pin, final=False)
                pin.write_text(original.replace(f"- {label}: {base}", f"- {label}: main"),
                               encoding="utf-8")
                self.refuses("40 lowercase hex", qualify.read_bases, pin, final=False)
            # A rehearsal needs the Wave 0 base only; a final record needs every label.
            pin.write_text(original.replace(f"- Host base: {base}\n", ""), encoding="utf-8")
            self.assertEqual(set(qualify.read_bases(pin, final=False)),
                             {"Wave 0 base", "Bind base"})
            with_qualification = original + f"- Qualification base: {base}\n"
            pin.write_text(with_qualification.replace(f"- Host base: {base}\n", ""),
                           encoding="utf-8")
            self.refuses("has no '- Host base:' line", qualify.read_bases, pin, final=True)
            pin.write_text(original.replace(f"- Bind base: {base}\n", ""), encoding="utf-8")
            self.refuses("has no '- Bind base:' line", qualify.read_bases, pin, final=True)
            # A final record needs the qualification base as well.
            pin.write_text(original, encoding="utf-8")
            self.refuses("has no '- Qualification base:' line", qualify.read_bases, pin,
                         final=True)
            pin.write_text(with_qualification, encoding="utf-8")
            self.assertEqual(set(qualify.read_bases(pin, final=True)),
                             {"Wave 0 base", "Host base", "Bind base", "Qualification base"})
            pin.write_text(original, encoding="utf-8")
            # The accepted baseline still reads after the refusals.
            self.assertEqual(qualify.read_bases(pin, final=False), accepted)

    # -- snapshot --------------------------------------------------------------

    def test_snapshot_binds_delta_and_tools(self) -> None:
        with scratch_repo() as scratch:
            snapshot = qualify.source_snapshot(scratch["base"])
            paths = [entry["path"] for entry in snapshot["inputs"]]
            self.assertEqual(paths, ["a.c", "b.c", "scripts/check-test-inventory.sh",
                                     "tool_a.txt"])
            self.assertEqual(snapshot["deleted"], ["old.c"])
            self.assertEqual(snapshot["input_count"], 4)
            entry = {item["path"]: item for item in snapshot["inputs"]}["a.c"]
            self.assertEqual(entry["sha256"], hashlib.sha256(b"int a = 2;\n").hexdigest())
            self.assertEqual(entry["mode"], "100644")
            self.assertEqual(snapshot["dirty_status_sha256"], qualify.EMPTY_SHA256)
            self.assertEqual(snapshot["excluded"]["prefixes"], [".planning/"])
            self.assertEqual(snapshot["excluded"]["files"],
                             ["docs/ci-reviews/LOG.md", *EVIDENCE_DOCS])
            # The edited evidence document is in the delta and is not a bound input.
            self.assertNotIn(EVIDENCE_DOCS[0], paths)
            self.assertEqual(snapshot, qualify.source_snapshot(scratch["base"]))
            # A tool that is absent is refused, never skipped.
            with mock.patch.object(qualify, "TOOL_PATHS", ("tool_a.txt", "missing-tool.txt")):
                self.refuses("bound source input is absent: missing-tool.txt",
                             qualify.source_snapshot, scratch["base"])
            self.refuses("40-hex", qualify.source_snapshot, "HEAD")

    def test_dirty_tree_refuses(self) -> None:
        with scratch_repo() as scratch:
            root = scratch["root"]
            baseline = qualify.source_snapshot(scratch["base"])
            needle = "uncommitted or untracked source inputs"
            # A tracked bound file edited and not committed.
            (root / "a.c").write_text("int a = 3;\n", encoding="utf-8")
            self.refuses(needle, qualify.source_snapshot, scratch["base"])
            _git(root, "checkout", "--", "a.c")
            # A new file, untracked, and the same file staged but not committed.
            (root / "new.c").write_text("int n;\n", encoding="utf-8")
            self.refuses(needle, qualify.source_snapshot, scratch["base"])
            _git(root, "add", "new.c")
            self.refuses(needle, qualify.source_snapshot, scratch["base"])
            _git(root, "rm", "-q", "-f", "new.c")
            # Planning and CI-ledger metadata are excluded by rule: they cannot
            # change a build, so an edit of them does not make the tree dirty.
            (root / ".planning/draft.md").write_text("draft\n", encoding="utf-8")
            (root / "docs/ci-reviews/LOG.md").write_text("ledger edit\n", encoding="utf-8")
            (root / ".planning/note.md").write_text("planning edit\n", encoding="utf-8")
            for relative in EVIDENCE_DOCS:
                (root / relative).write_text("evidence edit\n", encoding="utf-8")
            self.assertEqual(qualify.source_snapshot(scratch["base"]), baseline)
            # The qualifier itself refuses a dirty tree before it creates a record.
            (root / "a.c").write_text("int a = 4;\n", encoding="utf-8")
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                self.refuses(needle, qualify.native_qualify, rehearsal=True)
            self.assertFalse((root / qualify.PRIVATE_SUBPATH).exists())

    # -- records ---------------------------------------------------------------

    def _native(self, scratch: dict, *, rehearsal: bool) -> Path:
        with mock.patch.object(qualify, "_stage_table", _stub_table):
            return qualify.native_qualify(rehearsal=rehearsal)

    def test_native_and_verify_accepted(self) -> None:
        with scratch_repo() as scratch:
            audited = _add_qualification_base(scratch)
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                path = qualify.native_qualify(rehearsal=False)
                record = qualify.verify_record(path, rehearsal=False)
                self.assertTrue(path.is_file())
                self.assertEqual(path.stat().st_mode & 0o077, 0)
                self.assertEqual(path.parent.stat().st_mode & 0o077, 0)
                self.assertEqual(qualify._private_root().stat().st_mode & 0o077, 0)
                self.assertEqual(record["schema"], "t64.phase12-qualification")
                self.assertIs(record["rehearsal"], False)
                self.assertIsNone(record["statement"])
                self.assertEqual(record["tier"], "unit-tested, authored scope")
                self.assertEqual(record["claims"], CLAIMS)
                self.assertEqual(record["wave_bases"]["Wave 0 base"], scratch["base"])
                self.assertEqual(record["wave_bases"]["Host base"], scratch["base"])
                self.assertEqual(record["wave_bases"]["Bind base"], scratch["base"])
                self.assertEqual(sorted(record["freeze_record"]["goldens"]), sorted(GOLDEN_FILES))
                self.assertEqual(record["freeze_record"]["block"]["abi_version"], "0.1")
                self.assertEqual(record["artifact_identity"], {})
                self.assertEqual(record["wave_bases"]["Qualification base"], audited)
                self.assertEqual(record["inventory"]["names"], sorted(STUB_NAMES))
                self.assertEqual(record["inventory"]["count"], record["inventory"]["label_count"])
                self.assertEqual([s["name"] for s in record["stages"]], ["alpha", "beta"])
                self.assertEqual(record["qualification_drift_paths"], [])
                self.assertEqual(record["source_snapshot"]["base"], scratch["base"])
                # Planning, CI-ledger and evidence-document commits after the audited head
                # change no bound input.
                _write(scratch["root"], ".planning/later.md", "later\n")
                _write(scratch["root"], "docs/ci-reviews/LOG.md", "ledger later\n")
                for relative in EVIDENCE_DOCS:
                    _write(scratch["root"], relative, "evidence after the record\n")
                _commit(scratch["root"], "planning, ledger and evidence documents only")
                qualify.verify_record(path, rehearsal=False)

    def test_verify_refuses_changed_input(self) -> None:
        with scratch_repo() as scratch:
            path = self._native(scratch, rehearsal=True)
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                accepted = qualify.verify_record(path, rehearsal=True)
                self.assertTrue(accepted["rehearsal"])
                self.assertEqual(accepted["statement"], qualify.REHEARSAL_STATEMENT)
                # A bound input edited in a new commit: the snapshot differs.
                _write(scratch["root"], "a.c", "int a = 5;\n")
                _commit(scratch["root"], "bound input change")
                self.refuses("differ from the recorded snapshot", qualify.verify_record,
                             path, rehearsal=True)
                # A new bound file is also a difference, a dirty tree is refused first.
                _write(scratch["root"], "c.c", "int c;\n")
                self.refuses("uncommitted or untracked", qualify.verify_record, path,
                             rehearsal=True)

    def test_verify_refuses_changed_input_after_qualification_base(self) -> None:
        with scratch_repo() as scratch:
            _add_qualification_base(scratch)
            path = self._native(scratch, rehearsal=False)
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                qualify.verify_record(path, rehearsal=False)
                _write(scratch["root"], "b.c", "int b = 9;\n")
                _commit(scratch["root"], "bound change after the audited head")
                self.refuses("changed since the qualification base", qualify.verify_record,
                             path, rehearsal=False)

    def test_verify_refuses_rehearsal_as_final(self) -> None:
        with scratch_repo() as scratch:
            rehearsal_path = self._native(scratch, rehearsal=True)
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                qualify.verify_record(rehearsal_path, rehearsal=True)
                self.refuses("not final evidence", qualify.verify_record, rehearsal_path,
                             rehearsal=False)
                # Relabelling the record does not make it final: the label, the
                # statement and the missing qualification base each refuse.
                with mutated_record(rehearsal_path, lambda r: r.update(rehearsal=False)):
                    self.refuses("statement differs", qualify.verify_record, rehearsal_path,
                                 rehearsal=False)
                with mutated_record(rehearsal_path, lambda r: r.update(
                        rehearsal=False, statement=None)):
                    self.refuses("Qualification base", qualify.verify_record, rehearsal_path,
                                 rehearsal=False)
                _add_qualification_base(scratch)
                final_path = qualify.native_qualify(rehearsal=False)
                qualify.verify_record(final_path, rehearsal=False)
                self.refuses("presented as a rehearsal", qualify.verify_record, final_path,
                             rehearsal=True)

    def test_verify_refuses_outside_private_root(self) -> None:
        with scratch_repo() as scratch:
            path = self._native(scratch, rehearsal=True)
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                with tempfile.TemporaryDirectory(prefix="t64-p12q-out-") as name:
                    outside = Path(name)
                    moved = outside / "record.json"
                    moved.write_bytes(path.read_bytes())
                    self.refuses("outside the private qualification root",
                                 qualify.verify_record, moved, rehearsal=True)
                    renamed = path.parent / "other.json"
                    renamed.write_bytes(path.read_bytes())
                    self.refuses("outside the private qualification root",
                                 qualify.verify_record, renamed, rehearsal=True)
                    link = outside / "link.json"
                    link.symlink_to(path)
                    self.refuses("symlink", qualify.verify_record, link, rehearsal=True)
                qualify.verify_record(path, rehearsal=True)

    def test_verify_refuses_failed_stage(self) -> None:
        with scratch_repo() as scratch:
            path = self._native(scratch, rehearsal=True)
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                qualify.verify_record(path, rehearsal=True)
                first = lambda r: r["stages"][0]["commands"][0]  # noqa: E731
                cases = {
                    "failed or timed out": lambda r: first(r).update(return_code=1),
                    "failed or timed out (timeout flag)":
                        lambda r: first(r).update(timed_out=True),
                    "ran over its limit": lambda r: first(r).update(seconds=31),
                    "differs from the stage table (limit raised)":
                        lambda r: first(r).update(limit_seconds=9999),
                    "differs from the stage table (expectation lowered)":
                        lambda r: first(r).update(expect_exit=1, return_code=1),
                    "differs from the stage table (command changed)":
                        lambda r: first(r).update(argv=["python3", "-c", "pass"]),
                    "differs from the stage table (marker dropped)":
                        lambda r: first(r).update(marker=None),
                    "log was changed": lambda r: first(r).update(stdout_sha256="0" * 64),
                    "seconds differ": lambda r: r["stages"][0].update(seconds=0.0),
                }
                for needle, change in cases.items():
                    with mutated_record(path, change):
                        self.refuses(needle.split(" (")[0], qualify.verify_record, path,
                                     rehearsal=True)
                # The retained log itself changed: the hash no longer matches.
                log = path.parent / "steps/alpha.0.stdout.log"
                original = log.read_bytes()
                log.write_bytes(original + b"extra\n")
                try:
                    self.refuses("log was changed", qualify.verify_record, path, rehearsal=True)
                finally:
                    log.write_bytes(original)
                qualify.verify_record(path, rehearsal=True)

    def test_verify_refuses_missing_stage(self) -> None:
        with scratch_repo() as scratch:
            path = self._native(scratch, rehearsal=True)
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                qualify.verify_record(path, rehearsal=True)
                with mutated_record(path, lambda r: r["stages"].pop()):
                    self.refuses("lacks stage(s): beta", qualify.verify_record, path,
                                 rehearsal=True)
                with mutated_record(path, lambda r: r["stages"].pop(0)):
                    self.refuses("lacks stage(s): alpha", qualify.verify_record, path,
                                 rehearsal=True)
                with mutated_record(path, lambda r: r["stages"].reverse()):
                    self.refuses("differs from the stage table", qualify.verify_record, path,
                                 rehearsal=True)
                with mutated_record(path, lambda r: r["stages"].append(dict(r["stages"][0]))):
                    self.refuses("differs from the stage table", qualify.verify_record, path,
                                 rehearsal=True)
                with mutated_record(path, lambda r: r.pop("stages")):
                    self.refuses("lacks its stage list", qualify.verify_record, path,
                                 rehearsal=True)

    def test_native_refuses_failed_stage_and_writes_no_record(self) -> None:
        with scratch_repo() as scratch:
            def table(bases: dict) -> list[dict]:
                return [qualify._stage("gamma", "stub", [
                    qualify._command(["python3", "-c", "import sys; sys.exit(3)"], 30, 0.05)])]

            def missing_marker(bases: dict) -> list[dict]:
                return [qualify._stage("delta", "stub", [
                    qualify._command(["python3", "-c", "print('quiet')"], 30, 0.05,
                                     marker=r"^never printed$")])]

            def too_slow(bases: dict) -> list[dict]:
                return [qualify._stage("epsilon", "stub", [
                    qualify._command(["python3", "-c", "import time; time.sleep(30)"], 0.5, 0.05)])]

            for builder, needle in ((table, "exited 3, expected 0"),
                                    (missing_marker, "lacks its pass marker"),
                                    (too_slow, "timed out")):
                with mock.patch.object(qualify, "_stage_table", builder):
                    self.refuses(needle, qualify.native_qualify, rehearsal=True)
            # A command that exits cleanly but leaves its process group behind.
            lingering = {"return_code": 0, "timed_out": False, "seconds": 0.1,
                         "stdout": b"alpha ok\n", "stderr": b"", "process_group_empty": False}
            with mock.patch.object(qualify, "_stage_table", _stub_table), \
                    mock.patch.object(qualify, "run_bounded", return_value=lingering):
                self.refuses("left a process group", qualify.native_qualify, rehearsal=True)
            # The label selection must equal the PHASE12_TESTS block before any stage runs.
            with mock.patch.object(qualify, "_stage_table", _stub_table), \
                    mock.patch.object(qualify, "label_names", return_value=["t.one"]):
                self.refuses("selects different names", qualify.native_qualify, rehearsal=True)
            for record in (scratch["root"] / qualify.PRIVATE_SUBPATH).rglob("record.json"):
                self.fail(f"a failed run left a record: {record}")

    def test_verify_refuses_changed_bindings(self) -> None:
        with scratch_repo() as scratch:
            path = self._native(scratch, rehearsal=True)
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                qualify.verify_record(path, rehearsal=True)
                cases = {
                    "test inventory differs": lambda r: r["inventory"].update(
                        count=r["inventory"]["count"] + 1),
                    "test inventory differs (names)": lambda r: r["inventory"].update(
                        names=["t.one"]),
                    "artifact identity differs": lambda r: r.update(artifact_identity={
                        "core": {"path": "x", "build_id": "t64-build-v1-" + "0" * 64}}),
                    "wave bases differ": lambda r: r.update(wave_bases={
                        **r["wave_bases"], "Wave 0 base": "0" * 40}),
                    "recorded commit": lambda r: r.update(commit_sha="0" * 40),
                    "recorded commit (short)": lambda r: r.update(commit_sha="abc"),
                    "not a complete Phase 12": lambda r: r.update(schema="t64.phase11-qualification"),
                    "not a complete Phase 12 (version)": lambda r: r.update(schema_version=2),
                    "not a complete Phase 12 (status)": lambda r: r.update(status="failed"),
                    "not a complete Phase 12 (id)": lambda r: r.update(record_id="other"),
                    "freeze record or one of its four goldens": lambda r: r["freeze_record"][
                        "goldens"]["layout_sha256"].update(sha256="0" * 64),
                    "freeze record or one of its four goldens (block)": lambda r: r[
                        "freeze_record"]["block"].update(abi_version="0.2"),
                    "rehearsal label": lambda r: r.pop("rehearsal"),
                    "recorded qualification drift": lambda r: r.update(
                        qualification_drift_paths=["a.c"]),
                }
                for needle, change in cases.items():
                    with mutated_record(path, change):
                        self.refuses(needle.split(" (")[0], qualify.verify_record, path,
                                     rehearsal=True)
                qualify.verify_record(path, rehearsal=True)

    def test_private_root_unsafe_refuses(self) -> None:
        with scratch_repo() as scratch:
            root = scratch["root"] / qualify.PRIVATE_SUBPATH
            root.parent.mkdir(parents=True)
            root.mkdir(mode=0o755)
            root.chmod(0o755)
            self.refuses("accessible to other users", qualify._private_root_ready)
            root.chmod(0o700)
            self.assertEqual(qualify._private_root_ready(), root)
            root.rmdir()
            target = scratch["root"] / "elsewhere"
            target.mkdir()
            root.symlink_to(target)
            self.refuses("private root is unsafe", qualify._private_root_ready)
            root.unlink()
            created = qualify._private_root_ready()
            self.assertEqual(created.stat().st_mode & 0o777, 0o700)

    # -- bounded runner -----------------------------------------------------------

    def test_bounded_runner_deadline_and_descendant(self) -> None:
        with tempfile.TemporaryDirectory(prefix="t64-p12q-run-") as name:
            marker = Path(name) / "descendant.pid"
            script = (f"sleep 60 & echo $! > '{marker}'; echo started; wait")
            started = time.monotonic()
            outcome = qualify.run_bounded(["sh", "-c", script], timeout_seconds=0.5)
            self.assertLess(time.monotonic() - started, 10)
            self.assertTrue(outcome["timed_out"])
            self.assertTrue(outcome["process_group_empty"])
            self.assertIn(b"started", outcome["stdout"])
            pid = int(marker.read_text(encoding="utf-8").strip())
            with self.assertRaises(ProcessLookupError):
                os.kill(pid, 0)
            ok = qualify.run_bounded(["sh", "-c", "echo out; echo err >&2; exit 3"],
                                     timeout_seconds=10, env_delta={"T64_P12Q": "1"})
            self.assertEqual((ok["return_code"], ok["timed_out"]), (3, False))
            self.assertEqual((ok["stdout"], ok["stderr"]), (b"out\n", b"err\n"))
            missing = qualify.run_bounded(["/nonexistent/t64-p12q"], timeout_seconds=5)
            self.assertEqual(missing["return_code"], 127)

    # -- claims -------------------------------------------------------------------

    def test_tier_and_claims_pinned(self) -> None:
        self.assertEqual(qualify.TIER, "unit-tested, authored scope")
        self.assertEqual(qualify.CLAIMS, CLAIMS)
        with scratch_repo() as scratch:
            path = self._native(scratch, rehearsal=True)
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                for change in (lambda r: r.update(tier="hardware-accurate"),
                               lambda r: r["claims"].update(hardware_accuracy=True),
                               lambda r: r["claims"].update(oracle_comparison=True),
                               lambda r: r["claims"].update(real_time=True),
                               lambda r: r["claims"].update(libfuturo_adapter=True),
                               lambda r: r.pop("claims")):
                    with mutated_record(path, change):
                        self.refuses("tier or claims", qualify.verify_record, path,
                                     rehearsal=True)
                qualify.verify_record(path, rehearsal=True)

    def test_claims_pinned(self) -> None:
        """Only final_abi is true, as the ABI 1.0 binding; every other claim is refused when true."""
        self.assertIs(qualify.CLAIMS["final_abi"], True)
        for key in ("hardware_accuracy", "oracle_comparison", "release", "real_time",
                    "libfuturo_adapter"):
            self.assertIs(qualify.CLAIMS[key], False, key)
        self.assertEqual(sorted(qualify.CLAIMS),
                         ["final_abi", "hardware_accuracy", "libfuturo_adapter",
                          "oracle_comparison", "real_time", "release"])
        with scratch_repo() as scratch:
            path = self._native(scratch, rehearsal=True)
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                self.assertEqual(qualify.verify_record(path, rehearsal=True)["claims"],
                                 qualify.CLAIMS)
                for change in (lambda r: r["claims"].update(release=True),
                               lambda r: r["claims"].update(real_time=True),
                               lambda r: r["claims"].update(libfuturo_adapter=True),
                               lambda r: r["claims"].update(final_abi=False),
                               lambda r: r["claims"].update(final_abi=1),
                               lambda r: r["claims"].update(release=True, real_time=True),
                               lambda r: r["claims"].pop("final_abi"),
                               lambda r: r["claims"].pop("release"),
                               lambda r: r["claims"].pop("libfuturo_adapter"),
                               lambda r: r["claims"].update(stable_abi=False),
                               lambda r: r["claims"].update(release=0),
                               lambda r: r["claims"].update(real_time=0)):
                    with mutated_record(path, change):
                        self.refuses("tier or claims", qualify.verify_record, path,
                                     rehearsal=True)
                qualify.verify_record(path, rehearsal=True)

    def test_evidence_docs_unbound(self) -> None:
        """The three evidence documents are unbound: editing them never refuses the record (W-9)."""
        self.assertEqual(qualify.EVIDENCE_DOCS, EVIDENCE_DOCS)
        self.assertEqual(qualify.EXCLUDED_FILES, ("docs/ci-reviews/LOG.md", *EVIDENCE_DOCS))
        self.assertEqual(qualify.EXCLUDED_PREFIXES, (".planning/",))
        with scratch_repo() as scratch:
            root = scratch["root"]
            _add_qualification_base(scratch)
            path = self._native(scratch, rehearsal=False)
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                qualify.verify_record(path, rehearsal=False)
                # All three documents rewritten, one of them created later and one deleted: unbound.
                for relative in EVIDENCE_DOCS:
                    _write(root, relative, "cites the record: " + path.parent.name + "\n")
                _commit(root, "evidence documents written after the record")
                qualify.verify_record(path, rehearsal=False)
                (root / EVIDENCE_DOCS[1]).unlink()
                _commit(root, "evidence document removed")
                qualify.verify_record(path, rehearsal=False)
                # Any other document is a bound input: the closed Phase 10 and Phase 11
                # claims and qualification files and a file next to the ledger or the claims
                # file.
                for relative in ("docs/claims/phase10.md", "docs/claims/phase11.md",
                                 "docs/evidence/phase11-qualification.md",
                                 "docs/ci-reviews/other.md", "docs/claims/phase12.md.bak"):
                    _write(root, relative, "a bound input\n")
                    _commit(root, "a bound document " + relative)
                    self.refuses("changed since the qualification base", qualify.verify_record,
                                 path, rehearsal=False)
                    _git(root, "rm", "-q", relative)
                    _commit(root, "removed again " + relative)
                    qualify.verify_record(path, rehearsal=False)
                # A bound source input still refuses, whatever happened to the documents.
                _write(root, "a.c", "int a = 77;\n")
                _commit(root, "bound input change")
                self.refuses("changed since the qualification base", qualify.verify_record,
                             path, rehearsal=False)

    # -- stage table and output rules ----------------------------------------------

    def test_stage_table_shape(self) -> None:
        bases = {qualify.WAVE0_LABEL: "ab" * 20}
        table = qualify._stage_table(bases)
        self.assertEqual([stage["name"] for stage in table], EXPECTED_STAGES)
        self.assertEqual(len({stage["name"] for stage in table}), len(table))
        checks = {None, "gate", "ci-selftest", "inventory", "sweep", "tsan", "fresh-archive",
                  "freeze", "sanitize:shipped", "sanitize:instrumented",
                  "junit:phase12-junit.xml"}
        for stage in table:
            name = stage["name"]
            self.assertTrue(stage["commands"], name)
            self.assertGreater(len(stage["derivation"]), 40, name)
            self.assertEqual(stage["limit_seconds"],
                             sum(spec["limit_seconds"] for spec in stage["commands"]), name)
            for spec in stage["commands"]:
                label = (name, spec["argv"])
                self.assertTrue(spec["argv"] and all(
                    isinstance(item, str) and item for item in spec["argv"]), label)
                self.assertGreater(spec["measured_seconds"], 0, label)
                # The limit is a kill deadline derived from measured seconds: at
                # least twice the largest measurement, and a bounded value.
                self.assertGreaterEqual(spec["limit_seconds"], 2 * spec["measured_seconds"], label)
                self.assertLessEqual(spec["limit_seconds"], 3600, label)
                if spec["marker"] is not None:
                    re.compile(spec["marker"])
                self.assertIn(spec["check"], checks, label)
                # Records stay portable: placeholders and repository-relative paths,
                # never a workstation path.
                for text in (*spec["argv"], *spec["env"].values()):
                    self.assertFalse(text.startswith("/"), label)
                    self.assertNotIn("/Users/", text, label)
                # Only the abi-cheap build command has neither a marker nor a check.
                self.assertTrue(spec["marker"] is not None or spec["check"] is not None
                                or spec["argv"][:2] == ["cmake", "--build"], label)
        by_name = {stage["name"]: stage for stage in table}
        # The sweep reads the 40-hex Wave 0 literal, never HEAD, as its base.
        sweep = by_name["sweep"]["commands"][0]["argv"]
        self.assertEqual(sweep[sweep.index("--base") + 1], "ab" * 20)
        self.assertEqual(sweep[sweep.index("--head") + 1], "HEAD")
        self.assertEqual(sweep.count("HEAD"), 1)
        # Artifact identity comes from the stages that built or installed the artifacts.
        self.assertEqual(by_name["gate"]["artifacts"], ["core", "runner_real", "runner_test"])
        self.assertEqual(by_name["sanitize-instrumented"]["artifacts"], ["asan_core"])
        self.assertEqual(by_name["fresh-archive"]["artifacts"],
                         ["installed_core", "installed_consumer"])
        self.assertEqual(qualify._artifact_names(table),
                         ["asan_core", "core", "installed_consumer", "installed_core",
                          "runner_real", "runner_test"])
        # The two oracle commands: blocked passes, approval refuses with exit 2.
        oracle = by_name["oracle"]["commands"]
        self.assertEqual([spec["expect_exit"] for spec in oracle], [0, 2])
        self.assertIn("--expect-blocked", oracle[0]["argv"])
        self.assertIn("--require-approved", oracle[1]["argv"])
        # The JUnit command writes under the record directory only and selects phase12.
        junit = [spec for spec in by_name["installed"]["commands"]
                 if "--output-junit" in spec["argv"]]
        self.assertEqual(len(junit), 1)
        self.assertTrue(junit[0]["argv"][junit[0]["argv"].index("--output-junit") + 1]
                        .startswith("{record}/"))
        self.assertEqual(junit[0]["argv"][junit[0]["argv"].index("-L") + 1], "phase12")
        self.assertIn("--no-tests=error", junit[0]["argv"])
        # The cheap stage runs the abi, install and api04 name families by one regex.
        cheap = by_name["abi-cheap"]["commands"][-1]["argv"]
        self.assertEqual(cheap[cheap.index("-R") + 1], r"^(repo\.abi|install\.|api04\.)")
        # The lane stages run the lanes of check-install.sh, never edit or skip a case.
        self.assertEqual(by_name["sanitize-shipped"]["commands"][0]["argv"],
                         ["sh", "scripts/check-install.sh", "sanitize", "shipped"])
        self.assertEqual(by_name["sanitize-instrumented"]["commands"][0]["argv"],
                         ["sh", "scripts/check-install.sh", "sanitize", "instrumented"])
        self.assertEqual(by_name["tsan"]["commands"][0]["argv"],
                         ["sh", "scripts/check-install.sh", "tsan"])
        self.assertEqual(by_name["fresh-archive"]["commands"][0]["argv"],
                         ["sh", "scripts/check-install.sh", "fresh-archive"])
        # The freeze stage checks the prefix the fresh-archive stage installed, after it.
        freeze = by_name["freeze"]["commands"][0]["argv"]
        self.assertEqual(freeze[freeze.index("--prefix") + 1], qualify.FRESH_PREFIX)
        self.assertEqual(freeze[freeze.index("--record") + 1], "docs/abi/FREEZE-CANDIDATE.md")
        self.assertLess(EXPECTED_STAGES.index("fresh-archive"), EXPECTED_STAGES.index("freeze"))
        self.assertEqual(dict((n, p) for n, p, _ in qualify.ARTIFACTS)["installed_core"],
                         qualify.FRESH_PREFIX + "/lib/libt64_core.a")
        # A wrong table value is refused by the shape rules above: lower a limit.
        broken = qualify._command(["true"], 3, 2.0)
        self.assertLess(broken["limit_seconds"], 2 * broken["measured_seconds"])

    def test_junit_count_rule(self) -> None:
        names = ["a.one", "a.two", "a.three"]
        accepted = qualify.junit_problems(
            _junit(names, times={"a.one": "1.5", "a.two": "0.5", "a.three": "2.25"}), names)
        self.assertEqual(accepted["testcases"], 3)
        self.assertEqual(accepted["seconds"], {"a.one": 1.5, "a.three": 2.25, "a.two": 0.5})
        self.assertEqual(accepted["seconds_sum"], 4.25)
        refuse = self.refuses
        refuse("lacks the Phase 12 test(s): a.three", qualify.junit_problems,
               _junit(names[:2]), names)
        refuse("outside the Phase 12 inventory: a.four", qualify.junit_problems,
               _junit([*names, "a.four"]), names)
        refuse("lists a test case twice", qualify.junit_problems,
               _junit([*names, "a.two"]), names)
        refuse("failed, skipped or not run", qualify.junit_problems,
               _junit(names, failing=("a.two",)), names)
        refuse("failed, skipped or not run", qualify.junit_problems,
               _junit(names, skipped=("a.one",)), names)
        refuse("failed, skipped or not run", qualify.junit_problems,
               _junit(names, notrun=("a.three",)), names)
        refuse("time of a.one is not a number", qualify.junit_problems,
               _junit(names, times={"a.one": "fast"}), names)
        refuse("JUnit is malformed", qualify.junit_problems, b"<testsuite><testcase", names)
        refuse("lacks the Phase 12 test(s)", qualify.junit_problems, _junit([]), names)
        # The count rule is relative to the block: a longer block with the same
        # JUnit is a missing test, a shorter block is an extra one.
        refuse("lacks the Phase 12 test(s): a.four", qualify.junit_problems, _junit(names),
               [*names, "a.four"])
        refuse("outside the Phase 12 inventory: a.three", qualify.junit_problems, _junit(names),
               names[:2])
        # command_details reads the retained file and records its hash.
        spec = qualify._command(["true"], 10, 1.0, check="junit:j.xml")
        with tempfile.TemporaryDirectory(prefix="t64-p12q-junit-") as name:
            directory = Path(name)
            (directory / "j.xml").write_bytes(_junit(STUB_NAMES))
            with scratch_repo():
                details = qualify.command_details(spec, b"", b"", directory)
            self.assertEqual(details["testcases"], 2)
            self.assertEqual(details["junit_sha256"],
                             hashlib.sha256((directory / "j.xml").read_bytes()).hexdigest())

    def test_gate_sweep_lane_output_rules(self) -> None:
        # The gate summary: PASS line, the four steps and the CTest wall.
        gate = ("Total Test time (real) =  84.94 sec\n[gate] ctest ok (85s)\n[gate] summary:\n"
                "[gate]   build 0s\n[gate]   test-inventory 1s\n[gate]   ctest 85s\n"
                "[gate]   core-lint 9s\n[gate] total 143s\n[gate] PASS\n").encode()
        details = qualify.gate_details(gate)
        self.assertEqual(details["gate_total_seconds"], 143)
        self.assertEqual(details["ctest_wall_seconds"], 84.94)
        self.assertEqual(details["steps"]["ctest"], 85)
        self.refuses("did not print its PASS line", qualify.gate_details,
                     gate.replace(b"[gate] PASS", b"[gate] ok"))
        self.refuses("lacks the ctest step", qualify.gate_details,
                     gate.replace(b"[gate]   ctest 85s\n", b""))
        self.refuses("lacks its total or CTest wall", qualify.gate_details,
                     gate.replace(b"Total Test time (real) =  84.94 sec\n", b""))
        # The sweep: pinned coverage, zero differences, zero head-only lines.
        covered = f"sweep: {qualify.SWEEP_SHARED_LINES} shared lines, 0 differences, 0 head-only lines\n"
        self.assertEqual(qualify._sweep_details(covered.encode())["differences"], 0)
        self.refuses("1 differences", qualify._sweep_details,
                     covered.replace("0 differences", "1 differences").encode())
        self.refuses("head-only lines", qualify._sweep_details,
                     covered.replace("0 head-only", "4 head-only").encode())
        self.refuses("pinned coverage", qualify._sweep_details,
                     covered.replace(str(qualify.SWEEP_SHARED_LINES), "1598").encode())
        self.refuses("no summary line", qualify._sweep_details, b"error: nothing\n")
        # Scanner selftest and inventory summaries.
        self.assertEqual(qualify._ci_selftest_details(
            b"check-ci-yml selftest: cases=138 failures=0\n"), {"cases": 138})
        self.refuses("fewer than 138", qualify._ci_selftest_details,
                     b"check-ci-yml selftest: cases=137 failures=0\n")
        self.refuses("no clean summary line", qualify._ci_selftest_details,
                     b"check-ci-yml selftest: cases=138 failures=1\n")
        self.assertEqual(qualify._inventory_details(
            b"test inventory PASS preset=gate count=254\n"), {"count": 254})
        self.refuses("did not pass", qualify._inventory_details, b"test inventory FAIL\n")
        self._sanitizer_lane_rules()
        self._tsan_fresh_freeze_rules()

    def _sanitizer_lane_rules(self) -> None:
        cases = qualify._cases_listed()
        self.assertIn("sequence", cases)
        self.assertEqual(len(cases), len(set(cases)))

        def lane(mode: str, *, archive: int, consumer: int = 34, drop: str | None = None,
                 failed: str | None = None, steps: int = qualify.SANITIZE_STEPS) -> bytes:
            lines = [f"check-install: sanitize {mode}: AddressSanitizer runtime references: "
                     f"archive {archive}, consumer {consumer}"]
            for case in cases:
                if case == drop:
                    continue
                if case == failed:
                    lines.append(f"check-install: sanitize {mode}: case {case} FAILED (exit 1), log x")
                else:
                    lines.append(f"check-install: sanitize {mode}: case {case} ok, 1 s (checks=1 failures=0)")
            lines.append(f"check-install: sanitize {mode} ok: every case of cases.txt, sequence with "
                         f"--steps {steps} --digest-stride 1, 54 s in all")
            return ("\n".join(lines) + "\n").encode()

        shipped = qualify._sanitize_details(lane("shipped", archive=0), "shipped")
        self.assertEqual(shipped["cases"], len(cases))
        self.assertEqual((shipped["archive_asan_references"], shipped["sequence_steps"]),
                         (0, qualify.SANITIZE_STEPS))
        instrumented = qualify._sanitize_details(lane("instrumented", archive=733), "instrumented")
        self.assertEqual(instrumented["archive_asan_references"], 733)
        self.refuses("not as shipped", qualify._sanitize_details, lane("shipped", archive=3),
                     "shipped")
        self.refuses("archive is not instrumented", qualify._sanitize_details,
                     lane("instrumented", archive=0), "instrumented")
        self.refuses("consumer is not instrumented", qualify._sanitize_details,
                     lane("shipped", archive=0, consumer=0), "shipped")
        self.refuses("exactly the cases of cases.txt", qualify._sanitize_details,
                     lane("shipped", archive=0, drop="sequence"), "shipped")
        self.refuses("failed case", qualify._sanitize_details,
                     lane("shipped", archive=0, failed="smoke"), "shipped")
        self.refuses("sequence steps", qualify._sanitize_details,
                     lane("shipped", archive=0, steps=2000), "shipped")
        self.refuses("no clean summary line", qualify._sanitize_details,
                     lane("shipped", archive=0).replace(b" ok: every case", b" bad: every case"),
                     "shipped")
        self.refuses("exactly the cases of cases.txt", qualify._sanitize_details,
                     lane("instrumented", archive=5), "shipped")
        self.refuses("unknown sanitizer lane mode", qualify._sanitize_details, b"", "other")

    def _tsan_fresh_freeze_rules(self) -> None:
        digest = qualify.COMPOSITE_DIGEST
        tsan = (f"check-install: tsan: ThreadSanitizer runtime references: archive 564, consumer 21\n"
                f"check-install: tsan ok: two threads, one instance each, no ThreadSanitizer report; "
                f"both digests {digest}; library 2 s, consumer 1 s, run 2 s\n").encode()
        self.assertEqual(qualify._tsan_details(tsan)["archive_tsan_references"], 564)
        self.refuses("both be instrumented", qualify._tsan_details,
                     tsan.replace(b"archive 564", b"archive 0"))
        self.refuses("both be instrumented", qualify._tsan_details,
                     tsan.replace(b"consumer 21", b"consumer 0"))
        self.refuses("pinned composite digest", qualify._tsan_details,
                     tsan.replace(digest.encode(), b"0" * 64))
        self.refuses("no runtime reference line", qualify._tsan_details, b"nothing\n")
        build_id = "t64-build-v1-" + "ab" * 32
        commit = "cd" * 20
        fresh = (f"check-install: fresh-archive: case smoke ok (smoke: checks=13 failures=0)\n"
                 f"check-install: fresh-archive: case composite ok (composite: checks=25 failures=0)\n"
                 f"check-install: fresh-archive: archive of {commit} (1432 files), library build 3 s, "
                 f"consumer 0 s\n"
                 f"check-install: fresh-archive: installed archive build id {build_id}\n"
                 f"check-install: fresh-archive ok: exported tree built outside the repository\n").encode()
        accepted = qualify._fresh_archive_details(fresh)
        self.assertEqual((accepted["archive_commit"], accepted["build_id"]), (commit, build_id))
        self.assertEqual(accepted["cases"], ["composite", "smoke"])
        self.refuses("did not name the archived commit", qualify._fresh_archive_details,
                     fresh.replace(commit.encode(), b"HEAD"))
        self.refuses("printed no installed build id", qualify._fresh_archive_details,
                     fresh.replace(build_id.encode(), b"t64-build-v1-short"))
        self.refuses("exactly the smoke and composite cases", qualify._fresh_archive_details,
                     fresh.replace(b"case composite ok", b"case composite bad"))
        # The freeze summary equals the machine-read block of the real record.
        block = qualify.freeze_binding()["block"]
        line = (f"abi-freeze: headers=1 abi={block['abi_version']} package={block['package_version']} "
                f"symbols={block['public_function_count']} layout={block['layout_structs']}/"
                f"{block['layout_members']}/{block['layout_constants']} holes={block['padding_holes']} "
                f"statuses={block['status_count']} stop_reasons={block['stop_reason_count']}\n")
        details = qualify._freeze_details(line.encode())
        self.assertEqual(details["headers"], 1)
        self.assertEqual(details["printed"]["public_function_count"],
                         block["public_function_count"])
        symbols = f"symbols={block['public_function_count']}"
        self.refuses("measured public_function_count", qualify._freeze_details,
                     line.replace(symbols, "symbols=34").encode())
        self.refuses("measured abi_version", qualify._freeze_details,
                     line.replace(f"abi={block['abi_version']} ", "abi=0.2 ").encode())
        self.refuses("holds 2 headers", qualify._freeze_details,
                     line.replace("headers=1", "headers=2").encode())
        self.refuses("printed no summary line", qualify._freeze_details,
                     b"check-abi: freeze findings=0\n")

    def test_tool_paths_and_real_pin_line(self) -> None:
        listed = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, capture_output=True,
                                check=True).stdout.split(b"\0")
        tracked = {item.decode() for item in listed if item}
        for relative in qualify.TOOL_PATHS:
            self.assertIn(relative, tracked, relative)
            self.assertTrue((ROOT / relative).is_file(), relative)
        self.assertEqual(len(qualify.TOOL_PATHS), len(set(qualify.TOOL_PATHS)))
        for _, relative, _ in qualify.ARTIFACTS:
            self.assertTrue(relative.startswith("build/"), relative)
        # The freeze record and its four goldens are bound tools, and so are the
        # qualifier and this test (a stage result is only as good as its tool).
        for relative in ("docs/abi/FREEZE-CANDIDATE.md", *GOLDEN_FILES.values(),
                         "scripts/qualify-phase12.py", "scripts/check-install.sh",
                         "scripts/check-abi.py", "scripts/gate.sh"):
            self.assertIn(relative, qualify.TOOL_PATHS, relative)
        self.assertEqual(dict(qualify.FREEZE_GOLDENS), GOLDEN_FILES)
        # The real PIN-IMPACT.md carries exactly one strict line for each base that is
        # recorded (a textual check: no git history is needed, so a shallow clone is
        # fine). The Qualification base is appended by plan 12-18, so it may be absent.
        text = (ROOT / PIN_RELATIVE).read_text(encoding="utf-8")
        self.assertEqual(qualify.BASE_LABELS, ("Wave 0 base", "Host base", "Bind base",
                                               "Qualification base"))
        self.assertEqual(qualify.QUALIFICATION_LABEL, "Qualification base")
        for label in qualify.BASE_LABELS:
            lines = [line for line in text.splitlines() if line.startswith(f"- {label}:")]
            self.assertLessEqual(len(lines), 1, label)
            if label != qualify.QUALIFICATION_LABEL:
                self.assertEqual(len(lines), 1, label)
            for line in lines:
                self.assertRegex(line, rf"^- {label}: [0-9a-f]{{40}}$")
        # The real freeze record's block agrees with the four golden files on disk.
        binding = qualify.freeze_binding()
        self.assertEqual(sorted(binding["goldens"]), sorted(GOLDEN_FILES))
        self.assertEqual(binding["block"]["amendments"], "0")
        # The excluded rule is exactly planning metadata, the CI ledger and the three
        # evidence documents of Phase 12; the closed Phase 10 and Phase 11 documents stay bound.
        self.assertTrue(qualify._excluded(".planning/STATE.md"))
        self.assertTrue(qualify._excluded("docs/ci-reviews/LOG.md"))
        for relative in EVIDENCE_DOCS:
            self.assertTrue(qualify._excluded(relative), relative)
        for relative in ("docs/ci-reviews/other.md", "docs/evidence/phase10-qualification.md",
                         "docs/claims/phase10.md", "docs/claims/phase11.md",
                         "docs/evidence/phase11-qualification.md", "docs/abi/FREEZE-CANDIDATE.md",
                         "scripts/qualify-phase12.py", "core/t64_machine.c"):
            self.assertFalse(qualify._excluded(relative), relative)

    # -- real, read-only stage commands ------------------------------------------

    def test_real_stage_commands_start(self) -> None:
        """The real oracle and CI scanner commands start and satisfy their markers."""
        bases = {qualify.WAVE0_LABEL: "0" * 40}
        table = {stage["name"]: stage for stage in qualify._stage_table(bases)}
        self.assertIn("oracle", table)
        started = [(name, spec) for name in ("oracle", "ci-scanner")
                   for spec in table[name]["commands"] if "--selftest" not in spec["argv"]]
        self.assertEqual(len(started), 3)
        for name, spec in started:
            outcome = qualify.run_bounded(spec["argv"], timeout_seconds=spec["limit_seconds"])
            self.assertEqual(outcome["return_code"], spec["expect_exit"], spec["argv"])
            self.assertFalse(outcome["timed_out"])
            qualify.command_details(spec, outcome["stdout"], outcome["stderr"], ROOT)
        # The marker is a gate, not decoration: the same output without it refuses.
        spec = table["oracle"]["commands"][0]
        self.refuses("lacks its pass marker", qualify.command_details, spec, b"", b"", ROOT)

    def test_cli_modes_and_help(self) -> None:
        def run(*args: str) -> subprocess.CompletedProcess:
            return subprocess.run([sys.executable, "-B", str(QUALIFIER), *args], cwd=ROOT,
                                  capture_output=True, text=True, check=False, timeout=60)
        helped = run("--help")
        self.assertEqual(helped.returncode, 0)
        for flag in ("--native", "--rehearsal", "--verify"):
            self.assertIn(flag, helped.stdout)
        self.assertEqual(run().returncode, 2)
        self.assertEqual(run("--rehearsal").returncode, 2)
        self.assertEqual(run("--native", "--verify", "x").returncode, 2)
        refused = run("--verify", "build/gate/12-qualification-private/none/record.json")
        self.assertEqual(refused.returncode, 1)
        self.assertIn("refused original evidence", refused.stderr)
        self.assertNotIn("Traceback", refused.stderr)

    # -- artifact identity and the freeze binding -----------------------------------

    def test_artifact_identity_and_freeze_binding(self) -> None:
        core_id = "t64-build-v1-" + "11" * 32
        other_id = "t64-build-v1-" + "22" * 32
        with scratch_repo() as scratch:
            root = scratch["root"]
            for relative, text in (
                    ("art/core.a", f"\0{core_id}\0"), ("art/installed.a", f"x{core_id}y"),
                    ("art/consumer", f"{core_id} and again {core_id}"),
                    ("art/other.a", other_id), ("art/two.a", f"{core_id}{other_id}"),
                    ("art/none.a", "no build id here")):
                _write(root, relative, text)
            regex = qualify.CORE_BUILD_RE
            artifacts = (("core", "art/core.a", regex), ("installed_core", "art/installed.a", regex),
                         ("installed_consumer", "art/consumer", regex))
            with mock.patch.object(qualify, "ARTIFACTS", artifacts):
                found = qualify.artifact_identity(["core", "installed_core", "installed_consumer"])
                self.assertEqual({name: item["build_id"] for name, item in found.items()},
                                 dict(core=core_id, installed_core=core_id,
                                      installed_consumer=core_id))
                self.assertEqual(qualify.artifact_identity(["installed_core"]),
                                 {"installed_core": {"path": "art/installed.a",
                                                     "build_id": core_id}})
                self.refuses("unknown artifact", qualify.artifact_identity, ["nothing"])
                self.assertEqual(qualify.artifact_identity([]), {})
            # The fresh archive must carry the gate's identity, and the consumer the archive's.
            for first, second, bad in (("core", "installed_core", "art/other.a"),
                                       ("installed_core", "installed_consumer", "art/other.a")):
                changed = tuple((name, bad if name == second else relative, rx)
                                for name, relative, rx in artifacts)
                with mock.patch.object(qualify, "ARTIFACTS", changed):
                    self.refuses(f"{first} and {second} carry different build ids",
                                 qualify.artifact_identity, [first, second])
                    # An artifact named alone is not compared with an absent partner.
                    qualify.artifact_identity([second])
            for bad, needle in (("art/two.a", "exactly one build id"),
                                ("art/none.a", "exactly one build id"),
                                ("art/absent.a", "required file is unavailable")):
                changed = (("core", bad, regex),)
                with mock.patch.object(qualify, "ARTIFACTS", changed):
                    self.refuses(needle, qualify.artifact_identity, ["core"])
            # The lane's own build id and archived commit are tied to the record.
            commit = scratch["head"]
            stage = {"name": "fresh-archive", "details": {"0": {
                "archive_commit": commit, "build_id": core_id}}}
            identity = {"installed_core": {"path": "p", "build_id": core_id}}
            qualify.cross_checks(commit, [stage], identity)
            qualify.cross_checks(commit, [{"name": "gate", "details": {}}], identity)
            self.refuses("archived a commit other than the recorded one", qualify.cross_checks,
                         "0" * 40, [stage], identity)
            self.refuses("differs from the installed archive's", qualify.cross_checks, commit,
                         [stage], {"installed_core": {"path": "p", "build_id": other_id}})
            # The freeze binding: the block, four golden hashes and the padding rows.
            record = root / qualify.FREEZE_RECORD_SUBPATH
            original = record.read_text(encoding="utf-8")
            binding = qualify.freeze_binding()
            self.assertEqual(binding["block"]["abi_version"], "0.1")
            self.assertEqual(binding["padding_hole_rows"],
                             ["t64_a struct_size 4 4", "t64_b rule 20 4"])
            self.assertEqual(sorted(binding["goldens"]), sorted(GOLDEN_FILES))
            for key, relative in GOLDEN_FILES.items():
                self.assertEqual(binding["goldens"][key]["path"], relative)
                self.assertEqual(binding["goldens"][key]["sha256"],
                                 hashlib.sha256((root / relative).read_bytes()).hexdigest())
            self.assertEqual(binding, qualify.freeze_binding())
            for relative in GOLDEN_FILES.values():
                golden = root / relative
                data = golden.read_bytes()
                golden.write_bytes(data + b"edited\n")
                self.refuses(f"the golden {relative} hashes to", qualify.freeze_binding)
                golden.write_bytes(data)
            lines = original.splitlines()
            first_hash = next(line for line in lines if line.startswith("header_sha256: "))
            variants = {
                "does not hold exactly one machine-read block": original.replace(
                    "```freeze-record", "```freeze-records"),
                "states abi_version twice": original.replace(
                    "abi_version: 0.1\n", "abi_version: 0.1\nabi_version: 0.1\n"),
                "no SHA-256 for header_sha256": original.replace(
                    first_hash, "header_sha256: abc"),
                "lacks status_count": original.replace("status_count: 1\n", ""),
                "padding hole count that its rows do not match": original.replace(
                    "padding_holes: 2", "padding_holes: 3"),
                "malformed line": original.replace("amendments: 0", "amendments 0"),
                "hashes to": original.replace(first_hash, "header_sha256: " + "0" * 64),
            }
            for needle, text in variants.items():
                record.write_text(text, encoding="utf-8")
                self.refuses(needle, qualify.freeze_binding)
            record.write_text(original + "\n```freeze-record\nabi_version: 0.1\n```\n",
                              encoding="utf-8")
            self.refuses("does not hold exactly one machine-read block", qualify.freeze_binding)
            record.write_text(original, encoding="utf-8")
            self.assertEqual(qualify.freeze_binding(), binding)

    def test_no_undefined_global_names(self) -> None:
        for path in (QUALIFIER, Path(__file__)):
            self.assertEqual(_undefined_global_names(path), {}, path.name)


def main() -> int:
    args = sys.argv[1:]
    if args == ["--list-cases"]:
        print("\n".join(CASE_NAMES))
        return 0
    if args == ["--selftest"]:
        for name in CASE_NAMES:
            print(f"case reached: {name}")
        sys.argv = [sys.argv[0], "-v"]
        unittest.main(verbosity=2)
        return 0
    if len(args) == 2 and args[0] == "--case" and args[1] in CASE_NAMES:
        suite = unittest.TestSuite(
            [Phase12Qualification("test_" + args[1].replace("-", "_"))])
        print(f"case reached: {args[1]}")
        return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
    print("usage: test_phase12_qualification.py --list-cases | --selftest | --case NAME",
          file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
