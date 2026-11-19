# Phase 9: Controller, Save Media and Replay - Context

**Gathered:** 2026-11-19
**Status:** Ready for planning. Basis: the autonomy policy for routine reversible
decisions (`.planning/AUTONOMY.md` sections 1 and 4) plus the owner's instruction
"follow ur recs" to proceed with planning Phase 9. No decision below is recorded as
individually owner-approved; none triggers an AUTONOMY section 4 stop. Decisions
were synthesized from five research clusters and revised after an independent
adversarial review (`09-DISCUSSION-PROPOSAL.md`, `09-DISCUSSION-ADVERSARIAL.md`).
Decision numbers D-01..D-46 equal the proposal's D-1..D-46.

<domain>
## Phase Boundary

Deliver SAVE-01..04, CORE-02 (completion), CORE-04 and CORE-05 for one new composite
software profile family: a standard controller on port 0 over a modeled SI/PIF
joybus driven by timed logical input; explicitly profile-selected EEPROM (4 and 16
Kbit) and SRAM (32 KiB) with defined refusal postconditions and byte import/export;
all in-scope devices ordered by the one guest-time scheduler with coincident-event
and partition/interleave evidence and the CPU-to-RSP quantum in profile identity;
a versioned machine state export/import with an exact canonical digest round trip;
and an input replay bound to profile, build and content hash that reproduces the
recorded checkpoint digests.

Evidence is authored, local and software-model only (tiers `unit-tested`,
`regression-pinned`). No ROM, save, PIF/IPL image, firmware or hardware measurement
is used or needed. Phase 7 / VID-04 / Plan 13 stay stopped. No API/ABI freeze (Phase
10), no host player, no FlashRAM, Controller Pak or Rumble, no release.

</domain>

<decisions>
## Implementation Decisions

Labels: [FACT] primary-source fact; [COMM] community documentation (single source
unless stated); [CODE] code observation; [INF] inference; [POL] named deterministic
software-profile policy (a model choice, not a hardware claim); [UNK] unknown.
"Reversal" is the evidence that would change the default. Reversibility ratings follow
the template taxonomy; every profile-scoped choice is `reversible` until the identity
freeze F2 and becomes a new-profile cost afterwards.

### Identity, ordering and state (CORE-02, CORE-04, CORE-05)
- **D-01:** Profile family. One composite family: descriptor 12, model 20, state 25;
  four registered ids sharing the triple (D-43 scheme); device selected by the existing
  `save_kind` (0 none, 1 EEPROM 4 Kbit, 2 EEPROM 16 Kbit, 3 SRAM 32 KiB) and controller
  presence by `peripheral_flags` bit 0; new text fields `si_policy` and `save_policy`
  (labels at most 63 characters); both numeric fields are now validated (descriptor 12
  range-checks; `t64_machine_create` refuses non-zero values on older versions; legacy
  hashing unchanged because `core/tests/test_profile.c:152-153` hashes `save_kind = 7`).
  [POL] over [CODE]. Reversal: owner wants runtime-selectable media (a Phase 10 facade
  matter). — **Reversibility:** costly — every version-membership site (validator,
  writer, `t64_has_*` chain, create-time storage ladders, runner checks) is touched.
- **D-02:** Scheduler order stays `tick-sequence-fifo-v1`. No class rank, no new
  policy string. SI is a new device id (10) with two kinds (execute, complete);
  EEPROM has no event. [POL] (no hardware evidence for any tie; E's rank table would
  reorder qualified Phase 7/8 interplay). Reversal: the characterization or matrix
  finds a guest-history-independent order defect, a real guest needs a specific tie,
  or the owner prefers a specified rank table. — **Reversibility:** reversible.
- **D-03:** Host input. Strictly-after per-port latch (`current`, `next` with
  `visible_from`): a sample at tick E sees the last input stamped strictly before E;
  later host call wins at equal stamp; `t64_machine_queue_input` API and refusals
  unchanged (port field in event-value bits 32-33 under the new profile only). Replay
  records `at`; runner recorder and driver submit only at completed-tick boundaries
  (an inclusive tick-bound advance first). Equal digests across different submission
  schedules are not claimed. [POL]. Reversal: a coincident pair is partition-variant
  for guest-only reasons, or a guest needs "stamped T visible at T" (then E's rank 0
  in a later profile). — **Reversibility:** reversible.
