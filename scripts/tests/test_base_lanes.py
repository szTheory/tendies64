#!/usr/bin/env python3
"""Authored selection controls; full real-C entry is in test_base_run.py."""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
import pathlib
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from test_base_run import load_runner

ROOT = pathlib.Path(__file__).resolve().parents[2]


def load_wrapper():
    spec = importlib.util.spec_from_file_location("t64_base_lanes_test", ROOT / "scripts/base-diagnostic-lanes.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class LaneControls(unittest.TestCase):
    def fixture(self, root, members, *, code=0, interrupted=False):
        wrapper, helpers = load_wrapper(), load_runner()
        private = root / "private"
        private.mkdir(mode=0o700)
        wrapper.ROOT, wrapper.PRIVATE = root, private
        calls = []
        batch = private / "batch-authored"
        def dispatch(context, adapter, *, lane):
            calls.append((context, adapter, lane))
            batch.mkdir(mode=0o700)
            helpers._write_private(private / f"{lane}-dispatch-{'0' * 64}.json",
                                   helpers.canonical({"batch": batch.name}))
            helpers._write_private(batch / "classification.json", helpers.canonical({
                "schema": "t64.base-classification", "schema_version": 1,
                "context_sha256": "0" * 64, "members": members}))
            return {"context_sha256": "0" * 64, "classification_report": "classification.json",
                    "classification_exit_code": code, "interrupted": interrupted}
        runner = SimpleNamespace(run_required=dispatch, _private_bytes=helpers._private_bytes,
            _canonical_object=helpers._canonical_object, _write_private=helpers._write_private,
            canonical=helpers.canonical)
        return wrapper, runner, calls, batch

    def test_fixed_lane_routes_and_unexecuted_receipts(self):
        for lane in ("required", "informational", "stress"):
            with self.subTest(lane=lane), tempfile.TemporaryDirectory(prefix="lane-route-") as temp:
                root = pathlib.Path(temp)
                wrapper, runner, calls, batch = self.fixture(root, [
                    {"required": True, "lane": "required-base", "outcome": "unexecuted"}])
                with patch.object(wrapper, "_load_runner", return_value=runner), patch.dict(
                        os.environ, {"CI": "", "T64_REQUIRE_FIXTURES": "0"}):
                    self.assertEqual(wrapper.main([f"--run-{lane}", "--local-only"]), 0)
                self.assertEqual(calls, [(wrapper.PRIVATE,
                    root / "build/gate/runner/t64-base-diagnostic", lane)])
                receipt = json.loads((batch / "lane-invocation.json").read_bytes())
                self.assertFalse(receipt["authenticated_terminal_observed"])
                self.assertEqual(receipt["lane"], lane)
                self.assertEqual(receipt["schedule_kind"],
                    "owner-invoked-local-command" if lane == "stress" else "manual-local-command")

    def test_foreign_lane_terminal_cannot_be_promoted(self):
        for lane, foreign in (("informational", "required-base"), ("stress", "informational"),
                              ("required", "stress")):
            with self.subTest(lane=lane), tempfile.TemporaryDirectory(prefix="lane-isolation-") as temp:
                wrapper, runner, _, batch = self.fixture(pathlib.Path(temp), [
                    {"required": foreign == "required-base", "lane": foreign, "outcome": "pass"}])
                with patch.object(wrapper, "_load_runner", return_value=runner), patch.dict(
                        os.environ, {"CI": "", "T64_REQUIRE_FIXTURES": "0"}):
                    self.assertEqual(wrapper.main([f"--run-{lane}", "--local-only"]), 0)
                self.assertFalse(json.loads((batch / "lane-invocation.json").read_bytes())[
                    "authenticated_terminal_observed"])

    def test_only_c_authenticated_terminal_marks_observation(self):
        for outcome, expected in (("pass", True), ("fail", True), ("harness-error", False),
                                  ("guest-stop", False), ("blocked", False)):
            with self.subTest(outcome=outcome), tempfile.TemporaryDirectory(prefix="lane-observed-") as temp:
                wrapper, runner, _, batch = self.fixture(pathlib.Path(temp), [
                    {"required": False, "lane": "stress", "outcome": outcome}])
                with patch.object(wrapper, "_load_runner", return_value=runner), patch.dict(
                        os.environ, {"CI": "", "T64_REQUIRE_FIXTURES": "0"}):
                    self.assertEqual(wrapper.main(["--run-stress", "--local-only"]), 0)
                self.assertEqual(json.loads((batch / "lane-invocation.json").read_bytes())[
                    "authenticated_terminal_observed"], expected)

    def test_ci_fixture_and_missing_local_authority_refuse_before_loading(self):
        cases = (([], {"CI": "", "T64_REQUIRE_FIXTURES": "0"}),
                 (["--local-only"], {"CI": "true", "T64_REQUIRE_FIXTURES": "0"}),
                 (["--local-only"], {"CI": "", "T64_REQUIRE_FIXTURES": "1"}))
        wrapper = load_wrapper()
        for flags, env in cases:
            with self.subTest(flags=flags, env=env), patch.object(wrapper, "_load_runner") as load, patch.dict(
                    os.environ, env), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(wrapper.main(["--run-stress", *flags]), 4)
                load.assert_not_called()

    def test_arbitrary_profile_selector_and_experimental_requests_refuse(self):
        wrapper = load_wrapper()
        for args in (["--run-experimental", "--local-only"],
                     ["--run-required", "--local-only", "--profile", "foreign"],
                     ["--run-stress", "--local-only", "--selector", "foreign"],
                     ["--run-required", "--run-stress", "--local-only"]):
            with self.subTest(args=args), patch.object(wrapper, "_load_runner") as load, contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    wrapper.main(args)
                self.assertEqual(error.exception.code, 2)
                load.assert_not_called()

    def test_qualification_refusal_is_retained_as_refusal(self):
        wrapper = load_wrapper()
        runner = SimpleNamespace(run_required=lambda *args, **kwargs: (_ for _ in ()).throw(ValueError("authored pre-use refusal")))
        with patch.object(wrapper, "_load_runner", return_value=runner), patch.dict(
                os.environ, {"CI": "", "T64_REQUIRE_FIXTURES": "0"}), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(wrapper.main(["--run-informational", "--local-only"]), 4)


if __name__ == "__main__":
    unittest.main(verbosity=2)
