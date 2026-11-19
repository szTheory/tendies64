#!/usr/bin/env python3
"""Independent transcript oracle for the Phase 9 eeprom-rw and sram-rw ROMs.

The EEPROM and SRAM models below are written from the policy literals of
docs/spec/save-media.md (8-byte blocks, a 64-block or 256-block table, the 4
Kbit mirror written as an explicit modulo rule, handshake totals computed from
the joybus tick formula, a stored busy deadline compared with the execute tick
of each transaction). Nothing in this file reads a ROM, the core or the
runner: the expected transcript is derived from the rules below and nothing
else. Every figure is a named software-profile policy value; no hardware
accuracy or hardware timing is claimed.

Usage:
  python3 -B fixtures/save/save_oracle.py [--scenario NAME] --print|--sha256|--bytes
  python3 -B fixtures/save/save_oracle.py --check-margins

Scenarios: eeprom-4k (default), eeprom-16k, eeprom-wrong-4k, sram-32k,
sram-wrong-32k.
"""

from __future__ import annotations

import argparse
import hashlib
import sys

# ---- timing literals (named policy; ticks of the 62.5 MHz model clock) -------

BYTE_TICKS = 2000  # one joybus byte
CONSOLE_STOP = 188
TX_RX_GAP = 438
DEVICE_STOP = 250
READ_TAIL = 4096  # completion tail after the execute tick
WRITE_DMA = 2500  # WR64B completion
BUSY_WINDOW = 375000  # eeprom-busy-ceiling-6ms-v1 (named policy, not measured)
WAIT_TICKS = 440000  # 220,000 Count units, two ticks per Count unit
BLOCK = 8


def handshake(tx: int, rx: int) -> int:
    """A valid exact-length transaction: 2000 n + 188 + 438 + 2000 m + 250."""
    return BYTE_TICKS * tx + CONSOLE_STOP + TX_RX_GAP + BYTE_TICKS * rx + DEVICE_STOP


H_INFO = handshake(1, 3)
H_READ = handshake(2, 8)
H_WRITE = handshake(10, 1)
assert (H_INFO, H_READ, H_WRITE) == (8876, 20876, 22876)


class Eeprom:
    """Blocks of eight bytes; blank is 0xFF. The 4 Kbit part has 64 blocks and
    repeats them for block indexes 64 to 255 (an explicit modulo rule); the 16
    Kbit part has 256 distinct blocks. The deadline is the tick from which the
    part answers idle."""

    def __init__(self, blocks: int) -> None:
        self.blocks = blocks
        self.table = [bytes([0xFF] * BLOCK) for _ in range(blocks)]
        self.deadline = 0

    def slot(self, index: int) -> int:
        return index % self.blocks

    def info(self, tick: int) -> tuple[int, bytes]:
        ident = 0x80 if self.blocks == 64 else 0xC0
        status = 0x80 if tick < self.deadline else 0x00
        return 0x03, bytes([0x00, ident, status])

    def read(self, tick: int, index: int) -> tuple[int, bytes]:
        if tick < self.deadline:
            return 0x08 | 0x40, bytes([0xFF] * BLOCK)  # fill bytes untouched
        return 0x08, self.table[self.slot(index)]

    def write(self, tick: int, index: int, data: bytes) -> tuple[int, bytes]:
        if tick < self.deadline:
            return 0x01, bytes([0x80])  # already busy: nothing written
        self.table[self.slot(index)] = data
        self.deadline = tick + BUSY_WINDOW
        return 0x01, bytes([0x00])