- **D-04:** Quantum. Descriptor 12 refuses `synchronization_quantum != 1` and ratio
  other than 1:1; unit "RSP issue opportunities equal to guest ticks"; the only
  documented statement is that another quantum is a new profile (the "differ only for
  CPU-RSP races" sentence is deleted, [UNK]). Hash sensitivity shown at the hash
  function (existing contrast `core/tests/test_profile.c:450-470` plus a new explicit
  control). [CODE] inert today + [POL]. Reversal: a v2 performance milestone. —
  **Reversibility:** reversible.
- **D-05:** Canonical payload is the digest preimage (new profile only). A sink seam in
  `core/t64_digest.c` (hash sink; buffer sink for export); v25 branch emits
  `domain || (section id u32 || section bytes)...` in fixed ascending id order,
  self-delimiting fixed-width big-endian fields; legacy profiles unchanged and proven
  by existing pins; `build_id` and `profile_id` excluded from hashed packet metadata
  (still stored; importer re-derives them, valid because binding is exact);
  **strip-and-compare differential**: the v25 stream with markers, new sections and
  identity-tagged spans removed equals the v24 stream for each authored machine
  (idle, AI mid-DMA, VI frame, RDP state at least); `sizeof/offsetof` tripwire kept;
  v25 not comparable to v24. [CODE]+[POL]. Reversal: the legacy pins cannot be held
  after the seam change, which reopens design in 09-01 (there is no pre-approved
  fallback; the earlier "O2 separate exporter" option is retired). — **Reversibility:**
  costly — the walker is shared by digest, export, import and every device section.
- **D-06:** Freezes. F0 end of 09-01 (membership pattern, section ids and framing, id
  scheme, field validation); FH end of 09-03 (qualifier, collector/verifier, `ci.yml`,
  scanner and inventory logic; data rows only afterwards; flag-forwarding rows and runner
  flags are ordinary plan content qualified once); F1 end of 09-06 (state content;
  later changes are recorded defect fixes with goldens regenerated); FR end of 09-10
  (all logic); F2 start of 09-11 (literal hashes, claims, evidence). Tests use a
  symbolic accessor for the unfrozen profile. [POL]. — **Reversibility:** reversible
  before F2; a change after F2 is a declared qualification reset.
- **D-07:** Closed `(device, kind)` handler table built as the 09-01 ledger (E0), used
  later by state import; unknown pairs are refused at import. [CODE]. —
  **Reversibility:** reversible.

### SAVE-01: SI, PIF and the standard controller
- **D-08:** HLE PIF, no PIF ROM. Model-owned 64-byte PIF RAM reachable only through SI
  DMA; CPU access to `0x1FC00000..0x1FC007FF` stays unsupported; WR4B/RD4B refused;
  boot info and CIC challenge not modeled. [POL] (PIF ROM is rights-bearing). Reversal:
  a later rights-cleared guest needs direct access. — **Reversibility:** reversible.
- **D-09:** SI registers and DMA. `+0x00` DRAM address (24 bits), `+0x04` RD64B (PIF to
  RDRAM, write-only; the name is a trap, test both directions literally), `+0x10` WR64B,
  `+0x18` STATUS; PIF address must equal `0x1FC007C0`; one DMA at a time; reserve-then-
  commit with all events reserved before busy is visible; completion sets MI SI
  pending regardless of mask; any STATUS write clears it; IO_BUSY, DMA_STATE, PCH_STATE
  read 0; a **misaligned DRAM address is refused and sets sticky DMA_ERROR** (documented,
  [COMM]; alignment mask itself [UNK]); out-of-range span refused without the error
  ([POL]); a read DMA while a write DMA is active is an overlap (refused, sticky error;
  the source is silent on DMA-versus-DMA, so [POL]). Write DMA reads RDRAM and replaces
  PIF RAM at the start tick (named difference from PI's completion-time read, with an
  in-flight mutation witness); read DMA copies atomically at completion. [FACT]/[COMM]
  register layout and interrupt acknowledge; rest [POL]. Reversal: a second source for
  alignment, overlap or status effects. — **Reversibility:** reversible.
- **D-10:** PIF command byte (offset 63). Only bit `0x01` (parse) is honored and cleared
  after parsing; bit 0x01 clear with no other bits replaces RAM without re-parse; any
  other bit is an unmodeled control function and refuses the write-DMA start with
  unchanged state. The source lists 0x02 challenge and 0x04/0x08 joybus flag bits
  (TX-to-RX delay about 7 us versus about 520 us) but is internally inconsistent about
  which bits, so they are [UNK] and unmodeled. [COMM] single source; refusal is [POL],
  a named divergence from the PIF. Reversal: a guest needs another bit or a second
  source resolves the layout. — **Reversibility:** reversible.
- **D-11:** Joybus parser. Total over all 512-bit inputs, bounded, deterministic; escapes
  `0x00` skip, `0xFD` reset, `0xFE` end, `0xFF` nop; TX/RX counts low six bits, flags top
  two; handshake must fit offsets 0..62; five channels; malformed or unsupported frame
  **data** is a guest-visible outcome (DMA completes), never a host refusal; the rule
  "unmodeled function = refusal (D-10), malformed data = guest-visible" is stated once
  in the spec. Bounded fuzz target. [COMM]+[POL]. Reversal: a second source contradicts
  the escapes. — **Reversibility:** reversible.
- **D-12:** Channels and presence. Channels 0-3 are ports 0-3, channel 4 the cartridge;
  only port 0 has a controller; absent ports and channel 4 without EEPROM return RX flag
  `0x80`, no data. Schema carries ports 0-3. [COMM]/[POL]. Reversal: multiplayer wanted
  (additive mask). — **Reversibility:** reversible.
- **D-13:** Controller responses. `0x00` and `0xFF`: `05 00 02`; `0x01`: four bytes
  (A B Z Start Up Down Left Right; RST 0 L R C-up C-down C-left C-right; signed X; signed
  Y); pak read/write and any other command: timeout flag `0x40`, no data, no fabricated
  pak or CRC. Host `buttons` bits `0x00C0` are masked, not refused; initial latch neutral.
  Button bit positions are [INF] from field order and are pinned by 16 single-button
  literal vectors; a second source is sought before any wording beyond "the authored
  mapping". Reversal: a cross-check disagrees. — **Reversibility:** costly — API-visible
  once released; frozen only at Phase 10.
- **D-14:** Timing. Policy label `si-hle-joybus-wire-v1`-style: channel ticks
  `2000n + 188 + 438 + 2000m + 250` (present device), timeout `2000n + 188 + 3907`,
  write DMA `W = 2500`, read tail `X = 4096`, `E = start + handshake total`, `C = E + X`,
  all controllers sampled at `E`. Wire times [COMM] single source; formula and `W`, `X`,
  single sample point [POL] placeholders, no hardware claim. Tests compute `E` and `C`
  from literals independent of production helpers. Reversal: measurement or a second
  primary source. — **Reversibility:** reversible.
- **D-15:** Input schema. Absolute guest ticks (u64), level semantics, later row wins at
  equal `(tick, port)`; row `{tick, port, buttons u16, x i8, y i8}`; frame- or VI-relative
  authoring stays host-side. The existing `t64.input` v1 path stays. [POL]. —
  **Reversibility:** costly — replay and runner carry it before the Phase 10 freeze.
- **D-16:** `pad-poll` diagnostic. Cleared authored MIPS (same build/manifest gate as
  `fixtures/audio/pcm.S`) builds literal joybus blocks, polls via SI DMA and prints one
  ASCII line per poll through the ISViewer window; the ROM also reads CP0 Count before
  each poll and prints it (the model advances Count once per two guest ticks,
  `core/t64_cp0.c:580-590`) as an in-guest model-time witness. The harness computes `E_n`
  from the RD64B write's trace tick and the policy literals (stated as policy
  self-consistency); inputs at `E-1`, `E`, `E+1`. Controls that reach their comparator:
  tick shifted across `E`, one wrong byte, wrong port-1 flag, all-neutral baseline. —
  **Reversibility:** reversible.
- **D-44:** Profile-gated CPU store preflight. First task of 09-03: for the new family
  only, the extra-event budget of a 32-bit store to the SI window is 2 for RD64B and 1
  for WR64B (legacy budgets untouched, pinned by old-profile controls); 8-byte stores to
  SI have a defined outcome and a control; atomic refusal at `QUEUE_FULL` and
  `SEQUENCE_EXHAUSTED` at small queue capacity and sequence width. [CODE]
  (`core/t64_cpu.c:1483-1550`). Reversal: a cheaper atomic design is found. —
  **Reversibility:** costly — it is a change on the shared CPU path (gated and pinned).

### SAVE-02 and SAVE-04: explicit save devices and refusal
- **D-17:** Selection is the profile. Header, title, game code, checksum and file name
  are never inputs; an authored ROM with a save-type nibble in header byte `0x3F` is a
  negative control. [CODE]+[POL]. Reversal: runtime selection through a Phase 10 facade.
  — **Reversibility:** reversible.
- **D-18:** EEPROM commands (channel 4, exact lengths). `0x00` info (tx 1, rx 3):
  `00 80` or `00 C0` plus status (`0x80` busy); `0x04` read (tx 2, rx 8) only when idle;
  `0x05` write (tx 10, rx 1): `0x00` accepted or `0x80` already busy, data committed at
  acceptance, window not extended while busy. Other commands or lengths, and a read while
  busy: timeout flag `0x40`, nothing written. `0xFF` on channel 4 yields the timeout flag.
  [COMM] ids/lengths/status (EEPROM ids and busy bit corroborated by two sources); exact
  lengths and no-reply on busy read [POL]; busy-read hardware behavior [UNK]. —
  **Reversibility:** reversible.
- **D-19:** Block rule. 4 Kbit: the documented mirror `block & 0x3F` (blocks 64..255 repeat
  0..63; single-source [COMM], not independently verified); refusal is the recorded
  alternative `eeprom-4k-block-refuse`. 16 Kbit: all 256 blocks. Block-index test patterns
  expose a wrong mask on either part. Reversal: a second source or measurement
  contradicting the mirror. — **Reversibility:** reversible.
