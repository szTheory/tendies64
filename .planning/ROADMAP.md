# Roadmap: Tendies64

## Milestones

- ✅ **v1.0 Verified Headless Core** — Phases1–6 complete; planning archive2026-11-15. [Verified scope and history](milestones/v1.0-ROADMAP.md). This is not a software release.
- 🚧 **v1.1 Observable Machine** — Phases 7–9; active; Phase7 authored qualification complete, oracle acceptance blocked.
- 📋 **v1.2 Embeddable and Playable** — Phases10–11; provisional.
- 📋 **v2.0 Breadth, Accuracy and Performance** — provisional future scope.

The completed milestone covers 48 assigned requirements. Of the 25 remaining
requirements in the [requirements archive](milestones/v1.0-REQUIREMENTS.md),
17 are active in [v1.1 requirements](REQUIREMENTS.md) and eight remain deferred
to v1.2. CORE-02 retains its partial scheduler foundation; no remaining
requirement is marked complete by archival or milestone activation. Full historical plans, evidence and limitations
remain in [v1.0 phase records](milestones/v1.0-phases/).

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

## v1.1 Observable Machine

**Milestone Goal:** The machine produces raw video, raw audio and controller-driven
behavior, with explicit save devices, versioned machine state and reproducible input replay.
Authored diagnostics establish exact software expectations; required RDP differential evidence
remains separate and is blocked by its specific authority and asset gates.

Scope and sequence follow the [v1.1 research synthesis](research/SUMMARY.md).
Apply the approved [compatibility policy](../docs/spec/cpu-compatibility.md): preserve
existing identities and bytes where semantics are unchanged, and version only concrete
semantic changes. This milestone adds no public API/ABI freeze, installed consumer,
frontend, broad compatibility claim or software release.

## Phases

- [ ] **Phase 7: RDP Reference Renderer and VI Scanout** - In-tree software RDP and VI produce raw frames, and conformance is reported as labeled differential evidence
- [x] **Phase 8: Audio Interface and Raw Capture** - AI DMA and cadence produce raw audio with exact-sample diagnostics; raw video and audio are captured as files (completed 2026-11-19)
- [ ] **Phase 9: Controller, Save Media and Replay** - Standard controller over SI/PIF, explicit EEPROM/SRAM devices, versioned state export and input replay

## Phase Details

### Phase 7: RDP Reference Renderer and VI Scanout
**Goal**: Command lists render into RDRAM through an in-tree software RDP, and the VI produces raw frames under the profile's TV timing. Conformance is reported as labeled differential evidence, never as hardware truth
**Depends on**: Phase 6 (completed v1.0 machine; CPU command-list support from Phase 4 and fetched RSP from Phase 5)
**Requirements**: VID-01, VID-02, VID-03, VID-04, VID-05, VID-06
**Decision gates**: RDP/VI evidence check at discussion/planning. VID-04 acceptance remains blocked by unresolved D5 oracle authority and the parallel-rdp per-file/asset rights audit. No oracle execution/use/copying or derived-data publication before applicable gates resolve. Permitted authored/native implementation can continue; authored expectations and regression pins cannot replace labeled differential evidence
**Research**: DEEP phase-level checks (command timing, TMEM and coverage, SYNC_FULL ordering, VI filtering, video-domain clocks); reuse existing milestone research
**Success Criteria** (what must be TRUE):
  1. Before the full suite, a cleared authored DP fill command list writes independently calculated pixels to RDRAM, VI scans them into a raw frame, and the actual headless runner retrieves that frame. RDRAM pixels and VI output each match their independent expectations, and a deliberately wrong expected pixel fails
  2. Cleared authored diagnostics for the remaining required copy, textured and shaded triangle, combiner, blender, coverage and Z modes produce independently checked raw output; SYNC_FULL raises the DP interrupt per cited references. Regression pins retain the exact fixture/profile/build identity and do not imply hardware accuracy
  3. Raw frames follow the selected profile's TV timing, interlace, pixel format and filtering and carry resolution and guest-time metadata. Output does not assume 320×240; host scaling and presentation remain separate
  4. Noise seed and undefined memory color are visible as named deterministic policies bound to the affected profile behavior
  5. Once its gates resolve, VID-04 has conformance reports labeling each out-of-tree oracle's name, version and modifications as differential evidence, never hardware truth. Until then it remains explicitly blocked and incomplete
