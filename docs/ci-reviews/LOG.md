# CI critical-path review log

Phase-1-base: 57227793345c59259ab6eac12d1044bc408dfd60

This log is append-only (VER-04). Rows are added at the end of a table by
`scripts/ci-review.sh RUN_ID --append docs/ci-reviews/LOG.md`, and findings with
`--finding "id|finding|taken/declined|reason"`. An existing row is never edited
or removed. A mistake is corrected by a new finding row that names the old one.

`Phase-1-base` is `git rev-parse origin/main` recorded before the first push of
Phase 1. The pushed SHAs of the phase are `git rev-list <Phase-1-base>..origin/main`.

## Review policy

- Every completed CI run for any pushed SHA gets one Runs row, red runs
  included (AUTONOMY section 2: inspect measured critical-path delays after
  every CI run).
- The first run's row is the minimum the Phase 1 gate (VER-04) requires.
- A dated deferral row is allowed only when a run's data cannot be read. It
  keeps the same leading `| date | run id |` columns and states the reason in
  the last column.
- A commit that changes only this file carries `[skip ci]` in its message, so
  recording a review never triggers a run that would itself need a review.
  Every code-bearing commit triggers a run, and that run is reviewed. The rights
  and PII scans still run locally before each push.
- A finding is resolved as `taken` (applied, with the distinct evidence kept
  intact) or `declined` (with the reason), so it is not rediscovered at every
  milestone.

## Columns

Runs: `date (UTC)` is the run's creation date. `wall s` is the run's
`created_at` to `updated_at`. For each job, `queue` is the job's `started_at`
minus `created_at` and `run` is `completed_at` minus `started_at`, both in
seconds, written `queue/run`. `critical path s` is the maximum over jobs of
queue plus run. `slowest step` is the longest single step across all jobs.

Findings: `id` is `F-NNN` and unique. `run id` is the run the finding was read
from.

## Runs

| date (UTC) | run id | sha (12) | event | conclusion | wall s | critical path s | gate-linux queue/run s | gate-macos queue/run s | slowest step |
|---|---|---|---|---|---|---|---|---|---|
| 2026-11-10 | 42751487155 | 79a1301b8e0d | push | success | 36 | 35 | 2/21 | 2/33 | gate-macos/Gate 27s |

## Findings

| id | run id | finding | taken/declined | reason |
|---|---|---|---|---|
| F-001 | 42751487155 | Queue versus run: queue was 2 s on both jobs against 21 s (gate-linux) and 33 s (gate-macos) of run time. Wall 36 s, critical path 35 s on gate-macos. GitHub annotated the macOS job with a capacity notice that macOS arm64 queues may be longer. | declined | Queue is under 10 percent of the critical path, so there is nothing to change. Recheck if any queue exceeds 30 s; a queue over 30 minutes is a blocker (plan 01-13). |
| F-002 | 42751487155 | Duplicate setup or builds: each job does one checkout, one configure and one build. No work repeats across jobs, which run on different operating systems and toolchains. Set up job and Complete job take 0 to 1 s. | declined | No duplicate to remove. The two builds are the distinct evidence. |
| F-003 | 42751487155 | fetch-depth 0 cost: checkout with full history took 1 s (gate-linux) and 2 s (gate-macos) for about 70 commits. | declined | The gate needs full history: pii-scan and rights-scan read all reachable history and report P-SHALLOW on a shallow clone. Cost is negligible. Recheck when history passes a few thousand commits. |
| F-004 | 42751487155 | Caching: none is used. Configure plus build is 2 s on gate-linux and 3 s on gate-macos. | declined | CONTEXT says no caching in Phase 1, and a cache restore would cost about as much as the build it saves. Revisit when build time exceeds the restore cost. |
| F-005 | 42751487155 | Concurrency: group ci-ref with cancel-in-progress only for pull requests. This push-to-main run overlapped no other run, so cancellation was not exercised. | declined | The design keeps a verdict for every pushed SHA, which the review policy relies on. check-ci-yml.sh asserts the setting. No change. |
| F-006 | 42751487155 | Matrix cost: two parallel jobs, 21 s and 33 s of run time. gate-linux is x86_64 with gcc 13.3, GNU Awk 5.2.1 and cmake 3.31.6. gate-macos is arm64 with Apple clang 21, BWK awk 20200816 and cmake 4.4.5. | declined | Neither job can be dropped without losing a distinct failure class (architecture, compiler, awk dialect, libc, cmake major version). No change. |
| F-007 | 42751487155 | Public-repo CI cost and gating recheck (ROADMAP Phase 1 gate). The repository is public (read back). GitHub billing documentation states standard GitHub-hosted runners are free in public repositories, and both jobs use standard runners (no larger runners). main has no branch protection and no rulesets (404 and 0), so both jobs run on pull requests but do not yet block a merge. | declined | Cost: no action, standard runners only. Gating: requiring the checks gate-linux and gate-macos on main is a repository-setting change outside the one setting this plan is authorized to write, so it stays an owner-visible item to enable when pull requests start. |
| F-008 | 42751487155 | Assumption A8 (arm64 Linux with emulated x86-64 gcc represents the x64 runner): gate-linux ran on real x86_64 and passed all 49 tests with GNU Awk 5.2.1 and cmake 3.31.6, which the local runs never exercised. | declined | Assumption confirmed by the first hosted run. No change; A8 is closed. |
| F-009 | 42751487155 | Assumption A9 (macos-26 awk and sh match the local ones): the runner reported awk version 20200816, the same as the dev host, and all awk-based selftests (rights, pii, provenance, claims, docs) passed. The gate banner does not print the sh or bash version. | declined | Confirmed for awk and for the POSIX sh scripts. No change. |
| F-010 | 42751487155 | ctest jobs 4 (commit dccfbb8, measured on the dev host as 28 s serial against 8 s at 4 jobs). Hosted: ctest real time 8.6 s (gate-linux) and 15.9 s (gate-macos) while the 49 per-test times sum to 32.8 s and 63.0 s, so 4 jobs gives up to about 3.8 times and 4.0 times (the per-test times were taken under load, so these ratios are upper bounds). | taken | Confirmed on both hosted runners and kept. The longest single test is 4.7 s on linux and 9.1 s on macOS (gov.provenance.selftest), which is the floor for more parallelism. |
| F-011 | 42751487155 | Earlier serial gate timings on the dev host were 31 to 79 s, dominated by ctest running the awk-based selftests and four controls.suite tests of about 4.7 s each one after another. | taken | Fixed by jobs 4 in the gate test preset (F-010). The gate now takes 15 s (gate-linux) and 27 s (gate-macos) hosted, and 17 to 32 s on the dev host depending on load. |
| F-012 | 42751487155 | Critical path and slowest step: the critical path is gate-macos at 35 s (queue 2 s, run 33 s). Its slowest step is Gate at 27 s, of which ctest is 16 s and lint-selftest 5 s. The longest macOS tests are the awk-based selftests (gov.provenance 9.1 s, gov.rights 8.5 s). | declined | Far under timeout-minutes 10. The selftests are the negative controls, so removing them would lose evidence. Revisit if gate-macos exceeds 120 s. |
