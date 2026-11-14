#!/usr/bin/env python3
"""Clean authored controls for RSP build composition receipts."""

from __future__ import annotations

import os
import pathlib
import stat
import subprocess
import sys
import tempfile
import unittest

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
