# Requirements: Tendies64

**Milestone:** v1.2 Embeddable and Playable (planning identifier; no release version)
**Defined:** 2026-11-23
**Core value:** Every support claim is backed by a named profile, pinned build and reproducible, rights-cleared guest evidence.

## v1.2 Requirements

API-01..07 and VER-05 are carried verbatim from the v1.1 future-requirements list.
VID-07, CORE-06, VER-06, RUN-07 and GATE-01 are new carry-ins from the v1.1 audit,
retrospective and CI review log. Scope was set by the owner's standing autonomy direction
(AUTONOMY.md) and the provisional v1.2 outline; VID-04 stays deferred (see below).

### Carry-ins that must land before the freeze

Scope refined 2026-11-23 by a five-lens fan-out ([synthesis](research/v1.2-fanout/SYNTHESIS.md)).

- [ ] **GATE-02**: The Phase 7 oracle request pins each fixture-manifest row by its own hash, so adding an authored guest-ROM row no longer forces an identity-only rebind; `local_decision` stays blocked and no oracle, rights or publication authority is granted
- [ ] **GATE-01**: The hosted macOS gate has a measured cold-cache run inside its job limit. Route: explicit CTest COST properties so the longest test starts first, the redundant rsp-no-hle guard in the audio-records step removed (the step itself stays: the emit step consumes it), `timeout-minutes` raised to 15 with the CI-TIMEOUT scanner rule and its self-test changed together, before/after step timings recorded, and the outer limits of F-106, F-111 and F-114 recorded
  Recorded 10-04: the rsp-no-hle guard removal is declined (F-116; removal needs a closed Phase 8 qualifier edit); the step stays.
- [ ] **VER-06**: The Phase 7 test-strength gaps VG-01 to VG-05 and the standard-NTSC VI clock vector are closed with independently derived expected values and mutants that fail when the behavior is wrong; VG-01 lands on descriptor 10 before depth is enabled elsewhere
- [ ] **VID-07**: RDP depth (Z) is active on composite profiles under new registered profile ids (the shipped descriptor 11 and 12 ids keep their identities, digests and Z-refusal behavior byte-identical), with independently calculated expected values and a wrong-value control
- [ ] **CORE-06**: One state/container version marker covers the Phase 9 plan 09-10 content change (the RDP combiner block and memory-coverage table already travel in section 8) and the depth members VID-07 makes behavior-bearing; a mid-primitive and mid-scan-out round trip that includes depth is exact, a flip-every-member completeness test sees each member, and state blobs are documented as build-identity-bound and outside the ABI promise
- [ ] **RUN-07**: One composite guest (video, audio, input, save) runs through the real supervised runner twice (a raw-capture run, then a save-out run with the same final digest), each output compared with independent expectations; the Phase 9 raw-capture exclusion is not relaxed

### Installed consumer and frozen API

- [ ] **VER-05**: Maintainer can see a clean installed package (relocated, no source tree visible) run the same permitted headless fixture through an independent consumer that agrees with the in-tree runner and with independent expected literals, with relocation, missing-package, version-mismatch, doctored-header and symbol-collision negative controls
- [ ] **API-01**: A C-ABI instance API defines lifetime, an ABI-version handshake at create, `struct_size` on every in/out struct, profile selection, transactional content admission, timed input, bounded advance, output ring buffers with backpressure, reset and unload; the facade is installable (relocatable CMake package) with internal symbols prefixed
- [ ] **API-02**: Errors fall into three classes: refused (state unchanged), guest-stop (not an error, resumable) and defect (instance poisoned, only destroy and a poison query succeed), each with tested postconditions and a status-class table that lists every status and stop reason once; a refusal-is-a-no-op harness runs under ASan and UBSan against the shipped library
- [ ] **API-03**: The API reaches a freeze candidate (0.x ABI, documented postconditions, a bound ABI-identity record of header hash, public symbol list, struct-layout table and status-class table) only after the postconditions pass through the installed consumer, including a host-shaped loop; the final bind happens after the host and homebrew run on it, and between the two only defect-driven amendments with evidence and a re-run of the installed-consumer suite are allowed
- [ ] **API-04**: The core owns no files, clocks, entropy or threads; the host supplies them. A mechanical check on the members of the installed archive fails if one appears, and a seeded `clock_gettime`/`fopen`/`getentropy`/`pthread_create` object proves the check can fail

