#!/usr/bin/env python3
"""Headless authored controls for the private RSP source qualification."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import pathlib
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[2]
TEST_ROOT = ROOT / "build/test-rsp-qualification"
PREPARE = ROOT / "scripts/prepare-rsp-diagnostic.py"
VERIFY = ROOT / "scripts/verify-rsp-diagnostic-qualification.py"


def owner_only_directory(path: pathlib.Path) -> None:
    path.mkdir(mode=0o700, parents=True, exist_ok=True)
    if path.is_symlink() or not path.is_dir():
        raise AssertionError("RSP test workspace is linked or not a directory")
    path.chmod(0o700)
    if stat.S_IMODE(path.stat().st_mode) != 0o700:
        raise AssertionError("RSP test workspace must be owner-only")


def clean_environment() -> dict[str, str]:
    return {
        "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
        "LC_ALL": "C",
        "LANG": "C",
        "PYTHONDONTWRITEBYTECODE": "1",
    }


def invoke(script: pathlib.Path, *arguments: str, cwd: pathlib.Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, "-B", str(script), *arguments],
        cwd=cwd,
        env=clean_environment(),
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def canonical(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


class RspQualificationTests(unittest.TestCase):
    def setUp(self) -> None:
        owner_only_directory(TEST_ROOT)
        self.temporary = tempfile.TemporaryDirectory(prefix="run.", dir=TEST_ROOT)
        self.workspace = pathlib.Path(self.temporary.name)
        self.workspace.chmod(0o700)
        prepared = invoke(PREPARE, "--selftest", "--workspace", str(self.workspace), cwd=ROOT)
        self.assertEqual(
            prepared.returncode,
            0,
            "authored RSP preparation did not pass:\n" + prepared.stdout,
        )
        self.assertIn("PASS: authored RSP membership and closure controls", prepared.stdout)
        verified = invoke(VERIFY, "--selftest", "--workspace", str(self.workspace), cwd=ROOT)
        self.assertEqual(verified.returncode, 0, "authored RSP qualification selftest did not pass:\n" + verified.stdout)
        self.assertIn("PASS: clean authored RSP pre-use qualification selftest", verified.stdout)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_authored_default_rsp_membership_seals_aliases_and_zero_values(self) -> None:
        fixture_path = self.workspace / "fixture.json"
        fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
        rows = fixture["inventory_rows"]
        self.assertEqual(len(rows), 4)
        self.assertEqual([row["index"] for row in rows], [0, 1, 3, 4])
        self.assertEqual(rows[0]["execution"], "once")
        self.assertEqual(rows[0]["values"][0]["id"], "once")
        self.assertEqual(rows[1]["source_name"], rows[0]["source_name"])
        self.assertNotEqual(rows[1]["name"], rows[0]["name"])
        self.assertEqual(rows[1]["execution"], "values")
        self.assertEqual([value["index"] for value in rows[1]["values"]], [0, 1])
        self.assertEqual(rows[3]["lane"], "timing")

    def test_qualifier_rejects_omitted_source_member_after_digest_recompute(self) -> None:
        fixture_path = self.workspace / "fixture.json"
        fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
        fixture["inventory_rows"] = fixture["inventory_rows"][:-1]
        fixture["declared_counts"]["registrations"] = len(fixture["inventory_rows"])
        fixture["declared_counts"]["values"] = sum(
            len(row["values"]) for row in fixture["inventory_rows"]
        )
        fixture["inventory_sha256"] = hashlib.sha256(
            canonical(fixture["inventory_rows"])
        ).hexdigest()
        forged_path = self.workspace / "forged-omission.json"
        forged_path.write_bytes(canonical(fixture))
        forged_path.chmod(0o600)

        result = invoke(VERIFY, "--verify-fixture", str(forged_path), cwd=ROOT)
        self.assertNotEqual(
            result.returncode,
            0,
            "qualification accepted an omitted RSP registration after the inventory digest was recomputed:\n"
            + result.stdout,
        )
        self.assertIn("source registration/value partition is incomplete", result.stdout)

    def test_qualifier_rejects_cross_component_receipt(self) -> None:
        fixture_path = self.workspace / "fixture.json"
        fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
        fixture["component"] = "cpu"
        forged_path = self.workspace / "forged-component.json"
        forged_path.write_bytes(canonical(fixture))
        forged_path.chmod(0o600)
        result = invoke(VERIFY, "--verify-fixture", str(forged_path), cwd=ROOT)
        self.assertNotEqual(result.returncode, 0, "qualification accepted a CPU receipt as an RSP component")
        self.assertIn("not an RSP component receipt", result.stdout)

    def test_qualifier_rejects_duplicate_source_index(self) -> None:
        fixture_path = self.workspace / "fixture.json"
        fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
        fixture["source_rows"].append(dict(fixture["source_rows"][0]))
        fixture["source_sha256"] = hashlib.sha256(canonical(fixture["source_rows"])).hexdigest()
        forged_path = self.workspace / "forged-duplicate.json"
        forged_path.write_bytes(canonical(fixture))
        forged_path.chmod(0o600)
        result = invoke(VERIFY, "--verify-fixture", str(forged_path), cwd=ROOT)
        self.assertNotEqual(result.returncode, 0, "qualification accepted a duplicate source index")
        self.assertIn("source registration partition is malformed", result.stdout)

    def test_qualifier_rejects_unclosed_receipt_fields(self) -> None:
        fixture_path = self.workspace / "fixture.json"
        fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
        fixture["unexpected"] = "unreviewed"
        forged_path = self.workspace / "forged-open-schema.json"
        forged_path.write_bytes(canonical(fixture))
        forged_path.chmod(0o600)
        result = invoke(VERIFY, "--verify-fixture", str(forged_path), cwd=ROOT)
        self.assertNotEqual(result.returncode, 0, "qualification accepted an open receipt schema")
        self.assertIn("fields do not match the closed schema", result.stdout)

    def test_authored_closure_baseline_passes(self) -> None:
        result = invoke(
            VERIFY,
            "--verify-closure-fixture",
            str(self.workspace / "closure-fixture.json"),
            cwd=ROOT,
        )
        self.assertEqual(result.returncode, 0, "authored source/tool/lock/asset closure failed:\n" + result.stdout)
        self.assertIn("PASS: authored source, tool, lock, and asset closure verified", result.stdout)

    def test_authored_closure_rejects_forged_source_audit(self) -> None:
        fixture = json.loads((self.workspace / "closure-fixture.json").read_text(encoding="utf-8"))
        fixture["source"]["audit_rows"][0]["constructor"] = "super::rsp::Forged"
        fixture["source"]["audit_sha256"] = hashlib.sha256(canonical(fixture["source"]["audit_rows"])).hexdigest()
        path = self.workspace / "closure-forged-source.json"
        path.write_bytes(canonical(fixture))
        path.chmod(0o600)
        result = invoke(VERIFY, "--verify-closure-fixture", str(path), cwd=ROOT)
        self.assertNotEqual(result.returncode, 0, "authored closure accepted a source/audit mismatch")
        self.assertIn("differs from its source audit", result.stdout)

    def test_authored_closure_rejects_changed_tool_hash(self) -> None:
        fixture = json.loads((self.workspace / "closure-fixture.json").read_text(encoding="utf-8"))
        fixture["tools"]["files"][0]["sha256"] = "0" * 64
        path = self.workspace / "closure-changed-tool.json"
        path.write_bytes(canonical(fixture))
        path.chmod(0o600)
        result = invoke(VERIFY, "--verify-closure-fixture", str(path), cwd=ROOT)
        self.assertNotEqual(result.returncode, 0, "authored closure accepted changed tool bytes")
        self.assertIn("tool closure digest changed", result.stdout)

    def test_authored_closure_rejects_changed_lock_partition(self) -> None:
        fixture = json.loads((self.workspace / "closure-fixture.json").read_text(encoding="utf-8"))
        fixture["locks"]["files"]["sysroot"] = "0" * 64
        path = self.workspace / "closure-changed-lock.json"
        path.write_bytes(canonical(fixture))
        path.chmod(0o600)
        result = invoke(VERIFY, "--verify-closure-fixture", str(path), cwd=ROOT)
        self.assertNotEqual(result.returncode, 0, "authored closure accepted changed lock partition")
        self.assertIn("lock partition digest changed", result.stdout)

    def test_authored_closure_rejects_changed_asset_identity(self) -> None:
        fixture = json.loads((self.workspace / "closure-fixture.json").read_text(encoding="utf-8"))
        fixture["assets"]["boot_sha256"] = "0" * 64
        path = self.workspace / "closure-changed-asset.json"
        path.write_bytes(canonical(fixture))
        path.chmod(0o600)
        result = invoke(VERIFY, "--verify-closure-fixture", str(path), cwd=ROOT)
        self.assertNotEqual(result.returncode, 0, "authored closure accepted a changed boot asset")
        self.assertIn("asset closure digest changed", result.stdout)


class IdentityReaderOrderTests(unittest.TestCase):
    def test_historical_member_path_is_derived_from_private_receipt(self) -> None:
        spec = importlib.util.spec_from_file_location("rsp_history_path", PREPARE)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        owner_only_directory(TEST_ROOT)
        with tempfile.TemporaryDirectory(prefix="history.", dir=TEST_ROOT) as temporary:
            run_root = pathlib.Path(temporary)
            expected = run_root / "attempt-r0007v00000.command.json"
            receipt = {"selector": "r0007v00000",
                       "attempt": {"path": expected.relative_to(ROOT).as_posix()}}
            self.assertEqual(module.historical_attempt_path(run_root, receipt), expected)
            receipt["attempt"]["path"] = "elsewhere/attempt-r0008v00000.command.json"
            with self.assertRaisesRegex(module.Refusal, "differs"):
                module.historical_attempt_path(run_root, receipt)

    def test_unbound_replacement_is_refused_before_it_can_run(self) -> None:
        spec = importlib.util.spec_from_file_location("rsp_identity_order", VERIFY)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        owner_only_directory(TEST_ROOT)
        with tempfile.TemporaryDirectory(prefix="identity.", dir=TEST_ROOT) as temporary:
            root = pathlib.Path(temporary)
            executable = root / "build/gate/runner/t64-cpu-diagnostic"
            executable.parent.mkdir(parents=True)
            executable.write_bytes(b"authored identity reader")
            (root / "build/private-rsp-diagnostic").mkdir(mode=0o700)
            identity = {"core_build_sha256": "b" * 64, "source_revision": "a" * 40}
            expected = {"core_build_sha256": identity["core_build_sha256"],
                        "core_identity_source_revision": identity["source_revision"],
                        "core_identity_executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}
            result = subprocess.CompletedProcess([], 0, json.dumps(identity), "")
            with patch.object(module, "ROOT", root), patch.object(module.subprocess, "run", return_value=result) as run:
                module.verify_current_core_identity(expected, "c" * 64)
                run.assert_called_once()  # Accepted baseline actually queries its bound bytes.
                run.reset_mock()
                executable.write_bytes(b"replacement claiming the same identity")
                with self.assertRaisesRegex(module.Refusal, "missing"):
                    module.verify_current_core_identity(expected, "c" * 64)
                run.assert_not_called()


if __name__ == "__main__":
    unittest.main(verbosity=2)
