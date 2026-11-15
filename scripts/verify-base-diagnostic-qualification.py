#!/usr/bin/env python3
"""Independent private pre-use gate for the whole-BASE diagnostic context."""

from __future__ import annotations

import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]


class Refusal(ValueError):
    pass


def verify_and_dispatch(context_root: pathlib.Path, run_directory: pathlib.Path,
                        dispatch, repository_root: pathlib.Path = ROOT) -> bool:
    """Verify the complete immutable context before invoking normal dispatch."""
    del context_root, run_directory, dispatch, repository_root
    return False


def main() -> int:
    print("REFUSED: qualification entry not implemented")
    return 4


if __name__ == "__main__":
    raise SystemExit(main())
