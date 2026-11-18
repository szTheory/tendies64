from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
QUALIFIER = ROOT / "scripts/qualify-rdp-vi.py"


class RdpViQualificationTests(unittest.TestCase):
    def test_actual_producer_and_mutation_controls_through_final_cli(self) -> None:
        result = subprocess.run(
            [sys.executable, "-B", str(QUALIFIER), "--selftest"],
            cwd=ROOT, capture_output=True, text=True, timeout=90, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("actual accepted producer", result.stdout)
        self.assertIn("reached wrong-pixel comparator", result.stdout)
        self.assertIn("subprocess deadline", result.stdout)
        self.assertIn("forged original-record controls passed", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
