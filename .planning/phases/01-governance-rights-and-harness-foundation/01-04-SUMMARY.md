---
phase: 01-governance-rights-and-harness-foundation
plan: "04"
subsystem: governance
tags: [adr, madr, policy, ci, github-actions, posix-sh, awk, ctest, gov-01, gov-02, gov-06, ver-04]
status: complete

requires:
  - phase: 01-governance-rights-and-harness-foundation
    provides: "gate.sh, CMake presets and the tools glob (01-01), result protocol (01-02), lint allowlist citing ADR-0006 (01-03)"
provides:
  - "scripts/check-docs.sh: nine DOC-* rules, 23 selftest cases, the CONTEXT A-1 exact-path core rule"
  - "docs/adr: template 0000 and ADRs 0001-0007 (0002 proposed, the others accepted), each with a non-empty Reversal evidence section"
  - "docs/policy/copy-provenance.md and docs/policy/rights-and-fixtures.md with the headings the checker requires"
  - ".github/workflows/ci.yml: two gate-only jobs (ubuntu-24.04, macos-26), SHA-pinned checkout, permissions {} plus per-job contents: read"
  - "scripts/check-ci-yml.sh: 14 CI-* rules, 18 selftest cases (one seeded mutation per rule)"
  - "CTest tests repo.docs, repo.docs-selftest, repo.ci-yml, repo.ci-yml-selftest (label repo) via tools/repocheck/CMakeLists.txt"
affects: [01-06, 01-08, 01-09, 01-10, 01-11, 01-12, 01-13]

actuals:
  tokens: 18600
  tasks: 3
  commits: 4
plan_head_before: 2adef4fdd5f7f3ef7bf539143a3a558b2af58aee
plan_head_after: f666f15e2517c9a09afac12d7f5eea9ceba6b657

tech-stack:
  added: []
  patterns:
    - "A selftest builds a temp tree per rule with printf, runs the checker through --root, and asserts the rule id plus exit 1"
    - "Pass cases assert the rule id is absent (expect_absent) so the required-ADR list can grow without breaking the core-rule cases"
    - "CI checks are line-pattern awk that fails closed; mutations are written to new files with sed or awk, never in place"
    - "Policy headings live in one POLICIES table in check-docs.sh; the selftest generates the policy files from it"

key-files:
  created:
    - scripts/check-docs.sh
    - scripts/check-ci-yml.sh
    - tools/repocheck/CMakeLists.txt
    - docs/adr/0000-template.md
    - docs/adr/0001-record-decisions-with-madr.md
    - docs/adr/0002-language-build-and-abi.md
    - docs/adr/0003-cpu-timing-tiers.md
    - docs/adr/0004-emulator-extension-opcodes.md
    - docs/adr/0005-outcome-and-wall-clock-classification.md
    - docs/adr/0006-determinism-lint-mechanism.md
    - docs/adr/0007-copy-and-reading-policy.md
    - docs/policy/copy-provenance.md
    - docs/policy/rights-and-fixtures.md
    - .github/workflows/ci.yml
  modified: []

key-decisions:
  - "ADR-0002 stays proposed; check-docs.sh exempts it through the PROPOSED_OK variable, which plan 01-08 empties when it accepts the record"
  - "DOC-CORE-BEFORE-ADR is purely path-based: find core ! -type d against two literal paths, no content read, no marker (grep -c T64-STUB prints 0)"
  - "check-ci-yml.sh gained a --gate PATH option (not in the interfaces block) so the CI-GATE-EXEC mutation can point at a non-executable copy without touching the real gate.sh"
  - "CI-TIMEOUT requires an integer from 1 to 10 (the locked value is 10); CI-PERMISSIONS-JOB also rejects any permission other than contents: read"

patterns-established:
  - "Docs avoid the literal SPDX tag entirely (not just after line 20), so docs can never look copied to the provenance check in plan 01-10"
  - "No line in docs starts with 'Tendies64 ' together with accurate, compatible or supports: the acceptance grep prints 0"

requirements-completed: [GOV-01, GOV-02, GOV-06, VER-04]

