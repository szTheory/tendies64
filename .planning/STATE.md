---
gsd_state_version: "1.0"
milestone: v1.0
milestone_name: Verified Headless Core
current_phase: 3
current_phase_name: First Headless ROM Through Open IPL3
current_plan: Not started
status: planning
stopped_at: Phase 2 verified and closed; continuing Phase 3 decision-focused research
last_updated: "2026-11-11T00:07:02.051Z"
last_activity: 2026-11-10
state_head: 34b43f760e0858cdbc3a37e031058f176eea4073
progress:
  total_phases: 6
  completed_phases: 2
  total_plans: 21
  completed_plans: 21
  percent: 33
last_activity_desc: Phase 2 complete, transitioned to Phase 3
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-10)

**Core value:** Every Tendies64 support claim is backed by evidence: a named profile, a pinned build and a reproducible, rights-cleared guest result.
**Current focus:** Phase 3 — First Headless ROM Through Open IPL3

## Current Position

Phase: 3 — First Headless ROM Through Open IPL3
Current Plan: Not started
Status: Ready to plan
Last activity: 2026-11-10

Progress: [███░░░░░░░] 33%

## Performance Metrics

**Velocity:**
- Total plans completed: 21
- Average duration: -
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1 | 13 | - | - |
| 2 | 8 | - | - |

**Recent Trend:**
- Last 5 plans: -
- Trend: -

*Updated after each plan completion*
**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 01 P01 | 9min | 2 tasks | 15 files |
| Phase 01 P02 | 30min | 2 tasks | 16 files |
| Phase 1 P03 | 6min | 3 tasks | 24 files |
| Phase 1 P04 | 8min | 3 tasks | 14 files |
| Phase 1 P05 | 30min | 2 tasks | 17 files |
| Phase 1 P06 | 8 min | 3 tasks | 5 files |
| Phase 1 P07 | 4 min | 2 tasks | 24 files |
| Phase 1 P08 | 14min | 3 tasks | 33 files |
| Phase 1 P09 | 11min | 3 tasks | 47 files |
| Phase 1 P10 | 25 min | 2 tasks | 7 files |
| Phase 1 P11 | 6 min | 3 tasks | 40 files |
| Phase 1 P12 | 16min | 2 tasks | 21 files |
| Phase 1 P13 | 10 min | 2 tasks | 2 files |
| Phase 02 P01 | 31 min | 3 tasks | 9 files |
| Phase 02 P02 | 40 min | 2 tasks | 11 files |
| Phase 02 P03 | 47 min | 3 tasks | 14 files |
| Phase 02 P04 | 37 min | 2 tasks | 11 files |
| Phase 02 P05 | 40 min | 2 tasks | 15 files |
| Phase 02 P06 | 40 min | 2 tasks | 15 files |
| Phase 02 P07 | 54 min | 2 tasks | 11 files |
| Phase 02 P08 | 54 min | 3 tasks | 12 files |

## Accumulated Context

### Decisions

Decisions are logged in the PROJECT.md Key Decisions table. Recent decisions that affect current work:

