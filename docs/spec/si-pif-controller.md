# Serial interface, PIF RAM and standard controller

## Scope and evidence

The serial interface (SI), a modeled 64-byte PIF RAM, a joybus frame parser and
one standard controller on port 0 exist only on the composite family of four
profile ids that share descriptor 12, machine model 20 and state serialization
25 (see "Composite SI profile family" in
[machine-contract.md](machine-contract.md)). Every older profile keeps its
warm SI status stub and its observer-only input handler unchanged.

Every register rule, every refusal, every timing figure, the PIF command byte
rule, the flag handling, the sample point and the button bit positions below
are named software-profile policy. The profile policy label is
`si-hle-joybus-wire-v1`. Where a community or vendor document gives a fact, the
fact is restated in the spec's own words and the source is listed under
"Sources"; a single community source is recorded as single-source, and a figure
chosen here without any source is recorded as a placeholder. This document makes
no hardware-accuracy claim: it does not state that any figure matches the
behavior of a real console, and it does not state a hardware timing.

Not modeled in this slice: ports 1 to 3 (the host input schema and the
`t64.input` v1 parser keep their port-0-only rule), direct CPU access to the PIF
window `0x1fc00000` to `0x1fc007ff` (it stays an unsupported window), the
four-byte PIF transfers (WR4B and RD4B registers), the boot-time challenge and
response exchange, other joybus peripherals (controller pak, rumble), and
physical controller mapping. The cartridge channel (channel 4) answers "absent"
for every profile id in this slice; the save devices of a later slice replace
that answer behind the same internal interface.

The model has no PIF ROM. The PIF is a 64-byte RAM reachable only through the
SI DMA start registers, so the model needs no protected firmware image.

## Modeled registers

All offsets are relative to `0x04800000`; accesses are 32-bit. Narrower accesses
and every offset not listed read and write as unsupported (no mirroring). A
refused start changes nothing except the named sticky error flag.

| Offset | Register | Implemented behavior |
| --- | --- | --- |
| `0x00` | `SI_DRAM_ADDR` | Readable and writable. A write stores `value & 0x00ffffff`; a read returns the stored value. A write during a DMA changes only the latch; the active descriptor is immutable. |
| `0x04` | `SI_PIF_ADDR_RD64B` | Write-only. A write whose value equals `0x1fc007c0` starts a 64-byte transfer from the PIF RAM to RDRAM. Reads are unsupported. The name is a trap: this register moves data from the PIF to RDRAM. |
| `0x10` | `SI_PIF_ADDR_WR64B` | Write-only. A write whose value equals `0x1fc007c0` starts a 64-byte transfer from RDRAM to the PIF RAM. Reads are unsupported. |
| `0x18` | `SI_STATUS` | A read returns bit 0 while a DMA is active, bit 3 while the sticky error is set and bit 12 while MI SI pending is set; every other bit reads zero. A write of any value clears MI SI pending and nothing else (no other MI bit, no mask, not the sticky error). |
| `0x08`, `0x14` | WR4B, RD4B | Unsupported. |

A start is checked in this order, and the first failing check decides the
outcome: another DMA is active (refused, the sticky error is set); the written
PIF address is not exactly `0x1fc007c0` (refused); the latched DRAM address is
not 8-byte aligned (refused, the sticky error is set); the 64-byte span does not
lie wholly inside installed RDRAM (refused, no error); for a write start, the
PIF command byte holds an unmodeled bit (refused, no error); the SI token space
is exhausted (resource exhausted); the shared scheduler capacity rule fails
(queue full, sequence exhausted or tick overflow, no error). Every refusal
leaves the canonical state digest as it was, except that the first and third
also set the sticky error flag. The sticky error is cleared only by machine
reset or content replacement. The alignment mask and the overlap rule are
policy: the sources are silent or inconsistent about them.

A completion sets MI SI pending regardless of the MI mask.

The model reads RDRAM at the WR64B start tick and replaces the PIF RAM at that
tick; the model writes RDRAM at the RD64B completion tick. This is a named
difference from the PI model, which reads its source at completion. Two tests
witness it: RDRAM changed after a WR64B start never reaches the PIF RAM, and
the RD64B destination keeps its old bytes, including bytes written by the host,
until the completion tick overwrites all 64 of them at once.

