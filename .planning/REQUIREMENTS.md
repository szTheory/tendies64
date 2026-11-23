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

- [ ] **VID-07**: RDP depth (Z) is active on the composite profile descriptors 11 and 12 with independently calculated expected values, a wrong-value control and existing profile identities preserved
- [ ] **CORE-06**: The versioned machine state carries every field the RDP and VI need to continue equal after an import (combiner block and memory-coverage table included), with a state/container version marker, and a mid-primitive export/import/continue round trip is exact
- [ ] **VER-06**: The Phase 7 test-strength gaps VG-01 to VG-05 and the standard-NTSC VI clock vector are closed with independent expected values and mutants that fail when the behavior is wrong
- [ ] **RUN-07**: One composite end-to-end fixture (video, audio, input and save) runs through the runner and reports exact raw outputs, final digest and save bytes under a named profile
- [ ] **GATE-01**: The hosted macOS gate runs inside its job limit on a cold Binutils cache, with the planned CTest cost ordering and duplicate-step removal, and the qualification outer limits are recorded (docs/ci-reviews/LOG.md F-106, F-111, F-114)

### Installed consumer and frozen API

- [ ] **VER-05**: Maintainer can see a clean installed package run the same permitted headless fixture through an independent consumer, with relocation and negative controls
- [ ] **API-01**: A C-ABI instance API defines lifetime, profile selection, transactional content admission, timed input, bounded advance, output ring buffers with backpressure, reset and unload
- [ ] **API-02**: Errors fall into three classes: refused (state unchanged), guest-stop (not an error) and defect (instance poisoned), each with tested postconditions
- [ ] **API-03**: The API is frozen only after the postconditions are exercised through the installed consumer
- [ ] **API-04**: The core owns no files, clocks, entropy or threads; the host supplies them

### Minimal host and playable homebrew

- [ ] **API-05**: A minimal host (optional SDL3 target, separate from the core) presents raw video and audio and maps a physical controller
- [ ] **API-06**: A rights-cleared playable homebrew path (authored here or built from a pinned libdragon toolchain) runs both headlessly and in the minimal host
- [ ] **API-07**: LibFuturo fit is evaluated against its then-current contract, and any adapter is optional and does not shape the core API

## Deferred, not scheduled in v1.2

- **VID-04**: RDP differential conformance reported as labeled out-of-tree oracle evidence, never hardware truth. Not mapped to a phase: it needs the owner's D5 local-oracle decision, the parallel-rdp per-file rights audit, a rights-cleared exported corpus and a replay adapter (Plan 07-13, docs/evidence/phase07-oracle-authority.json). It joins v1.2 only by an owner decision; no oracle work until then.
- **v2 outlines** (PERF, ACC, EXP, PLAY families) remain in [v1.1-REQUIREMENTS.md](milestones/v1.1-REQUIREMENTS.md) and ROADMAP.md.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Public release, version tag, packaging channel | D6 is an owner stop; v1.2 is a planning identifier |
| Branding, logos, store listings in the host | D7 name is not cleared |
| Commercial ROMs, saves, protected inputs | Owner rights stop; the playable title must be authored or rights-cleared |
| Oracle execution (Angrylion-Plus, parallel-rdp) | D5 unresolved; no oracle material is run or copied |
| A GPU renderer, save-state UI, rewind | v2 player features (PLAY-01..03) |
| LibFuturo adapter that shapes the core API | API-07 requires any adapter be optional |

## Traceability

Filled by the roadmapper.

| Requirement | Phase | Status |
|-------------|-------|--------|
| VID-07 | TBD | Pending |
| CORE-06 | TBD | Pending |
| VER-06 | TBD | Pending |
| RUN-07 | TBD | Pending |
| GATE-01 | TBD | Pending |
| VER-05 | TBD | Pending |
| API-01 | TBD | Pending |
| API-02 | TBD | Pending |
| API-03 | TBD | Pending |
| API-04 | TBD | Pending |
| API-05 | TBD | Pending |
| API-06 | TBD | Pending |
| API-07 | TBD | Pending |

**Coverage:** 13 v1.2 requirements; 0 mapped; VID-04 deliberately unmapped.

---
*Requirements defined: 2026-11-23*
