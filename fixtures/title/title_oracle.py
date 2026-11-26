#!/usr/bin/env python3
"""Independent oracle for the Phase 12 title (API-06, plan 12-02).

Every constant below is copied from fixtures/title/title-spec.md (cited as
"title-spec.md TS-n" at the constant) or from the document it names there
(docs/spec/vi.md for the field clock). Nothing in this file reads a ROM, the
core, the runner, a build output or a manifest row: it imports only the
standard library, reads only the input scripts that sit beside it and the
constants it carries, and a selftest proves the import set. The oracle was
written before the guest source exists, so no expected value comes from the
producer under test (guardrail G6). Every figure is a named software-profile
policy value of an authored title; no hardware accuracy is claimed.

Field clock (title-spec.md TS-4). The VI enable tick T0 is the origin of the
field boundaries B(k) = T0 + ceil(k * 812175 * 62500000 / 48681812), evaluated
in exact Fraction arithmetic with the ceiling rule of docs/spec/vi.md. T0 is a
NOMINAL value (ORIGIN_TICKS), not a measurement, and the model assumes that
the guest has programmed the VI and entered its polling loop before the first
boundary. A guest whose real T0 is within 100,000 ticks of the nominal value
gives the same expectations, because every input stamp must lie at least
200,000 ticks from every boundary (the oracle exits 2 otherwise). Plan 12-03
measures the real T0.

Usage:
  python3 -B fixtures/title/title_oracle.py --selftest
  python3 -B fixtures/title/title_oracle.py --print
  python3 -B fixtures/title/title_oracle.py --write FILE
  python3 -B fixtures/title/title_oracle.py --check FILE
  python3 -B fixtures/title/title_oracle.py --selftest --check FILE
  python3 -B fixtures/title/title_oracle.py --print --input SCRIPT [--max-fields N]
  python3 -B fixtures/title/title_oracle.py --emit-inputs DIR

--print --input SCRIPT models one input script and prints its expected run;
--max-fields N refuses a script that renders more than N fields (the short
scenarios use 3). --emit-inputs DIR writes the four scenario scripts of
title-spec.md TS-14 with their stamps at field midpoints; the committed
scripts are exactly that output. Without --input, --print writes the
expectation document to standard output. --write FILE writes it (the committed
fixtures/title/title.expected.json) and --check FILE regenerates the same text
and exits 1, naming the first differing byte, when FILE is not byte for byte
equal to it, so the committed file cannot drift from this script. --selftest runs the built-in cases (hand-written literals,
never values taken from the functions under test) and exits 1 on any FAIL.
Exit status 2 means a refusal: an invalid argument or script, or a model case
the oracle does not cover.
"""

from __future__ import annotations

import argparse
import ast
import hashlib
import io
import json
import math
import os
import re
import sys
from fractions import Fraction

SPEC = "fixtures/title/title-spec.md"
HERE = os.path.dirname(os.path.abspath(__file__))

# ---- title-spec.md TS-3 screen, TS-9 render ---------------------------------

WIDTH = 128
HEIGHT = 96
BYTES_PER_PIXEL = 4
FRAME_BYTES = WIDTH * HEIGHT * BYTES_PER_PIXEL  # 49,152 (DR-02)
BACKGROUND = (0x20, 0x30, 0x60, 0xFF)  # SetFillColor 0x203060ff (TS-9 step 4)
SQUARE = (0xF0, 0xC0, 0x20, 0xFF)  # SetFillColor 0xf0c020ff (TS-9 step 6)
SQUARE_SIZE = 16

# ---- title-spec.md TS-7 input rule ------------------------------------------

START_POSITION = (56, 40)
MAX_X = 112  # WIDTH - SQUARE_SIZE
MAX_Y = 80  # HEIGHT - SQUARE_SIZE

# ---- title-spec.md TS-4 time base (docs/spec/vi.md clock policy) ------------

TICK_RATE = 62_500_000
VI_CLOCK = 48_681_812
H_SYNC = 3093
V_SYNC = 525
ORIGIN_TICKS = 550_000  # nominal T0, the VI enable tick (an assumption)
BOUNDARY_MARGIN = 200_000  # DR-05, TS-15
SAVE_SIZE = 32_768  # docs/spec/save-media.md, SRAM on the -s32k profile


class Refused(Exception):
    """The input is invalid or the model does not cover the case (exit 2)."""


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# ---- field boundaries (TS-4) -------------------------------------------------


def boundary(k: int) -> int:
    """Tick of field boundary k (k = 0 is the VI enable tick T0)."""
    cycles = Fraction(k * (H_SYNC + 1) * V_SYNC, 2)
    return ORIGIN_TICKS + math.ceil(cycles * TICK_RATE / VI_CLOCK)


# ---- frame model (TS-9) -------------------------------------------------------


def render(x: int, y: int) -> bytes:
    """The 49,152-byte frame with the square at (x, y), R,G,B,A byte order."""
    if not (0 <= x <= MAX_X and 0 <= y <= MAX_Y):
        raise Refused("square position (%d, %d) is outside the screen" % (x, y))
    background = bytes(BACKGROUND)
    square = bytes(SQUARE)
    plain_row = background * WIDTH
    square_row = background * x + square * SQUARE_SIZE + background * (WIDTH - x - SQUARE_SIZE)
    rows = [square_row if y <= py < y + SQUARE_SIZE else plain_row for py in range(HEIGHT)]
    return b"".join(rows)


BLANK_FRAME = bytes(FRAME_BYTES)  # the frame of a refused packet (TS-13)


# ---- constants of the input rule, the tone, the slot and the limits ---------------

# title-spec.md TS-6: button bits of the 16-bit controller word.
BUTTON_BITS = {
    "A": 0x8000, "B": 0x4000, "Z": 0x2000, "S": 0x1000,
    "U": 0x0800, "D": 0x0400, "L": 0x0200, "R": 0x0100,
}  # fmt: skip
BUTTON_A, BUTTON_B, BUTTON_START = BUTTON_BITS["A"], BUTTON_BITS["B"], BUTTON_BITS["S"]
# title-spec.md TS-7: pixels per field per direction, the stick threshold.
STEP = 2
STICK_THRESHOLD = 16
# title-spec.md TS-8: the tone table (512 stereo frames, square wave of period 64, amplitude
# 0x2000) and the burst of two descriptors.
TONE_FRAMES = 512
TONE_PERIOD = 64
TONE_AMPLITUDE = 0x2000
# title-spec.md TS-12: the save slot.
SLOT_MAGIC = 0x54363454
SLOT_BYTES = 16
# title-spec.md TS-14, TS-15: scenario limits.
SHORT_MAX_FIELDS = 3
MIN_A_GAP_POLLS = 3
MIN_A_TO_START_POLLS = 2
PERSIST_FIELDS = 3
LONG_MIN_FIELDS = 10  # DR-05: consumer and pump scenarios render 10 or more fields
MAX_POLLS = 64
MAX_EVENTS = 4096  # DR-13
MAX_SCRIPT_BYTES = 65_536  # DR-13
MAX_TICK = 2**64 - 1
FIELD_CYCLES = (H_SYNC + 1) * V_SYNC // 2  # 812,175 source cycles (TS-4)

