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

Import and export of save bytes, their byte formats and the runner flags that
carry them are specified under Host import and export below. Not part of this
slice: any automatic load or flush, any file container other than the headerless
byte images named there, autodetection of a format, and persistence by the core.
Nothing here is an API or ABI freeze; the status values, the kind and format
constants, `t64_save_info`, `t64_machine_save_info`, `t64_machine_save_import`,
`t64_machine_save_export`, the flag names and the detail names are provisional
until the Phase 10 interface review, and only the kind numbering above and the
`save_policy` label stay fixed for the rest of Phase 9.

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
unchanged, and the PI interrupt is pending from it. The SRAM to RDRAM completion is
an RDRAM write like the ROM DMA: it clears the RDP memory coverage marks of the
destination span and the CPU link of a linked word that overlaps it, and nothing
outside the span. A transfer that ends exactly at the window end (cart address
`0x08007FF0`, 16 bytes) is accepted and one that ends two bytes past it is refused. The alignment rules are the
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

The save statuses are the values 20 to 25 of the public header: the first three
answer the device request above, the last three answer host import and export
(Host import and export, below).

| Status | Value |
| --- | --- |
| `T64_STATUS_SAVE_UNKNOWN_DEVICE` | 20 |
| `T64_STATUS_SAVE_UNSUPPORTED_DEVICE` | 21 |
| `T64_STATUS_SAVE_DEVICE_MISMATCH` | 22 |
| `T64_STATUS_SAVE_BAD_FORMAT` | 23 |
| `T64_STATUS_SAVE_SIZE_MISMATCH` | 24 |
| `T64_STATUS_SAVE_BUSY` | 25 |

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
replacement rule for the import generation is stated under Host import and
export.

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

## Host import and export

Plan 09-05. Save bytes move in and out of a machine as explicit, named,
headerless byte images. The core never opens a file, derives a path,
autodetects a format, parses a container, or loads or flushes a save on its own;
persistence is a host decision made through explicit flags. Every rule below is
a named software-profile policy.

### API

```
t64_status t64_machine_save_import(t64_machine *machine, uint32_t device_kind,
                                   uint32_t format, const uint8_t *bytes,
                                   size_t length);
t64_status t64_machine_save_export(const t64_machine *machine,
                                   uint32_t device_kind, uint32_t format,
                                   uint8_t *bytes_out, size_t capacity,
                                   size_t *length_out);
```

`device_kind` is one of the requestable kinds (1 to 7). `format` is
`T64_SAVE_FORMAT_RAW` 1, `T64_SAVE_FORMAT_SWAP16` 2 or
`T64_SAVE_FORMAT_WORDREV32` 3. Both calls move exactly the storage size of the
profile kind: 512, 2,048 or 32,768 bytes, no padding, no truncation and no
container. The info function gains a trailing `write_generation` field.

### Check order

The order is exact. Row names are the ones the test table `D24_ORDER` uses.

| Row | Condition | Status |
| --- | --- | --- |
| R0 | machine is NULL | `T64_STATUS_INVALID_ARGUMENT` |
| R1 | no admitted image | `T64_STATUS_NOT_READY` |
| R2 | pointer or span: import, bytes NULL, span arithmetic overflow, span overlapping machine storage; export, length_out NULL, bytes_out NULL with a non-zero capacity, span overlapping machine storage or length_out | `T64_STATUS_INVALID_ARGUMENT` |
| R3 | the profile has no save device (`save_kind` 0) | `T64_STATUS_SAVE_DEVICE_MISMATCH` (22) |
| R4 | kind not in 1 to 7 | `T64_STATUS_SAVE_UNKNOWN_DEVICE` (20) |
| R5 | kind in 4 to 7 | `T64_STATUS_SAVE_UNSUPPORTED_DEVICE` (21) |
| R6 | kind in 1 to 3 but not the profile kind | `T64_STATUS_SAVE_DEVICE_MISMATCH` (22) |
| R7 | format not in 1 to 3 | `T64_STATUS_SAVE_BAD_FORMAT` (23) |
| R8 | import, length not equal to the kind size (a length of 0 included); export, capacity below the kind size | `T64_STATUS_SAVE_SIZE_MISMATCH` (24); `T64_STATUS_BUFFER_TOO_SMALL` (9) with `*length_out` set to the size and the buffer untouched |
| R9 | import only: an EEPROM write window is open or an SRAM DMA is active | `T64_STATUS_SAVE_BUSY` (25) |

