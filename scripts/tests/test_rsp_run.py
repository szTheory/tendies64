#!/usr/bin/env python3
"""Authored controls for the local-only bounded RSP run wrapper."""

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

    def test_v5_runtime_binding_is_saved_in_the_current_private_run(self) -> None:
        with tempfile.TemporaryDirectory(prefix="v5-binding.", dir=TEST_ROOT) as temporary:
            root = pathlib.Path(temporary)
            root.chmod(0o700)
            private = root / "build/private-rsp-diagnostic"
            private.mkdir(mode=0o700, parents=True)
            run_dir = private / "run-authored"
            run_dir.mkdir(mode=0o700)
            adapter = root / "build/gate/runner/t64-rsp-diagnostic"
            adapter.parent.mkdir(mode=0o700, parents=True)
            adapter.write_bytes(b"authored RSP adapter bytes")
            adapter.chmod(0o700)
            runtime = {
                "core_build_sha256": "a" * 64,
                "core_source_revision": "b" * 40,
                "profile_descriptor_sha256": "c" * 64,
                "profile_id": "authored-rsp-profile",
            }
            qualification = {"profile_id": "authored-rsp-profile"}
            qualification_bytes = json.dumps(qualification, sort_keys=True).encode()
            (private / "qualification.json").write_bytes(qualification_bytes)
            binding = {
                "authorized_for_future_guest_use": True,
                "component": "rsp",
                "core_model": 6,
                "core_state_schema": 12,
                "guest_execution_performed": False,
                "profile_id": "authored-rsp-profile",
                "qualification_sha256": hashlib.sha256(qualification_bytes).hexdigest(),
                "reviewed_source_delta": {
                    "baseline_revision": "d" * 40,
                    "tested_source_revision": "e" * 40,
                },
                "rsp_adapter_identity": {
                    "executable_sha256": hashlib.sha256(adapter.read_bytes()).hexdigest(),
                    "path": "build/gate/runner/t64-rsp-diagnostic",
                },
                "rsp_runtime_identity": runtime,
                "schema": "t64.rsp-core-identity-binding.v5",
                "status": "preuse-verified-current-core-for-qualified-rsp-image",
            }
            core_binding = private / "current-core-identity-binding-v5.json"
            core_binding.write_text(json.dumps(binding, sort_keys=True), encoding="utf-8")
            core_binding.chmod(0o600)
            with patch.object(self.module, "ROOT", root), \
                    patch.object(self.module, "PRIVATE", private), \
                    patch.object(self.module, "CORE_BINDING", core_binding), \
                    patch.object(self.module, "PROFILE_ID", "authored-rsp-profile"):
                record = self.module.verify_runtime_binding(adapter, runtime, run_dir)
            self.assertEqual(record["schema"], "t64.rsp-current-execution-binding.v3")
            self.assertEqual(record["tested_source_revision"], "e" * 40)
            self.assertTrue((run_dir / "runtime-adapter-binding.json").is_file())
            self.assertFalse((private / "plan04" / "runtime-adapter-binding.json").exists())

    def test_v5_run_all_reaches_simulated_member_and_completion_validation(self) -> None:
        root = self.workspace
        private = root
        adapter = root / "build/gate/runner/t64-rsp-diagnostic"
        core = root / "build/gate/runner/t64-cpu-diagnostic"
        adapter.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        adapter.write_bytes(b"authored v5 RSP adapter")
        adapter.chmod(0o700)
        core.write_bytes(b"authored v5 CPU reader")
        core.chmod(0o700)
        profile_id = "authored-rsp-profile"
        runtime = {
            "core_build_sha256": "a" * 64,
            "core_source_revision": "b" * 40,
            "profile_descriptor_sha256": "c" * 64,
            "profile_id": profile_id,
        }
        qualification = {"profile_id": profile_id}
        qualification_bytes = json.dumps(qualification, sort_keys=True).encode()
        qualification_path = private / "qualification.json"
        qualification_path.write_bytes(qualification_bytes)
        qualification_path.chmod(0o600)
        binding = {
            "authorized_for_future_guest_use": True,
            "component": "rsp",
            "core_model": 6,
            "core_state_schema": 12,
            "current_identity": {
                "core_build_sha256": runtime["core_build_sha256"],
                "executable_sha256": hashlib.sha256(core.read_bytes()).hexdigest(),
                "path": "build/gate/runner/t64-cpu-diagnostic",
                "source_revision": runtime["core_source_revision"],
            },
            "guest_execution_performed": False,
            "profile_id": profile_id,
            "qualification_sha256": hashlib.sha256(qualification_bytes).hexdigest(),
            "reviewed_source_delta": {
                "baseline_revision": "d" * 40,
                "tested_source_revision": "e" * 40,
            },
            "rsp_adapter_identity": {
                "executable_sha256": hashlib.sha256(adapter.read_bytes()).hexdigest(),
                "path": "build/gate/runner/t64-rsp-diagnostic",
            },
            "rsp_runtime_identity": runtime,
            "schema": "t64.rsp-core-identity-binding.v5",
            "status": "preuse-verified-current-core-for-qualified-rsp-image",
        }
        binding_path = private / "current-core-identity-binding-v5.json"
        binding_path.write_text(json.dumps(binding, sort_keys=True), encoding="utf-8")
        binding_path.chmod(0o600)
        inventory = {"component": "rsp", "members": [sample_member()]}
        build = {"composition": {"layout": {"selector_rom_offset": 0}}}
        canonical_runtime = json.dumps(runtime, sort_keys=True, separators=(",", ":")).encode()
        canonical_inventory = json.dumps(inventory, sort_keys=True, separators=(",", ":")).encode()
        run_dir = private / "run-all-v5"
        adapter_results = []
        capture_commands = []

        def capture_preuse(command, **_kwargs):
            capture_commands.append(command)
            if command[0] == sys.executable:
                return 0, b"authored closure verified", b""
            return 0, canonical_runtime, b""

        with patch.dict(os.environ, {}, clear=True), \
                patch.object(self.module, "ROOT", root), \
                patch.object(self.module, "PRIVATE", private), \
                patch.object(self.module, "CORE_BINDING", binding_path), \
                patch.object(self.module, "PROFILE_ID", profile_id), \
                patch.object(self.module, "adapter_path", return_value=adapter), \
                patch.object(self.module, "run_capture", side_effect=capture_preuse), \
                patch.object(self.module, "verify_preuse", return_value=(qualification, {}, build, b"authored-rom", root / "authored-rom")), \
                patch.object(self.module, "build_inventory", return_value=(inventory, canonical_inventory)), \
                patch.object(self.module, "inventory_members", return_value=[sample_member()]), \
                patch.object(self.module, "write_selector_rom", return_value=root / "selected-authored-rom"), \
                patch.object(self.module, "run_guest", return_value={"classification": "refused", "selector": sample_member()["selector"]}) as run_guest, \
                patch.object(self.module, "invoke_adapter", side_effect=lambda _adapter, *args: (adapter_results.append(args[0]) or (0, b"authored validation ok", b""))) as invoke:
            result = self.module.run_mode("run-all", adapter, run_dir)
        self.assertEqual(result, 0)
        self.assertEqual(len(capture_commands), 2)
        self.assertIn("verify-rsp-diagnostic-qualification.py", capture_commands[0][2])
        self.assertEqual(capture_commands[1][0], str(adapter))
        run_guest.assert_called_once()
        self.assertIn("--validate-inventory", adapter_results)
        self.assertIn("--validate-batch-json", adapter_results)
        self.assertIn("--verify-complete-json", adapter_results)
        self.assertEqual(adapter_results.count("--verify-complete-json"), 1)
        self.assertEqual(invoke.call_count, 3)
        self.assertTrue((run_dir / "execution-binding.json").is_file())
        self.assertTrue((run_dir / "complete-verification.stdout.log").is_file())
        self.assertFalse((private / "plan04" / "runtime-adapter-binding.json").exists())

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
