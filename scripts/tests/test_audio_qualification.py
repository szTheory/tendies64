#!/usr/bin/env python3
"""Exercise the Phase 08 authored-evidence qualification controls."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
INVENTORY = ROOT / "docs/spec/audio-cases.json"


class AudioQualificationTests(unittest.TestCase):
    def test_original_host_error_guest_state_case_is_explicitly_reached(self) -> None:
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        cases = [
            case
            for suite in inventory["suites"].values()
            for binary in suite["binaries"]
            if binary["path"] == "runner/test_audio_capture"
            for case in binary["cases"]
        ]
        self.assertIn(
            "host-error-guest-state",
            cases,
            "the original capture-error guest-state contract needs its own reached case",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
