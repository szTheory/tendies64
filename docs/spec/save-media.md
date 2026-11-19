# Save media: EEPROM, SRAM and the save-media state section

## Scope and evidence

Three of the four composite family ids carry an explicit save device:

| Profile id | `save_kind` | Device | Storage |
| --- | --- | --- | --- |
| `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` | 0 | none | none |
| `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k` | 1 | EEPROM, 4 Kbit | 512 bytes (64 blocks of 8 bytes) |
| `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k` | 2 | EEPROM, 16 Kbit | 2,048 bytes (256 blocks of 8 bytes) |
| `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k` | 3 | SRAM | 32,768 bytes |

Every figure, rule and label in this document is a named software-profile
policy. The facts behind them rest on community and vendor documentation that
this project has not checked against any physical console, and the evidence
level of each rule is stated where it appears. No sentence here is a claim about
measured hardware timing, hardware behavior, or any property of a game; the
tests prove that the model follows the rules below and nothing more. The
`save_policy` label of descriptor 12 is
`save-explicit-eeprom4k16k-sram32k-busy6ms-v1`.

Not part of this slice: import and export of save bytes, the byte formats of
those operations, any file or other persistence, and the replacement table
rows beyond the one stated under Storage and reset. Nothing here is an API or
ABI freeze; the status values, the kind constants, `t64_save_info` and
`t64_machine_save_info` are provisional until the Phase 10 interface review, and
only the kind numbering above and the `save_policy` label stay fixed for the
rest of Phase 9.

## Profile selection

The device comes from the profile `save_kind` and from nothing else. The ROM
header, the title, the game code, the checksum bytes and any file name are never
inputs: a header whose version byte (byte `0x3F`) carries the values 0, 1, 2,
3, 0x10 or 0xFF admits and runs on every id with the same device behavior, and
the same ROM bytes on the id without a device see no device on channel 4. No
header or database detection of a save type exists, and none is planned in this
slice.

## EEPROM commands on channel 4

The cartridge joybus channel (channel 4, the fifth entry of a joybus frame) is
answered by the profile's EEPROM. Classification of a transaction is static: it
uses only the profile kind, the declared transmit count, the declared receive
count and the command byte, because the handshake total of a read DMA is planned
from the table before the execute tick exists and so cannot depend on the busy
state. The three exact triples below are valid; any other transaction on an
EEPROM profile is a timeout; a profile without an EEPROM is absent.

| Command | Transmit | Receive | Idle answer |
| --- | --- | --- | --- |
| `0x00` info | 1 | 3 | id bytes `00 80` (4 Kbit) or `00 C0` (16 Kbit), then the status byte `00` |
| `0x04` read | 2 (command, block) | 8 | the eight bytes of the block |
| `0x05` write | 10 (command, block, 8 data bytes) | 1 | `00`, the block is replaced at acceptance |

The receive count byte of the frame is rebuilt from the parsed table on every
execution, with the flag bits or-ed in: `0x40` for a timeout and `0x80` for no
device. A timeout or absent answer writes no receive data and writes nothing to
the storage; the 0xFF fill bytes stay as they were. The costs are
`2000 n + 188 + 438 + 2000 m + 250` ticks for a valid transaction with n
transmit and m receive bytes (info 8,876, read 20,876, write 22,876) and
`2000 n + 188 + 3907` for a timeout or absent one. A read while the part is busy
is valid at plan time and answers the timeout flag at its execute tick with no
data; the hardware behavior of a busy read is unknown, so this is a named
policy. A write while busy answers `0x80` in its data byte, writes nothing and
leaves the deadline alone; the info status byte is `0x80` while busy. A frame
with the transmit flag `0x80` on its channel 4 entry is skipped by the joybus
executor and writes nothing. These rules are the exact-length command rule of
the Joybus documentation cited below; they are a single-source community fact
restated here as policy, not independently verified.

## Block rule

