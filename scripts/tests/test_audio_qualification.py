#!/usr/bin/env python3
"""Exercise the bounded audio evidence verifier without builds or nested CTest."""

from __future__ import annotations

import ctypes
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Any


_SCRIPT_ROOT = Path(__file__).resolve().parents[2]
_WORKING_ROOT = Path.cwd().resolve()
if (_SCRIPT_ROOT / "docs/spec/audio-cases.json").is_file() and \
        (_SCRIPT_ROOT / "scripts/qualify-audio.py").is_file():
    ROOT = _SCRIPT_ROOT
elif (_WORKING_ROOT / "docs/spec/audio-cases.json").is_file() and \
        (_WORKING_ROOT / "scripts/qualify-audio.py").is_file():
    ROOT = _WORKING_ROOT
else:
    ROOT = _SCRIPT_ROOT
QUALIFIER = ROOT / "scripts/qualify-audio.py"
CASE_NAMES = (
    "accepted-original-binding",
    "changed-sample-rejected",
    "wrong-profile-build-input-rejected",
    "missing-zero-truncated-rejected",
    "zero-or-omitted-case-resealed-rejected",
    "stale-or-missing-command-result-rejected",
    "raw-upload-rejected",
    "pii-projection-rejected",
    "hosted-platform-witness-required",
    "exact-hosted-context-bound",
    "native-command-plan-explicit-builds",
    "phase08-gcc-closure-units-and-frame-policy",
    "qualification-header-refuses-smoke-or-incomplete-suites",
    "gate-junit-hook-is-private-and-opt-in",
    "hosted-api-zip-digest-and-context-bound",
    "native-process-record-rejects-failed-or-truncated-tail",
    "hosted-record-rejects-entry-smoke-or-missing-platform",
    "public-hosted-pair-requires-both-platforms",
    "combined-upload-uses-exact-pii-scan-layout",
    "final-evidence-cli-accepts-explicit-context-and-refuses-missing-record",
)

_SPEC = importlib.util.spec_from_file_location("t64_qualify_audio", QUALIFIER)
assert _SPEC is not None and _SPEC.loader is not None
qualify = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(qualify)


def _projection() -> dict[str, Any]:
    digest = "a" * 64
    source_revision = "b" * 40
    core_build = "t64-build-v1-" + "c" * 64
    runner_build = "t64-runner-v1-" + "d" * 64
    inventory = qualify._validate_inventory(
        qualify._read(qualify.INVENTORY, 128 * 1024),
        qualify.EXPECTED_INVENTORY_SHA256,
    )
    return {
        "schema": qualify.PUBLIC_SCHEMA,
        "schema_version": 1,
        "evidence_mode": "entry-path-validation",
        "workflow_sha": None,
        "source_revision": source_revision,
        "job_name": "local",
        "run_id": None,
        "run_attempt": None,
        "platform": "linux",
        "profile": {
            "profile_id": qualify.PROFILE_ID,
            "profile_sha256": qualify.PROFILE_SHA256,
            "descriptor_version": 11,
            "machine_model_version": 19,
            "state_serialization_version": 24,
        },
        "fixture": {
            "fixture_id": qualify.FIXTURE_ID,
            "fixture_sha256": qualify.FIXTURE_SHA256,
            "fixture_size": 4728,
            "input_sha256": qualify.INPUT_SHA256,
            "input_size": 54,
        },
        "producer": {
            "source_revision": source_revision,
            "core_build_id": core_build,
            "runner_build_id": runner_build,
            "real_runner_sha256": digest,
            "supervisor_sha256": digest,
            "checker_sha256": digest,
            "test_runner_sha256": digest,
            "test_supervisor_sha256": digest,
            "test_binary_sha256": digest,
            "core_library_sha256": digest,
        },
        "capture": {
            "manifest_sha256": digest,
            "video_packets": 29,
            "audio_packets": 2,
            "total_bytes": 16581,
            "pcm_literal_sha256": qualify._sha(qualify.PCM),
            "video_observer_sha256": digest,
            "audio_observer_sha256": digest,
        },
        "execution": {
            "capture_ctest": "runner.raw-capture",
            "race_ctest": "controls.real.raw-capture-race",
            "command_sha256": digest,
            "race_command_sha256": digest,
            "last_test_sha256": digest,
            "junit_sha256": digest,
            "return_code": 0,
            "status": "pass",
            "timed_out": False,
        },
        "inventory": inventory,
        "platform_witness": {
            "platform": "linux",
            "test_binary_sha256": digest,
            "runner_binary_sha256": digest,
            "supervisor_binary_sha256": digest,
            "checker_binary_sha256": digest,
            "ctest_command_sha256": digest,
            "contender_command_sha256": [digest, digest],
            "outcomes": [
                "harness-error:capture-no-replace-commit",
                "pass:positive-match",
            ],
        },
    }


