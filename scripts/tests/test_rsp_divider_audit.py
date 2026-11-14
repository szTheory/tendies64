#!/usr/bin/env python3
"""Authored, clean-workspace controls for the RSP divider source audit."""

from __future__ import annotations

import importlib.util
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True

ROOT = pathlib.Path(__file__).resolve().parents[2]
AUDIT = ROOT / "scripts/audit-rsp-divider.py"
TEST_ROOT = ROOT / "build/test-rsp-divider-audit"


def clean_environment() -> dict[str, str]:
    return {
        "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
        "LC_ALL": "C",
        "LANG": "C",
        "PYTHONDONTWRITEBYTECODE": "1",
    }


def audit_module():
    spec = importlib.util.spec_from_file_location("rsp_divider_audit", AUDIT)
    if spec is None or spec.loader is None:
        raise AssertionError("could not load divider audit module")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class DividerAuditTests(unittest.TestCase):
    def setUp(self) -> None:
        TEST_ROOT.mkdir(mode=0o700, parents=True, exist_ok=True)
        if TEST_ROOT.is_symlink() or not TEST_ROOT.is_dir():
            self.fail("synthetic fixture root is linked or not a directory")
        TEST_ROOT.chmod(0o700)
        self.temporary = tempfile.TemporaryDirectory(prefix="test.", dir=TEST_ROOT)
        self.workspace = pathlib.Path(self.temporary.name)
        self.workspace.chmod(0o700)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_cli_selftest_uses_authored_synthetic_controls(self) -> None:
        result = subprocess.run(
            [sys.executable, "-B", str(AUDIT), "--selftest"],
            cwd=ROOT,
            env=clean_environment(),
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("PASS: authored synthetic divider source, notice, and history controls", result.stdout)
        self.assertNotIn("build/research", result.stdout)

    def test_fixture_accepts_bound_inputs_and_rejects_changed_source(self) -> None:
        module = audit_module()
        candidate = self.workspace / "candidate"
        candidate.mkdir(mode=0o700)
        contract = module.make_synthetic_fixture(candidate)
        self.assertEqual(module.verify_candidate(candidate, contract), [])

        (candidate / "rsp.cpp").write_bytes(b"unreviewed replacement\n")
        findings = module.verify_candidate(candidate, contract)
        self.assertIn("saved source bytes differ for synthetic/rsp.cpp", findings)

    def test_fixture_rejects_changed_notice_and_unbounded_history(self) -> None:
        module = audit_module()
        candidate = self.workspace / "candidate"
        candidate.mkdir(mode=0o700)
        contract = module.make_synthetic_fixture(candidate)
        (candidate / "ISC-NOTICE.txt").write_bytes(b"foreign notice\n")
        self.assertIn(
            "saved root ISC notice differs from the accepted excerpt",
            module.verify_candidate(candidate, contract),
        )

        (candidate / "ISC-NOTICE.txt").write_text(contract["notice_text"], encoding="utf-8")
        history_path = candidate / "selected-file-history.json"
        history = module.read_json(history_path)
        history["as_of_revision"] = "c" * 40
        history_path.write_text(
            json.dumps(history, sort_keys=True, indent=2) + "\n",
            encoding="utf-8",
        )
        contract["history_sha256"] = module.sha256(history_path.read_bytes())
        self.assertIn(
            "history is not bounded at the pinned revision",
            module.verify_candidate(candidate, contract),
        )


if __name__ == "__main__":
    unittest.main()
