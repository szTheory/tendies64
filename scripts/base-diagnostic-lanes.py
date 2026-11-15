#!/usr/bin/env python3
"""Fixed local lane selections over the independently qualified BASE runner."""
from __future__ import annotations

import importlib.util
import argparse
import hashlib
import os
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "build/private-base-diagnostic"


def _load_runner():
    spec = importlib.util.spec_from_file_location("t64_base_lane_runner", ROOT / "scripts/run-base-diagnostic.py")
    if spec is None or spec.loader is None:
        raise ValueError("normal BASE runner is unavailable")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--run-required", action="store_true")
    mode.add_argument("--run-informational", action="store_true")
    mode.add_argument("--run-stress", action="store_true")
    parser.add_argument("--local-only", action="store_true")
    args = parser.parse_args(argv)
    if (not args.local_only or os.environ.get("CI", "").lower() in {"1", "true", "yes"} or
            os.environ.get("T64_REQUIRE_FIXTURES") == "1"):
        print("BASE lane refused: explicit local-only execution is required", file=sys.stderr)
        return 4
    lane = "required" if args.run_required else "informational" if args.run_informational else "stress"
    try:
        operator_sha = hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest()
        # This operator command supplies only a fixed lane enum. The normal
        # runner owns qualification, membership, limits, dispatch and C reports.
        runner = _load_runner()
        result = runner.run_required(PRIVATE, ROOT / "build/gate/runner/t64-base-diagnostic", lane=lane)
        context_sha = result["context_sha256"]
        marker = runner._canonical_object(runner._private_bytes(
            PRIVATE / f"{lane}-dispatch-{context_sha}.json"))
        batch_name = marker.get("batch")
        report_name = result.get("classification_report")
        if (not isinstance(batch_name, str) or not batch_name.startswith("batch-") or
                pathlib.PurePosixPath(batch_name).name != batch_name or
                not isinstance(report_name, str) or pathlib.PurePosixPath(report_name).name != report_name):
            raise ValueError("normal runner receipt paths are malformed")
        batch = PRIVATE / batch_name
        report = runner._canonical_object(runner._private_bytes(batch / report_name))
        if (report.get("schema") != "t64.base-classification" or report.get("schema_version") != 1 or
                report.get("context_sha256") != context_sha):
            raise ValueError("normal C classification belongs to another context")
        def selected(member):
            if lane == "required":
                return member["required"]
            if lane == "informational":
                return member["lane"] in {"informational", "timing", "cycle", "cop0-hazard"}
            return member["lane"] == "stress"
        # A bound C pass/fail requires actual entry and terminal retirements.
        # A lane definition, attempted setup, or blocked receipt cannot set this.
        terminal = any(selected(member) and member["outcome"] in {"pass", "fail"}
                       for member in report["members"])
        if hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest() != operator_sha:
            raise ValueError("lane operator source changed during invocation")
        receipt = {"schema": "t64.base-lane-invocation", "schema_version": 1,
            "lane": lane, "local_only": True, "context_sha256": context_sha,
            "operator_sha256": operator_sha,
            "classification_report": report_name,
            "classification_exit_code": result["classification_exit_code"],
            "authenticated_terminal_observed": terminal,
            "schedule_kind": "owner-invoked-local-command" if lane == "stress" else "manual-local-command"}
        runner._write_private(batch / "lane-invocation.json", runner.canonical(receipt))
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(f"BASE lane refused: {exc}", file=sys.stderr)
        return 4
    print("classification=local-lane-receipt-retained")
    return 130 if result.get("interrupted") else result["classification_exit_code"]


if __name__ == "__main__":
    raise SystemExit(main())