- **D-20:** Busy window. `busy_until` compared at handshake execute ticks; no scheduler
  event; the sourced "up to 6 milliseconds" ceiling used as a fixed 375,000-tick duration
  (62.5 MHz, `core/t64_vi.c:23`); label `eeprom-busy-ceiling-6ms-v1`, never "exact". Idle
  at exactly `busy_until`. A stale deadline is kept as is. Other figures (about 15 ms,
  30 ms) are uncorroborated. [POL] over a sourced upper bound. Reversal: a primary document
  or measurement (physical measurement would be an AUTONOMY section 4 stop at that time). —
  **Reversibility:** reversible.
- **D-21:** SRAM. 32 KiB at `[0x08000000, 0x08008000)`; rest of domain 2 unsupported (no
  mirroring, no banking); PI DMA both directions with the `1 + N/2` duration equation,
  effect atomic at the completion event, direction and target in the active descriptor;
  32-bit aligned word CPU/host access only; profiles without SRAM keep refusing read-length
  starts. Initial contents EEPROM `0xFF`, SRAM `0x00`, never auto-loaded. Base address and
  32-bit direct access [COMM] (not re-verified); size, blank contents and non-mirroring
  [UNK], named [POL]. — **Reversibility:** reversible.
- **D-22:** Channel 4 without EEPROM. Every handshake returns `0x80` and writes nothing;
  RTC commands `0x06..0x08` are unsupported. [COMM]. — **Reversibility:** reversible.
- **D-23:** Unknown, unsupported, deferred requests. `device_kind` values reserved now
  (1/2/3 supported; FlashRAM and Pak kinds known-unsupported `SAVE_UNSUPPORTED_DEVICE`;
  others `SAVE_UNKNOWN_DEVICE`; kind versus profile `SAVE_DEVICE_MISMATCH`). Tests assert
  the specific status and an unchanged digest, never a silent success. [POL]. —
  **Reversibility:** reversible.

### SAVE-03: import and export
- **D-24:** API and formats. Provisional `t64_machine_save_info/export/import`; formats
  `RAW`, `SWAP16`, `WORDREV32`; caller always names kind and format; no autodetect, no
  containers; exact lengths 512 / 2048 / 32,768. Check order, all before any caller-byte
  read or mutation: admitted, pointer/length, profile has a device, kind known, kind
  supported, kind equals profile, format known, length equals size, nothing save-affecting
  in flight (import refused while EEPROM busy or an SRAM DMA active; export always
  allowed). Refusal postconditions: digest, device bytes, deadline, scheduler, tick,
  counters, trace and `write_generation` unchanged; export refusal leaves caller bytes
  unchanged; too-small export reports the required length. Success: bytes equal the
  normalized input, busy cleared, queue and time unchanged, `write_generation` (observer
  only, outside the digest) increments. Literal 8-byte vectors pin the transforms; no
  mapping to any other emulator's convention is asserted. [CODE]+[POL]. Reversal: owner
  wants interchange containers or RAW-only. — **Reversibility:** reversible.
- **D-25:** Replacement semantics. Successful ROM replacement restores blank save content
  and clears the deadline; refused admission leaves bytes unchanged; the host imports
  after admission. [CODE]+[INF]. — **Reversibility:** costly — observable in
  replacement-table semantics.
- **D-26:** Persistence is host-owned. The core never opens files; the runner imports
  through the verified-input fd path and exports via the 09-02 publisher; no automatic
  load, flush or path derivation; result JSON and logs carry only kind, length, format and
  flags, never save bytes. [CODE]. — **Reversibility:** reversible.
- **D-27:** Save in digest/state. A `save-media` section holds kind, size, `busy_until` and
  all bytes; unselected devices contribute nothing. [CODE]. — **Reversibility:** reversible
  until F1.

### CORE-02: one scheduler
- **D-36:** Deliverables. E0 pairwise FIFO characterization on existing profiles (both
  insertion orders, independent literals, a transposed-table mutation control), CPU-vs-
  device and CPU-vs-RSP probes; SI refusal atomicity; one shared matrix: whole versus
  splits at `{T-1, T, T+1}` and steps `{1, 3, 7, 4096, one-shot}`, mid-tick counter and
  resource-limit stops then resume, two and three interleaved machines (one of another
  profile), trace on/off, a span longer than one Count wrap, and later state import at a
  mid-tick stop. MI stays synchronous in its producing handler and is sampled by the CPU
  at step start (`core/t64_cpu.c:2175-2180`); the spec and claim say "ordered through its
  producers' events". **Required mutants, each failing a named test:** handler reads
  `machine->tick` instead of `event->tick`; sequence taken at commit instead of reserve;
  SI completion derived from the per-advance budget; input promotion/sample order
  reversed. Evidence limit stated: regression pins plus self-comparison across schedules,
  so a consistently wrong tie would pass. Every tie is [POL] with an owning test name. —
  **Reversibility:** reversible.
- **D-37:** Quantum evidence. Refusal of `Q != 1` with unchanged allocator counters,
  accepted `Q = 1`, independent reference hash, the D-04 hash-sensitivity control,
  quantum present in result identity records (check, add if missing). [CODE]. —
  **Reversibility:** reversible.

### CORE-04: state export and import
- **D-28:** Container (provisional). Binary, big-endian: magic `T64STATE`, container
  version, state serialization version, total length, section count (at most 32), flags
  (zero), profile digest, content SHA-256 and length, build id, guest tick (cross-checked),
  declared canonical digest, section table (id, flags, offset, length, SHA-256), canonical
  sections, an observer-continuity section outside the digest, SHA-256 trailer; no
  compression, no unknown-section skipping, 64 MiB hard ceiling and a profile-derived
  exact length. [CODE]+[POL]. — **Reversibility:** reversible until Phase 10.
- **D-29:** Exclusions and observers. Excluded: trace, drain cursors, ring head/count,
  `rsp_observed_*`, caller buffers, pointers, allocator, queue head, padding, build identity
  as behavior. Retained packet slots (about 1.1 MiB) are carried because drained slots are
  hashed. Observer state gates dispatch (`packet_count`, `core/t64_scheduler.c:310-336`,
  `core/t64_ai.c:384,416,487`), so default import restores observer continuity; "drop
  observers" is documented as changing stop points (not results) and T3 asserts both. —
  **Reversibility:** reversible.
- **D-30:** Import: validate then apply, no allocation; contract is memory safety plus
  liveness. A validated blob cannot cause memory unsafety, an unbounded loop or
  arithmetic undefined behavior; semantically unreachable states are accepted and then run
  deterministically; hashes are integrity checks, not authentication; reachability is not
  promised. One reader per section with validate and apply modes over the same code; a
  bounded invariant summary on the stack; order: arguments and disjointness, header, whole-
  blob SHA-256 trailer, identity binding (a status per mismatch), section table, per-field
  ranges, structural checks, recompute the digest from the blob's canonical bytes against
  the declared digest, one infallible commit (handlers from the closed table, queue head 0,
  trace cleared with enablement preserved). Uniform checks: tuples strictly ordered,
  sequence below `next_sequence`, tick at or above machine tick, `(device, kind)` in the
  table, derived lines recomputed. Per-device "busy implies exact events" rules are
  derived per state machine (CPU step plus exception re-reserve, PI canceled state, VI/AI
  events queued but undispatchable) and tested both ways; EEPROM has none. In tests only,
  recompute the digest after apply (a check, not a rollback; the commit performs no
  allocation or host call and core lint forbids allocation names and mutable file-scope
  statics, `tools/lint/rules.txt` L1-NAME-ALLOC, L1-STATIC-MUTABLE). Refusals leave the live
  machine bit-identical and never `release` an unprovable descriptor. [CODE]+[INF].
  Reversal: the summary cannot cover a needed cross-check (then a documented staged
  allocation as an explicit allocator-contract extension, a Phase 10 matter, not an owner
  stop). — **Reversibility:** costly — importer rules shape the Phase 10 allocator and
  error contract.
