from __future__ import annotations

import subprocess
import sys
import unittest
import json
import os
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
QUALIFIER = ROOT / "scripts/qualify-rdp-vi.py"


class RdpViQualificationTests(unittest.TestCase):
    def test_actual_hosted_emitter_entrypoint(self) -> None:
        # Isolate publication storage, while retaining the real CLI dispatch,
        # source tree, fixture, binaries, producer and original-record verifier.
        # Caller-supplied workflow identities make this a local control only.
        with tempfile.TemporaryDirectory(
                prefix="emitter-entry-", dir=ROOT / "build/gate") as temporary:
            publication = Path(temporary) / "public"
            source = subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
            launcher = (
                "import importlib.util, pathlib, sys; "
                "spec=importlib.util.spec_from_file_location('rdp_qualifier',sys.argv[1]); "
                "module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module); "
                "module.PUBLIC_ROOT=pathlib.Path(sys.argv[2]); "
                "sys.argv=[sys.argv[1],'--emit-hosted','--job','gate-linux','--sha',sys.argv[3]]; "
                "raise SystemExit(module.main())"
            )
            result = subprocess.run(
                [sys.executable, "-B", "-c", launcher, str(QUALIFIER), str(publication), source],
                cwd=ROOT, capture_output=True, text=True, timeout=30, check=False,
                env={**os.environ, "GITHUB_JOB": "gate-linux", "GITHUB_SHA": source,
                     "GITHUB_RUN_ID": "123", "GITHUB_RUN_ATTEMPT": "1",
                     "PYTHONDONTWRITEBYTECODE": "1"},
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("emitted authored raw evidence", result.stdout)
            output = publication / "gate-linux/rdp-vi/gate-linux"
            self.assertEqual({p.name for p in output.iterdir()}, {
                "manifest.json", "inventory.json", "observation.json", "raw-export.json"})
            manifest = json.loads((output / "manifest.json").read_text())
            self.assertEqual(manifest["commit_sha"], source)
            self.assertEqual(manifest["run_id"], "123")
            self.assertEqual(manifest["run_attempt"], "1")
            self.assertEqual(manifest["job_name"], "gate-linux")

    def test_actual_producer_and_mutation_controls_through_final_cli(self) -> None:
        result = subprocess.run(
            [sys.executable, "-B", str(QUALIFIER), "--selftest"],
            cwd=ROOT, capture_output=True, text=True, timeout=90, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("actual accepted producer", result.stdout)
        self.assertIn("reached wrong-pixel comparator", result.stdout)
        self.assertIn("subprocess deadline", result.stdout)
        self.assertIn("child/grandchild", result.stdout)
        self.assertIn("native source binding control", result.stdout)
        self.assertIn("actual hosted emitter entrypoint", result.stdout)
        self.assertIn("forged original-record controls passed", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