def eeprom_transcript(blocks: int, wrong: bool, extra_gap: int) -> bytes:
    header = "EEPROM 4k\n" if blocks == 64 else "EEPROM 16k\n"
    last = 0x19 if wrong else 0x18
    w1 = bytes([0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, last])
    w2 = bytes([0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28])
    w3 = bytes([0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48])
    # (tag, kind, block index, data); "wait" rows re-anchor on a write start.
    script = [
        ("I1", "info", 0, None),
        ("W1", "write", 0x01, w1),
        ("I2", "info", 0, None),
        ("R1", "read", 0x01, None),
        ("W2", "write", 0x02, w2),
        ("WAIT", None, 0, None),
        ("I3", "info", 0, None),
        ("R2", "read", 0x01, None),
        ("R5", "read", 0x02, None),
        ("W3", "write", 0x41, w3),
        ("WAIT", None, 0, None),
        ("R3", "read", 0x01, None),
        ("R4", "read", 0x41, None),
    ]
    part = Eeprom(blocks)
    lines = [header]
    now = 100000  # the guest time at the first transaction (any value works)
    anchor = 0
    for tag, kind, index, data in script:
        if kind is None:
            now = max(now, anchor + WAIT_TICKS)
            continue
        now += extra_gap
        now += WRITE_DMA  # WR64B moves the frame
        start = now  # RD64B start tick
        if kind == "write":
            anchor = start  # the wait anchors on the write's own start tick
        handshake_ticks = {"info": H_INFO, "read": H_READ, "write": H_WRITE}[kind]
        execute = start + handshake_ticks
        if kind == "info":
            rx_byte, rx = part.info(execute)
        elif kind == "read":
            rx_byte, rx = part.read(execute, index)
        else:
            rx_byte, rx = part.write(execute, index, data)
        lines.append(f"{tag} {rx_byte:02x} {rx.hex()}\n")
        now = execute + READ_TAIL
    return "".join(lines).encode("ascii")


def sram_transcript(wrong: bool) -> bytes:
    storage = bytearray(32768)  # blank is zero
    pattern = bytes.fromhex("00112233445566778899aabbccddeeff")
    assert len(pattern) == 16
    if wrong:
        pattern = pattern[:-1] + bytes([0xFE])
    lines = ["SRAM 32k\n"]
    lines.append("Z " + bytes(storage[0x7FFC:0x8000]).hex() + "\n")
    storage[0x100:0x110] = pattern  # PI read-length DMA, RDRAM to SRAM
    returned = bytes(storage[0x100:0x110])  # PI write-length DMA back
    lines.append("D " + returned.hex() + "\n")
    storage[0x104:0x108] = bytes([0x12, 0x34, 0x56, 0x78])  # CPU word store
    lines.append("C " + bytes(storage[0x104:0x108]).hex() + "\n")
    returned = bytes(storage[0x100:0x110])  # second read-back DMA
    lines.append("M " + returned.hex() + "\n")
    return "".join(lines).encode("ascii")


SCENARIOS = {
    "eeprom-4k": lambda gap: eeprom_transcript(64, False, gap),
    "eeprom-16k": lambda gap: eeprom_transcript(256, False, gap),
    "eeprom-wrong-4k": lambda gap: eeprom_transcript(64, True, gap),
    "sram-32k": lambda gap: sram_transcript(False),
    "sram-wrong-32k": lambda gap: sram_transcript(True),
}


def transcript(scenario: str, extra_gap: int = 0) -> bytes:
    return SCENARIOS[scenario](extra_gap)


def check_margins() -> int:
    """Rerun every EEPROM scenario with extra inter-transaction gaps of 0 and
    50,000 ticks: the transcripts must be identical, and the wait must exceed
    the busy window plus the write handshake with margin."""
    failures = 0
    margin = WAIT_TICKS - (BUSY_WINDOW + H_WRITE)
    if margin < 40000:
        print(f"wait margin too small: {margin}", file=sys.stderr)
        failures += 1
    for name in ("eeprom-4k", "eeprom-16k", "eeprom-wrong-4k"):
        base = transcript(name, 0)
        for gap in (0, 50000):
            if transcript(name, gap) != base:
                print(f"{name}: transcript changes with a {gap}-tick gap",
                      file=sys.stderr)
                failures += 1
    # The busy rows (I2, R1, W2) need to stay inside the window with the
    # larger gap: three transactions of at most gap + 2500 + handshake + tail.
    worst = 3 * (50000 + WRITE_DMA + H_WRITE + READ_TAIL)
    if worst >= BUSY_WINDOW:
        print(f"busy rows leave the window under a 50,000-tick gap: {worst}",
              file=sys.stderr)
        failures += 1
    if failures == 0:
        print(f"margins ok: wait margin {margin} ticks, busy-row worst case {worst}")
    return 1 if failures else 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", choices=sorted(SCENARIOS), default="eeprom-4k")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--print", action="store_true", dest="show")
    mode.add_argument("--sha256", action="store_true")
    mode.add_argument("--bytes", action="store_true", dest="count")
    mode.add_argument("--check-margins", action="store_true")
    args = parser.parse_args(argv)
    if args.check_margins:
        return check_margins()
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