## PIF command byte

Byte 63 of the written image is the PIF command byte. Value `0x01` asks for a
parse: the model parses the written image and then clears the byte in the PIF
RAM. Value `0x00` replaces the PIF RAM and keeps the previously parsed table, so
a later read transfer executes the old table over the new bytes (the repeated
read pattern). Any other value is an unmodeled control function (the
documented sources are inconsistent about which bits mean what) and the start is
refused with the PIF RAM, the table and the digest unchanged. This refusal is a
named policy divergence: an unmodeled function is refused, whereas malformed
frame data is a guest-visible outcome and never a host refusal.

## Joybus frame parser

The parser reads only bytes 0 to 62 of a 64-byte image, keeps at most five
entries and never fails: it is total over all 512-bit images. It walks one
channel index at a time, starting at channel 0:

- `0xfe` ends parsing; earlier entries stay valid.
- `0xff` is a no-op: it consumes one byte and no channel index.
- `0x00` is a skip: it consumes one byte and one channel index and makes no
  entry.
- `0xfd` is a channel reset: it consumes one byte and one channel index and
  makes a reset entry.
- Any other first byte is a transmit byte: its low six bits are the transmit
  count and its top two bits are flags. A count of zero ends parsing. The next
  byte is the receive byte: its low six bits are the receive count (its top two
  bits are ignored when parsing). The handshake body is the command bytes
  followed by the response bytes and must end at offset 62 or earlier; if it
  does not, parsing ends. Otherwise the handshake becomes an entry holding the
  channel index, the offset of its transmit byte, both counts and the flags,
  and parsing continues after the body.
- Parsing ends after the fifth channel index.

The parser is deterministic and independent of byte 63. The test suite checks
it against an independently written offset-driven reference parser on 20,000
vectors in the fast preset and 250,000 under the sanitizer preset (the same
test name, with the count raised through `T64_JOYBUS_FUZZ_ITERATIONS`), against
authored vectors for each rule above, and for bounds and repeatability.

## Controller and absent-device responses

Channels 0 to 3 stand for ports 0 to 3 and channel 4 for the cartridge. Only
channel 0 has a device in this slice: the standard controller, present when the
profile carries `peripheral_flags` bit 0. Every other channel, and channel 0
without the flag, reports no device.

For an executed (not skipped) handshake the model rebuilds the receive byte from
the parsed count on every execution, so a frame executed again never gathers
flag bits, and then:

- A channel with no device sets the receive flag `0x80` and writes no data.
- A transmit flag `0x80` skips the execution: nothing is written and the
  handshake costs no time.
- A transmit flag `0x40` is handled as command `0xff`.
- With exactly one command byte, commands `0x00` and `0xff` return the three
  bytes `05 00 02` (identifier and "no pak"); command `0x01` returns the four
  state bytes below.
- Every other command, and every command count other than one, sets the receive
  flag `0x40` (timeout) and writes no data. No pak read, pak write or checksum
  is modeled.
- A receive count above the response length writes the available bytes and sets
  `0x40`; a smaller receive count writes only that many bytes with no flag; a
  receive count of zero writes nothing.

The four state bytes are the buttons high byte, the buttons low byte with bits
`0x0080` and `0x0040` cleared, then the X and Y axes as the unsigned bytes of the
signed values. The button bit positions follow the field order of the cited
sources and are pinned by sixteen single-button literal vectors: bit 15 is A,
14 B, 13 Z, 12 Start, 11 Up, 10 Down, 9 Left, 8 Right, 7 reset (masked), 6
reserved (masked), 5 L, 4 R, 3 C-up, 2 C-down, 1 C-left and 0 C-right. The
mapping is [INF] from field order and no second source is cited; the project
calls it "the authored mapping". Masked bits are never refused.

## Timing and source policy

All figures are guest ticks of 1/62,500,000 second and are named policy of
`si-hle-joybus-wire-v1`. The wire figures derive from one community source
(single source) and the formula, the write completion time and the read tail are
placeholders with no source.

