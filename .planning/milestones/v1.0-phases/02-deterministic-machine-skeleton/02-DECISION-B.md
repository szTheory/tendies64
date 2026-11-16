# Phase 2 Decision B: Bus, RDRAM, MI, and DMA

**Status:** recommendation for Phase 2 planning; not a hardware-accuracy claim  
**Scope:** physical map, 4 MiB base RDRAM, MI pending/mask behavior, PI ROM DMA, and SP DMA register/queue semantics. No source code, emulator source, ROM, or boot data was used.

## Comparison

| Option | Pros | Cons | Complexity | Recommendation |
|--------|------|------|------------|----------------|
| Copy memory at DMA request time and mark the device busy until its deadline | Small implementation, simple deterministic completion flag | CPU observes destination data before completion; violates DMA's asynchronous guest-visible ordering and `BUS-02`'s non-instantaneous requirement | 3 device paths, no event payload -- Risk: guest-visible early data and false timing | Rec only for isolated internal helpers that are not exposed to guest execution |
| Stage an immutable DMA request, schedule completion, and commit the complete transfer at that event | CPU can run while the device is busy, completion/IRQ ordering is visible, deterministic event and digest behavior, no partially written destination | Does not model progressive visibility or shared-bus contention; PI cost is a named model, not measured hardware timing | RDRAM + PI/SP + scheduler, no new dependency -- Risk: timing profile can be mistaken for hardware fidelity | Rec if Phase 2 remains the deterministic 4 MiB skeleton and the profile names this timing model |
| Model each transfer beat progressively and arbitrate every RDRAM request | Can represent overlap, partial visibility, and bus contention | PI timing equation, SP-vs-other-agent arbitration, and several edge outcomes are not grounded by the primary documents reviewed | Bus arbiter, per-beat events, all RDRAM clients -- Risk: implementing guessed rules creates precise but unsupported behavior | Rec if later hardware-derived traces or a better primary specification establish arbitration and per-beat semantics |

**Rationale:** The Nintendo and SGI documents specify enough to implement the address map, MI register encodings, SP transfer shape, alignment, double buffer, and broad SP timing bounds. They do not specify a PI cycle equation or enough RDRAM arbitration detail for a cycle-accurate bus. The staged completion model satisfies the skeleton's determinism and asynchronous-observation goals, while keeping unsupported timing in an explicit profile version that can be replaced without changing the core/device boundary.

## Source record

Primary sources are vendor-authored Nintendo/SGI manuals or Nintendo SDK reference material. The register header is useful for exact software-visible encodings; it is an SDK header, not a complete electrical/timing specification. SHA-256 values are for the exact downloaded files in `/tmp/t64-phase2-evidence/`; no source document is added to the repository.