- **D-45:** Generated field-use audit. A 09-09 script maps every state field used in `core/`
  as an index, length, shift, loop bound or array subscript (about 500 fields) to its import
  range rule and fails on an unmapped field. [INF]. — **Reversibility:** reversible.
- **D-31:** Binding. Exact profile digest, state version, content digest and build id;
  cross-build import refused in v1 (a labeled diagnostic mode is deferred); distinct
  statuses PROFILE, CONTENT, BUILD, STATE_VERSION; importer registered per state version.
  [POL]. — **Reversibility:** reversible.
- **D-32:** Proof ladder and construction sources. T1 round trip, T2 export-import-export
  byte identity, T3 continuation (one-shot and 1/7/31-tick partitions, onto a dirty peer and
  onto the source after advancing), T4 non-interference. Required boundary set and its
  construction sources (each guarded by a "boundary reached" assertion on the state before
  export; builder names are candidates to be confirmed by the pattern mapper): authored ROMs
  for SI mid-transaction, EEPROM busy, pending input; `fixtures/audio` and
  `core/tests/test_ai.c` for AI two descriptors, paused partial packet and output-full stall;
  `core/tests/test_pi_dma.c` and `core/tests/test_sp_dma.c` for PI active/canceled and SP
  active plus pending; `core/tests/test_rsp_control.c` and siblings and `fixtures/rsp` for
  RSP mid-program; `core/tests/test_rdp.c`, `core/tests/test_rdp_triangles.c`, `fixtures/rdp`
  for RDP pending words; `core/tests/test_vi.c`, `core/tests/test_video_output.c` for VI
  mid-field, retained packet and stalled event; `core/tests/test_cpu_cache.c`,
  `core/tests/test_cpu_mmu.c`, `core/tests/test_cpu_path.c` for delay slot, dirty D-cache
  and TLB; `core/tests/test_determinism.c`, `core/tests/test_scheduler.c` for coincident
  events and near-exhaustion (small queue and sequence width). The required set is capped
  here; each boundary has a one-field perturbation control T3 must detect. Self-consistency
  of the model, not hardware. — **Reversibility:** reversible.
- **D-33:** State privacy. Blobs private (0600, no-replace, no symlink parents), never
  uploaded or committed; public evidence is a bounded projection of authored-fixture
  digests and sizes; the existing `scripts/rights-scan.sh` denied-extension table is
  extended for state/replay extensions and magic; no host paths, times or user names in
  headers. [CODE]. — **Reversibility:** reversible.

### CORE-05: replay
- **D-34:** Replay artifact. Canonical JSON `t64.replay` v1 (closed keys, decimal-string
  u64, lowercase SHA-256, no floats, duplicate keys refused, bounded parser, identity =
  SHA-256 of canonical bytes). Header: profile id/digest, descriptor/model/state versions,
  build id (binding), source revision (informational), content SHA-256 and size,
  `save_init` (`blank` or SHA-256 plus length), input schema version. Ops: `input{at,
  event_tick, port, buttons, x, y}`, `checkpoint{at, digest}`, terminal `end{at, digest,
  stop kind}`; `at` nondecreasing; equal `at` in array order; checkpoint after the advance;
  `event_tick >= at`. **Goldens are build-independent** (ops plus checkpoint digests as
  literals, header composed at test time; sanitizer and gcc:13 tests record fresh); the
  recorder wraps the actual accepted `t64_machine_queue_input` calls and the original run is
  driven from an independently authored `t64.input` script; a scripted pre-queue records
  `at = 0`; a driver `QUEUE_FULL` is a divergence/refusal report; profile, build and content
  mismatch each have a refusal control; a mutated `at` must diverge. Boot is `cold-admit`
  only (`state` reserved); caps 4 MiB, 65,536 ops, 4,096 checkpoints; mid-run save import is
  not replayable. Divergence goes to a separate closed `t64.replay-report` record, not
  `t64.result` v2. Recorder and driver are runner-side. [CODE]+[POL]. —
  **Reversibility:** reversible until Phase 10.
- **D-35:** Runner surface (provisional). The input schedule keeps its path; flags for save
  import/export, state export/import and replay record/play validate before guest start, use
  the verified-input snapshot for inputs and the new publisher for outputs, refuse undefined
  combinations, and carry a version string per format. The device is chosen by naming a
  registered profile id. Each plan adds its own supervisor forwarding row. —
  **Reversibility:** reversible.

### QA, security, CI and claims
- **D-38:** Harness mechanism (09-02, not 09-01). `--artifact-dir`/`--artifact-max-bytes`
  supervisor pair setting the real-mode child's `RLIMIT_FSIZE` to exactly
  `artifact_max_bytes + slack` only when present (default 69,632 =
  `T64_REAL_RESULT_MAX_BYTES` 4096 + 64 KiB, `runner/t64_supervisor.c:44,1534-1536`;
  accepted maximum bounded at 32 MiB); report and capture still bounded and controlled;
  changed backstop recorded in `docs/spec/raw-capture.md`; a **second, small publisher
  module** copying the Phase 8 transaction pattern with provenance and its own no-clobber
  and CR-01 substitution controls (`runner/t64_raw_capture.c` is left untouched);
  runner fixture/profile tables and observation branches generalized; inventory lane;
  fail-closed `scripts/qualify-phase09.py` stub; upload allowlist and rights-scan extension
  with mutation controls; manifest rows; claim skeleton; one real-runner admission and
  canonical tick on each new id. The real collector/verifier and `ci.yml` steps land once in
  09-03. `scripts/build-boot-fixtures.sh` is not touched (Binutils cache key, F-100). —
  **Reversibility:** reversible.
- **D-39:** Fixtures. Five authored ROMs from three assembler sources, each with a manifest
  row, build-only identity and rights gate before guest use, independent literals and a
  same-setup wrong-value control: `pad-poll` (09-03), `eeprom-rw` (4k and 16k builds) and
  `sram-rw` (09-04), `save-roundtrip` (09-05), `replay-mix` (09-07, length measured first).
  — **Reversibility:** reversible.
- **D-40:** Cheap-first order and hosted budget. In every plan: fixture identity/rights,
  schema/inventory/claims/CI scanners, `ast.parse` for Python, gcc:13 `-Werror` on changed C
  units, one accepted operation through the real runner with cleared input, focused CTest via
  the existing preset (four jobs), ASan selection, old-profile regressions; full gate only at
  the end of 09-03 (first hosted pair), 09-06 and 09-11. Hosted additions are budgeted against
  the **cold** cache figure (cold macOS 461-507 s of a 600 s limit; warm about 275-279 s;
  `docs/ci-reviews/LOG.md:116-117`, `08-05-SUMMARY.md:65`): about +25 s macOS and +25 s Linux,
  both measured in the first hosted pair; fuzz corpora, ASan, gcc:13, state-hash-heavy loops
  and the replay soak stay native; sampled mutation offsets only. No new CI job. —
  **Reversibility:** reversible.