| Constant | Ticks | Meaning |
| --- | --- | --- |
| `T64_SI_WRITE_DMA_TICKS` | 2,500 | write start to completion (placeholder) |
| `T64_SI_READ_TAIL_TICKS` | 4,096 | read execute tick to completion (placeholder) |
| `T64_JOYBUS_BYTE_TICKS` | 2,000 | one byte on the wire |
| `T64_JOYBUS_CONSOLE_STOP_TICKS` | 188 | console stop bit |
| `T64_JOYBUS_TX_RX_GAP_TICKS` | 438 | gap between the transmit and the reply |
| `T64_JOYBUS_DEVICE_STOP_TICKS` | 250 | device stop bit |
| `T64_JOYBUS_TIMEOUT_TICKS` | 3,907 | wait for a reply that never starts |
| `T64_JOYBUS_RESET_TICKS` | 62,500 | channel reset escape |

A present device that answers with `m` response bytes after `n` command bytes
costs `2000n + 188 + 438 + 2000m + 250` ticks; a channel that times out (no
device, an unsupported command or a command count other than one) costs
`2000n + 188 + 3907`; a skipped handshake costs nothing; a reset escape costs
62,500. The handshake total `H` is the sum over the parsed entries; the read
transfer executes at `E = S + H` and completes at `C = E + 4096`, where `S` is
the start tick, so an empty frame completes at `S + 4096`; the write transfer
completes at `S + 2500`. All controllers are sampled at `E` (a single sample
point). Worked totals used by the tests: one state command 10,876; one
information command 8,876; one absent channel 6,095; one unsupported command
with three command bytes 10,095; the two-channel pad-poll frame 16,971; the
reset escape 62,500.

A read start whose frame needs two events (execute and complete) and a write
start that needs one are checked by one scheduler function before anything is
reserved, and then the events are reserved and committed in turn, so a refusal
never leaves a partial start. The CPU preflight of a 32-bit store to RD64B needs
room for two events beyond its successor and a store to WR64B for one (D-44);
every other store keeps its earlier budget.

## Input latch

The family keeps, per port, a current value and a pending value with the tick
from which it becomes visible (`next_visible_from`). Both the input event and
the SI execute event first promote a pending value whose visible-from tick is at
or before the event tick, using the event's own tick and never the machine tick
of an advance request. An input event then stores its value as pending with
visible-from equal to its tick plus one. A sample at tick `E` therefore sees the
last input stamped strictly before `E`; a later host call at an equal stamp
replaces the earlier one; and the result does not depend on how the host splits
its advances or in which order it submits inputs. Equal digests across different
submission orders are not claimed. The initial latch is neutral (no buttons,
axes zero). The event value stores buttons in bits 0 to 15, X in bits 16 to 23,
Y in bits 24 to 31 and the port in bits 32 and 33; the public input call keeps
accepting port 0 only, with its earlier packing.

## Section 10 field table

Section 10 of the v25 digest stream is emitted after section 9. Every field is
big-endian and in the new-span class, in this order:

| Field | Width | Meaning |
| --- | --- | --- |
| `dram_address` | u32 | latched DRAM address |
| `error` | u32 | sticky error flag (0 or 1) |
| `active_valid` | u32 | a DMA is active |
| `active_direction` | u32 | 0 none, 1 WR64B, 2 RD64B |
| `active_dram_address` | u32 | DRAM address of the active DMA |
| `start_tick` | u64 | start tick of the active DMA |
| `execute_tick` | u64 | execute tick of an active read, otherwise 0 |
| `complete_tick` | u64 | completion tick of the active DMA |
| `active_token` | u64 | token carried by the active events |
| `next_token` | u64 | next token (starts at 1 after reset) |
| `token_exhausted` | u32 | token space used up |
| `execute_pending` | u32 | an active read has not executed yet |
| PIF RAM | 64 bytes | raw PIF RAM |
| `table_parsed` | u32 | a frame has been parsed |
| `entry_count` | u32 | entries in the parsed table |
| five entries | 6 x u32 each | kind (0 unused, 1 transaction, 2 reset), channel, offset, command count, response count, flags; zero for unused entries |
| four ports | per port | u32 current buttons, current X, current Y, next buttons, next X, next Y; u64 `next_visible_from`; u32 `next_valid` (axes as the unsigned byte) |

