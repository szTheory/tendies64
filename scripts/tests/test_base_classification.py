#!/usr/bin/env python3
"""Fast authored controls through the real BASE classification executable."""
from __future__ import annotations

import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
ADAPTER = Path(os.environ.get("T64_BASE_TEST_ADAPTER",
                              ROOT / "build/gate/runner/t64-base-diagnostic"))
PRIVATE = ROOT / "build/private-base-diagnostic"


def canonical(value):
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def digest(data):
    return hashlib.sha256(data).hexdigest()


class ClassificationControls(unittest.TestCase):
    def setUp(self):
        PRIVATE.mkdir(mode=0o700, parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="authored-classification-", dir=PRIVATE)
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        self.serial = 0
        self.context = "0" * 64
        self.identity = json.loads(subprocess.check_output(
            [str(ADAPTER), "--runtime-identity-json"], cwd=ROOT, timeout=5))
        member = {"execution": "once", "group": "authored", "index": 0,
                  "lane": "required-base", "level": "BasicFunctionality",
                  "name": "authored selector", "reason": "", "required": True,
                  "value_id": "", "value_index": 0}
        self.authority = {"members": [member], "schema": "t64.base-source-authority",
                          "schema_version": 1, "source_revision": "0" * 40}
        authority_bytes = canonical(self.authority)
        self.anchor = digest(authority_bytes)
        self.inventory = {**self.authority, "schema": "t64.base-inventory",
                          "source_authority_sha256": self.anchor}
        self.write("authority.json", authority_bytes)
        self.write("inventory.json", canonical(self.inventory))

    def write(self, name, data):
        path = self.path / name
        path.write_bytes(data)
        path.chmod(0o600)
        return str(path)

    def row(self, outcome="unexecuted", reason="not-selected", image=""):
        self.serial += 1
        return {"expected_rom_sha256": image,
                "original": str(self.path / f"capture-{self.serial}"),
                "outcome": outcome, "reason": reason,
                "record": str(self.path / f"record-{self.serial}.json"),
                "registration_index": 0, "value_index": 0}

    def classify(self, rows, *, context=None, env=None):
        self.serial += 1
        manifest = self.write(f"input-{self.serial}.json", canonical({
            "members": rows, "schema": "t64.base-classification-input", "schema_version": 1}))
        report = self.path / f"report-{self.serial}.json"
        authored_env = {key: value for key, value in os.environ.items()
                        if key not in ("CI", "T64_REQUIRE_FIXTURES")}
        result = subprocess.run([str(ADAPTER), "--classify-manifest",
            str(self.path / "authority.json"), self.anchor,
            str(self.path / "inventory.json"), context or self.context,
            manifest, str(report)], cwd=ROOT, capture_output=True, timeout=5,
            env=authored_env if env is None else env)
        return result, json.loads(report.read_bytes()) if report.exists() else None

    def test_complete_pass_fail_and_terminal_controls(self):
        """Authored records exercise classification, without claiming guest use."""
        for outcome in ("pass", "fail"):
            capture = canonical({"records": [
                {"event": "start", "registration_index": 0, "value_index": 0},
                {"event": "outcome", "status": outcome},
                {"complete": True, "event": "done", "record_count": "3"}],
                "schema": "t64.cpu-run", "schema_version": 1})
            record = {"capture_exact": True, "capture_lost": False,
                "capture_total": str(len(capture)), "context_sha256": self.context,
                "core_build_id": self.identity["core_build_identity"],
                "entry_reached": True, "inventory_sha256": digest(canonical(self.inventory)),
                "original_sha256": digest(capture), "outcome": outcome,
                "park_sequence": 4, "profile_id": self.identity["profile_id"],
                "profile_sha256": self.identity["profile_sha256"], "reason": "",
                "registration_index": 0, "schema": "t64.base-member-record",
                "schema_version": 2, "selected_rom_sha256": "1" * 64,
                "terminal_delay_opcode": 0, "terminal_opcode": 0x08000108,
                "terminal_pc": 0x80000420, "tick": "42", "value_index": 0}
            row = self.row("record", "", "1" * 64)
            self.write(Path(row["original"]).name, capture)
            self.write(Path(row["record"]).name, canonical(record))
            result, report = self.classify([row])
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertTrue(report["authenticated_execution_complete"])
            self.assertEqual(report["all_pass"], outcome == "pass")
            for field, value in (("terminal_delay_opcode", 1), ("park_sequence", 3),
                                 ("capture_exact", False), ("capture_total", "0"),
                                 ("selected_rom_sha256", "2" * 64)):
                with self.subTest(outcome=outcome, field=field):
                    changed = {**record, field: value}
                    altered = {**row, "record": self.write(
                        f"{outcome}-{field}.json", canonical(changed))}
                    result, report = self.classify([altered])
                    self.assertEqual(result.returncode, 4)
                    self.assertIsNone(report)

    def test_c_encoder_receipt_roundtrip(self):
        row = self.row()
        result, report = self.classify([row])
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertTrue(report["source_complete"])
        self.assertFalse(report["authenticated_execution_complete"])
        self.assertFalse(report["all_pass"])
        record = json.loads(Path(row["record"]).read_bytes())
        self.assertEqual(record["schema_version"], 2)
        replay = {**row, "outcome": "record", "reason": ""}
        result, second = self.classify([replay])
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(report, second)

    def test_missing_is_visible_and_incomplete(self):
        result, report = self.classify([])
        self.assertEqual(result.returncode, 3)
        self.assertFalse(report["source_complete"])
        self.assertEqual(report["members"][0]["outcome"], "missing")

    def test_duplicate_foreign_and_blocked_as_pass_refused(self):
        for mutation in ("duplicate", "foreign", "false-blocker"):
            with self.subTest(mutation=mutation):
                row = self.row()
                rows = [row]
                if mutation == "duplicate":
                    rows.append(self.row())
                elif mutation == "foreign":
                    row["registration_index"] = 7
                else:
                    row.update(outcome="blocked", reason="invented blocker")
                result, report = self.classify(rows)
                self.assertEqual(result.returncode, 4)
                self.assertIsNone(report)

    def test_target_and_original_mutations_refused(self):
        row = self.row()
        result, _ = self.classify([row])
        self.assertEqual(result.returncode, 0)
        record = json.loads(Path(row["record"]).read_bytes())
        for field, value in (("context_sha256", "f" * 64),
                             ("profile_id", "foreign-profile"),
                             ("profile_sha256", "f" * 64),
                             ("core_build_id", "t64-build-v1-" + "f" * 64),
                             ("schema_version", 1), ("outcome", "pass")):
            with self.subTest(field=field):
                changed = copy.deepcopy(record)
                changed[field] = value
                replay = {**row, "outcome": "record", "reason": "",
                          "record": self.write(f"changed-{field}.json", canonical(changed))}
                result, report = self.classify([replay])
                self.assertEqual(result.returncode, 4)
                self.assertIsNone(report)
        Path(row["original"]).write_bytes(b"forged capture")
        result, report = self.classify([{**row, "outcome": "record", "reason": ""}])
        self.assertEqual(result.returncode, 4)
        self.assertIsNone(report)

    def test_fifo_and_ci_refused_without_blocking(self):
        row = self.row()
        os.mkfifo(row["record"], 0o600)
        self.write(Path(row["original"]).name, b"")
        result, report = self.classify([{**row, "outcome": "record", "reason": ""}])
        self.assertEqual(result.returncode, 4)
        self.assertIsNone(report)
        result, report = self.classify([], env={**os.environ, "CI": "true"})
        self.assertEqual(result.returncode, 4)
        self.assertIsNone(report)


if __name__ == "__main__":
    unittest.main()