The 4 Kbit part has 64 blocks. The documented behavior, used here as the named
policy `block & 0x3F`, is that the two high bits of the block index are ignored,
so block indexes 64 to 255 repeat blocks 0 to 63: a write to index `0x41` is
visible at `0x01`, `0x41`, `0x81` and `0xC1`. The 16 Kbit part keeps all 256
indexes distinct. The mirror is documented by one source and is not
independently verified. The alternative, refusing indexes 64 to 255 on the 4
Kbit part, is recorded as `eeprom-4k-block-refuse` and is not implemented.

## Busy window and its policy label

A write that is accepted at the execute tick E stores a deadline
`busy_until = E + 375,000` with a checked 64-bit addition (an overflowing sum is
refused with the timeout flag and writes nothing). The deadline is compared with
the execute tick of each later transaction: busy while the tick is below the
deadline, idle at exactly the deadline and after. No scheduler event exists for
the window. The deadline is derived from the execute tick E and never from the
RD64B store tick, the SI start tick or the completion tick; a write while busy
does not extend it; a stale deadline is kept as it is until the next accepted
write and is part of the state digest.

The label is `eeprom-busy-ceiling-6ms-v1`. The figure 375,000 is the 62.5 MHz
tick count of 6 ms, the ceiling in the Joybus documentation ("up to 6
milliseconds"), used as a fixed deterministic duration. It is a policy built on
an upper bound; it is neither an exact value nor a measured one. Other figures
that appear in older sources (15 ms in the vendor manual, 30 ms in an older
revision of the community page) are uncorroborated and unused. A later physical
measurement would be an owner stop under AUTONOMY section 4 and is not part of
this project's authorized work.

## SRAM window and PI DMA

On the `-s32k` id the window `[0x08000000, 0x08008000)` is carved out of the PI
domain 2 unsupported arm of the bus. It is reached by aligned 32-bit CPU or host
accesses (big-endian words in the storage) and by PI DMA. Widths 1, 2 and 8, a
misaligned word, every address outside the window (there is no mirroring and no
banking) and any cache write-back are refused exactly as on the profiles without
the window, with the specific bus status of the unchanged validation path
(`BUS_UNSUPPORTED` for the width and window rows, `BUS_ALIGNMENT` for a
misaligned word). The base address, the size, the 32-bit access rule, the blank
contents and the absence of mirroring are policy resting on one community
source.

The PI read-length register (`0x04600008`) starts a DMA from RDRAM to the SRAM
and the PI write-length register (`0x0460000C`) starts a DMA from the SRAM to
RDRAM when the latched cart address lies in the window; the ROM path is
unchanged. The duration is the existing `1 + N / 2` ticks, one scheduler event
`(PI, complete)`, and the effect is atomic at the completion event: the source
is read at that event, a controller reset before it leaves the storage and RDRAM
unchanged, and the PI interrupt is pending from it. The alignment rules are the
existing ones (even length, even cart address, DRAM address a multiple of 8);
the DRAM span must lie in the installed RDRAM and the cart span in the window.
Every refusal returns `BUS_DEVICE_REFUSED` (or the specific capacity status) and
leaves the storage unchanged. Every other profile still latches the read-length
write and refuses the start, and a write-length start with an SRAM cart address
is refused there.

## Channel 4 without EEPROM

A profile without an EEPROM (the stem id and the `-s32k` id) answers every
channel 4 transaction with the no-device flag `0x80` in the receive count byte,
untouched fill bytes and the cost of a timeout, and writes nothing.

## Unsupported and deferred requests

`t64_machine_save_info(machine, requested_kind, info_out)` runs these checks in
this exact order: argument pointers (`INVALID_ARGUMENT`, output untouched),
admitted (`NOT_READY`), the profile has a device (a profile with none answers
`SAVE_DEVICE_MISMATCH` for any kind), the kind is known (0 and values above 7
answer `SAVE_UNKNOWN_DEVICE`), the kind is supported (4 to 7 answer
`SAVE_UNSUPPORTED_DEVICE`), the kind equals the profile kind (otherwise
`SAVE_DEVICE_MISMATCH`). On success it reports the kind, the storage size and
the flag `T64_SAVE_INFO_BUSY` while an EEPROM deadline is in the future or an
SRAM DMA is active.

| Status | Value |
| --- | --- |
| `T64_STATUS_SAVE_UNKNOWN_DEVICE` | 20 |
| `T64_STATUS_SAVE_UNSUPPORTED_DEVICE` | 21 |
| `T64_STATUS_SAVE_DEVICE_MISMATCH` | 22 |

The reserved requestable kinds are FlashRAM (4), Controller Pak (5), Rumble Pak
(6) and Transfer Pak (7); they are known and unsupported, and descriptor
creation still refuses them as a `save_kind`. FlashRAM, the paks, the real-time
clock and 64DD media are refusal-only: no stub appears to work, no banked or
mirrored SRAM exists, and no automatic persistence exists. A wire-level request
for one of them (an RTC or pak command on channel 4 of an EEPROM id, a window
access on an EEPROM id) answers its specific flag or bus status. Every refusal
leaves the digest, the storage, the deadline, the queue, the tick and the
sequence counter unchanged.

## Storage and reset

The storage is one trailing region of the single machine allocation, aligned to
8 and sized by the profile kind alone; there is no extra allocation and no file
access. It is filled with `0xFF` (EEPROM) or `0x00` (SRAM) with a zero deadline
at creation and at the reset site of ROM admission, so a successful replacement
by `t64_machine_admit_rom` on an admitted machine restores a blank device and
the digest of a freshly created machine of the same id. A refused replacement
(for example an invalid image) leaves the bytes and the deadline unchanged. The
rest of the replacement matrix belongs to a later plan.

## Section 11 field table

Section 11 of the v25 digest stream follows section 10 and exists for the three
save ids only; the stem id keeps markers 1 to 10 and its digest is unchanged.
Every field is big-endian and in the new-span class, in this order:

| Field | Width | Meaning |
| --- | --- | --- |
| `kind` | u32 | the profile `save_kind` (1, 2 or 3) |
| `size_bytes` | u32 | 512, 2,048 or 32,768 |
| `busy_until` | u64 | the stored EEPROM deadline (0 until a write; a stale value is kept) |
| `pi_save_direction` | u32 | the active PI direction: 0 none or ROM, 1 SRAM to RDRAM, 2 RDRAM to SRAM |
| storage | `size_bytes` bytes | the raw storage |

The test suite carries an independent encoder written from this table and
compares it with the production bytes for the blank, written, mid-deadline,
stale, word-written and mid-DMA states, and flips each field family once to show
that every one changes the digest. The strip differential against the Phase 8
stream still holds for the three save ids. The section content is provisional
until the state content freeze at the end of plan 09-06.

## Authored fixtures and transcript oracle

`fixtures/save/eeprom.S` and `fixtures/save/sram.S` are authored guest programs
(no third-party code); `scripts/build-save-fixtures.sh` builds the five ROMs from
them through assembler defsyms, each embedding only the already cleared openIPL3
payload. The EEPROM program runs thirteen steps (info, write, info while busy,
read while busy, write while busy, a wait, info, read, a read of an unwritten
block, a second write to block `0x41`, a second wait, and two reads) and each
wait spins until 440,000 guest ticks (220,000 Count units) after the start of
the preceding write, well beyond the 375,000-tick window. The SRAM program writes
a 16-byte pattern to the window by DMA, reads it back by DMA, stores one CPU word
inside it and reads the region back again.

| ROM id | ROM SHA-256 | Size | Transcript | Transcript SHA-256 |
| --- | --- | --- | --- | --- |
| `phase09-eeprom-rw-e4k-v1` | `0f3cf3c7...e9` | 5,584 | 191 bytes | `03891120...6502` |
| `phase09-eeprom-rw-e16k-v1` | `af3267db...6812` | 5,584 | 192 bytes | `d9a5bf95...ca95` |
| `phase09-eeprom-rw-wrong-e4k-v1` | `4cab313e...9cec` | 5,584 | 191 bytes (R2 ends in 19) | `87609f3f...8a7c` |
| `phase09-sram-rw-s32k-v1` | `9616bd3b...07db` | 4,808 | 101 bytes | `52b0f1ca...a026` |
| `phase09-sram-rw-wrong-s32k-v1` | `783f4633...ce67` | 4,808 | 101 bytes (D and M end in fe) | `29174dfa...f06c` |

The transcripts are derived by `fixtures/save/save_oracle.py` outside the ROMs
and outside the core (an EEPROM model with an explicit modulo mirror and a
deadline model, and an SRAM model), and each digest equals the SHA-256 of the
hand-written literal. `--check-margins` reruns every EEPROM scenario with extra
gaps of 0 and 50,000 ticks and requires identical transcripts. The 16 Kbit ROM
differs from the 4 Kbit ROM only in its header text; the wrong-byte ROMs differ
only in one data byte and carry their baseline's pin, so a wrong text reaches the
same comparator and fails it (`guest-fail`, `wrong-transcript`). One ROM binds
one profile id: the real runner refuses a ROM presented under another id before
the guest starts. The measured completion ticks of the real supervised runs are
1,567,331 (4 Kbit), 1,567,338 (16 Kbit) and 539,986 (SRAM), all below the
3,000,000-tick bound; the per-row cap is the long cap.

## Determinism checks

`unit.core.save.device` runs twelve cases: blank storage and section 11 markers,
the EEPROM tracer at literal ticks, the SRAM window and its DMA, the command,
block, busy and timing matrices, selection, refusals, the section reference
encoder and reset. `runner.save-fixtures` and `controls.real.save-wrong-byte`
run the five ROMs through the real supervised runner. Six out-of-tree mutants
(a deadline from the start tick, a deadline from the completion tick, an
inclusive compare, a window extension, a mirror mask of 0x7F and an SRAM DMA
duration without the base tick) each compile under the gate warning flags and
fail a named comparator. No new scheduler event pair exists, so the closed event
ledger and the membership list are unchanged except for the two save predicates
and the rename of the one PI reservation site.

## Sources

Facts below are restated in authored words; no table or vector is pasted.

- n64brew, Joybus Protocol (https://n64brew.dev/wiki/Joybus_Protocol); fetched
  2026-11-19. Community documentation, single source for the EEPROM commands,
  the id bytes, the status byte, the 4 Kbit mirror and the "up to 6
  milliseconds" write ceiling.
- n64brew, PIF-NUS (https://n64brew.dev/wiki/PIF-NUS); fetched 2026-11-19.
  Community documentation for the cartridge channel and the receive flags.
- n64brew, Peripheral Interface (https://n64brew.dev/wiki/Peripheral_Interface);
  fetched 2026-11-19. Community documentation for the domain 2 address range and
  the 32-bit direct access rule; it does not give the SRAM size, mirroring or
  timing, which are therefore policy here.
- n64brew, ROM Header (https://n64brew.dev/wiki/ROM_Header); fetched
  2026-11-19. The official header has no save field, which is why the header is
  never an input.
- Nintendo SDK programming manual, EEPROM section, mirrored at
  https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/pro-man/pro26/26-06.htm ;
  vendor document for the block and byte counts; it also gives a longer write
  figure that is not used. Carried from the earlier research, not re-fetched for
  this slice.
- libdragon `src/joybus/eeprom.c` (public domain), read as reference only for
  the 6 ms busy ceiling; no code or text was copied.

The retrieval dates are those of the Phase 9 discussion verification log and its
adversarial re-fetch; the pages can change.
