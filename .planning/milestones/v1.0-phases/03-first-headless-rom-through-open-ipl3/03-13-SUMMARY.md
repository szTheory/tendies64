---
phase: 03-first-headless-rom-through-open-ipl3
plan: "13"
subsystem: testing
tags: [ci, boot-evidence, sanitizer, cross-platform]
requires:
  - phase: 03-12
    provides: Real guest controls and canonical determinism checks.
provides:
  - Exact successful Linux/GCC and macOS/Clang guest evidence with matching identities and state.
  - Required selector-bound artifact collection and comparison with negative controls.
  - Current-source scoped verification, resolved code findings and ASVS1 mitigation audit.
affects: [phase-03-validation, headless-runner, phase-04]
actuals:
  tokens: 45857
  tasks: 3
  commits: 18
plan_head_before: be85859cc23ca2066ae140865d9111db05d27647
plan_head_after: 5b3b99de84eacda8bf6383f5f1fa12cf7ca241c8
tech-stack:
  added: []
  patterns:
    - Exact SHA/run/attempt/job metadata precedes strict canonical guest comparison.
key-files:
  created: [scripts/collect-boot-evidence.sh, scripts/compare-boot-evidence.sh, docs/evidence/phase3-ci.md]
  modified: [.github/workflows/ci.yml, scripts/check-ci-yml.sh, scripts/gate.sh, runner/t64_result_check.c, runner/tests/test_result_v2.c, docs/spec/boot-limits.md]
key-decisions:
  - "Keep two hosted jobs and their ten-minute limits; diagnose upload failure without speculative timeout changes."
  - "Separate API archive digest metadata from independently computed extracted JSON digests."
  - "Retain the late package audit and TDD deviations honestly; do not infer human validation."
requirements-completed: [BOOT-01, BOOT-02, BOOT-03, BOOT-04, RUN-01, RUN-03, RUN-05, PROF-05]
coverage:
  - id: D1
    description: "Both successful exact-source hosted jobs publish canonical guest records and the actual pair agrees."
    requirement: PROF-05
    verification:
      - kind: integration
        ref: "GitHub run 43103669084 attempt 1; actual collect and compare with SHA 7c0ea9b5f3076122d506074aa7d3a2283cf9e2a6"
        status: pass
    human_judgment: false
  - id: D2
    description: "Selector, wrapper and comparison controls reject malformed or mismatched evidence."
    requirement: RUN-01
    verification:
      - kind: unit
        ref: "runner.result-v2; collect-boot-evidence --selftest; compare-boot-evidence --selftest; check-ci-yml --selftest"
        status: pass
    human_judgment: false
  - id: D3
    description: "Sanitizer controls and actual-entry code/security findings are checked and resolved."
    verification:
      - kind: integration
        ref: "docs/evidence/phase3-ci.md; 03-REVIEW-FIX.md; 03-SECURITY.md"
        status: pass
    human_judgment: false
duration: 169min
completed: 2026-11-11
status: complete
---

# Phase 3 Plan 13: Exact Hosted Guest Evidence Summary

The collector in `scripts/collect-boot-evidence.sh` and comparator in
`scripts/compare-boot-evidence.sh` verified the actual two-host pair.

## Performance

Elapsed 169 minutes from the first Task 1 commit at 2026-11-11T14:01:36-05:00 through
closeout at 2026-11-11T21:50:52.157575+00:00. This includes CI failures, queue time, reviews and
closeout; it is not a coding-effort or emulator-throughput measurement.
Three tasks completed; 32 changed files across the scoped plan/review range.

## Accomplishments

- Both existing hosted jobs publish only selector-bound sanitized JSON after the common gate and rights/PII checks.
- Required collection checks repository, workflow, source, run, attempt, successful named jobs, unique scoped artifacts and strict wrapper/result identity before atomic pair publication.
- The actual Linux/GCC and macOS/Clang pair matches guest profile/content/input/bound/log/state while preserving distinct truthful compiler build IDs.
- Full sanitizer controls and later changed-path checks are recorded with their exact coverage limits; review and security findings are fixed.

## Task Commits

| Task | Commits | Result |
|---|---|---|
| 1: workflow and emitter | b9cab9e | Exact scoped publication contract |
| 2: collection/checker/comparison | 73e63e0, d6adbec, d3ad1c8, 8eea81c | Required collection, Linux test setup checks and retained-attempt selection |
| Review repairs | ba08b27, 4ee0283, 7c0ea9b | Admission preflight, malformed report and CI guard fixes |
| 3: final evidence | 5b3b99d | Actual successful hosted pair and scoped sanitizer/security facts |

## Automated Checks

- Successful push run 43103669084 attempt 1 tested source 7c0ea9b5f3076122d506074aa7d3a2283cf9e2a6. Both named jobs passed the common gate and staged rights/PII scans.
- Actual selector-bound collection and comparison passed in build/evidence-run-43103669084. Linux artifact 13892259569 and macOS artifact 13892669317 each contain one 1587-byte JSON record. Full API/record identities and compiler build IDs are in docs/evidence/phase3-ci.md.
- Both records report pass/positive-match, tick 530438, 530437 retired instructions, bound 530500, exact 30-byte authored log and state 2e336d7b28c9696a47fa8b794a0583a581b8660d39e6e70664174549540f2f7c.
- Full local gate passed with 91 names. Full ASan/UBSan suite passed with 93 names before later test-setup/shell changes; the later affected C tests passed instrumented, and the shell guard passed all 60 mutation controls. No full-suite rerun is claimed for the final shell-only change.
- Code review disposition is four fixed, zero open. The ASVS1 audit verifies all 27 planned mitigations, zero open. The package legitimacy record is retrospective and preserves the missed pre-use check.

## Deviations and Issues

- Task 2 had a real pre-feature CLI refusal, but its formal C regression cases were added after implementation; there is no claimed earlier formal RED commit. That TDD history is retained rather than reconstructed.
- Linux warnings found ignored test-only setup syscall results; those checks were fixed before the successful pair. The integration watchdog test budget was corrected after sanitizer load exposed setup timing, without changing the production watchdog.
- A predecessor upload received HTML instead of JSON and retried until the unchanged job limit. Its transient inconsistent run status later became terminal. No timeout, job, concurrency or artifact deletion workaround was used. Its lone Linux record was never mixed into the successful pair.
- The collector now handles validated retained artifacts from older attempts while selecting only the requested attempt.
- The executor was interrupted during slow bookkeeping after committing actual evidence; the parent reconciled the committed tree and completed this record directly. No source work was re-executed.

## Evidence Limits and Readiness

This is software smoke evidence for the named prepared-warm profile and authored
fixture. It does not establish authentic IPL2/cold boot, hardware accuracy,
controller consumption or broad compatibility. All three judgment-prohibition
flags remain non-authoritative under AUTONOMY, with no human approval or risk
acceptance asserted. The archive digest is API metadata, not independently
recomputed archive authentication. Phase goal verification remains the next
step; this summary completes the plan, not the phase.

## Self-Check

The actual hosted pair and recorded evidence exist; implementation commits and
review/security records are present. No protected inputs or raw private logs
were published. Owner configuration and AGENTS edits remain unstaged.
