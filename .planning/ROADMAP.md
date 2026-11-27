# Roadmap: Tendies64

## Milestones

- ✅ **v1.0 Verified Headless Core** — Phases 1–6 complete; planning archive 2026-11-15. [Verified scope and history](milestones/v1.0-ROADMAP.md). This is not a software release.
- ✅ **v1.1 Observable Machine** — Phases 7–9 complete 2026-11-20 (VID-04 descoped to v1.2 by owner decision). [Archive](milestones/v1.1-ROADMAP.md), [requirements](milestones/v1.1-REQUIREMENTS.md), [audit](milestones/v1.1-MILESTONE-AUDIT.md), [phase records](milestones/v1.1-phases/). This is not a software release.
- ✅ **v1.2 Embeddable and Playable** — Phases 10–12 complete 2026-11-26 (14 requirements; ABI bound at 1.0, package 1.0.0 as an ABI identity; VID-04 stays deferred). [Archive](milestones/v1.2-ROADMAP.md), [requirements](milestones/v1.2-REQUIREMENTS.md), [audit](milestones/v1.2-MILESTONE-AUDIT.md). Phase directories 10–12 stay in `.planning/phases/` because live gate scripts read their PIN-IMPACT records by path. This is a planning identifier, not a software release.
- 📋 **Rolling horizons (v1.3 next, v1.4 near, v1.5+ mid/later)** — refreshed every milestone; see [Rolling Roadmap](#rolling-roadmap-refreshed-2026-11-27).

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

<details>
<summary>✅ v1.2 Embeddable and Playable (Phases 10–12) — COMPLETE 2026-11-26</summary>

- [x] Phase 10: Carry-in Closure Before the Freeze (21/21 plans) — completed 2026-11-25
- [x] Phase 11: Installed Consumer and Freeze Candidate (29/29 plans) — completed 2026-11-26
- [x] Phase 12: Minimal Host, Playable Homebrew and Final Bind (18/18 plans) — completed 2026-11-26

Full phase details, success criteria and plan lists: [v1.2 archive](milestones/v1.2-ROADMAP.md). Cold gate-macos of the final head: 606 s of 900 s (F-142).

</details>

## Rolling Roadmap (refreshed 2026-11-27)

Priority order. NEXT and NEAR items name their promotion evidence and blocking decision; MID and LATER name only the evidence class. Refreshed at every new milestone from a fan-out; milestone ids are planning identifiers, not software versions. Source: [fan-out roadmap lens](research/v1.2-fanout/lens-roadmap.md); refreshed at the v1.2 close (the next fan-out at `/gsd-new-milestone` re-checks it).

**Guardrails.** G1 after the freeze, no ABI break to add a profile, device or error subcode. G2 compatibility appears only as per-profile evidence tiers; private-lane results are never public claims. G3 at most 5 carried warning-level gaps; carry-ins older than two milestones are closed or moved to Out of Scope. G4 the required gate may not grow without removing equal time; toolchain builds, the SDL3 host and any oracle run in cached or optional lanes. G5 at most 3 phases per milestone, about 10-15 plans per phase, one owner-decision batch per milestone, and every conditional phase names its fallback. G6 no evidence derived from the producer under test; oracle agreement is not hardware truth. G7 per-file rights rows before first use of any corpus; commercial content only under a decided private lane.

| Horizon | Milestone | Goal | Promotion evidence / blocker |
|---------|-----------|------|------------------------------|
| NEXT | v1.3 Real Microcode | RSP-to-RDP and RSP-to-AI paths driven by a real rights-cleared microcode corpus (libdragon rspq/rdpq/mixer, per-file license to confirm), PERF-01 measurement only, EXP-01 Expansion Pak (additive profile). Conditional phase: VID-04 oracle conformance if D5 is granted; fallback ACC-02 hardware-derived public vectors | Per-file rights rows for the corpus; D4 and D5 decisions; real microcode is the gate to any retail-shaped boot; v1.3 also takes the v1.2 carry-ins DEF-W1-01 (three unpinned perspective-floor copies in sealed RDP bodies), T2 replay-driver duplication, T5 recorder failure names, T6 `T64_REAL_KEEP_WORKSET`, the untriaged Phase 10 video-packet payload digest advisory (W-5) and, when the first additive 1.1 amendment lands, a consumer built with the 1.0 header to prove the older-minor rule (W-8) |
| NEAR | v1.4 Commercial-Shaped Boot and Private Lane | HLE boot per CIC profile (EXP-03, 6105 last), large ROMs, FlashRAM and Controller Pak (EXP-04/05), a local private lane for owner-held commercial ROMs with hash-keyed local results and tier labels, PERF-02 only if measurement points at the bottleneck | New decision D8 (private lane); v1.3 evidence |
| MID | v1.5 and beyond | ACC-01 timing, PAL/MPAL (EXP-02), RTC (EXP-06), Rumble/Transfer Pak, then PERF-03 JIT last; PLAY-01 and PLAY-03 as cheap fillers | Primary-document timing sources; lockstep digest equality for any JIT |
| LONG | Open | 64DD (EXP-07, rights-blocked), optional GPU renderer (PLAY-02), a LibFuturo adapter, public external CI (D2) | Rights-cleared disk route; owner decisions |

**Owner-decision batches.** Batch 1 (any time; does not block v1.3): D4 allowlist additions (BSD-2, zlib, MIT-0, Apache-2.0 sibling code), D5 local parallel-rdp oracle after the per-file audit, D6 release channel and versioning, D7 name posture. Batch 2 (now due, at the v1.2 close; does not block v1.3): D8 private commercial-ROM lane, the Phase 12 owner checklist and the recorded exceptions for owner review (nine in `docs/evidence/phase12-qualification.md`); D2 stays deferred because the authored hello is the public smoke lane. Batch 3 (at v1.3 close, optional): whether the owner will ever run test ROMs on their own console.

## Progress

| Milestone | Phases | Status | Completed |
|-----------|--------|--------|-----------|
| v1.0 Verified Headless Core | 1-6 | Complete | 2026-11-15 |
| v1.1 Observable Machine | 7-9 | Complete | 2026-11-20 |
| v1.2 Embeddable and Playable | 10-12 | Complete | 2026-11-26 |
