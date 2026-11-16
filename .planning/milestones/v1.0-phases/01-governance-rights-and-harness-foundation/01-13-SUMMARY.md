---
phase: 01-governance-rights-and-harness-foundation
plan: "13"
subsystem: ci
tags: [github-actions, ci-review, critical-path, fork-pr-approval, posix-sh, gh-api, ver-04, gov-05]
status: complete

requires:
  - phase: 01-governance-rights-and-harness-foundation
    provides: "scripts/gate.sh and the gate presets (01-01), ci.yml and check-ci-yml.sh (01-03), rights-scan.sh and pii-scan.sh (01-04), every gate check added by 01-05 to 01-12"
provides:
  - "scripts/ci-review.sh RUN_ID [--repo] [--append FILE] [--finding] [--defer] [--log]: measured queue/run/wall/critical-path/slowest-step review from gh api run and jobs data; refuses incomplete runs and duplicate rows"
  - "docs/ci-reviews/LOG.md: append-only Runs and Findings tables, review policy, Phase-1-base marker, 1 Runs row and 16 findings"
  - "Phase 1 published to origin/main (public repo) after the full pre-push sequence; the hosted gate for the pushed code SHA is green on gate-linux (ubuntu-24.04) and gate-macos (macos-26)"
  - "Fork-PR approval on szTheory/tendies64 set to all_external_contributors, verified by read-back"
affects: [phase-1-verification, phase-2]

actuals:
  tokens: 4850
  tasks: 2
  commits: 3
plan_head_before: dccfbb86f96e24351519d8dc465c9d613d1b9857
plan_head_after: db2a45fcbc77571e7b7b77b8912261f7e3489831

tech-stack:
  added: []
  patterns:
    - "CI review as data: queue and run seconds come from gh api --jq fromdate arithmetic over .../runs/<id>/jobs, never from gh run view (which lacks job created_at)"
    - "Append-only log edited only by script: rows are inserted under the last table row of a section, a run id or finding id already present is refused"
    - "Log-only commits carry [skip ci], so recording a review never triggers a run that would need its own review"

key-files:
  created:
    - scripts/ci-review.sh
    - docs/ci-reviews/LOG.md
  modified: []

key-decisions:
  - "Only the first push triggered a hosted run: the 64 phase commits went up as one push, and the two log-only follow-ups carry [skip ci], so exactly one completed run (42751487155) needs a Runs row and it has one"
  - "No finding changes ci.yml, so the workflow stays untouched (F-014); the one taken performance change (ctest jobs 4) lives in CMakePresets.json and was confirmed on both hosted runners (F-010)"
  - "Requiring gate-linux and gate-macos as status checks on main is left as an owner-visible item (F-007): it is a second repository setting, and the plan authorizes exactly one write"
  - "sha_pinning_required and workflow lockfiles are declined with 'owner decision' (F-015, F-016)"

patterns-established:
  - "Pre-push sequence as a script: gate, CI=true gate, rights scan, PII scan (tree plus history), PII scan over origin/main..HEAD, clean status, run on the exact commit before every push"

requirements-completed: [VER-04, GOV-05]

coverage:
  - id: D1
    description: "Phase 1 pushed after the pre-push sequence; hosted run for the code-bearing pushed SHA is green on gate-linux and gate-macos"
    requirement: "GOV-05"
    verification:
      - kind: integration
        ref: "plan 01-13 Task 1 automated verify 1 (gh run list plus gh api runs/<id>/jobs: jobs success = gate-linux,gate-macos)"
        status: pass
    human_judgment: false
  - id: D2
    description: "ci-review.sh produces measured queue/run/critical-path rows and the LOG has a Runs row for every completed run of the pushed SHAs and at least 3 findings (16 present, including the public-repo cost/gating recheck)"
    requirement: "VER-04"
    verification:
      - kind: integration
        ref: "plan 01-13 Task 1 automated verify 2 (LOG row for every completed run id of git rev-list Phase-1-base..origin/main, F-NNN count >= 3)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Fork-PR workflow approval is all_external_contributors"
    requirement: "GOV-05"
    verification:
      - kind: integration
        ref: "gh api repos/szTheory/tendies64/actions/permissions/fork-pr-contributor-approval --jq .approval_policy"
        status: pass
    human_judgment: false
  - id: D4
    description: "Requiring both CI jobs as status checks on main is a repository setting not written by this plan"
    verification: []
    human_judgment: true
    rationale: "Owner-visible decision recorded as finding F-007; changing it is outside the one authorized repository-setting write"

