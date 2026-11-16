# Tendies64

## What This Is

Tendies64 is a Nintendo 64 emulator built around one canonical, deterministic
machine core. Each support claim is tied to an explicitly named machine, TV/region,
boot, RAM-expansion and peripheral profile, and to versioned, reproducible
evidence. The core has no dependency on host clocks, files, input mapping,
presentation or durable storage. Host integrations sit on top: a headless runner
first, then playable frontends and possibly adapters such as LibFuturo. It is for
the owner, for emulator and homebrew developers who need an observable and
scriptable N64, and eventually for players. "Tendies64" is the owner-selected
working name. It has not been cleared as a trademark.

## Core Value

Every Tendies64 support claim is backed by evidence: a named profile, a pinned
build and a reproducible, rights-cleared guest result. When tradeoffs arise,
correct and deterministic behavior that can be verified comes before speed,
breadth or polish.

## Requirements

### Validated

- ✓ Governance, provenance, rights/PII scans, deterministic-core lint and the
  six-outcome synthetic harness — Phase 1; source regressions remain in the gate.
- ✓ Named NTSC cartridge/4 MB machine skeleton, per-instance scheduler, checked
  physical map, MI, delayed PI/SP DMA, canonical digest and observational trace
  — Phase 2. The original skeleton retains its capability limits; later profiles
  below provide bounded guest boot.
- ✓ Transactional production ROM admission with authored-byte tests and bounded
  ASan/UBSan mutation coverage — Phase 2. This does not prove memory safety.
- ✓ Authored hello through pinned Unlicense IPL3, exact bounded ISViewer output,
  verified descriptor admission and timed-input trace — Phase 3. Actual
  Linux/GCC and macOS/Clang records agree for the named prepared-warm profile;
  this is smoke evidence, not cold-boot or hardware-accuracy qualification.

- ✓ Scoped VR4300 integer/CP0/MMU/cache/COP1 interpreter with independently
  expected native tests, preserved strict profile and separately versioned
  software-compatibility policies — Phase 4. Qualified local CPU reporting
  is available; this is not full accuracy or hardware qualification.

- ✓ Functional fetched RSP scalar/vector execution, SP control and queued DMA,
  a permanent portable VU reference, authored ROM/native/instrumented evidence
  and independently checked local classification — Phase 5. Exact artifacts,
  authored-only hosted checks and configured review closure are recorded;
  hardware accuracy, optimized differential execution and general microcode
  compatibility remain unestablished.

- ✓ Named NTSC cartridge/4 MiB profiles, timed logical input, bounded
  machine-readable headless outcomes, and a rights-cleared HLE-PIF/open-IPL3
  route — v1.0. Older profile identities and refusal behavior remain stable.
- ✓ Two separate A/B cold software profiles run the same authored hello through
  preset RDRAM or the modeled register protocol and produce identical literal
  output. Count/Random use an explicit T1 cost policy; T2 and physical accuracy
  remain unqualified — Phase 6.
- ✓ Full pinned source classification with required, informational and local
  stress lanes, independent membership/image authority, immutable producer
  archives and original-record validation — Phase 6. Classification completeness
  is distinct from execution completion and all-pass; private results stay local.
- ✓ Rolling milestone v1.0 has48 assigned requirements and six verified phases;
  later outlines retain25 future requirements. This is planning completion,
  not a software release or market-demand claim.

### Active

These are hypotheses until shipped and verified. They are scoped further in
`REQUIREMENTS.md`.

- [ ] Complete SI/PIF and later video/audio device integration through the
      canonical scheduler; current memory, MI, PI, SP and RDRAM paths retain
      their tested profile limits
- [ ] Broader RSP microcode compatibility and later audio/video task evidence,
      beyond the qualified functional profile's ROM-level tests
- [ ] Public external diagnostic CI only after D2 rights permission; keep
      qualified external diagnostics local and preserve the cleared authored
      hello as the permanent public smoke lane
- [ ] RDP command processing and VI scanout that produce raw framebuffer
      output, kept separate from host scaling and presentation
- [ ] An AI path (DMA, sample cadence, interrupts) that produces raw audio,
      kept separate from host playback
- [ ] A cartridge save-media baseline (EEPROM 4/16 Kbit, SRAM, then others)
      with explicit device selection and refused-operation postconditions
- [ ] Standard controller input over SI/PIF; accessories (Controller Pak,
      Rumble Pak, Transfer Pak) as separate, explicitly named profiles
- [ ] A public core API that defines instance lifetime, profile selection,
      content admission and refusal, timed input, bounded advancement, raw
      output buffers, reset/unload and failure postconditions before it is
      frozen
- [ ] An independent installed consumer test that runs the same permitted
      headless fixture against a clean installed package
- [ ] A legally cleared, playable homebrew path (an original or
      rights-reviewed source-built title) so the project does not become
      tests-only
