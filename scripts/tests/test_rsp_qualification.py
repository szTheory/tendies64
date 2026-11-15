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
import copy
import builtins
import symtable
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


class ReviewedSourceScopeControls(unittest.TestCase):
    def setUp(self) -> None:
        spec = importlib.util.spec_from_file_location(
            "rsp_reviewed_source_scope", VERIFY
        )
        if spec is None or spec.loader is None:
            raise AssertionError("RSP source scope verifier could not be loaded")
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)

    def test_source_scope_requires_every_exact_authored_path(self) -> None:
        rows = [
            {"path": "core/authored-machine.c", "sha256": "a" * 64},
            {"path": "runner/authored-protocol.h", "sha256": "b" * 64},
        ]
        checked = self.module.validate_scoped_source_rows(
            rows, {"core/authored-machine.c", "runner/authored-protocol.h"}
        )
        self.assertEqual(set(checked), {"core/authored-machine.c", "runner/authored-protocol.h"})

    def test_source_scope_rejects_missing_duplicate_and_escaping_paths(self) -> None:
        expected = {"core/authored-machine.c", "runner/authored-protocol.h"}
        cases = (
            [{"path": "core/authored-machine.c", "sha256": "a" * 64}],
            [
                {"path": "core/authored-machine.c", "sha256": "a" * 64},
                {"path": "core/authored-machine.c", "sha256": "b" * 64},
            ],
            [
                {"path": "../core/authored-machine.c", "sha256": "a" * 64},
                {"path": "runner/authored-protocol.h", "sha256": "b" * 64},
            ],
        )
        for rows in cases:
            with self.subTest(rows=rows), self.assertRaises(self.module.Refusal):
                self.module.validate_scoped_source_rows(rows, expected)

    def test_profile_policy_pins_descriptor_model_state_and_issue_contract(self) -> None:
        profile = "ntsc-cart-4m-openipl3-rsp-functional-v1"
        source = "static const t64_profile_descriptor " + \
            "ntsc_cart_4m_openipl3_rsp_functional_profile = {\n" + \
            f'  .profile_id = "{profile}",\n' + \
            '  .console_qualification = "unqualified-target-software-seed",\n' + \
            "  .region = T64_REGION_US,\n" + \
            "  .media_kind = T64_MEDIA_CARTRIDGE,\n" + \
            "  .rdram_bytes = T64_BASE_RAM_BYTES,\n" + \
            "  .expansion_bytes = 0u,\n" + \
            "  .peripheral_flags = 0u,\n" + \
            "  .save_kind = T64_SAVE_NONE,\n" + \
            '  .boot_route = "hle-pif+open-ipl3",\n' + \
            '  .boot_status = "prepared-rsp-functional-entry-unqualified",\n' + \
            '  .guest_tick_unit = "model-rsp-cycle",\n' + \
            '  .timing_tier = "functional-rsp-issue-model-policy-v1",\n' + \
            '  .sp_model_version = "rsp-functional-issue-dma-divider-integer-v1",\n' + \
            '  .sp_register_policy = "rsp-issue-status-dma-policy-v1",\n' + \
            '  .dma_visibility_policy = "rsp-one-beat-per-issue-v1",\n' + \
            "  .descriptor_version = 3u,\n" + \
            "  .machine_model_version = 6u,\n" + \
            "  .state_serialization_version = 12u,\n};\n"
        self.module.verify_rsp_profile_policy(source, profile)
        with self.assertRaises(self.module.Refusal):
            self.module.verify_rsp_profile_policy(
                source.replace(".machine_model_version = 6u", ".machine_model_version = 5u"),
                profile,
            )


