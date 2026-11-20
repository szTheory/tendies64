#!/usr/bin/env python3
"""Independent oracle for the Phase 9 save-roundtrip ROM and the save import
and export files.

The model below is written from the policy literals of docs/spec/save-media.md
and the plan text of 09-05 and nothing else: the authored image formula, the
two guest blocks, the three byte-order selections written as an explicit offset
mapping (SWAP16 is offset XOR 1, WORDREV32 is offset XOR 3, RAW is identity),
and the four-line transcript of the ROM. Nothing in this file reads a ROM, the
core or the runner. The transform definitions are authored policy (assumption
A1), not a hardware fact, and no other emulator's convention is asserted.
Every figure is a named software-profile policy value; no hardware accuracy or
hardware timing is claimed.

Usage:
  python3 -B fixtures/save/roundtrip_oracle.py [--scenario NAME] --print|--sha256|--bytes
  python3 -B fixtures/save/roundtrip_oracle.py --image-sha256 import|export
      [--kind sram32k|eeprom4k|eeprom16k] [--format raw|swap16|wordrev32]

Scenarios: baseline (default), swap16-as-raw (an image encoded SWAP16 but
declared RAW).

--image-sha256 import is the SHA-256 of the import file: the authored image of
the kind encoded in the format. --image-sha256 export is the SHA-256 of the
file the runner publishes after a pass: the final image of the kind (for the
sram32k roundtrip ROM the authored image with the W and V blocks rewritten; for
the EEPROM kinds the authored image, because their admit ROMs write nothing)
encoded in the format.
"""

from __future__ import annotations

import argparse
import hashlib
import sys

SIZES = {"sram32k": 32768, "eeprom4k": 512, "eeprom16k": 2048}
XOR_MASKS = {"raw": 0, "swap16": 1, "wordrev32": 3}

# Guest literals of fixtures/save/roundtrip.S (16 bytes each) and the SRAM
# offsets they are written at.
W_BLOCK = bytes.fromhex("c35a96e10f78b24d218ef63469d017ab")
V_BLOCK = bytes.fromhex("18e57cb903d64a91ff2ea760c8358b5e")
W_OFFSET = 0x0100
V_OFFSET = 0x7FF0
BLOCK = 16


def image_byte(index: int) -> int:
    """Authored formula: (37 i + 11 + 101 (i >> 8)) modulo 256. Every 4-byte
    word holds four distinct bytes, so SWAP16 and WORDREV32 differ."""
    return (37 * index + 11 + 101 * (index >> 8)) % 256


def authored_image(size: int) -> bytes:
    return bytes(image_byte(i) for i in range(size))


def encode(raw: bytes, selection: str) -> bytes:
    """The byte at offset i of the encoded image is the byte at offset
    i XOR mask of the RAW image. The permutation is an involution, so the same
    function also decodes."""
    mask = XOR_MASKS[selection]
    return bytes(raw[i ^ mask] for i in range(len(raw)))


def rewritten(stored: bytes) -> bytes:
    """The SRAM content after the guest wrote its two blocks."""
    image = bytearray(stored)
    image[W_OFFSET : W_OFFSET + BLOCK] = W_BLOCK
    image[V_OFFSET : V_OFFSET + BLOCK] = V_BLOCK
    return bytes(image)


def transcript(scenario: str) -> bytes:
    raw_image = authored_image(SIZES["sram32k"])
    if scenario == "baseline":
        import_file = encode(raw_image, "raw")
    elif scenario == "swap16-as-raw":
        import_file = encode(raw_image, "swap16")
    else:
        raise ValueError(scenario)
    # The runner declares RAW for both scenarios, so the stored bytes are the
    # import file unchanged.
    stored = import_file
    after = rewritten(stored)
    lines = [
        ("R0", stored[0:BLOCK]),
        ("R1", stored[V_OFFSET : V_OFFSET + BLOCK]),
        ("W0", after[W_OFFSET : W_OFFSET + BLOCK]),
        ("W1", after[V_OFFSET : V_OFFSET + BLOCK]),
    ]
    return "".join("%s %s\n" % (tag, data.hex()) for tag, data in lines).encode("ascii")


SCENARIOS = ("baseline", "swap16-as-raw")


def image_digest(direction: str, kind: str, selection: str) -> str:
    size = SIZES[kind]
    raw_image = authored_image(size)
    if direction == "import":
        data = encode(raw_image, selection)
    else:
        final = rewritten(raw_image) if kind == "sram32k" else raw_image
        data = encode(final, selection)
    return hashlib.sha256(data).hexdigest()


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", choices=SCENARIOS, default="baseline")
    parser.add_argument("--kind", choices=sorted(SIZES), default="sram32k")
    parser.add_argument("--format", choices=sorted(XOR_MASKS), default="raw", dest="selection")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--print", action="store_true", dest="show")
    mode.add_argument("--sha256", action="store_true")
    mode.add_argument("--bytes", action="store_true", dest="count")
    mode.add_argument("--image-sha256", choices=("import", "export"), dest="direction")
    args = parser.parse_args(argv)
    if args.direction is not None:
        print(image_digest(args.direction, args.kind, args.selection))
        return 0
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