Export has no R9. A profile with no device answers every requested kind with the
mismatch status at R3, so a no-device profile with an unknown kind reports the
mismatch, not the unknown-kind status; this keeps the order observable. A zero
length constructs no span (an empty span cannot overlap or overflow). Every
check precedes the first read of a caller byte and the first mutation, so
atomicity is structural and no staging buffer exists.

The info function checks its pointers before admission because it must write
its output; import and export check admission (R1) before the pointer and span
row (R2) because they read or write a caller span. The two orders are kept as
they are and this is the one place they differ.

### Postconditions

A refusal leaves the canonical digest, the device bytes, the busy deadline, the
scheduler queue and its next sequence, the guest tick, the trace, the allocator
counters and `write_generation` bit-identical; a refused export also leaves the
caller buffer unchanged, except that R8 writes the required length to
`*length_out`. A successful import stores the normalized bytes, sets the deadline
to 0 (an import is not a guest write, so a stale deadline is cleared), leaves the
queue and the tick unchanged, writes no trace record, makes no allocator call and
increments `write_generation`. Export is always allowed and returns the
committed bytes: during an EEPROM window the block written through the SI is
already in the storage, and during an SRAM DMA the storage still holds the bytes
from before the DMA, because that effect is applied atomically at the completion
event.

### Byte-order selections (authored policy)

The three selections are authored policy under assumption A1. They are not a
hardware fact, and no mapping to any other emulator's convention is asserted or
implied; a host that wants another container converts outside the core.

| Selection | Definition | The RAW bytes `00 01 02 03 04 05 06 07` encoded as |
| --- | --- | --- |
| RAW | identity | `00 01 02 03 04 05 06 07` |
| SWAP16 | swap the two bytes inside every 16-bit unit (offset XOR 1) | `01 00 03 02 05 04 07 06` |
| WORDREV32 | reverse the four bytes inside every 32-bit unit (offset XOR 3) | `03 02 01 00 07 06 05 04` |

Both transforms are involutions, so import and export apply the same
permutation. The three sizes are multiples of 4, so there is no padding. A
SWAP16 vector declared as WORDREV32 (and the reverse) stores `02 03 00 01 06 07
04 05`, which is neither of the two vectors.

### Import generation and replacement

`write_generation` is an observer-only counter that lives in the machine and is
outside the canonical digest. It counts successful imports since creation or the
last replacement; guest writes do not increment it (a Phase 10 follow-up). A
successful state import (`t64_machine_state_import`) into a profile with a save
device adds exactly one, saturating, as the last write of its commit, because it
replaces the save bytes as a save import does; a refused state import never
touches the counter and a profile with no save device keeps 0. The counter never
travels in the state container (decided in plan 09-09, `docs/spec/state-container.md`,
Save media counters and the deadline bound), so a peer imported from one blob holds
its own generation. Replacement (`t64_machine_admit_rom` on an admitted machine) restores blank save content (`0xFF` for EEPROM, `0x00` for SRAM), clears
the deadline and resets the generation, so the digest equals a freshly created
and admitted machine of the same id; a refused admission leaves the bytes, the
deadline and the generation unchanged. Replacement while a write window is open
clears the deadline. Because replacement restarts the counter at 0, a value seen
after an import into ROM A can be seen again after an import into ROM B with
different content: a host that tracks "the content changed" by comparing a remembered
generation must also track its own admissions (an epoch). Both writers use one
internal saturating increment, so the 64-bit maximum is reached by neither wrapping
nor a second code path.

### What counts as in flight

D-24 names two conditions: the EEPROM deadline above the machine tick, and an
active PI DMA to or from the SRAM. An SI transaction that is pending between its
start and its execute tick is not refused; it reads the imported bytes at its
execute tick.

