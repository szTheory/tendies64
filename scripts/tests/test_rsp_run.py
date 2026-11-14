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
PRIVATE = ROOT / "build/private-rsp-diagnostic"
RUNNER = ROOT / "scripts/run-rsp-diagnostic.py"


def load_runner():
    spec = importlib.util.spec_from_file_location("rsp_run_controls", RUNNER)
    if spec is None or spec.loader is None:
        raise AssertionError("RSP run wrapper could not be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def owner_only_private_root() -> None:
    PRIVATE.mkdir(mode=0o700, parents=True, exist_ok=True)
    if PRIVATE.is_symlink() or not PRIVATE.is_dir():
        raise AssertionError("private RSP test root is linked or not a directory")
    PRIVATE.chmod(0o700)
    if stat.S_IMODE(PRIVATE.stat().st_mode) != 0o700:
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
        self.temporary = tempfile.TemporaryDirectory(prefix="rsp-run-control.", dir=PRIVATE)
        self.workspace = pathlib.Path(self.temporary.name)
        self.workspace.chmod(0o700)
        self.module = load_runner()

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
            "os.write(1, b'x' * 5000)\n",
        )
        command = json.loads(command_path.read_text(encoding="utf-8"))
        self.assertTrue(command["output_overflow"])
        self.assertLessEqual(command["capture_bytes"], 4096)
        self.assertEqual(command["classification"], "harness-error")

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