- **D-41:** Importer security rules. Parse then validate then commit; caps before allocation;
  checked arithmetic; closed size sets; no overlap with live storage; identity binding;
  no-clobber publication; zero padding and reserved fields; structure-aware mutants that keep
  magic and checksums valid and each reach their specific refusal; independent review and
  security audit (09-10) against real mutated blobs and complete, with its fix loop, before
  F2. — **Reversibility:** reversible.
- **D-42:** Claims. One `docs/claims/phase09.md`; skeleton checked in 09-02, counts filled in
  09-11; [COMM] facts carry source URL and fetch date in the spec and are restated in
  authored words, not pasted. — **Reversibility:** reversible.
- **D-43:** Profile id scheme. Stem `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` (38 characters)
  with suffixes `-e4k`, `-e16k`, `-s32k` (longest 43); a test asserts every registered id is
  at most 63 characters across the core table and the runner allowed-profile table. [CODE]
  (current id 55 of 63, `runner/t64_real.c:40-41,488,681`). — **Reversibility:** costly —
  ids appear in tables, fixtures, manifests and claims.
- **D-46:** Review before freeze. 09-10 precedes F2; findings are fixed in 09-10 or gap
  plans, never after F2; one fix cycle budgeted. — **Reversibility:** reversible.

### Locked sequencing: plans and waves (strictly sequential, eleven plans)
- **S-01 09-01 Core identity and walker (wave 1; CORE-02 ledger/quantum, CORE-04 walker).**
  Family and ids, script-generated membership sweep, field validation, quantum validation,
  sink and v25 walker with the strip-and-compare differential, E0 ledger and FIFO
  characterization, stale digest-table correction in `docs/spec/machine-contract.md`. Core
  only. Frozen after: F0.
- **S-02 09-02 Runner, supervisor, publisher, harness mechanism (wave 2).** D-38 scope, no
  pre-built forwarding. Frozen after: publisher module and supervisor artifact pair.
- **S-03 09-03 SI/PIF/controller tracer and first hosted data (wave 3; SAVE-01, SI share of
  CORE-02).** First task D-44; SI, PIF, joybus, latch, `pad-poll`, one hosted pair, real
  collector/verifier and `ci.yml`, cross-platform controls from real two-platform data,
  warm and cold timing. Frozen after: FH.
- **S-04 09-04 Save devices (wave 4; SAVE-02, SAVE-04 device side).** EEPROM, SRAM, refusals,
  save section. Frozen after: save policy label and kind numbering.
- **S-05 09-05 Save import/export and persistence (wave 5; SAVE-03, SAVE-04 API side).**
- **S-06 09-06 CORE-02 ordering matrix (wave 6; CORE-02).** D-36 and D-37; full gate. Frozen
  after: F1 (state content).
- **S-07 09-07 Replay (wave 7; CORE-05).**
- **S-08 09-08 State export and container (wave 8; CORE-04 export).** Real-runner state-size
  control under the raised limit.
- **S-09 09-09 State import, invariants, boundary matrix (wave 9; CORE-04 import).** D-30,
  D-32, D-45.
- **S-10 09-10 Independent review, security audit and fix loop (wave 10).** Frozen after: FR.
- **S-11 09-11 Identity freeze and qualification (wave 11).** Literal pins, claims, evidence,
  native normal + ASan/UBSan + gcc:13 + both hosted jobs on the exact SHA; no logic edits.
  Success criteria 1-4 each map to a named test row and a claim block.
  Effort is low-confidence [INF]: Phase 8 took about 25 hours of commits and six native
  attempts; plan for a review/fix loop and at least two qualification cycles; gate nothing on
  estimates.

### Non-goals (locked)
- FlashRAM, Controller Pak, Rumble Pak, Transfer Pak, RTC, 64DD: refusal with defined
  postconditions only; no stubs that appear to work; kinds reserved.
- Banked or larger SRAM, SRAM mirroring, other-emulator save containers, header or database
  save detection, automatic persistence, any real save as a fixture.
- Ports 1-3 hardware behavior, hot-plug, stick shaping, RST/recentering, per-channel
  sequential sampling, direct CPU PIF access, WR4B/RD4B, CIC challenge, PIF ROM execution,
  other joybus peripherals, physical controller mapping (Phase 11).
- Class-rank scheduler policy and Q > 1 bursts; cross-build or cross-version state import;
  state-anchored replay; binary replay schema; compression; rewind and save-state UI.
- API/ABI freeze, installed consumer, host player, new dependency, new CI job, production
  thread; hardware, oracle or differential claims; VID-04; release (D6); naming (D7).

### Claim tiers and wording boundaries (locked)
Tiers per `docs/claims/TEMPLATE.md`. The words "accurate", "compatible" and "supports" are
reserved for hardware-derived claims and must not appear in any Phase 9 claim block or in a
prose line starting with the project name.
- **SAVE-01** `unit-tested` (the harness holds the expected values): an authored ROM reads the
  scheduled button and axis values for the standard controller on port 0 through the modeled
  SI/PIF joybus at the scheduled guest ticks of the model's named timing policy; Count
  readings are model time. Not claimed: other ports/peripherals, physical controllers, joybus
  timing against hardware, rumble, Controller Pak, commercial software.
- **SAVE-02/04** `unit-tested` (`self-checking-fixture` only if the guest emits its own
  verdict): explicitly selected EEPROM 4/16 Kbit and SRAM follow the authored block,
  busy-window and refusal contracts; unselected, unknown and deferred requests are refused
  with the stated postconditions. Not claimed: FlashRAM, save-chip timing, title-based
  detection, battery or wear, the 6 ms ceiling or the mirror as measured hardware facts.
- **SAVE-03** `unit-tested`: exported bytes round-trip with the named transforms; a refused
  import leaves the canonical digest unchanged. Not claimed: third-party formats.
- **CORE-02** `unit-tested`: ordered by one guest-time scheduler under the authored
  coincident/partition/interleave cases, pinned as regression behavior and compared across
  schedules (a consistently wrong tie would pass); MI ordered through its producers' events;
  quantum in identity and fixed at 1. Not claimed: hardware ordering, cycle accuracy, any
  other quantum.
- **CORE-04** `unit-tested`: exported state re-imports and reproduces the canonical digest and
  continuation digests for authored machines on the same build. Not claimed: cross-build or
  cross-version import, other emulators' states.
- **CORE-05** `regression-pinned` plus `unit-tested` for format and refusals: a recorded input
  replay bound to profile, build and content hash reproduces the recorded checkpoint digests;
  original run from an independently authored script; goldens are build-independent. Not
  claimed: portability across builds/platforms/profiles, equality across submission schedules.
Every `not_claimed` also names other builds, profiles and fixture sets, unexercised behavior
and commercial software. Hosted evidence compares platform-invariant outputs and digests only.

### Hard-won process rules (Phase 8 and `.planning/AUTONOMY.md`; apply in every plan)
- **Bound-source cycle trap.** Any change to a bound file (qualifier, collector, `ci.yml`,
  `scripts/check-*.sh`, scanner, supervisor logic) after native qualification costs a native
  plus hosted cycle (about 15 minutes, F-096/F-098). Land harness logic early (09-02/09-03),
  freeze it (FH), qualify once at the end, and do review fixes before F2.
