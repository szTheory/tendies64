#!/usr/bin/env python3
"""Controls for the Phase 9 harness guards and the fail-closed qualifier stub.

Cases are listed with --list-cases, run with --selftest (every case, verbose)
or one at a time with --case NAME. No builds and no nested CTest run here.
"""

from __future__ import annotations

import ast
import builtins
import copy
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import stat
import tempfile
import unittest
import warnings
import zipfile
from unittest import mock
from io import BytesIO
from pathlib import Path


_SCRIPT_ROOT = Path(__file__).resolve().parents[2]
_WORKING_ROOT = Path.cwd().resolve()
if (_SCRIPT_ROOT / "scripts/gate.sh").is_file():
    ROOT = _SCRIPT_ROOT
elif (_WORKING_ROOT / "scripts/gate.sh").is_file():
    ROOT = _WORKING_ROOT
else:
    ROOT = _SCRIPT_ROOT

# Every case of this file, in the order --list-cases prints them. The
# dispatcher below only runs names listed in this tuple.
CASE_NAMES: tuple[str, ...] = (
    "gate-junit-hook-private-phase09",
    "stub-modes-refuse",
    "hosted-rows-pinned-independently",
    "row-observation-refuses-each-field",
    "projection-closed-key-set-and-context",
    "upload-layouts-exactly-three",
    "rdp-native-junit-roots",
    "phase09-sibling-scan-context",
    "platform-pair-compare-invariants",
    "native-command-plan-and-private-root",
    "phase09-gcc13-closure-units",
    "native-record-rejects-failed-truncated-or-stale",
    "hosted-context-and-api-zip-bound",
    "hosted-record-rejects-entry-smoke-or-missing-platform",
    "rehearsal-flag-and-pending-sources",
    "final-evidence-cli-dispatch",
    "zero-case-inventory-refused",
    "inventory-shape-refused",
    "public-members-exact-allowlist",
    "public-member-private-suffix-refused",
    "public-member-state-magic-refused",
    "public-member-symlink-or-nested-refused",
    "source-paths-exist-and-cover-guards",
    "no-undefined-global-names",
    "case-checker-selftest-and-mismatch",
    "claim-skeleton-has-no-real-block",
)

# Named subsets run by the dedicated CTest names (no new logic, only grouping).
SUITES: dict[str, tuple[str, ...]] = {
    "stub": ("stub-modes-refuse", "zero-case-inventory-refused",
             "inventory-shape-refused"),
    "hosted-rows": ("hosted-rows-pinned-independently",
                    "row-observation-refuses-each-field",
                    "projection-closed-key-set-and-context"),
    "upload-contract": ("upload-layouts-exactly-three", "rdp-native-junit-roots",
                        "phase09-sibling-scan-context"),
    "collector": ("platform-pair-compare-invariants", "native-command-plan-and-private-root",
                  "phase09-gcc13-closure-units",
                  "native-record-rejects-failed-truncated-or-stale",
                  "hosted-context-and-api-zip-bound",
                  "hosted-record-rejects-entry-smoke-or-missing-platform",
                  "rehearsal-flag-and-pending-sources", "final-evidence-cli-dispatch"),
    "allowlist": ("public-members-exact-allowlist",
                  "public-member-private-suffix-refused",
                  "public-member-state-magic-refused",
                  "public-member-symlink-or-nested-refused"),
}

QUALIFIER = ROOT / "scripts/qualify-phase09.py"
INVENTORY = ROOT / "docs/spec/phase09-cases.json"
_SPEC = importlib.util.spec_from_file_location("t64_qualify_phase09", QUALIFIER)
assert _SPEC is not None and _SPEC.loader is not None
qualify = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(qualify)


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
            if args.vararg:
                bound.add(args.vararg.arg)
            if args.kwarg:
                bound.add(args.kwarg.arg)
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

# A placeholder root: no real checkout path appears in this file.
PLACEHOLDER_ROOT = "/r"

# ---- synthetic evidence helpers --------------------------------------------
# Shared by the hosted-rows suite and the later suites. Every value below is
# built here from labels; nothing is copied from a real record.

ORACLE = ROOT / "fixtures/pad/poll_oracle.py"
ROW_KEYS = ("row_id", "ctest", "fixture_id", "fixture_sha256", "profile_id",
            "profile_hash", "input_sha256", "expected_outcome", "expected_detail",
            "log_bytes", "log_sha256")
ROW_SCENARIOS = {"accepted-baseline": "baseline", "shifted-script": "shifted",
                 "neutral-script": "neutral", "wrong-rom": "wrong-rom"}
HEX64 = re.compile(r"^[0-9a-f]{64}$")
SYNTHETIC_STARTS = (500000, 521200, 542400, 563600)
SYNTHETIC_COMPLETION = 600000
SYNTHETIC_SHA = "1" * 40


def _digest(label: str) -> str:
    return hashlib.sha256(label.encode("utf-8")).hexdigest()


def _v2_record(row: dict, *, guest_tick: int = SYNTHETIC_COMPLETION,
               label: str = "synthetic", state_label: str = "shared-state") -> dict:
    """A result record in the exact v2 field shape that matches one authored row."""
    if row["expected_outcome"] == "harness-error":
        return {
            "build_id": None,
            "counts": {"log_bytes": "0", "log_bytes_total": "0"},
            "detail": row["expected_detail"],
            "evidence_kind": "guest-execution",
            "evidence_tier": "smoke",
            "fixture_id": None,
            "fixture_sha256": None,
            "guest_stop": {"bound_tick": "1000000", "kind": "refused"},
            "guest_tick": "0",
            "input_digest": None,
            "instruction_count": "0",
            "ipl3_payload_sha256": None,
            "ipl3_sha256": None,
            "log_sha256": None,
            "milestones": [],
            "outcome": row["expected_outcome"],
            "profile_digest": None,
            "profile_id": None,
            "schema": "t64.result",
            "schema_version": 2,
            "source_revision": None,
            "state_digest": None,
        }
    return {
        "build_id": "t64-build-v1-" + _digest(label + " core build"),
        "counts": {"log_bytes": str(row["log_bytes"]),
                   "log_bytes_total": str(row["log_bytes"])},
        "detail": row["expected_detail"],
        "evidence_kind": "guest-execution",
        "evidence_tier": "smoke",
        "fixture_id": row["fixture_id"],
        "fixture_sha256": row["fixture_sha256"],
        "guest_stop": {"bound_tick": "1000000", "kind": "authored-completion"},
        "guest_tick": str(guest_tick),
        "input_digest": row["input_sha256"],
        "instruction_count": "590000",
        "ipl3_payload_sha256": _digest("synthetic payload"),
        "ipl3_sha256": _digest("synthetic ipl3"),
        "log_sha256": row["log_sha256"],
        "milestones": [
            {"guest_tick": "0", "kind": "ipl3-entry", "pc": "ffffffffa4000040"},
            {"guest_tick": str(guest_tick), "kind": "hello-completion",
             "pc": "ffffffff800005a8"},
        ],
        "outcome": row["expected_outcome"],
        "profile_digest": row["profile_hash"],
        "profile_id": row["profile_id"],
        "schema": "t64.result",
        "schema_version": 2,
        "source_revision": SYNTHETIC_SHA,
        "state_digest": _digest(state_label + " state " + row["row_id"]),
    }


def _synthetic_summary(platform: str = "linux", label: str = "synthetic",
                       source_revision: str = SYNTHETIC_SHA) -> dict:
    """The accepted observations of one platform, as the suite reader returns them."""
    observations = []
    for row in qualify.HOSTED_ROWS:
        tick = 0 if row["expected_outcome"] == "harness-error" else SYNTHETIC_COMPLETION
        record = _v2_record(row, guest_tick=tick, label=label)
        observations.append(qualify.row_observation(record, row["input_sha256"], row))
    return {
        "platform": platform,
        "source_revision": source_revision,
        "observations": observations,
        "timing": {"poll_starts": list(SYNTHETIC_STARTS),
                   "completion_tick": SYNTHETIC_COMPLETION},
        "producer": {
            "source_revision": source_revision,
            "core_build_id": "t64-build-v1-" + _digest(label + " core build"),
            "runner_build_id": "t64-runner-v1-" + _digest(label + " runner build"),
            "real_runner_sha256": _digest(label + " runner"),
            "supervisor_sha256": _digest(label + " supervisor"),
            "checker_sha256": _digest(label + " checker"),
            "test_binary_sha256": _digest(label + " test binary"),
            "core_library_sha256": _digest(label + " core library"),
        },
        "execution": {
            "suite": "hosted",
            "ctests": ["runner.pad-poll", "controls.real.pad-wrong-value"],
            "command_sha256": _digest(label + " command"),
            "last_test_sha256": _digest(label + " last test"),
            "junit_sha256": _digest(label + " junit"),
            "return_code": 0, "status": "pass", "timed_out": False,
        },
        "inventory": qualify.inventory_snapshot(INVENTORY.read_bytes(), "hosted"),
    }


def _synthetic_projection(job: str = "gate-linux", platform: str | None = None,
                          label: str = "synthetic", run: str = "4242",
                          attempt: str = "1") -> dict:
    if platform is None:
        platform = "macos" if job == "gate-macos" else "linux"
    workflow_sha = None if job == "local" else SYNTHETIC_SHA
    return qualify._projection(
        _synthetic_summary(platform, label), job=job, workflow_sha=workflow_sha,
        run_id=None if job == "local" else run,
        attempt=None if job == "local" else attempt)


def _context(job: str = "gate-linux", run: str = "4242", attempt: str = "1") -> dict:
    local = job == "local"
    return {"job": job, "sha": None if local else SYNTHETIC_SHA,
            "run": None if local else run, "attempt": None if local else attempt}


def _write_public(directory: Path, projection: dict) -> Path:
    directory.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(projection, sort_keys=True, separators=(",", ":")).encode() + b"\n"
    target = directory / "manifest.json"
    target.write_bytes(payload)
    target.chmod(0o644)
    return directory


def _oracle(scenario: str, flag: str) -> bytes:
    done = subprocess.run([sys.executable, "-B", str(ORACLE), "--scenario", scenario, flag],
                          capture_output=True, check=True)
    return done.stdout


_RDP_MODULE = None


def _rdp():
    """The shared RDP/VI qualifier module, loaded by path once."""
    global _RDP_MODULE
    if _RDP_MODULE is None:
        spec = importlib.util.spec_from_file_location("t64_qualify_rdp_vi_for_phase09",
                                                      ROOT / "scripts/qualify-rdp-vi.py")
        assert spec is not None and spec.loader is not None
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _RDP_MODULE = module
    return _RDP_MODULE


