#!/usr/bin/env python3
"""Clean authored controls for RSP build composition receipts."""

from __future__ import annotations

import os
import importlib.util
import hashlib
import json
import pathlib
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[2]
TEST_ROOT = ROOT / "build/test-rsp-build"
VERIFY = ROOT / "scripts/verify-rsp-diagnostic-build.py"


def owner_only_directory(path: pathlib.Path) -> None:
    path.mkdir(mode=0o700, parents=True, exist_ok=True)
    if path.is_symlink() or not path.is_dir():
        raise AssertionError("RSP build test workspace is linked or not a directory")
    path.chmod(0o700)
    if stat.S_IMODE(path.stat().st_mode) != 0o700:
        raise AssertionError("RSP build test workspace must be owner-only")


def clean_environment() -> dict[str, str]:
    return {
        "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
        "LC_ALL": "C",
        "LANG": "C",
        "PYTHONDONTWRITEBYTECODE": "1",
    }


class RspBuildTests(unittest.TestCase):
    def test_recovery_runs_inspector_instead_of_trusting_an_old_pass(self) -> None:
        spec = importlib.util.spec_from_file_location("rsp_build_recovery", ROOT / "scripts/build-qualified-rsp-diagnostic.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        owner_only_directory(TEST_ROOT)
        with tempfile.TemporaryDirectory(prefix="recovery.", dir=TEST_ROOT) as temporary:
            root = pathlib.Path(temporary)
            private = root / "build/private-rsp-diagnostic"
            work = private / "rsp-build-attempt-authored"
            work.mkdir(parents=True, mode=0o700)
            private.chmod(0o700)
            source, prefix = root / "source", root / "prefix"
            source.mkdir()
            (prefix / "lib/rustlib/src/rust/library").mkdir(parents=True)
            locks = [source / "Cargo.lock", prefix / "lib/rustlib/src/rust/library/Cargo.lock"]
            for path in locks:
                path.write_bytes(b"authored locked resolution")
            lock_sha = hashlib.sha256(locks[0].read_bytes()).hexdigest()
            qualification = private / "qualification.json"
            qualification.write_text(json.dumps({"source_revision": "a" * 40}))
            preuse = {"schema": "t64.rsp-build-attempt.v1", "attempt_id": work.name,
                      "qualification_sha256": hashlib.sha256(qualification.read_bytes()).hexdigest(),
                      "project_lock_before": lock_sha, "sysroot_lock_before": lock_sha,
                      "preuse_closure_sha256": "b" * 64, "core_build_sha256": "c" * 64}
            cargo = {"exit": 0, "timed_out": False, "process_group_empty": True,
                     "deadline_seconds": 600,
                     "argv": ["cargo", "build", "--jobs", "4", "--release", "--target", "mips-nintendo64-none.json", "--offline", "--locked", "--frozen"]}
            target = private / "cargo-target-rsp-attempt-01/mips-nintendo64-none/release/n64-systemtest"
            target.parent.mkdir(parents=True)
            for path in (target, work / "rsp.payload.bin", work / "rsp.z64"):
                path.write_bytes(b"authored changed output")
            for name, value in (("attempt-preuse.json", preuse), ("cargo.command.json", cargo),
                                ("composition.json", {"schema": "t64.rsp-build-composition.v1",
                                                      "status": "PASS_POSTBUILD_COMPOSITION",
                                                      "qualification_sha256": preuse["qualification_sha256"]})):
                (work / name).write_text(json.dumps(value))
            def inspect(command, directory, label, **kwargs):
                self.assertEqual(directory, work)
                self.assertEqual(label, "composition-recovered")
                self.assertIn("--inspect", command)
                raise ValueError("authored inspector rejects changed output")
            with patch.object(module, "ROOT", root), patch.object(module, "PRIVATE", private), \
                    patch.object(module, "SOURCE", source), patch.object(module, "PREFIX", prefix), \
                    patch.object(module, "run", side_effect=inspect) as run:
                with self.assertRaisesRegex(ValueError, "inspector rejects"):
                    module.seal_existing_attempt(work.name)
                run.assert_called_once()
                self.assertFalse((private / "qualified-build.json").exists())
                self.assertFalse((work / "postbuild-inspection.json").exists())

    def test_authored_composition_controls_reject_identity_and_closure_drift(self) -> None:
        owner_only_directory(TEST_ROOT)
        with tempfile.TemporaryDirectory(prefix="run.", dir=TEST_ROOT) as temporary:
            workspace = pathlib.Path(temporary)
            workspace.chmod(0o700)
            result = subprocess.run(
                [sys.executable, "-B", str(VERIFY), "--selftest", "--workspace", str(workspace)],
                cwd=ROOT,
                env=clean_environment(),
                check=False,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=30,
            )
            self.assertEqual(result.returncode, 0, "authored RSP build controls failed:\n" + result.stdout)
            self.assertIn("PASS: authored RSP build composition controls", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