- **Early verifier-tail replay.** Run every new collector/verifier tail against real retained
  hosted artifacts, and build cross-platform comparison controls from real two-platform data,
  before the final cycle (F-098; two Phase 8 defects appeared only on real hosted data).
- **Cheap-first checks.** Seconds-long local checks before long suites (D-40); `act -l/-n` for
  Linux jobs, never hosted evidence; reproduce CI locally; use the existing presets and four
  jobs; keep the two hosted jobs and ten-minute limits; do not touch
  `scripts/build-boot-fixtures.sh`.
- **RED before GREEN.** Capture the first failing behavioral assertion and observed values (a
  setup or addressing failure is not RED); a `tdd=true` task records its actual pre-implementation
  report; verification-only work may start green with a recorded rationale.
- **Mutant-must-compile.** Every mutation or wrong-value control must build, run the accepted
  setup, and reach its specific comparator; assert each intended case hits its specific refusal.
  A green process alone does not prove cases ran; the case list must equal the JSON inventory.
- **Independent expectations.** Expected values are authored literals derived independently of
  the code under test (pin a field table and a literal producer packet before decoder breadth);
  a helper repeating the production transform is not an oracle.
- **Identity checks.** New-descriptor check: mutate every policy field through hash,
  serialization and known-id construction, test empty/unterminated policy text, pin retained
  older bytes; keep version membership explicit.
- **Evidence retention.** Copy the owning `LastTest.log` before any later CTest or discovery;
  unique JUnit path per manual batch; sanitize before the first commit; run the tree PII and
  rights scans before committing evidence; repo-relative paths only; `ast.parse` not
  `py_compile`.
- **Refusal and old-profile pins.** Every refusal asserts bit-identical digest; every plan keeps
  the old-profile pins and refusals; `T64_STOP_GUEST_TICK` is absolute.
- **gcc:13.** Bounded read-only network-disabled `-Werror` check of changed C units with the
  existing image before publication (Clang-clean code failed GCC13 twice).
- **Efficiency.** Reuse approved contracts; begin the smallest verifying implementation
  promptly; reopen design only for a concrete contradiction or failing witness; orchestrator
  writes routine summary/state closeout; do not rerun passing pairs for bookkeeping.

### Claude's Discretion
Internal representations, final profile-id and label text within the 63-character limit and the
D-43 scheme, domain string text, section id numbering, exact test names and registration,
container field order details, state-boundary builder choice per D-32 (subject to the
"reached" assertion), flag names (provisional), and the number of plan tasks. Reopen a locked
decision only for a concrete contradiction or failing witness.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project authority and policy
- `AGENTS.md` — goals, evidence rules, rights stops, autonomy.
- `.planning/AUTONOMY.md` — sections 1, 2 (verification, CI/CD, process lessons) and 4 (explicit owner stops).
- `.planning/PROJECT.md`, `.planning/ROADMAP.md` (Phase 9, Phase 10 boundaries), `.planning/REQUIREMENTS.md` (SAVE-01..04, CORE-02/04/05), `.planning/STATE.md` (Decisions, follow-ups).
- `.planning/bootstrap/EMULATOR-DNA.md`, `.planning/bootstrap/START-HERE.md` — project seed and constraints.
- `.planning/research/ARCHITECTURE.md`, `.planning/research/FEATURES.md`, `.planning/research/PITFALLS.md`, `.planning/research/STACK.md`, `.planning/research/SUMMARY.md` — prior research (dated; verify before relying).

### Phase 9 discussion record
- `.planning/phases/09-controller-save-media-and-replay/09-DISCUSSION-PROPOSAL.md` — the synthesized, adversarially revised recommendation (verification log, conflict resolutions, section 10 dispositions).
- `.planning/phases/09-controller-save-media-and-replay/09-DISCUSSION-ADVERSARIAL.md` — independent review and findings H1-H6, M1-M9, L1-L6.
- `.planning/phases/09-controller-save-media-and-replay/09-FANOUT-A-si-pif-controller.md`, `09-FANOUT-B-save-media.md`, `09-FANOUT-C-state-replay.md`, `09-FANOUT-E-scheduler.md`, `09-FANOUT-F-qa-slicing.md` (same directory) — cluster research; superseded where the proposal differs.

### Specifications to read and update
- `docs/spec/machine-contract.md` — bus map, PI model, scheduler and digest text (stale digest table to be corrected in 09-01), allocator contract, replacement reset table, output/timed-input contract.
- `docs/spec/cpu-compatibility.md` — compatibility policy (preserve identities, version concrete changes), v2 profile precedent.
- `docs/spec/audio-interface.md` — Phase 8 composite-profile precedent (descriptor 11/model 19/state 24), packet contract, determinism checks.
- `docs/spec/raw-capture.md` — staged no-clobber publication transaction, the known 68 KiB limit.
- `docs/spec/result-protocol.md` — closed result schema (replay report is a separate record).
- `docs/spec/vi.md`, `docs/spec/cpu-timing.md`, `docs/spec/cpu-profile.md`, `docs/profile-limits.md` — device and profile timing/limits.
- `docs/claims/TEMPLATE.md`, `docs/claims/phase08.md` — claim format and a worked example; `docs/evidence/phase08-audio.md` — evidence shape and limits.
- `docs/ci-reviews/LOG.md` — F-096..F-100 findings, warm/cold CI timings.

### Phase 8 precedent and lessons
- `.planning/phases/08-audio-interface-and-raw-capture/08-CONTEXT.md`, `08-DISCUSSION-PROPOSAL.md`, `08-REVIEW.md` (CR-01 publication race, WR-01), `08-SECURITY.md`, `08-VERIFICATION.md`, `08-05-SUMMARY.md` (qualification attempt history and cost).

### Source files that decisions depend on
- `core/t64_scheduler.c` (order key, reserve/commit, dispatch, backpressure), `core/t64_internal.h` (`t64_event`, `t64_has_*` chain, device ids, state structs), `core/t64_digest.c` (walker), `core/t64_machine.c` (profile table, validator, writer, create-time storage), `core/include/tendies64/t64_machine.h` (descriptor, statuses, input and observation API).
- `core/t64_observe.c` (`t64_machine_queue_input`), `core/t64_pi.c` (DMA model SI mirrors), `core/t64_bus.c`, `core/t64_warm_devices.c` (SI/PIF windows today), `core/t64_mi.c`, `core/t64_cp0.c` (Count mapping), `core/t64_cpu.c` (store preflight `:1483-1550`, step), `core/t64_exception.c`, `core/t64_ai.c`, `core/t64_vi.c` (packet queues, observer state), `core/CMakeLists.txt` (`T64_BUILD_ID` manifest).
- `runner/t64_supervisor.c` (option parsing, child argv, `RLIMIT_FSIZE`), `runner/t64_real.c` (profile checks, input schedule queueing), `runner/t64_real_fixture.c`, `runner/t64_real_fixture.h` (approved identities), `runner/t64_input.c`, `runner/t64_input.h`, `runner/t64_verified_input.c`, `runner/t64_raw_capture.c` (pattern only; do not generalize), `runner/t64_synthetic.c` (flood control), `runner/t64_real_result.h`.
- `tools/lint/rules.txt` — core lint rules (no allocation names, no mutable statics).
- `core/tests/test_profile.c`, `test_determinism.c`, `test_scheduler.c`, `test_ai.c`, `test_vi.c`, `test_video_output.c`, `test_pi_dma.c`, `test_sp_dma.c`, `test_rdp.c`, `test_rdp_triangles.c`, `test_rsp_control.c`, `test_cpu_cache.c`, `test_cpu_mmu.c`, `test_cpu_path.c`, `test_boot_entry.c`, `fuzz_rom_admission.c` — existing builders and patterns (confirm each reaches its boundary).
- `fixtures/audio/pcm.S`, `scripts/build-audio-fixture.sh`, `fixtures/manifest.json`, `fixtures/manifest.schema.json` — authored-fixture template and manifest fields.
- `scripts/qualify-audio.py`, `scripts/gate.sh`, `scripts/check-ci-yml.sh`, `scripts/check-test-inventory.sh`, `scripts/check-claims.sh`, `scripts/rights-scan.sh`, `scripts/pii-scan.sh`, `.github/workflows/ci.yml`, `CMakePresets.json` — gates, scanners and the bound-source set.