def _runner_table_row(fixture_id: str) -> tuple[str, str, str]:
    """(sha256, profile id, profile hash) of one runner/t64_real_fixture.c row."""
    text = (ROOT / "runner/t64_real_fixture.c").read_text(encoding="utf-8")
    found = re.search(r'\{"' + re.escape(fixture_id) +
                      r'",\s*"([0-9a-f]{64})",\s*\d+u,\s*"([^"]+)",\s*"([0-9a-f]{64})"', text)
    assert found is not None, fixture_id
    return found.group(1), found.group(2), found.group(3)



class Phase09Qualification(unittest.TestCase):
    def test_gate_junit_hook_private_phase09(self) -> None:
        """The N2 arm admits one more private directory and nothing else."""
        gate = (ROOT / "scripts/gate.sh").read_text(encoding="utf-8")
        self.assertIn('if [ -n "${T64_GATE_JUNIT_OUT:-}" ]; then', gate)
        block = re.search(r'  case "\$T64_GATE_JUNIT_OUT" in\n.*?\n  esac\n', gate, re.S)
        self.assertIsNotNone(block)
        assert block is not None
        root = PLACEHOLDER_ROOT
        cases = (
            # Accepted: the Phase 9 private directory, and the Phase 8 one.
            (f"{root}/build/gate/09-11-phase09-private/run/junit.xml", 0),
            (f"{root}/build/gate/08-05-audio-private/run/junit.xml", 0),
            # Refused: a `..` component in the middle and at the end.
            (f"{root}/build/gate/09-11-phase09-private/../x.xml", 2),
            (f"{root}/build/gate/09-11-phase09-private/a/..", 2),
            # Refused: outside both private directories, including near misses.
            (f"{root}/build/gate/other/junit.xml", 2),
            (f"{root}/build/gate/09-11-phase09-privatex/junit.xml", 2),
            (f"{root}/build/gate/09-11-phase09-private", 2),
        )
        failures = []
        for value, expected in cases:
            script = 'ROOT="$1"; T64_GATE_JUNIT_OUT="$2"\n' + block.group(0)
            result = subprocess.run(["sh", "-c", script, "sh", root, value],
                                    capture_output=True, text=True, check=False)
            print(f"gate hook {value} -> exit {result.returncode} (want {expected})",
                  file=sys.stderr)
            if result.returncode != expected:
                failures.append((value, result.returncode, expected))
        self.assertEqual(failures, [])
        # Both private arms are present exactly once; the run lines of the
        # hook and the plain branch are unchanged.
        self.assertEqual(gate.count('"$ROOT"/build/gate/08-05-audio-private/*)'), 1)
        self.assertEqual(gate.count('"$ROOT"/build/gate/09-11-phase09-private/*)'), 1)
        self.assertIn(
            'step ctest ctest --preset gate --output-junit "$T64_GATE_JUNIT_OUT"', gate)
        self.assertIn('step ctest ctest --preset gate\nfi', gate)

    # ---- evidence modes ----------------------------------------------------

    def _run_qualifier(self, *arguments: str) -> subprocess.CompletedProcess:
        return subprocess.run([sys.executable, "-B", str(QUALIFIER), *arguments],
                              capture_output=True, text=True, check=False, cwd=ROOT)

    def test_stub_modes_refuse(self) -> None:
        """No mode exits 0 without evidence; each refusal follows an accepted run."""
        accepted = self._run_qualifier("--help")
        self.assertEqual(accepted.returncode, 0, accepted.stderr)
        # No mode at all is a usage error, never a success.
        none = self._run_qualifier()
        self.assertNotEqual(none.returncode, 0)
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="t64-p09-modes.") as tmp, \
                tempfile.TemporaryDirectory(prefix="t64-p09-empty.", dir=ROOT / "build") as empty:
            root = Path(tmp)
            # Accepted baseline in the same setup: a real projection passes the scan.
            good = _write_public(root / "good", _synthetic_projection("local"))
            (root / "empty-public").mkdir()
            baseline = self._run_qualifier("--scan-pii", "--job", "local", "--out", str(good))
            self.assertEqual(baseline.returncode, 0, baseline.stderr)
            self.assertIn("strict public allowlist and PII scan passed", baseline.stdout)
            refused = {
                "emit": self._run_qualifier(
                    "--emit-hosted", "--job", "local", "--build", empty,
                    "--suite", "hosted", "--artifact-prefix", "t64-p09-none",
                    "--out", str(root / "emit-out")),
                "verify-record": self._run_qualifier(
                    "--verify-record", str(root / "absent-run"),
                    "--build", str(ROOT / "build/gate"),
                    "--last-test", str(root / "LastTest.log"),
                    "--junit", str(root / "junit.xml")),
                "scan": self._run_qualifier(
                    "--scan-pii", "--job", "local", "--out", str(root / "empty-public")),
            }
            needles = {"emit": "does not select exactly one",
                       "verify-record": "retained run directory is missing or unsafe",
                       "scan": "no manifest.json"}
            for name, result in refused.items():
                self.assertEqual(result.returncode, 1, (name, result.stdout, result.stderr))
                self.assertIn("refused original evidence", result.stderr, name)
                self.assertIn(needles[name], result.stderr, name)
                self.assertEqual(result.stdout, "", name)
            # The native, hosted-collection and final modes refuse bad context.
            missing = str(root / "missing.json")
            context = ["--repo", "szTheory/tendies64", "--sha", "d" * 40, "--run", "5",
                       "--attempt", "1"]
            for label, mode, needle in (
                    ("native", ["--native", "--sha", "d" * 40, "--out", str(root / "native")],
                     "native source SHA must exactly match"),
                    ("collect", ["--collect-hosted", *context, "--native-record", missing,
                                 "--out", str(root / "public")],
                     "exact repository/SHA/run/attempt"),
                    ("verify", ["--verify-evidence", *context, "--native-record", missing,
                                "--hosted-record", missing],
                     "hosted record is outside the private qualification tree")):
                result = self._run_qualifier(*mode)
                self.assertEqual(result.returncode, 1, (label, result.stdout, result.stderr))
                self.assertIn("refused original evidence", result.stderr, label)
                self.assertIn(needle, result.stderr, label)
                self.assertEqual(result.stdout, "", label)
            # The baseline is still accepted after every refusal.
            again = self._run_qualifier("--scan-pii", "--job", "local", "--out", str(good))
            self.assertEqual(again.returncode, 0, again.stderr)

    # ---- authored rows, observations and the public projection --------------

    def test_hosted_rows_pinned_independently(self) -> None:
        """Every row is pinned from the oracle, the manifest and the runner table."""
        rows = qualify.HOSTED_ROWS
        self.assertEqual(len(rows), 5)
        manifest = json.loads((ROOT / "fixtures/manifest.json").read_text(encoding="utf-8"))
        manifest_rows = {entry["id"]: entry for entry in manifest["entries"]}
        keys: set[tuple[str, str]] = set()
        ids: set[str] = set()
        for row in rows:
            self.assertEqual(tuple(row), ROW_KEYS, row.get("row_id"))
            for name in ("fixture_sha256", "profile_hash", "input_sha256", "log_sha256"):
                self.assertRegex(row[name], HEX64, f"{row['row_id']} {name}")
            key = (row["fixture_id"], row["input_sha256"])
            self.assertNotIn(key, keys, f"duplicate (fixture_id, input_sha256) for {row['row_id']}")
            self.assertNotIn(row["row_id"], ids)
            keys.add(key)
            ids.add(row["row_id"])
            self.assertIn(row["ctest"], ("runner.pad-poll", "controls.real.pad-wrong-value"))
            scenario = ROW_SCENARIOS.get(row["row_id"])
            if scenario is not None:
                transcript = _oracle(scenario, "--print")
                self.assertEqual(row["log_bytes"], int(_oracle(scenario, "--bytes")), scenario)
                self.assertEqual(row["log_bytes"], len(transcript), scenario)
                self.assertEqual(row["log_sha256"], _oracle(scenario, "--sha256").decode().strip())
                # A second, independent hash over the literal oracle text.
                self.assertEqual(row["log_sha256"], hashlib.sha256(transcript).hexdigest())
                # Fixture and profile identity match the manifest and the runner table.
                self.assertIn(row["fixture_id"], manifest_rows)
                self.assertEqual(row["fixture_sha256"], manifest_rows[row["fixture_id"]]["sha256"])
                self.assertEqual(row["profile_id"], manifest_rows[row["fixture_id"]]["profile"])
                table = _runner_table_row(row["fixture_id"])
                self.assertEqual((row["fixture_sha256"], row["profile_id"], row["profile_hash"]),
                                 table, row["row_id"])
            else:
                # The replacement row: no transcript, an unregistered altered copy.
                self.assertEqual(row["row_id"], "replacement-refused")
                self.assertEqual(row["log_bytes"], 0)
                self.assertEqual(row["log_sha256"], hashlib.sha256(b"").hexdigest())
                self.assertEqual((row["expected_outcome"], row["expected_detail"]),
                                 ("harness-error", "refusal"))
                self.assertNotIn(row["fixture_id"], manifest_rows)
                baseline = next(item for item in rows if item["row_id"] == "accepted-baseline")
                table = _runner_table_row(baseline["fixture_id"])
                self.assertEqual((row["profile_id"], row["profile_hash"]), table[1:])
                # The altered copy is the built baseline ROM with its last byte flipped.
                rom = (ROOT / "build/fixtures/phase09-pad-poll-si-v1.z64").read_bytes()
                self.assertEqual(hashlib.sha256(rom).hexdigest(), baseline["fixture_sha256"])
                altered = rom[:-1] + bytes([rom[-1] ^ 1])
                self.assertEqual(row["fixture_sha256"], hashlib.sha256(altered).hexdigest())
                self.assertNotEqual(row["fixture_sha256"], baseline["fixture_sha256"])
        self.assertEqual([row["row_id"] for row in rows[:1]], ["accepted-baseline"])
        self.assertEqual(rows[0]["ctest"], "runner.pad-poll")
        self.assertTrue(all(row["ctest"] == "controls.real.pad-wrong-value" for row in rows[1:]))
        self.assertEqual((qualify.POLL_PAIR_MIN_TICKS, qualify.POLL_HANDSHAKE_TICKS),
                         (21067, 16971))

    def test_row_observation_refuses_each_field(self) -> None:
        """A matching v2 record is accepted; each changed field is refused by name."""
        rows = qualify.HOSTED_ROWS
        for row in rows:
            observation = qualify.row_observation(_v2_record(row, guest_tick=0 if
                                                  row["expected_outcome"] == "harness-error"
                                                  else 600001), row["input_sha256"], row)
            self.assertEqual(set(observation) >= {"guest_tick", "state_digest"}, True)
        pass_row, guest_fail_row, refusal_row = rows[0], rows[1], rows[-1]
        other = _digest("another value")
        mutations = (
            ("outcome", lambda r: r.__setitem__("outcome", "harness-error")),
            ("detail", lambda r: r.__setitem__("detail", "refusal")),
            ("log_bytes", lambda r: r["counts"].__setitem__("log_bytes", "67")),
            ("log_sha256", lambda r: r.__setitem__("log_sha256", other)),
            ("fixture_id", lambda r: r.__setitem__("fixture_id", "phase09-admit-si-v1")),
            ("fixture_sha256", lambda r: r.__setitem__("fixture_sha256", other)),
            ("profile_id", lambda r: r.__setitem__("profile_id", "other-profile")),
            ("profile_hash", lambda r: r.__setitem__("profile_digest", other)),
            ("input_sha256", lambda r: r.__setitem__("input_digest", other)),
            ("guest_tick", lambda r: r.__setitem__("guest_tick", "0")),
            ("guest_tick", lambda r: r.__setitem__("guest_tick", "not-a-number")),
        )
        for row in (pass_row, guest_fail_row):
            self.assertIsInstance(qualify.row_observation(
                _v2_record(row), row["input_sha256"], row)["guest_tick"], int)
            for name, change in mutations:
                record = copy.deepcopy(_v2_record(row))
                change(record)
                with self.assertRaises(qualify.Refusal, msg=(row["row_id"], name)) as caught:
                    qualify.row_observation(record, row["input_sha256"], row)
                self.assertIn(f"result {name} differs", str(caught.exception),
                              (row["row_id"], name))
                # The same row is still accepted in the same setup.
                self.assertEqual(qualify.row_observation(
                    _v2_record(row), row["input_sha256"], row)["guest_tick"],
                    SYNTHETIC_COMPLETION)
            # The retained input script is bound too.
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.row_observation(_v2_record(row), other, row)
            self.assertIn("input_sha256", str(caught.exception))
        # The refusal row: the recorded refusal outcome, detail and guest tick 0.
        accepted = qualify.row_observation(_v2_record(refusal_row), refusal_row["input_sha256"],
                                           refusal_row)
        self.assertEqual(accepted["guest_tick"], 0)
        for name, change in (
                ("outcome", lambda r: r.__setitem__("outcome", "pass")),
                ("detail", lambda r: r.__setitem__("detail", "positive-match")),
                ("guest_tick", lambda r: r.__setitem__("guest_tick", "5")),
                ("fixture_id", lambda r: r.__setitem__("fixture_id", pass_row["fixture_id"]))):
            record = copy.deepcopy(_v2_record(refusal_row))
            change(record)
            with self.assertRaises(qualify.Refusal, msg=name) as caught:
                qualify.row_observation(record, refusal_row["input_sha256"], refusal_row)
            self.assertIn(f"result {name} differs", str(caught.exception), name)
            self.assertEqual(qualify.row_observation(
                _v2_record(refusal_row), refusal_row["input_sha256"], refusal_row)["guest_tick"], 0)
        # A record that is not the v2 shape is refused before any field is read.
        for change in (lambda r: r.__setitem__("schema_version", 1),
                       lambda r: r.__setitem__("schema", "t64.other")):
            record = copy.deepcopy(_v2_record(pass_row))
            change(record)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.row_observation(record, pass_row["input_sha256"], pass_row)
            self.assertIn("v2 shape", str(caught.exception))

    def test_projection_closed_key_set_and_context(self) -> None:
        """The projection has exactly the public keys and is bound to its context."""
        host = qualify._host_platform()
        job = "gate-macos" if host == "macos" else "gate-linux"
        projection = _synthetic_projection(job, platform=host)
        self.assertEqual(set(projection), qualify.PUBLIC_KEYS)
        self.assertEqual([row["row_id"] for row in projection["rows"]],
                         [row["row_id"] for row in qualify.HOSTED_ROWS])
        self.assertEqual(projection["platform"], host)
        for row, public in zip(qualify.HOSTED_ROWS, projection["rows"]):
            self.assertEqual({k: v for k, v in public.items()
                              if k not in ("guest_tick", "state_digest")}, row)
        context = _context(job)
        self.assertIs(qualify.public_projection(projection, context), projection)
        # The local entry path is accepted with no run identity at all.
        local = _synthetic_projection("local", platform=host)
        self.assertIs(qualify.public_projection(local, _context("local")), local)

        def refuse(value: dict, expected: dict, needle: str, label: str) -> None:
            with self.assertRaises(qualify.Refusal, msg=label) as caught:
                qualify.public_projection(value, expected)
            self.assertIn(needle, str(caught.exception), label)
            # The accepted baseline is still accepted in the same setup.
            self.assertIs(qualify.public_projection(projection, context), projection)

        extra = copy.deepcopy(projection)
        extra["extra"] = 1
        refuse(extra, context, "unknown or missing top-level fields", "extra key")
        missing = copy.deepcopy(projection)
        del missing["timing"]
        refuse(missing, context, "unknown or missing top-level fields", "missing key")
        short = copy.deepcopy(projection)
        short["rows"] = short["rows"][:-1]
        refuse(short, context, "rows differ", "missing row")
        longer = copy.deepcopy(projection)
        longer["rows"].append(dict(longer["rows"][0], row_id="extra-row"))
        refuse(longer, context, "rows differ", "extra row")
        duplicated = copy.deepcopy(projection)
        duplicated["rows"][1] = copy.deepcopy(duplicated["rows"][0])
        refuse(duplicated, context, "rows differ", "duplicated row")
        zero = copy.deepcopy(projection)
        zero["rows"] = []
        refuse(zero, context, "zero rows", "zero rows")
        refuse(projection, dict(context, job="gate-linux" if job == "gate-macos" else "gate-macos"),
               "exact supplied run context", "wrong job")
        refuse(projection, dict(context, sha="2" * 40), "exact supplied run context", "wrong SHA")
        refuse(projection, dict(context, run="9999"), "exact supplied run context", "wrong run")
        refuse(projection, dict(context, attempt="2"), "exact supplied run context",
               "wrong attempt")
        no_run = copy.deepcopy(projection)
        no_run["run_id"] = None
        refuse(no_run, context, "missing its exact run and attempt", "hosted record without run")
        no_attempt = copy.deepcopy(projection)
        no_attempt["run_attempt"] = None
        refuse(no_attempt, context, "missing its exact run and attempt",
               "hosted record without attempt")
        entry_with_run = copy.deepcopy(local)
        entry_with_run["run_id"] = "7"
        with self.assertRaises(qualify.Refusal) as caught:
            qualify.public_projection(entry_with_run, _context("local"))
        self.assertIn("unexpectedly claims a hosted run", str(caught.exception))
        other_platform = "linux" if host == "macos" else "macos"
        wrong_platform = copy.deepcopy(projection)
        wrong_platform["platform"] = other_platform
        refuse(wrong_platform, context, "platform does not match the exact workflow job",
               "platform swapped")
        unsupported = copy.deepcopy(projection)
        unsupported["platform"] = "windows"
        refuse(unsupported, context, "host platform is unsupported", "unsupported platform")

    # ---- shared upload contract ---------------------------------------------

    @staticmethod
    def _layout_fixture(script: Path, job: str, sha: str, run: str, attempt: str,
                        out: Path) -> None:
        done = subprocess.run(
            [sys.executable, "-B", str(script), "--layout-fixture", "--job", job,
             "--sha", sha, "--run", run, "--attempt", attempt, "--out", str(out)],
            capture_output=True, text=True, check=False, cwd=ROOT)
        assert done.returncode == 0, done.stderr

    def _combined_members(self, root: Path, job: str, sha: str, run: str,
                          attempt: str) -> tuple[list[str], dict[str, bytes]]:
        """Directories and files of the third layout for one job (synthetic)."""
        phase09 = root / "phase09-fixture"
        audio = root / "audio-fixture"
        self._layout_fixture(Path(__file__).resolve(), job, sha, run, attempt, phase09)
        self._layout_fixture(ROOT / "scripts/tests/test_audio_qualification.py", job, sha,
                             run, attempt, audio)
        boot = {"schema": "t64.boot-evidence", "schema_version": 1, "commit_sha": sha,
                "run_id": run, "run_attempt": attempt, "job_name": job, "result": {}}
        files = {f"{job}.json": json.dumps(boot, sort_keys=True).encode()}
        for name in ("manifest.json", "inventory.json", "observation.json", "raw-export.json"):
            files[f"rdp-vi/{job}/{name}"] = b"{}\n"
        files[f"audio/{job}/manifest.json"] = (audio / "manifest.json").read_bytes()
        files[f"phase09/{job}/manifest.json"] = (phase09 / "manifest.json").read_bytes()
        dirs = ["rdp-vi", f"rdp-vi/{job}", "audio", f"audio/{job}", "phase09", f"phase09/{job}"]
        return dirs, files

    @staticmethod
    def _zip_bytes(dirs: list[str], files: dict[str, bytes], *, duplicate: str | None = None,
                   symlink: str | None = None) -> bytes:
        buffer = BytesIO()
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            with zipfile.ZipFile(buffer, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                for name in sorted(dirs):
                    archive.writestr(name + "/", b"")
                for name in sorted(files):
                    archive.writestr(name, files[name])
                if duplicate is not None:
                    archive.writestr(duplicate, files[duplicate])
                if symlink is not None:
                    link = zipfile.ZipInfo(symlink)
                    link.create_system = 3
                    link.external_attr = (stat.S_IFLNK | 0o777) << 16
                    archive.writestr(link, b"gate-linux.json")
        return buffer.getvalue()

    def test_upload_layouts_exactly_three(self) -> None:
        """Boot and raw, plus audio, plus audio and phase09 are the only layouts."""
        rdp = _rdp()
        job, other = "gate-linux", "gate-macos"
        sha, run, attempt = "a" * 40, "4242", "1"
        with tempfile.TemporaryDirectory(prefix="t64-p09-upload.") as tmp:
            root = Path(tmp)
            dirs, files = self._combined_members(root, job, sha, run, attempt)
            (root / "other").mkdir()
            _, other_files = self._combined_members(root / "other", other, sha, run, attempt)
            counter = [0]

            def extract(archive: bytes, label: str) -> Path:
                counter[0] += 1
                destination = root / f"out-{counter[0]}-{label}"
                rdp._safe_extract_upload(archive, destination, job)
                return destination

            def layout(include_audio: bool, include_phase09: bool,
                       raw: bool = True) -> tuple[list[str], dict[str, bytes]]:
                keep_dirs, keep_files = [], {}
                for name in files:
                    top = name.split("/")[0]
                    if top == "audio" and not include_audio:
                        continue
                    if top == "phase09" and not include_phase09:
                        continue
                    if top == "rdp-vi" and not raw:
                        continue
                    keep_files[name] = files[name]
                for name in dirs:
                    top = name.split("/")[0]
                    if (top == "audio" and not include_audio) or \
                            (top == "phase09" and not include_phase09) or \
                            (top == "rdp-vi" and not raw):
                        continue
                    keep_dirs.append(name)
                return keep_dirs, keep_files

            # The three layouts are accepted.
            for label, (audio, phase09) in (("legacy", (False, False)), ("audio", (True, False)),
                                            ("phase09", (True, True))):
                d, f = layout(audio, phase09)
                destination = extract(self._zip_bytes(d, f), label)
                self.assertTrue((destination / f"{job}.json").is_file(), label)
                self.assertEqual((destination / "phase09" / job / "manifest.json").is_file(),
                                 phase09, label)

            def refuse(archive: bytes, needle: str, label: str) -> None:
                with self.assertRaises(rdp.Refusal, msg=label) as caught:
                    extract(archive, "refused")
                self.assertIn(needle, str(caught.exception), label)
                # The accepted baseline is still accepted in the same setup.
                d, f = layout(True, True)
                accepted = extract(self._zip_bytes(d, f), "again")
                self.assertTrue((accepted / "phase09" / job / "manifest.json").is_file(), label)

            contract = "differ from the combined CI upload contract"
            d, f = layout(False, True)
            refuse(self._zip_bytes(d, f), contract, "phase09 without audio")
            d, f = layout(True, True, raw=False)
            refuse(self._zip_bytes(d, f), contract, "phase09 without raw members")
            d, f = layout(True, True)
            refuse(self._zip_bytes(d, dict(f, **{"unexpected.txt": b"x"})),
                   "unexpected or special file", "extra member")
            refuse(self._zip_bytes(d, f, duplicate=f"phase09/{job}/manifest.json"),
                   "duplicate or empty members", "duplicate member")
            refuse(self._zip_bytes(d, dict(f, **{"../escape.json": b"{}"})),
                   "traversal", "traversal member")
            moved = {name: data for name, data in f.items() if not name.startswith("phase09/")}
            moved[f"phase09/{other}/manifest.json"] = f[f"phase09/{job}/manifest.json"]
            refuse(self._zip_bytes(d + [f"phase09/{other}"], moved),
                   "unexpected", "phase09 manifest under the other job")
            wrong_job = dict(f)
            wrong_job[f"phase09/{job}/manifest.json"] = other_files[f"phase09/{other}/manifest.json"]
            refuse(self._zip_bytes(d, wrong_job),
                   "combined phase09 manifest failed its exact hosted context and privacy checks",
                   "phase09 manifest content for the other job")
            refuse(self._zip_bytes(d, dict(f, **{f"phase09/{job}/extra.json": b"{}"})),
                   "unexpected or special file", "extra file inside the phase09 directory")
            refuse(self._zip_bytes(d, f, symlink="unexpected-link"),
                   "symlink or encrypted member", "symlink member")

    def test_rdp_native_junit_roots(self) -> None:
        """The private JUnit predicate admits exactly the two private roots."""
        rdp = _rdp()
        roots = rdp.NATIVE_JUNIT_PRIVATE_ROOTS
        self.assertEqual(tuple(Path(root).relative_to(ROOT).as_posix() for root in roots),
                         ("build/gate/08-05-audio-private", "build/gate/09-11-phase09-private"))
        audio_root = ROOT / "build/gate/08-05-audio-private"
        phase09_root = ROOT / "build/gate/09-11-phase09-private"
        created = []
        for root in (audio_root, phase09_root):
            if not root.exists():
                root.mkdir(parents=True, mode=0o700)
                created.append(root)
        stem = f"t64-p09-junit-{os.getpid()}"
        existing = phase09_root / f"{stem}-existing.xml"
        link = phase09_root / f"{stem}-link.xml"
        try:
            existing.write_text("x", encoding="utf-8")
            link.symlink_to(existing)
            fresh_audio = audio_root / f"{stem}-fresh.xml"
            fresh_phase09 = phase09_root / f"{stem}-fresh.xml"
            self.assertTrue(rdp._native_junit_path_ok(str(fresh_audio)))
            self.assertTrue(rdp._native_junit_path_ok(str(fresh_phase09)))
            refused = (
                ("relative path", f"build/gate/09-11-phase09-private/{stem}.xml"),
                ("existing path", str(existing)),
                ("symlink", str(link)),
                ("outside both roots", str(ROOT / "build/gate/other" / f"{stem}.xml")),
                ("dot-dot component", f"{phase09_root}/../{stem}.xml"),
                ("dot-dot inside a root", f"{phase09_root}/sub/../{stem}.xml"),
                ("sibling prefix", f"{phase09_root}x/{stem}.xml"),
            )
            for label, value in refused:
                self.assertFalse(rdp._native_junit_path_ok(value), label)
                # The accepted fresh path is still accepted in the same setup.
                self.assertTrue(rdp._native_junit_path_ok(str(fresh_phase09)), label)
        finally:
            link.unlink(missing_ok=True)
            existing.unlink(missing_ok=True)
            for root in created:
                shutil.rmtree(root, ignore_errors=True)

    def test_phase09_sibling_scan_context(self) -> None:
        """The layout fixture passes the scan with its own context, not another."""
        sha, run, attempt = "b" * 40, "777", "2"
        with tempfile.TemporaryDirectory(prefix="t64-p09-scan.") as tmp:
            out = Path(tmp) / "fixture"
            self._layout_fixture(Path(__file__).resolve(), "gate-macos", sha, run, attempt, out)
            self.assertEqual([entry.name for entry in out.iterdir()], ["manifest.json"])

            def scan(job: str = "gate-macos", sha_: str = sha, run_: str = run,
                     attempt_: str = attempt) -> subprocess.CompletedProcess:
                return self._run_qualifier("--scan-pii", "--job", job, "--sha", sha_,
                                           "--run", run_, "--attempt", attempt_, "--out", str(out))

            accepted = scan()
            self.assertEqual(accepted.returncode, 0, accepted.stderr)
            for label, changed in (("job", {"job": "gate-linux"}), ("sha", {"sha_": "c" * 40}),
                                   ("run", {"run_": "778"}), ("attempt", {"attempt_": "3"})):
                refused = scan(**changed)
                self.assertEqual(refused.returncode, 1, label)
                self.assertIn("refused original evidence", refused.stderr, label)
                self.assertTrue(
                    "exact supplied run context" in refused.stderr or
                    "does not match the exact workflow job" in refused.stderr, (label, refused.stderr))
                self.assertEqual(scan().returncode, 0, label)


    # ---- collector, native record and rehearsal ------------------------------

    def _pair(self, **changes) -> dict[str, dict]:
        """Two manifests that differ exactly where real records differ by build."""
        linux = _synthetic_projection("gate-linux", label="linux build")
        macos = _synthetic_projection("gate-macos", label="macos build")
        for name, change in changes.items():
            change(linux if name.startswith("linux") else macos)
        return {"gate-linux": linux, "gate-macos": macos}

    def test_platform_pair_compare_invariants(self) -> None:
        """Build identities may differ; every authored and model-time value may not."""
        accepted = self._pair()
        # The stand-in is deliberately not an identical copy of one record.
        self.assertNotEqual(accepted["gate-linux"]["producer"], accepted["gate-macos"]["producer"])
        self.assertNotEqual(accepted["gate-linux"]["execution"]["command_sha256"],
                            accepted["gate-macos"]["execution"]["command_sha256"])
        qualify.compare_hosted_pair(accepted)

        def refuse(label: str, needle: str, change) -> None:
            pair = self._pair()
            change(pair["gate-macos"])
            with self.assertRaises(qualify.Refusal, msg=label) as caught:
                qualify.compare_hosted_pair(pair)
            self.assertIn(needle, str(caught.exception), label)
            qualify.compare_hosted_pair(self._pair())

        for index in (0, 1, 3, 4):
            row = qualify.HOSTED_ROWS[index]["row_id"]
            for field, value in (("log_sha256", _digest("other transcript")), ("log_bytes", 67),
                                 ("fixture_sha256", _digest("other fixture")),
                                 ("profile_hash", _digest("other profile")),
                                 ("input_sha256", _digest("other input")),
                                 ("expected_outcome", "pass"), ("expected_detail", "other")):
                refuse(f"{row} {field}", f"rows[{index}].{field}",
                       lambda m, i=index, f=field, v=value: m["rows"][i].__setitem__(f, v))
        for index in (0, 1, 2, 3):
            refuse(f"state digest {index}", f"rows[{index}].state_digest",
                   lambda m, i=index: m["rows"][i].__setitem__("state_digest", _digest("x")))
            refuse(f"guest tick {index}", f"rows[{index}].guest_tick",
                   lambda m, i=index: m["rows"][i].__setitem__("guest_tick", 123456))
        for index in range(qualify.POLL_COUNT):
            refuse(f"poll start {index}", f"timing.poll_starts[{index}]",
                   lambda m, i=index: m["timing"]["poll_starts"].__setitem__(
                       i, m["timing"]["poll_starts"][i] + 1))
        refuse("completion tick", "timing.completion_tick",
               lambda m: m["timing"].__setitem__("completion_tick", 600001))
        refuse("missing row", "rows", lambda m: m["rows"].pop())
        refuse("evidence mode", "evidence_mode",
               lambda m: m.__setitem__("evidence_mode", "entry-path-validation"))
        refuse("source revision", "source_revision",
               lambda m: m.__setitem__("source_revision", "2" * 40))
        refuse("run id", "run_id", lambda m: m.__setitem__("run_id", "9999"))
        refuse("attempt", "run_attempt", lambda m: m.__setitem__("run_attempt", "2"))
        refuse("execution status", "execution.status",
               lambda m: m["execution"].__setitem__("status", "fail"))
        refuse("inventory", "inventory",
               lambda m: m["inventory"].__setitem__("case_count", 99))
        with self.assertRaises(qualify.Refusal) as caught:
            qualify.compare_hosted_pair({"gate-linux": accepted["gate-linux"]})
        self.assertIn("both named platform jobs", str(caught.exception))
        twice = {"gate-linux": accepted["gate-linux"],
                 "gate-macos": copy.deepcopy(accepted["gate-linux"])}
        with self.assertRaises(qualify.Refusal) as caught:
            qualify.compare_hosted_pair(twice)
        self.assertIn("one manifest per platform job", str(caught.exception))
        qualify.compare_hosted_pair(accepted)

    def test_native_command_plan_and_private_root(self) -> None:
        """The plan names every inventory suite on both builds under the private root."""
        suites = list(json.loads(INVENTORY.read_text(encoding="utf-8"))["suites"])
        self.assertGreaterEqual(len(suites), 5)
        normal_junit = qualify.PRIVATE_ROOT / "unit/normal-junit.xml"
        plan = qualify._native_command_plan("09-unit", normal_junit)
        expected = (["shared-native-gates"] + [f"normal-inventory-{name}" for name in suites]
                    + [f"asan-inventory-{name}" for name in suites]
                    + ["normal-source-body-check", "asan-source-body-check"])
        self.assertEqual([item["label"] for item in plan], expected)
        shared = plan[0]
        self.assertEqual(shared["command"][-1], "--native")
        self.assertEqual(shared["timeout_seconds"], 2400)
        self.assertEqual(shared["environment"], {"T64_GATE_JUNIT_OUT": str(normal_junit)})
        build_dirs = {"normal": str((ROOT / "build/gate").resolve()),
                      "asan": str((ROOT / "build/asan").resolve())}
        for item in plan[1:-2]:
            kind, _, suite = item["label"].partition("-inventory-")
            command = item["command"]
            self.assertEqual(command[command.index("--build") + 1], build_dirs[kind])
            self.assertEqual(command[command.index("--suite") + 1], suite)
            self.assertEqual(command[command.index("--artifact-prefix") + 1], f"09-unit-{kind}")
            self.assertNotIn("--run", command)
            self.assertEqual(item["timeout_seconds"], 720)
        for item, kind in zip(plan[-2:], ("normal", "asan")):
            self.assertEqual(item["command"][-1], build_dirs[kind])
            self.assertEqual(item["command"][-2], "--build")
        for label, prefix, junit in (
                ("outside the private root", "09-unit", ROOT / "build/gate/other/j.xml"),
                ("relative path", "09-unit", Path("build/gate/09-11-phase09-private/j.xml")),
                ("unsafe prefix", "09-un it", normal_junit),
                ("prefix without the phase marker", "08-unit", normal_junit),
                ("path escaping the root", "09-unit", qualify.PRIVATE_ROOT / "../j.xml")):
            with self.assertRaises(qualify.Refusal, msg=label) as caught:
                qualify._native_command_plan(prefix, junit)
            self.assertIn("native artifact prefix is malformed", str(caught.exception), label)
            self.assertEqual(len(qualify._native_command_plan("09-unit", normal_junit)), len(plan))

    def test_phase09_gcc13_closure_units(self) -> None:
        """The closure covers every changed C unit and keeps its compile policy."""
        head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True,
                              text=True, check=True).stdout.strip()
        self.assertRegex(qualify.PHASE09_SOURCE_BASE, r"^[0-9a-f]{40}$")
        ancestor = subprocess.run(["git", "merge-base", "--is-ancestor",
                                   qualify.PHASE09_SOURCE_BASE, head], cwd=ROOT, check=False)
        self.assertEqual(ancestor.returncode, 0)
        required = qualify._required_gcc_units(head)
        for unit in ("core/t64_si.c", "core/t64_joybus.c", "core/tests/test_si.c",
                     "runner/tests/test_pad_poll.c"):
            self.assertIn(unit, required)
        self.assertEqual(required, sorted(set(required)))
        rdp = qualify._rdp_helpers()
        sources = sorted(set(required) | {"runner/t64_real.c", "runner/t64_supervisor.c"})
        command = rdp._gcc13_closure_command(sources, qualify.EXPECTED_GCC13_IMAGE)
        self.assertEqual(qualify._validate_gcc13_closure(sources, command, head), sources)

        def refuse(label: str, needle: str, closure, built) -> None:
            fake = mock.Mock()
            fake._gcc13_closure_command = lambda units, image: built
            with mock.patch.object(qualify, "_rdp_helpers", return_value=fake):
                with self.assertRaises(qualify.Refusal, msg=label) as caught:
                    qualify._validate_gcc13_closure(closure, built, head)
            self.assertIn(needle, str(caught.exception), label)
            self.assertEqual(qualify._validate_gcc13_closure(sources, command, head), sources)

        refuse("dropped unit", "omits required", [u for u in sources if u != "core/t64_si.c"],
               command)
        wrong_image = rdp._gcc13_closure_command(sources, "sha256:" + "0" * 64)
        refuse("image differs", "differs from its exact source unit set", sources, wrong_image)
        # With a builder that agrees with the mutated command, the policy checks fire.
        script_index = command.index("sh") + 2

        def edited(old: str, new: str) -> list[str]:
            changed = list(command)
            self.assertIn(old, changed[script_index])
            changed[script_index] = changed[script_index].replace(old, new)
            return changed

        for label, needle, built in (
                ("frame flag absent", "core frame limit",
                 edited("-Wframe-larger-than=2048", "")),
                ("real testing define absent", "testing define",
                 edited("-DT64_REAL_RUNNER_TESTING", "-DT64_REAL")),
                ("supervisor testing define absent", "testing define",
                 edited("-DT64_SUPERVISOR_TESTING", "-DT64_SUPERVISOR"))):
            fake = mock.Mock()
            fake._gcc13_closure_command = lambda units, image, built=built: built
            with mock.patch.object(qualify, "_rdp_helpers", return_value=fake):
                with self.assertRaises(qualify.Refusal, msg=label) as caught:
                    qualify._validate_gcc13_closure(sources, built, head)
            self.assertIn(needle, str(caught.exception), label)
            self.assertEqual(qualify._validate_gcc13_closure(sources, command, head), sources)

    def _native_record(self, sha: str, *, rehearsal: bool = False) -> dict:
        inventory = qualify._inventory_summary(INVENTORY.read_bytes())
        suites = list(json.loads(INVENTORY.read_text(encoding="utf-8"))["suites"])
        return {
            "schema": "t64.phase09-native-qualification", "schema_version": 1,
            "evidence_mode": "native-qualification", "status": "complete",
            "rehearsal": rehearsal, "source_sha": sha,
            "source_snapshot": {"commit_sha": sha, "inputs": [], "absent_pending": []},
            "excluded_owner_metadata": [
                {"path": path, "present": True, "dirty_status": "", "size_bytes": 1,
                 "sha256": _digest(path)} for path in qualify.OWNER_METADATA_PATHS],
            "inventory": inventory,
            "normal_full_ctest_junit_path": "/private/normal-full-ctest-junit.xml",
            "shared_native_path": "/private/native",
            "shared_native_record_sha256": _digest("shared"),
            "shared_native_tree_manifest": [{"path": "source-binding.json"}],
            "shared_native_closure": {"units": ["core/t64_si.c"], "gcc13_command": ["docker", "run"]},
            "steps_path": "/private/steps",
            "suites": {"normal": {"inventory_suites": suites}, "asan": {"inventory_suites": suites}},
            "output_path": "/private/native-out",
        }

    def test_native_record_rejects_failed_truncated_or_stale(self) -> None:
        """A complete record passes; each stale, failed or truncated part is refused."""
        sha = "c" * 40
        record = self._native_record(sha)
        snapshot = record["source_snapshot"]
        qualify._validate_native_record_header(record, sha, expected_snapshot=snapshot)

        def refuse(label: str, needle: str, change, **kwargs) -> None:
            value = copy.deepcopy(record)
            change(value)
            with self.assertRaises(qualify.Refusal, msg=label) as caught:
                qualify._validate_native_record_header(
                    value, kwargs.pop("sha", sha), expected_snapshot=snapshot, **kwargs)
            self.assertIn(needle, str(caught.exception), label)
            qualify._validate_native_record_header(record, sha, expected_snapshot=snapshot)

        refuse("schema", "smoke-only, incomplete", lambda r: r.__setitem__("schema", "t64.other"))
        refuse("status", "smoke-only, incomplete", lambda r: r.__setitem__("status", "failed"))
        refuse("mode", "smoke-only, incomplete",
               lambda r: r.__setitem__("evidence_mode", "entry-path-validation"))
        refuse("source SHA", "smoke-only, incomplete", lambda r: None, sha="d" * 40)
        refuse("source snapshot", "source inputs differ",
               lambda r: r["source_snapshot"].__setitem__("commit_sha", "e" * 40))
        refuse("inventory", "authored case denominator",
               lambda r: r["inventory"].__setitem__("case_count", r["inventory"]["case_count"] - 1))
        refuse("suite not in the inventory", "suite that is not in the authored inventory",
               lambda r: r["suites"]["asan"]["inventory_suites"].append("ghost-suite"))
        refuse("missing build", "smoke-only, incomplete",
               lambda r: r["suites"].pop("asan"))
        # Pending sources: refused without the rehearsal flag, accepted with it.
        pending = copy.deepcopy(record)
        pending["source_snapshot"]["absent_pending"] = ["scripts/build-save-fixtures.sh"]
        with self.assertRaises(qualify.Refusal) as caught:
            qualify._validate_native_record_header(
                pending, sha, expected_snapshot=pending["source_snapshot"])
        self.assertIn("rehearsal", str(caught.exception))
        pending["rehearsal"] = True
        qualify._validate_native_record_header(
            pending, sha, expected_snapshot=pending["source_snapshot"], rehearsal=True)
        # Process originals: exit status, truncated output, retained tree, missing step.
        with tempfile.TemporaryDirectory(prefix="t64-p09-native.") as tmp:
            directory = Path(tmp) / "step"
            directory.mkdir()
            command = ["tool", "--exact", "argv"]
            stdout = b"finished successfully\n"
            (directory / "stdout.log").write_bytes(stdout)
            (directory / "stderr.log").write_bytes(b"")
            (directory / "command.json").write_text(json.dumps({
                "argv": command, "cwd": str(ROOT), "env": {}, "timeout_seconds": 5}))
            process = {"return_code": 0, "timed_out": False, "timeout_seconds": 5,
                       "stdout_bytes": len(stdout), "stderr_bytes": 0,
                       "stdout_sha256": _digest("x"), "stderr_sha256": _digest("y")}
            process["stdout_sha256"] = hashlib.sha256(stdout).hexdigest()
            process["stderr_sha256"] = hashlib.sha256(b"").hexdigest()
            process_path = directory / "process.json"
            process_path.write_text(json.dumps(process))
            qualify._verify_process_record(directory, command, max_timeout=10)
            retained = qualify._tree_manifest(directory)
            (directory / "stdout.log").write_bytes(stdout[:-3])
            with self.assertRaises(qualify.Refusal) as caught:
                qualify._verify_process_record(directory, command, max_timeout=10)
            self.assertIn("changed or truncated", str(caught.exception))
            self.assertNotEqual(qualify._tree_manifest(directory), retained)
            (directory / "stdout.log").write_bytes(stdout)
            self.assertEqual(qualify._tree_manifest(directory), retained)
            for changed, needle in ((dict(process, return_code=1), "absent, stale, or failed"),
                                    (dict(process, timed_out=True), "absent, stale, or failed")):
                process_path.write_text(json.dumps(changed))
                with self.assertRaises(qualify.Refusal) as caught:
                    qualify._verify_process_record(directory, command, max_timeout=10)
                self.assertIn(needle, str(caught.exception))
            process_path.write_text(json.dumps(process))
            qualify._verify_process_record(directory, command, max_timeout=10)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify._tree_manifest(Path(tmp) / "missing-step")
            self.assertIn("missing or unsafe", str(caught.exception))
            qualify._verify_process_record(directory, command, max_timeout=10)

    def test_hosted_context_and_api_zip_bound(self) -> None:
        """The API record, ZIP bytes and binding file all name the exact attempt."""
        sha, run, attempt, job = "c" * 40, "123", "1", "gate-linux"
        archive = b"bounded synthetic archive bytes"
        digest = "sha256:" + hashlib.sha256(archive).hexdigest()
        artifact = {"id": 401, "name": f"t64-authored-evidence-{sha}-{run}-{attempt}-{job}",
                    "expired": False, "size_in_bytes": len(archive), "digest": digest,
                    "workflow_run": {"id": 123, "head_sha": sha}}
        binding = {"artifact_id": "401", "commit_sha": sha, "run_id": run, "run_attempt": attempt,
                   "job_name": job, "archive_digest": digest, "archive_bytes": len(archive)}
        kwargs = dict(repo="szTheory/tendies64", sha=sha, run=run, attempt=attempt, job=job)
        self.assertIs(qualify._verify_artifact_download(artifact, binding, archive, **kwargs),
                      artifact)

        def refuse(label: str, needle: str, art=artifact, bind=binding, data=archive) -> None:
            with self.assertRaises(qualify.Refusal, msg=label) as caught:
                qualify._verify_artifact_download(art, bind, data, **kwargs)
            self.assertIn(needle, str(caught.exception), label)
            self.assertIs(qualify._verify_artifact_download(artifact, binding, archive, **kwargs),
                          artifact)

        identity = "API identity differs from the exact attempt"
        refuse("name", identity, art=dict(artifact, name=artifact["name"].replace("-1-", "-2-")))
        refuse("expired", identity, art=dict(artifact, expired=True))
        refuse("size", identity, art=dict(artifact, size_in_bytes=2 * 1024 * 1024))
        refuse("digest format", identity, art=dict(artifact, digest="sha1:abc"))
        refuse("workflow run id", identity,
               art=dict(artifact, workflow_run={"id": 124, "head_sha": sha}))
        refuse("head SHA", identity,
               art=dict(artifact, workflow_run={"id": 123, "head_sha": "d" * 40}))
        refuse("id type", identity, art=dict(artifact, id=True))
        refuse("binding field", "binding differs from its original ZIP",
               bind=dict(binding, run_attempt="2"))
        refuse("binding key set", "binding differs from its original ZIP",
               bind=dict(binding, extra="x"))
        refuse("archive bytes", "length or API digest differs", data=archive + b"x",
               bind=dict(binding, archive_bytes=len(archive) + 1))
        # The hosted context check binds repository, SHA, run, attempt, job and platform.
        projection = _synthetic_projection(job, run=run, attempt=attempt)
        context = dict(repo="szTheory/tendies64", sha=SYNTHETIC_SHA, run=run, attempt=attempt,
                       job=job)
        self.assertIs(qualify._validate_hosted_context(projection, **context), projection)
        for label, changed, needle in (
                ("repository", {"repo": "other/repo"}, "exact repository, SHA, run, attempt"),
                ("SHA", {"sha": "e" * 40}, "exact supplied run context"),
                ("run", {"run": "124"}, "exact supplied run context"),
                ("attempt", {"attempt": "2"}, "exact supplied run context"),
                ("job", {"job": "gate-macos"}, "platform does not match the exact workflow job")):
            with self.assertRaises(qualify.Refusal, msg=label) as caught:
                qualify._validate_hosted_context(projection, **dict(context, **changed))
            self.assertIn(needle, str(caught.exception), label)
            self.assertIs(qualify._validate_hosted_context(projection, **context), projection)

    def test_hosted_record_rejects_entry_smoke_or_missing_platform(self) -> None:
        """A hosted record must be complete hosted evidence for both platforms."""
        sha = "c" * 40
        base = {
            "schema": "t64.phase09-hosted-collection", "schema_version": 1,
            "evidence_mode": "hosted-gate", "status": "complete", "rehearsal": False,
            "repo": "szTheory/tendies64", "sha": sha, "run": "123", "attempt": "1",
            "jobs": ["gate-linux", "gate-macos"],
            "source_snapshot": {"absent_pending": []}, "excluded_owner_metadata": [],
            "native_record_path": "/private/native-record.json",
            "native_record_sha256": _digest("native"),
            "archive_bindings": [{}, {}],
            "collection_path": "/private/collection", "downloads_path": "/private/downloads",
            "combined_path": "/private/combined", "pii_input_path": "/private/pii-upload-layout",
            "collector_command_path": "/private/collector-command",
            "public_output_path": "/private/public",
        }
        kwargs = dict(repo="szTheory/tendies64", sha=sha, run="123", attempt="1")
        self.assertIs(qualify._validate_hosted_record_header(base, **kwargs), base)

        def refuse(label: str, needle: str, value: dict, **more) -> None:
            with self.assertRaises(qualify.Refusal, msg=label) as caught:
                qualify._validate_hosted_record_header(value, **dict(kwargs, **more))
            self.assertIn(needle, str(caught.exception), label)
            self.assertIs(qualify._validate_hosted_record_header(base, **kwargs), base)

        incomplete = "missing exact context, platforms, or private originals"
        refuse("entry smoke", incomplete, dict(base, evidence_mode="entry-path-validation"))
        refuse("one platform", incomplete, dict(base, jobs=["gate-linux"]))
        refuse("status", incomplete, dict(base, status="partial"))
        refuse("wrong SHA", incomplete, dict(base, sha="d" * 40))
        refuse("wrong run", incomplete, dict(base), run="124")
        refuse("missing path", incomplete, {k: v for k, v in base.items() if k != "pii_input_path"})
        refuse("native binding", incomplete, dict(base, native_record_sha256="short"))
        refuse("one binding", incomplete, dict(base, archive_bindings=[{}]))
        pending = dict(base, source_snapshot={"absent_pending": ["scripts/k5-mutants.py"]})
        refuse("pending without rehearsal", "rehearsal", pending)
        rehearsal_record = dict(pending, rehearsal=True)
        self.assertIs(qualify._validate_hosted_record_header(
            rehearsal_record, rehearsal=True, **kwargs), rehearsal_record)
        refuse("final record passed as rehearsal", "rehearsal", dict(base), rehearsal=True)

    def test_rehearsal_flag_and_pending_sources(self) -> None:
        """The final tuple is data; pending files gate final evidence, not rehearsal."""
        literal = (
            "CMakeLists.txt", "core", "runner", "tools/manifest", "tools/rights", "fixtures",
            "scripts/check-phase09-cases.py", "scripts/check-rsp-no-hle.py",
            "scripts/check-test-inventory.sh", "scripts/check-ci-yml.sh",
            "scripts/rights-scan.sh", "scripts/pii-scan.sh", "scripts/check-provenance.sh",
            "scripts/check-claims.sh", "scripts/gate.sh", "scripts/list-membership-sites.py",
            "docs/spec/profile-membership-sites.txt", "scripts/qualify-phase09.py",
            "scripts/tests/test_phase09_qualification.py", "scripts/tests/test_rsp_no_hle.py",
            "docs/spec/phase09-cases.json", ".github/workflows/ci.yml",
            "scripts/build-phase09-admit-fixture.sh", "scripts/build-pad-fixture.sh",
            "scripts/collect-boot-evidence.sh", "scripts/qualify-rdp-vi.py",
            "scripts/qualify-audio.py", "scripts/check-audio-cases.py",
            "scripts/build-boot-fixtures.sh",
            "scripts/build-save-fixtures.sh", "scripts/build-replay-fixture.sh",
            "scripts/check-state-field-use.py", "scripts/k5-mutants.py",
            "docs/spec/state-field-rules.txt", "docs/spec/state-mutants.txt",
        )
        self.assertEqual(qualify.SOURCE_PATHS + tuple(qualify.PENDING_SOURCE_PATHS), literal)
        self.assertEqual(qualify.PENDING_SOURCE_PATHS, {
            "scripts/build-save-fixtures.sh": "09-04",
            "scripts/build-replay-fixture.sh": "09-07",
            "scripts/check-state-field-use.py": "09-09",
            "scripts/k5-mutants.py": "09-06",
            "docs/spec/state-field-rules.txt": "09-09.1",
            "docs/spec/state-mutants.txt": "09-09.1",
        })
        tracked = subprocess.run(["git", "ls-files", "--", *qualify.SOURCE_PATHS], cwd=ROOT,
                                 capture_output=True, text=True, check=True).stdout.split()
        for relative in qualify.SOURCE_PATHS:
            self.assertTrue(any(item == relative or item.startswith(relative + "/")
                                for item in tracked), f"{relative} is not tracked")
        head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True,
                              text=True, check=True).stdout.strip()
        snapshot = qualify._source_snapshot(head, require_clean=False)
        present = [name for name in qualify.PENDING_SOURCE_PATHS if (ROOT / name).exists()]
        self.assertEqual(snapshot["absent_pending"],
                         sorted(set(qualify.PENDING_SOURCE_PATHS) - set(present)))
        self.assertIsInstance(snapshot["absent_pending"], list)
        # The gate: absent pending files refuse final evidence, a rehearsal passes.
        with self.assertRaises(qualify.Refusal) as caught:
            qualify._rehearsal_gate(["scripts/k5-mutants.py"], False)
        self.assertIn("pending source paths are absent", str(caught.exception))
        self.assertIn("rehearsal and not final evidence",
                      qualify._rehearsal_gate(["scripts/k5-mutants.py"], True))
        self.assertIsNone(qualify._rehearsal_gate([], False))
        # --rehearsal belongs to three modes only.
        for mode in (["--scan-pii", "--job", "local", "--out", "x"],
                     ["--emit-hosted", "--job", "local", "--build", "x", "--suite", "hosted",
                      "--artifact-prefix", "p", "--out", "o"],
                     ["--verify-record", "x", "--build", "b", "--last-test", "l", "--junit", "j"]):
            result = self._run_qualifier(*mode, "--rehearsal")
            self.assertEqual(result.returncode, 2, mode)
            self.assertIn("--rehearsal is only valid with", result.stderr, mode)
        accepted = self._run_qualifier("--verify-evidence", "--rehearsal")
        self.assertEqual(accepted.returncode, 2)
        self.assertNotIn("--rehearsal is only valid with", accepted.stderr)

    def test_final_evidence_cli_dispatch(self) -> None:
        """Every mode reaches its final entry through main, not only through helpers."""
        calls: list[tuple] = []
        ctx = ["--repo", "szTheory/tendies64", "--sha", "c" * 40, "--run", "123",
               "--attempt", "1"]

        def run_main(*arguments: str) -> tuple[int, str]:
            import io
            from contextlib import redirect_stderr, redirect_stdout
            out, err = io.StringIO(), io.StringIO()
            with mock.patch.object(sys, "argv", ["qualify-phase09.py", *arguments]), \
                    redirect_stdout(out), redirect_stderr(err):
                try:
                    code = qualify.main()
                except SystemExit as exit_:
                    code = int(exit_.code)
            return code, out.getvalue() + err.getvalue()

        def native(sha, out, *, rehearsal=False):
            calls.append(("native", sha, str(out), rehearsal))
            return Path("/private/native-record.json")

        def collect(repo, sha, run, attempt, native_path, out, *, rehearsal=False):
            calls.append(("collect", repo, sha, run, attempt, str(native_path), str(out), rehearsal))
            return Path("/private/hosted-record.json")

        def verify(path, *, repo, sha, run, attempt, native_path, rehearsal=False):
            calls.append(("verify", str(path), repo, sha, run, attempt, str(native_path), rehearsal))
            return {}

        with mock.patch.object(qualify, "_native_qualify", native), \
                mock.patch.object(qualify, "_collect_hosted", collect), \
                mock.patch.object(qualify, "_verify_hosted_record", verify):
            code, text = run_main("--native", "--sha", "c" * 40, "--out", "o", "--rehearsal")
            self.assertEqual(code, 0, text)
            self.assertIn("native qualification passed", text)
            self.assertIn("rehearsal", text)
            code, text = run_main("--collect-hosted", *ctx, "--native-record", "n.json",
                                  "--out", "p")
            self.assertEqual(code, 0, text)
            self.assertIn("exact hosted artifacts collected", text)
            code, text = run_main("--verify-evidence", *ctx, "--native-record", "n.json",
                                  "--hosted-record", "h.json", "--rehearsal")
            self.assertEqual(code, 0, text)
            self.assertIn("complete native, ASan, GCC13, and both hosted platform records verified",
                          text)
            self.assertIn("rehearsal and not final evidence", text)
            code, text = run_main("--verify-evidence", *ctx, "--native-record", "n.json",
                                  "--hosted-record", "h.json")
            self.assertEqual(code, 0, text)
            self.assertNotIn("not final evidence", text)
        self.assertEqual([call[0] for call in calls], ["native", "collect", "verify", "verify"])
        self.assertEqual(calls[0], ("native", "c" * 40, "o", True))
        self.assertEqual(calls[1][-1], False)
        self.assertEqual(calls[2][-1], True)
        self.assertEqual(calls[3][-1], False)
        # Missing context is a usage error, never a success.
        for label, arguments, needle in (
                ("native without out", ["--native", "--sha", "c" * 40], "--native requires"),
                ("collect without native record",
                 ["--collect-hosted", *ctx, "--out", "p"], "--collect-hosted requires"),
                ("verify without hosted record",
                 ["--verify-evidence", *ctx, "--native-record", "n.json"],
                 "--verify-evidence requires"),
                ("verify without context", ["--verify-evidence"], "--verify-evidence requires")):
            code, text = run_main(*arguments)
            self.assertEqual(code, 2, label)
            self.assertIn(needle, text, label)
        # With real missing records the final mode refuses with its own message.
        result = self._run_qualifier("--verify-evidence", *ctx, "--native-record",
                                     str(ROOT / "missing-native.json"), "--hosted-record",
                                     str(ROOT / "missing-hosted.json"))
        self.assertEqual(result.returncode, 1)
        self.assertIn("hosted record is outside the private qualification tree", result.stderr)


    def _inventory(self) -> dict:
        return json.loads(INVENTORY.read_text(encoding="utf-8"))

    def _write(self, directory: Path, value: object) -> Path:
        path = directory / "inventory.json"
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def test_zero_case_inventory_refused(self) -> None:
        """An inventory with no suites, no cases or no binaries is refused."""
        summary = qualify.validate_case_inventory(INVENTORY)
        # Later plans add suites as data rows; the summary must count exactly
        # the suites of the committed inventory and keep the two original ones.
        suites = self._inventory()["suites"]
        self.assertEqual(summary["suites"], len(suites))
        self.assertTrue({"identity", "mechanism"} <= set(suites))
        self.assertGreater(summary["cases"], 0)
        with tempfile.TemporaryDirectory(prefix="t64-p09-inv.") as tmp:
            directory = Path(tmp)
            base = self._inventory()
            zero_suites = dict(base, suites={})
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.validate_case_inventory(self._write(directory, zero_suites))
            self.assertIn("zero suites", str(caught.exception))
            no_binaries = json.loads(json.dumps(base))
            no_binaries["suites"]["identity"]["binaries"] = []
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.validate_case_inventory(self._write(directory, no_binaries))
            self.assertIn("binaries", str(caught.exception))
            all_empty = json.loads(json.dumps(base))
            for suite in all_empty["suites"].values():
                for binary in suite["binaries"]:
                    binary["cases"] = []
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.validate_case_inventory(self._write(directory, all_empty))
            self.assertIn("cases", str(caught.exception))
            # The committed inventory is still accepted in the same setup.
            self.assertEqual(qualify.validate_case_inventory(INVENTORY), summary)

    def test_inventory_shape_refused(self) -> None:
        """Each malformed shape is refused with its own message."""
        self.assertGreater(qualify.validate_case_inventory(INVENTORY)["cases"], 0)

        def mutate(change) -> dict:
            value = json.loads(json.dumps(self._inventory()))
            change(value)
            return value

        cases = (
            ("wrong schema id", lambda v: v.__setitem__("schema", "t64.audio-case-inventory"),
             "schema"),
            ("wrong version", lambda v: v.__setitem__("schema_version", 2), "schema"),
            ("empty ctest list", lambda v: v["suites"]["identity"].__setitem__("ctest", []),
             "ctest"),
            ("empty cases list",
             lambda v: v["suites"]["identity"]["binaries"][0].__setitem__("cases", []),
             "cases"),
            ("duplicate case",
             lambda v: v["suites"]["identity"]["binaries"][0]["cases"].append(
                 v["suites"]["identity"]["binaries"][0]["cases"][0]),
             "duplicate"),
            ("duplicate ctest name",
             lambda v: v["suites"]["identity"]["ctest"].append(
                 v["suites"]["identity"]["ctest"][0]),
             "duplicate"),
            ("unsafe binary path",
             lambda v: v["suites"]["identity"]["binaries"][0].__setitem__(
                 "path", "../core/test_si_profile"),
             "path"),
            ("absolute binary path",
             lambda v: v["suites"]["identity"]["binaries"][0].__setitem__(
                 "path", "/core/test_si_profile"),
             "path"),
            ("suite is not an object", lambda v: v["suites"].__setitem__("identity", []),
             "suite"),
            ("non-string case",
             lambda v: v["suites"]["identity"]["binaries"][0]["cases"].append(7),
             "cases"),
        )
        with tempfile.TemporaryDirectory(prefix="t64-p09-inv.") as tmp:
            directory = Path(tmp)
            for label, change, needle in cases:
                path = self._write(directory, mutate(change))
                with self.assertRaises(qualify.Refusal, msg=label) as caught:
                    qualify.validate_case_inventory(path)
                self.assertIn(needle, str(caught.exception), label)
            # Not JSON at all is also a refusal, never an exception of another type.
            bad = directory / "inventory.json"
            bad.write_text("{not json", encoding="utf-8")
            with self.assertRaises(qualify.Refusal):
                qualify.validate_case_inventory(bad)

    # ---- public member allowlist -------------------------------------------

    MANIFEST = json.dumps({"schema": "t64.phase09-qualification",
                           "schema_version": 1}).encode("utf-8")

    def _public_dir(self, root: Path, name: str, manifest: bytes | None = None) -> Path:
        directory = root / name
        directory.mkdir()
        (directory / "manifest.json").write_bytes(
            self.MANIFEST if manifest is None else manifest)
        return directory

    def test_public_members_exact_allowlist(self) -> None:
        """Exactly one regular manifest.json is accepted; anything else is not."""
        with tempfile.TemporaryDirectory(prefix="t64-p09-pub.") as tmp:
            root = Path(tmp)
            accepted = self._public_dir(root, "accepted")
            value = qualify.scan_public_members(accepted)
            self.assertEqual(value["schema"], "t64.phase09-qualification")
            extra = self._public_dir(root, "extra")
            (extra / "notes.txt").write_text("x", encoding="utf-8")
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(extra)
            self.assertIn("unexpected member", str(caught.exception))
            missing = root / "missing"
            missing.mkdir()
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(missing)
            self.assertIn("no manifest.json", str(caught.exception))
            renamed = root / "renamed"
            renamed.mkdir()
            (renamed / "other.json").write_bytes(self.MANIFEST)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(renamed)
            self.assertIn("unexpected member", str(caught.exception))
            absent = root / "absent"
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(absent)
            self.assertIn("absent or unsafe", str(caught.exception))
            # The baseline is still accepted after the refusals.
            self.assertEqual(qualify.scan_public_members(accepted), value)

    def test_public_member_private_suffix_refused(self) -> None:
        with tempfile.TemporaryDirectory(prefix="t64-p09-pub.") as tmp:
            root = Path(tmp)
            baseline = self._public_dir(root, "baseline")
            self.assertEqual(qualify.scan_public_members(baseline)["schema_version"], 1)
            for index, name in enumerate(("x.sta", "y.rpl", "z.sav", "e.eep", "s.sra",
                                          "r.z64", "UPPER.STA")):
                directory = self._public_dir(root, f"case{index}")
                (directory / name).write_bytes(b"private")
                with self.assertRaises(qualify.Refusal, msg=name) as caught:
                    qualify.scan_public_members(directory)
                self.assertIn("private-suffix member", str(caught.exception), name)

    def test_public_member_state_magic_refused(self) -> None:
        with tempfile.TemporaryDirectory(prefix="t64-p09-pub.") as tmp:
            root = Path(tmp)
            baseline = self._public_dir(root, "baseline")
            self.assertEqual(qualify.scan_public_members(baseline)["schema_version"], 1)
            magic = self._public_dir(root, "magic", b"T64STATE" + self.MANIFEST)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(magic)
            self.assertIn("state container magic", str(caught.exception))
            # Local paths and personal identifiers are built at run time so no
            # literal of either shape is committed in this file.
            home = "/" + "Users" + "/" + "someone" + "/run"
            path_manifest = json.dumps({"schema": "t64.phase09-qualification",
                                        "where": home}).encode("utf-8")
            pathdir = self._public_dir(root, "path", path_manifest)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(pathdir)
            self.assertIn("local path", str(caught.exception))
            address = "someone" + "@" + "example" + "." + "org"
            email_manifest = json.dumps({"schema": "t64.phase09-qualification",
                                         "who": address}).encode("utf-8")
            emaildir = self._public_dir(root, "email", email_manifest)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(emaildir)
            self.assertIn("personal identifier", str(caught.exception))
            oversize = self._public_dir(root, "oversize", b"{" + b" " * (17 * 1024) + b"}")
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(oversize)
            self.assertIn("bounded", str(caught.exception))

    def test_public_member_symlink_or_nested_refused(self) -> None:
        with tempfile.TemporaryDirectory(prefix="t64-p09-pub.") as tmp:
            root = Path(tmp)
            baseline = self._public_dir(root, "baseline")
            self.assertEqual(qualify.scan_public_members(baseline)["schema_version"], 1)
            nested = self._public_dir(root, "nested")
            (nested / "sub").mkdir()
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(nested)
            self.assertIn("nested directory", str(caught.exception))
            linked = root / "linked"
            linked.mkdir()
            target = root / "elsewhere.json"
            target.write_bytes(self.MANIFEST)
            (linked / "manifest.json").symlink_to(target)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(linked)
            self.assertIn("symlink", str(caught.exception))
            link_dir = root / "link-dir"
            link_dir.symlink_to(baseline)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(link_dir)
            self.assertIn("absent or unsafe", str(caught.exception))

    # ---- bound sources and names -------------------------------------------

    def test_source_paths_exist_and_cover_guards(self) -> None:
        paths = qualify.SOURCE_PATHS
        self.assertEqual(len(paths), len(set(paths)), "duplicate source path")
        for guard in ("scripts/qualify-rdp-vi.py", "scripts/collect-boot-evidence.sh",
                      "scripts/qualify-audio.py", "scripts/check-audio-cases.py",
                      "scripts/build-boot-fixtures.sh", "scripts/build-pad-fixture.sh",
                      "scripts/build-phase09-admit-fixture.sh",
                      "scripts/check-rsp-no-hle.py", "scripts/check-test-inventory.sh",
                      "scripts/check-ci-yml.sh", "scripts/rights-scan.sh",
                      "scripts/pii-scan.sh", "scripts/check-provenance.sh",
                      "scripts/check-claims.sh", "scripts/gate.sh",
                      "scripts/check-phase09-cases.py", "scripts/qualify-phase09.py",
                      "scripts/tests/test_phase09_qualification.py",
                      "scripts/list-membership-sites.py",
                      "docs/spec/profile-membership-sites.txt",
                      "docs/spec/phase09-cases.json", ".github/workflows/ci.yml",
                      "core", "runner", "tools/rights", "tools/manifest", "fixtures"):
            self.assertIn(guard, paths)
        in_git = subprocess.run(["git", "rev-parse", "--is-inside-work-tree"], cwd=ROOT,
                                capture_output=True, text=True, check=False)
        for relative in paths:
            self.assertFalse(Path(relative).is_absolute() or ".." in Path(relative).parts,
                             relative)
            self.assertTrue((ROOT / relative).exists(), f"{relative} is missing")
            if in_git.returncode == 0:
                tracked = subprocess.run(["git", "ls-files", "--", relative], cwd=ROOT,
                                         capture_output=True, text=True, check=False)
                self.assertTrue(tracked.stdout.strip(), f"{relative} is not tracked in git")

    def test_no_undefined_global_names(self) -> None:
        for relative in ("scripts/qualify-phase09.py", "scripts/check-phase09-cases.py",
                         "scripts/tests/test_phase09_qualification.py"):
            self.assertEqual(_undefined_global_names(ROOT / relative), {},
                             f"{relative} loads names it never defines")

    def test_case_checker_selftest_and_mismatch(self) -> None:
        Phase09CaseChecker().check(self)

    def test_claim_skeleton_has_no_real_block(self) -> None:
        """The claim file is a skeleton: example fences only, no real block."""
        path = ROOT / "docs/claims/phase09.md"
        lines = path.read_text(encoding="utf-8").splitlines()
        self.assertNotIn("```t64-claim", lines)
        self.assertGreaterEqual(lines.count("```t64-claim-example"), 6)
        checked = subprocess.run(
            ["sh", str(ROOT / "scripts/check-claims.sh"), "--files",
             str(path.relative_to(ROOT))],
            capture_output=True, text=True, check=False, cwd=ROOT)
        self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)