| Source | Locator and use | Retrieved SHA-256 |
|---|---|---|
| [Nintendo 64 Introductory Manual, RDRAM](https://ultra64.ca/files/documentation/online-manuals/man/kantan/step1/2-4.html) | §1.2.4, “RDRAM (N64 Main Memory)”: 4 MiB × 9 base memory, two 2 MiB chips, 1 MiB banks, shared by CPU/RCP | `53547b3266fe21c319601385874f0728d088e7ac96081ddb1a468a654a200cbb` (HTML) |
| [Nintendo Ultra 64 Programming Manual, Chapter 3](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro03/03-06.htm) | §3.6.1–3.6.3, printed pp. 46–47: shared physical memory, CPU vs RSP address use, general DMA alignment, PI main-memory 8-byte and ROM 2-byte alignment | `bca6606df417448458608e91d975cd624c5779535eb8df3b1bf3c37dc790b09c` (HTML); the downloaded manual PDF is `1cf05657ec35b807a6222e59e63913096bc21a047199e274b5667ecedd6ca89a` |
| [Nintendo SDK `rcp.h` RCP register map](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm) | RDRAM map/registers lines 14–119; SP lines 162–201; MI lines 386–467; PI lines 667–767. Exact address fields, bit masks, PI status and completion notes | `982f5ae01956783653d1085ef31c3d936f471f09bd1e65e27a578fae498f972f` (HTML) |
| [SGI/Nintendo 64 RSP Programmer’s Guide](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf) | Rev. 1.0, §§4 “RSP Coprocessor 0” and “DMA”, printed pp. 82–86, 95–96: SP registers, len/count/skip encoding, alignment, setup/throughput range, active+pending queue | `6ec646ba9bd836af9aa30ae90099af55dbaf291fbd8b39ff8be5664ada3bd668` (PDF) |
| [Nintendo Programming Manual, `osPiRawStartDma`](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/n64man/os/osPiRawStartDma.htm) | Function description: PI DMA max 16 MiB, even byte count, PI address 2-byte aligned, RDRAM address at least 8-byte aligned; PI status error on I/O request during DMA | `560ef2b35b5cb96b2c175608e137d70203ab1f1e3cb4a14572935bf266f4a83b` (HTML) |
| [Nintendo Programming Manual, `osPiStartDma`](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/n64man/os/osPiStartDma.htm) | §8.8.5: software PI manager queues requests; hardware PI has one I/O operation at a time; managed API count is bytes, up to 16 MiB | `dd61655aad910048b69bc6de24b689c6f4f303cf9549f20bcc147ae418793e8b` (HTML) |
| [Nintendo Programming Manual, EPI/PI domain parameters](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/pro-man/pro27/27-01.htm) | §27.1: latency, page size, release duration, and pulse width determine PI bus access speed; software must configure values for the device | Not fetched to `/tmp` |

The guide’s SP timing wording is a bound (“peak 8 bytes/cycle” and “6–12 clocks” setup), not a complete timing equation. Nintendo’s PI API docs are software contracts and do not establish the raw register’s `length + 1` encoding or a hardware duration formula. The latter is treated below as an explicit implementation inference/policy.

## Hardware facts, decisions, and implementation contract

### Physical map and base memory

Keep the bus API physical-address based. Virtual-to-physical translation belongs to the CPU layer; the N64 manual identifies KSEG0/KSEG1 direct mapping and RSP physical addressing, and says the application defines its memory map.

For the `ntsc-cart-4m` profile, define the following decoded ranges (half-open ranges in code):

| Physical range | Phase 2 behavior |
|---|---|
| `0x00000000..0x00400000` | Installed 4 MiB base-profile RDRAM byte plane; valid offsets `0..0x3fffff` |
| `0x03F00000..0x04000000` | RDRAM chip-register window; only explicitly implemented offsets decode |
| `0x04000000..0x04002000` | SP DMEM `0x04000000..0x04001000`, IMEM `0x04001000..0x04002000` |
| `0x04040000` base | SP DMA/status/semaphore registers at offsets `0x00..0x1c`; SP PC at `0x04080000` |
| `0x04100000` base | DP register window, reserved device stub in this phase |
| `0x04300000` base | MI register window; implement Mode, Version, Interrupt, Interrupt Mask at offsets `0,4,8,0xc` |
| `0x04400000`, `0x04500000` | VI and AI windows, reserved stubs in this phase |
| `0x04600000` base | PI registers at offsets `0x00..0x30` |
| `0x04700000`, `0x04800000` | RI and SI windows, reserved stubs in this phase |
| `0x10000000..0x1FC00000` | admitted cartridge ROM domain 1 window for the first cartridge profile; bytes outside the admitted image are unsupported |
| `0x1FC00000..0x1FC00800` | boot/PIF region reserved for the HLE-PIF profile; do not install or synthesize Nintendo boot bytes here |

The SDK header splits base RAM into `0x000000..0x1fffff` and `0x200000..0x3fffff`; its broader RDRAM aperture is `0x00000000..0x03efffff`, but the selected profile has only the documented 4 MiB installed. Implement `RDRAM_SIZE = 0x00400000` and test `address < size`; treat addresses beyond installed RAM as unsupported in this profile without claiming how physical hardware aliases or responds. Do not copy the header’s `IS_RDRAM` macro verbatim because its inclusive `RDRAM_END=0x003fffff` is compared with `<`, which would reject the final byte. The hardware manual describes 4 MiB × 9, with the ninth bit used for AA/Z work. Phase 2 should implement the 4 MiB byte plane and preserve a separate extension point for hidden bits; Phase 2 must not claim RDP coverage/hidden-bit fidelity.

Unknown/unimplemented MMIO windows must not silently alias RDRAM. Return the bus layer’s explicit unmapped/unsupported result; precise open-bus values and device side effects remain unknown and are not claimed. Expansion Pak, 64DD, domain-2 save devices, full RI/RDRAM initialization, and PIF ROM behavior are outside this profile.

### MI pending and mask state

Keep `mi_pending` and `mi_mask` as separate six-bit state. `MI_INTR_REG` (`0x04300008`) is read-only pending state: bit 0 SP, 1 SI, 2 AI, 3 VI, 4 PI, 5 DP. `MI_INTR_MASK_REG` (`0x0430000c`) reads the six enabled mask bits but accepts paired write commands: clear/set SP `0x001/0x002`, SI `0x004/0x008`, AI `0x010/0x020`, VI `0x040/0x080`, PI `0x100/0x200`, DP `0x400/0x800`. Ignore reserved write bits. A device completion changes pending state; a mask write never acknowledges or clears pending state.

Recommended line equation: `mi_irq_line = ((mi_pending & mi_mask) != 0)`. Recompute after either state changes; for this phase expose the line to the CPU seam, while CPU interrupt exception-entry details remain in the CPU phase. The register source directly supports the two fields and their command encodings; the AND equation is consistent with mask semantics and corroborated by community n64docs, but is not stated in the Nintendo register header. Mark tests for this equation `spec-derived`, not hardware-derived. The mask-pair precedence if clear and set for one source are asserted together is not specified; choose clear-then-set (set wins) as a named implementation rule and include it in the profile/model version.

PI completion sets pending PI. Writing `PI_STATUS` bit 1 clears PI pending. Do not clear PI pending when masking it. SP DMA completion alone does not assert SP pending; SP pending comes from the RSP interrupt/status behavior, whose task-completion and break semantics are a later RSP slice.

### SP DMA

SP register bases and fields follow the SGI guide and Nintendo SDK header:

- `SP_MEM_ADDR` `0x04040000`: bit 12 selects DMEM/IMEM; bits 11:0 are the internal byte address.
- `SP_DRAM_ADDR` `0x04040004`: bits 23:0 are physical RDRAM address.
- `SP_RD_LEN` `0x04040008`: RDRAM to I/DMEM. `SP_WR_LEN` `0x0404000c`: I/DMEM to RDRAM.
- DMA field `F`: length field `L=F&0xfff`, count field `C=(F>>12)&0xff`, skip field `S=F>>20`.
- `bytes_per_line = (L | 7) + 1`; `line_count = C + 1`; total payload bytes `bytes_per_line * line_count`. The register encodes length/count as value-minus-one; low length bits are ignored, so all transfers round to 8-byte units.
- Source/destination low 3 address bits are ignored as zero by the guide’s DMA addressing section. Validate/debug-record the raw value, then use the aligned address for the transfer. `skip` is a DRAM gap after each transferred line; next DRAM line begins at previous start + `bytes_per_line + S`. Internal memory stays contiguous and wraps within its selected 4 KiB bank by the 12-bit address field. The documents specify the 12-bit bank address and 4 KiB size; the wrap-at-boundary consequence is an address-field inference, not a direct tested hardware statement.
- DMA begins on a write to either length register. One transfer is active and one may be pending. `DMA_BUSY` is set while active or pending work exists; `DMA_FULL` is set exactly when both slots are occupied. A pending request starts at the preceding completion tick; busy remains set across promotion.

For the skeleton timing model `sp_dma_skeleton_v1`, use `12 + total_payload_bytes/8` RSP-cycle units per request (round the documented 6–12 setup range up to 12; one 8-byte beat per cycle is the documented peak). Convert this duration through the profile’s exact RSP-cycle-to-GuestTick ratio. This is a conservative deterministic model, not a sourced exact latency. It ignores RDRAM contention and single-port access stalls because there is no RSP execution in the bus skeleton. On a third request while active+pending are occupied, keep both accepted transactions unchanged and drop the new request with an internal trace reason; hardware documentation says no more requests can be serviced but does not specify overflow side effects. Do not set a fabricated MI interrupt or guest error bit.

At a scheduled completion event, copy the entire staged request atomically, then promote pending. Before that tick the destination remains unchanged. This meets deterministic ordering and makes DMA non-instantaneous; it deliberately does not model partially visible beats. Same-tick device events follow the scheduler’s global `(tick, insertion_sequence)` order.

### PI DMA

Register map: `PI_DRAM_ADDR=0x04600000` (24-bit RDRAM start), `PI_CART_ADDR=0x04600004` (32-bit AD16 bus address), `PI_RD_LEN=+0x08`, `PI_WR_LEN=+0x0c`, `PI_STATUS=+0x10`; domain 1 timing registers start `+0x14`, domain 2 `+0x24`. Status reads expose DMA busy bit 0, I/O busy bit 1, error bit 2. Status writes use bit 0 for controller reset/abort and bit 1 to clear PI interrupt. Domain 1 is used for ROM at `0x10000000`; don't claim domain 2 save/64DD support.

Software contract for a legal PI request: RDRAM address 8-byte aligned, cartridge address 2-byte aligned, byte count even and at most 16 MiB. The PI is a single hardware operation at a time; Nintendo’s PI manager queue is OS software and must not be represented as an unbounded hardware queue in the core. `PI_STATUS.ERROR` denotes an I/O request while DMA is busy. For a new DMA start while busy, leave the active request/addresses intact, set the sticky error bit, and do not enqueue a second PI transfer. Clear the error only on PI reset. This busy-start effect is conservative given the SDK status text; the precise raw-register behavior is a source gap.

For `pi_dma_skeleton_v1`, infer the raw register byte count as `N = length_register + 1`: the PI length register is 24 bits and Nintendo’s API allows up to 16 MiB, which exactly fits a 24-bit `N-1` field. Treat the inference as tentative and accept only even `N`; legal starts require the alignments above. Do not silently round invalid lengths/addresses. An unsupported request produces a deterministic bus-device fault before setting busy, enqueueing work, copying destination bytes, or raising completion; address/length latches still retain the guest writes that established them. This is a profile rule, not a claim about physical open-bus behavior. Bounds-check the full cartridge/RDRAM spans before staging any bytes. A refused host ROM admission remains transactional and keeps the machine digest unchanged.

PI's exact register timing equation was not located in primary docs. The four PI timing register field definitions and their speed role are documented, but not how field values combine into transfer cycles. For the named skeleton profile only, schedule a transfer after `1 + N/2` model GuestTicks (one setup tick plus one tick per 16-bit PI word). These constants are implementation policy, not N64 timing. At completion, atomically copy the staged bytes, clear DMA/IO busy, and set MI PI pending. A PI reset during a transfer marks it canceled, preserves the no-partial-copy guarantee, and transitions idle/raises PI completion at its scheduled event; the SDK header says reset of a busy controller generates an interrupt as it returns idle, but exact cancellation duration is unspecified. PI write-to-cartridge is unsupported in the cartridge-ROM profile.

## Independent expected-value tests

Tests should use synthetic in-memory byte patterns authored for the tests; expected values below are derived from the cited fields/formulas, not another emulator or protected fixture. Record each case's source/revision in the test manifest as `spec-derived` unless a hardware capture is later added.

| Test | Input | Expected result and derivation |
|---|---|---|
| RDRAM upper boundary | Write byte `0xA5` at physical `0x003fffff`, then read it; next write at `0x00400000` | First operation succeeds, second is rejected by the 4 MiB half-open profile range; value at `0x003fffff` remains `0xA5`. Source: two inclusive 2 MiB ranges in Nintendo `rcp.h`, converted to a byte count of `0x400000`; policy is bounds-checked no-wrap. |
| MI pending survives mask changes | Set PI pending (`0x10`); write mask-set PI (`0x0200`), then mask-clear PI (`0x0100`) | Reads: pending `0x10` throughout; mask `0x10`, then `0`; derived from pending vs mask fields and PI command pairs in `rcp.h`; line true then false per selected AND equation. |
| MI command isolation | Pending `0x11` (SP+PI), mask `0`; write set-SP+set-PI (`0x0202`), then clear-PI (`0x0100`) | Mask reads `0x11`, then `0x01`; pending stays `0x11`; line remains true due SP. Derived from `rcp.h` bit/command mapping and selected line equation. |
| SP single row decode | `SP_RD_LEN=0x00000000`, internal addr `0`, DRAM `0x100` containing ascending bytes | One 8-byte line, one line, skip 0; at completion DMEM `0..7` equals RDRAM `0x100..0x107`, not before. Formula from SGI Guide pp. 83–84; timing result `+13 RSP cycles` from `12 + 8/8` is the chosen skeleton model. |
| SP rectangular decode and skip | `SP_RD_LEN=0x0010200f`, DRAM `0x200`, internal `0x20` | `L=0x00f -> 16 bytes/line`, `C=2 -> 3 lines`, `S=1`; payload 48 bytes. DRAM reads begin at `0x200`, `0x211`, `0x222`; contiguous internal offsets `0x20..0x4f`; duration `+18 RSP cycles` (`12+48/8`). Each value is algebraically derived from register fields and guide formula. |
| SP two-entry queue | Start request A (8 bytes), queue B (16 bytes), inspect; tick to A completion; then tick to B completion | Initially busy=1/full=1; after A copy busy remains 1/full=0 and B becomes active; after B copy busy=0/full=0. Completion dates are start `+13` and next start `+14` RSP cycles under `sp_dma_skeleton_v1`. Queue facts from SGI Guide p.96; exact dates are policy-model expectations. |
| SP internal-bank wrap inference | DMEM address `0xff8`, transfer 16 bytes from valid RDRAM | Skeleton result writes first 8 bytes at DMEM `0xff8..0xfff` and next 8 at `0..7`. Derived from 12-bit address plus 4 KiB DMEM size, but classify as profile/spec inference; promote only after a hardware-derived vector or an explicit primary statement. |
| PI pending/completion order | Legal even 16-byte DMA from admitted ROM `0x10000000` to aligned RDRAM `0x100` | Before `start+9` model ticks: destination unchanged and PI busy. At tick: 16 exact bytes copied, busy=0, MI pending PI bit `0x10` set. Derivation: SDK limits/alignment, raw `N-1` inference, and named `1+N/2` profile formula. No hardware timing claim. |
| Refusal is atomic | Attempt host ROM admission with malformed header/size or `sha`-matched test sentinel bytes that violate the selected profile | Admission returns refused; machine state digest, bus/device registers, queues, and destination RDRAM bytes are unchanged. This checks the project refusal contract, not a console behavior. |

## Gaps and promotion evidence

| Unknown | Safe Phase 2 handling | Evidence to promote |
|---|---|---|
| PI `length + 1` raw register encoding and odd-length edge behavior | Use `N-1` only as a labeled inference; reject odd/unaligned starts in the named skeleton profile | Explicit register-level primary documentation or rights-cleared hardware capture vectors at lengths 2, 8, 16, boundary and maximum |
| PI transfer timing equation, page crossing, exact clock conversion, and timing-register effects | Versioned fixed skeleton cost; keep domain timing registers separately readable/writable with masks but do not call timings accurate | Primary PI electrical/RTL specification or instrumented hardware traces spanning a range of LAT/PWD/PGS/RLS settings and request sizes |
| SP address rollover, third-request overflow, and exact cycle cost within the 6–12 setup range | 12-bit bank wrap and drop-on-full are explicit profile policies; fixed upper setup bound plus peak throughput | SGI/Nintendo correction or a rights-cleared hardware fixture that checks bank crossings, third request, busy/full transitions, and cycle-counted completion |
| RDRAM contention/arbitration among CPU, SP, PI, DP, VI, AI, SI, plus partial DMA visibility | Atomic scheduled completion, global stable tie order, no bus-contention claim | Primary arbitration specification or hardware-derived traces with concurrent agents and timestamped reads/writes |
| Unmapped/open-bus values, hidden ninth-bit layout, and RDRAM behavior above 4 MiB without Expansion Pak | Keep expansion/hidden plane/open-bus behavior unclaimed; fault unsupported requests atomically in the named profile | Profile-specific hardware measurements or applicable primary memory-controller documentation |
| PI error-bit clear/hold semantics and MI mask pair both clear and set for one source in a single write | Choose sticky-until-reset PI error and set-wins MI mask as documented model rules | Direct hardware vectors or primary register semantics |

These gaps do not prevent planning a deterministic skeleton if the plan names the two timing-model versions, distinguishes spec-derived checks from hardware-derived evidence, and limits support claims to the named 4 MiB cartridge profile. They do block a claim that Phase 2 implements hardware-exact PI/SP timing, raw invalid-request behavior, or RDRAM bus arbitration.