- [Roadmap]: Milestone v1.0 covers Phases 1-6, ending with the full `nemu64-test` base suite classified. GSD milestone IDs are not software versions
- [Roadmap]: D1 is accepted: C17, CMake 3.25+ and Ninja; the API/ABI remains unfrozen. ADR-0002 records spike limits; Phase 2 fuzzed real admission under ASan/UBSan.
- [Roadmap]: Primary-document verification (VR4300 manual and Nintendo docs pinned by SHA) opens Phase 3, and the CPU-area check is the Phase 4 entry gate
- [Owner, 2026-11-09]: Apache-2.0 license; public repository under szTheory. These are settled, not open gates
- [Phase 01]: 01-01: t64_core declared in root CMakeLists.txt; core/ holds only the two CONTEXT A-1 files
- [Phase 01]: 01-01: canonical JSON writer copies keys into per-depth 127-byte buffers; longer keys refused
- [Phase 01]: 01-02: fixture present but not a regular file, over 64 MiB or unreadable maps to harness-error/usage (no dedicated detail in the closed enum)
- [Phase 01]: 01-02: runner targets add _DARWIN_C_SOURCE on Apple because _POSIX_C_SOURCE=200809L hides O_NOFOLLOW and mkdtemp (core is untouched)
- [Phase 01]: 01-02: synthetic verdict grammar is PASS lines starting 'PASS ', FAIL lines starting 'FAIL ', and a verdict line exactly 'Done!' (synthetic-only)
- [Phase 1]: 01-03: Layer-1 lint skips name checks inside attribute parentheses; selftest rejects meta rules without a dedicated control; allowlist malformed row is a tool error
- [Phase 1]: 01-04: ADR-0002 stays proposed (PROPOSED_OK exemption in check-docs.sh, removed by plan 01-08); DOC-CORE-BEFORE-ADR is path-based with no content marker
- [Phase 1]: 01-04: check-ci-yml.sh adds a --gate option for the CI-GATE-EXEC mutation; CI-TIMEOUT accepts 1 to 10 minutes
- [Phase 1]: 01-05: JSON parser rejects object keys over 127 bytes (string-length) because the canonical writer cannot emit them; depth limit clamped to 64
- [Phase 1]: 01-05: run-expect.sh treats only --ext-matching files as seeds and matches rule_id ok as a whole word
- [Phase 1]: 01-06: allowlisted digests waive content classes only; R-DENIED-EXT is never waived, and zero-scanned results exit 2
- [Phase 1]: 01-06: history blob names come from rev-list --objects plus the raw log, since rev-list names a shared blob once; gitlinks read from raw-log mode 160000
- [Phase 1]: 01-06: added P-SHALLOW to pii-scan (unlisted rule id) so a shallow clone cannot pass a vacuous history scan; ROADMAP SC4 artifact leg covered by seeded staging-rom-magic and staging-nul cases
- [Phase 1]: 01-07: result consumer checks schema, then schema_version, then exact key sets, and checks canonical bytes last; S-DUPLICATE precedes S-ORDER and S-INVENTORY; only S-TRUNCATED and S-INVENTORY echo a harness-error detail line
- [Phase 1]: 01-07: identity prints nothing unless every line validated and compares no inventory (run suite first); profile_id/build_id restricted to [A-Za-z0-9._+-]{1,128}
- [Phase 1]: D1 decided by the pre-registered Rust spike: stay with C17 (no Rule (a) or Rule (b) trigger); ADR-0002 accepted, check-docs requires all of ADR 0001-0007 accepted
- [Phase 1]: Spike Rule (a) reads 'both OSes' as a conjunction: a class triggers only if the C gate lint exits 0 on host and Linux container and the Rust twin exits 1
- [Phase 1]: 01-09: manifest forbidden shape lists sha256/size/source/build/expected/path as accept-anything members so M-FORBIDDEN-DIGEST reports them; non-forbidden shape allows optional forbidden_reason and M-FORBIDDEN-REASON fires when either right is forbidden
- [Phase 1]: 01-09: a manifest with any finding makes rights-scan.sh exit 2 and emits no allowlist rows; --root defaults to the current directory; the CTest wrapper builds its argv label at run time because LastTest.log echoes the command line
- [Phase 1]: 01-10: MIT text for the MADR record comes from LICENSE.MIT at tag 4.0.0 (LICENSE holds only the expression); record pins the peeled commit 2475fe19..., maintenance_owner szTheory
- [Phase 1]: 01-10: provenance detector counts a tag only at line start (optional comment leader) within the first 20 lines; selftest asserts exact rule-id sets (76 cases, 16 ids)
- [Phase 1]: 01-11: host-misbehavior behaviours are end directives (hang, abort, segv, exit0-no-result, exit7); logrep stored as one repeated event; matrix columns runner_flags and log_limit appended after exit
- [Phase 1]: 01-11: no supervisor or classifier change was needed; all 19 controls matched the locked precedence table and goldens derived independently with printf and shasum
- [Phase 1]: 01-12: claims gate is POSIX sh + awk; evidence must pass t64-result-check identity AND end with a t64.suite record; synthetic evidence reports only C-EVIDENCE-SYNTHETIC
- [Phase 1]: 01-12: oracle_kind/oracle_revision empty is C-ORACLE; conditional keys on the wrong tier are C-FIELD-UNKNOWN; selftest (49 cases) proves rules the 17 seeds cannot reach
- [Phase 1]: 01-13: only the first push triggers a hosted run; log-only commits carry [skip ci], so every completed run has a Runs row
- [Phase 1]: 01-13: requiring gate-linux and gate-macos as status checks on main left as an owner-visible item (F-007); fork-PR approval set to all_external_contributors

- [Phase 2]: Machine skeleton verified on both hosted lanes; native model evidence only. Three non-authoritative judgment flags retained under autonomous soft-gate policy; no violation or accepted risk.
- [Phase 2]: CORE-02 remains partial; all-device integration is assigned to Phase 9.

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

Last session: 2026-11-11T00:07:02.001Z
Stopped at: Phase 2 verified and closed; continuing Phase 3 decision-focused research
Resume file: None