# title-spec.md TS-14: the scenario table as (event field j, buttons, X, Y).
SCENARIO_ROWS = {
    "short": ((0, "RA", 0, 0), (1, "B", 0, 0), (2, "S", 0, 0)),
    "short-wrong": ((0, "LA", 0, 0), (1, "B", 0, 0), (2, "S", 0, 0)),
    "long": (
        (0, "R", 0, 0), (1, "RD", 0, 0), (2, "", 40, 0), (3, "", 15, -15), (4, "AU", 0, 0),
        (5, "UL", 0, 0), (6, "", 0, 0), (7, "A", -100, -50), (8, "B", 0, 0), (9, "RD", 0, 0),
        (10, "RD", 0, 0), (11, "S", 0, 0),
    ),
}  # fmt: skip
SCENARIO_SHIFTED = {"shifted": ("long", 1)}  # the long events one field later
SCENARIO_NAMES = ("short", "short-wrong", "long", "shifted")
SHORT_SCENARIOS = ("short", "short-wrong")

# ---- the title model (TS-4 to TS-12) ----------------------------------------------


def midpoint(j: int) -> int:
    """Stamp tick of event field j: the midpoint of the field B(j) to B(j + 1) (TS-4)."""
    low = boundary(j)
    return low + (boundary(j + 1) - low) // 2


def pad_bits(letters: str) -> int:
    return sum(BUTTON_BITS[letter] for letter in letters)


def scenario_events(name: str) -> list[tuple[int, int, int, int]]:
    """The (field, buttons, X, Y) rows of a scenario of TS-14."""
    shift = 0
    if name in SCENARIO_SHIFTED:
        name, shift = SCENARIO_SHIFTED[name]
    if name not in SCENARIO_ROWS:
        raise Refused("unknown scenario %r" % name)
    return [(j + shift, pad_bits(letters), x, y) for j, letters, x, y in SCENARIO_ROWS[name]]


def input_text(name: str) -> str:
    """The t64.input v1 script of a scenario, stamps at field midpoints (--emit-inputs)."""
    events = [
        {"buttons": buttons, "port": 0, "tick": str(midpoint(j)), "x": x, "y": y}
        for j, buttons, x, y in scenario_events(name)
    ]
    document = {"events": events, "schema": "t64.input", "schema_version": 1}
    return json.dumps(document, sort_keys=True, separators=(",", ":")) + "\n"


def _no_duplicates(pairs):
    keys = [key for key, _ in pairs]
    if len(keys) != len(set(keys)):
        raise Refused("input script holds a duplicate key")
    return dict(pairs)


def parse_input(text: str) -> list[tuple[int, int, int, int]]:
    """Strict t64.input v1 subset (DR-13, TS-15): the stamps (tick, buttons, X, Y)."""
    if len(text.encode("utf-8")) > MAX_SCRIPT_BYTES:
        raise Refused("input script is larger than %d bytes" % MAX_SCRIPT_BYTES)
    try:
        document = json.loads(text, object_pairs_hook=_no_duplicates)
    except ValueError as error:
        raise Refused("input script is not valid JSON: %s" % error) from None
    if (
        not isinstance(document, dict)
        or set(document) != {"events", "schema", "schema_version"}
        or document["schema"] != "t64.input"
        or type(document["schema_version"]) is not int
        or document["schema_version"] != 1
        or not isinstance(document["events"], list)
    ):
        raise Refused("input script schema is not t64.input version 1 with closed keys")
    if len(document["events"]) > MAX_EVENTS:
        raise Refused("input script has more than %d events" % MAX_EVENTS)
    stamps = []
    previous = 0
    for event in document["events"]:
        if not isinstance(event, dict) or set(event) != {"tick", "port", "buttons", "x", "y"}:
            raise Refused("input event keys are not exactly tick, port, buttons, x, y")
        tick_text = event["tick"]
        if not isinstance(tick_text, str) or re.fullmatch(r"0|[1-9][0-9]*", tick_text) is None:
            raise Refused("input event tick is not a canonical decimal string")
        tick = int(tick_text)
        if tick > MAX_TICK:
            raise Refused("input event tick exceeds 64 bits")
        if tick < previous:
            raise Refused("input event ticks go backwards")
        previous = tick
        for key in ("port", "buttons", "x", "y"):
            if type(event[key]) is not int:
                raise Refused("input event %s is not an integer" % key)
        if event["port"] != 0:
            raise Refused("input event port is not 0")
        if not 0 <= event["buttons"] <= 0xFFFF:
            raise Refused("input event buttons are outside 0 to 65535")
        if not (-128 <= event["x"] <= 127 and -128 <= event["y"] <= 127):
            raise Refused("input event axis is outside -128 to 127")
        stamps.append((tick, event["buttons"], event["x"], event["y"]))
    return stamps


