# Phase 09 fan-out A: SI, PIF state machine, joybus and the standard controller

Cluster A of the Phase 9 decision fan-out. Requirement SAVE-01; roadmap
success criterion 1 (and the SI share of criterion 4 / CORE-02). This is a
recommendation document. No source was written or committed. Other clusters:
B save media (EEPROM/SRAM), C state export, D replay, E scheduler ordering.

## Evidence key and sources

Labels used below: **[DOC-P]** primary-ish vendor document (archived Nintendo
SDK pages); **[DOC-C]** community hardware documentation (n64brew wiki;
reverse-engineered, not vendor); **[CODE]** observation of this repository;
**[REF]** behavior learned from another emulator or library, reference only,
nothing copied; **[INF]** inference; **[POL]** explicit software-profile
policy (deterministic choice, not a hardware claim); **[UNK]** unknown.

| Source | Used for | Note |
| --- | --- | --- |
| Nintendo SDK `rcp.h` (https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm) [DOC-P] | SI base `0x04800000`, `SI_DRAM_ADDR` +0x00, `SI_PIF_ADDR_RD64B` +0x04, `SI_PIF_ADDR_WR64B` +0x10, `SI_STATUS` +0x18; status bits DMA_BUSY `0x1`, RD_BUSY (commented "IO busy") `0x2`, DMA_ERROR `0x8`, INTERRUPT `0x1000`; "any write clears interrupt"; `PIF_RAM_START 0x1FC007C0`..`0x1FC007FF`; +0x08/+0x0c/+0x14 reserved | The register comments are written from the DRAM side ("RD64B: any write causes a 64B DMA write" means PIF to DRAM). The register name RD64B means PIF to RDRAM; this naming trap needs a literal test |
| Nintendo `os_cont.h` (https://ultra64.ca/files/documentation/online-manuals/man/header/os_cont.htm) and Programming Manual 26.2 (https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro26/26-02.htm) [DOC-P] | Button names, stick range -128..127, a controller read takes about 2 ms at API level, NO_RESPONSE/OVERRUN are library-level error values | The manual page does NOT give the wire framing, the 4-byte response layout, the info response, or raw RX error bits. SAVE-01 research claim "[PRIMARY] standard controller query" (FEATURES.md) is true only at the `osCont*` API level |
| n64brew PIF-NUS (https://n64brew.dev/wiki/PIF-NUS) [DOC-C] | PIF RAM 64 bytes, command byte at offset 0x3F, command bits, parse/execute split, handshake `TX RX tt.. rr..`, escape codes `0x00` skip, `0xFD` reset, `0xFE` end, `0xFF` nop, TX flags `0x80` skip / `0x40` reset, RX flags `0x80` no device / `0x40` timeout, channels 0-3 ports and 4 cartridge, execution order 4 down to 0, handshakes run only on SI DMA reads, one parse can be executed by many reads | Single community source for frame semantics; PIF RAM location and command-byte role corroborated by the libdragon joybus reference and web search results only at the coarse level. Treat frame-level rules as medium confidence |
| n64brew Serial Interface (https://n64brew.dev/wiki/Serial_Interface) [DOC-C] | SI_DRAM_ADDR 24 bits, PIF address bits 10:2, SI_STATUS INTERRUPT bit 12, DMA_STATE 11:8, PCH_STATE 7:4, DMA_ERROR bit 3 (overlapping DMA, only power reset clears), IO_BUSY bit 1, DMA_BUSY bit 0, interrupt at DMA or direct-write completion, WR4B/RD4B exist, PIF mirrors across `0x1FC00000..0x1FCFFFFF`, offsets `0x7C0..0x7FF` are RAM | WR64B layout listed "TODO" there |
| n64brew Joybus Protocol (https://n64brew.dev/wiki/Joybus_Protocol) [DOC-C] | 0x00 info (2-byte identifier + 1 status byte), 0xFF reset+info, 0x01 state (4 bytes: 14 buttons, signed X, signed Y, RST), 0x02/0x03 pak read/write; identifier `0x0500`; third info byte bits 0-1 pak status (1 inserted, 2 absent, 3 newly inserted), bit 2 previous-command checksum error; bit time 4 us, console stop 3 us, controller stop 4 us, accessory must answer within about 62.5 us; unsupported commands time out; pak-absent reads return zeros with CRC `0xFF` | Bit-by-bit button layout is only an SVG there |
| libdragon `controller.h` field order (https://raw.githubusercontent.com/DragonMinded/libdragon/trunk/include/controller.h) [REF], `joybus.h` identifiers (https://raw.githubusercontent.com/DragonMinded/libdragon/trunk/include/joybus.h) [REF] | Struct order A, B, Z, Start, Up, Down, Left, Right, 2 padding bits, L, R, C-up, C-down, C-left, C-right, then signed X, Y; identifier `0x0500` N64 controller. No code or text copied | The header carries no license text in the fetched header; libdragon is a public project (verify license before ever quoting). Only facts used |
| ares `si/io.cpp` (https://raw.githubusercontent.com/ares-emulator/ares/master/ares/n64/si/io.cpp) [REF] | Shape only: write DMA uses a fixed latency, read DMA asks the PIF for an estimate, interrupt on completion, status write clears it. No constants adopted; the fetched excerpt did not show the repository license, and the unit of its constants was not established | Reference only |
| SDK `os_cont.h` button masks (names only on the fetched page) | Mask names; hex values below follow the long-standing `OSContPad.button` 16-bit layout (A `0x8000`, B `0x4000`, Z `0x2000`, Start `0x1000`, D-up `0x0800`, D-down `0x0400`, D-left `0x0200`, D-right `0x0100`, L `0x0020`, R `0x0010`, C-up `0x0008`, C-down `0x0004`, C-left `0x0002`, C-right `0x0001`) | The page fetched in this session printed only macro names (CONT_A etc.), so the hex values are [INF] from the libdragon bitfield order cross-checked against the wire (MSB-first): this is consistent but must be pinned by a literal test and re-verified against a second primary-ish document before release |

Repository observations [CODE]:

- `core/t64_bus.c:21,34-35,140-141,162`: SI window `0x04800000`..+window is
  routed to `T64_BUS_REGION_WARM`; PIF `0x1FC00000..0x1FC007FF` is
  `T64_BUS_REGION_UNSUPPORTED`.
- `core/t64_warm_devices.c:98,167-175`: SI_STATUS reads return 0; a write of
  exactly 0 clears MI SI pending; nonzero is `BUS_DEVICE_REFUSED`; every other
  SI offset is `BUS_UNSUPPORTED`; all warm access is 32-bit only.
- `core/t64_observe.c:86-120` and `docs/spec/machine-contract.md` "Output
  observation and timed input": `t64_machine_queue_input` accepts port 0 only,
  `[-128,127]` axes, value `buttons | x<<16 | y<<24`, trace device 6 kind 1,
  handler is a no-op. The existing tests queue arbitrary bit patterns
  (for example `0x1234` in `core/tests/test_isviewer.c:662`), so any
  mapping into the wire format must not retroactively refuse button bits.
- `core/t64_scheduler.c:10-13`: event order is `(tick, sequence)` only.
  Sequence is assigned at reservation time (`:88-128`), i.e. in the order
  host and guest cause reservations. There is no device-class tie-break.
- `docs/spec/machine-contract.md:141-195` (PI model) is the template for this
  cluster: reserve-then-commit, descriptor snapshot at start, atomic effect at
  the completion event, MI pending set regardless of mask and cleared only by
  the status write, busy second request refused with sticky ERROR, tokens.
- `fixtures/audio/pcm.S` is the authored-guest template: fetched CPU code
  masks CP0 interrupts, uses uncached RDRAM, writes device registers and
  reports through the ISViewer window at `0xB3FF0000`.
- `runner/` has no timed-input script option today ([CODE] grep of
  `runner/*.c`; `runner/t64_input.c` is content admission, not controller
  input). Cluster D owns adding it.

## Decision points

Numbering: D-A1.. . "Rec" = RECOMMENDED default. All recommendations are
software-profile choices where marked [POL]; none claims hardware accuracy.

### D-A1. Layering: an HLE joybus engine behind SI, not a PIF ROM

Options:
1. Execute the PIF boot/run ROM. Refused: rights-bearing data (AGENTS.md hard
   rule; `EMULATOR-DNA.md:30`), no independent oracle.
2. Model only SI registers and return canned controller bytes at DMA time.
   Too thin: skips the 64-byte frame, so SAVE-02 EEPROM (same channel) and
   multi-command frames cannot share it.
3. **Rec.** Authored HLE PIF: a model-owned 64-byte PIF RAM, the command byte,
   a joybus frame parser and per-channel device handlers (channels 0-3 ports,
   channel 4 cartridge for cluster B). No PIF ROM bytes exist anywhere.

Scope of what is modeled in v1 (all [POL] unless noted):
- PIF RAM is reachable only through SI DMA. CPU loads/stores to
  `0x1FC00000..0x1FC007FF` stay `BUS_UNSUPPORTED` (unchanged, preserving
  prior behavior), and SI_PIF_AD_WR4B/RD4B (+0x08, +0x14) stay refused.
  The SDK and libdragon use 64-byte DMA for joybus; direct PIF access has
  hardware sub-behaviors (IO_BUSY, blocking reads, parse on direct write
  [DOC-C]) with no authored evidence.
- Initial PIF RAM is all zero. Boot-info bytes (offset 0x24..0x27) and the
  CIC challenge area (0x30..) are not modeled; the open-IPL3 route does not
  touch the PIF in the current profiles ([CODE] PIF window is unsupported and
  the actual IPL3 guest boots), so they are not needed. [UNK] whether any
  admitted OS-library ROM reads boot info from PIF RAM; refuse by absence.

Evidence: [DOC-C] PIF-NUS; [CODE] boot route. Risks: a guest that expects CIC
challenge/response or boot-info through PIF RAM stops with a refusal rather
than running wrongly (intended). Authored plan: see D-A4 (unsupported command
bits refuse) and D-A13. Would change if: a rights-cleared guest in Phase 11
needs direct PIF access or the challenge. Reversibility: high; each is an
additive profile policy version.

### D-A2. SI register set, validation and DMA semantics

Registers (all 32-bit; other widths `BUS_UNSUPPORTED` as today):

| Offset | Register | Rec behavior |
| --- | --- | --- |
| +0x00 | SI_DRAM_ADDR | Write stores `value & 0x00ffffff` ([DOC-C] bits 23:0). Readable (last stored value); unchanged by DMA completion in v1 ([UNK] [DOC-C] says the register ends at the last RDRAM word written by a read DMA; unverified, no software depends on it; leave unmodeled and record as unknown). A write while a DMA is active updates the latch only; the active descriptor is immutable (PI precedent). |
| +0x04 | SI_PIF_ADDR_RD64B | Write starts PIF-RAM to RDRAM 64-byte DMA. Write-only; read `BUS_UNSUPPORTED` (matches the AI precedent of refusing unsupported reads). |
| +0x10 | SI_PIF_ADDR_WR64B | Write starts RDRAM to PIF-RAM 64-byte DMA. Write-only. |
| +0x18 | SI_STATUS | See D-A3. |
| +0x08, +0x14 | WR4B / RD4B | Refused (D-A1). |
| +0x0c | reserved | `BUS_UNSUPPORTED`. |

Start validation (all before any mutation; failure = `BUS_DEVICE_REFUSED`, no
event, no state change except where D-A3 names the sticky error):
- PIF address written must equal `0x1FC007C0` exactly (canonical value the SDK
  passes; hardware mirrors across `0x1FC00000..0x1FCFFFFF` per [DOC-C], but a
  64-byte transfer from other offsets spans ROM bytes and is out of scope).
  Alternative considered: accept `value & 0x7ff == 0x7c0`; rejected for v1
  because a refusal is cheaper to reverse than a wrongly accepted mirror
  (compatibility policy: never guess).
- RDRAM span `[dram_addr, dram_addr+64)` must lie inside installed RDRAM (4
  MiB profile) with checked 64-bit arithmetic; `dram_addr` must be 8-byte
  aligned ([POL] mirroring the PI 8-byte RDRAM alignment rule,
  `machine-contract.md:161`; [UNK] hardware alignment mask not established by
  any fetched source).
- Queue slot, token and sequence capacity reserved before busy becomes visible
  (reserve-then-commit; `core/t64_scheduler.c:88-190`).

Transfer visibility [POL]:
- **Write (RDRAM to PIF):** RDRAM is read and PIF RAM replaced at the start
  tick; the frame is parsed at the start tick. Rationale: PIF RAM is observable
  only through a later read DMA that cannot start until completion (busy
  refuses overlap), so the choice is unobservable except for guest RDRAM
  mutated while in flight, for which this fixes a named witness (source latched
  at start). This differs from the PI model (source read at completion) on
  purpose; document it, and give it an in-flight mutation witness test.
- **Read (PIF to RDRAM):** joybus execution at tick E and the atomic 64-byte
  copy into RDRAM at the completion tick C (D-A9, D-A10). A CPU read of the
  destination between start and C sees old bytes; the copy is all-or-nothing.
- Completion clears DMA_BUSY and sets MI SI pending (regardless of mask).

Evidence: [DOC-P] rcp.h register map; [DOC-C] masks and DMA description;
[CODE] PI precedent. Failure modes: wrong read/write naming (RD64B is PIF to
RDRAM); alignment refusal too strict (symptom: a guest stops at the first SI
DMA, visible immediately in the authored diagnostic). Authored plan: literal
test vectors for both directions with distinct source/destination bytes; a
swapped-direction negative control; refusal vectors (bad PIF address,
unaligned, out-of-range, overlap) each assert bit-identical digest before and
after; in-flight RDRAM mutation witness for write source latching and for read
destination overwrite. Would change if: a second primary source states the
alignment mask or SI_DRAM_ADDR post-DMA value. Reversibility: high (profile
policy marker).

### D-A3. SI_STATUS, interrupt and sticky error

Rec bit model (reads are derived from machine state):
- bit 0 DMA_BUSY: set from accepted start until completion tick, for both
  directions.
- bit 1 IO_BUSY: always 0 (no direct PIF I/O modeled).
- bit 3 DMA_ERROR: sticky; set when a second DMA start arrives while busy
  ([DOC-C] "overlapping DMA"). The original request is retained, the second is
  refused with `BUS_DEVICE_REFUSED` (PI busy-start precedent,
  `machine-contract.md:172`). Cleared only by machine reset/replacement
  ([DOC-C] "power reset only"); not by an SI_STATUS write.
- bit 12 INTERRUPT: derived mirror of MI SI pending (not separately stored).
- bits 11:8 DMA_STATE and 7:4 PCH_STATE: read as 0 ([UNK]; hardware shows
  nonzero values during DMA; no software contract found). Documented.
- Write of any value: clears MI SI pending only (SDK "any write clears
  interrupt", [DOC-P]). This relaxes the current warm rule (only 0 accepted)
  under the new profile only; the warm profile keeps its refusal.

Risks: a guest that writes nonzero status expecting nothing else. Authored
plan: bits-over-time table pinned by a test that reads SI_STATUS at start tick,
at C-1 and at C (read/write ordering follows scheduler sequence at equal
ticks); sticky error negative control; MI mask untouched assertion.
Changes if: a primary source shows DMA_ERROR clearing or other write effects.
Reversibility: high.

### D-A4. PIF command byte (RAM offset 0x3F)

Rec: at the write-DMA start the model inspects byte 63 of the new PIF RAM image
(run mode only):
- bit `0x01` set: parse the joybus frame (D-A5), then clear the bit, as the
  PIF does ([DOC-C]).
- all other bits (`0x02` challenge/response, `0x04`/`0x08` joybus timing flags
  and terminate-boot, `0x10`, `0x20`, `0x40`): **refuse the DMA start** with
  `BUS_DEVICE_REFUSED` before copying (state unchanged). Rationale: silently
  ignoring a challenge request would let a guest proceed on fabricated
  semantics; a refusal is the project's honest "unsupported" stop. [POL]
- byte 63 with bit 0x01 clear and no other bits: accepted; PIF RAM replaced,
  no parse (the read DMA then re-executes the previously parsed table, as the
  SDK's repeat-read pattern requires; [DOC-C] "multiple reads re-execute the
  same frame"; [INF] SDK skips the write DMA when its command is unchanged).
- Parsed table is persistent machine state: up to 5 channel entries (offset,
  tx length, rx length, valid) plus a "parsed" flag. Re-parsing replaces it.
  Alternative: stateless re-parse of current PIF RAM on every read (smaller
  state, equal for SDK/libdragon usage). Rejected as default because it
  diverges from the documented parse-once behavior when a guest rewrites RAM
  without bit 0x01; kept as the fallback if state size/serialization cost in
  cluster C matters.

Evidence: [DOC-C] PIF-NUS. Confidence medium (single community source).
Authored plan: vector with bit 0x01 only; vector with 0x02 set refuses and
digest unchanged; two-read vector (write once, read twice with changed input in
between, second read reflects the new input and the first read's RX flag bits
did not break re-execution). Changes if: a rights-cleared guest needs bit
0x08/0x04 or the challenge. Reversibility: high; unrefusing a bit is additive.

### D-A5. Joybus frame parsing and malformed frames

Rec parser over PIF RAM bytes 0..62 [DOC-C], per channel index c = 0,1,2,...:
1. Read TX byte `t`. Escape codes first: `0x00` skip (consume 1 byte, c++),
   `0xFD` channel reset (consume 1 byte, c++; see timing), `0xFE` end of frame
   (stop), `0xFF` nop (consume 1 byte, c unchanged).
2. Otherwise TX count = `t & 0x3f`, flags `t & 0xc0` (`0x80` skip, `0x40`
   reset device) checked at execution. Read RX byte; RX count = `r & 0x3f`
   (top two bits ignored when parsing; set as flags at execution).
3. Handshake body = TX count command bytes followed by RX count response
   bytes. It must fit entirely within offsets 0..62 ([POL]: offset 63 is the
   command byte; [UNK] exact hardware bound). If not, parsing stops; channels
   already parsed stay valid; nothing else happens, no error status.
4. Parsing stops after channel index 4 (five channels), `0xFE`, or an
   incomplete handshake.
5. A TX count of 0 that is not the `0x00` escape (e.g. byte `0x40`/`0x80`) is
   [POL] treated like an incomplete handshake (stop parsing). [UNK] hardware.

Malformed frames are guest-visible PIF behavior, not host refusals: the DMA
completes normally. Corollary: the parser is total over all 2^512 inputs,
bounded and deterministic; add a bounded fuzz target (existing
`core/tests/fuzz_rom_admission.c` pattern) asserting no read past offset 62,
no state change outside the table, identical results across repeated runs.

Evidence: [DOC-C]. Risks: single-source framing; unknown edge cases above are
policy. Authored plan: literal frame vectors (D-A12) including one frame per
escape code, a skip-flag channel (`tx|0x80`), an overlong handshake, and five-
channel limit. Changes if: second source contradicts escape semantics.
Reversibility: high.

### D-A6. Channels and device presence

Rec: channels 0-3 = controller ports 0-3; channel 4 = cartridge bus.
Profile v1 connected mask = port 0 only (standard controller). Ports 1-3 and,
unless cluster B's profile selects EEPROM, channel 4 are "absent": executing a
non-skipped handshake sets RX flag `0x80` (no device), writes no data.
Device handlers use a small internal interface (assumed by cluster B):
`transact(channel, tx_bytes, tx_len, rx_buf, rx_len) -> {rx_written, timeout,
wire_ticks, wants_dispatch_tick}`; the engine owns flags, timing and ordering.
Alternatives: model all four ports connected (rejected: needs four independent
latch streams and tests for no extra SAVE-01 evidence); only port 0 with no
channel structure (rejected: breaks EEPROM sharing).

Evidence: [DOC-C]; [CODE] port-0-only API. Authored plan: frame polling port 1
returns first RX length byte with `0x80` set and unchanged data bytes.
Changes if: owner wants multiplayer (additive: bump the connected mask, same
schema). Reversibility: high.

### D-A7. Standard-controller commands and responses

Per executed non-skipped controller channel (flags: RX flags cleared first):

| Command (TX byte 0) | Response bytes | Notes |
| --- | --- | --- |
| `0x00` info | `05 00 02` ([DOC-C]: identifier `0x0500`; status `0x02` = pak absent; consistent with SDK `CONT_CARD_ON` clear) | |
| `0xFF` reset+info | same as `0x00` ([DOC-C]) | Stick recentering is not modeled: input is absolute logical state ([POL]) |
| `0x01` state | 4 bytes, D-A8 | TX count must be 1 |
| `0x02` pak read, `0x03` pak write | unsupported: no data; RX flag `0x40` (timeout) | Matches [DOC-C] "devices that do not support a command time out"; the controller reports pak absent in info, so a well-behaved guest never issues them. [POL] that we do not emulate absent-pak reads (zeros + CRC `0xFF`) or CRC-8 in v1 |
| any other command | no data; RX flag `0x40` | |

Length rules [POL]: if `rx_count` exceeds the response length the available
bytes are written and `0x40` is set; if smaller, only the first `rx_count`
bytes are written, no flag; TX count not equal to the command's expected length
(1) is treated as unsupported (`0x40`, no data). `rx_count == 0`: command
executes, nothing is written. TX flag `0x80` skip: no execution, no RX write.
TX flag `0x40` reset: handled as command `0xFF` semantics (info reset); [UNK]
exact hardware use, low priority.

Rationale: the SAVE-01 claim is "standard controller", so rumble/mem pak
traffic must produce a defined, honest result instead of fabricated pak data.
Authored plan: vectors for each row, with one deliberately wrong expectation
(info status `0x01`) that the comparator must reject. Changes if: owner adds
controller pak (D-A15). Reversibility: high.

### D-A8. Controller state encoding and input mapping

Wire (response to `0x01`, [DOC-C] 4 bytes, [REF] field order): byte 0 =
`A B Z Start Up Down Left Right` (bit 7 down to bit 0), byte 1 =
`RST 0 L R C-up C-down C-left C-right`, byte 2 = X (signed), byte 3 = Y
(signed). Equivalently the 16-bit big-endian word `buttons` with the SDK masks
listed above, followed by X, Y.

Rec: the existing `queue_input` `buttons` field is interpreted as exactly that
16-bit word (bit 15 = A ... bit 0 = C-right); X/Y are the same signed octets.
Bits `0x00c0` (RST and the reserved bit) supplied by the host are ignored
(masked) when forming the wire response; RST is never reported ([POL]; the
L+R+Start recentering behavior [DOC-C] is a physical-controller feature that
absolute logical input does not need; [UNK] left unmodeled). Masking, not
refusing, preserves the existing API contract and tests.

Initial latched state = buttons 0, X 0, Y 0 (neutral). No dead-zone or
octagon-gate model: the host supplies the logical value, the core does not
reshape it.

Risks: an unverified mask layout would silently swap buttons. Mitigation:
pin the 16 bit positions with 16 single-button literal vectors checked against
the SDK-order table and (before release) against a second primary-ish source.
Changes if: that cross-check disagrees. Reversibility: medium (mapping is
API-visible once released: freeze only at Phase 10).

### D-A9. Timing (guest ticks)

Guest tick = 1/62,500,000 s (`audio-interface.md` timing section). 1 us = 62.5
ticks. Evidence-backed vs policy:

Evidence-backed inputs ([DOC-C] Joybus): bit 4 us = 250 ticks; console stop 3
us = 187.5 (use 188, ceil); controller stop 4 us = 250; device reply start
bound ~62.5 us = 3,907 ticks (ceil); default TX-to-RX gap ~7 us = 438 ticks
([DOC-C] PIF-NUS command-byte timing table, default branch). API-level
plausibility check: SDK "about 2 ms" per controller read ([DOC-P]) is
consistent in order of magnitude with four polled channels plus overhead.

Rec policy `joybus-wire-time-v1` [POL derived from the above, not measured]:

- Present device, tx bytes `n`, response bytes actually transferred `m`:
  `channel_ticks = 2000*n + 188 + 438 + 2000*m + 250`
  (2000 ticks = 8 bits x 250). Example: command `0x01` (n=1, m=4) =
  10,876 ticks (about 174 us); info (n=1, m=3) = 8,876.
- Absent/timeout channel: `2000*n + 188 + 3,907` (waits out the reply bound);
  unsupported command on a present device: same timeout cost.
- Skipped/escape channels: 0 ticks; `0xFD` reset: 62,500 ticks ([DOC-C] ~1 ms).
- Read-DMA handshake total `H` = sum over parsed channels (order 4 to 0, only
  the sum matters in v1).
- Write DMA: fixed `W` = 2,500 ticks [POL, no evidence, order of tens of us;
  named constant]. Transfer-to-RDRAM tail `X` = 4,096 ticks [POL, no evidence].
  Both are placeholders the planner may retune before pinning; they are
  explicitly not hardware claims.
- Execute tick `E = start + H` (all controllers sampled at `E`, D-A10);
  completion tick `C = E + X` (so with no frame `C = start + X`).
- Events: one write-DMA completion (reserved at start) or two read-DMA events
  (E then C), all reserved before busy becomes visible, so queue pressure
  cannot half-start a DMA.

What is NOT evidence-backed: W, X, and the single-sample-point
simplification (hardware polls ports sequentially; per-channel sample ticks
are a later refinement that would add a profile version). [REF] other
emulators use fixed or PIF-estimated constants; none adopted.

Authored plan: the test computes expected E and C from literal numbers, not
from the production helper (independent-derivation rule; AUTONOMY raw-command
preflight): `C = rd_start + 10,876 + 4,096` for one-channel `0x01`, and also
for info, absent, pak-timeout and `0xFD`. Pin boundaries to the exact tick
(SI_STATUS busy at C-1, idle at C). Changes if: a measured or primary timing
appears. Reversibility: high, but pinned vectors move (that is intended and
versioned).

### D-A10. When is controller state latched; equal-tick rule

Problem found [CODE]: scheduler order is `(tick, sequence)`. If an input event
with tick T and the read-DMA execute event with tick T=E are both queued, the
dispatch order depends on reservation order, which depends on how the host
partitions advances (the input may be queued before or after the guest
reserved E). Split-vs-whole advance would then change which state the guest
sees: a determinism failure for replay (CORE-05) and for CORE-02.

Options:
1. Input handler writes the latch at dispatch; execute reads the latch.
   Partition-dependent at equal ticks. Rejected.
2. Add a device-class tie-break (host input before devices) to
   `event_precedes`. Correct, but changes scheduler identity (cluster E);
   keep as the alternative if E adopts a global class order anyway.
3. **Rec.** Strictly-after visibility, implemented without scheduler change:
   per port keep `current{buttons,x,y}` and `next{buttons,x,y,visible_from}`.
   The input handler (device 6 kind 1, now stateful) first promotes `next` if
   `visible_from <= event.tick`, then stores `next` with `visible_from =
   event.tick + 1`. The execute event at tick `E` first promotes `next` if
   `visible_from <= E`, then samples `current`. Result: an input with tick
   `T` is seen by samples at `E > T` and never by `E <= T`, regardless of
   dispatch order at `E == T`. Two inputs at the same tick follow host call
   order (monotone sequence), the later wins; partition-independent because
   sequence order among host calls is call order.
   [INF] Check: same-tick `E == T`: input first, handler stores next with
   visible_from T+1, execute promotes only if visible_from <= T, no, old
   current; execute first, same. Samples at `E == T+1` see the input.

Frame vs tick: the latch is level state (each event carries the full state).

Evidence: [CODE] scheduler; the argument above is the evidence. Failure modes:
`next`/`visible_from` are new machine-digest state; forgetting them in
serialization breaks CORE-04. Authored plan: test queues the same input at
`E-1`, `E`, `E+1` (E from the independent formula) in two orders and in
split/whole partitions; the sample at `E` must see the `E-1` input and must
not see the `E` or `E+1` inputs (visible only when `T < E`). Wrong-expectation
control: flip one boundary and the comparator must fail. Reversibility: medium (state fields; versioned).

### D-A11. Input script schema and ports

Options: ticks vs frames/VI fields. VI frame boundaries depend on guest-
programmed VI registers (`vi.md`), so frame-relative scripts need a profile
and a running guest to resolve.
Rec: canonical schema is **absolute guest ticks** (u64), matching
`t64_machine_queue_input`. Script row = `{tick, port, buttons u16, x i8,
y i8}`, rows in nondecreasing tick order, level semantics, later row wins at
the same `(tick, port)`. Frame- or field-based authoring is a host-side
convenience that resolves to ticks before queueing; never in core (consistent
with AGENTS.md: host input mapping stays outside the machine; the machine sees
logical guest-timed input). Port field: event value currently packs bits 0-31
with port implied 0; add port in bits 32-33 for ports 0-3 under the new
profile only; connected-mask check refuses absent ports as today
(`INVALID_ARGUMENT`, no mutation). The replay schema (cluster D) must carry
`port` from day one even though v1 only accepts port 0, so that enabling
ports later is a profile change, not a schema break. Previous profiles keep
the observer-only no-op handler and the old value packing.

Risks: schema drift between D and the runner flag. Authored plan: round-trip
of the same script through the runner flag and the C API produces the same
digest; reordered rows or past ticks refuse with unchanged digest.
Reversibility: medium for the schema (needs Phase 10 freeze care).

### D-A12. Authored diagnostic: how a ROM reports button states at expected times

Design `fixtures/si/controller-poll.S` (fetched MIPS code, cleared authored,
same build/manifest/rights gate as `fixtures/audio/pcm.S`; no SDK code):

1. Mask CP0 interrupt enable (as in `pcm.S`) so MI SI pending is polled.
2. Build a 64-byte joybus block in uncached RDRAM (`0xA0003800`, 8-aligned)
   with literal bytes:
   ch0 `01 04 01 FF FF FF FF` (state), ch1 `01 04 01 FF FF FF FF` (absent
   port), `FE`, zero fill, byte 63 `01`. A second block variant: ch0 info
   `01 03 00 FF FF FF`; third: pak read `03 21 02 00 00` plus 33 `FF` (expect
   RX byte `0x61`); a skip (`00`) variant; a `0x02`-bit variant (refusal).
3. Poll loop, N times (N and spacing literal): clear SI status, set
   SI_DRAM_ADDR, write the PIF address to WR64B (first iteration only, since
   re-execution is the pattern under test), spin on SI_STATUS bit 0, set
   SI_DRAM_ADDR to a receive buffer, write RD64B, spin on bit 0, acknowledge
   with a status write, then load the RX words.
4. Report each poll through ISViewer as one ASCII line
   `P<n> <8 hex digits for the 4 data bytes> <rx-flag byte hex>\n`
   (hello.S text-emission pattern). Spacing between polls is an authored
   busy-wait of literal loop count.

Time evidence: the guest cannot know guest ticks, so the expected state per
poll comes from the host harness, not the guest. Core test: queue scripted
inputs at literal ticks `T_i`; run; for poll `n` the harness reads the tick of
the guest's RD64B register-write trace record (`T64_TRACE_DEVICE_BUS`,
existing observer) and computes `E_n = write_tick + 10,876` from literals;
expected line n = state with the greatest `T_i < E_n`. Place inputs at
`E-1`, `E`, `E+1` deliberately. Runner test: the actual runner (needs the
input-script flag from cluster D) passes the same script and compares the
ISViewer transcript to a literal expected transcript produced from the same
literals; the transcript hash feeds the existing result protocol.

Negative controls (same accepted setup, must reach the comparator and fail):
(a) script with one input tick shifted across `E`; (b) one expected byte
changed (A `0x80` vs B `0x40`); (c) status of port 1 expected `0x04` not
`0x84`. Also a no-controller-input baseline (all-neutral) so non-silence alone
cannot pass. Hashes alone, "state changed" or fixed observation records do not
count (Phase 8 D-01 rule reused).

Reversibility: high (fixture + literals).

### D-A13. Refusal taxonomy and postconditions

Two classes, never mixed:

A. **Host-visible refusals** (a guest store or the host API is rejected;
bit-identical machine digest before/after, no trace record change except the
refusal record if enabled, no sequence consumed): bad register offset/width;
read of write-only register; non-canonical PIF address; unaligned or
out-of-range RDRAM span; command byte with unsupported bits (D-A4); second DMA
while busy (exception: sets sticky DMA_ERROR and retains the request, like PI);
queue/token/sequence exhaustion (checked before busy becomes visible);
`queue_input` with absent port, out-of-range axis, past tick, not-ready,
halted machine, full queue.

B. **Guest-visible PIF outcomes** (the DMA completes, MI SI pending is set,
RX flags describe the result): malformed frame stops parsing; absent port `0x80`;
unsupported command `0x40`. These never refuse.

How a refusal surfaces to the guest CPU follows the existing bus-refusal
handling ([CODE] class A statuses are the same ones PI/AI/RDP use); this
cluster does not alter it.

Authored plan: table-driven refusal test asserting digest equality and
sequence counter unchanged for every A row, with one row per branch;
property: any 64-byte PIF block either executes or follows class B, never
mutating outside PIF RAM/RX bytes/MI SI/SI regs. Reversibility: high.

### D-A14. Profile identity, versioning, state and compatibility

Policy source: `docs/spec/cpu-compatibility.md` (preserve existing identities
where semantics are unchanged; version concrete changes) and the Phase 8
precedent (`audio-interface.md`: a new composite profile with descriptor,
machine model and state serialization bumps; older profiles retain prior
refusals).

Rec:
- New composite profile after the current
  `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1` (descriptor 11,
  model 19, state 24): next descriptor/model/state numbers, policy marker such
  as `si-pif-joybus-wire-time-v1`; exact name belongs to the synthesizer
  because cluster B (EEPROM/SRAM selection) and C (state) bump the same
  composite. **Recommend one Phase 9 composite bump, not one per cluster**,
  with explicit save-device variants selected as profile parameters
  (SAVE-02 "never guessed from the title").
- Previous profiles: SI window stays the warm stub (status 0, zero write
  clears), PIF unsupported, input events observer-only, byte-for-byte digests
  unchanged (regression pins).
- New serialized fields in the new state version: SI DRAM latch; DMA
  active/direction/PIF address/RDRAM address/token/E/C ticks and the
  read-phase flag (executed/not); DMA_ERROR sticky; next token and exhaustion;
  PIF RAM (64 bytes); parsed table (<=5 entries plus flag); per-port
  `current`/`next`/`visible_from`; connected mask is profile-derived, not
  state. Excluded: trace, drain cursors, host script cursor.
- Reset/replacement: PIF RAM zero, table cleared, SI idle, controller latches
  neutral, DMA events removed (audited in the replacement-reset table in
  `machine-contract.md`).
- Evidence boundary: every policy constant is named in the spec with its label;
  claims name the exact profile only.

Reversibility: policy constants are profile-scoped; changing one is a new
policy marker and re-pins vectors.

### D-A15. Out of scope for this phase

Rumble pak, controller pak (memory pak) emulation including CRC-8 and absent-pak
read/write semantics, transfer pak, mouse, keyboard/randnet, VRU, GBA/64GB
link, Real-Time Clock, 64DD, direct CPU PIF-RAM access, SI_PIF_AD_WR4B/RD4B,
CIC challenge/response and PIF boot ROM execution, per-port sequential
sampling, controller hot-plug, stick octagon/dead-zone shaping, physical
controller mapping (Phase 11), and FlashRAM (deferred by roadmap). Cartridge
channel semantics beyond the dispatch interface belong to cluster B.

## Interfaces assumed from other clusters

- **B (save media):** EEPROM shares the joybus: channel 4 handshake dispatched
  to a device handler via the interface in D-A6; B owns commands
  `0x00`/`0x04`/`0x05`/`0xFF`-family responses (EEPROM identifiers `0x0080`,
  `0x00c0`, [REF] libdragon), the busy flag and the ~6 ms write window
  ([DOC-C]) as scheduler events/time-tagged state; the engine provides tx/rx
  buffers, flags and wire ticks, and B supplies the extra "write busy"
  timing so channel_ticks stays consistent. A profile without EEPROM selected
  must yield `0x80` on channel 4 (never guessed from the title).
- **C (state export):** must serialize the D-A14 field list; the digest domain
  and state version are shared with B's fields. SI/PIF state contains no
  pointers or host data.
- **D (replay):** consumes the D-A11 schema (absolute ticks, `port`, level
  semantics, later-row-wins) and must queue through `t64_machine_queue_input`
  semantics so the strictly-after rule (D-A10) holds on replay; replay binds
  profile, build and content hash as already required; the runner flag for
  input scripts is shared with this cluster's diagnostic.
- **E (scheduler):** SI/PIF adds events of one new device id (execute and
  complete kinds) and the stateful input handler; no device-class tie-break is
  required (D-A10), but if E adopts a global class order, align it (input
  before devices at equal tick) and keep both rules compatible. Needs from E:
  coincident-event tests for `(SI complete, MI mask write)`, `(SI complete, VI/AI
  event)`, split 1/7/31-tick partitions, trace on/off and two interleaved
  machines with SI traffic.

## Proposed requirement-to-slice mapping (this cluster)

| Slice | Content | Requirements |
| --- | --- | --- |
| A1 | New composite profile; SI registers, PIF RAM, WR/RD64B DMA with events, SI_STATUS, MI SI interrupt, refusals, digest/state fields, regression pins for old profiles (no joybus semantics yet) | SAVE-01 (foundation); input to CORE-02 and CORE-04 |
| A2 | Joybus parser, channel engine, info/reset/state responses, absent/timeout flags, timing policy, input latch and strictly-after rule, literal vectors and parser fuzz | SAVE-01 |
| A3 | Authored `controller-poll` guest through the actual runner with scripted timed input, boundary witnesses and negative controls; manifest/rights entry | SAVE-01 (criterion 1) |
| A4 | With E: SI/MI events in the shared ordering and partition/interleave suite | CORE-02 (SI share, criterion 4) |
| A5 | With C/D: SI/PIF/controller fields in export/import round trip and replay | CORE-04, CORE-05 (contribution) |

Cheap accepted-setup check first (STACK.md rule): one scripted poll through the
runner returns the expected line before the full suites.

## Unknowns needing the owner (or a second source before locking)

1. Software-policy constants W (write DMA ticks), X (read transfer tail) and
   the 7 us/62.5 us reply policy: owner may accept as named policy or defer
   until a rights-cleared hardware measurement exists. No hardware numbers
   exist in fetched primary sources.
2. Hex button mask values and info status byte `0x02`: [INF]/[DOC-C]; need a
   second primary-ish source before any support statement (Phase 10 freeze).
3. SI_DRAM_ADDR alignment mask and post-DMA value; DMA_STATE/PCH_STATE bits.
4. Whether refusing command-byte bits other than `0x01` is acceptable for the
   admitted OS-library authored guests expected in Phases 10-11 (libdragon
   uses the joybus bit; a guest requiring the challenge would stop).
5. Whether the owner wants ports 1-3 connected in the first profile (this
   document recommends port 0 only, schema ready for 4).
6. Licensing of reference material: n64brew content is community-licensed
   documentation; no text was copied. ares and libdragon were used only for
   facts; ares repository license was not shown in the fetched excerpt, the
   libdragon header fetched had no license text. Re-check before citing
   anything verbatim.

## Ten-line summary

1. Build an authored HLE PIF/joybus engine behind SI with a 64-byte PIF RAM; no PIF ROM; direct CPU PIF access and WR4B/RD4B stay refused.
2. SI v1: WR64B/RD64B only; canonical PIF address `0x1FC007C0`, 8-byte-aligned in-range RDRAM, one DMA at a time (second sets sticky DMA_ERROR, PI precedent); completion sets MI SI pending, any SI_STATUS write clears it.
3. Joybus: parse-once/execute-many on read DMA; command byte bit `0x01` only, other bits refuse the start; escapes `00/FD/FE/FF`, TX/RX flags, `0x80` no device, `0x40` timeout; malformed frames are guest-visible, never host refusals.
4. Controller: info `05 00 02`, reset, state `0x01` (16-bit word A..C-right + signed X,Y); pak/unknown commands time out; port 0 only connected, schema carries ports 0-3.
5. Timing is software policy derived from documented wire times (4 us/bit, 3/4 us stops), e.g. `0x01` = 10,876 ticks, plus named placeholder constants W/X with no evidence.
6. Found a real determinism hazard: scheduler ties are `(tick, sequence)`, so same-tick input vs SI sampling depends on advance partitioning; fix with a strictly-after latch (`next`/`visible_from`), no scheduler change needed.
7. Input schema: absolute guest ticks, level semantics, later row wins; frame/VI-based authoring stays host-side.
8. Authored diagnostic `controller-poll`: fetched MIPS code polls repeatedly and prints hex lines via ISViewer; expected lines derived by the harness from literal tick arithmetic around `E-1/E/E+1`, with wrong-tick, wrong-byte and port-1 controls.
9. One Phase 9 composite profile bump shared by clusters A/B/C (state version +1); old profiles' digests and refusals unchanged.
10. Risks: framing facts rest on one community source (n64brew), button mask hex values are inferred from struct order, W/X constants and alignment are unverified; runner has no input-script flag yet (cluster D).
