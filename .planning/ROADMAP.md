# Roadmap: Tendies64

## Overview

Tendies64 starts as an empty repository and works toward a deterministic, headless-first
Nintendo 64 machine core in which every support claim names its profile, build and evidence.
The order is: governance and the harness first, so negative controls exist before any emulator
code; then a deterministic machine skeleton; then the earliest end-to-end proof, a hello ROM
booted through an open IPL3 and observed through ISViewer; then the complete VR4300, the RSP and
the full `nemu64-test` base suite. Later milestones add RDP/VI video, AI audio, the controller,
save media and replay, then a frozen C API proven through an installed consumer and a playable
rights-cleared homebrew. The long-term goal stays broad and accurate N64 coverage: more regions,
CICs, expansion and accessory profiles, 64DD and performance work. Each is promoted only on
evidence (see v2.0 below).

**This roadmap is rolling** (`.planning/AUTONOMY.md`). Milestone v1.0 is planned in detail.
Milestones v1.1, v1.2 and v2.0 are provisional outlines. They are revised at each milestone close
and whenever new evidence changes scope, order or risk.

**GSD milestone IDs are not software versions.** `v1.0`, `v1.1` and the rest identify planning
cycles only. Software release versioning and the release channel are an open owner decision (D6).
The v1 requirements in `REQUIREMENTS.md` span milestones v1.0 to v1.2.

**Hardware evidence rule.** Hardware-behavior requirements are hypotheses until they are checked
against a primary document pinned by SHA, or against a hardware-derived test. The boot, CPU, RSP
and RDP phases each begin with that check.

## Milestones

- 🚧 **v1.0 Verified Headless Core** - Phases 1-6 (active, detailed)
- 📋 **v1.1 Observable Machine** - Phases 7-9 (planned, provisional outline)
- 📋 **v1.2 Embeddable and Playable** - Phases 10-11 (planned, provisional outline)
- 📋 **v2.0 Breadth, Accuracy and Performance** - phases numbered when the milestone opens (v2 requirements)

## Decision Gates

These gates block specific phases. Already decided, so not gates: Apache-2.0 license and a public
repository under szTheory (owner, 2026-11-09).

| ID | Decision | Status / who decides | Default until decided | Blocks |
|----|----------|----------------------|-----------------------|--------|
| D1 | Implementation language, build system, API/ABI | Recorded as the Phase 1 ADR. Self-unblockable, but one-way once core code exists, so it goes in the owner decision batch. Not an owner stop | C17, CMake 3.25+, Ninja, CTest (Rust is the runner-up) | Any core code merging (Phase 1 exit) |
| D2 | `nemu64-test` ROM use in public CI and public artifacts | **Owner stop** (AUTONOMY section 4). Rights not documented (Geneva 9 font, embedded IPL3, compiled crates) | Build and run locally only. No committed ROMs, no uploaded artifacts, no public results derived from it | Public CI lanes and published results in Phases 4-6. Does **not** block local work |
| D3 | IPL3 variant/revision pin and maintenance owner | Resolved in Phase 3 | n64brew/libdragon IPL3 compat pinned to 98154dafd5ac50e74ee09a5de1035d24a53f54f0; source/binary Unlicense reviewed; Tendies64 maintainers own the pin. See Phase 3 primary/provenance and verification records | Phase 3 (BOOT-02) |
| D4 | Copy allowlist beyond ISC, MIT, BSD-3, Unlicense | Open (owner) | Keep the allowlist as written. Link-only zlib host dependencies follow the dependency policy, with notices | Any copy of out-of-list code (e.g. BSD-2, zlib, MIT-0, Apache-2.0 sibling code) |
| D5 | Angrylion-Plus as a local oracle; redistribution of derived expected data | Unknown (owner or legal) | Out-of-tree local runs only. Never copy its code. Derived data stays local | Phase 7 conformance claims that use it |
| D6 | Release channel and versioning | Open (owner) | Research and local implementation continue; nothing is released | Releases at milestone close only |
| D7 | "Tendies64" name clearance | Not trademark-cleared | No branding, logos or store listings | Public branding and store presence |

The standing hard stops in AUTONOMY section 4 apply to every phase: protected content and fixtures
without documented rights; physical-console measurement; PII, credentials and private paths; and
paid services.