class V5IdentityReceiptControls(unittest.TestCase):
    def setUp(self) -> None:
        owner_only_directory(TEST_ROOT)
        spec = importlib.util.spec_from_file_location("rsp_v5_identity_receipts", VERIFY)
        if spec is None or spec.loader is None:
            raise AssertionError("RSP v5 receipt verifier could not be loaded")
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        self.temporary = tempfile.TemporaryDirectory(prefix="v5-receipts.", dir=TEST_ROOT)
        self.addCleanup(self.temporary.cleanup)
        self.workspace = pathlib.Path(self.temporary.name)
        self.workspace.chmod(0o700)
        self.attempt = self.workspace / "build/private-rsp-diagnostic/plan10-current-binding-v5-authored"
        self.attempt.mkdir(mode=0o700, parents=True)
        self.private_root = self.workspace / "build/private-rsp-diagnostic"
        self.private_root.chmod(0o700)
        self.adapter = self.workspace / "build/gate/runner/t64-rsp-diagnostic"
        self.core = self.workspace / "build/gate/runner/t64-cpu-diagnostic"
        self.adapter.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        self.adapter.write_bytes(b"authored adapter identity bytes")
        self.core.write_bytes(b"authored CPU identity bytes")
        self.runtime = {
            "core_build_sha256": "a" * 64,
            "core_source_revision": "b" * 40,
            "profile_descriptor_sha256": "c" * 64,
            "profile_id": "authored-rsp-profile",
        }
        self.core_identity = {
            "core_build_sha256": self.runtime["core_build_sha256"],
            "executable_sha256": hashlib.sha256(self.core.read_bytes()).hexdigest(),
            "path": "build/gate/runner/t64-cpu-diagnostic",
            "source_revision": self.runtime["core_source_revision"],
        }
        self.adapter_identity = {
            "executable_sha256": hashlib.sha256(self.adapter.read_bytes()).hexdigest(),
            "path": "build/gate/runner/t64-rsp-diagnostic",
        }
        self.closure_sha = "d" * 64
        closure_stdout_rel = "build/private-rsp-diagnostic/plan10-current-binding-v5-authored/source-closure.stdout.log"
        closure_stderr_rel = "build/private-rsp-diagnostic/plan10-current-binding-v5-authored/source-closure.stderr.log"
        closure_stdout = b"PASS: independent RSP source, assets, tools, and locks verified; no identity executable was invoked\n"
        closure_stderr = b""
        (self.attempt / "source-closure.stdout.log").write_bytes(closure_stdout)
        (self.attempt / "source-closure.stderr.log").write_bytes(closure_stderr)
        closure_command = {
            "argv": ["python3", "-B", "scripts/verify-rsp-diagnostic-qualification.py",
                     "--verify-source-closure", "build/private-rsp-diagnostic/qualification.json"],
            "exit": 0,
            "source_closure_sha256": self.closure_sha,
            "stdout_path": closure_stdout_rel,
            "stdout_sha256": hashlib.sha256(closure_stdout).hexdigest(),
            "stderr_path": closure_stderr_rel,
            "stderr_sha256": hashlib.sha256(closure_stderr).hexdigest(),
        }
        self._write_json(self.attempt / "source-closure-command.json", closure_command)
        self._write_json(self.attempt / "identity-executable-hashes.json", {
            "adapter_path": self.adapter_identity["path"],
            "adapter_sha256_before_use": self.adapter_identity["executable_sha256"],
            "core_reader_path": self.core_identity["path"],
            "core_reader_sha256_before_use": self.core_identity["executable_sha256"],
            "source_closure_command": "passed-before-identity-use",
        })
        rsp_stdout = self._canonical(self.runtime)
        cpu_identity = {"core_build_sha256": self.runtime["core_build_sha256"],
                        "source_revision": self.runtime["core_source_revision"]}
        cpu_stdout = self._canonical(cpu_identity)
        receipts = []
        for label, executable, args, stdout in (
                ("rsp-runtime-identity", self.adapter, ["--runtime-identity-json"], rsp_stdout),
                ("cpu-runtime-identity", self.core, ["--compat-v2", "--runtime-identity-json"], cpu_stdout)):
            stdout_name, stderr_name = f"{label}.stdout.log", f"{label}.stderr.log"
            (self.attempt / stdout_name).write_bytes(stdout)
            (self.attempt / stderr_name).write_bytes(b"")
            receipts.append({
                "argv": [str(executable), *args], "deadline_seconds": 10,
                "elapsed_seconds": 0.001, "exit": 0, "process_group_empty": True,
                "stderr": stderr_name, "stderr_sha256": hashlib.sha256(b"").hexdigest(),
                "stdout": stdout_name, "stdout_sha256": hashlib.sha256(stdout).hexdigest(),
                "timed_out": False,
            })
        self.binding = {
            "identity_attempt_dir": self.attempt.relative_to(self.workspace).as_posix(),
            "preuse_source_closure": {
                "path": (self.attempt / "source-closure-command.json").relative_to(self.workspace).as_posix(),
                "sha256": hashlib.sha256((self.attempt / "source-closure-command.json").read_bytes()).hexdigest(),
            },
            "identity_command_receipts": receipts,
            "rsp_adapter_identity": self.adapter_identity,
            "current_identity": self.core_identity,
            "rsp_runtime_identity": self.runtime,
            "profile_id": self.runtime["profile_id"],
        }

    @staticmethod
    def _canonical(value: object) -> bytes:
        return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode()

    @classmethod
    def _write_json(cls, path: pathlib.Path, value: object) -> None:
        path.write_bytes(cls._canonical(value))

    def test_accepted_identity_receipts_reach_full_v5_check(self) -> None:
        with patch.object(self.module, "ROOT", self.workspace):
            checked = self.module.verify_v5_identity_receipts(
                self.binding, self.closure_sha, self.core_identity,
                self.runtime, self.runtime["core_source_revision"]
            )
        self.assertEqual(checked["core_build_sha256"], self.runtime["core_build_sha256"])
        self.assertEqual(checked["source_revision"], self.runtime["core_source_revision"])

    def test_v5_identity_receipts_reject_bad_profile_and_binary_binding(self) -> None:
        bad_profile = copy.deepcopy(self.binding)
        bad_profile["rsp_runtime_identity"]["profile_id"] = "other-authored-profile"
        bad_binary = copy.deepcopy(self.binding)
        bad_binary["current_identity"]["executable_sha256"] = "0" * 64
        with patch.object(self.module, "ROOT", self.workspace):
            with self.assertRaises(self.module.Refusal):
                self.module.verify_v5_identity_receipts(
                    bad_profile, self.closure_sha, self.core_identity,
                    bad_profile["rsp_runtime_identity"], self.runtime["core_source_revision"]
                )
            with self.assertRaises(self.module.Refusal):
                self.module.verify_v5_identity_receipts(
                    bad_binary, self.closure_sha, bad_binary["current_identity"],
                    self.runtime, self.runtime["core_source_revision"]
                )