### External sources (facts restated in authored words; record URL and fetch date in the spec)
- n64brew Joybus Protocol, PIF-NUS, Serial Interface and Peripheral Interface pages (https://n64brew.dev/wiki/Joybus_Protocol, https://n64brew.dev/wiki/PIF-NUS, https://n64brew.dev/wiki/Serial_Interface, https://n64brew.dev/wiki/Peripheral_Interface) — community documentation; text may be CC BY-SA, do not paste tables or vectors.
- Nintendo SDK `rcp.h` and programming manual pages mirrored at https://ultra64.ca/files/documentation/online-manuals/ (register map; EEPROM section 26-06) — vendor documents; the SDK 15 ms figure is carried, not re-fetched.
- libdragon `joybus.h` identifiers and EEPROM reference — reference only; nothing copied.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- PI DMA model (`core/t64_pi.c`): reserve-then-commit, token cancel, atomic effect at the completion event, sticky error on busy start — the template for SI and SRAM DMA.
- Profile machinery (`core/t64_machine.c`): immutable descriptor, text policy fields (`ai_policy` precedent), hashed `save_kind` and `peripheral_flags`, create-time profile-keyed storage (video/audio rings) — the pattern for device storage and the new fields.
- Digest walker (`core/t64_digest.c`): helpers `hash_u32/hash_u64/hash_event`, per-version domains; already hashes event tuples, `next_sequence`, retained packets.
- Input path: `t64_machine_queue_input` (`core/t64_observe.c`) and the runner's closed `t64.input` v1 parser with verified-input snapshot (`runner/t64_input.c`, `runner/t64_verified_input.c`).
- Capture transaction (`runner/t64_raw_capture.c`): parent-fd walk, 0700 staging, 0600 files, `RENAME_EXCL`/`RENAME_NOREPLACE`, identity-checked cleanup — the pattern copied into the new publisher.
- Authored-fixture template: `fixtures/audio/pcm.S` (CP0 interrupts masked, uncached RDRAM, device register writes, ISViewer reporting), `scripts/build-audio-fixture.sh`, manifest rows at `fixtures/manifest.json:658-710`; the same separately rights-cleared IPL3 composition.
- Test patterns: independent reference encoders (`core/tests/test_boot_entry.c`), field-mutation digest tests, partition/interleave tests (`core/tests/test_determinism.c`, `core/tests/test_ai.c`), bounded fuzz (`core/tests/fuzz_rom_admission.c`), seeded negative controls (`tests/seeded`).

### Established Patterns
- Compatibility policy: new composite profile with descriptor/model/state bumps, older identities and refusals preserved, version membership explicit.
- Refusal before mutation: every refusal leaves the digest bit-identical; reserve-then-commit with queue-full and sequence-exhaustion atomicity.
- Core owns no files, clocks or allocation outside create/admit/replace/destroy; the runner owns files and publication.
- Guest tick is 1/62,500,000 s; Count advances once per two ticks in the model.
- All events order by `(tick, sequence)`; sequence assigned at reserve time; MI is set synchronously by producer handlers and sampled by the CPU at step start.

### Integration Points
- `core/t64_machine.c` profile table and every `t64_has_*`/model-state ladder (new tuple 12/20/25).
- `core/t64_bus.c` and `core/t64_warm_devices.c` (SI window, PIF window, domain 2 SRAM window); `core/t64_pi.c` (SRAM start path, read-length for SRAM profiles only).
- `core/t64_cpu.c` store preflight (profile-gated SI budget).
- `core/t64_digest.c` sink seam and v25 branch; `core/t64_internal.h` predicates and new SI/PIF/save state structs.
- `runner/t64_real.c`, `runner/t64_real_fixture.c/.h`, `runner/t64_supervisor.c` (fixture/profile admission, flags, forwarding rows, FSIZE), new publisher module, new replay recorder/driver module beside `runner/t64_input.c`.
- `scripts/` and `.github/workflows/ci.yml` (qualifier, collector/verifier, scanners, inventory) — touched in 09-02/09-03 only, then frozen (FH).

</code_context>

<specifics>
## Specific Ideas

- Unresolved single-source points are named policy, never hardware claims: joybus wire times, `W`/`X`, single sample point, button bit positions, EEPROM busy-read behavior, SRAM size, blank contents, SI alignment mask.
- The RD64B/WR64B naming trap (RD64B moves PIF to RDRAM) gets a literal two-direction test with a swapped-direction control.
- Two submission schedules of the same logical input give identical behavior but different digests; the replay `at` log, not core equality, is the contract.
- Hosted evidence comparison uses platform-invariant data only; build ids differ per platform by design, which is why `build_id` is outside the v25 digest.
- An in-guest Count witness in `pad-poll` keeps expected-time evidence from being purely policy-derived; wording still says it is model time.

</specifics>

<deferred>
## Deferred Ideas

- Class-rank scheduler order, Q > 1 RSP bursts (v2 performance milestone).
- FlashRAM (EXP-04), Controller/Rumble/Transfer Pak (EXP-05), RTC (EXP-06), 64DD (EXP-07).
- Ports 1-3, per-channel sequential joybus sampling, PIF challenge, direct PIF access (EXP-03 and later).
- Cross-build state import (labeled diagnostic mode), state-anchored replay, binary replay schema, rewind and save-state UI (PLAY-01).
- Optional hardware measurement of EEPROM/SI timing (an AUTONOMY section 4 stop when pursued), and a second-source check of button bit positions.
- Larger raw capture beyond what the artifact-dir mechanism incidentally allows.
- API/ABI shapes and the installed consumer (Phase 10); host player and playable homebrew (Phase 11).
- Docs-only CI lane (F-099) stays declined unless the owner asks.

</deferred>

---

*Phase: 09-controller-save-media-and-replay*
*Context gathered: 2026-11-19*

## Amendments from planning research (2026-11-19)

`09-RESEARCH.md` checked the locked decisions against the code and found sixteen
facts (K1-K16, with file:line evidence) that contradict a locked sentence or add a
hidden dependency. Where this section and an earlier sentence in this file differ,
this section wins; the decision numbers above are unchanged and each amendment names
the decision it corrects. These are routine reversible planning resolutions under the
autonomy policy, not owner approvals. Planners and executors must cite the K-id.

- **K1 (D-01, D-05):** the shipped Phase 8 descriptor 11 has RDP depth inactive (the
  predicate is guarded by `descriptor_version == 10u`). Keep depth inactive on descriptor
  12, make no depth claim, and capture a golden predicate matrix for every registered id
  before editing any ladder. Record a STATE follow-up.
- **K2 (D-01):** `t64_isviewer_active` is a per-id allow-list and is a membership site;
  add it to the membership-site list.
- **K3 (D-16, D-20, D-39):** the real runner refuses a guest bound above 600,000 ticks in
  two places, and authored user code first retires near tick 538,800. Do not raise the
  bound globally: add a per-fixture tick cap to the approved-fixture table, defaulting to
  the current value, and give only the fixtures that must outlast the 375,000-tick EEPROM
  busy window a named larger constant (4,000,000 suggested; probe cost about 0.06 s per
  539,000 ticks). Decided and tested first in 09-02.
- **K4 (D-16):** the runner reports `pass` only for the exact 30-byte hello transcript.
  Add per-fixture expected-transcript pins (and result-check pins) to the fixture table,
  decided first in 09-02 before any fixture is authored; the independent oracle remains a
  Python transcript derived from literals held outside the ROM.
- **K5 (D-36):** the first required mutant (handler reads `machine->tick` instead of
  `event->tick`) is equivalent because dispatch sets the machine tick first. Replace it
  with non-equivalent mutants: latch `visible_from` computed from the wrong tick and
  EEPROM `busy_until` computed from the wrong tick.
- **K6 (D-35):** the supervisor child argv is a fixed `child_args[24]` with no bound
  check; 09-02 enlarges and bounds it before adding flags.
- **K7 (D-12, D-15):** the shared `t64.input` v1 parser accepts port 0 only. Replay v1
  keeps a `port` field restricted to 0; ports 1-3 stay a non-goal.
- **K8 (D-30):** core builds with `-Wframe-larger-than=2048`; the 256-entry invariant
  summary cannot live in one stack frame. Use machine-owned or staging storage.
- **K9 (D-38):** the synthetic flood child runs under the generic-mode FSIZE limit, not
  the real-mode line; test the artifact pair through a `t64-runner-test` fault.
- **K10 (D-37):** the quantum is bound through `profile_digest`; add no key to
  `t64.result`.
- **K11 (D-42):** the claim skeleton uses `t64-claim-example` fences until 09-11 (the
  checker requires the seven counts to sum and a byte-exact statement).
- **K12 (D-38, D-39):** ROM bytes bind exactly one profile id and runner rows need a
  literal profile hash; 09-02 authors tiny admit ROMs (`fixtures/phase09/admit.S`) and a
  profile-hash print helper.
- **K13 (D-40):** the inventory selftest cost scales per CTest name (about +15 s on macOS
  for 78 names); budget 45 or fewer new CTest names, consolidating cases where a name adds
  no isolation value.
- **K14 (D-36):** the Count-wrap case uses the direct-state pattern of
  `core/tests/test_cpu_time.c`.
- **K15 (S-02, S-08):** new runner modules and flags join `T64_RUNNER_BUILD_INPUTS`.
- **K16 (D-09, D-44):** `t64_schedule_commit` invalidates other outstanding reservations
  and cancel does not restore `next_sequence`, so "reserve both SI events before busy is
  visible" is impossible with the current API. Use a capacity pre-check (queue room and
  sequence headroom for all events the operation needs), then reserve and commit
  sequentially so a refusal leaves no partial state; the CPU store preflight reuses the
  same rule.