**Plans**: 12/13 plans executed (Plan12 preserves the existing authority stop)

Plans:
**Wave 1**
- [x] 07-01-PLAN.md — Production fill-to-frame tracer through the actual runner

**Wave 2** *(complete)*
- [x] 07-02-PLAN.md — Incremental DP command transport, RSP dispatch and completion fences

**Wave 3** *(complete at authored/native tier)*
- [x] 07-03-PLAN.md — TMEM loads and independently checked copy-mode pixels

**Wave 4** *(ready under owner-approved D-11 software numeric policy)*
- [x] 07-04-PLAN.md — Source-reconciled triangle edges, coverage generation and shading

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 07-05-PLAN.md — Textured triangles with independently derived interpolation and sampling

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 07-06-PLAN.md — One-cycle and two-cycle combiner with named undefined policies

**Wave 7** *(blocked on Wave 6 completion)*
- [x] 07-07-PLAN.md — Hidden coverage ownership and blender render modes

**Wave 8** *(blocked on Wave 7 completion)*
- [x] 07-08-PLAN.md — Depth mapping, compare/update and Z-enabled triangle paths

**Wave 9** *(blocked on Wave 8 completion)*
- [x] 07-09-PLAN.md — Profile-timed VI register geometry, fields and interrupts

**Wave 10** *(blocked on Wave 9 completion)*
- [x] 07-10-PLAN.md — VI filtering and lossless raw-output backpressure

**Wave 11** *(complete)*
- [x] 07-11-PLAN.md — Complete authored native, sanitizer, compiler and hosted qualification

**Wave 12** *(authored preparation complete; existing owner/legal stop preserved)*
- [x] 07-12-PLAN.md — Prepare a concrete oracle authority record and preserve the existing stop

**Wave 13** *(blocked until genuine oracle authority and applicable rights pass the live gate)*
- [ ] 07-13-PLAN.md — Run authorized out-of-tree comparisons and label actual differential evidence

### Phase 8: Audio Interface and Raw Capture
**Goal**: The AI turns guest DMA into raw audio buffers whose cadence comes only from guest time, and a headless run captures raw video and audio as files with metadata
**Depends on**: Qualified authored guest-timed raw output/video foundation from Phase7 Plans01–11; Phase5 is complete. Owner approved this dependency change on2026-11-18. Phase7/VID-04 remain incomplete and their oracle authority/rights stop persists. Authored PCM verifies AI transport without requiring broader RSP audio-microcode compatibility
**Requirements**: AUD-01, AUD-02, AUD-03, RUN-06
**Research**: MODERATE (DACRATE values, cadence phase, 8 KiB carry vectors)
**Success Criteria** (what must be TRUE):
  1. A cleared authored audio diagnostic produces raw sample buffers with sample index/rate metadata and exact expected values across DMA boundaries, including two-deep queue behavior and the interrupt at the documented `ai_full` 1→0 queue transition. A submission or DMA start alone does not imply a new interrupt
  2. Audio cadence follows DACRATE and guest time, so runs throttled differently on the host produce identical sample streams
  3. A headless run writes raw video frames and raw audio buffers as files whose metadata covers sample index, rate, frame timing and profile/build identity, separate from host presentation
**Plans**: 5/5 plans complete
**Wave 1**
- [x] 08-01-PLAN.md — Fetched PCM tracer and composite inheritance

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 08-02-PLAN.md — Registers, DMA/MI and rational cadence

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 08-03-PLAN.md — Lossless observation and determinism

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 08-04-PLAN.md — General raw A/V capture and no-clobber transaction

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 08-05-PLAN.md — Evidence controls and final qualification

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
**Plans**: 9/13 plans executed (strictly sequential; declared waves 1-13)
**Wave 1**
- [x] 09-01-PLAN.md — Core identity, v25 digest walker, membership sweep and scheduler ledger

