#!/usr/bin/env python3
"""Qualify authored RDP/VI output and bind records to their original run."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def verify_evidence(directory: Path) -> bool:
    """Initial verifier seam; strict artifact binding is added after RED."""
    try:
        metadata = json.loads((directory / "observation.json").read_text(encoding="utf-8"))
        source = (directory / "rdram.bin").read_bytes()
        packet = (directory / "vi-packet.bin").read_bytes()
    except (OSError, json.JSONDecodeError):
        return False
    return (
        metadata.get("schema") == "t64.rdp-vi-observation"
        and metadata.get("schema_version") == 1
        and len(source) == metadata.get("source_bytes")
        and len(packet) == metadata.get("packet_bytes")
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify-evidence", type=Path, metavar="DIR")
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--native", action="store_true")
    parser.add_argument("--collect-hosted", action="store_true")
    args = parser.parse_args()
    if args.verify_evidence is not None:
        if not verify_evidence(args.verify_evidence):
            print("RDP/VI evidence is missing or inconsistent", file=sys.stderr)
            return 1
        print("RDP/VI evidence accepted")
        return 0
    if args.selftest:
        print("RDP/VI qualification selftest is not implemented", file=sys.stderr)
        return 1
    if args.native:
        print("RDP/VI native qualification is not implemented", file=sys.stderr)
        return 2
    if args.collect_hosted:
        print("RDP/VI hosted collection is not implemented", file=sys.stderr)
        return 2
    parser.error("select --selftest, --native, --collect-hosted, or --verify-evidence DIR")


if __name__ == "__main__":
    raise SystemExit(main())