### Minimal host and playable homebrew

- [ ] **API-05**: A minimal host separate from the core presents raw video and audio and maps a controller: a pure-C host pump (ring draining, injected clock, input mapping, scripted input) is unit-tested in the required gate, and an optional SDL3 shell (link-only against a system install, never vendored, off by default, in a non-gating lane) sits on top
- [ ] **API-06**: A rights-cleared playable title authored in this repository with the pinned Binutils and open IPL3 (route A) runs headlessly through the runner and the installed consumer and in the minimal host, staying inside qualified scope (fill-list RDP, standard controller, AI tone, one save slot; no Z, textures, RSP microcode or TLB); the libdragon toolchain route is research only in v1.2
- [ ] **API-07**: A written LibFuturo fit evaluation pinned to a commit SHA exists: the paper contract read happens in Phase 11 before the freeze candidate, the final text in Phase 12; any adapter is optional and does not shape the core API

## Deferred, not scheduled in v1.2

- **VID-04**: RDP differential conformance reported as labeled out-of-tree oracle evidence, never hardware truth. Not mapped to a phase: it needs the owner's D5 local-oracle decision, the parallel-rdp per-file rights audit, a rights-cleared exported corpus and a replay adapter (Plan 07-13, docs/evidence/phase07-oracle-authority.json). It joins v1.2 only by an owner decision; no oracle work until then.
- **v2 outlines** (PERF, ACC, EXP, PLAY families) remain in [v1.1-REQUIREMENTS.md](milestones/v1.1-REQUIREMENTS.md) and ROADMAP.md.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Public release, version tag, packaging channel | D6 is an owner stop; v1.2 is a planning identifier (set `git.create_tag` false) |
| Building the libdragon GCC/newlib toolchain, or vendoring SDL3 | Rights and D4 allowlist (zlib is not on it), toolchain build cost and unqualified RSP microcode; research only |
| Real-time, vsync, latency or throughput claims for the host | No throughput is measured (PERF-01 is later); "plays" means correct, not real-time |
| Physical-controller or visual checks as must-haves | Owner-held hardware; recorded as one non-blocking owner-attested checklist |
| Branding, logos, store listings in the host | D7 name is not cleared |
| Commercial ROMs, saves, protected inputs | Owner rights stop; the playable title must be authored or rights-cleared |
| Oracle execution (Angrylion-Plus, parallel-rdp) | D5 unresolved; no oracle material is run or copied |
| A GPU renderer, save-state UI, rewind | v2 player features (PLAY-01..03) |
| LibFuturo adapter that shapes the core API | API-07 requires any adapter be optional |

## Traceability

Filled by the roadmapper on 2026-11-23. Carry-ins close in Phase 10, before the C-ABI freeze in Phase 11.

| Requirement | Phase | Status |
|-------------|-------|--------|
| GATE-02 | Phase 10 | Pending |
| VID-07 | Phase 10 | Pending |
| CORE-06 | Phase 10 | Pending |
| VER-06 | Phase 10 | Pending |
| RUN-07 | Phase 10 | Pending |
| GATE-01 | Phase 10 | Pending |
| VER-05 | Phase 11 | Pending |
| API-01 | Phase 11 | Pending |
| API-02 | Phase 11 | Pending |
| API-03 | Phase 11 | Pending |
| API-04 | Phase 11 | Pending |
| API-05 | Phase 12 | Pending |
| API-06 | Phase 12 | Pending |
| API-07 | Phase 12 | Pending |

**Coverage:** 14 v1.2 requirements; 14 mapped (Phase 10: 6, Phase 11: 5, Phase 12: 3); 0 unmapped. VID-04 deliberately unmapped (deferred, not scheduled).

---
*Requirements defined: 2026-11-23; traceability filled 2026-11-23 (roadmap Phases 10-12)*