## v1.0 Verified Headless Core

**Milestone Goal:** A deterministic machine core boots a project-authored ROM through an open IPL3
with no Nintendo boot bytes. Its VR4300 and RSP pass native tests that use independent expected
values. The full `nemu64-test` base suite is classified per group with pinned inventories. Every
result names its profile, build identity and evidence tier, and digests match across two operating
systems and two compilers.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Governance, Rights and Harness Foundation** - ADRs, provenance and fixture policies, the gate command, determinism lint and a six-outcome runner with negative controls, before any emulator code (completed 2026-11-10)
- [x] **Phase 2: Deterministic Machine Skeleton** - Guest-time scheduler, named profile, memory map, RDRAM, MI, PI/SP DMA, ROM admission and digests equal across interleaved instances (completed 2026-11-10)
- [x] **Phase 3: First Headless ROM Through Open IPL3** - Primary documents pinned; HLE-PIF plus pinned open IPL3 boots a hello ROM that prints through ISViewer, with digests matching across two operating systems and two compilers (completed 2026-11-11)
- [x] **Phase 4: Complete VR4300 CPU** - MIPS III integer, delay slots, CP0, TLB, unaligned access, LL/SC, cache scope and soft-float COP1, with per-group `nemu64-test` CPU status (completed 2026-11-13)
- [ ] **Phase 5: RSP Low-Level Emulation** - Scalar and vector units, DMA and status semantics, checked against a permanent scalar VU oracle and the `nemu64-test` RSP groups
- [ ] **Phase 6: Cold Boot, CPU Timing and Full Base Suite** - RDRAM register protocol, instruction-granular timing and the whole base suite classified by lane with pinned counts

## Phase Details

### Phase 1: Governance, Rights and Harness Foundation
**Goal**: Before any emulator code exists, the maintainer can run one gate command that enforces the provenance, rights and determinism rules, plus a runner skeleton that classifies every outcome correctly
**Depends on**: Nothing (first phase)
**Requirements**: GOV-01, GOV-02, GOV-03, GOV-04, GOV-05, GOV-06, GOV-07, RUN-02, RUN-04, PROF-07, VER-04
**Decision gates**: D1 is recorded here as an ADR (default C17, CMake 3.25+, Ninja) and accepted before any core code merges. D4: the allowlist is recorded as written, and out-of-list code is not copied. The rule for reading GPL and MAME-licensed sources is ratified here. CI cost and gating are rechecked for a public repository
**Research**: Standard patterns
**Success Criteria** (what must be TRUE):
  1. The maintainer can read accepted ADRs for language/build/ABI (with the evidence that would reverse the choice), CPU timing-tier numbering, the emulator-extension-opcode policy, wall-clock timeout classification and the determinism-lint mechanism. The copy-provenance policy (allowlist, the never-copy GPL/Angrylion-Plus rule, the reading rule, per-copy records) and the claims/evidence-tier template are also written down
  2. The maintainer runs the single gate command locally, and CI runs that same command verbatim on explicitly labeled runners with SHA-pinned actions. The first CI run has a recorded critical-path review
  3. The runner skeleton classifies synthetic cases into exactly one of pass, guest-fail, boot-fail, timeout, missing-fixture and harness-error. In CI, a wrong expected value fails; missing, malformed, timed-out and crashed runs never pass; and a changed test inventory is detected
  4. The gate goes red on each seeded violation: a core source that reads host time, files, threads, entropy or a mutable global (caught by both the source lint and the link-level symbol allowlist); a protected or unmanifested binary in the tree or in a CI artifact; and a fixture-manifest entry missing source, revision, recipe, digest, profile, rights, notices or expected-result provenance
**Plans**: 13/13 plans complete in 5 waves