class V5AdapterSourceProvenanceControls(unittest.TestCase):
    def setUp(self) -> None:
        prepare_spec = importlib.util.spec_from_file_location("rsp_v5_source_producer", PREPARE)
        verify_spec = importlib.util.spec_from_file_location("rsp_v5_source_verifier", VERIFY)
        if prepare_spec is None or prepare_spec.loader is None or verify_spec is None or verify_spec.loader is None:
            raise AssertionError("RSP v5 source provenance helpers could not be loaded")
        self.producer = importlib.util.module_from_spec(prepare_spec)
        self.verifier = importlib.util.module_from_spec(verify_spec)
        prepare_spec.loader.exec_module(self.producer)
        verify_spec.loader.exec_module(self.verifier)

    def test_producer_refresh_provenance_round_trips_and_rejects_mutation(self) -> None:
        annotation = "a" * 40
        record = self.producer.v5_adapter_source_provenance(annotation)
        self.verifier.verify_v5_adapter_source_provenance(record, annotation)

        bad_tag = dict(record)
        bad_tag["source_provenance"] = "unrecognized-provenance"
        with self.assertRaises(self.verifier.Refusal):
            self.verifier.verify_v5_adapter_source_provenance(bad_tag, annotation)

        bad_annotation = dict(record)
        bad_annotation["core_source_annotation"] = "b" * 40
        with self.assertRaises(self.verifier.Refusal):
            self.verifier.verify_v5_adapter_source_provenance(bad_annotation, annotation)

    def test_adapter_annotation_delta_accepts_only_exact_adapter_paths(self) -> None:
        allowed = self.producer.V5_ADAPTER_SOURCE_PATHS
        self.producer.validate_v5_adapter_annotation_delta(allowed, True)
        self.verifier.verify_v5_adapter_annotation_delta(allowed, True)

        for forbidden in (
            "core/authored-core.c",
            "tools/common/authored-compiler-policy.c",
        ):
            with self.subTest(forbidden=forbidden):
                with self.assertRaises(self.producer.Refusal):
                    self.producer.validate_v5_adapter_annotation_delta(
                        {*allowed, forbidden}, True
                    )
                with self.assertRaises(self.verifier.Refusal):
                    self.verifier.verify_v5_adapter_annotation_delta(
                        {*allowed, forbidden}, True
                    )

        with self.assertRaises(self.producer.Refusal):
            self.producer.validate_v5_adapter_annotation_delta(allowed, False)
        with self.assertRaises(self.verifier.Refusal):
            self.verifier.verify_v5_adapter_annotation_delta(allowed, False)

    def test_adapter_archive_receipt_keeps_exact_predecessor_binding(self) -> None:
        receipt = {
            "adapter_path": "build/gate/runner/t64-rsp-diagnostic",
            "adapter_sha256": "a" * 64,
            "binding_matches_adapter": True,
            "binding_path": "build/private-rsp-diagnostic/authored-binding.json",
            "binding_sha256": "b" * 64,
            "schema": "t64.rsp-plan10-pre-json-bound-validator-fix-adapter-archive.v1",
        }
        self.producer.validate_v5_adapter_archive_receipt(receipt)
        self.verifier.verify_v5_adapter_archive_receipt(receipt)

        bad_match = dict(receipt, binding_matches_adapter=False)
        missing_match = dict(receipt)
        del missing_match["binding_matches_adapter"]
        for invalid in (bad_match, missing_match):
            with self.subTest(invalid=invalid):
                with self.assertRaises(self.producer.Refusal):
                    self.producer.validate_v5_adapter_archive_receipt(invalid)
                with self.assertRaises(self.verifier.Refusal):
                    self.verifier.verify_v5_adapter_archive_receipt(invalid)


