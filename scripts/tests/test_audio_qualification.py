#!/usr/bin/env python3
"""Exercise the bounded audio evidence verifier without builds or nested CTest."""

from __future__ import annotations

import importlib.util
import json
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
            "pcm_literal_sha256": digest,
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
