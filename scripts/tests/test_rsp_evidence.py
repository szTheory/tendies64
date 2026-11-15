#!/usr/bin/env python3
"""Authored controls for the final RSP evidence checker."""
from __future__ import annotations

import importlib.util
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/check-rsp-evidence.py"
SPEC = importlib.util.spec_from_file_location("check_rsp_evidence", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
CHECKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECKER)


def clean_env() -> dict[str, str]:
    return {
        "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
        "LC_ALL": "C",
        "LANG": "C",
        "PYTHONDONTWRITEBYTECODE": "1",
    }


def invoke(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, "-B", str(SCRIPT), *args], cwd=ROOT, env=clean_env(),
        check=False, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    )


class RspEvidenceTests(unittest.TestCase):
    def test_additional_registered_sanitizer_regressions_must_pass(self) -> None:
        log = (
            "1/2 Test   #1: required.case .... Passed 0.01 sec\n"
            "2/2 Test #2: regression.case .... Passed 0.01 sec\n"
            "100% tests passed out of 2\n"
        )
        allowed = {"required.case", "regression.case"}
        CHECKER.validate_ctest_log(log, {"required.case"}, allowed=allowed)
        for invalid in (
            log.replace("regression.case", "invented.case"),
            log.replace("regression.case .... Passed", "regression.case .... Failed"),
        ):
            with self.assertRaises(CHECKER.Refusal):
                CHECKER.validate_ctest_log(invalid, {"required.case"}, allowed=allowed)
        with self.assertRaises(CHECKER.Refusal):
            CHECKER.validate_ctest_log(log, {"required.case"})

    def test_quoted_private_fields_are_rejected(self) -> None:
        for leak in (
            b'{"inventory_sha256":"' + b'a' * 64 + b'"}',
            b'{"private_build_id": "' + b'b' * 64 + b'"}',
            b'{"source_name":"authored_private_name"}',
            b'{"classification":"refused"}',
        ):
            with self.subTest(field=leak.split(b':', 1)[0]):
                with self.assertRaises(CHECKER.Refusal):
                    CHECKER.privacy_scan(leak)

    def test_only_the_approved_composition_rom_hash_is_allowed(self) -> None:
        approved = "a" * 64
        CHECKER.privacy_scan(
            ('{"rom_sha256":"' + approved + '"}').encode(),
            approved_rom_hash=approved,
        )
        with self.assertRaises(CHECKER.Refusal):
            CHECKER.privacy_scan(
                ('{"rom_sha256":"' + "b" * 64 + '"}').encode(),
                approved_rom_hash=approved,
            )

    def test_compiled_source_hash_covers_cmake_policy(self) -> None:
        with tempfile.TemporaryDirectory(prefix="rsp-evidence-source.") as temporary:
            root = pathlib.Path(temporary)
            for directory in ("core", "runner", "tools"):
                (root / directory).mkdir()
            (root / "core/softfloat").mkdir()
            (root / "CMakeLists.txt").write_text("project(synthetic)\n", encoding="utf-8")
            (root / "CMakePresets.json").write_text("{}\n", encoding="utf-8")
            policy = root / "core/softfloat/sources.cmake"
            policy.write_text("set(SOURCES synthetic.c)\n", encoding="utf-8")
            before = CHECKER.source_content_sha256(root)
            policy.write_text("set(SOURCES synthetic.c other.c)\n", encoding="utf-8")
            self.assertNotEqual(before, CHECKER.source_content_sha256(root))

    def test_authored_baseline_and_all_evidence_mutations(self) -> None:
        result = invoke("--selftest")
        self.assertEqual(result.returncode, 0, "authored evidence selftest failed:\n" + result.stdout)
        self.assertEqual(result.stdout.strip(), "PASS: authored RSP final-evidence controls")

    def test_last_tested_source_is_only_a_full_existing_revision(self) -> None:
        result = invoke("--last-tested-source")
        self.assertEqual(result.returncode, 0, "saved hosted source was not resolved:\n" + result.stdout)
        revision = result.stdout.strip()
        self.assertRegex(revision, r"^[0-9a-f]{40}$")
        exists = subprocess.run(
            ["git", "-C", str(ROOT), "cat-file", "-e", f"{revision}^{{commit}}"],
            check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        self.assertEqual(exists.returncode, 0, "reported source revision is not an existing commit")

    def test_last_tested_source_rejects_failed_or_unpaired_rows(self) -> None:
        with tempfile.TemporaryDirectory(prefix="rsp-evidence-ci.", dir=ROOT / "build/gate") as temporary:
            root = pathlib.Path(temporary)
            log = root / "docs/ci-reviews/LOG.md"
            log.parent.mkdir(mode=0o700, parents=True)
            source = "a" * 12
            log.write_text(
                "| date | run id | sha (12) | event | conclusion |\n"
                "|---|---|---|---|---|\n"
                f"| today | 99999999 | {source} | push | failure |\n"
                f"| F-999 | 99999999 | Both hosts pass {source} | taken | test |\n",
                encoding="utf-8",
            )
            with self.assertRaises(CHECKER.Refusal):
                CHECKER.latest_tested_source(root)
            log.write_text(
                "| date | run id | sha (12) | event | conclusion |\n"
                "|---|---|---|---|---|\n"
                f"| today | 99999999 | {source} | push | success |\n",
                encoding="utf-8",
            )
            with patch.object(CHECKER.subprocess, "run") as run:
                with self.assertRaises(CHECKER.Refusal):
                    CHECKER.latest_tested_source(root)
                run.assert_not_called()


if __name__ == "__main__":
    unittest.main(verbosity=2)
