#!/usr/bin/env python3
"""Run one locally qualified BASE member and retain its original outcome."""

from __future__ import annotations

import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]


class Refusal(ValueError):
    pass


def run_one(context_root: pathlib.Path, run_directory: pathlib.Path,
            adapter_path: pathlib.Path, *, repository_root: pathlib.Path = ROOT,
            dispatch=None) -> dict:
    del context_root, run_directory, adapter_path, repository_root, dispatch
    return {"attempts": 0, "status": "not-run"}


def main() -> int:
    print("REFUSED: BASE member dispatch not implemented")
    return 4


if __name__ == "__main__":
    raise SystemExit(main())