duration: 10min
completed: 2026-11-10
---

# Phase 1 Plan 13: Publish, hosted gate review and fork-PR approval Summary

**Phase 1 is public on origin/main, the hosted run of the same gate is green on ubuntu-24.04 (x86_64, 21 s) and macos-26 (arm64, 33 s) with a 35 s measured critical path, a script-built CI review log carries the run row and 16 resolved findings, and outside-contributor workflows now require approval.**

## Performance

- **Duration:** 10 min (the plan itself is mostly waiting on three gate runs and one 36 s hosted run)
- **Started:** 2026-11-10T03:41:00Z
- **Completed:** 2026-11-10T03:52:00Z
- **Tasks:** 2
- **Files modified:** 2 created (`scripts/ci-review.sh`, `docs/ci-reviews/LOG.md`); `.github/workflows/ci.yml` untouched

## Accomplishments

- `scripts/ci-review.sh` reads `gh api` run and jobs data and computes queue (`started_at - created_at`), run (`completed_at - started_at`), wall, critical path (max over jobs of queue plus run) and the slowest step, with `--jq fromdate` arithmetic that strips fractional seconds first. It refuses a run that is not `completed`, refuses a duplicate run or finding id, validates findings (`F-NNN`, `taken` or `declined`, no `|`) before any write, and inserts rows under the last table row of a section so existing rows are never touched. Tried on a public run of another repository before use, including the duplicate refusals and a deferral row.
- Pre-push sequence ran on each of the three pushed commits and passed every time: `./scripts/gate.sh`, `CI=true ./scripts/gate.sh` (banner confirmed `CI mode: --require-fixtures`), rights scan (history=382 to 384 blobs, findings=0), PII scan (tree 293 files, 67 to 69 commits, findings=0), PII scan over `origin/main..HEAD` (findings=0), clean status.
- Hosted run 42751487155 for 79a1301 is green on both runners. gate-linux: x86_64, gcc 13.3, GNU Awk 5.2.1, cmake 3.31.6, 49 of 49 tests, queue 2 s, run 21 s. gate-macos: arm64, Apple clang 21, awk 20200816, cmake 4.4.5, 49 of 49 tests, queue 2 s, run 33 s. Wall 36 s, critical path 35 s (gate-macos), slowest step `gate-macos/Gate` 27 s.
- All the platform assumptions the earlier plans flagged as unverified until CI held on the first run: gawk on x64 (A8), BWK awk on macos-26 (A9), POSIX ERE, `O_NOFOLLOW` paths, glibc. No red rounds, no fix-up commits.
- Fork-PR approval changed from `first_time_contributors` to `all_external_contributors` with `PUT .../fork-pr-contributor-approval`; the assumed body field `approval_policy` (A4) was accepted on the first call and the read-back returned the new value. No other repository setting was written.
- LOG findings F-001 to F-016: queue versus run, duplicate setup, `fetch-depth: 0`, caching, concurrency, matrix cost, A8, A9, the `ctest jobs 4` confirmation (F-010) and the serial gate-time history (F-011), critical path, the public-repo cost and gating recheck (F-007), fork-PR approval taken (F-013), `ci.yml` unchanged (F-014), `sha_pinning_required` and lockfiles declined as owner decisions (F-015, F-016).

## Pushed SHAs

Phase-1-base `57227793345c59259ab6eac12d1044bc408dfd60` (origin/main before the first push). Three pushes, 64 commits in `git rev-list <base>..origin/main`:

| Push | Head SHA | Contents | Hosted run |
|------|----------|----------|------------|
| 1 | 79a1301b8e0db4ce676d2682153fe8d6c8152eb0 | the whole phase (61 commits) plus the ci-review.sh and LOG commit, code-bearing | 42751487155, success on both jobs, Runs row present |
| 2 | 49e739bb4037f38c97d9723c6de3b2e672f7589b | LOG only, `[skip ci]` | none triggered (confirmed by `gh run list`) |
| 3 | db2a45fcbc77571e7b7b77b8912261f7e3489831 | LOG only, `[skip ci]` | none triggered |