coverage:
  - id: D1
    description: "ADRs 0001-0007 exist with MADR headings, matching status lines and non-empty Reversal evidence; ADR-0002 is proposed and the rest accepted; a missing, malformed or unaccepted record fails the gate"
    requirement: "GOV-01, GOV-06"
    verification:
      - kind: integration
        ref: "sh scripts/check-docs.sh (check-docs: ok); sh scripts/check-docs.sh --selftest (cases=23 failures=0); ctest repo.docs, repo.docs-selftest"
        status: pass
    human_judgment: false
  - id: D2
    description: "Any file under core/ other than the two allowlisted paths fails until ADR-0002 is accepted, for .c, .h and build files, independent of file content"
    requirement: "GOV-01"
    verification:
      - kind: integration
        ref: "check-docs selftest core cases (a)-(f): all print ok, including (f) which proves content is never consulted"
        status: pass
    human_judgment: false
  - id: D3
    description: "The copy-provenance and rights-and-fixtures policies carry the headings the checker requires, including the REUSE decision and the Phase 1 staging-coverage statement"
    requirement: "GOV-02"
    verification:
      - kind: integration
        ref: "check-docs.sh DOC-POLICY-HEADING table plus selftest cases for a missing policy file and a missing heading"
        status: pass
    human_judgment: false
  - id: D4
    description: "CI is one workflow of two gate-only jobs with a SHA-pinned checkout; each hardening rule fails on a seeded mutation"
    requirement: "VER-04"
    verification:
      - kind: integration
        ref: "sh scripts/check-ci-yml.sh (ok); --selftest cases=18 failures=0; actionlint and yamllint clean on ci.yml; ctest repo.ci-yml, repo.ci-yml-selftest"
        status: pass
    human_judgment: false
  - id: D5
    description: "The doc and CI checks run in every gate run as CTest tests labeled repo"
    requirement: "VER-04"
    verification:
      - kind: integration
        ref: "ctest --preset gate -L repo -N lists 4 tests; ./scripts/gate.sh PASS (ctest 8 of 8)"
        status: pass
    human_judgment: false

duration: 8min
completed: 2026-11-10
---

# Phase 1 Plan 04: Decision records, policies and hardened CI Summary

**ADRs 0001-0007 and two policies are gate-checked by `check-docs.sh` (which also keeps any core file beyond the two allowlisted paths out until ADR-0002 is accepted), and CI is one gate-only workflow whose hardening rules each fail on a seeded mutation.**

## Accomplishments

- Tracer slice proven end to end (template, ADR-0001, ADR-0005, `check-docs.sh`, `repo.docs` in the gate), then expanded.
- `check-docs.sh`: DOC-TEMPLATE, DOC-ADR-NAME, DOC-ADR-STRUCT, DOC-STATUS-MISMATCH, DOC-ZERO-ADRS, DOC-ADR-MISSING, DOC-ADR-NOT-ACCEPTED, DOC-POLICY-HEADING, DOC-CORE-BEFORE-ADR. Heading order is a subsequence check that ignores fenced blocks; Reversal evidence counts any non-blank line, including an H3 below it.
- ADR-0002 (proposed) quotes both pre-registered switch criteria verbatim, records the A-2 interpretation and the A-3 stop ("escalate"), and names `DOC-CORE-BEFORE-ADR`. ADR-0003 (tiers; `cop0hazard` separate), ADR-0004 (emux `0x20 0x25 0x2c`; reserved-instruction behavior labelled an inference), ADR-0005 (six outcomes, `wall-backstop`, exit codes), ADR-0006 (two layers, the measured deltas and the allowlist policy), ADR-0007 (four-license allowlist, flat reading ban, owner-visible items).
- Policies: `copy-provenance.md` (eight required headings, the copied-looking definition, REUSE scope: not full REUSE in v1) and `rights-and-fixtures.md` (six headings, statuses, channels, the staging statement).
- `ci.yml` and `check-ci-yml.sh`: every rule in the interfaces block has a seeded mutation, and the selftest fails if a rule id has none.

## Measured results

| Item | Value |
|------|-------|
| `./scripts/gate.sh`, clean `build/gate` (Darwin arm64, Apple clang 21.0.0, CMake 4.4.3, bash 3.2 as sh, BWK awk 20200816) | `[gate] PASS`, `total 8s` (sh-n 0s, lint-selftest 4s, configure 0s, build 0s, ctest 4s, core-lint 0s); ctest 8 of 8 passed |
| `ctest --preset gate -L repo -N` | 4 tests: `repo.docs`, `repo.docs-selftest`, `repo.ci-yml`, `repo.ci-yml-selftest` |
| `repo` label time | 3.3 s of process time (the docs selftest is the longest, about 1.7 s) |
| `check-docs.sh --selftest` | `cases=23 failures=0`; DOC-CORE-BEFORE-ADR (a)-(f) all `ok` |
| `check-ci-yml.sh --selftest` | `cases=18 failures=0`; all 14 rule ids asserted |
| Acceptance greps | `ls docs/adr/000[1-7]-*.md` 7; `run: ./scripts/gate.sh` 2; `fetch-depth: 0` 2; pinned SHA 2; `^## REUSE scope` 1; reserved-word grep over `docs` 0; `T64-STUB` in the checker 0; `^status: proposed` in ADR-0002 1 |
| Linux parity probe (`docker run rust:1.91`, aarch64, dash, mawk 1.3.4) | both checkers clean and both selftests `failures=0` |
| `actionlint`, `yamllint` on `ci.yml` | no findings |

The Linux probe ran on aarch64 only. The x64 ubuntu-24.04 runner (gawk) and macos-26 are first exercised by CI in plan 01-13.

## Task Commits

1. Task 1 (tracer): `d485933` - template, ADR-0001, ADR-0005, `check-docs.sh`, `repo.docs` in the gate
2. Task 2: `d5234f8` - ADR-0002 (proposed), ADR-0003, ADR-0004, ADR-0006; required list extended
3. Task 3: `745f2ac` - ADR-0007, both policies, `ci.yml`, `check-ci-yml.sh`, `repo.ci-yml*` registration
4. Follow-up: `f666f15` - selftest stays valid once ADR-0002 is accepted (see Deviations)