Plans:
- [x] 01-01-PLAN.md — Gate, CMake/presets, the two CONTEXT A-1 core stub paths, tools/common (SHA-256, UTF-8, canonical writer) with vector tests (wave 1)
- [x] 01-02-PLAN.md — Synthetic runner + supervisor, two golden controls, pure precedence classifier with full unit table, result-protocol spec (wave 2)
- [x] 01-03-PLAN.md — Two-layer determinism lint (PROF-07) wired into the gate, 19-seed corpus, rules.txt coverage gate (wave 2)
- [x] 01-04-PLAN.md — ADRs 0000-0007 (0002 proposed), copy-provenance and rights-and-fixtures policies, exact-path core rule, hardened ci.yml + checker (wave 2)
- [x] 01-05-PLAN.md — Strict JSON parser, canonical round-trip tool, fuzz + ASan preset, shared seeded-case driver (wave 2)
- [x] 01-06-PLAN.md — GOV-07 rights scanner (tree, full history, staging) and PII/private-path scan with seeded temp-repo controls (wave 2)
- [x] 01-07-PLAN.md — t64-result-check (result/suite/seal/identity) with seeded result and suite corpora (wave 3)
- [x] 01-08-PLAN.md — Pre-registered Rust spike; ADR-0002 accepted only on "stay with C17", switch escalates to owner (wave 3)
- [x] 01-09-PLAN.md — Fixture manifest + schema-subset validator, per-field seeds, single CI=true block, rights allowlist feed (wave 3)
- [x] 01-10-PLAN.md — Per-copy provenance records (MADR adoption), LICENSES/MIT.txt, provenance checker with temp-tree selftest (wave 3)
- [x] 01-11-PLAN.md — Full 19-case negative-control matrix, twice-run byte compare, pinned INVENTORY, live suite controls (wave 4)
- [x] 01-12-PLAN.md — Claims template + claims gate refusing synthetic evidence, seeded claim corpus (wave 4)
- [x] 01-13-PLAN.md — Pre-push checks, first hosted CI run for the pushed SHA, measured critical-path review, fork-PR approval tightened (wave 5)

### Phase 2: Deterministic Machine Skeleton
**Goal**: A machine instance with a named profile advances in guest time over the memory map, RDRAM, MI, PI and SP DMA, and produces the same state digest whether it runs alone or interleaved with another instance
**Depends on**: Phase 1
**Requirements**: CORE-01, CORE-03, CORE-06, PROF-01, PROF-02, PROF-03, PROF-04, PROF-06, BUS-01, BUS-02, BUS-04
**Partial contribution**: CORE-02 scheduler foundation and profile quantum are verified here; completion requires the later devices, with final integration owned by Phase 9.
**Decision gates**: None beyond D1 (accepted in Phase 1)
**Research**: Standard patterns for time, profile, outputs and bus. DMA length/alignment and MI semantics cite references, and each citation records its evidence tier
**Success Criteria** (what must be TRUE):
  1. The user selects the named first profile (NTSC, cartridge-only, 4 MB RDRAM, open-IPL3 boot route, CPU-RSP sync quantum included in its identity) and sees the profile hash and build identity in every result. The profile-limits document lists unsupported, deferred and unknown features, each with the evidence that would promote it
  2. A synthetic workload advanced to a guest-time, instruction-count or frame-event bound stops exactly at that bound and reports why. Two instances interleaved in one process produce digests equal to their isolated runs
  3. Component tests show PI and SP DMA completing as scheduled guest-time events under the documented length and alignment rules, never as an instantaneous copy. They also show MI separating masked interrupts from pending ones. Each expectation cites its source
  4. ROM admission refuses oversized, truncated and malformed images, and images of unknown byte order, and the state digest is unchanged after each refusal. A ROM whose header country byte disagrees with the selected profile still runs under the profile's TV type and region (two-profile diff test)
  5. Turning on tracing produces guest-time-stamped events at device boundaries, and the final digest is identical to an untraced run
  6. The ROM admission parser is fuzzed under ASan and UBSan with inputs that exercise every header-field-dependent read, and the fuzz target is the production parser, not a stub (ADR-0002, "Evidence limits": the Phase 1 spike parser read four bytes, so its fuzz established nothing about memory safety)
**Plans**: 8/8 plans complete

Plans:
**Wave 1**
- [x] 02-01-PLAN.md — Owned instance tracer, linted hash primitive and explicit profile identity

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 02-02-PLAN.md — Transactional production ROM normalization and allocator refusal contracts

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 02-03-PLAN.md — Exact guest-time scheduler, canonical digest and observational trace

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 02-04-PLAN.md — Checked physical memory map and separate MI pending/masks

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 02-05-PLAN.md — Scheduled PI DMA, cancellation and exact register policy

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 02-06-PLAN.md — Checked SP row DMA, active/pending queue and source visibility