**Wave 2**
- [x] 09-02-PLAN.md — Runner harness: per-fixture tick cap, transcript pins, artifact publisher and FSIZE pair

**Wave 3**
- [x] 09-03-PLAN.md — SI/PIF/joybus core, strictly-after input latch and pad-poll tracer

**Wave 4**
- [x] 09-03.1-PLAN.md — Collector, CI exact-match changes, first hosted pair and the FH freeze

**Wave 5**
- [x] 09-04-PLAN.md — EEPROM 4/16 Kbit and SRAM devices with explicit profile selection

**Wave 6**
- [x] 09-05-PLAN.md — Save import/export, transforms and refusal postconditions

**Wave 7**
- [x] 09-06-PLAN.md — CORE-02 ordering matrix, mutant procedure and the state-content freeze

**Wave 8**
- [x] 09-07-PLAN.md — Input replay bound to profile, build and content

**Wave 9**
- [x] 09-08-PLAN.md — Versioned machine state export and container

**Wave 10**
- [ ] 09-09-PLAN.md — State import (validate-then-apply) and exact round trip

**Wave 11**
- [ ] 09-09.1-PLAN.md — State boundary matrix, mutants, fuzz and field-use audit

**Wave 12**
- [ ] 09-10-PLAN.md — Independent review, security audit, goal verification and one fix cycle

**Wave 13**
- [ ] 09-11-PLAN.md — Identity freeze and exact-SHA native and hosted qualification

## Progress

Execution order: Phase 7 → Phase 8 → Phase 9. Plans01–12 are complete within their authored scope; the prepared checker preserves the existing authority stop. Owner approved beginning Phase8 from the qualified authored foundation; its discussion/research is active. Oracle use and VID-04 remain blocked; Phase7 is not complete.
VID-04's specific acceptance gate does not prevent permitted authored/native work.

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 7. RDP Reference Renderer and VI Scanout | 12/13 | Blocked: oracle authority | - |
| 8. Audio Interface and Raw Capture | 5/5 | Complete    | 2026-11-19 |
| 9. Controller, Save Media and Replay | 9/13 | In Progress, ready to execute | - |

## v1.2 Embeddable and Playable

**Milestone Goal (provisional):** A frozen C-ABI, proven through an independent installed consumer,
and a rights-cleared homebrew that plays in a minimal host, so Tendies64 does not stay tests-only.
This outline is revised at the v1.1 close.

- **Provisional Phase 10: Frozen API and Installed Consumer** - C-ABI postconditions exercised through a clean installed package before the freeze
- **Provisional Phase 11: Minimal Host and Playable Homebrew** - Optional SDL3 host, a rights-cleared playable title and a LibFuturo fit evaluation

### Provisional Phase 10: Frozen API and Installed Consumer
**Goal**: An independent consumer, using only a clean installed package, exercises every lifetime, admission, advance, output and failure postcondition, and only then is the C-ABI frozen
**Depends on**: Phase 9
**Requirements**: API-01, API-02, API-03, API-04, VER-05
**Research**: MODERATE
**Success Criteria** (what must be TRUE):
  1. A consumer built against a relocated, clean installed package (no source tree visible) runs the same permitted headless fixture and gets the same result as the in-tree runner. Negative controls, such as a missing package or a mismatched version, fail
  2. Each error class has tested postconditions through the installed consumer: refused leaves state bit-identical, guest-stop is resumable, and a defect poisons the instance
  3. The C-ABI (lifetime, profile selection, transactional admission, timed input, bounded advance, output rings with backpressure, reset, unload) is frozen only after those tests pass, with the host supplying all files, clocks, entropy and threads
**Plans**: TBD

### Provisional Phase 11: Minimal Host and Playable Homebrew
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
