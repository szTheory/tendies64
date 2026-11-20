#!/usr/bin/env python3
"""Independent transcript oracle for the Phase 9 replay-mix scenario.

The scenario literals, the controller state encoding and the strictly-after
visibility rule are written here, outside the guest ROM and outside the core.
Nothing in this file reads the ROM, the core or the runner: the expected
transcript is derived from the rule below and nothing else. Every figure is a
named software-profile policy value; no hardware accuracy is claimed.

The ROM polls the controller six times with a delay loop between polls. The
committed input script stamps one state in each of the five gaps between two
consecutive polls (states S1 to S5; S3 is two states, S3a then S3b, stamped at
the same tick, a later array row winning at an equal tick). Poll n sees the
last stamp strictly before its execute tick E(n); poll 1 sees the neutral
value because no stamp precedes it.

Usage:
  python3 -B fixtures/replay/mix_oracle.py --print|--sha256|--bytes
  python3 -B fixtures/replay/mix_oracle.py --check-input PATH
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys

# Controller states: (buttons u16, x i8, y i8). Bits 0x00C0 of the low button
# byte are masked on the wire.
NEUTRAL = (0x0000, 0, 0)
STATE_S1 = (0x8000, 1, -1)
STATE_S2 = (0x4021, 127, -128)
STATE_S3A = (0x2010, -128, 127)
STATE_S3B = (0x1804, -2, 2)
STATE_S4 = (0x0102, 0, 64)
STATE_S5 = (0x0CC3, 5, -5)

POLL_COUNT = 6

# The stamps in script order: (gap index n meaning the stamp lies between E(n)
# and E(n+1), state). S3a and S3b share gap 3 and one tick; S3b is later.
SCRIPT_STATES = [
    (1, STATE_S1),
    (2, STATE_S2),
    (3, STATE_S3A),
    (3, STATE_S3B),
    (4, STATE_S4),
    (5, STATE_S5),
]

# The execute ticks are arbitrary here: only the ordering of the stamps and
# the polls matters for the rule, so E(n) = 100000 * n and a stamp in gap n
# lies at E(n) + 50000.
EXECUTE_TICKS = {poll: 100000 * poll for poll in range(1, POLL_COUNT + 1)}


def stamp_ticks() -> list[int]:
    return [EXECUTE_TICKS[gap] + 50000 for gap, _ in SCRIPT_STATES]


def visible_state(stamps, sample_tick: int):
    """The strictly-after rule: the last state stamped strictly before the
    sample tick; a later array row at an equal tick wins; neutral when none."""
    chosen = NEUTRAL
    best = None
    for index, (stamp_tick, state) in enumerate(stamps):
        if stamp_tick < sample_tick and (best is None or (stamp_tick, index) >= best):
            best = (stamp_tick, index)
            chosen = state
    return chosen


def state_bytes(state: tuple[int, int, int]) -> bytes:
    """Wire data bytes: buttons high byte, buttons low byte with the 0x00c0
    bits masked, then x and y as unsigned bytes of the signed values."""
    buttons, x, y = state
    return bytes([(buttons >> 8) & 0xFF, buttons & 0x3F, x & 0xFF, y & 0xFF])


def transcript() -> bytes:
    stamps = list(zip(stamp_ticks(), [state for _, state in SCRIPT_STATES]))
    lines = []
    for poll in range(1, POLL_COUNT + 1):
        data = state_bytes(visible_state(stamps, EXECUTE_TICKS[poll]))
        lines.append(f"M{poll} {data.hex()}\n")
    return "".join(lines).encode("ascii")


def canonical_script(ticks: list[int]) -> bytes:
    """The t64.input v1 canonical form: sorted keys, no whitespace, decimal
    string ticks, one trailing LF."""
    events = [
        {"buttons": state[0], "port": 0, "tick": str(tick), "x": state[1], "y": state[2]}
        for tick, (_, state) in zip(ticks, SCRIPT_STATES)
    ]
    document = {"events": events, "schema": "t64.input", "schema_version": 1}
    return (json.dumps(document, sort_keys=True, separators=(",", ":")) + "\n").encode(
        "ascii"
    )


def check_input(path: str) -> int:
    """Compares the committed script with the oracle literals: six rows in the
    oracle order with the oracle states, port 0, nondecreasing ticks that are
    multiples of 1000, S3a and S3b at one tick, every other pair strictly
    increasing, and the file equal to its canonical rendering."""
    with open(path, "rb") as handle:
        raw = handle.read()
    document = json.loads(raw)
    events = document["events"]
    if len(events) != len(SCRIPT_STATES):
        print(f"input mismatch: {len(events)} events, want {len(SCRIPT_STATES)}")
        return 1
    ticks = []
    for index, (event, (_, state)) in enumerate(zip(events, SCRIPT_STATES)):
        got = (event["buttons"], event["x"], event["y"])
        if got != state or event["port"] != 0:
            print(f"input mismatch at row {index}: {got} port {event['port']}")
            return 1
        ticks.append(int(event["tick"]))
    for index in range(1, len(ticks)):
        equal_required = index == 3
        if equal_required and ticks[index] != ticks[index - 1]:
            print("input mismatch: S3a and S3b must share one tick")
            return 1
        if not equal_required and ticks[index] <= ticks[index - 1]:
            print(f"input mismatch: tick order at row {index}")
            return 1
    if any(tick % 1000 != 0 for tick in ticks):
        print("input mismatch: ticks must be multiples of 1000")
        return 1
    if canonical_script(ticks) != raw:
        print("input mismatch: the file is not in canonical form")
        return 1
    print("input matches the oracle literals")
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--print", action="store_true", dest="show")
    mode.add_argument("--sha256", action="store_true")
    mode.add_argument("--bytes", action="store_true", dest="count")
    mode.add_argument("--check-input", metavar="PATH")
    args = parser.parse_args(argv)
    if args.check_input is not None:
        return check_input(args.check_input)
    data = transcript()
    if args.show:
        sys.stdout.write(data.decode("ascii"))
    elif args.sha256:
        print(hashlib.sha256(data).hexdigest())
    else:
        print(len(data))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
