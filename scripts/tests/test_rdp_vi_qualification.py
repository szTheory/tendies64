from __future__ import annotations

import os
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build/gate"
PRODUCER = BUILD / "runner/test_rdp_video"
SUPERVISOR = BUILD / "runner/t64-supervisor"
CHECKER = BUILD / "runner/t64-result-check"
FIXTURE = ROOT / "build/fixtures/rdp-fill.z64"
INPUT = ROOT / "build/fixtures/hello.input.json"
QUALIFIER = ROOT / "scripts/qualify-rdp-vi.py"


class RdpViQualificationTests(unittest.TestCase):
    def test_actual_original_is_accepted_and_swapped_export_is_rejected(self) -> None:
        required = (PRODUCER, SUPERVISOR, CHECKER, FIXTURE, INPUT)
        missing = [str(path.relative_to(ROOT)) for path in required if not path.is_file()]
        self.assertEqual(missing, [], f"build the gate preset first: {missing}")

        run_root = Path(tempfile.mkdtemp(prefix="rdp-vi-qualification-red-", dir=BUILD))
        run_root.chmod(0o700)
        evidence = run_root / "original"
        evidence.mkdir(mode=0o700)
        environment = os.environ.copy()
        environment["T64_RDP_VI_EXPORT_DIR"] = str(evidence)
        command = [
            str(PRODUCER),
            "accepted-cpu-fill-to-raw-frame",
            str(SUPERVISOR),
            str(CHECKER),
            str(FIXTURE),
            str(INPUT),
        ]
        (run_root / "command.json").write_text(
            json.dumps({"argv": command, "cwd": str(ROOT), "export_dir": str(evidence)},
                       sort_keys=True) + "\n", encoding="utf-8",
        )
        result = subprocess.run(
            command, cwd=ROOT, env=environment, capture_output=True,
            text=True, timeout=10, check=False,
        )
        (run_root / "stdout.log").write_text(result.stdout, encoding="utf-8")
        (run_root / "stderr.log").write_text(result.stderr, encoding="utf-8")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for name in ("observation.json", "rdram.bin", "vi-packet.bin"):
            self.assertTrue((evidence / name).is_file(), f"real producer omitted {name}")

        def digest(path: Path) -> str:
            return hashlib.sha256(path.read_bytes()).hexdigest()

        (run_root / "outcome.json").write_text(
            json.dumps({
                "exit_code": result.returncode,
                "timed_out": False,
                "stdout_sha256": digest(run_root / "stdout.log"),
                "stderr_sha256": digest(run_root / "stderr.log"),
                "executable_sha256": digest(PRODUCER),
                "fixture_sha256": digest(FIXTURE),
                "input_sha256": digest(INPUT),
                "artifacts": {name: digest(evidence / name) for name in
                              ("observation.json", "rdram.bin", "vi-packet.bin")},
            }, sort_keys=True) + "\n", encoding="utf-8",
        )

        accepted = subprocess.run(
            [sys.executable, "-B", str(QUALIFIER), "--verify-evidence", str(evidence)],
            cwd=ROOT, capture_output=True, text=True, timeout=5, check=False,
        )
        self.assertEqual(accepted.returncode, 0, accepted.stdout + accepted.stderr)

        packet = evidence / "vi-packet.bin"
        packet.write_bytes((evidence / "rdram.bin").read_bytes() * 2)
        forged = subprocess.run(
            [sys.executable, "-B", str(QUALIFIER), "--verify-evidence", str(evidence)],
            cwd=ROOT, capture_output=True, text=True, timeout=5, check=False,
        )
        self.assertNotEqual(
            forged.returncode, 0,
            "verifier accepted a swapped raw export after accepting the actual producer record",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