class AudioQualificationTests(unittest.TestCase):
    def test_truncated_forged_resealed_audio_observation_rejected(self) -> None:
        build_id = "t64-build-v1-" + "c" * 64
        entries = [
            {
                "sequence": "1", "first_sample_index": "0",
                "frame_count": "4", "rate_numerator": "48681812",
                "rate_denominator": "1104", "channels": "2",
                "bits_per_sample": "16", "byte_length": "16",
                "first_tick": "540414", "last_tick": "544666",
                "encoding": "s16be-interleaved-stereo",
            },
            {
                "sequence": "2", "first_sample_index": "4",
                "frame_count": "4", "rate_numerator": "48681812",
                "rate_denominator": "1104", "channels": "2",
                "bits_per_sample": "16", "byte_length": "16",
                "first_tick": "546083", "last_tick": "550335",
                "encoding": "s16be-interleaved-stereo",
            },
        ]
        manifest = {"core_build_id": build_id, "audio": entries}
        result = {"guest_tick": "551001"}
        observation = qualify._TAudioObservation()
        observation.magic = 0x54363441
        observation.version = 2
        observation.packet_count = 2
        observation.bus_write_count = 16
        observation.sw_retirement_count = 15
        observation.mi_ai_read_seen = 1
        observation.guest_mi_snapshot_seen = 1
        observation.trace_records = 2346555
        observation.mi_ai_read_tick = 550738
        observation.guest_last_cpu_tick = 551000
        observation.guest_stop_tick = 551001
        observation.guest_stop_reason = 8
        packet_data = [qualify.PCM[:16], qualify.PCM[16:]]
        for index, (entry, raw) in enumerate(zip(entries, packet_data)):
            packet = observation.packets[index]
            info = packet.info
            info.struct_size = ctypes.sizeof(qualify._TAudioPacketInfo)
            info.sequence = int(entry["sequence"])
            info.first_sample_index = int(entry["first_sample_index"])
            info.first_tick = int(entry["first_tick"])
            info.last_tick = int(entry["last_tick"])
            info.frame_count = int(entry["frame_count"])
            info.rate_numerator = int(entry["rate_numerator"])
            info.rate_denominator = int(entry["rate_denominator"])
            info.channels = int(entry["channels"])
            info.bits_per_sample = int(entry["bits_per_sample"])
            info.encoding = 1
            info.byte_length = int(entry["byte_length"])
            info.profile_id = qualify.PROFILE_ID.encode("ascii")
            info.build_id = build_id.encode("ascii")
            for byte_index, value in enumerate(raw):
                packet.bytes[byte_index] = value
        valid = ctypes.string_at(ctypes.addressof(observation),
                                 ctypes.sizeof(observation))

        qualify._verify_audio_observation(valid, manifest, entries, packet_data, result)
        self.assertEqual(len(valid), 3976)
        with self.assertRaises(qualify.Refusal):
            qualify._verify_audio_observation(valid[:-1], manifest, entries,
                                              packet_data, result)

        wrong_version = qualify._TAudioObservation.from_buffer_copy(valid)
        wrong_version.version = 3
        forged_version = ctypes.string_at(ctypes.addressof(wrong_version),
                                          ctypes.sizeof(wrong_version))
        self.assertEqual(len(qualify._sha(forged_version)), 64)
        with self.assertRaises(qualify.Refusal):
            qualify._verify_audio_observation(forged_version, manifest, entries,
                                              packet_data, result)

        forged = qualify._TAudioObservation.from_buffer_copy(valid)
        forged.packets[0].info.byte_length = 15
        forged_bytes = ctypes.string_at(ctypes.addressof(forged),
                                        ctypes.sizeof(forged))
        resealed_manifest = json.loads(json.dumps(manifest))
        resealed_manifest["audio"][0]["byte_length"] = "15"
        self.assertEqual(len(qualify._sha(forged_bytes)), 64)
        with self.assertRaises(qualify.Refusal):
            qualify._verify_audio_observation(
                forged_bytes, resealed_manifest, resealed_manifest["audio"],
                packet_data, result,
            )

    def test_hosted_api_zip_digest_and_context_bound(self) -> None:
        archive = b"bounded synthetic archive bytes"
        digest = "sha256:" + qualify._sha(archive)
        artifact = {
            "id": 401,
            "name": "t64-authored-evidence-" + "c" * 40 + "-123-1-gate-linux",
            "expired": False,
            "size_in_bytes": len(archive),
            "digest": digest,
            "workflow_run": {"id": 123, "head_sha": "c" * 40},
        }
        binding = {
            "artifact_id": "401", "commit_sha": "c" * 40, "run_id": "123",
            "run_attempt": "1", "job_name": "gate-linux",
            "archive_digest": digest, "archive_bytes": len(archive),
        }
        qualify._verify_artifact_download(
            artifact, binding, archive, repo="szTheory/tendies64", sha="c" * 40,
            run="123", attempt="1", job="gate-linux",
        )
        with self.assertRaises(qualify.Refusal):
            qualify._verify_artifact_download(
                artifact, binding, archive + b"x", repo="szTheory/tendies64",
                sha="c" * 40, run="123", attempt="1", job="gate-linux",
            )
        wrong_attempt = dict(artifact, name=artifact["name"].replace("-123-1-", "-123-2-"))
        with self.assertRaises(qualify.Refusal):
            qualify._verify_artifact_download(
                wrong_attempt, binding, archive, repo="szTheory/tendies64",
                sha="c" * 40, run="123", attempt="1", job="gate-linux",
            )
        for field in ("id", "size_in_bytes"):
            forged = dict(artifact, **{field: True})
            with self.assertRaises(qualify.Refusal):
                qualify._verify_artifact_download(
                    forged, binding, archive, repo="szTheory/tendies64",
                    sha="c" * 40, run="123", attempt="1", job="gate-linux",
                )

    def test_native_process_record_rejects_failed_or_truncated_tail(self) -> None:
        import hashlib

        with tempfile.TemporaryDirectory(prefix="t64-audio-process-record-") as temporary:
            directory = Path(temporary)
            command = ["tool", "--exact", "argv"]
            stdout = b"finished successfully\n"
            stderr = b""
            (directory / "stdout.log").write_bytes(stdout)
            (directory / "stderr.log").write_bytes(stderr)
            (directory / "command.json").write_text(json.dumps({
                "argv": command, "cwd": str(ROOT), "env": {}, "timeout_seconds": 5,
            }))
            process = {
                "return_code": 0, "timed_out": False, "timeout_seconds": 5,
                "stdout_bytes": len(stdout), "stderr_bytes": len(stderr),
                "stdout_sha256": hashlib.sha256(stdout).hexdigest(),
                "stderr_sha256": hashlib.sha256(stderr).hexdigest(),
            }
            process_path = directory / "process.json"
            process_path.write_text(json.dumps(process))
            qualify._verify_process_record(directory, command, max_timeout=10)
            (directory / "stdout.log").write_bytes(stdout[:-3])
            with self.assertRaises(qualify.Refusal):
                qualify._verify_process_record(directory, command, max_timeout=10)
            (directory / "stdout.log").write_bytes(stdout)
            process["return_code"] = 1
            process_path.write_text(json.dumps(process))
            with self.assertRaises(qualify.Refusal):
                qualify._verify_process_record(directory, command, max_timeout=10)

    def test_hosted_record_rejects_entry_smoke_or_missing_platform(self) -> None:
        base = {
            "schema": "t64.audio-hosted-collection",
            "schema_version": 1,
            "evidence_mode": "hosted-gate",
            "status": "complete",
            "repo": "szTheory/tendies64",
            "sha": "c" * 40,
            "run": "123",
            "attempt": "1",
            "jobs": ["gate-linux", "gate-macos"],
            "source_snapshot": {},
            "excluded_owner_metadata": [],
            "native_record_path": "/private/native-record.json",
            "native_record_sha256": "a" * 64,
            "archive_bindings": [{}, {}],
            "collection_path": "/private/collection",
            "downloads_path": "/private/downloads",
            "combined_path": "/private/combined",
            "pii_input_path": "/private/pii-upload-layout",
            "collector_command_path": "/private/collector-command",
            "public_output_path": "/private/public-audio",
        }
        qualify._validate_hosted_record_header(
            base, repo="szTheory/tendies64", sha="c" * 40, run="123", attempt="1",
        )
        smoke = dict(base, evidence_mode="entry-path-validation")
        with self.assertRaises(qualify.Refusal):
            qualify._validate_hosted_record_header(
                smoke, repo="szTheory/tendies64", sha="c" * 40, run="123", attempt="1",
            )
        one_platform = dict(base, jobs=["gate-linux"])
        with self.assertRaises(qualify.Refusal):
            qualify._validate_hosted_record_header(
                one_platform, repo="szTheory/tendies64", sha="c" * 40, run="123", attempt="1",
            )
        missing_pii = dict(base)
        missing_pii.pop("pii_input_path")
        with self.assertRaises(qualify.Refusal):
            qualify._validate_hosted_record_header(
                missing_pii, repo="szTheory/tendies64", sha="c" * 40,
                run="123", attempt="1",
            )

    def test_public_hosted_pair_requires_both_platforms(self) -> None:
        with tempfile.TemporaryDirectory(prefix="t64-audio-hosted-pair-") as temporary:
            public = Path(temporary) / "audio"
            public.mkdir()
            for job, platform in (("gate-linux", "linux"), ("gate-macos", "macos")):
                projection = _projection()
                projection.update({
                    "evidence_mode": "hosted-gate",
                    "workflow_sha": "c" * 40,
                    "source_revision": "c" * 40,
                    "job_name": job,
                    "run_id": "123",
                    "run_attempt": "1",
                    "platform": platform,
                })
                projection["producer"]["source_revision"] = "c" * 40
                projection["platform_witness"]["platform"] = platform
                job_dir = public / job
                job_dir.mkdir()
                (job_dir / "manifest.json").write_text(json.dumps(projection))
            result = qualify._verify_public_pair(public, sha="c" * 40, run="123", attempt="1")
            self.assertEqual(set(result), {"gate-linux", "gate-macos"})
            (public / "gate-macos/manifest.json").unlink()
            with self.assertRaises(qualify.Refusal):
                qualify._verify_public_pair(public, sha="c" * 40, run="123", attempt="1")

    def test_combined_upload_uses_exact_pii_scan_layout(self) -> None:
        with tempfile.TemporaryDirectory(prefix="t64-audio-pii-layout-") as temporary:
            root = Path(temporary)
            combined = root / "combined"
            for job in ("gate-linux", "gate-macos"):
                (combined / "rdp-vi" / job).mkdir(parents=True)
                (combined / "audio" / job).mkdir(parents=True)
                (combined / f"{job}.json").write_text("{}\n")
                (combined / "rdp-vi" / job / "manifest.json").write_text("{}\n")
                (combined / "audio" / job / "manifest.json").write_text("{}\n")
            destination = root / "pii-upload-layout"
            qualify._build_pii_upload_layout(combined, destination)
            expected = {
                "gate-linux/gate-linux.json", "gate-linux/rdp-vi/gate-linux/manifest.json",
                "gate-linux/audio/gate-linux/manifest.json", "gate-macos/gate-macos.json",
                "gate-macos/rdp-vi/gate-macos/manifest.json",
                "gate-macos/audio/gate-macos/manifest.json",
            }
            actual = {
                path.relative_to(destination).as_posix()
                for path in destination.rglob("*") if path.is_file()
            }
            self.assertEqual(actual, expected)
            for job in ("gate-linux", "gate-macos"):
                for category in ("audio", "rdp-vi"):
                    source = combined / category / job / "manifest.json"
                    target = destination / job / category / job / "manifest.json"
                    self.assertEqual(target.read_bytes(), source.read_bytes())
            with self.assertRaises(qualify.Refusal):
                qualify._build_pii_upload_layout(combined, destination)

    def test_final_evidence_cli_accepts_explicit_context_and_refuses_missing_record(self) -> None:
        result = subprocess.run(
            [sys.executable, "-B", str(QUALIFIER), "--verify-evidence",
             "--repo", "szTheory/tendies64", "--sha", "c" * 40,
             "--run", "123", "--attempt", "1",
             "--native-record", str(ROOT / "missing-native-record.json"),
             "--hosted-record", str(ROOT / "missing-hosted-record.json")],
            cwd=ROOT, env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
            capture_output=True, text=True, timeout=5, check=False,
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("hosted record is outside the private qualification tree", result.stderr)
        self.assertNotIn("unrecognized arguments", result.stderr)

    def test_phase08_gcc_closure_units_and_frame_policy(self) -> None:
        sources = sorted(qualify.PHASE08_GCC_REQUIRED_UNITS)
        rdp = qualify._rdp_helpers()
        command = rdp._gcc13_closure_command(sources, qualify.EXPECTED_GCC13_IMAGE)
        self.assertEqual(qualify._validate_gcc13_closure(sources, command), sources)
        with self.assertRaises(qualify.Refusal):
            qualify._validate_gcc13_closure(
                [source for source in sources if source != "runner/t64_raw_capture.c"], command,
            )
        wrong_image = rdp._gcc13_closure_command(sources, "sha256:" + "0" * 64)
        with self.assertRaises(qualify.Refusal):
            qualify._validate_gcc13_closure(sources, wrong_image)

    def test_qualification_header_refuses_smoke_or_incomplete_suites(self) -> None:
        self.assertEqual(qualify._validate_owner_exclusion_scope(
            ["AGENTS.md", ".planning/config.json"]),
            ["AGENTS.md", ".planning/config.json"],
        )
        with self.assertRaises(qualify.Refusal):
            qualify._validate_owner_exclusion_scope(["core/t64_ai.c"])
        inventory = qualify._validate_inventory(
            qualify._read(qualify.INVENTORY, 128 * 1024),
            qualify.EXPECTED_INVENTORY_SHA256,
        )
        base: dict[str, Any] = {
            "schema": "t64.audio-native-qualification",
            "schema_version": 1,
            "evidence_mode": "native-qualification",
            "status": "complete",
            "source_sha": "c" * 40,
            "source_snapshot": {},
            "excluded_owner_metadata": [
                {"path": path, "present": True, "dirty_status": "", "size_bytes": 1,
                 "sha256": "e" * 64}
                for path in qualify.OWNER_METADATA_PATHS
            ],
            "normal_full_ctest_junit_path": "/private/normal-full-ctest-junit.xml",
            "inventory": inventory,
            "shared_native_path": "/private/native",
            "shared_native_record_sha256": "a" * 64,
            "shared_native_tree_manifest": [{"path": "source-binding.json"}],
            "shared_native_closure": {
                "units": sorted(qualify.PHASE08_GCC_REQUIRED_UNITS),
                "gcc13_command": ["docker", "run"],
            },
            "steps_path": "/private/steps",
            "suites": {"normal": {}, "asan": {}},
        }
        qualify._validate_native_record_header(base, "c" * 40)
        smoke = dict(base)
        smoke["evidence_mode"] = "entry-path-validation"
        with self.assertRaises(qualify.Refusal):
            qualify._validate_native_record_header(smoke, "c" * 40)
        missing_asan = dict(base)
        missing_asan["suites"] = {"normal": {}}
        with self.assertRaises(qualify.Refusal):
            qualify._validate_native_record_header(missing_asan, "c" * 40)
        reduced = dict(base)
        reduced["inventory"] = dict(inventory, case_count=inventory["case_count"] - 1)
        with self.assertRaises(qualify.Refusal):
            qualify._validate_native_record_header(reduced, "c" * 40)

    def test_native_command_plan_explicit_builds(self) -> None:
        normal_junit = ROOT / "build/gate/08-05-audio-private/unit/normal-junit.xml"
        plan = qualify._native_command_plan("08-05-unit", normal_junit)
        self.assertEqual([item["label"] for item in plan], [
            "shared-native-gates", "normal-audio-qualification", "asan-audio-qualification",
            "normal-source-body-check", "asan-source-body-check",
        ])
        self.assertEqual(plan[0]["command"][-1], "--native")
        self.assertEqual(plan[0]["environment"]["T64_GATE_JUNIT_OUT"], str(normal_junit))
        self.assertEqual(plan[1]["command"][plan[1]["command"].index("--build") + 1],
                         str((ROOT / "build/gate").resolve()))
        self.assertEqual(plan[2]["command"][plan[2]["command"].index("--build") + 1],
                         str((ROOT / "build/asan").resolve()))
        self.assertEqual(plan[1]["command"][plan[1]["command"].index("--suite") + 1],
                         "qualification")
        self.assertEqual(plan[2]["command"][plan[2]["command"].index("--suite") + 1],
                         "qualification")
        self.assertNotIn("--run", plan[1]["command"])
        self.assertNotIn("--run", plan[2]["command"])

    def test_gate_junit_hook_is_private_and_opt_in(self) -> None:
        gate = (ROOT / "scripts/gate.sh").read_text(encoding="utf-8")
        self.assertIn('if [ -n "${T64_GATE_JUNIT_OUT:-}" ]; then', gate)
        self.assertIn('"$ROOT"/build/gate/08-05-audio-private/*)', gate)
        self.assertIn('step ctest ctest --preset gate --output-junit "$T64_GATE_JUNIT_OUT"', gate)
        self.assertIn('step ctest ctest --preset gate\nfi', gate)

    def test_exact_hosted_context_bound(self) -> None:
        value = _projection()
        value.update({
            "evidence_mode": "hosted-gate",
            "workflow_sha": "c" * 40,
            "job_name": "gate-linux",
            "run_id": "123",
            "run_attempt": "2",
        })
        value["platform_witness"]["platform"] = "linux"
        qualify._validate_hosted_context(
            value, repo="szTheory/tendies64", sha="c" * 40,
            run="123", attempt="2", job="gate-linux",
        )
        with self.assertRaises(qualify.Refusal):
            qualify._validate_hosted_context(
                value, repo="szTheory/tendies64", sha="c" * 40,
                run="123", attempt="3", job="gate-linux",
            )
        with self.assertRaises(qualify.Refusal):
            qualify._validate_hosted_context(
                value, repo="szTheory/tendies64", sha="c" * 40,
                run="123", attempt="2", job="gate-macos",
            )
        forged_literal = _projection()
        forged_literal["capture"]["pcm_literal_sha256"] = "a" * 64
        with self.assertRaises(qualify.Refusal):
            qualify._public_projection(forged_literal)

    def test_accepted_original_binding(self) -> None:
        value = qualify._public_projection(_projection(), {
            "job": "local", "sha": None, "run": None, "attempt": None,
        })
        self.assertEqual(value["evidence_mode"], "entry-path-validation")
        self.assertEqual(value["source_revision"], "b" * 40)

    def test_changed_sample_rejected(self) -> None:
        samples = bytearray(qualify.PCM)
        samples[0] ^= 1
        with self.assertRaises(qualify.Refusal):
            qualify._verify_pcm_literal(bytes(samples))

    def test_wrong_profile_build_input_rejected(self) -> None:
        value = _projection()
        value["profile"]["profile_sha256"] = "e" * 64
        with self.assertRaises(qualify.Refusal):
            qualify._public_projection(value)
        value = _projection()
        value["fixture"]["input_sha256"] = "f" * 64
        with self.assertRaises(qualify.Refusal):
            qualify._public_projection(value)
        result = {
            "profile_id": qualify.PROFILE_ID,
            "profile_digest": qualify.PROFILE_SHA256,
            "fixture_id": qualify.FIXTURE_ID,
            "fixture_sha256": qualify.FIXTURE_SHA256,
            "input_digest": qualify.INPUT_SHA256,
            "source_revision": "b" * 40,
            "build_id": "t64-build-v1-" + "c" * 64,
        }
        manifest = {
            "profile_id": qualify.PROFILE_ID,
            "profile_sha256": qualify.PROFILE_SHA256,
            "fixture_id": qualify.FIXTURE_ID,
            "fixture_sha256": qualify.FIXTURE_SHA256,
            "input_digest": qualify.INPUT_SHA256,
            "source_revision": "b" * 40,
            "core_build_id": result["build_id"],
            "runner_build_id": "t64-runner-v1-" + "d" * 64,
        }
        qualify._verify_capture_identities(result, manifest)
        manifest["core_build_id"] = "t64-build-v1-" + "e" * 64
        with self.assertRaises(qualify.Refusal):
            qualify._verify_capture_identities(result, manifest)

    def test_missing_zero_truncated_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="t64-audio-packets-") as temporary:
            root = Path(temporary)
            with self.assertRaises(qualify.Refusal):
                qualify._packet_file(root / "missing.s16be", 16)
            zero = root / "zero.s16be"
            zero.write_bytes(b"")
            with self.assertRaises(qualify.Refusal):
                qualify._packet_file(zero, 16)
            truncated = root / "truncated.s16be"
            truncated.write_bytes(qualify.PCM[:8])
            with self.assertRaises(qualify.Refusal):
                qualify._packet_file(truncated, 16)

    def test_zero_or_omitted_case_resealed_rejected(self) -> None:
        original = json.loads(qualify._read(qualify.INVENTORY, 128 * 1024))
        denominator = qualify._validate_inventory(
            qualify._read(qualify.INVENTORY, 128 * 1024),
            qualify.EXPECTED_INVENTORY_SHA256,
        )
        self.assertGreater(denominator["case_count"], 0)
        mutations = []
        omitted = json.loads(json.dumps(original))
        omitted["suites"]["qualification"]["binaries"][0]["cases"].pop()
        mutations.append(omitted)
        empty = json.loads(json.dumps(original))
        empty["suites"]["qualification"]["binaries"][0]["cases"] = []
        mutations.append(empty)
        unknown = json.loads(json.dumps(original))
        unknown["suites"]["qualification"]["binaries"][0]["cases"][0] = "unknown-case"
        mutations.append(unknown)
        duplicate = json.loads(json.dumps(original))
        cases = duplicate["suites"]["qualification"]["binaries"][0]["cases"]
        cases[1] = cases[0]
        mutations.append(duplicate)
        for mutation in mutations:
            resealed = json.dumps(mutation, separators=(",", ":")).encode()
            with self.assertRaises(qualify.Refusal):
                qualify._validate_inventory(resealed, qualify.EXPECTED_INVENTORY_SHA256)

    def test_stale_or_missing_command_result_rejected(self) -> None:
        with self.assertRaises(qualify.Refusal):
            qualify._parse_command("1/1 Testing: runner.raw-capture\nTest Passed.\n")
        result = {"outcome": "pass", "detail": "positive-match",
                  "guest_tick": "551001", "state_digest": "a" * 64}
        producer = dict(result)
        producer["state_digest"] = "b" * 64
        with self.assertRaises(qualify.Refusal):
            qualify._reports_agree(result, producer)

    def test_raw_upload_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="t64-audio-public-") as temporary:
            public = Path(temporary)
            (public / "manifest.json").write_text(json.dumps(_projection()))
            qualify._scan_public(public)
            (public / "audio").mkdir()
            (public / "audio/packet-000000.s16be").write_bytes(qualify.PCM[:16])
            with self.assertRaises(qualify.Refusal):
                qualify._scan_public(public)

    def test_pii_projection_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="t64-audio-public-") as temporary:
            public = Path(temporary)
            value = _projection()
            value["profile"]["profile_id"] = "/" + "Users/" + "private/profile"
            (public / "manifest.json").write_text(json.dumps(value))
            with self.assertRaises(qualify.Refusal):
                qualify._scan_public(public)

    def test_hosted_platform_witness_required(self) -> None:
        value = _projection()
        value["platform_witness"] = None
        with self.assertRaises(qualify.Refusal):
            qualify._public_projection(value)
        value = _projection()
        value["platform_witness"]["platform"] = "macos"
        with self.assertRaises(qualify.Refusal):
            qualify._public_projection(value)
        value = _projection()
        with self.assertRaises(qualify.Refusal):
            qualify._public_projection(value, {
                "job": "gate-linux", "sha": "c" * 40,
                "run": "123", "attempt": "1",
            })
        value = _projection()
        value["inventory"]["case_count"] -= 1
        with self.assertRaises(qualify.Refusal):
            qualify._public_projection(value, {
                "job": "gate-linux", "sha": "c" * 40,
                "run": "123", "attempt": "1",
            })


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1] == "--layout-fixture":
        import argparse

        parser = argparse.ArgumentParser(description="write a synthetic archive-layout fixture")
        parser.add_argument("--job", choices=("gate-linux", "gate-macos"), required=True)
        parser.add_argument("--sha", required=True)
        parser.add_argument("--run", required=True)
        parser.add_argument("--attempt", required=True)
        parser.add_argument("--out", type=Path, required=True)
        args = parser.parse_args(sys.argv[2:])
        value = _projection()
        platform = "linux" if args.job == "gate-linux" else "macos"
        value.update({
            "evidence_mode": "hosted-gate",
            "workflow_sha": args.sha,
            "source_revision": args.sha,
            "job_name": args.job,
            "run_id": args.run,
            "run_attempt": args.attempt,
            "platform": platform,
        })
        value["producer"]["source_revision"] = args.sha
        value["platform_witness"]["platform"] = platform
        qualify._public_projection(value, {
            "job": args.job, "sha": args.sha, "run": args.run, "attempt": args.attempt,
        })
        args.out.mkdir(parents=True, exist_ok=False)
        (args.out / "manifest.json").write_text(
            json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n",
            encoding="utf-8",
        )
        print("audio qualification: wrote test-only synthetic archive-layout fixture")
        return 0
    if len(sys.argv) == 2 and sys.argv[1] == "--list-cases":
        print("\n".join(CASE_NAMES))
        return 0
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        for name in CASE_NAMES:
            print(f"case reached: {name}")
        sys.argv = [sys.argv[0], "-v"]
    elif len(sys.argv) != 1:
        print("usage: test_audio_qualification.py [--list-cases|--selftest]",
              file=sys.stderr)
        return 2
    unittest.main(verbosity=2)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