The 64 pushed SHAs, in `git rev-list` order: db2a45f 49e739b 79a1301 dccfbb8 6c27709 7144f55 6b82d98 fde1f8d b5a2374 9d5ff8d 47a8fdb 80b8110 82e6400 d16bbe7 cbc8c9e e660b7f f760fd7 f0cacf0 9116457 2dcc969 7ca9786 8323be5 adb552e 31c7bb6 90a497b 0063abd 9390f0b b9ce2cb 93c18ed de1f316 27999b8 cf4c36f a2f2861 585741f 93616ad b353612 7402e67 b6adbb7 82d627b 89f29a5 50702cd 7d68d8f bc8edc7 cbe4061 f666f15 745f2ac d5234f8 d485933 2adef4f e8f70c9 0ee8432 f3c946f c5ecaa9 055fed0 3d0f6d3 9c8c44f 1dafcdc eed2153 9d09010 bed5d9c f1c87d8 501bae6 8170cc5 dbc40d4. The pre-push sequence passed on each push head, and the rights and PII scans cover full history, so every SHA in the range was scanned.

## Task Commits

1. **Task 1: pre-push checks, push, hosted gate, review row** - `79a1301` (feat: `ci-review.sh` and the LOG header, code-bearing); `49e739b` (docs, `[skip ci]`: the run row and F-001 to F-012)
2. **Task 2: fork-PR approval and findings** - `db2a45f` (docs, `[skip ci]`: F-013 to F-016); the settings write itself is external state, not a commit

## Deviations from Plan

### Auto-fixed Issues

None.

### Notes (not plan violations)

- **Untracked `.planning/state.json` and the `git status --porcelain` checks.** The plan asks `git status --porcelain` to print nothing, but the run instructions say to leave `.planning/state.json` untracked. The pre-push sequence therefore treated that single file as expected and failed on any other change. The literal Task 2 verify `test -z "$(git status --porcelain)"` prints `?? .planning/state.json` for that reason alone; every other part of that verify passes (`check-ci-yml.sh`, its selftest with 18 cases and 0 failures, and `origin/main == HEAD` at the time of the push).
- **The orchestrator's hash for the `jobs: 4` commit** (`f241fc5`) is not in the pushed history; the same change was committed as `dccfbb8`, which is what the log cites.
- **The SUMMARY, STATE and ROADMAP commit is not pushed.** The plan directs pushes only for the CI-review work; that docs commit is local. The Task 1 verify `origin/main == HEAD` held at the end of Task 2 and fails again once any later local commit exists. Push the planning docs with `[skip ci]` after the phase-close commits.

**Total deviations:** 0 auto-fixed. **Impact:** none on the plan's goal.

## Authentication Gates

None. `gh` was already authenticated with `repo` and `workflow` scopes; both the push and the settings write succeeded on the first call.

## Issues Encountered

None. No hosted run failed, so none of the 5 red rounds were used.

## Known Stubs

None.

## Threat Flags

None. The one new external write (fork-PR approval) is the mitigation for T-01-46, which the plan already lists. T-01-45: pre-push scans clean for every push. T-01-47: the only write call was the single documented PUT, no token written to a file or log. T-01-48: public CI logs were read and carry only tool versions and the banner. T-01-49: no check was weakened.

## Next Phase Readiness

Phase 1 is complete in the plan sense: all 13 plans have summaries, the gate runs verbatim on both hosted runners, and the first-run review is recorded. Open owner-visible items, none blocking: require `gate-linux` and `gate-macos` as checks on main when pull requests start (F-007), `sha_pinning_required` (F-015), workflow lockfiles when a second action appears (F-016). The next CI run for a code-bearing push needs a Runs row via `sh scripts/ci-review.sh <id> --append docs/ci-reviews/LOG.md`.

## Self-Check: PASSED

- FOUND: scripts/ci-review.sh, docs/ci-reviews/LOG.md
- FOUND on origin/main: 79a1301, 49e739b, db2a45f
- Task verifies re-run after the final push: Task 1 verify 1 passed (gate-linux,gate-macos both success for the newest code-bearing commit), Task 1 and Task 2 LOG verify passed (run 42751487155 has its row, 16 findings), fork-PR read-back is `all_external_contributors`, `check-ci-yml.sh` and its selftest passed.
- Acceptance: LOG run row carries numeric queue/run seconds for both jobs and a critical path; `[skip ci]` appears only on the two commits whose diff touches only `docs/ci-reviews/LOG.md`; F-007 mentions the public-repo cost and gating recheck; F-013, F-015 and F-016 mention `all_external_contributors`, `sha_pinning_required` and lockfiles.
