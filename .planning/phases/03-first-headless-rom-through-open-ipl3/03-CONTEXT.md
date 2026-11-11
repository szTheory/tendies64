# Phase 3: First Headless ROM Through Open IPL3 - Context

**Gathered:** 2026-11-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Execute a project-authored hello cartridge through HLE-PIF initialization and the
actual pinned open IPL3, capture exact ISViewer guest bytes, and emit a real
headless result with deterministic state across both existing hosted lanes.
The primary evidence index opens this phase. All six roadmap success criteria
remain binding. The claim is this named prepared-RDRAM warm-entry smoke path;
full cold boot, CPU/RSP breadth and controller consumption remain later work.
</domain>

<decisions>
## Implementation Decisions

### Boot input and provenance

- **D-01:** Pin n64brew/libdragon commit `98154dafd5ac50e74ee09a5de1035d24a53f54f0`,
  `boot/bin/ipl3_compat.z64`, the r8-source compatibility variant. It is the flat
  loader, with direct Unlicense source/binary coverage documented in Decision A.
  Tendies64 maintainers own the pin, notices, review and updates. Verify actual
  bytes/digest lengths and source provenance before admission; recommendations
  do not substitute for the fixture manifest and production checks.
- **D-02:** The baseline is an upstream committed generated artifact retrieved
  from the pinned source, with byte identity checked. Its original compiler and
  signer are unknown. A deterministic fetch/assembly recipe can reproduce the
  selected fixture input without proving regeneration from source. Qualify a
  clean source rebuild separately before making that stronger claim; pin tools,
  remove stale compat output, distinguish signature-region differences and retain
  the full digest. No full SDK or unsigned alternative silently replaces the pin.
- **D-03:** Keep every downloaded manual/document outside the repo and CI
  artifacts. Commit the primary evidence index, URLs, exact hashes, identity,
  sections and claim scope only. Hardware facts, upstream software expectations,
  project model policies and unknowns must remain distinct. CIC checksum signing
  does not authorize or require Nintendo firmware bytes.

### Deterministic boot machine

- **D-04:** Add a clearly named, versioned NTSC/cartridge/4 MiB prepared-RDRAM
  warm-entry profile. Its HLE-PIF transition copies the owned open IPL3 payload
  and starts CPU execution at the source-defined entry; it does not skip IPL3.
  Prepared memory/reset type, boot artifact identity, HLE seed/version and CPU/
  cache/time policies belong to profile identity and documented limits. Exact
  original IPL2 handoff values remain unknown. Select concrete numeric seed/state
  as an explicit software contract before implementation, from the pinned path's
  requirements; do not label guessed constants as hardware observations.
- **D-05:** The first executable slice pins tools, inspects/disassembles the
  actual linked artifact and records the warm path's opcode/device inventory.
  This is a prerequisite to bounded CPU implementation, not a source-level guess
  or permission checkpoint. Support the reachable integer/control/delay-slot,
  load/store, CP0 and cache behavior with independent expectations; unsupported
  execution stops explicitly and never becomes an implicit no-op or pass.
- **D-06:** CPU execution uses the existing per-instance deterministic scheduler;
  define instruction retirement, guest ticks, device deadlines and stop ordering
  together. Host clocks remain backstops only. Include all future-affecting CPU,
  CP0, prepared-memory, boot and device state in the versioned canonical digest;
  update profile/digest versions and independent goldens deliberately.
- **D-07:** Implement the bounded reset/register/probe subset the actual warm path
  consumes (RI preset, memory-capacity/zero-source policy, MI/SP/PI and SI/AI
  reset writes), with explicit policy labels where hardware evidence is absent.
  CPU code executing from SP DMEM is not RSP instruction execution. Keep the RSP
  engine in Phase 5 and RI/RDRAM cold calibration in Phase 6. Verify virtual
  KSEG accesses before routing them to the existing physical debug bus.

### Authored fixture, output and time

- **D-08:** Author a tiny MIPS assembly hello plus linker script and narrow
  deterministic cartridge packer. Use a source/byte-pinned cross assembler/linker
  toolchain for that concrete capability; avoid the full SDK and a new custom
  assembler. No nemu64-test or commercial/protected bytes. Record source and
  binary rights separately, tool identities, recipe, size/hash, expected text
  provenance, permitted CI/redistribution and boot/profile requirements.
- **D-09:** Model the narrow libdragon ISViewer compatibility contract, not an
  assertion about original ISViewer hardware: magic, 512-byte buffer, big-endian
  stores and byte-count flush at +0x14, with a separately specified +0x16 halfword
  lane. Flush exact requested bytes without alignment padding. Bound output,
  handle zero/partial/full/oversize/repeated flushes, and never let lost/truncated
  capture produce pass. Output crosses a core-owned bounded copy seam, not host
  I/O callbacks during advance. Define/drain/test it before promising an API.