def _run_checker(build: Path, inventory: Path, suite: str) -> subprocess.CompletedProcess[str]:
    env = dict(os.environ)
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    return subprocess.run(
        [sys.executable, "-B", str(ROOT / "scripts/check-phase09-cases.py"),
         "--build", str(build), "--suite", suite, "--inventory", str(inventory)],
        capture_output=True, text=True, check=False, env=env)


class Phase09CaseChecker:
    """The inventory checker, driven against a tiny generated build directory."""

    SUITE = "stubsuite"
    CASES = ["alpha", "beta", "gamma"]

    def _write_stub(self, path: Path, cases: list[str]) -> None:
        body = "#!/bin/sh\n" + "".join(
            f'[ "$1" = --list-cases ] && echo {name}\n' for name in cases) + "exit 0\n"
        path.write_text(body, encoding="utf-8")
        path.chmod(0o755)

    def _write_inventory(self, path: Path, cases: list[str]) -> None:
        path.write_text(json.dumps({
            "schema": "t64.phase09-case-inventory",
            "schema_version": 1,
            "suites": {self.SUITE: {
                "ctest": ["stub.one"],
                "binaries": [{"path": "core/test_stub", "cases": cases}],
            }},
        }, indent=2), encoding="utf-8")

    def check(self, tc: unittest.TestCase) -> None:
        checker = ROOT / "scripts/check-phase09-cases.py"
        env = dict(os.environ)
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        selftest = subprocess.run(
            [sys.executable, "-B", str(checker), "--selftest"],
            capture_output=True, text=True, check=False, env=env)
        tc.assertEqual(selftest.returncode, 0, selftest.stderr)
        with tempfile.TemporaryDirectory(prefix="t64-p09-chk.") as tmp:
            root = Path(tmp)
            source = root / "src"
            build = root / "build"
            (source).mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.25)\nproject(stubs NONE)\n"
                "enable_testing()\nadd_test(NAME stub.one COMMAND true)\n",
                encoding="utf-8")
            configured = subprocess.run(
                ["cmake", "-S", str(source), "-B", str(build)],
                capture_output=True, text=True, check=False)
            tc.assertEqual(configured.returncode, 0, configured.stderr)
            (build / "core").mkdir()
            stub = build / "core/test_stub"
            inventory = root / "inventory.json"

            def attempt(stub_cases: list[str], inventory_cases: list[str]):
                self._write_stub(stub, stub_cases)
                self._write_inventory(inventory, inventory_cases)
                return _run_checker(build, inventory, self.SUITE)

            # Accepted baseline in the same setup.
            baseline = attempt(self.CASES, self.CASES)
            tc.assertEqual(baseline.returncode, 0, baseline.stdout + baseline.stderr)
            tc.assertIn("validated nonempty stubsuite", baseline.stdout)
            prefix = "Phase09 case inventory error"
            # One case added to the inventory: the stub no longer matches.
            added = attempt(self.CASES, self.CASES + ["delta"])
            tc.assertEqual(added.returncode, 1)
            tc.assertIn(prefix, added.stderr)
            tc.assertIn("source-derived cases differ from inventory", added.stderr)
            tc.assertIn("'delta'", added.stderr)
            # One case removed from the stub.
            removed = attempt(self.CASES[:-1], self.CASES)
            tc.assertEqual(removed.returncode, 1)
            tc.assertIn("source-derived cases differ from inventory", removed.stderr)
            tc.assertIn("got ['alpha', 'beta']", removed.stderr)
            # Two cases swapped: the order is part of the contract.
            swapped = attempt(["beta", "alpha", "gamma"], self.CASES)
            tc.assertEqual(swapped.returncode, 1)
            tc.assertIn("source-derived cases differ from inventory", swapped.stderr)
            tc.assertIn("got ['beta', 'alpha', 'gamma']", swapped.stderr)
            # A duplicate in the stub's own list.
            duplicate = attempt(["alpha", "beta", "beta"], ["alpha", "beta", "gamma"])
            tc.assertEqual(duplicate.returncode, 1)
            tc.assertIn("case list is empty or duplicated", duplicate.stderr)
            # A wrong schema id and a missing suite are refused with their own text.
            self._write_stub(stub, self.CASES)
            wrong = json.loads(inventory.read_text(encoding="utf-8"))
            self._write_inventory(inventory, self.CASES)
            wrong = json.loads(inventory.read_text(encoding="utf-8"))
            wrong["schema"] = "t64.audio-case-inventory"
            inventory.write_text(json.dumps(wrong), encoding="utf-8")
            schema = _run_checker(build, inventory, self.SUITE)
            tc.assertEqual(schema.returncode, 1)
            tc.assertIn("unsupported phase09 case inventory schema", schema.stderr)
            self._write_inventory(inventory, self.CASES)
            unknown = _run_checker(build, inventory, "no-such-suite")
            tc.assertEqual(unknown.returncode, 1)
            tc.assertIn("unknown suite: no-such-suite", unknown.stderr)
            # The accepted baseline still passes after the refusals.
            again = attempt(self.CASES, self.CASES)
            tc.assertEqual(again.returncode, 0, again.stdout + again.stderr)


