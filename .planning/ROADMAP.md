# Roadmap: Tendies64

## Milestones

- ✅ **v1.0 Verified Headless Core** — Phases1–6 complete; planning archive2026-11-15. [Verified scope and history](milestones/v1.0-ROADMAP.md). This is not a software release.
- 📋 **v1.1 Observable Machine** — Phases7–9; next authorized milestone.
- 📋 **v1.2 Embeddable and Playable** — Phases10–11; provisional.
- 📋 **v2.0 Breadth, Accuracy and Performance** — provisional future scope.

The completed milestone covers48 assigned requirements. The25 remaining
requirements are preserved pending in the [requirements archive](milestones/v1.0-REQUIREMENTS.md),
for explicit scoping when the next milestone opens. No deferred requirement is
marked complete by archival. Full historical plans, evidence and limitations
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