Tracer feedback gate (auto mode): `<verify>` re-run end to end after Task 1, passed, expanded.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Selftest loop variable clobbered by a helper**
- **Found during:** Task 2 (first selftest run after extending the required list)
- **Issue:** the "ADR may stay proposed" loop reused `_n`, which `new_tree` also uses, so the case ran against ADR 0006 and failed. POSIX sh has no function-local variables.
- **Fix:** the loop uses its own variable. The case now reports `ADR 0002 may stay proposed`.
- **Files modified:** scripts/check-docs.sh
- **Commit:** d5234f8

**2. [Rule 1 - Bug] The selftest tree builder hard-coded ADR-0002 as proposed**
- **Found during:** final review, checking the state after plan 01-08 empties `PROPOSED_OK`
- **Issue:** with `PROPOSED_OK` empty the clean-tree case would fail (a proposed ADR-0002), and the core cases b and d would pass vacuously because the builder's ADR-0002 file was accepted.
- **Fix:** the builder derives each status from `PROPOSED_OK`, and the core cases remove the builder's ADR-0002 before seeding their own. Verified in both states.
- **Files modified:** scripts/check-docs.sh
- **Commit:** f666f15

### Interpretation notes (no conflict with a measured fact)

- `check-ci-yml.sh` has an extra `--gate PATH` option so the CI-GATE-EXEC mutation can use a non-executable copy; the default is the real `scripts/gate.sh`.
- The status line check compares the first word of the body line, so a later plan may append a marker after the status without a mismatch. The ADRs keep the line exactly the status.
- CI-TIMEOUT accepts 1 to 10 minutes (the locked value is 10). Plan 01-13 must edit the checker in the same change if a job ever needs longer.
- The docs avoid the literal SPDX tag everywhere, which is stricter than the plan's "after line 20, never at line start". It keeps `provenance-check` (plan 01-10) from ever seeing a doc as copied-looking.
- ADR dates are the decision date from CONTEXT (2026-11-09), the same date the lint allowlist cites for ADR-0006.

## Known Stubs

None. ADR-0002 is intentionally `proposed`; plan 01-08 accepts it, or stops and escalates on a "switch" outcome (CONTEXT A-3).

## Threat Flags

None beyond the plan's register. Mitigations applied: T-01-11 (no `pull_request_target`, no expression in run lines, exact run line; CI-PR-TARGET, CI-EXPR-IN-RUN and CI-RUN-LINE each mutation-tested), T-01-12 (`permissions: {}`, per-job `contents: read`, `persist-credentials: false`, no secrets), T-01-13 (40-hex SHA pin on the only action, CI-USES-PIN), T-01-14 (accepted: review of `scripts/`, `tools/`, `tests/` and CMake diffs; fork-PR approval is plan 01-13), T-01-15 (ADR structure, mismatch, missing, not-accepted and core-before-ADR rules, each with a selftest case), T-01-16 (decision-makers are `szTheory`; a scan of the new files for home paths, temp paths and email addresses found nothing), T-01-SC-04 (docs and sh only, no installs).

## Notes for later plans

- Plan 01-08: when ADR-0002 is accepted, set `PROPOSED_OK=""` in `scripts/check-docs.sh`. The selftest derives every ADR status from that variable and skips the exemption case when it is empty (verified on a scratch copy with `PROPOSED_OK=""`: `cases=22 failures=0`). After acceptance, core files beyond the two allowlisted paths become legal.
- Plan 01-10: add the MADR provenance header to `docs/adr/0000-template.md` (the checker's template rule only needs front matter, the body Status line and the headings). The two policies contain no literal SPDX tag.
- Plan 01-12: the writing rule (no line starting `Tendies64 ` that also holds accurate, compatible or supports as a whole word) currently holds for all of `docs`; the acceptance grep is the same instrument.
- Plan 01-13: the first CI run confirms gawk on x64 for both checkers, `macos-26` sh/awk, and `fetch-depth: 0` with the full-history scan. `sha_pinning_required` and the fork-PR approval PUT remain owner-visible repo settings for that plan. The phase SUMMARY must repeat that ROADMAP SC4's artifact leg is covered by the seeded `staging-rom-magic` and `staging-nul` cases in `gov.rights.selftest` in every gate run (Phase 1 CI uploads nothing).
- Owner-visible items recorded: D1 (ADR-0002, proposed), the pre-vetted-but-not-allowed license list, and the flat reading ban (ADR-0007).

## Self-Check: PASSED

- FOUND: scripts/check-docs.sh, scripts/check-ci-yml.sh, tools/repocheck/CMakeLists.txt, .github/workflows/ci.yml, docs/adr/0000-template.md through 0007, docs/policy/copy-provenance.md, docs/policy/rights-and-fixtures.md
- FOUND commits: d485933, d5234f8, 745f2ac, f666f15 (all ancestors of HEAD)
