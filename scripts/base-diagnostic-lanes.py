#!/usr/bin/env python3
"""Fixed local lane selections over the independently qualified BASE runner."""
from __future__ import annotations

import importlib.util
import pathlib

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
    # No lane is dispatched until its full-entry contract is implemented.
    return 4


if __name__ == "__main__":
    raise SystemExit(main())
