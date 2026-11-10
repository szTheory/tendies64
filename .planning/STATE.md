---
gsd_state_version: "1.0"
milestone: v1.0
milestone_name: Verified Headless Core
current_phase: 01
current_phase_name: Governance, Rights and Harness Foundation
status: executing
stopped_at: Completed 01-02-PLAN.md
last_updated: "2026-11-10T01:31:41.826Z"
last_activity: 2026-11-09
last_activity_desc: Phase 1 execution started
state_head: 3d0f6d3b6d59a4bafec5a0cad4c7a8cd6b4e178a
progress:
  total_phases: 6
  completed_phases: 0
  total_plans: 13
  completed_plans: 2
  percent: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-09)

**Core value:** Every Tendies64 support claim is backed by evidence: a named profile, a pinned build and a reproducible, rights-cleared guest result.
**Current focus:** Phase 1 — Governance, Rights and Harness Foundation

## Current Position

Phase: 1 (Governance, Rights and Harness Foundation) — EXECUTING
Plan: 3 of 13
Status: Ready to execute
Last activity: 2026-11-09 — Phase 1 execution started

Progress: [░░░░░░░░░░] 0%

## Performance Metrics

**Velocity:**
- Total plans completed: 0
- Average duration: -
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**
- Last 5 plans: -
- Trend: -

*Updated after each plan completion*
**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 01 P01 | 9min | 2 tasks | 15 files |
| Phase 01 P02 | 30min | 2 tasks | 16 files |

## Accumulated Context

### Decisions

Decisions are logged in the PROJECT.md Key Decisions table. Recent decisions that affect current work:

- [Roadmap]: Milestone v1.0 covers Phases 1-6, ending with the full `nemu64-test` base suite classified. GSD milestone IDs are not software versions
- [Roadmap]: D1 (language/build/ABI) is recorded as the Phase 1 ADR with C17, CMake 3.25+ and Ninja as the default, and goes in the owner decision batch
- [Roadmap]: Primary-document verification (VR4300 manual and Nintendo docs pinned by SHA) opens Phase 3, and the CPU-area check is the Phase 4 entry gate
- [Owner, 2026-11-09]: Apache-2.0 license; public repository under szTheory. These are settled, not open gates
- [Phase 01]: 01-01: t64_core declared in root CMakeLists.txt; core/ holds only the two CONTEXT A-1 files
- [Phase 01]: 01-01: canonical JSON writer copies keys into per-depth 127-byte buffers; longer keys refused
- [Phase 01]: 01-02: fixture present but not a regular file, over 64 MiB or unreadable maps to harness-error/usage (no dedicated detail in the closed enum)
- [Phase 01]: 01-02: runner targets add _DARWIN_C_SOURCE on Apple because _POSIX_C_SOURCE=200809L hides O_NOFOLLOW and mkdtemp (core is untouched)
- [Phase 01]: 01-02: synthetic verdict grammar is PASS lines starting 'PASS ', FAIL lines starting 'FAIL ', and a verdict line exactly 'Done!' (synthetic-only)

### Pending Todos

None yet.

### Blockers/Concerns

- D2 (owner stop): `nemu64-test` rights are not documented. Until they are, it runs local-only with no uploads or public results (Phases 4-6). Owner decides after a per-asset rights audit
- D3: IPL3 variant/revision pin and maintenance owner (libdragon archived 2026-10-01). Must be resolved in Phase 3
- D4: copy allowlist beyond ISC/MIT/BSD-3/Unlicense (owner). Default: keep the list as written
- D5: Angrylion-Plus oracle use (owner/legal). Blocks Phase 7 conformance claims only
- D6: release channel and versioning (owner). Blocks releases only
- D7: name not trademark-cleared. No branding
- Hardware-behavior requirements are unverified against primary documents until the Phase 3/4 checks

## Deferred Items

Items acknowledged and deferred at milestone close, most recent first:

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| *(none)* | | | | |

## Session Continuity

Last session: 2026-11-10T01:31:41.808Z
Stopped at: Completed 01-02-PLAN.md
Resume file: None
