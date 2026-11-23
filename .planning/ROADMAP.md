# Roadmap: Tendies64

## Milestones

- ✅ **v1.0 Verified Headless Core** — Phases 1–6 complete; planning archive 2026-11-15. [Verified scope and history](milestones/v1.0-ROADMAP.md). This is not a software release.
- ✅ **v1.1 Observable Machine** — Phases 7–9 complete 2026-11-20 (VID-04 descoped to v1.2 by owner decision). [Archive](milestones/v1.1-ROADMAP.md), [requirements](milestones/v1.1-REQUIREMENTS.md), [audit](milestones/v1.1-MILESTONE-AUDIT.md), [phase records](milestones/v1.1-phases/). This is not a software release.
- 🚧 **v1.2 Embeddable and Playable** — Phases 10–12; planned 2026-11-23. 13 requirements mapped; VID-04 deferred and unscheduled. This is a planning identifier, not a software release.
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

**Milestone Goal:** A frozen C-ABI, proven through an independent installed consumer, and a
rights-cleared homebrew that plays in a minimal host, so Tendies64 does not stay tests-only.

**Ordering rule (decided at v1.2 start):** every carry-in that changes state, descriptors or
digests (VID-07, CORE-06, VER-06, RUN-07, GATE-01) closes in Phase 10, before the C-ABI freeze in
Phase 11, so the frozen API and state format rest on settled behavior. Phase 12 builds only on the
frozen API.

**Deferred, not scheduled:** VID-04 (differential oracle evidence) is unmapped. It joins v1.2 only
by an owner D5 decision plus the parallel-rdp per-file rights audit, a rights-cleared exported
corpus and a replay adapter (Plan 07-13). No oracle work happens until then.

- [ ] **Phase 10: Carry-in Closure Before the Freeze** - Z depth on the composite profiles, complete RDP/VI state, the Phase 7 test-strength debt, one composite end-to-end fixture and a hosted macOS gate inside its limit
- [ ] **Phase 11: Installed Consumer and Frozen API** - C-ABI postconditions exercised through a clean installed package by an independent consumer, then frozen
- [ ] **Phase 12: Minimal Host and Playable Homebrew** - Optional SDL3 host on the frozen API, a rights-cleared playable title and a LibFuturo fit evaluation

### Phase 10: Carry-in Closure Before the Freeze
**Goal**: The composite profiles render depth, export and import every RDP and VI field they behave on, and carry independently checked tests for the accepted Phase 7 gaps; one composite fixture runs end to end through the runner; and the hosted gate fits its job limit, so the state, descriptors and digests that Phase 11 freezes are settled
**Depends on**: Phase 9
**Requirements**: VID-07, CORE-06, VER-06, RUN-07, GATE-01
**Decision gates**: D5 (no oracle material; every expected value is authored independently). A `fixtures/manifest.json` row edit moves the Phase 7 oracle-request pin; check the scope of the recorded owner rebind approval (STATE.md Blockers) before relying on it, and keep `local_decision` blocked
**Research**: LIGHT (codebase-internal; recheck RDP depth semantics against the primary RDP documentation before deriving expected values)
**Success Criteria** (what must be TRUE):
  1. An authored depth fixture on descriptors 11 and 12 produces Z-tested pixels and depth values that equal independently calculated literals, a wrong-depth control fails, and the digests of all 17 pre-Phase-9 profile ids and the existing composite profile identities stay as documented (byte-identical, or versioned under the compatibility policy with the change recorded)
  2. A state exported in the middle of an RDP primitive and during a VI scan-out over coverage marks imports and continues to the same frames, digest and outputs as the uninterrupted run; the container carries a state/container version marker, and a digest-completeness test that flips every behavior-bearing RDP and VI member (combiner block and memory-coverage table included) sees each flip
  3. VG-01 to VG-05 and the standard-NTSC VI clock vector each have independently derived expected values and at least one mutant that fails when the behavior is wrong
  4. One composite fixture driving video, audio, input and save on a named profile runs through the real supervised runner and reports exact raw video and audio bytes, the final digest and the save bytes, each compared with independent expectations
  5. A hosted gate-macos run on a cold Binutils cache finishes inside its job limit with the CTest cost ordering and the duplicated audio-records step removed, and the qualification outer limits from F-106, F-111 and F-114 are recorded in docs/ci-reviews/LOG.md
**Plans**: TBD

### Phase 11: Installed Consumer and Frozen API
**Goal**: An independent consumer, using only a clean installed package, exercises every lifetime, admission, advance, output and failure postcondition of the C-ABI, and only then is the C-ABI frozen
**Depends on**: Phase 10
**Requirements**: VER-05, API-01, API-02, API-03, API-04
**Decision gates**: D6 (the installed package and the freeze are local; neither is a release, version tag or packaging channel)
**Research**: MODERATE
**Success Criteria** (what must be TRUE):
  1. A consumer built against a relocated, clean installed package (no source tree visible) runs the Phase 10 composite fixture and gets the same result as the in-tree runner. Negative controls, such as a missing package, a mismatched version or a visible source tree, fail
  2. Through the installed consumer, the C-ABI covers instance lifetime, profile selection, transactional content admission, timed input, bounded advance, output ring buffers with backpressure, reset and unload, each with an accepted operation and a refusal
  3. Each error class has tested postconditions through the installed consumer: a refusal leaves state bit-identical, a guest-stop is reported as not an error and can be resumed, and a defect poisons the instance so that later calls refuse
  4. The installed core library owns no files, clocks, entropy or threads: the host supplies them, and a check of the installed library's imported symbols fails if one appears
  5. The C-ABI is frozen only after criteria 1 to 4 pass through the installed consumer; the freeze record binds the header and ABI identity, and an incompatible header or ABI change fails a check
**Plans**: TBD

### Phase 12: Minimal Host and Playable Homebrew
**Goal**: A rights-cleared homebrew title runs headlessly and plays in a minimal host window with a physical controller, using only the frozen API, without the core depending on the host
**Depends on**: Phase 11
**Requirements**: API-05, API-06, API-07
**Decision gates**: D7 (no branding in the host or packaging). D6 applies to any release. D4 covers SDL3, which is zlib-licensed and link-only. Pin the route B libdragon toolchain and record its GCC/newlib notices
**Research**: DEEP (playable toolchain route; LibFuturo contract recheck)
**Success Criteria** (what must be TRUE):
  1. A rights-cleared homebrew title (authored here or built from a pinned libdragon toolchain, with its manifest entry) produces the expected guest output headlessly through the runner and through the installed consumer
  2. The same title plays in the minimal host: it presents the raw video and audio, and a physical controller mapped by the host to logical input moves the game; scripted host input produces the same guest result as the headless run
  3. The minimal host is an optional SDL3 target built on the frozen API from the installed package; the core library builds and links with no SDL dependency, and a build without the host target succeeds
  4. A written LibFuturo fit evaluation against its then-current contract exists. Any adapter is optional and leaves the frozen core API unchanged
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

**Execution order:** 10 → 11 → 12

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 10. Carry-in Closure Before the Freeze | v1.2 | 0/TBD | Not started | - |
| 11. Installed Consumer and Frozen API | v1.2 | 0/TBD | Not started | - |
| 12. Minimal Host and Playable Homebrew | v1.2 | 0/TBD | Not started | - |
