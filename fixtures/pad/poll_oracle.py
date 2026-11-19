#!/usr/bin/env python3
"""Independent transcript oracle for the Phase 9 pad-poll diagnostic.

The scenario literals, the controller state encoding and the strictly-after
visibility rule are written here, outside the guest ROM and outside the core.
Nothing in this file reads the ROM, the core or the runner: the expected
transcript is derived from the rule below and nothing else. Every figure is a
named software-profile policy value; no hardware accuracy is claimed.

Usage:
  python3 -B fixtures/pad/poll_oracle.py [--scenario NAME] --print|--sha256|--bytes

Scenarios: baseline (default), shifted, neutral, wrong-rom.
"""

from __future__ import annotations

import argparse
import hashlib
import sys

# Controller states: (buttons u16, x i8, y i8). Bit 15 is A and bit 0 is
# C-right in the authored mapping; bits 0x0080 and 0x0040 are masked.
STATE_A = (0x8000, 1, -1)
STATE_B = (0x4021, 127, -128)
STATE_C = (0x1802, -1, 1)
NEUTRAL = (0x0000, 0, 0)

POLL_COUNT = 4

# The poll execute ticks are arbitrary here: only their ordering relative to
# the stamps matters for the rule, so E_k = 1000 * k.
EXECUTE_TICKS = {poll: 1000 * poll for poll in range(1, POLL_COUNT + 1)}

# A stamp is (poll the offset is relative to, offset from that poll's E,
# state). baseline: A one tick below E1, B exactly at E2, C one tick above E3.
SCENARIO_STAMPS = {
    "baseline": [(1, -1, STATE_A), (2, 0, STATE_B), (3, +1, STATE_C)],
    "shifted": [(1, -1, STATE_A), (2, -1, STATE_B), (3, +1, STATE_C)],
    "neutral": [],
    "wrong-rom": [(1, -1, STATE_A), (2, 0, STATE_B), (3, +1, STATE_C)],
}

# Byte 8 of the read-back image is the response count byte of channel 1: its
# count 4 with the no-device flag 0x80. The wrong-rom scenario is the baseline
# transcript of a ROM that prints it with that flag bit cleared.
NO_DEVICE_COUNT_BYTE = 0x84
WRONG_ROM_COUNT_BYTE = 0x04


def visible_state(stamps: list[tuple[int, int]], sample_tick: int):
    """The strictly-after rule: the last state stamped strictly before the
    sample tick; a later stamp at an equal tick wins; neutral when none."""
    chosen = NEUTRAL
    best = None
    for index, (stamp_tick, state) in enumerate(stamps):
        if stamp_tick < sample_tick and (
            best is None or (stamp_tick, index) >= best
        ):
            best = (stamp_tick, index)
            chosen = state
    return chosen


def state_bytes(state: tuple[int, int, int]) -> bytes:
    """Wire data bytes: buttons high byte, buttons low byte with the 0x00c0
    bits masked, then x and y as unsigned bytes of the signed values."""
    buttons, x, y = state
    return bytes([(buttons >> 8) & 0xFF, buttons & 0x3F, x & 0xFF, y & 0xFF])


def transcript(scenario: str) -> bytes:
    stamps = [
        (EXECUTE_TICKS[poll] + offset, state)
        for poll, offset, state in SCENARIO_STAMPS[scenario]
    ]
    count_byte = (
        WRONG_ROM_COUNT_BYTE if scenario == "wrong-rom" else NO_DEVICE_COUNT_BYTE
    )
    lines = []
    for poll in range(1, POLL_COUNT + 1):
        data = state_bytes(visible_state(stamps, EXECUTE_TICKS[poll]))
        witness = "-" if poll == 1 else "+"
        lines.append(f"P{poll} {data.hex()} {count_byte:02x} {witness}\n")
    return "".join(lines).encode("ascii")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--scenario", choices=sorted(SCENARIO_STAMPS), default="baseline"
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--print", action="store_true", dest="show")
    mode.add_argument("--sha256", action="store_true")
    mode.add_argument("--bytes", action="store_true", dest="count")
    args = parser.parse_args(argv)
    data = transcript(args.scenario)
    if args.show:
        sys.stdout.write(data.decode("ascii"))
    elif args.sha256:
        print(hashlib.sha256(data).hexdigest())
    else:
        print(len(data))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