class V6SourceAndIdentityControls(unittest.TestCase):
    def setUp(self) -> None:
        prepare_spec = importlib.util.spec_from_file_location("rsp_v6_source_producer", PREPARE)
        verify_spec = importlib.util.spec_from_file_location("rsp_v6_source_verifier", VERIFY)
        if prepare_spec is None or prepare_spec.loader is None or verify_spec is None or verify_spec.loader is None:
            raise AssertionError("RSP v6 source controls could not be loaded")
        self.producer = importlib.util.module_from_spec(prepare_spec)
        self.verifier = importlib.util.module_from_spec(verify_spec)
        prepare_spec.loader.exec_module(self.producer)
        verify_spec.loader.exec_module(self.verifier)

    def test_v6_source_allowlist_and_production_build_policy_are_exact(self) -> None:
        paths = self.producer.V6_HOST_SOURCE_PATHS
        self.producer.validate_v6_host_source_paths(paths)
        self.verifier.verify_v6_host_source_paths(sorted(paths))
        for unexpected in ("scripts/verify-anything.py", "core/t64_rsp.c"):
            with self.subTest(unexpected=unexpected):
                with self.assertRaises(self.producer.Refusal):
                    self.producer.validate_v6_host_source_paths({*paths, unexpected})
                with self.assertRaises(self.verifier.Refusal):
                    self.verifier.verify_v6_host_source_paths({*paths, unexpected})
        self.producer.validate_v6_no_production_delta([])
        self.verifier.verify_v6_no_production_delta([])
        for changed in (["core/authored-core.c"], ["CMakePresets.json"]):
            with self.subTest(changed=changed):
                with self.assertRaises(self.producer.Refusal):
                    self.producer.validate_v6_no_production_delta(changed)
                with self.assertRaises(self.verifier.Refusal):
                    self.verifier.verify_v6_no_production_delta(changed)

    def test_v6_profile_identity_rejects_profile_and_build_mutations(self) -> None:
        profile = "ntsc-cart-4m-openipl3-rsp-functional-v1"
        qualification = {
            "component": "rsp",
            "status": "qualified",
            "profile_id": profile,
            "build_identity": {
                "profile_id": profile,
                "core_model": 6,
                "core_state_schema": 12,
                "core_build_sha256": "f" * 64,
            },
        }
        runtime = {
            "core_build_sha256": "a" * 64,
            "core_source_revision": "b" * 40,
            "profile_descriptor_sha256": "c" * 64,
            "profile_id": profile,
        }
        cpu = {"core_build_sha256": runtime["core_build_sha256"],
               "source_revision": runtime["core_source_revision"]}
        self.producer.validate_v6_profile_identity(qualification, runtime, cpu)
        self.verifier.verify_v6_profile_identity(qualification, runtime, cpu)
        bad_profile = copy.deepcopy(runtime)
        bad_profile["profile_id"] = "authored-other-profile"
        bad_build = copy.deepcopy(cpu)
        bad_build["core_build_sha256"] = "f" * 64
        for changed_runtime, changed_cpu in ((bad_profile, cpu), (runtime, bad_build)):
            with self.subTest(runtime=changed_runtime, cpu=changed_cpu):
                with self.assertRaises(self.producer.Refusal):
                    self.producer.validate_v6_profile_identity(
                        qualification, changed_runtime, changed_cpu)
                with self.assertRaises(self.verifier.Refusal):
                    self.verifier.verify_v6_profile_identity(
                        qualification, changed_runtime, changed_cpu)

    def test_v6_native_proof_requires_exact_passing_test_rows(self) -> None:
        helper_spec = importlib.util.spec_from_file_location(
            "rsp_v6_evidence_rows", ROOT / "scripts/check-rsp-evidence.py")
        if helper_spec is None or helper_spec.loader is None:
            raise AssertionError("strict native evidence helper could not be loaded")
        helper = importlib.util.module_from_spec(helper_spec)
        helper_spec.loader.exec_module(helper)
        native_required = helper.native_gate_tests(ROOT)
        _, phase_tests, _ = helper.owned_tables(ROOT)
        asan_required = (set(phase_tests) | {"runner.cpu-diagnostic-protocol",
                                            "runner.cpu-diagnostic-inventory",
                                            "fuzz.rom-admission",
                                            "sanitizer.negative-control"})

        def ctest_rows(names: set[str]) -> list[str]:
            return [f"{index}/{len(names)} Test #{index}: {name} ........ Passed"
                    for index, name in enumerate(sorted(names), start=1)]

        normal_rows = ctest_rows(native_required)
        asan_rows = ctest_rows(asan_required)
        normal = (f"test inventory PASS preset=gate count={len(native_required)}\n" +
                  "\n".join(normal_rows) +
                  f"\n100% tests passed out of {len(native_required)}\n")
        asan = "\n".join(asan_rows) + f"\n100% tests passed out of {len(asan_required)}\n"
        self.assertEqual(self.producer.validate_v6_ctest_proofs(normal, asan),
                         len(native_required))
        self.assertEqual(self.verifier.verify_v6_ctest_proofs(normal, asan),
                         len(native_required))

        marker_only = (f"test inventory PASS preset=gate count={len(native_required)}\n"
                       f"100% tests passed out of {len(native_required)}\n")
        failed = normal.replace(" Passed", " Failed", 1)
        duplicate = normal.replace(
            f"100% tests passed out of {len(native_required)}",
            f"100% tests passed out of {len(native_required) + 1}")
        duplicate += normal_rows[0] + "\n"
        unknown = ("\n".join(asan_rows + [
            f"{len(asan_required) + 1}/{len(asan_required) + 1} Test #999: unit.invented.row .... Passed"
        ]) + f"\n100% tests passed out of {len(asan_required) + 1}\n")
        for bad_normal, bad_asan in (
                (marker_only, asan), (failed, asan), (duplicate, asan), (normal, unknown)):
            with self.subTest(normal=bad_normal[-80:], asan=bad_asan[-80:]):
                with self.assertRaises(self.producer.Refusal):
                    self.producer.validate_v6_ctest_proofs(bad_normal, bad_asan)
                with self.assertRaises(self.verifier.Refusal):
                    self.verifier.verify_v6_ctest_proofs(bad_normal, bad_asan)

    def test_v6_identity_receipt_helpers_accept_and_reject_mutations(self) -> None:
        owner_only_directory(TEST_ROOT)
        temporary = tempfile.TemporaryDirectory(prefix="v6-receipts.", dir=TEST_ROOT)
        self.addCleanup(temporary.cleanup)
        workspace = pathlib.Path(temporary.name)
        workspace.chmod(0o700)
        private = workspace / "build/private-rsp-diagnostic"
        attempt = private / "plan10-current-binding-v6-authored"
        owner_only_directory(attempt)
        binary_root = workspace / "build/gate/runner"
        binary_root.mkdir(mode=0o700, parents=True)
        adapter = binary_root / "t64-rsp-diagnostic"
        core = binary_root / "t64-cpu-diagnostic"
        adapter.write_bytes(b"authored v6 RSP adapter")
        core.write_bytes(b"authored v6 CPU reader")
        adapter.chmod(0o700)
        core.chmod(0o700)

        profile = "ntsc-cart-4m-openipl3-rsp-functional-v1"
        qualification = {
            "component": "rsp",
            "status": "qualified",
            "profile_id": profile,
            "closure": {"sha256": "d" * 64},
            "build_identity": {
                "profile_id": profile,
                "core_model": 6,
                "core_state_schema": 12,
                "core_build_sha256": "f" * 64,
            },
        }
        runtime = {
            "core_build_sha256": "a" * 64,
            "core_source_revision": "b" * 40,
            "profile_descriptor_sha256": "c" * 64,
            "profile_id": profile,
        }
        cpu_identity = {"core_build_sha256": runtime["core_build_sha256"],
                        "source_revision": runtime["core_source_revision"]}
        closure_stdout = b"PASS: independent RSP source, assets, tools, and locks verified\n"
        closure_stderr = b""
        (attempt / "source-closure.stdout.log").write_bytes(closure_stdout)
        (attempt / "source-closure.stderr.log").write_bytes(closure_stderr)
        closure_command = {
            "argv": ["python3", "-B", "scripts/verify-rsp-diagnostic-qualification.py",
                     "--verify-source-closure", "build/private-rsp-diagnostic/qualification.json"],
            "exit": 0,
            "source_closure_sha256": qualification["closure"]["sha256"],
            "stdout_path": (attempt / "source-closure.stdout.log").relative_to(workspace).as_posix(),
            "stdout_sha256": hashlib.sha256(closure_stdout).hexdigest(),
            "stderr_path": (attempt / "source-closure.stderr.log").relative_to(workspace).as_posix(),
            "stderr_sha256": hashlib.sha256(closure_stderr).hexdigest(),
        }
        closure_path = attempt / "source-closure-command.json"
        closure_path.write_bytes(canonical(closure_command))
        (attempt / "identity-executable-hashes.json").write_bytes(canonical({
            "adapter_path": "build/gate/runner/t64-rsp-diagnostic",
            "adapter_sha256_before_use": hashlib.sha256(adapter.read_bytes()).hexdigest(),
            "core_reader_path": "build/gate/runner/t64-cpu-diagnostic",
            "core_reader_sha256_before_use": hashlib.sha256(core.read_bytes()).hexdigest(),
            "source_closure_command": "passed-before-identity-use",
        }))
        receipts = []
        for label, executable, arguments, output in (
                ("rsp-runtime-identity", adapter, ["--runtime-identity-json"], canonical(runtime)),
                ("cpu-runtime-identity", core,
                 ["--compat-v2", "--runtime-identity-json"], canonical(cpu_identity))):
            stdout_name, stderr_name = f"{label}.stdout.log", f"{label}.stderr.log"
            (attempt / stdout_name).write_bytes(output)
            (attempt / stderr_name).write_bytes(b"")
            receipts.append({
                "argv": [str(executable), *arguments],
                "deadline_seconds": 10,
                "elapsed_seconds": 0.001,
                "exit": 0,
                "process_group_empty": True,
                "stderr": stderr_name,
                "stderr_sha256": hashlib.sha256(b"").hexdigest(),
                "stdout": stdout_name,
                "stdout_sha256": hashlib.sha256(output).hexdigest(),
                "timed_out": False,
            })
        binding = {
            "identity_attempt_dir": attempt.relative_to(workspace).as_posix(),
            "preuse_source_closure": {
                "path": closure_path.relative_to(workspace).as_posix(),
                "sha256": hashlib.sha256(closure_path.read_bytes()).hexdigest(),
            },
            "identity_command_receipts": receipts,
            "rsp_adapter_identity": {
                "path": "build/gate/runner/t64-rsp-diagnostic",
                "executable_sha256": hashlib.sha256(adapter.read_bytes()).hexdigest(),
            },
            "current_identity": {
                "path": "build/gate/runner/t64-cpu-diagnostic",
                "executable_sha256": hashlib.sha256(core.read_bytes()).hexdigest(),
                "core_build_sha256": runtime["core_build_sha256"],
                "source_revision": runtime["core_source_revision"],
            },
            "rsp_runtime_identity": runtime,
        }
        for path in attempt.iterdir():
            path.chmod(0o600)
        with patch.object(self.verifier, "ROOT", workspace):
            self.verifier.verify_v6_identity_receipts(
                binding, qualification, binding["current_identity"], runtime)
            self.verifier.verify_v6_profile_identity(qualification, runtime, cpu_identity)
            bad_profile = copy.deepcopy(runtime)
            bad_profile["profile_id"] = "authored-other-profile"
            with self.assertRaises(self.verifier.Refusal):
                self.verifier.verify_v6_profile_identity(qualification, bad_profile, cpu_identity)
            bad_source = copy.deepcopy(binding)
            bad_source["identity_attempt_dir"] = "build/private-rsp-diagnostic/other-attempt"
            with self.assertRaises(self.verifier.Refusal):
                self.verifier.verify_v6_identity_receipts(
                    bad_source, qualification, binding["current_identity"], runtime)
            adapter.write_bytes(b"mutated authored v6 adapter")
            with self.assertRaises(self.verifier.Refusal):
                self.verifier.verify_v6_identity_receipts(
                    binding, qualification, binding["current_identity"], runtime)

    def test_v6_full_binding_verifier_accepts_and_dispatches_runtime_identity(self) -> None:
        owner_only_directory(TEST_ROOT)
        temporary = tempfile.TemporaryDirectory(prefix="v6-full-binding.", dir=TEST_ROOT)
        self.addCleanup(temporary.cleanup)
        workspace = pathlib.Path(temporary.name)
        workspace.chmod(0o700)
        private = workspace / "build/private-rsp-diagnostic"
        owner_only_directory(private)
        attempt = private / "plan10-current-binding-v6-refresh1-authored"
        owner_only_directory(attempt)
        binary_root = workspace / "build/gate/runner"
        binary_root.mkdir(mode=0o700, parents=True)
        adapter = binary_root / "t64-rsp-diagnostic"
        core = binary_root / "t64-cpu-diagnostic"
        adapter.write_bytes(b"authored v6-refresh1 adapter")
        core.write_bytes(b"authored v6-refresh1 CPU reader")
        adapter.chmod(0o700)
        core.chmod(0o700)

        profile = "ntsc-cart-4m-openipl3-rsp-functional-v1"
        base_identity = "e" * 64
        qualification = {
            "schema": "t64.rsp-qualification.v1",
            "status": "qualified",
            "component": "rsp",
            "profile_id": profile,
            "closure": {"sha256": "d" * 64},
            "build_identity": {
                "profile_id": profile,
                "core_model": 6,
                "core_state_schema": 12,
                "core_build_sha256": base_identity,
            },
        }
        qualification_bytes = canonical(qualification)
        qualification_sha = hashlib.sha256(qualification_bytes).hexdigest()

        def write_private(path: pathlib.Path, data: bytes) -> None:
            path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
            path.write_bytes(data)
            path.chmod(0o600)

        write_private(private / "qualification.json", qualification_bytes)
        rom_path = private / "authored-rom.z64"
        rom_bytes = b"authored unexecuted ROM fixture"
        write_private(rom_path, rom_bytes)
        qualified_build = {
            "qualification_sha256": qualification_sha,
            "guest_rom_executed": False,
            "artifacts": {"rom": {
                "path": rom_path.relative_to(workspace).as_posix(),
                "sha256": hashlib.sha256(rom_bytes).hexdigest(),
            }},
        }
        qualified_build_bytes = canonical(qualified_build)
        write_private(private / "qualified-build.json", qualified_build_bytes)

        runtime = {
            "core_build_sha256": "a" * 64,
            "core_source_revision": "b" * 40,
            "profile_descriptor_sha256": "c" * 64,
            "profile_id": profile,
        }
        cpu_identity = {
            "core_build_sha256": runtime["core_build_sha256"],
            "source_revision": runtime["core_source_revision"],
        }
        history = {
            "adapter_exit": 1,
            "classification": "refused",
            "historical_files": [],
            "original_launcher_failure_preserved": True,
            "outcome": "refused",
            "status": "retained",
        }
        original_identity = {"source_revision": "9" * 40}
        prior = {
            "schema": "t64.rsp-core-identity-binding.v4",
            "qualification_sha256": qualification_sha,
            "original_identity": original_identity,
            "historical_first_use": history,
        }
        prior_path = private / "current-core-identity-binding-v4.json"
        prior_bytes = canonical(prior)
        write_private(prior_path, prior_bytes)

        closure_stdout = b"PASS: independent RSP source, assets, tools, and locks verified\n"
        closure_stderr = b""
        stdout_path = attempt / "source-closure.stdout.log"
        stderr_path = attempt / "source-closure.stderr.log"
        write_private(stdout_path, closure_stdout)
        write_private(stderr_path, closure_stderr)
        closure_command = {
            "argv": ["python3", "-B", "scripts/verify-rsp-diagnostic-qualification.py",
                     "--verify-source-closure", "build/private-rsp-diagnostic/qualification.json"],
            "exit": 0,
            "source_closure_sha256": qualification["closure"]["sha256"],
            "stdout_path": stdout_path.relative_to(workspace).as_posix(),
            "stdout_sha256": hashlib.sha256(closure_stdout).hexdigest(),
            "stderr_path": stderr_path.relative_to(workspace).as_posix(),
            "stderr_sha256": hashlib.sha256(closure_stderr).hexdigest(),
        }
        closure_path = attempt / "source-closure-command.json"
        closure_bytes = canonical(closure_command)
        write_private(closure_path, closure_bytes)
        adapter_sha = hashlib.sha256(adapter.read_bytes()).hexdigest()
        core_sha = hashlib.sha256(core.read_bytes()).hexdigest()
        write_private(attempt / "identity-executable-hashes.json", canonical({
            "adapter_path": "build/gate/runner/t64-rsp-diagnostic",
            "adapter_sha256_before_use": adapter_sha,
            "core_reader_path": "build/gate/runner/t64-cpu-diagnostic",
            "core_reader_sha256_before_use": core_sha,
            "source_closure_command": "passed-before-identity-use",
        }))
        receipts = []
        for label, executable, arguments, output in (
                ("rsp-runtime-identity", adapter, ["--runtime-identity-json"], canonical(runtime)),
                ("cpu-runtime-identity", core,
                 ["--compat-v2", "--runtime-identity-json"], canonical(cpu_identity))):
            stdout_name, stderr_name = f"{label}.stdout.log", f"{label}.stderr.log"
            write_private(attempt / stdout_name, output)
            write_private(attempt / stderr_name, b"")
            receipts.append({
                "argv": [str(executable), *arguments],
                "deadline_seconds": 10,
                "elapsed_seconds": 0.001,
                "exit": 0,
                "process_group_empty": True,
                "stderr": stderr_name,
                "stderr_sha256": hashlib.sha256(b"").hexdigest(),
                "stdout": stdout_name,
                "stdout_sha256": hashlib.sha256(output).hexdigest(),
                "timed_out": False,
            })

        binding = {
            "authorized_for_future_guest_use": True,
            "binary_archive_path": "build/private-rsp-diagnostic/plan10-v6-refresh1-executable-archive",
            "component": "rsp",
            "core_model": 6,
            "core_source_scope": [],
            "core_state_schema": 12,
            "current_identity": {
                "core_build_sha256": runtime["core_build_sha256"],
                "executable_sha256": core_sha,
                "path": "build/gate/runner/t64-cpu-diagnostic",
                "source_revision": runtime["core_source_revision"],
            },
            "guest_execution_performed": False,
            "historical_first_use": history,
            "host_source_scope": [],
            "identity_attempt_dir": attempt.relative_to(workspace).as_posix(),
            "identity_command_receipts": receipts,
            "native_evidence": {},
            "original_identity": original_identity,
            "preuse_source_closure": {
                "path": closure_path.relative_to(workspace).as_posix(),
                "sha256": hashlib.sha256(closure_bytes).hexdigest(),
            },
            "profile_id": profile,
            "qualified_build_sha256": hashlib.sha256(qualified_build_bytes).hexdigest(),
            "qualified_rom_sha256": hashlib.sha256(rom_bytes).hexdigest(),
            "qualification_sha256": qualification_sha,
            "reviewed_source_delta": {},
            "rsp_adapter_identity": {
                "executable_sha256": adapter_sha,
                "path": "build/gate/runner/t64-rsp-diagnostic",
            },
            "rsp_runtime_identity": runtime,
            "schema": "t64.rsp-core-identity-binding.v6-refresh1",
            "status": "preuse-verified-current-core-for-qualified-rsp-image",
            "supersedes_binding_v4": {
                "path": prior_path.relative_to(workspace).as_posix(),
                "sha256": hashlib.sha256(prior_bytes).hexdigest(),
            },
            "verification_scope": "authored test fixture",
        }
        binding_bytes = canonical(binding)
        binding_path = private / "current-core-identity-binding-v6-refresh1.json"
        write_private(binding_path, binding_bytes)
        archive_root = private / "plan10-v6-refresh1-executable-archive"
        owner_only_directory(archive_root)
        write_private(archive_root / "identity-binding.json", binding_bytes)
        write_private(archive_root / "adapter.bin", adapter.read_bytes())
        write_private(archive_root / "core-reader.bin", core.read_bytes())
        archive_receipt = {
            "adapter_path": "build/private-rsp-diagnostic/plan10-v6-refresh1-executable-archive/adapter.bin",
            "adapter_sha256": adapter_sha,
            "binding_path": "build/private-rsp-diagnostic/plan10-v6-refresh1-executable-archive/identity-binding.json",
            "binding_sha256": hashlib.sha256(binding_bytes).hexdigest(),
            "core_reader_path": "build/private-rsp-diagnostic/plan10-v6-refresh1-executable-archive/core-reader.bin",
            "core_reader_sha256": core_sha,
            "schema": "t64.rsp-plan10-v6-refresh1-executable-archive.v1",
        }
        write_private(archive_root / "archive-receipt.json", canonical(archive_receipt))

        def identity_run(command, **_kwargs):
            output = canonical(runtime if command[0] == str(adapter) else cpu_identity)
            return subprocess.CompletedProcess(command, 0, output, b"")

        with patch.object(self.verifier, "ROOT", workspace), \
                patch.object(self.verifier, "verify_v6_source_scope", return_value=([], [])), \
                patch.object(self.verifier, "verify_v6_native_evidence"), \
                patch.object(self.verifier.subprocess, "run", side_effect=identity_run), \
                patch.object(self.verifier, "verify_rsp_core_binding_v6",
                             wraps=self.verifier.verify_rsp_core_binding_v6) as full_binding:
            self.assertEqual(self.verifier.verify_rsp_core_binding_v6(
                binding, qualification_sha, qualification["build_identity"], "v6-refresh1"), runtime)
            self.verifier.verify_current_core_identity(
                qualification["build_identity"], qualification_sha, "v6-refresh1")
            self.assertEqual(full_binding.call_count, 2)
            self.assertEqual(full_binding.call_args.args[3], "v6-refresh1")

            bad_source = copy.deepcopy(binding)
            bad_source["current_identity"]["source_revision"] = "malformed"
            with self.assertRaisesRegex(self.verifier.Refusal, "artifact identity is malformed"):
                self.verifier.verify_rsp_core_binding_v6(
                    bad_source, qualification_sha, qualification["build_identity"], "v6-refresh1")
            bad_profile = copy.deepcopy(binding)
            bad_profile["rsp_runtime_identity"]["profile_descriptor_sha256"] = "malformed"
            with self.assertRaisesRegex(self.verifier.Refusal, "runtime identity differs"):
                self.verifier.verify_rsp_core_binding_v6(
                    bad_profile, qualification_sha, qualification["build_identity"], "v6-refresh1")
            bad_archive = copy.deepcopy(binding)
            bad_archive["binary_archive_path"] = "build/private-rsp-diagnostic/plan10-v6-executable-archive"
            with self.assertRaisesRegex(self.verifier.Refusal, "unexpected executable archive"):
                self.verifier.verify_rsp_core_binding_v6(
                    bad_archive, qualification_sha, qualification["build_identity"], "v6-refresh1")
            with self.assertRaisesRegex(self.verifier.Refusal, "sealed functional profile"):
                self.verifier.verify_rsp_core_binding_v6(
                    binding, qualification_sha, qualification["build_identity"], "v6")

    def test_v6_closure_failure_precedes_any_identity_invocation(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="v6-closure-order.", dir=TEST_ROOT)
        self.addCleanup(temporary.cleanup)
        workspace = pathlib.Path(temporary.name)
        workspace.chmod(0o700)
        private = workspace / "build/private-rsp-diagnostic"
        owner_only_directory(private)
        qualification = {
            "schema": "t64.rsp-qualification.v1",
            "status": "qualified",
            "component": "rsp",
            "profile_id": "ntsc-cart-4m-openipl3-rsp-functional-v1",
            "closure": {"sha256": "a" * 64},
        }
        qualification_path = private / "qualification.json"
        qualification_path.write_bytes(canonical(qualification))
        qualification_path.chmod(0o600)
        fake = type("CommandResult", (), {"returncode": 1, "stdout": b"refused", "stderr": b""})()
        with patch.object(self.producer, "ROOT", workspace), \
                patch.object(self.producer.subprocess, "run", return_value=fake) as run:
            with self.assertRaisesRegex(self.producer.Refusal, "closure did not pass"):
                self.producer.bind_current_rsp_preuse_v6()
        self.assertEqual(run.call_count, 1)
        self.assertIn("--verify-source-closure", run.call_args.args[0])

    def test_v6_fixed_outputs_refuse_collisions(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="v6-collision.", dir=TEST_ROOT)
        self.addCleanup(temporary.cleanup)
        workspace = pathlib.Path(temporary.name)
        workspace.chmod(0o700)
        private = workspace / "build/private-rsp-diagnostic"
        owner_only_directory(private)
        with patch.object(self.producer, "ROOT", workspace):
            archive = workspace / self.producer.V6_ARCHIVE_RELATIVE
            archive.mkdir(mode=0o700, parents=True)
            with self.assertRaises(self.producer.Refusal):
                self.producer.ensure_v6_outputs_available(private)
            archive.rmdir()
            (private / "current-core-identity-binding-v6.json").write_bytes(b"occupied")
            with self.assertRaises(self.producer.Refusal):
                self.producer.ensure_v6_outputs_available(private)
        receipt = {
            "adapter_path": (self.producer.V6_ARCHIVE_RELATIVE / "adapter.bin").as_posix(),
            "adapter_sha256": "a" * 64,
            "binding_path": (self.producer.V6_ARCHIVE_RELATIVE / "identity-binding.json").as_posix(),
            "binding_sha256": "b" * 64,
            "core_reader_path": (self.producer.V6_ARCHIVE_RELATIVE / "core-reader.bin").as_posix(),
            "core_reader_sha256": "c" * 64,
            "schema": "t64.rsp-plan10-v6-executable-archive.v1",
        }
        self.producer.validate_v6_executable_archive_receipt(receipt)
        for invalid in (
            dict(receipt, schema="t64.rsp-plan10-v5-executable-archive.v1"),
            dict(receipt, adapter_path="build/private-rsp-diagnostic/other.bin"),
            dict(receipt, core_reader_sha256="bad"),
        ):
            with self.subTest(invalid=invalid):
                with self.assertRaises(self.producer.Refusal):
                    self.producer.validate_v6_executable_archive_receipt(invalid)

    def test_v6_refresh1_uses_separate_immutable_paths_and_archive_schema(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="v6-refresh1-paths.", dir=TEST_ROOT)
        self.addCleanup(temporary.cleanup)
        workspace = pathlib.Path(temporary.name)
        workspace.chmod(0o700)
        private = workspace / "build/private-rsp-diagnostic"
        owner_only_directory(private)
        with patch.object(self.producer, "ROOT", workspace):
            old_binding = private / "current-core-identity-binding-v6.json"
            old_binding.write_bytes(b"preserved original v6 binding")
            old_archive = workspace / self.producer.V6_ARCHIVE_RELATIVE
            old_archive.mkdir(mode=0o700, parents=True)
            self.producer.ensure_v6_outputs_available(private, refresh1=True)
            self.assertEqual(old_binding.read_bytes(), b"preserved original v6 binding")

            refresh_archive = workspace / self.producer.V6_REFRESH1_ARCHIVE_RELATIVE
            refresh_archive.mkdir(mode=0o700, parents=True)
            with self.assertRaises(self.producer.Refusal):
                self.producer.ensure_v6_outputs_available(private, refresh1=True)
            refresh_archive.rmdir()
            (private / "current-core-identity-binding-v6-refresh1.json").write_bytes(b"occupied")
            with self.assertRaises(self.producer.Refusal):
                self.producer.ensure_v6_outputs_available(private, refresh1=True)

        refresh_receipt = {
            "adapter_path": "build/private-rsp-diagnostic/plan10-v6-refresh1-executable-archive/adapter.bin",
            "adapter_sha256": "a" * 64,
            "binding_path": "build/private-rsp-diagnostic/plan10-v6-refresh1-executable-archive/identity-binding.json",
            "binding_sha256": "b" * 64,
            "core_reader_path": "build/private-rsp-diagnostic/plan10-v6-refresh1-executable-archive/core-reader.bin",
            "core_reader_sha256": "c" * 64,
            "schema": "t64.rsp-plan10-v6-refresh1-executable-archive.v1",
        }
        self.producer.validate_v6_executable_archive_receipt(refresh_receipt, refresh1=True)
        with self.assertRaises(self.producer.Refusal):
            self.producer.validate_v6_executable_archive_receipt(refresh_receipt)

    def test_v6_refresh1_cli_selects_only_fixed_refresh_context(self) -> None:
        with patch.object(self.producer.sys, "argv", [str(PREPARE), "--refresh-current-rsp-preuse-v6"]), \
                patch.object(self.producer, "bind_current_rsp_preuse_v6") as bind:
            self.assertEqual(self.producer.main(), 0)
        bind.assert_called_once_with(refresh1=True)

    def test_changed_python_modules_have_no_unbound_global_references(self) -> None:
        special = {"__file__", "__name__", "__package__", "__spec__", "__loader__", "__cached__"}
        allowed = set(dir(builtins)) | special
        paths = (PREPARE, VERIFY, ROOT / "scripts/run-rsp-diagnostic.py",
                 ROOT / "scripts/tests/test_rsp_qualification.py",
                 ROOT / "scripts/tests/test_rsp_run.py")
        unresolved: list[str] = []

        def walk(table: symtable.SymbolTable, declared: set[str], relative: str) -> None:
            for name in table.get_identifiers():
                symbol = table.lookup(name)
                if (symbol.is_global() and symbol.is_referenced() and
                        name not in declared and name not in allowed):
                    unresolved.append(f"{relative}:{table.get_name()}:{name}:{table.get_lineno()}")
            for child in table.get_children():
                walk(child, declared, relative)

        for path in paths:
            relative = path.relative_to(ROOT).as_posix()
            module = symtable.symtable(path.read_text(encoding="utf-8"), relative, "exec")
            declared = {name for name in module.get_identifiers()
                        if module.lookup(name).is_assigned() or module.lookup(name).is_imported()}
            walk(module, declared, relative)
        self.assertEqual(unresolved, [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