def nearest_boundary_distance(tick: int) -> int:
    """Distance from tick to the nearest field boundary B(k), k >= 0 (TS-4)."""
    below = max((tick - ORIGIN_TICKS) * VI_CLOCK // (FIELD_CYCLES * TICK_RATE), 0)
    return min(abs(tick - boundary(k)) for k in range(max(below - 1, 0), below + 3))


def check_stamps(stamps) -> None:
    """TS-15: every stamp lies at least BOUNDARY_MARGIN ticks from every boundary."""
    for tick, _buttons, _x, _y in stamps:
        distance = nearest_boundary_distance(tick)
        if distance < BOUNDARY_MARGIN:
            raise Refused(
                "a stamp at tick %d is %d ticks from a field boundary (margin %d)"
                % (tick, distance, BOUNDARY_MARGIN)
            )


def wire_bytes(buttons: int, x: int, y: int) -> bytes:
    """The four state bytes of TS-6: high byte, low byte with 0x80 and 0x40 cleared, X, Y."""
    return bytes([(buttons >> 8) & 0xFF, buttons & 0x3F, x & 0xFF, y & 0xFF])


def pad_at(stamps, poll: int) -> tuple[int, int, int]:
    """Logical pad of poll p: the last event stamped before B(p - 1) (TS-4); a later row
    at an equal tick wins; neutral when there is none."""
    limit = boundary(poll - 1)
    chosen = (0, 0, 0)
    for tick, buttons, x, y in stamps:
        if tick < limit:
            chosen = (buttons, x, y)
    return chosen


def step_position(x: int, y: int, buttons: int, axis_x: int, axis_y: int) -> tuple[int, int]:
    """TS-7 step 1: B resets, otherwise D-pad and stick move 2 pixels per axis, clamped."""
    if buttons & BUTTON_B:
        return START_POSITION
    right = bool(buttons & BUTTON_BITS["R"]) or axis_x >= STICK_THRESHOLD
    left = bool(buttons & BUTTON_BITS["L"]) or axis_x <= -STICK_THRESHOLD
    down = bool(buttons & BUTTON_BITS["D"]) or axis_y <= -STICK_THRESHOLD
    up = bool(buttons & BUTTON_BITS["U"]) or axis_y >= STICK_THRESHOLD
    new_x = x + STEP * int(right) - STEP * int(left)
    new_y = y + STEP * int(down) - STEP * int(up)
    return (min(max(new_x, 0), MAX_X), min(max(new_y, 0), MAX_Y))


def tone_table() -> bytes:
    """TS-8: 512 stereo frames, +0x2000 for 32 frames then -0x2000 for 32, left = right."""
    table = bytearray()
    for frame in range(TONE_FRAMES):
        sample = TONE_AMPLITUDE if frame % TONE_PERIOD < TONE_PERIOD // 2 else 0x10000 - TONE_AMPLITUDE
        table += sample.to_bytes(2, "big") * 2
    return bytes(table)


def slot_image(x: int, y: int, count: int, base: bytes | None = None) -> bytes:
    """TS-12: the SRAM image holding the slot at offset 0 (four big-endian words)."""
    slot = b"".join(word.to_bytes(4, "big") for word in (SLOT_MAGIC, x, y, count))
    image = bytearray(base if base is not None else bytes(SAVE_SIZE))
    image[0:SLOT_BYTES] = slot
    return bytes(image)


def parse_slot(image: bytes):
    """TS-12 boot load: (x, y, count) when the magic matches and x <= 112, y <= 80."""
    if len(image) != SAVE_SIZE:
        raise Refused("save image is %d bytes, not %d" % (len(image), SAVE_SIZE))
    magic, x, y, count = (int.from_bytes(image[i : i + 4], "big") for i in range(0, SLOT_BYTES, 4))
    if magic != SLOT_MAGIC or x > MAX_X or y > MAX_Y:
        return None
    return (x, y, count)


def play(stamps, *, fields=None, max_fields=None, restore=None, base_image=None) -> dict:
    """Model one run (TS-5 to TS-12). fields=None runs to the Start poll; an integer stops the
    host after that many video packets (the persist scenario). restore is the boot-load result
    of TS-12 (a tuple or None) and base_image the SRAM image at boot (blank by default)."""
    check_stamps(stamps)
    image = base_image if base_image is not None else bytes(SAVE_SIZE)
    x, y = START_POSITION
    count = 0
    transcript = ""
    if restore is not None:
        x, y, count = restore
        transcript += "L %02x %02x %08x\n" % (x, y, count)
    last_tick = stamps[-1][0] if stamps else -1
    previous_a = False
    frames, positions, polled, bursts = [], [], [], []
    completes = False
    poll = 1
    while True:
        if fields is not None and poll > fields:
            break
        if poll > MAX_POLLS:
            raise Refused("the run needs more than %d polls" % MAX_POLLS)
        buttons, axis_x, axis_y = pad_at(stamps, poll)
        polled.append(wire_bytes(buttons, axis_x, axis_y).hex())
        if buttons & BUTTON_START:
            if poll == 1:
                raise Refused("Start at poll 1 ends the run before any video packet")
            completes = True
            image = slot_image(x, y, count, image)
            transcript += "S %02x %02x %08x %02x\n" % (x, y, count, poll)
            break
        if fields is None and boundary(poll - 1) > last_tick:
            raise Refused("the script never presses Start")
        x, y = step_position(x, y, buttons, axis_x, axis_y)
        a_now = bool(buttons & BUTTON_A)
        if a_now and not previous_a:
            count = (count + 1) & 0xFFFFFFFF
            bursts.append(poll)
        previous_a = a_now
        frames.append(sha256(render(x, y)))
        positions.append((x, y))
        poll += 1
    if max_fields is not None and len(frames) > max_fields:
        raise Refused(
            "the run renders %d fields; a short script may render at most %d" % (len(frames), max_fields)
        )
    for earlier, later in zip(bursts, bursts[1:]):
        if later - earlier < MIN_A_GAP_POLLS:
            raise Refused("A press edges at polls %d and %d are fewer than %d polls apart" % (earlier, later, MIN_A_GAP_POLLS))
    if bursts and poll - bursts[-1] < MIN_A_TO_START_POLLS:
        raise Refused("the last A press edge (poll %d) is fewer than %d polls before the end (poll %d)" % (bursts[-1], MIN_A_TO_START_POLLS, poll))
    table = tone_table()
    return {
        "audio": [table] * (2 * len(bursts)),
        "bursts": bursts,
        "completes": completes,
        "count": count,
        "frames": frames,
        "image": image,
        "polled": polled,
        "poll_count": len(polled),
        "positions": positions,
        "transcript": transcript,
    }


def nonzero_ranges(image: bytes) -> list[dict]:
    ranges = []
    index = 0
    while index < len(image):
        if image[index] == 0:
            index += 1
            continue
        start = index
        while index < len(image) and image[index] != 0:
            index += 1
        ranges.append({"hex": image[start:index].hex(), "offset": start})
    return ranges


def scenario_document(result: dict) -> dict:
    """The JSON form of one modeled run (checkpoint digests, never long raw captures)."""
    audio = result["audio"]
    image = result["image"]
    transcript = result["transcript"].encode("ascii")
    return {
        "audio": {
            "packet_bytes": len(audio[0]) if audio else 0,
            "packet_count": len(audio),
            "packet_sha256": [sha256(packet) for packet in audio],
            "stream_sha256": sha256(b"".join(audio)),
        },
        "burst_polls": result["bursts"],
        "completes": result["completes"],
        "frames": result["frames"],
        "poll_count": result["poll_count"],
        "polled": result["polled"],
        "positions": [list(position) for position in result["positions"]],
        "press_count": result["count"],
        "save": {"bytes": len(image), "nonzero": nonzero_ranges(image), "sha256": sha256(image)},
        "transcript": result["transcript"],
        "transcript_bytes": len(transcript),
        "transcript_sha256": sha256(transcript),
        "video_packets": len(result["frames"]),
    }


# ---- the expectation document -------------------------------------------------


def constants_block() -> dict:
    return {
        "a_edge_gap_polls": MIN_A_GAP_POLLS,
        "a_to_start_polls": MIN_A_TO_START_POLLS,
        "background_rgba": bytes(BACKGROUND).hex(),
        "boundary_margin_ticks": BOUNDARY_MARGIN,
        "field_cycles": FIELD_CYCLES,
        "frame_bytes": FRAME_BYTES,
        "h_sync": H_SYNC,
        "height": HEIGHT,
        "max_x": MAX_X,
        "max_y": MAX_Y,
        "origin_ticks_nominal": ORIGIN_TICKS,
        "persist_fields": PERSIST_FIELDS,
        "save_bytes": SAVE_SIZE,
        "short_max_fields": SHORT_MAX_FIELDS,
        "slot_magic": "%08x" % SLOT_MAGIC,
        "square_rgba": bytes(SQUARE).hex(),
        "square_size": SQUARE_SIZE,
        "start_x": START_POSITION[0],
        "start_y": START_POSITION[1],
        "step_pixels": STEP,
        "stick_threshold": STICK_THRESHOLD,
        "tick_rate": TICK_RATE,
        "v_sync": V_SYNC,
        "vi_clock": VI_CLOCK,
        "width": WIDTH,
    }


def tone_block() -> dict:
    table = tone_table()
    return {
        "amplitude": TONE_AMPLITUDE,
        "bitrate": 15,
        "dacrate": 1103,
        "descriptor_bytes": len(table),
        "descriptors_per_burst": 2,
        "frames": TONE_FRAMES,
        "period_frames": TONE_PERIOD,
        "table_sha256": sha256(table),
    }


def committed_script(name: str):
    """The committed input script of a scenario: its stamps and its bytes. The file must be
    exactly what --emit-inputs writes, so the stamps follow the boundary function."""
    path = input_path(name)
    with open(path, "rb") as handle:
        data = handle.read(MAX_SCRIPT_BYTES + 1)
    text = data.decode("ascii")
    if text != input_text(name):
        raise Refused("%s differs from the --emit-inputs output (regenerate it)" % os.path.basename(path))
    return parse_input(text), data


def first_difference(left: list, right: list) -> int:
    """Index of the first position at which two digest lists differ."""
    for index in range(min(len(left), len(right))):
        if left[index] != right[index]:
            return index
    raise Refused("the digest lists do not differ")


def expected_document() -> dict:
    inputs = {}
    runs = {}
    for name in SCENARIO_NAMES:
        stamps, data = committed_script(name)
        inputs[name] = {
            "events": len(stamps),
            "file": os.path.basename(input_path(name)),
            "sha256": sha256(data),
        }
        cap = SHORT_MAX_FIELDS if name in SHORT_SCENARIOS else None
        runs[name] = play(stamps, max_fields=cap)
    long_image = runs["long"]["image"]
    persist = play([], fields=PERSIST_FIELDS, restore=parse_slot(long_image), base_image=long_image)
    fresh = play([], fields=1)
    blank = sha256(BLANK_FRAME)
    for name, run in list(runs.items()) + [("persist", persist)]:
        if blank in run["frames"]:
            raise Refused("a frame of scenario %s equals the blank frame" % name)
    for name in ("long", "shifted"):
        if len(runs[name]["frames"]) < LONG_MIN_FIELDS:
            raise Refused("scenario %s renders fewer than %d fields" % (name, LONG_MIN_FIELDS))
    long_vs_shifted = first_difference(runs["long"]["frames"], runs["shifted"]["frames"])
    short_vs_wrong = first_difference(runs["short"]["frames"], runs["short-wrong"]["frames"])
    if persist["frames"][0] == fresh["frames"][0]:
        raise Refused("the persist frame equals a fresh boot frame")
    scenarios = {name: scenario_document(run) for name, run in runs.items()}
    scenarios["persist"] = scenario_document(persist)
    scenarios["persist"]["boot_image_of"] = "long"
    return {
        "blank_bytes": len(BLANK_FRAME),
        "blank_save_sha256": sha256(bytes(SAVE_SIZE)),
        "blank_sha256": blank,
        "boundaries": [boundary(k) for k in range(0, 15)],
        "constants": constants_block(),
        "distinguishable": {
            "frames_equal_to_blank": 0,
            "long_vs_shifted_first_difference": long_vs_shifted,
            "persist_vs_fresh_boot_frame_differ": True,
            "short_vs_short_wrong_first_difference": short_vs_wrong,
        },
        "fresh_boot_frame_sha256": fresh["frames"][0],
        "inputs": inputs,
        "neutral_frame": {
            "sha256": sha256(render(*START_POSITION)),
            "x": START_POSITION[0],
            "y": START_POSITION[1],
        },
        "refused": {
            "audio_packets": 0,
            "completes": False,
            "frames_each_sha256": blank,
            "save_sha256": sha256(bytes(SAVE_SIZE)),
            "transcript": "",
        },
        "scenarios": scenarios,
        "schema": "t64.title-oracle",
        "schema_version": 1,
        "spec": SPEC,
        "tone": tone_block(),
    }


def document_text(document: dict) -> str:
    return json.dumps(document, indent=2, sort_keys=True) + "\n"


def expected_text() -> str:
    return document_text(expected_document())


# ---- selftest -------------------------------------------------------------------

# Hand-computed once, outside this file, by integer ceiling division and by bc:
# ceil(k * 812175 * 62500000 / 48681812) for k = 1 to 12 (k = 1 is the figure
# 1,042,709 that docs/spec/vi.md gives for a standard field).
FIELD_TICKS_LITERAL = (
    1042709, 2085417, 3128126, 4170834, 5213543, 6256251,
    7298960, 8341668, 9384377, 10427085, 11469794, 12512502,
)  # fmt: skip
# SHA-256 of 49,152 zero bytes, computed once outside this file (shasum).
BLANK_SHA256_LITERAL = "2aae7dc846aaf25f1cadf55f1666862046c6db9d65d84bdc07fa039dac405606"

# Standard-library modules the oracle may import (closed list).
ALLOWED_IMPORTS = frozenset(
    {"__future__", "argparse", "ast", "contextlib", "fractions", "hashlib", "io", "json",
     "math", "os", "re", "sys"}
)  # fmt: skip
# Substrings that no non-docstring string constant of this file may hold
# (assembled so that this definition does not hold them itself).
FORBIDDEN_FRAGMENTS = (".z" + "64", "mani" + "fest", "bui" + "ld/", "run" + "ner/", "co" + "re/")


class Selftest:
    def __init__(self) -> None:
        self.failures = 0
        self.cases = 0

    def case(self, name: str, ok: bool, detail: str = "") -> None:
        self.cases += 1
        if ok:
            print("PASS %s" % name)
        else:
            self.failures += 1
            print("FAIL %s: %s" % (name, detail))

    def equal(self, name: str, observe, expected) -> None:
        """observe is a callable; any exception is a failure carrying its text."""
        try:
            observed = observe()
        except Exception as error:  # noqa: BLE001 - a selftest reports every failure kind
            self.case(name, False, "expected %r, raised %s(%s)" % (expected, type(error).__name__, error))
            return
        self.case(name, observed == expected, "expected %r, observed %r" % (expected, observed))

    def refuses(self, name: str, action, reason: str = "") -> None:
        """action is a callable that must raise Refused (the exit-2 class); reason, when given,
        is a substring the refusal text must hold, so a case cannot pass for another reason."""
        try:
            action()
        except Refused as error:
            if "not implemented yet" in str(error) or reason not in str(error):
                self.case(name, False, "expected a refusal holding %r, observed %r" % (reason, str(error)))
            else:
                self.case(name, True)
            return
        except Exception as error:  # noqa: BLE001
            self.case(name, False, "expected Refused, raised %s(%s)" % (type(error).__name__, error))
            return
        self.case(name, False, "expected Refused, nothing raised")


def source_imports() -> set[str]:
    with open(os.path.abspath(__file__), "r", encoding="utf-8") as handle:
        tree = ast.parse(handle.read())
    found: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            found.update(alias.name.split(".")[0] for alias in node.names)
        elif isinstance(node, ast.ImportFrom):
            found.add((node.module or "").split(".")[0])
    return found


def source_strings() -> list[str]:
    """Every string constant of this file that is not a docstring."""
    with open(os.path.abspath(__file__), "r", encoding="utf-8") as handle:
        tree = ast.parse(handle.read())
    docstrings = set()
    for node in ast.walk(tree):
        if isinstance(node, (ast.Module, ast.FunctionDef, ast.ClassDef)):
            body = node.body
            if body and isinstance(body[0], ast.Expr) and isinstance(body[0].value, ast.Constant):
                docstrings.add(id(body[0].value))
    return [
        node.value
        for node in ast.walk(tree)
        if isinstance(node, ast.Constant) and isinstance(node.value, str)
        and id(node) not in docstrings
    ]


def selftest_tracer(test: Selftest) -> None:
    for k, literal in enumerate(FIELD_TICKS_LITERAL, start=1):
        test.case(
            "boundary-%d" % k,
            boundary(k) == ORIGIN_TICKS + literal,
            "B(%d) = %d, literal %d" % (k, boundary(k), ORIGIN_TICKS + literal),
        )
    test.case("boundary-origin", boundary(0) == ORIGIN_TICKS, "B(0) = %d" % boundary(0))
    test.case(
        "blank-digest",
        sha256(bytes(49152)) == BLANK_SHA256_LITERAL
        and expected_document()["blank_sha256"] == BLANK_SHA256_LITERAL,
        "blank digest %s" % expected_document()["blank_sha256"],
    )
    frame = render(56, 40)
    pixel = lambda px, py: tuple(frame[(py * WIDTH + px) * 4 : (py * WIDTH + px) * 4 + 4])  # noqa: E731
    test.case("neutral-frame-size", len(frame) == 49152, "length %d" % len(frame))
    test.case("neutral-frame-corner", pixel(0, 0) == (0x20, 0x30, 0x60, 0xFF), str(pixel(0, 0)))
    test.case("neutral-frame-square-first", pixel(56, 40) == (0xF0, 0xC0, 0x20, 0xFF), str(pixel(56, 40)))
    test.case("neutral-frame-square-last", pixel(71, 55) == (0xF0, 0xC0, 0x20, 0xFF), str(pixel(71, 55)))
    test.case(
        "neutral-frame-square-edges",
        pixel(55, 40) == (0x20, 0x30, 0x60, 0xFF)
        and pixel(72, 55) == (0x20, 0x30, 0x60, 0xFF)
        and pixel(56, 39) == (0x20, 0x30, 0x60, 0xFF)
        and pixel(71, 56) == (0x20, 0x30, 0x60, 0xFF),
        "an edge pixel is inside the square",
    )
    square_pixels = sum(
        1 for index in range(0, len(frame), 4) if tuple(frame[index : index + 4]) == (0xF0, 0xC0, 0x20, 0xFF)
    )
    test.case("neutral-frame-square-area", square_pixels == 256, "%d square pixels" % square_pixels)
    test.case("neutral-frame-not-blank", sha256(frame) != BLANK_SHA256_LITERAL, "frame equals blank")
    imports = source_imports()
    test.case(
        "imports-standard-library-only",
        imports <= ALLOWED_IMPORTS,
        "unexpected imports %s" % sorted(imports - ALLOWED_IMPORTS),
    )
    hits = [s for s in source_strings() for fragment in FORBIDDEN_FRAGMENTS if fragment in s]
    test.case("no-producer-paths", not hits, "string constants name producer files: %s" % hits[:3])


def selftest_model(test: Selftest) -> None:
    """Cases of the full title model. Every literal is written by hand from title-spec.md."""
    A, B, START = 0x8000, 0x4000, 0x1000
    RIGHT, LEFT, DOWN, UP, Z = 0x0100, 0x0200, 0x0400, 0x0800, 0x2000

    def stamps_of(*rows):
        """rows of (field j, buttons, x, y) -> stamps (tick, buttons, x, y) at midpoints."""
        return [(midpoint(j), buttons, x, y) for j, buttons, x, y in rows]

    short = stamps_of((0, RIGHT | A, 0, 0), (1, B, 0, 0), (2, START, 0, 0))
    short_wrong = stamps_of((0, LEFT | A, 0, 0), (1, B, 0, 0), (2, START, 0, 0))
    long_rows = (
        (0, RIGHT, 0, 0), (1, RIGHT | DOWN, 0, 0), (2, 0, 40, 0), (3, 0, 15, -15),
        (4, A | UP, 0, 0), (5, UP | LEFT, 0, 0), (6, 0, 0, 0), (7, A, -100, -50),
        (8, B, 0, 0), (9, RIGHT | DOWN, 0, 0), (10, RIGHT | DOWN, 0, 0), (11, START, 0, 0),
    )  # fmt: skip
    long = stamps_of(*long_rows)
    shifted = stamps_of(*[(j + 1, b, x, y) for j, b, x, y in long_rows])

    # TS-4: midpoints of the first three fields, computed by hand from the literals above.
    test.equal(
        "midpoints-0-2",
        lambda: [midpoint(0), midpoint(1), midpoint(2)],
        [1071354, 2114063, 3156771],
    )
    # TS-14 short scenario: poll 1 neutral, poll 2 Right+A, poll 3 B, poll 4 Start.
    test.equal("short-positions", lambda: play(short)["positions"], [(56, 40), (58, 40), (56, 40)])
    test.equal(
        "short-polled-words",
        lambda: play(short)["polled"],
        ["00000000", "81000000", "40000000", "10000000"],
    )
    test.equal("short-burst-after-poll-2", lambda: play(short)["bursts"], [2])
    test.equal("short-audio-packets", lambda: len(play(short)["audio"]), 2)
    test.equal(
        "short-slot",
        lambda: play(short)["image"][:16].hex(),
        "54363454" "00000038" "00000028" "00000001",
    )
    test.equal("short-transcript", lambda: play(short)["transcript"], "S 38 28 00000001 04\n")
    test.equal("short-completes", lambda: (play(short)["completes"], play(short)["poll_count"]), (True, 4))
    test.equal("short-wrong-positions", lambda: play(short_wrong)["positions"], [(56, 40), (54, 40), (56, 40)])
    test.equal(
        "short-wrong-frames-differ-at-2",
        lambda: (play(short)["frames"][1] != play(short_wrong)["frames"][1]),
        True,
    )
    # TS-14 long scenario.
    long_positions = [
        (56, 40), (58, 40), (60, 42), (62, 42), (62, 42), (62, 40),
        (60, 38), (60, 38), (58, 40), (56, 40), (58, 42), (60, 44),
    ]  # fmt: skip
    test.equal("long-positions", lambda: play(long)["positions"], long_positions)
    test.equal("long-bursts", lambda: play(long)["bursts"], [6, 9])
    test.equal("long-audio-packets", lambda: len(play(long)["audio"]), 4)
    test.equal("long-transcript", lambda: play(long)["transcript"], "S 3c 2c 00000002 0d\n")
    test.equal(
        "long-slot",
        lambda: play(long)["image"][:16].hex(),
        "54363454" "0000003c" "0000002c" "00000002",
    )
    test.equal("shifted-start-poll", lambda: (play(shifted)["poll_count"], len(play(shifted)["frames"])), (14, 13))
    test.equal(
        "shifted-differs-from-long-at-first-moved-field",
        lambda: (play(long)["frames"][1] != play(shifted)["frames"][1], play(shifted)["positions"][1]),
        (True, (56, 40)),
    )
    test.equal(
        "no-frame-equals-blank",
        lambda: any(f == BLANK_SHA256_LITERAL for r in (short, short_wrong, long, shifted) for f in play(r)["frames"]),
        False,
    )
    # TS-7 movement, clamp and the stick threshold.
    test.equal("stick-15-inside", lambda: step_position(56, 40, 0, 15, -15), (56, 40))
    test.equal("stick-right-16", lambda: step_position(56, 40, 0, 16, 0), (58, 40))
    test.equal("stick-left-16", lambda: step_position(56, 40, 0, -16, 0), (54, 40))
    test.equal("stick-up-16", lambda: step_position(56, 40, 0, 0, 16), (56, 38))
    test.equal("stick-down-16", lambda: step_position(56, 40, 0, 0, -16), (56, 42))
    test.equal("dpad-and-stick-same-direction", lambda: step_position(56, 40, RIGHT, 100, 0), (58, 40))
    test.equal("opposites-cancel", lambda: step_position(56, 40, LEFT | RIGHT | UP | DOWN, 0, 0), (56, 40))
    test.equal(
        "clamp",
        lambda: [
            step_position(112, 40, RIGHT, 0, 0), step_position(111, 40, RIGHT, 0, 0),
            step_position(0, 40, LEFT, 0, 0), step_position(1, 40, LEFT, 0, 0),
            step_position(56, 0, UP, 0, 0), step_position(56, 80, DOWN, 0, 0),
            step_position(56, 79, DOWN, 0, 0), step_position(56, 1, UP, 0, 0),
        ],
        [(112, 40), (112, 40), (0, 40), (0, 40), (56, 0), (56, 80), (56, 80), (56, 0)],
    )  # fmt: skip
    test.equal("b-resets-and-wins", lambda: step_position(100, 70, B | RIGHT, 0, 0), (56, 40))
    test.equal("z-ignored", lambda: step_position(56, 40, Z, 0, 0), (56, 40))
    # TS-7 A press edge: held A counts once.
    held = stamps_of((0, A, 0, 0), (1, A, 0, 0), (2, A, 0, 0), (3, START, 0, 0))
    test.equal("a-held-counts-once", lambda: (play(held)["bursts"], play(held)["transcript"]), ([2], "S 38 28 00000001 05\n"))
    # TS-15 refusals: boundary margin, short cap, A gaps, Start rules, strict grammar.
    near = [(boundary(1) + 150000, 0, 0, 0), (midpoint(2), START, 0, 0)]
    test.refuses("stamp-150000-after-boundary", lambda: play(near), "from a field boundary")
    test.refuses("stamp-199999-from-boundary", lambda: check_stamps([(boundary(2) + 199999, 0, 0, 0)]), "from a field boundary")
    test.equal("stamp-200000-from-boundary", lambda: check_stamps([(boundary(2) + 200000, 0, 0, 0)]), None)
    test.refuses("stamp-before-boundary-199999", lambda: check_stamps([(boundary(2) - 199999, 0, 0, 0)]), "from a field boundary")
    four_fields = stamps_of((0, RIGHT, 0, 0), (1, LEFT, 0, 0), (2, RIGHT, 0, 0), (3, START, 0, 0))
    test.refuses("short-with-fourth-field", lambda: play(four_fields, max_fields=3), "at most 3")
    test.equal("long-script-may-have-four-fields", lambda: len(play(four_fields, max_fields=None)["frames"]), 4)
    test.refuses("short-cap-applies-to-long", lambda: play(long, max_fields=3), "at most 3")
    close_edges = stamps_of((0, A, 0, 0), (1, 0, 0, 0), (2, A, 0, 0), (6, START, 0, 0))
    test.refuses("a-edges-2-polls-apart", lambda: play(close_edges), "polls apart")
    ok_edges = stamps_of((0, A, 0, 0), (1, 0, 0, 0), (2, 0, 0, 0), (3, A, 0, 0), (7, START, 0, 0))
    test.equal("a-edges-3-polls-apart", lambda: play(ok_edges)["bursts"], [2, 5])
    test.refuses("a-edge-1-poll-before-start", lambda: play(stamps_of((0, A, 0, 0), (1, START, 0, 0))), "polls before the end")
    test.refuses("never-presses-start", lambda: play(stamps_of((0, RIGHT, 0, 0), (1, 0, 0, 0))), "never presses Start")
    test.refuses("start-at-poll-1", lambda: play([(100000, START, 0, 0)]), "Start at poll 1")

    def doc(**fields):
        event = {"buttons": 0, "port": 0, "tick": "1071354", "x": 0, "y": 0}
        event.update(fields)
        return json.dumps({"events": [event], "schema": "t64.input", "schema_version": 1}, sort_keys=True)

    test.equal("parser-accepts-canonical", lambda: parse_input(doc()), [(1071354, 0, 0, 0)])
    for name, text, reason in (
        ("unknown-key", doc(extra=1), "keys are not exactly"),
        ("tick-not-decimal", doc(tick="12a"), "canonical decimal"),
        ("tick-negative", doc(tick="-5"), "canonical decimal"),
        ("tick-unicode-digits", doc(tick="\uff16\uff10\uff10"), "canonical decimal"),
        ("tick-leading-zero", doc(tick="0600000"), "canonical decimal"),
        ("tick-number", doc(tick=600000), "canonical decimal"),
        ("tick-over-64-bits", doc(tick=str(2**64)), "exceeds 64 bits"),
        ("port-1", doc(port=1), "port is not 0"),
        ("buttons-boolean", doc(buttons=True), "not an integer"),
        ("buttons-float", doc(buttons=1.0), "not an integer"),
        ("buttons-range", doc(buttons=65536), "outside 0 to 65535"),
        ("axis-range", doc(x=128), "axis is outside"),
        ("axis-range-low", doc(y=-129), "axis is outside"),
        ("schema-version", doc().replace('"schema_version": 1', '"schema_version": 2'), "schema is not"),
        ("schema-name", doc().replace("t64.input", "t64.other"), "schema is not"),
        ("duplicate-key", doc().replace('"port": 0', '"port": 0, "port": 0'), "duplicate key"),
        ("not-json", "{", "not valid JSON"),
        ("top-level-extra", doc()[:-1] + ', "more": 1}', "schema is not"),
        ("events-not-list", '{"events": 1, "schema": "t64.input", "schema_version": 1}', "schema is not"),
        ("event-not-object", '{"events": [1], "schema": "t64.input", "schema_version": 1}', "keys are not exactly"),
    ):
        test.refuses("parser-refuses-" + name, lambda text=text, reason=reason: parse_input(text), reason)
    test.refuses("parser-refuses-backwards-ticks", lambda: parse_input(json.dumps({
        "events": [
            {"buttons": 0, "port": 0, "tick": "1071354", "x": 0, "y": 0},
            {"buttons": 0, "port": 0, "tick": "1071353", "x": 0, "y": 0},
        ],
        "schema": "t64.input", "schema_version": 1,
    })), "go backwards")  # fmt: skip
    test.refuses("parser-refuses-oversize", lambda: parse_input(doc() + " " * 70000), "larger than 65536")
    # TS-12 slot and boot load (persist scenario): restored from the long run's image.
    long_image = (lambda: play(long)["image"])
    test.equal("slot-parse-long", lambda: parse_slot(long_image()), (60, 44, 2))
    test.equal("slot-parse-blank", lambda: parse_slot(bytes(32768)), None)
    test.equal("slot-parse-x-113", lambda: parse_slot(slot_image(113, 40, 1)), None)
    test.equal("slot-parse-y-81", lambda: parse_slot(slot_image(56, 81, 1)), None)
    test.equal("slot-parse-bad-magic", lambda: parse_slot(b"\x54\x36\x34\x55" + bytes(32764)), None)
    test.equal(
        "persist-positions",
        lambda: play([], fields=3, restore=parse_slot(long_image()))["positions"],
        [(60, 44)] * 3,
    )
    test.equal(
        "persist-transcript",
        lambda: play([], fields=3, restore=parse_slot(long_image()))["transcript"],
        "L 3c 2c 00000002\n",
    )
    test.equal(
        "persist-quiet",
        lambda: (
            play([], fields=3, restore=parse_slot(long_image()))["bursts"],
            play([], fields=3, restore=parse_slot(long_image()))["completes"],
        ),
        ([], False),
    )
    test.equal(
        "persist-differs-from-default",
        lambda: play([], fields=1, restore=parse_slot(long_image()))["frames"]
        != play([], fields=1, restore=None)["frames"],
        True,
    )
    # TS-14: short scripts finish below the runner cap; a fourth field would not.
    test.case(
        "short-run-below-runner-cap",
        boundary(3) + 60000 == 3738126 and boundary(3) + 60000 < 4000000 <= boundary(4) == 4720834,
        "B(3) + 60000 = %d, B(4) = %d" % (boundary(3) + 60000, boundary(4)),
    )
    test.equal(
        "input-text-short",
        lambda: input_text("short"),
        '{"events":[{"buttons":33024,"port":0,"tick":"1071354","x":0,"y":0},'
        '{"buttons":16384,"port":0,"tick":"2114063","x":0,"y":0},'
        '{"buttons":4096,"port":0,"tick":"3156771","x":0,"y":0}],"schema":"t64.input","schema_version":1}\n',
    )
    # TS-8 tone table: the first period by hand (32 frames of +0x2000, then 32 of -0x2000).
    table = tone_table()
    test.equal(
        "tone-table-shape",
        lambda: (len(table), table[0:4].hex(), table[124:128].hex(), table[128:132].hex(), table[252:256].hex(), table[256:260].hex(), table[2044:2048].hex()),
        (2048, "20002000", "20002000", "e000e000", "e000e000", "20002000", "e000e000"),
    )
    test.equal("blank-save-digest", lambda: sha256(bytes(32768)), "c35020473aed1b4642cd726cad727b63fff2824ad68cedd7ffb73c7cbd890479")
    test.equal(
        "burst-audio-bytes",
        lambda: [sha256(packet) for packet in play(short)["audio"]],
        [sha256(table), sha256(table)],
    )
    # The expectation document keeps its promises (DR-05, DR-06, T-12-06).
    def document_checks():
        document = expected_document()
        scenarios = document["scenarios"]
        blank = document["blank_sha256"]
        return (
            set(scenarios) == {"short", "short-wrong", "long", "shifted", "persist"},
            len(scenarios["short"]["frames"]) <= 3 and len(scenarios["short-wrong"]["frames"]) <= 3,
            len(scenarios["long"]["frames"]) >= 10 and len(scenarios["shifted"]["frames"]) >= 10,
            all(frame != blank for name in scenarios for frame in scenarios[name]["frames"]),
            scenarios["long"]["frames"] != scenarios["shifted"]["frames"],
            scenarios["short"]["frames"][1] != scenarios["short-wrong"]["frames"][1],
            scenarios["persist"]["save"]["sha256"] == scenarios["long"]["save"]["sha256"],
            document["refused"]["frames_each_sha256"] == blank == BLANK_SHA256_LITERAL,
        )

    test.equal("document-promises", document_checks, (True,) * 8)
    test.equal("main-missing-input-exits-2", lambda: quiet_main(["--print", "--input", "no-such-title-script"]), 2)
    test.equal("main-unknown-flag-exits-2", lambda: quiet_main(["--no-such-flag"]), 2)
    test.equal("main-input-without-print-exits-2", lambda: quiet_main(["--input", "x"]), 2)
    test.equal("main-write-and-check-exit-2", lambda: quiet_main(["--write", "x", "--check", "x"]), 2)
    # The scenario table (TS-14) equals the hand-written rows of this selftest.
    test.equal("table-long", lambda: scenario_events("long"), [(j, b, x, y) for j, b, x, y in long_rows])
    test.equal("table-short", lambda: [(j, b) for j, b, _x, _y in scenario_events("short")], [(0, RIGHT | A), (1, B), (2, START)])
    test.equal("table-short-wrong", lambda: [(j, b) for j, b, _x, _y in scenario_events("short-wrong")], [(0, LEFT | A), (1, B), (2, START)])
    test.equal("table-shifted", lambda: scenario_events("shifted"), [(j + 1, b, x, y) for j, b, x, y in long_rows])
    test.equal(
        "emitted-scripts-equal-hand-stamps",
        lambda: {name: parse_input(input_text(name)) for name in ("short", "short-wrong", "long", "shifted")},
        {"short": short, "short-wrong": short_wrong, "long": long, "shifted": shifted},
    )
    test.refuses("unknown-scenario", lambda: scenario_events("middle"), "unknown scenario")
    test.refuses("poll-limit", lambda: play([(midpoint(80), START, 0, 0)]), "more than 64 polls")
    test.equal("far-stamp-is-fast", lambda: nearest_boundary_distance(10**15) >= 0, True)


def quiet_main(argv):
    """Run main() in process with the standard error stream silenced; return its status."""
    saved = sys.stderr
    sys.stderr = io.StringIO()
    try:
        return main(argv)
    finally:
        sys.stderr = saved


def run_selftest() -> int:
    test = Selftest()
    selftest_tracer(test)
    selftest_model(test)
    print("selftest: %d cases, %d failed" % (test.cases, test.failures))
    return 1 if test.failures else 0


# ---- command line -----------------------------------------------------------------


def check_file(path: str, text: str) -> int:
    with open(path, "rb") as handle:
        committed = handle.read()
    data = text.encode("ascii")
    if committed == data:
        print("title expectation equals the oracle (%d bytes)" % len(data))
        return 0
    index = 0
    while index < min(len(committed), len(data)) and committed[index] == data[index]:
        index += 1
    print(
        "title expectation differs from the oracle at byte %d (file %d bytes, oracle %d bytes)"
        % (index, len(committed), len(data)),
        file=sys.stderr,
    )
    return 1


def read_script(path: str):
    with open(path, "rb") as handle:
        data = handle.read(MAX_SCRIPT_BYTES + 1)
    return parse_input(data.decode("ascii"))


def input_path(name: str) -> str:
    return os.path.join(HERE, "title.%s.input.json" % name)


def emit_inputs(directory: str) -> None:
    for name in SCENARIO_NAMES:
        with open(os.path.join(directory, "title.%s.input.json" % name), "wb") as handle:
            handle.write(input_text(name).encode("ascii"))


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--selftest", action="store_true", help="run the built-in cases")
    parser.add_argument("--print", action="store_true", dest="show", help="print the expectation document")
    parser.add_argument("--write", metavar="FILE", help="write the committed expectation")
    parser.add_argument("--check", metavar="FILE", help="compare FILE with the regenerated text")
    parser.add_argument("--input", metavar="PATH", help="with --print: model this one input script")
    parser.add_argument("--max-fields", metavar="N", type=int, help="with --input: refuse more rendered fields")
    parser.add_argument("--emit-inputs", metavar="DIR", help="write the four scenario input scripts into DIR")
    try:
        args = parser.parse_args(argv)
    except SystemExit as error:
        return 2 if error.code else 0
    if args.write is not None and args.check is not None:
        print("title oracle refused: --write and --check are exclusive", file=sys.stderr)
        return 2
    if (args.input is not None or args.max_fields is not None) and not args.show:
        print("title oracle refused: --input and --max-fields need --print", file=sys.stderr)
        return 2
    if args.max_fields is not None and args.input is None:
        print("title oracle refused: --max-fields needs --input", file=sys.stderr)
        return 2
    if not (args.selftest or args.show or args.write is not None or args.check is not None
            or args.emit_inputs is not None):  # fmt: skip
        print("title oracle refused: choose --selftest, --print, --write, --check or --emit-inputs", file=sys.stderr)
        return 2
    status = 0
    try:
        if args.selftest:
            status = max(status, run_selftest())
        if args.emit_inputs is not None:
            emit_inputs(args.emit_inputs)
        if args.show and args.input is not None:
            result = play(read_script(args.input), max_fields=args.max_fields)
            sys.stdout.write(document_text(scenario_document(result)))
        elif args.show:
            sys.stdout.write(expected_text())
        if args.write is not None:
            with open(args.write, "wb") as handle:
                handle.write(expected_text().encode("ascii"))
        if args.check is not None:
            status = max(status, check_file(args.check, expected_text()))
    except (Refused, ValueError, KeyError, TypeError, OSError) as error:
        print("title oracle refused: %s" % error, file=sys.stderr)
        return 2
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