- [ ] Named performance benchmarks, run only after a repeatable correctness
      baseline exists, with throughput and latency reported separately from CI
      time

### Out of Scope

Current boundaries. A boundary moves only when a named piece of evidence
promotes it.

- **Bundling or redistributing Nintendo PIF/IPL/CIC data, commercial ROMs,
  saves or 64DD media.** This is an owner rights stop. A hash proves identity,
  not rights.
- **64DD in the initial profile.** It is a separate later system profile
  (disk media, writable areas, RTC, internal ROM, required Expansion Pak). It
  is promoted after the cartridge machine is verified and a rights-cleared
  disk fixture route exists.
- **A promise to support every region, every game, every microcode or every
  accessory at first release.** The first profile is delivery order, not a
  ceiling. Each variant is promoted on evidence.
- **Commercial-game compatibility claims from boot screenshots, single test
  suites, output hashes or agreement with another emulator.** These do not
  establish broad compatibility or hardware accuracy.
- **A plugin architecture copied from Project64 or Mupen64Plus.** These are
  reference points only. Tendies64 keeps one canonical core.
- **Physical-console measurement with owner-held hardware.** This is an owner
  stop. Hardware-derived vectors from documented public sources may be used
  with full provenance.
- **Vendoring the Retro Regen commercial test suite.** Its terms forbid
  modification and rebranding.
- **Claiming a market gap or unmet demand.** The Switch Online study is
  context about play in one sampled ecosystem, not proof of demand.

## Context

### Owner direction (2026-11-09)

- Build a high-quality, accurate, deterministic and useful N64 emulator under
  the working name Tendies64. Do not reopen the name without materially new
  evidence.
- Work autonomously under `.planning/AUTONOMY.md`: rolling roadmap, fan-out
  discussion per phase, measured CI-efficiency reviews, and milestone
  releases from the exact tested artifact once license, visibility and
  release channel are decided.
- Prefer a small, flat dependency tree. "Another copy-paste is better than
  another dependency," provided the copied code's provenance and maintenance
  owner are clear.
- This project is part of a demand-aware, multi-console strategy. Sibling
  projects (for example Schwaystation for PS1) exist. Their hardware
  assumptions, blockers, licenses, fixtures and approvals do not transfer.

### Hardware facts (Nintendo manuals via archival mirrors; recheck before use as requirements)

- The CPU is a VR4300-family MIPS III at 93.75 MHz. The RCP runs at 62.5 MHz
  and contains the RSP (4 KB IMEM and 4 KB DMEM, running loaded microcode)
  and the RDP (rasterizer).
- Base RDRAM is 4 MB. The Expansion Pak adds 4 MB. The CPU and RCP run
  concurrently over shared memory.
- The VI scans out framebuffers from RDRAM with NTSC/PAL/MPAL timing,
  multiple resolutions, interlace and filtering. A 320×240 final framebuffer
  is not guaranteed.
- The AI DMAs RSP-generated samples to a stereo DAC, with its own timing and
  interrupts.
- PI handles cartridge access and DMA. SI/PIF handles controllers and
  port-device protocols. Save media (EEPROM 4/16 Kbit, SRAM, and others) are
  distinct devices, not one byte array.
- CIC/PIF/IPL3 affect boot. OS TV type and cartridge region are related but
  distinct properties.

### Verification leads (community and implementation sources; audit before adoption)

- `thelemmy/nemu64-test` is MIT-licensed. It covers CPU/COP0, LL/SC, 64-bit
  ops, exceptions, TLB, access widths and RSP, and writes results to
  ISViewer-style registers. Boot is via Libdragon's open IPL3. The license on
  the repository does not by itself clear the generated ROMs or embedded
  assets.
- `PeterLemon/N64` is Unlicense. It has broad CPU, RSP, RDP, RDRAM, VI,
  input, interrupt and sound tests, and its author reports testing on a
  64drive. Verify cases individually.
- `parallel-rdp-standalone` provides RDP/VI conformance suites compared
  against Angrylion-Plus. They are differential evidence, not physical truth.

### Ecosystem (observed 2026-11-09)

- ares v148 (2026-05-30) rewrote its N64 CPU/RSP JIT and keeps a versioned
  compatibility table. Project64 supports N64 and 64DD with plugins.
  Mupen64Plus splits the emulator into core, graphics, audio, input, RSP and
  frontend components. The ecosystem is mature.
- Tendies64's candidate value, to be proven rather than assumed:
  owner-controlled implementation; support claims bound to profiles and
  evidence tiers; a deterministic, observable, headless-first core suited to
  automated testing and developer tooling; and an independent integration
  route (a clean embeddable API, and possibly LibFuturo).
- LibFuturo is an experimental C API with synthetic, NES and 7800 profiles
  and no N64 profile. It is an integration candidate only. Recheck its
  current contract before any decision.

### Current state

