#!/usr/bin/env python3
"""Authored controls for one bounded BASE member dispatch."""

from __future__ import annotations

import importlib.util
import pathlib
import tempfile
import unittest

from test_base_qualification import create_context

ROOT = pathlib.Path(__file__).resolve().parents[2]
RUNNER = ROOT / "scripts/run-base-diagnostic.py"


def load_runner():
    spec = importlib.util.spec_from_file_location("t64_base_runner", RUNNER)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load production BASE runner")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class BaseRunControls(unittest.TestCase):
    def test_timeout_remains_timeout_after_cleanup_without_retry(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-run-timeout-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            context_root = create_context(repository)
            adapter_path = context_root / "executable.evidence"
            adapter_path.chmod(0o700)
            run_directory = context_root / "run-timeout"
            attempts: list[pathlib.Path] = []
            runner = load_runner()

            def timed_out_attempt(directory: pathlib.Path) -> dict:
                attempts.append(directory)
                return {
                    "cleanup_verified": True,
                    "returncode": 0,
                    "stderr": "",
                    "stdout": "classification=pass\n",
                    "timed_out": True,
                }

            result = runner.run_one(
                context_root, run_directory, adapter_path,
                repository_root=repository, dispatch=timed_out_attempt,
            )
            self.assertEqual(result["status"], "timeout")
            self.assertEqual(result["attempts"], 1)
            self.assertEqual(attempts, [run_directory])
            self.assertTrue(run_directory.is_dir())


if __name__ == "__main__":
    unittest.main(verbosity=2)
