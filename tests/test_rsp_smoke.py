"""End-to-end tests for the authored, rights-cleared RSP smoke ROM."""

import subprocess
import unittest


RUNNER = "build/gate/runner/t64-rsp-smoke"
ROM = "build/fixtures/rsp-smoke/rsp-smoke.z64"
ROM_SHA256 = "bb08e97a86c6b2eff47f0aea1106da2b907f04dc1073b5a887207dd783b280a5"


class RspSmokeTests(unittest.TestCase):
    def test_authored_guest_finishes_through_cpu_sp_rsp(self):
        result = subprocess.run(
            [RUNNER, "--run", ROM, ROM_SHA256],
            capture_output=True,
            check=False,
            text=True,
            timeout=10,
        )
        self.assertEqual(
            0,
            result.returncode,
            f"runner exit={result.returncode}\n{result.stdout}{result.stderr}",
        )
        self.assertIn("classification=pass", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
