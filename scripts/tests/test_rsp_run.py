#!/usr/bin/env python3
"""Authored controls for the local-only bounded RSP run wrapper."""

from __future__ import annotations

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
TEST_ROOT = ROOT / "build/test-rsp-run"
RUNNER = ROOT / "scripts/run-rsp-diagnostic.py"


def load_runner():
    spec = importlib.util.spec_from_file_location("rsp_run_controls", RUNNER)
    if spec is None or spec.loader is None:
        raise AssertionError("RSP run wrapper could not be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def owner_only_private_root() -> None:
    TEST_ROOT.mkdir(mode=0o700, parents=True, exist_ok=True)
    if TEST_ROOT.is_symlink() or not TEST_ROOT.is_dir():
        raise AssertionError("private RSP test root is linked or not a directory")
    TEST_ROOT.chmod(0o700)
    if stat.S_IMODE(TEST_ROOT.stat().st_mode) != 0o700:
        raise AssertionError("private RSP test root must be owner-only")


def fake_adapter(path: pathlib.Path, body: str) -> pathlib.Path:
    path.write_text("#!/usr/bin/env python3\n" + body, encoding="utf-8")
    path.chmod(0o700)
    return path


def sample_member() -> dict[str, object]:
    return {
        "registration_index": 7,
        "value_index": 0,
        "selector": "r0007v00000",
        "selected_rom_sha256": "a" * 64,
    }


class RspRunControls(unittest.TestCase):
    def setUp(self) -> None:
        owner_only_private_root()
        self.temporary = tempfile.TemporaryDirectory(prefix="rsp-run-control.", dir=TEST_ROOT)
        self.workspace = pathlib.Path(self.temporary.name)
        self.workspace.chmod(0o700)
        self.module = load_runner()
        self.module.PRIVATE = self.workspace

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def run_guest(self, script: str, *, outer_seconds: float = 12.0) -> pathlib.Path:
        batch_directory = self.workspace / "batches"
        batch_directory.mkdir(mode=0o700, exist_ok=True)
        run_directory = self.workspace
        adapter = fake_adapter(self.workspace / "adapter.py", script)
        member = sample_member()
        batch_path = batch_directory / f"{member['selector']}.json"
        with patch.object(self.module, "OUTER_SECONDS", outer_seconds):
            with self.assertRaises(self.module.Refusal):
                self.module.run_guest(
                    adapter,
                    self.workspace / "inventory.json",
                    self.workspace / "member.z64",
                    4096,
                    member,
                    batch_path,
                    run_directory,
                )
        return run_directory / f"attempt-{member['selector']}.command.json"

    def test_deadline_stays_failure_when_leader_exits_zero_and_child_holds_pipes(self) -> None:
        command_path = self.run_guest(
            "import subprocess,sys\n"
            "subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(5)'])\n",
            outer_seconds=0.25,
        )
        command = json.loads(command_path.read_text(encoding="utf-8"))
        self.assertEqual(command["adapter_exit"], 0)
        self.assertTrue(command["outer_deadline_expired"])
        self.assertIsInstance(command["cleanup_group_empty"], bool)
        self.assertIsInstance(command["cleanup_signal_denied"], bool)
        self.assertEqual(command["classification"], "timeout")

    def test_output_overflow_is_retained_as_harness_failure(self) -> None:
        command_path = self.run_guest(
            "import os\n"
            "os.write(1, b'classification=pass\\n' + b'x' * 5000)\n",
        )
        command = json.loads(command_path.read_text(encoding="utf-8"))
        self.assertTrue(command["output_overflow"])
        self.assertLessEqual(command["capture_bytes"], 4096)
        self.assertEqual(command["classification"], "harness-error")

    def test_closed_pipes_do_not_end_the_outer_deadline(self) -> None:
        command_path = self.run_guest(
            "import os,pathlib,sys,time\n"
            "pathlib.Path(sys.argv[-1]).write_text('{}')\n"
            "os.write(1, b'classification=pass\\n')\n"
            "os.close(1); os.close(2); time.sleep(0.6)\n",
            outer_seconds=0.1,
        )
        command = json.loads(command_path.read_text())
        self.assertTrue(command["outer_deadline_expired"])
        self.assertEqual(command["classification"], "timeout")
        self.assertIsNone(command["batch_path"])
        self.assertIsNotNone(command["rejected_batch_path"])
        self.assertFalse(any((self.workspace / "batches").iterdir()))

    def test_existing_run_verification_does_not_recreate_inputs(self) -> None:
        run_dir = self.workspace / "saved"
        run_dir.mkdir(mode=0o700)
        (run_dir / "batches").mkdir(mode=0o700)
        inventory = run_dir / "rsp-inventory.json"
        inventory.write_bytes(b"authored immutable inventory")
        inventory.chmod(0o600)
        original = run_dir / "runtime-identity.stdout.json"
        original.write_bytes(b"authored historical identity")
        original.chmod(0o600)
        adapter = self.workspace / "adapter"
        with patch.dict(os.environ, {}, clear=True), \
                patch.object(self.module, "adapter_path", return_value=adapter), \
                patch.object(self.module, "verify_before_adapter_use", return_value=(b"", b"")), \
                patch.object(self.module, "invoke_adapter", return_value=(3, b"incomplete", b"")) as invoke:
            self.assertEqual(self.module.run_mode("verify-complete", adapter, run_dir), 3)
            self.assertEqual(self.module.run_mode("verify-complete", adapter, run_dir), 3)
        self.assertEqual(invoke.call_count, 2)
        self.assertTrue(all(call.args[1] == "--verify-complete-json" for call in invoke.call_args_list))
        self.assertEqual(inventory.read_bytes(), b"authored immutable inventory")
        self.assertEqual(original.read_bytes(), b"authored historical identity")

    def test_successful_bounded_adapter_receipt_is_retained(self) -> None:
        batch_dir = self.workspace / "batches"
        batch_dir.mkdir(mode=0o700)
        member = sample_member()
        adapter = fake_adapter(self.workspace / "adapter.py",
            "import pathlib,sys\n"
            "pathlib.Path(sys.argv[-1]).write_text('{}')\n"
            "print('classification=refused')\n"
            "raise SystemExit(2)\n")
        receipt = self.module.run_guest(adapter, self.workspace / "inventory.json",
            self.workspace / "member.z64", 4096, member,
            batch_dir / f"{member['selector']}.json", self.workspace)
        self.assertEqual(receipt["classification"], "refused")
        self.assertTrue(receipt["cleanup_group_empty"])
        self.assertFalse(receipt["outer_deadline_expired"])
        self.assertIsNotNone(receipt["batch_path"])

    def test_bad_cleanup_cannot_be_promoted_by_classification_text(self) -> None:
        real_killpg = os.killpg
        def unresolved_group(pid, sig):
            if sig == 0:
                return None  # A process group remains after cleanup.
            return real_killpg(pid, sig)
        with patch.object(self.module.os, "killpg", side_effect=unresolved_group):
            command_path = self.run_guest(
                "import pathlib,sys\n"
                "pathlib.Path(sys.argv[-1]).write_text('{}')\n"
                "print('classification=pass')\n",
            )
        command = json.loads(command_path.read_text())
        self.assertFalse(command["cleanup_group_empty"])
        self.assertEqual(command["classification"], "harness-error")

    def test_unverified_adapter_is_not_invoked_for_identity(self) -> None:
        adapter = self.workspace / "unverified-adapter"
        with patch.dict(os.environ, {}, clear=True), \
                patch.object(self.module, "adapter_path", return_value=adapter), \
                patch.object(self.module, "run_capture", return_value=(1, b"", b"refused")) as capture:
            with self.assertRaises(self.module.Refusal):
                self.module.run_mode("run-one", adapter, self.workspace / "attempt")
        self.assertTrue(capture.call_args_list)
        self.assertEqual(capture.call_args_list[0].args[0][0], sys.executable)
        self.assertFalse(any(call.args[0][0] == str(adapter) for call in capture.call_args_list))

    def test_guest_execution_refuses_fixture_mode_before_adapter_use(self) -> None:
        with patch.dict(os.environ, {"CI": "true", "T64_REQUIRE_FIXTURES": "1"}):
            with self.assertRaisesRegex(self.module.Refusal, "local-only"):
                self.module.run_mode(
                    "run-one",
                    self.workspace / "missing-adapter",
                    self.workspace / "missing-run-dir",
                )
        self.assertFalse((self.workspace / "missing-run-dir").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
