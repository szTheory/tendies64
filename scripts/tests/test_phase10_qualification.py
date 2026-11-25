#!/usr/bin/env python3
"""Hermetic controls of the Phase 10 qualifier (scripts/qualify-phase10.py).

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
import subprocess
import sys
import tempfile
import time
import unittest
from collections.abc import Iterator
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
QUALIFIER = ROOT / "scripts/qualify-phase10.py"

_SPEC = importlib.util.spec_from_file_location("t64_qualify_phase10", QUALIFIER)
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
    "private-root-unsafe-refuses",
    "bounded-runner-deadline-and-descendant",
    "tier-and-claims-pinned",
    "real-stage-commands-start",
    "cli-modes-and-help",
    "no-undefined-global-names",
)

STUB_NAMES = ["t.one", "t.two"]
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


def _pin_text(**labels: str) -> str:
    lines = ["## Wave bases", ""]
    for label, value in labels.items():
        lines.append(f"- {label.replace('_', ' ')}: {value}")
    return "\n".join(lines) + "\n"


@contextlib.contextmanager
def scratch_repo() -> Iterator[dict]:
    """A scratch repository with the qualifier's inputs; ROOT is patched to it.

    Layout: commit A (tools, inventory block, a.c, old.c), then commit B (a.c
    changed, b.c added, old.c deleted, a planning file, the CI ledger and the
    PIN-IMPACT file with the Wave 0 to Wave 3 bases at A). HEAD is B.
    """
    with tempfile.TemporaryDirectory(prefix="t64-p10q-") as name:
        root = Path(name).resolve()
        _git(root, "init", "-q")
        _write(root, ".gitignore", "build/\n")
        _write(root, "tool_a.txt", "tool a\n")
        _write(root, "scripts/check-test-inventory.sh",
               "PHASE10_TESTS='" + "\n".join(STUB_NAMES) + "'\n")
        _write(root, "a.c", "int a = 1;\n")
        _write(root, "old.c", "int old = 1;\n")
        base = _commit(root, "A")
        _write(root, "a.c", "int a = 2;\n")
        _write(root, "b.c", "int b = 1;\n")
        (root / "old.c").unlink()
        _write(root, ".planning/note.md", "planning\n")
        _write(root, "docs/ci-reviews/LOG.md", "ledger\n")
        _write(root, PIN_RELATIVE, _pin_text(
            Wave_0_base=base, Wave_1_base=base, Wave_2_base=base, Wave_3_base=base))
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


class Phase10Qualification(unittest.TestCase):
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
            self.assertEqual(set(accepted), {"Wave 0 base", "Wave 1 base", "Wave 2 base",
                                             "Wave 3 base"})
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
            # A final record needs the qualification base as well.
            pin.write_text(original, encoding="utf-8")
            self.refuses("has no '- Qualification base:' line", qualify.read_bases, pin,
                         final=True)
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
            self.assertEqual(snapshot["excluded"]["files"], ["docs/ci-reviews/LOG.md"])
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
                self.assertEqual(record["schema"], "t64.phase10-qualification")
                self.assertIs(record["rehearsal"], False)
                self.assertIsNone(record["statement"])
                self.assertEqual(record["tier"], "unit-tested, authored scope")
                self.assertEqual(record["claims"],
                                 {"hardware_accuracy": False, "oracle_comparison": False})
                self.assertEqual(record["wave_bases"]["Wave 0 base"], scratch["base"])
                self.assertEqual(record["wave_bases"]["Qualification base"], audited)
                self.assertEqual(record["inventory"]["names"], sorted(STUB_NAMES))
                self.assertEqual(record["inventory"]["count"], record["inventory"]["label_count"])
                self.assertEqual([s["name"] for s in record["stages"]], ["alpha", "beta"])
                self.assertEqual(record["qualification_drift_paths"], [])
                self.assertEqual(record["source_snapshot"]["base"], scratch["base"])
                # Planning and CI-ledger commits after the audited head change no bound input.
                _write(scratch["root"], ".planning/later.md", "later\n")
                _write(scratch["root"], "docs/ci-reviews/LOG.md", "ledger later\n")
                _commit(scratch["root"], "planning and ledger only")
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
                with tempfile.TemporaryDirectory(prefix="t64-p10q-out-") as name:
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
            for record in (scratch["root"] / qualify.PRIVATE_SUBPATH).rglob("record.json"):
                self.fail(f"a failed run left a record: {record}")

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
        with tempfile.TemporaryDirectory(prefix="t64-p10q-run-") as name:
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
                                     timeout_seconds=10, env_delta={"T64_P10Q": "1"})
            self.assertEqual((ok["return_code"], ok["timed_out"]), (3, False))
            self.assertEqual((ok["stdout"], ok["stderr"]), (b"out\n", b"err\n"))
            missing = qualify.run_bounded(["/nonexistent/t64-p10q"], timeout_seconds=5)
            self.assertEqual(missing["return_code"], 127)

    # -- claims -------------------------------------------------------------------

    def test_tier_and_claims_pinned(self) -> None:
        self.assertEqual(qualify.TIER, "unit-tested, authored scope")
        self.assertEqual(qualify.CLAIMS, {"hardware_accuracy": False,
                                          "oracle_comparison": False})
        with scratch_repo() as scratch:
            path = self._native(scratch, rehearsal=True)
            with mock.patch.object(qualify, "_stage_table", _stub_table):
                for change in (lambda r: r.update(tier="hardware-accurate"),
                               lambda r: r["claims"].update(hardware_accuracy=True),
                               lambda r: r["claims"].update(oracle_comparison=True),
                               lambda r: r.pop("claims")):
                    with mutated_record(path, change):
                        self.refuses("tier or claims", qualify.verify_record, path,
                                     rehearsal=True)
                qualify.verify_record(path, rehearsal=True)

    # -- real, read-only stage commands ------------------------------------------

    def test_real_stage_commands_start(self) -> None:
        """The real oracle and CI scanner commands start and satisfy their markers."""
        bases = {qualify.WAVE0_LABEL: "0" * 40}
        table = {stage["name"]: stage for stage in qualify._stage_table(bases)}
        self.assertIn("oracle", table)
        for name in ("oracle",):
            for spec in table[name]["commands"]:
                outcome = qualify.run_bounded(spec["argv"],
                                              timeout_seconds=spec["limit_seconds"])
                self.assertEqual(outcome["return_code"], spec["expect_exit"], spec["argv"])
                self.assertFalse(outcome["timed_out"])
                qualify.command_details(spec, outcome["stdout"], outcome["stderr"], ROOT)

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
        refused = run("--verify", "build/gate/10-qualification-private/none/record.json")
        self.assertEqual(refused.returncode, 1)
        self.assertIn("refused original evidence", refused.stderr)
        self.assertNotIn("Traceback", refused.stderr)

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
            [Phase10Qualification("test_" + args[1].replace("-", "_"))])
        print(f"case reached: {args[1]}")
        return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
    print("usage: test_phase10_qualification.py --list-cases | --selftest | --case NAME",
          file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