The fields `next_token` and `token_exhausted` are part of the section so that no
state that decides later behavior is left out of the digest. The test suite carries an independent encoder written from this table, compares
it with the production bytes in five states, and flips each field family once to
show that every one changes the digest. The section content is provisional until
the state content freeze at the end of plan 09-06; nothing here is an API or ABI
freeze.

## Authored tracer

`fixtures/pad/poll.S` is an authored guest program (one source, two ROMs through
one assembler definition). It builds a literal 64-byte frame (channel 0: the
state command expecting four bytes; channel 1: the same; the end escape; a
parse request), writes it with one WR64B, and then polls four times with RD64B.
Before each poll it reads CP0 Count; after each poll it prints one 17-byte line
`P<n> <8 hex digits> <2 hex digits> <w>` through the ISViewer window: the four
data bytes of channel 0, the receive count byte of channel 1 (4 with the
no-device flag, so `84`), and `w`, a Count step witness (`-` for the first
poll, otherwise `+` when the unsigned Count difference from the previous poll is
at least 10,000 and `x` otherwise). Count advances once per two guest ticks in
the model, so `w` is a model-time witness at two-tick resolution.

`fixtures/pad/poll_oracle.py` derives the expected transcripts from scenario
literals and the strictly-after rule alone. With input stamped one tick below
the execute tick of poll 1, exactly at poll 2's and one tick above poll 3's, the
four lines are `P1 800001ff 84 -`, `P2 800001ff 84 +`, `P3 40217f80 84 +` and
`P4 1802ff01 84 +` (68 bytes). Controls that reach the same comparator: a script
with the middle input one tick earlier, an all-neutral script, and a ROM that
prints the count byte with its `0x80` bit cleared.

## Determinism checks

- An authored vector for every parser rule, every responder rule, every button
  bit and every refusal, each asserted after an accepted baseline in the same
  setup.
- Timing totals written out from the formulas in the tests, never read from a
  production helper, with a comparator that reports a one-tick error.
- Sampled bytes and the final digest identical across advance partitions (whole,
  steps of 1, 3, 7 and 4,096, stops at `E - 1`, `E` and `E + 1`) and sampled
  bytes identical across all six submission orders of three inputs.
- A direction-swap mutant, a missing mask, a missing sticky error, a missing
  command byte check, a missing token check, a read destination written at the
  start, a latch promoted after storing, a latch visible from the stamp tick and
  a parser that reads the flag bits as the count each fail their named case in
  an out-of-tree build.
- Evidence limits: the figures above are policy self-consistency, not hardware
  evidence; the Count witness has two-tick resolution; and the timing literals
  in the tests are independent of the production helpers by construction, so a
  consistently wrong policy would pass them.

## Sources

Facts below are restated in authored words; no table or vector is pasted.

- n64brew, Joybus Protocol (https://n64brew.dev/wiki/Joybus_Protocol); fetched
  2026-11-19. Community documentation, single source for the frame framing, the
  escape bytes, the wire timing and the standard controller commands.
- n64brew, PIF-NUS (https://n64brew.dev/wiki/PIF-NUS); fetched 2026-11-19.
  Community documentation, single source for the command byte, the parse then
  execute behavior of the PIF and the repeated read pattern. The page is
  inconsistent about which command bytes mean what, which is why they are
  refused here.
- n64brew, Serial Interface (https://n64brew.dev/wiki/Serial_Interface); fetched
  2026-11-19. Community documentation for the register layout, the sticky DMA
  error and the interrupt acknowledge.
- n64brew, Peripheral Interface (https://n64brew.dev/wiki/Peripheral_Interface);
  listed for the PI comparison only; not re-fetched for this slice.
- Nintendo SDK `rcp.h` and the programming manual pages mirrored at
  https://ultra64.ca/files/documentation/online-manuals/ ; vendor documents for
  the register map and the interrupt acknowledge; carried from the earlier
  research, not re-fetched for this slice.

The retrieval dates are those of the Phase 9 discussion verification log; the
pages can change.
