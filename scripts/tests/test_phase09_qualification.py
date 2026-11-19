#!/usr/bin/env python3
"""Controls for the Phase 9 harness guards and the fail-closed qualifier stub.

Cases are listed with --list-cases, run with --selftest (every case, verbose)
or one at a time with --case NAME. No builds and no nested CTest run here.
"""

from __future__ import annotations

import re
import subprocess
import sys
import unittest
from pathlib import Path


_SCRIPT_ROOT = Path(__file__).resolve().parents[2]
_WORKING_ROOT = Path.cwd().resolve()
if (_SCRIPT_ROOT / "scripts/gate.sh").is_file():
    ROOT = _SCRIPT_ROOT
elif (_WORKING_ROOT / "scripts/gate.sh").is_file():
    ROOT = _WORKING_ROOT
else:
    ROOT = _SCRIPT_ROOT

# Every case of this file, in the order --list-cases prints them. Later tasks
# of the same plan append their cases here; the dispatcher below only runs
# names listed in this tuple.
CASE_NAMES: tuple[str, ...] = (
    "gate-junit-hook-private-phase09",
)

# A placeholder root: no real checkout path appears in this file.
PLACEHOLDER_ROOT = "/r"


class Phase09Qualification(unittest.TestCase):
    def test_gate_junit_hook_private_phase09(self) -> None:
        """The N2 arm admits one more private directory and nothing else."""
        gate = (ROOT / "scripts/gate.sh").read_text(encoding="utf-8")
        self.assertIn('if [ -n "${T64_GATE_JUNIT_OUT:-}" ]; then', gate)
        block = re.search(r'  case "\$T64_GATE_JUNIT_OUT" in\n.*?\n  esac\n', gate, re.S)
        self.assertIsNotNone(block)
        assert block is not None
        root = PLACEHOLDER_ROOT
        cases = (
            # Accepted: the Phase 9 private directory, and the Phase 8 one.
            (f"{root}/build/gate/09-11-phase09-private/run/junit.xml", 0),
            (f"{root}/build/gate/08-05-audio-private/run/junit.xml", 0),
            # Refused: a `..` component in the middle and at the end.
            (f"{root}/build/gate/09-11-phase09-private/../x.xml", 2),
            (f"{root}/build/gate/09-11-phase09-private/a/..", 2),
            # Refused: outside both private directories, including near misses.
            (f"{root}/build/gate/other/junit.xml", 2),
            (f"{root}/build/gate/09-11-phase09-privatex/junit.xml", 2),
            (f"{root}/build/gate/09-11-phase09-private", 2),
        )
        failures = []
        for value, expected in cases:
            script = 'ROOT="$1"; T64_GATE_JUNIT_OUT="$2"\n' + block.group(0)
            result = subprocess.run(["sh", "-c", script, "sh", root, value],
                                    capture_output=True, text=True, check=False)
            print(f"gate hook {value} -> exit {result.returncode} (want {expected})",
                  file=sys.stderr)
            if result.returncode != expected:
                failures.append((value, result.returncode, expected))
        self.assertEqual(failures, [])
        # Both private arms are present exactly once; the run lines of the
        # hook and the plain branch are unchanged.
        self.assertEqual(gate.count('"$ROOT"/build/gate/08-05-audio-private/*)'), 1)
        self.assertEqual(gate.count('"$ROOT"/build/gate/09-11-phase09-private/*)'), 1)
        self.assertIn(
            'step ctest ctest --preset gate --output-junit "$T64_GATE_JUNIT_OUT"', gate)
        self.assertIn('step ctest ctest --preset gate\nfi', gate)


def main() -> int:
    args = sys.argv[1:]
    if args == ["--list-cases"]:
        print("\n".join(CASE_NAMES))
        return 0
    if args == ["--selftest"]:
        for name in CASE_NAMES:
            print(f"case reached: {name}")
        sys.argv = [sys.argv[0], "-v"]
        unittest.main(verbosity=2)
        return 0
    if len(args) == 2 and args[0] == "--case" and args[1] in CASE_NAMES:
        method = "test_" + args[1].replace("-", "_")
        suite = unittest.TestSuite([Phase09Qualification(method)])
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        return 0 if result.wasSuccessful() else 1
    print("usage: test_phase09_qualification.py "
          "--list-cases | --selftest | --case NAME", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