### Runner flags

The supervisor and the real runner carry the explicit flags below (the names,
the version-suffixed format tokens and the detail names are provisional).

| Flag | Meaning |
| --- | --- |
| `--save-import PATH`, `--save-import-size N`, `--save-import-sha256 HEX` | the import file, its exact size and its SHA-256, all three together; prepared as a private verified descriptor with a 32,768-byte bound and forwarded as `--save-import-fd FD` |
| `--save-kind TOKEN` | `eeprom4k` (512 bytes), `eeprom16k` (2,048) or `sram32k` (32,768) |
| `--save-format TOKEN` | `raw-v1`, `swap16-v1` or `wordrev32-v1`; the version suffix is the per-format version string and there is no unversioned token |
| `--save-export-leaf NAME` | the published leaf; requires the artifact pair, with `--artifact-max-bytes` at least the kind size |

Validation, all in the usage class and repeated by the runner before the guest
starts: kind and format are required together with at least one of import or
export leaf; tokens are exactly the closed lists; the import size equals the
kind size and the import trio is all or none; an export leaf needs the artifact
pair and one of the allowed extensions; the save options are refused together
with `--raw-capture-dir` and in synthetic mode. One `--save-format` governs the
import and the export of a run; a conversion between two selections is a file
conversion outside the run.

Run behavior. After admission the runner checks the device kind with the info
function and imports the verified descriptor in the named format; any failure
ends the run before the guest starts, at guest tick 0, with the existing
admission-refusal classification. The publisher is opened before the guest runs
with a byte cap equal to the kind size, so a bad parent directory, a bad leaf
grammar or an occupied target ends the run at guest tick 0 with the publisher's
own error name. Export happens only after a pass outcome; every other outcome
aborts the publisher and publishes nothing.

Leaf extensions. Exported leaves carry exactly one of the lowercase extensions
`eep`, `sra` and `sav`, all of which are in the rights scanner's denied-extension
list and in the public upload member scan's private suffixes, so an exported save
is never mistaken for a publishable file. The leaf grammar (a lowercase stem of 1
to 32 characters, one dot, three lowercase letters) is the publisher's.

Publication rules are those of the 09-02 private no-clobber publisher: an
owner-only parent opened without following symlinks, a private stage, a real
no-replace rename, a hard byte cap and identity-checked cleanup. The closed
result details of a publisher error are `artifact-cap`, `artifact-dir-unsafe`,
`artifact-exists`, `artifact-leaf-name`, `artifact-no-replace-unsupported`,
`artifact-rename`, `artifact-stage`, `artifact-substitution` and
`artifact-write`, accepted only for a host-error stop.

Privacy. Saves are private by default and never uploaded. No key is added to the
closed result record and no log line is added; save bytes appear in neither, and
the only save bytes a test writes are authored index-derived patterns in
temporary directories.

Evidence limits. The tests establish authored self-consistency: the model
follows the rules above against independent literals and an independent oracle.
The transform definitions are policy. Nothing here is a statement about a
physical console, a game, or the behavior of any other emulator.

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

`unit.core.save.import-export` runs nine cases: a tracer, the transform vectors
and full-size mapping, success postconditions, the refusal cross product with
every row proven reached, forked reads against an inaccessible span, export
refusals, the real in-flight paths, replacement, and a bounded sweep with a
seeded campaign (2,000 requests, 20,000 under AddressSanitizer). Seven
out-of-tree mutants (the length check before the format check, the busy check
before the kind checks, a caller read before the last check, SWAP16 as the
WORDREV32 permutation, an import that keeps the deadline and the generation, a
generation increment on a refusal path and an inclusive busy compare) each
compile under the gate warning flags and fail a named comparator.
`runner.save-roundtrip` runs nine cases through the real supervised runner: the
roundtrip ROM, the format matrix, EEPROM passthrough, the wrong-format control,
the refused imports, the publication rules, the no-save-bytes scan, the maximum
option set and the upload allowlist.

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
