# Roadmap: Tendies64

## Milestones

- ✅ **v1.0 Verified Headless Core** — Phases 1–6 complete; planning archive 2026-11-15. [Verified scope and history](milestones/v1.0-ROADMAP.md). This is not a software release.
- ✅ **v1.1 Observable Machine** — Phases 7–9 complete 2026-11-20 (VID-04 descoped to v1.2 by owner decision). [Archive](milestones/v1.1-ROADMAP.md), [requirements](milestones/v1.1-REQUIREMENTS.md), [audit](milestones/v1.1-MILESTONE-AUDIT.md), [phase records](milestones/v1.1-phases/). This is not a software release.
- 📋 **v1.2 Embeddable and Playable** — Phases 10–11; provisional.
- 📋 **v2.0 Breadth, Accuracy and Performance** — provisional future scope.

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

## Phases

<details>
<summary>✅ v1.1 Observable Machine (Phases 7–9) — COMPLETE 2026-11-20</summary>

- [x] Phase 7: RDP Reference Renderer and VI Scanout (12/12 milestone plans) — completed 2026-11-20; Plan 07-13 and VID-04 deferred to v1.2
- [x] Phase 8: Audio Interface and Raw Capture (5/5 plans) — completed 2026-11-19
- [x] Phase 9: Controller, Save Media and Replay (13/13 plans) — completed 2026-11-20

</details>

## v1.2 Embeddable and Playable

**Milestone Goal (provisional):** A frozen C-ABI, proven through an independent installed consumer,
and a rights-cleared homebrew that plays in a minimal host, so Tendies64 does not stay tests-only.
Carried in from v1.1: VID-04 (differential oracle evidence, only after the owner's D5 decision and the parallel-rdp rights audit), the Phase 7 test-strength debt (VG-01..VG-05), RDP depth on the composite profiles, a composite end-to-end fixture, and the hosted macOS gate-time and requalification follow-ups (docs/ci-reviews/LOG.md F-106, F-111, F-114). This outline is revised when v1.2 opens.

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