- **D-10:** Parse a bounded canonical timed logical-input schedule, bind its
  digest to the result and emit events at their guest ticks through the existing
  trace seam. This phase proves scheduling/trace only; it does not claim SI/PIF
  controller consumption. Trace-on/off and advance partitions must preserve guest
  outcomes and final state at the same bound.

### Host/result attestation

- **D-11:** Close WR-06 before any real guest result: bounded copy and hash into
  a private per-run file, finalize/read-only/reopen/verify, unlink and pass the
  already-open descriptor only. Real runner admission reads that descriptor's
  bytes and binds the result to their digest, never reopens the original path.
  Protect input schedule bytes equivalently. Transactional failures must not
  launch a guest or produce pass; close/reap/clean every descriptor/process path.
- **D-12:** The existing real-protocol baseline is `t64.result` **v1**, currently
  synthetic-only; no v2 exists yet. Add explicit strict real **v2** and dual-read
  support while retaining byte-identical v1 behavior/goldens and separate native
  `t64.model-result` v1. Require real build/profile/fixture/input/log/state identity
  and guest stop/time fields. Keep unknown-field/version/refusal rules strict;
  internal schema versioning is separate from the unresolved release channel.
- **D-13:** Pass requires the authored fixture's exact transcript and actual CPU
  progress through IPL3 and hello. Preserve the six outcomes and harness-error
  precedence. Empty output is boot-fail even at a guest bound; non-empty incomplete
  bounded output is timeout; unsupported execution and refusal are explicit.
  Host kills/crashes/report failures remain harness errors. Synthetic misbehavior
  controls retain their own scope and cannot serve as CPU execution evidence.
- **D-14:** Use the existing Linux/GCC and macOS/Clang gate jobs. Both must load
  identical authorized fixture/input/profile bytes, assert exact guest text and
  compare the same canonical state digest; compiler build identities may differ.
  Partition/interleave/trace checks and meaningful wrong-text/empty-output,
  unsupported-opcode, source-replacement/descriptor and tool/fixture-identity
  controls must fail for the right reason. Review each hosted run's measured cost.

### Agent discretion

- **D-15:** Choose small module boundaries, command spelling, precise bounded
  capacities and tool pins from the above contracts and existing patterns. Resolve
  reversible engineering choices autonomously. Any new rights conflict, protected
  content use, physical test, publication/paid/destructive action outside standing
  policy is an explicit stop. No owner acknowledgment is needed for ordinary
  implementation, tests, CI, reviews, fixes or tracking.
</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets

- Per-instance owned admission/profile/state, physical bus, scheduler, PI/SP DMA,
  MI, digest and trace in core/. Public machine API and machine-contract define
  refused-operation and caller storage obligations.
- Existing supervisor, canonical result checker/JSON helpers, SHA primitive,
  fixture manifest, claims gate and negative controls in runner/, fixtures/ and
  scripts/. Native model producer/consumer stays a separate evidence surface.

### Established Patterns

- Validate/prepare/commit; checked resource reservations; named policies;
  explicit closed schemas; exact test inventory; all-target sanitizer builds.
- Consumer test targets depend on the producer/checker executables they invoke.
  Keep targeted builds truthful and exercise the second hosted compiler early.

### Integration Points

- CPU scheduler step and future-state digest; boot/profile seed lifecycle;
  CPU virtual-to-physical bus; missing warm-path reset/probe registers;
  bounded ISViewer output and timed-input trace; descriptor-backed supervisor
  launch; real v2 producer/checker; fixture builder/provenance and existing CI.
</code_context>

<specifics>
## Specific Evidence and Limits

Decision A records exact IPL3 identity and direct Unlicense coverage. Decision B
and the primary evidence index separate primary hardware facts from source entry
expectations; exact original IPL2 handoff and compiled opcode coverage remain
unestablished until the executable qualification slice. Decision C records the
real TOCTOU gap, original ISViewer-hardware uncertainty and strict result boundary.
</specifics>

<deferred>
## Deferred Ideas

- Full VR4300 ISA/CP0/TLB/cache/COP1 qualification: Phase 4.
- RSP scalar/vector execution: Phase 5; true cold RI/RDRAM/calibration: Phase 6.
- RDP/VI/audio and guest-visible controller/save/replay: later roadmap phases.
- New IPL3 variants, full libdragon SDK/ELF path and general archive parsers.
- Owner stops remain unchanged: nemu64-test rights, release channel, copy-license
  expansion, oracle rights, protected owner files and physical hardware testing.
</deferred>