## Amendments from plan checking (2026-11-19)

The plan checker and the cross-plan review (`09-PLAN-CHECK.md`, `09-PLAN-CROSSCHECK.md`) found
the following, resolved as routine reversible planning decisions under the autonomy policy
(not owner approvals). Where this section and earlier text differ, this section wins.

- **K17 (D-36):** the locked mutant "SI completion from the per-advance budget" is written as
  `si-completion-from-event-budget` (the literal form is equivalent under the canonical profile),
  and "sequence at commit instead of reserve" is realized as `cpu-successor-stale-sequence`; two
  control mutants (`control-equivalent-handler-tick` must SURVIVE, `control-no-compile` must be
  INVALID) check the driver's own classification. 09-06 carries these.
- **K18 (D-39):** fixtures use one assembler source and one build script per family (admit, pad,
  eeprom, sram, roundtrip, mix) instead of three sources; defsym variants provide the wrong-value
  controls. The rights and manifest rules are unchanged.
- **FH moves to the end of 09-03.1 (D-06, D-38):** 09-03 builds the SI/joybus/latch tracer and 09-03.1
  lands the real collector, the ci.yml and check-ci-yml.sh changes, the first hosted pair and FH. The
  locked full-gate schedule becomes: full local gate at the end of 09-03, 09-03.1, 09-06 and the
  09-10 rehearsal, the exact-SHA native gate and both hosted jobs in 09-11, and one non-qualifying
  hosted rehearsal after the 09-10 fix cycle so a hosted-time overrun is repaired before the freeze.
- **Mid-tick state import (D-36) is owned by 09-09.1:** the boundary matrix carries "export, import
  at a mid-tick stop, continue equals the uninterrupted run" on production events (the 09-06
  synthetic-event recipe is refused by the closed handler table and stays a refused control).
  09-10 carries review findings only (D-46).
- **`write_generation` on state import:** decided in 09-09 with a test (a successful state import into
  a profile with a save device increments it exactly once; a refused import never does; replacement
  resets it to 0). The independent review may challenge the choice.
- **One artifact producer per run:** save export, state export and replay record publish through the
  one frozen artifact directory pair; each later flag family refuses combination with earlier
  families (state export refuses with save and replay flags; state import refuses with all earlier
  families), and the 09-10 drift sweep verifies the option-pair matrix.
- **Goal-backward verification before the qualifying cycle:** the independent gsd-verifier pass moves
  inside 09-10 (between the review/audit and the single fix cycle) so its findings land in the
  budgeted fix cycle before F2; after 09-11 the orchestrator's verification is confirmatory.
- **CTest-name budget:** 45 gate names total; the joybus fuzz is a case inside an existing name with
  environment-raised iteration counts under ASan (no ASan-only name), and 09-10 and 09-11 add zero.
- **Plan size:** every plan exceeds the generic 100,000-token estimate; recorded as an accepted scope
  decision (the authors already split two plans; the Phase 8 precedent ran five plans at the same
  density; SUMMARY files record actual cost and any further split).
- **K19 (D-24 check order for the info call):** `t64_machine_save_info` (09-04) must write its output
  structure, so it validates pointers before admission; the import and export calls (09-05) keep the
  locked D-24 order (admission first, then pointer and length). The difference is deliberate, recorded
  in the 09-05 and 09-09 specs, and tested by each plan's refusal matrix; D-24's order governs
  import and export, and the info call is a provisional API (Phase 10 owns any freeze).
- **Cold-time basis:** the cold CI projection adds the larger of the measured cold-minus-warm job delta
  and the fixture-step delta (macOS 186 s from F-100: 461 s cold, 275 s warm; Linux 84 s), so the
  600-second clause is conservative; warm must stay at or below 540 s.
- **Replay `save_init`:** the `image` form is verified in process only; the real runner refuses replay
  together with the save flags, so a supervised run records `blank` only. The replay spec and claim
  block 10 say so.