Planning milestone v1.0 delivers a verified deterministic headless core in the
public Apache-2.0 repository: bounded ROM admission, CPU/CP0/MMU/cache/COP1,
fetched scalar/vector RSP, queued DMA, observable state, and the separate
RDRAM/T1 A/B boot profiles. Exact authored hello evidence passes on Linux/GCC
and macOS/Clang. Support is limited to the named profiles, builds and fixtures.
Source/model tests and private diagnostic classification are not silicon or
broad compatibility proof. Original historical failures and evidence limits
remain disclosed; current artifacts have immutable consumer/binding retention.

RDP/VI rendering, AI audio, SI/PIF/controller/save integration, an installed
consumer and a playable cleared homebrew remain later milestone work. The
milestone audit covers48 requirements;25 future requirements remain pending.
No software release or release-version tag has been published. The working
name remains uncleared; D2/D5/D6 and physical/rights stops remain unchanged.

## Constraints

- **Rights:** Do not commit, use in public CI, or redistribute protected PIF,
  IPL, CIC, ROM, save, 64DD or firmware data without documented rights.
  Every fixture needs a manifest entry covering source, revision, build
  recipe, digest, profile, rights and expected-result provenance.
- **Determinism:** The canonical core must give identical results for
  identical profile, content, input and guest-time bounds. Host wall-clock,
  filesystem and device state never feed into guest behavior.
- **Evidence:** Keep owner direction, hardware-document facts, code
  observations, historical reports, measurements, inferences,
  recommendations and unknowns distinct. Emulator test results are not
  physical hardware proof.
- **Dependencies:** Keep the dependency tree small and flat. Each dependency
  needs a concrete capability, a justified lifetime and an accepted
  supply-chain cost.
- **Performance:** Correctness comes first. Benchmark named workloads only
  after a correctness baseline exists, and never trade correct output for
  speed.
- **Publication:** The owner chose Apache-2.0 and a public repository under
  szTheory (2026-11-09). The release channel and versioning scheme are still
  open owner decisions; releases wait for them. Never publish PII,
  credentials, private paths, or rights-bearing content.
- **Copied code:** Under Apache-2.0, copy only from compatible licenses
  (ISC, MIT, BSD-3, Unlicense) and record the source, commit SHA, license and
  notice for each copy. Do not copy GPL emulator code or Angrylion-Plus.
- **Security:** ROMs, saves, archives and fixtures are untrusted input.
  Parsers must bound sizes, reject malformed media and leave state unchanged
  when they refuse an operation.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Working name "Tendies64" (owner) | Owner selection. Not trademark clearance. | ✓ Good |
| Autonomous GSD workflow per AUTONOMY.md (owner) | Rolling milestones with explicit rights, authority and physical stops. | ✓ v1.0 applied |
| One canonical machine core; host owns clocks, files, input mapping, presentation and storage (owner) | Determinism and multiple observable hosts without forking machine behavior. | ✓ v1.0 bounded checks |
| Small, flat dependency tree; copy-with-provenance preferred (owner) | Supply-chain and maintenance cost. | ✓ v1.0; no new dependency for evidence fixes |
| Cartridge-first vertical slice; 64DD as a separate later profile | Verifiable first profile; disk media, RTC and expansion remain separate. | ✓ v1.0 scoped cartridge model |
| Boot without Nintendo PIF/IPL bytes, starting with the open IPL3 route | Cleared source and exact manifest identity; HLE-PIF seed is not physical IPL2. | ✓ Authored warm/A/B cold evidence |
| C17, CMake/Ninja; API/ABI freeze later | ADR-0002 accepted C17 with explicit spike evidence limits; real admission fuzzing added in Phase 2. | ✓ Scoped evidence |
| Preserve strict CPU and version software compatibility separately | Owner-approved compat-v1/v2 policies use distinct profile/state domains; undefined choices are not silicon evidence. | ✓ Phase 4 scoped checks |
| Exact local reporting before accuracy claims | Independent source/image denominator and C-owned original-record classification stay separate from public authored CI and hardware qualification. | ✓ Phases 4–6, bounded evidence |
| License: Apache-2.0 (owner, 2026-11-09; MIT was an acceptable alternative) | Owner preference. Permissive, with an explicit patent grant. Code copied from other projects must be license-compatible: ISC, MIT, BSD-3 and Unlicense sources are copyable with notices kept; GPL sources (Project64, Mupen64Plus-core) and the Angrylion-Plus legacy MAME license must not be copied. | ✓ Good |
| Repository visibility: public under the szTheory GitHub account (owner, 2026-11-09) | Owner decision. No PII, credentials or private paths in any tracked file or commit. Commits use the GitHub noreply address. | ✓ Good |
| Release channel and versioning | Still an open owner decision. Releases wait on it. | — Pending (owner) |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-11-15 after v1.0 milestone audit*