def _layout_fixture_main(argv: list[str]) -> int:
    """Write the synthetic phase09 sibling of an archive-layout control."""
    import argparse

    parser = argparse.ArgumentParser(description="write a synthetic archive-layout fixture")
    parser.add_argument("--job", choices=("gate-linux", "gate-macos"), required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--run", required=True)
    parser.add_argument("--attempt", required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    platform = "linux" if args.job == "gate-linux" else "macos"
    value = qualify._projection(
        _synthetic_summary(platform, source_revision=args.sha), job=args.job,
        workflow_sha=args.sha, run_id=args.run, attempt=args.attempt)
    qualify.public_projection(value, {"job": args.job, "sha": args.sha, "run": args.run,
                                      "attempt": args.attempt})
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / "manifest.json").write_text(
        json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")
    print("phase09 qualification: wrote test-only synthetic archive-layout fixture")
    return 0


def main() -> int:
    args = sys.argv[1:]
    if args and args[0] == "--layout-fixture":
        return _layout_fixture_main(args[1:])
    if args == ["--list-cases"]:
        print("\n".join(CASE_NAMES))
        return 0
    if args == ["--selftest"]:
        for name in CASE_NAMES:
            print(f"case reached: {name}")
        sys.argv = [sys.argv[0], "-v"]
        unittest.main(verbosity=2)
        return 0
    selected: tuple[str, ...] | None = None
    if len(args) == 2 and args[0] == "--case" and args[1] in CASE_NAMES:
        selected = (args[1],)
    elif len(args) == 2 and args[0] == "--suite" and args[1] in SUITES:
        selected = SUITES[args[1]]
    if selected is not None:
        suite = unittest.TestSuite(
            [Phase09Qualification("test_" + name.replace("-", "_")) for name in selected])
        for name in selected:
            print(f"case reached: {name}")
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        return 0 if result.wasSuccessful() else 1
    print("usage: test_phase09_qualification.py "
          "--list-cases | --selftest | --case NAME | --suite NAME",
          file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