**Wave 7** *(blocked on Wave 6 completion)*
- [x] 02-07-PLAN.md — Production sanitizer campaign and strict separate native model-result contract

**Wave 8** *(blocked on Wave 7 completion)*
- [x] 02-08-PLAN.md — Checked host probe, cross-device determinism, exact inventory gate and final evidence

### Phase 3: First Headless ROM Through Open IPL3
**Goal**: A hello ROM boots through HLE-PIF seeding and a pinned open IPL3 with no Nintendo boot bytes, prints through ISViewer, and the headless runner reports a pass with identical digests across platforms
**Depends on**: Phase 2
**Requirements**: BOOT-01, BOOT-02, BOOT-03, BOOT-04, RUN-01, RUN-03, RUN-05, PROF-05
**Decision gates**: D3 (IPL3 variant/revision pin and named maintenance owner) must be resolved before BOOT-02 is satisfied. **First task: primary-document verification.** Pin mirrored copies of the NEC VR4300 user's manual and the Nintendo documents cited by the bootstrap pack by SHA-256 in an evidence index. The documents are rights-bearing, so record the source and digest and do not commit the files
**Research**: DEEP. Open questions: the post-IPL2 state IPL3 expects; libdragon `boot/` per-file licensing; IPL3 entropy sources; ISViewer length-register semantics (0x14 vs libdragon's 0x16 byte count); the IPL3 variant (prod/compat) and revision (r7 vs r8)
**Success Criteria** (what must be TRUE):
  1. The evidence index records SHA-256 digests and sources for the VR4300 manual and the cited Nintendo documents. Every boot-related hardware claim used in this phase cites one of them or a hardware-derived test
  2. The user runs the project-authored (or rights-reviewed) hello ROM through the headless runner with a fixed reset, the named profile, a timed-input script and a guest-time limit, and gets a machine-readable pass. Its ISViewer log matches the expected text, the ISViewer length and buffer semantics are written down, and the input events appear in the trace at their scheduled guest ticks
  3. The pinned IPL3 has a recorded sha256, md5, source revision, license and named maintenance owner. The gate confirms that no Nintendo PIF, IPL or CIC bytes exist in the repository or in CI artifacts
  4. A guest that produces no output is classified boot-fail, never pass. The boot-limits document lists unmodeled PIF challenge/response, CIC variants and timeout behavior as deferred, so the project never reports "boot solved"
  5. The hello-ROM run produces identical state digests on two operating systems and with two compilers, shown by a CI matrix
  6. The headless runner's fixture digest attests the bytes the runner actually reads: real-ROM runs pass an already-open verified descriptor or a private copy, so the supervisor's digest cannot differ from what the guest loads (closes the Phase 1 review finding WR-06; see docs/spec/result-protocol.md, "Known limitation")
**Plans**: 13/13 plans complete

Plans:
- [x] 03-01-PLAN.md — Qualify and execute the actual warm-entry tracer.
- [x] 03-02-PLAN.md — Build the authored flat hello from qualified tools.
- [x] 03-03-PLAN.md — Register warm-entry evidence and coherent gate provisioning.
- [x] 03-04-PLAN.md — Expand the qualified CPU integer, CP0 and cache path.
- [x] 03-05-PLAN.md — Execute bounded warm devices, probes and delayed loader DMA.
- [x] 03-06-PLAN.md — Capture exact bounded ISViewer guest bytes.
- [x] 03-07-PLAN.md — Copy owned output and submit deterministic timed input.
- [x] 03-08-PLAN.md — Parse canonical timed-input schedules and prove their trace.
- [x] 03-09-PLAN.md — Attest immutable fixture and schedule descriptors.
- [x] 03-10-PLAN.md — Define the strict real-result v2 checker contract.
- [x] 03-11-PLAN.md — Run and supervise actual IPL3-to-hello execution.
- [x] 03-12-PLAN.md — Prove real failure controls and guest determinism.
- [x] 03-13-PLAN.md — Collect exact-SHA hosted evidence and verify current sources.

### Phase 4: Complete VR4300 CPU
**Goal**: The VR4300 interpreter implements MIPS III integer, CP0, TLB, cache-scope and soft-float COP1 behavior. Native tests with independent expected values verify it, and the `nemu64-test` CPU groups report per-group status
**Depends on**: Phase 3
**Requirements**: CPU-01, CPU-02, CPU-03, CPU-04, CPU-05, CPU-06, CPU-07, CPU-09, VER-01, VER-03
**Decision gates**: **Entry gate: finish the CPU-area primary-document check** against the manual pinned in Phase 3. That covers TLB/XTLB vectors, CP0 write masks, CACHE operations, Count rate, FCR31 and unimplemented operations, LL/SC link rules and delay-slot exceptions. Each must move from community or inference to a citation or test before the phase plan is final. D2: `nemu64-test` runs local-only until its rights are documented. D4: copy Berkeley SoftFloat 3e only after an exact license check and a provenance record; otherwise write a subset
**Research**: DEEP (no researcher has read the VR4300 manual; stepping/revision as a profile field)
**Success Criteria** (what must be TRUE):
  1. Native component tests pass for 64-bit operations with 32-bit sign extension; branches, branch-likely and delay slots, including exceptions in delay slots; CP0 registers and writable masks; TLB refill, invalid, modified and XTLB cases plus address errors; unaligned loads and stores; and LL/SC and LLD/SCD. Each test has edge and negative cases, and every expected value cites a primary document, a hardware-derived vector or the project's own derivation, never another emulator's output
  2. COP1 soft-float tests pass for single and double precision in all four rounding modes, including FCR31 cause and flag bits and unimplemented-operation exceptions. The lint shows that the canonical path uses no host floating point
  3. The maintainer can read the written cache-scope decision (which cache behavior is guest-visible), and the functional cache-model tests pass
  4. The maintainer builds `nemu64-test` from its pinned commit, runs it headlessly and gets per-group status for the CPU groups, with inventories pinned by exact count; a missing or reduced test set fails. Until D2 is resolved this runs locally only, and no derived ROM or output is uploaded
**Plans**: 20/20 plans complete
**Wave 1**
- [x] 04-01-PLAN.md
- [x] 04-14-PLAN.md

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 04-02-PLAN.md

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 04-03-PLAN.md

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 04-04-PLAN.md

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 04-05-PLAN.md

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 04-06-PLAN.md

**Wave 7** *(blocked on Wave 6 completion)*
- [x] 04-07-PLAN.md

**Wave 8** *(blocked on Wave 7 completion)*
- [x] 04-08-PLAN.md

**Wave 9** *(blocked on Wave 8 completion)*
- [x] 04-09-PLAN.md

**Wave 10** *(blocked on Wave 9 completion)*
- [x] 04-10-PLAN.md

**Wave 11** *(blocked on Wave 10 completion)*
- [x] 04-11-PLAN.md

**Wave 12** *(blocked on Wave 11 completion)*
- [x] 04-17-PLAN.md

**Wave 13** *(blocked on Wave 12 completion)*
- [x] 04-12-PLAN.md

**Wave 14** *(blocked on Wave 13 completion)*
- [x] 04-13-PLAN.md

**Wave 15** *(blocked on Wave 14 completion)*
- [x] 04-15-PLAN.md

**Wave 16** *(blocked on Wave 15 completion)*
- [x] 04-18-PLAN.md

**Wave 17** *(blocked on Wave 16 completion)*
- [x] 04-19-PLAN.md — approved separate software-compatibility profile and gate binding
- [x] 04-20-PLAN.md — resolve observed compatibility qualification gaps in isolated v2
- [x] 04-16-PLAN.md

### Phase 5: RSP Low-Level Emulation
**Goal**: The RSP runs real microcode on its scalar and vector units from IMEM/DMEM, with DMA and status semantics, checked against a permanent scalar VU reference and the `nemu64-test` RSP groups
**Depends on**: Phase 4
**Requirements**: RSP-01, RSP-02, RSP-03, RSP-04, RSP-05
**Decision gates**: **Entry gate: RSP-area evidence check.** DMA copy timing, dual-issue and stall rules, DMEM/IMEM contention, halt corruption, and VMOV and clamp details are checked against test vectors or primary sources rather than wiki prose. D2 applies to the RSP groups (local-only)
**Research**: DEEP
**Success Criteria** (what must be TRUE):
  1. ROM-level and native tests show scalar and vector instructions executing from IMEM/DMEM, with SP_STATUS, semaphore, halt, break and SP_PC behavior matching cited expectations
  2. Per-opcode fuzzing finds zero differences between any optimized VU path and the scalar-portable reference. Long stress runs (vmulf through vrsq32) run in a scheduled lane, outside the required gate
  3. RSP DMA tests for alignment, bank wrap, stride and queue behavior pass
  4. The `nemu64-test` RSP groups pass or are classified individually with reasons (local-only until D2), and an automated check confirms the canonical core contains no HLE microcode path
**Plans**: 6/10 plans executed
**Wave 1**
- [x] 05-01-PLAN.md

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 05-02-PLAN.md

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 05-03-PLAN.md

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 05-04-PLAN.md

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 05-05-PLAN.md

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 05-06-PLAN.md

**Wave 7** *(blocked on Wave 6 completion)*
- [ ] 05-07-PLAN.md

**Wave 8** *(blocked on Wave 7 completion)*
- [ ] 05-08-PLAN.md

**Wave 9** *(blocked on Wave 8 completion)*
- [ ] 05-09-PLAN.md

**Wave 10** *(blocked on Wave 9 completion)*
- [ ] 05-10-PLAN.md

### Phase 6: Cold Boot, CPU Timing and Full Base Suite
**Goal**: The machine reaches IPL3 through the full RDRAM register protocol with instruction-granular CPU timing, and the complete `nemu64-test` base suite is classified per group, by lane, with pinned counts
**Depends on**: Phase 4, Phase 5
**Requirements**: BUS-03, CPU-08, VER-02
**Decision gates**: D2: lanes are defined and run locally. Public CI lanes that use `nemu64-test` turn on only after D2 is resolved. The project-authored hello ROM stays the permanent public smoke lane. D6 applies to any milestone-close release
**Research**: MODERATE (RDRAM calibration values; behavior between 4 and 8 MiB without an Expansion Pak)
**Success Criteria** (what must be TRUE):
  1. The hello ROM boots both with the RDRAM tier A preset and through the tier B register protocol (delay, device ID, broadcast, MI repeat mode, calibration read-back), and both runs produce the same ISViewer output
  2. Count and Random advance under the tier 1 instruction-granular cost model, and the tests cite their expectation sources. The cycle-accurate tier is documented as deferred, with the evidence needed to promote it
  3. The full base suite reports per-group status with its total count pinned. The timing, cycle and cop0hazard groups are reported as informational, experimental groups stay out of the required gate, and stress groups run in a scheduled lane
  4. The milestone's support statement for the first profile uses the claims template: profile, build identity, evidence tier and fixture-manifest entry for every reported result. It makes no claim beyond those results
**Plans**: TBD

## v1.1 Observable Machine

**Milestone Goal (provisional):** The machine produces raw video, raw audio and controller-driven
behavior, with save media and a replayable state, each verified by authored diagnostics with exact
expectations. This outline is revised at the v1.0 close.

- [ ] **Phase 7: RDP Reference Renderer and VI Scanout** - In-tree software RDP and VI produce raw frames, and conformance is reported as labeled differential evidence
- [ ] **Phase 8: Audio Interface and Raw Capture** - AI DMA and cadence produce raw audio with exact-sample diagnostics; raw video and audio are captured as files
- [ ] **Phase 9: Controller, Save Media and Replay** - Standard controller over SI/PIF, explicit EEPROM/SRAM devices, versioned state export and input replay

### Phase 7: RDP Reference Renderer and VI Scanout
**Goal**: Command lists render into RDRAM through an in-tree software RDP, and the VI produces raw frames under the profile's TV timing. Conformance is reported as labeled differential evidence, never as hardware truth
**Depends on**: Phase 4 (CPU-built command lists). Can run in parallel with Phases 5 and 6
**Requirements**: VID-01, VID-02, VID-03, VID-04, VID-05, VID-06
**Decision gates**: Entry gate: RDP/VI evidence check. D5 applies to any Angrylion-Plus-derived claim. The parallel-rdp suites need a per-file license audit before use
**Research**: DEEP (command timing, TMEM and coverage, SYNC_FULL ordering, VI filtering, video-domain clocks)
**Success Criteria** (what must be TRUE):
  1. Authored diagnostic ROMs that exercise fill, copy, textured and shaded triangles, combiner, blender, coverage and Z produce raw frames recorded as regression pins, and SYNC_FULL raises the DP interrupt per the cited references
  2. Each raw frame carries the profile's resolution, interlace, pixel format and filter state as metadata. Nothing assumes 320x240, and no host scaling is applied
  3. Conformance reports label each oracle (name, version, modifications) as differential evidence, and undefined-behavior choices (noise seed, undefined memory color) appear as named policies
**Plans**: TBD

### Phase 8: Audio Interface and Raw Capture
**Goal**: The AI turns guest DMA into raw audio buffers whose cadence comes only from guest time, and a headless run captures raw video and audio as files with metadata
**Depends on**: Phase 5 (real audio needs RSP microcode), Phase 7 (video capture)
**Requirements**: AUD-01, AUD-02, AUD-03, RUN-06
**Research**: MODERATE (DACRATE values, cadence phase, 8 KiB carry vectors)
**Success Criteria** (what must be TRUE):
  1. An authored audio diagnostic produces raw sample buffers whose exact values match expectations across DMA boundaries, and tests cover the two-deep queue and the IRQ raised at DMA start
  2. Audio cadence follows DACRATE and guest time, so runs throttled differently on the host produce identical sample streams
  3. A headless run writes raw video frames and raw audio buffers as files whose metadata covers sample index, rate, frame timing and profile/build identity, separate from host presentation
**Plans**: TBD

### Phase 9: Controller, Save Media and Replay
**Goal**: Timed logical input drives the standard controller, explicitly selected save devices behave with defined refusal postconditions, and machine state and input replays reproduce runs exactly
**Depends on**: Phase 8 (state export covers every device in the profile)
**Requirements**: SAVE-01, SAVE-02, SAVE-03, SAVE-04, CORE-02, CORE-04, CORE-05
**Research**: MODERATE (joybus response timing, EEPROM write timing; FlashRAM deferred)
**Success Criteria** (what must be TRUE):
  1. A timed-input script drives the standard controller over SI/PIF joybus, and an authored ROM reports the expected button states at the expected guest times
  2. EEPROM (4 and 16 Kbit, with the busy window and block rules) and SRAM work only when the profile selects them explicitly. Unknown device requests are refused with defined postconditions. Save bytes round-trip through import and export with endianness handling, and a refused import leaves the digest unchanged
  3. Exporting and re-importing machine state reproduces the canonical digest exactly, and a recorded input replay bound to profile, build and content hash reproduces the original run's digests
  4. CPU, RSP, RDP, VI, AI, PI, SI and MI activity is ordered by the same guest-time scheduler, including coincident-event and partition/interleave checks; the CPU-to-RSP quantum remains part of profile identity (CORE-02 completion).
**Plans**: TBD

## v1.2 Embeddable and Playable

**Milestone Goal (provisional):** A frozen C-ABI, proven through an independent installed consumer,
and a rights-cleared homebrew that plays in a minimal host, so Tendies64 does not stay tests-only.
This outline is revised at the v1.1 close.

- [ ] **Phase 10: Frozen API and Installed Consumer** - C-ABI postconditions exercised through a clean installed package before the freeze
- [ ] **Phase 11: Minimal Host and Playable Homebrew** - Optional SDL3 host, a rights-cleared playable title and a LibFuturo fit evaluation

### Phase 10: Frozen API and Installed Consumer
**Goal**: An independent consumer, using only a clean installed package, exercises every lifetime, admission, advance, output and failure postcondition, and only then is the C-ABI frozen
**Depends on**: Phase 9
**Requirements**: API-01, API-02, API-03, API-04, VER-05
**Research**: MODERATE
**Success Criteria** (what must be TRUE):
  1. A consumer built against a relocated, clean installed package (no source tree visible) runs the same permitted headless fixture and gets the same result as the in-tree runner. Negative controls, such as a missing package or a mismatched version, fail
  2. Each error class has tested postconditions through the installed consumer: refused leaves state bit-identical, guest-stop is resumable, and a defect poisons the instance
  3. The C-ABI (lifetime, profile selection, transactional admission, timed input, bounded advance, output rings with backpressure, reset, unload) is frozen only after those tests pass, with the host supplying all files, clocks, entropy and threads
**Plans**: TBD

### Phase 11: Minimal Host and Playable Homebrew
**Goal**: A rights-cleared homebrew title runs headlessly and plays in a minimal host window with a physical controller, without the core depending on the host
**Depends on**: Phase 10
**Requirements**: API-05, API-06, API-07
**Decision gates**: D7 (no branding in the host or packaging). D6 applies to any release. D4 covers SDL3, which is zlib-licensed and link-only. Pin the route B libdragon toolchain and record its GCC/newlib notices
**Research**: DEEP (playable toolchain route; LibFuturo contract recheck)
**Success Criteria** (what must be TRUE):
  1. A rights-cleared homebrew title (authored here or built from a pinned libdragon toolchain, with its manifest entry) produces the expected guest output headlessly and plays in the minimal host with a physical controller
  2. The minimal host (an optional SDL3 target) presents raw video and audio, and the core library builds and links with no SDL dependency
  3. A written LibFuturo fit evaluation against its then-current contract exists. Any adapter is optional and leaves the core API unchanged
**Plans**: TBD
**UI hint**: yes

## v2.0 Breadth, Accuracy and Performance

**Milestone Goal (outline):** Broaden accurate N64 coverage beyond the first profile, and make it
fast without changing results. Phases are numbered when this milestone opens. Each item is promoted
by the evidence named in `REQUIREMENTS.md` (v2), never by schedule.

- **Performance:** named benchmark workloads with output held constant, throughput and latency
  reported separately from CI time (PERF-01), then a cached interpreter and RSP decode cache
  (PERF-02), and a block JIT last with lockstep digest equality (PERF-03)
- **Accuracy:** cycle-accurate CPU timing and CP0 hazards (ACC-01); hardware-derived public
  vectors where they strengthen a material claim (ACC-02)
- **Expansion profiles:** Expansion Pak 8 MB (EXP-01), PAL and MPAL (EXP-02), other CICs and the
  PIF challenge model (EXP-03), FlashRAM (EXP-04), Controller/Rumble/Transfer Pak (EXP-05),
  RTC (EXP-06), and 64DD as a separate system profile once a rights-cleared disk route exists
  (EXP-07)
- **Player features:** save-state UI, rewind and fast-forward (PLAY-01); an optional GPU renderer
  as host presentation (PLAY-02); a GDB stub over the trace seam (PLAY-03)
- **Compatibility claims:** commercial-title behavior is claimed only per named profile with
  evidence tiers, never from boots, screenshots or agreement with another emulator

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6, then 7 → 8 → 9 → 10 → 11.
Phase 7 may run in parallel with Phases 5 and 6 once Phase 4 is complete.

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Governance, Rights and Harness Foundation | v1.0 | 13/13 | Complete | 2026-11-10 |
| 2. Deterministic Machine Skeleton | v1.0 | 8/8 | Complete | 2026-11-10 |
| 3. First Headless ROM Through Open IPL3 | v1.0 | 13/13 | Complete | 2026-11-11 |
| 4. Complete VR4300 CPU | v1.0 | 20/20 | Complete | 2026-11-13 |
| 5. RSP Low-Level Emulation | v1.0 | 6/10 | In Progress | - |
| 6. Cold Boot, CPU Timing and Full Base Suite | v1.0 | 0/TBD | Not started | - |
| 7. RDP Reference Renderer and VI Scanout | v1.1 | 0/TBD | Not started | - |
| 8. Audio Interface and Raw Capture | v1.1 | 0/TBD | Not started | - |
| 9. Controller, Save Media and Replay | v1.1 | 0/TBD | Not started | - |
| 10. Frozen API and Installed Consumer | v1.2 | 0/TBD | Not started | - |
| 11. Minimal Host and Playable Homebrew | v1.2 | 0/TBD | Not started | - |
