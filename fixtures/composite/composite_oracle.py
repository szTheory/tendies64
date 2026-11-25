#!/usr/bin/env python3
"""Independent oracle for the Phase 10 composite guest (RUN-07).

The model below is written from the authored constants of
fixtures/composite/composite.S, the input script, and the policy text of
docs/spec/rdp.md (V7 depth policy, triangle sample policy), docs/spec/vi.md
(RGBA5551 to RGBA8 expansion), docs/spec/si-pif-controller.md (controller state
encoding and the strictly-after input latch) and docs/spec/save-media.md (blank
SRAM content). Nothing in this file reads a ROM, the core, the runner or any
tool of this repository: it imports only the standard library. Every figure is
a named software-profile policy value; no hardware accuracy is claimed.

It prints one JSON document with the expected outputs of the guest:

  video          the 4x3 RGBA8 scan-out packet (48 bytes) after the Z test
  audio          the two 16-byte S16BE packets, in sequence order
  save_sha256    SHA-256 of the 32,768-byte SRAM image after the guest wrote
                 its 16-byte record, plus the nonzero byte ranges ("save")
  transcript     the text the guest prints through the ISViewer window

and the same for the empty (neutral) input script under the key "empty".

Usage:
  python3 -B fixtures/composite/composite_oracle.py [--input PATH]
      [--variant accepted|wrong]
  python3 -B fixtures/composite/composite_oracle.py --transcript [--input PATH]
      [--variant accepted|wrong]

Without --input the input script is empty. --variant wrong models the ROM built
with --defsym T64_COMPOSITE_WRONG=1: the low bit of the last record word is
flipped (same comparator, wrong value). The exit status is 2 when the input
script is invalid or a stamp lies too close to a controller poll for the
strictly-after rule to decide it (the ordering would depend on the timing).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from fractions import Fraction

# ---- authored constants of fixtures/composite/composite.S ------------------

WIDTH = 4
HEIGHT = 3
PIXELS = WIDTH * HEIGHT

# SetPrimDepth (raw 16-bit depth, delta) and SetPrimColor (R, G, B, A) of the
# near triangle, which is drawn first, and of the far one, drawn second.
NEAR_DEPTH = (0x1357, 9)
FAR_DEPTH = (0x2000, 9)
NEAR_COLOR = (0, 255, 0, 255)
FAR_COLOR = (255, 0, 0, 255)
Z_INIT_WORD = 0xFFFC  # the guest fills the 4x3 Z image with the far word

# The one triangle both draws use (Z triangle opcode 0x09). Y fields are s11.2,
# the X fields and slopes are s15.16. YH = 0, YM = YL = 3 rows; the major edge
# H is vertical at x = 0 (lft bit 0 makes it the left edge); the minor edge
# starts at x = 0 with a slope of +40 pixels per row. XM and XH are both zero
# so reading either as the start of the minor edge gives the same edge. The Z
# plane words are zero (the primitive depth source is selected).
Y_HIGH = Fraction(0)
Y_MID = Fraction(3)
Y_LOW = Fraction(3)
X_MAJOR = Fraction(0)
DX_MAJOR = Fraction(0)
X_MINOR = Fraction(0)
DX_MINOR = Fraction(40)

# Four authored stereo S16BE frames per 16-byte audio packet: (left, right).
AUDIO_FRAMES = (
    ((0x0000, 0x4000), (0x2000, 0xE000), (0x7FFF, 0x8001), (0x0123, 0xFEDD)),
    ((0x1111, 0xEEEF), (0x8000, 0x0001), (0x5A5A, 0xA5A6), (0xFFFF, 0x0000)),
)

# The SRAM record at offset 0x0100: magic "T64C", the last polled word, the sum
# modulo 2^32 and the exclusive or of those two words.
SAVE_SIZE = 32768
RECORD_OFFSET = 0x0100
RECORD_MAGIC = 0x54363443

# The three controller polls execute near these guest ticks. They were measured
# once from the neutral run with a dense probe script (stamps every 400 ticks)
# and stand for ordering only: a stamp within POLL_MARGIN ticks of a nominal
# poll tick is refused as ambiguous, so no result depends on the exact values.
POLL_TICKS = (555600, 573600, 591600)
POLL_MARGIN = 2000

# docs/spec/rdp.md Z exponent table: (shift, add) of exponents 0 to 7.
Z_ROWS = (
    (6, 0x00000), (5, 0x20000), (4, 0x30000), (3, 0x38000),
    (2, 0x3C000), (1, 0x3E000), (0, 0x3F000), (0, 0x3F800),
)
Z_LIMIT = 0x40000  # exclusive end of the 18-bit range


class Refused(Exception):
    """The model does not cover the case (or the input is invalid)."""


# ---- RDP model (docs/spec/rdp.md) ------------------------------------------


def z_compress(linear: int) -> tuple[int, int]:
    """Return (exponent, 11-bit mantissa) for an 18-bit linear Z."""
    linear = max(0, min(linear, Z_LIMIT - 1))
    for exponent in range(7, -1, -1):
        shift, add = Z_ROWS[exponent]
        if linear >= add:
            return exponent, min((linear - add) >> shift, 0x7FF)
    raise AssertionError("unreachable")


def z_decompress(word: int) -> int:
    exponent = (word >> 13) & 7
    mantissa = (word >> 2) & 0x7FF
    shift, add = Z_ROWS[exponent]
    return (mantissa << shift) + add


def primitive_z(raw16: int) -> int:
    """Primitive Z is min(raw, 0x7fff) << 3 (the 15.3 conversion)."""
    return min(raw16, 0x7FFF) << 3


def delta_code(delta: int) -> int:
    """Highest set bit index of the delta (the 4-bit code), 0 for zero."""
    return max(delta.bit_length() - 1, 0)


def samples():
    """The sixteen sample offsets of one pixel: 1/8, 3/8, 5/8, 7/8 each way."""
    offsets = [Fraction(i, 8) for i in (1, 3, 5, 7)]
    for sy in offsets:
        for sx in offsets:
            yield sx, sy


def covered_samples(px: int, py: int) -> int:
    """Count the samples of pixel (px, py) inside the triangle: Y is half-open
    [YH, YL); the left edge is inclusive and the right edge exclusive."""
    count = 0
    for sx, sy in samples():
        y = py + sy
        x = px + sx
        if not (Y_HIGH <= y < Y_LOW):
            continue
        if y >= Y_MID:
            raise Refused("the low edge is not modeled (a sample row is past YM)")
        left = X_MAJOR + DX_MAJOR * (y - Y_HIGH)
        right = X_MINOR + DX_MINOR * (y - Y_HIGH)
        if left <= x < right:
            count += 1
    return count


def quantize_channel(value: int) -> int:
    """5-bit channel; the dither bias (0 to 7) must not change the result for
    the authored 0 and 255 channel values, which is checked over every bias."""
    results = {min(255, value + bias) >> 3 for bias in range(8)}
    if len(results) != 1:
        raise Refused("a dither bias changes a color channel; model does not cover it")
    return results.pop()


def rgba5551(color: tuple[int, int, int, int]) -> int:
    red, green, blue, alpha = color
    if alpha not in (0, 255):
        raise Refused("alpha dither threshold is not modeled for this alpha")
    return (
        (quantize_channel(red) << 11)
        | (quantize_channel(green) << 6)
        | (quantize_channel(blue) << 1)
        | (1 if alpha == 255 else 0)
    )


def draw_frame():
    """Run the two Z-tested draws. Returns (color words, depth words)."""
    color = [0] * PIXELS  # the guest's color image is overwritten by pixels
    written = [False] * PIXELS
    depth = [Z_INIT_WORD] * PIXELS
    sidecar = [0] * PIXELS  # bit 7 valid, low two delta-code bits
    for (raw_z, delta), prim_color in ((NEAR_DEPTH, NEAR_COLOR), (FAR_DEPTH, FAR_COLOR)):
        word = rgba5551(prim_color)
        for index in range(PIXELS):
            px, py = index % WIDTH, index // WIDTH
            covered = covered_samples(px, py)
            if covered == 0:
                continue
            if covered != 16:
                raise Refused("partial coverage is not modeled")
            new_z = min(primitive_z(raw_z), Z_LIMIT - 1)
            old_z = z_decompress(depth[index])
            stored_code = ((depth[index] & 3) << 2) | (
                (sidecar[index] & 3) if sidecar[index] & 0x80 else 0
            )
            max_delta = max(delta, 1 << stored_code)
            if new_z > min(old_z + max_delta, Z_LIMIT - 1):
                continue  # opaque mode, Z compare fails: nothing is written
            code = delta_code(delta)
            exponent, mantissa = z_compress(new_z)
            depth[index] = (exponent << 13) | (mantissa << 2) | (code >> 2)
            sidecar[index] = 0x80 | (code & 3)
            color[index] = word
            written[index] = True
    if not all(written):
        raise Refused("a pixel of the window is never written")
    return color, depth


# ---- VI model (docs/spec/vi.md) --------------------------------------------


def expand5(value: int) -> int:
    return (value << 3) | (value >> 2)


def scanout_packet(color_words) -> bytes:
    """The RGBA8 packet of a fully covered 4x3 RGBA5551 window."""
    out = bytearray()
    for word in color_words:
        out += bytes(
            (
                expand5((word >> 11) & 31),
                expand5((word >> 6) & 31),
                expand5((word >> 1) & 31),
                255 if word & 1 else 0,
            )
        )
    return bytes(out)


# ---- AI model --------------------------------------------------------------


def audio_packets() -> list[bytes]:
    packets = []
    for frames in AUDIO_FRAMES:
        data = bytearray()
        for left, right in frames:
            data += left.to_bytes(2, "big") + right.to_bytes(2, "big")
        packets.append(bytes(data))
    return packets


# ---- SI input model (docs/spec/si-pif-controller.md) -----------------------


def parse_input(text: str):
    document = json.loads(text)
    if (
        not isinstance(document, dict)
        or document.get("schema") != "t64.input"
        or document.get("schema_version") != 1
        or set(document) != {"events", "schema", "schema_version"}
        or not isinstance(document["events"], list)
    ):
        raise Refused("input script schema")
    stamps = []
    previous = 0
    for event in document["events"]:
        if set(event) != {"tick", "port", "buttons", "x", "y"} or event["port"] != 0:
            raise Refused("input event shape")
        tick = int(event["tick"])
        if tick < previous:
            raise Refused("input ticks go backwards")
        previous = tick
        if not (0 <= event["buttons"] <= 0xFFFF and -128 <= event["x"] <= 127
                and -128 <= event["y"] <= 127):
            raise Refused("input event range")
        stamps.append((tick, event["buttons"], event["x"], event["y"]))
    return stamps


def wire_bytes(buttons: int, x: int, y: int) -> bytes:
    """Buttons high byte, buttons low byte with the reserved bits 6 and 7
    masked, then the signed axes as unsigned bytes."""
    return bytes([(buttons >> 8) & 0xFF, buttons & 0x3F, x & 0xFF, y & 0xFF])


def polled_words(stamps) -> list[bytes]:
    """The state each poll reads: the last stamp strictly before the poll tick
    (a later row at an equal tick wins), neutral when none."""
    words = []
    for poll in POLL_TICKS:
        chosen = (0, 0, 0)
        for tick, buttons, x, y in stamps:
            if abs(tick - poll) <= POLL_MARGIN:
                raise Refused(
                    "a stamp at tick %d is within %d ticks of a poll" % (tick, POLL_MARGIN)
                )
            if tick < poll:
                chosen = (buttons, x, y)
        words.append(wire_bytes(*chosen))
    return words


# ---- save model (docs/spec/save-media.md) ----------------------------------


def save_image(polled: bytes, wrong: bool) -> bytes:
    magic = RECORD_MAGIC
    last = int.from_bytes(polled, "big")
    total = (magic + last) & 0xFFFFFFFF
    mixed = magic ^ last
    if wrong:
        mixed ^= 1
    record = b"".join(word.to_bytes(4, "big") for word in (magic, last, total, mixed))
    image = bytearray(SAVE_SIZE)  # blank SRAM is 0x00
    image[RECORD_OFFSET : RECORD_OFFSET + len(record)] = record
    return bytes(image)


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
        ranges.append({"offset": start, "hex": image[start:index].hex()})
    return ranges


# ---- the scenario ----------------------------------------------------------


def scenario(stamps, wrong: bool) -> dict:
    color, depth = draw_frame()
    video = scanout_packet(color)
    audio = audio_packets()
    polled = polled_words(stamps)
    image = save_image(polled[-1], wrong)
    record = image[RECORD_OFFSET : RECORD_OFFSET + 16]
    color_bytes = b"".join(word.to_bytes(2, "big") for word in color)
    depth_bytes = b"".join(word.to_bytes(2, "big") for word in depth)
    lines = [
        "VC %s\n" % color_bytes.hex(),
        "VZ %s\n" % depth_bytes.hex(),
        "I1 %s\n" % polled[0].hex(),
        "I2 %s\n" % polled[1].hex(),
        "I3 %s\n" % polled[2].hex(),
        "SR %s\n" % record.hex(),
        "AI %08x\n" % 4,  # the AI interrupt bit of MI_INTERRUPT after both DMAs
    ]
    transcript = "".join(lines)
    return {
        "video": {
            "width": WIDTH,
            "height": HEIGHT,
            "packet_bytes": len(video),
            "rgba8": video.hex(),
        },
        "audio": {
            "packet_count": len(audio),
            "packet_bytes": 16,
            "packets": [packet.hex() for packet in audio],
        },
        "save_sha256": hashlib.sha256(image).hexdigest(),
        "save": {"bytes": len(image), "nonzero": nonzero_ranges(image)},
        "polled": [word.hex() for word in polled],
        "transcript": transcript,
        "transcript_bytes": len(transcript.encode("ascii")),
        "transcript_sha256": hashlib.sha256(transcript.encode("ascii")).hexdigest(),
    }


def build(stamps, wrong: bool) -> dict:
    result = scenario(stamps, wrong)
    result["empty"] = scenario([], wrong)
    result["schema"] = "t64.composite-oracle"
    result["schema_version"] = 1
    result["variant"] = "wrong" if wrong else "accepted"
    return result


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", help="input script (default: the empty script)")
    parser.add_argument("--variant", choices=("accepted", "wrong"), default="accepted")
    parser.add_argument("--transcript", action="store_true", help="print only the transcript")
    args = parser.parse_args(argv)
    try:
        stamps = []
        if args.input is not None:
            with open(args.input, "r", encoding="ascii") as handle:
                stamps = parse_input(handle.read())
        result = build(stamps, args.variant == "wrong")
    except (Refused, ValueError, KeyError, TypeError, OSError) as error:
        print("composite oracle refused: %s" % error, file=sys.stderr)
        return 2
    if args.transcript:
        sys.stdout.write(result["transcript"])
    else:
        print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
