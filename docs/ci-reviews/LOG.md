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
| 2026-11-10 | 42764219989 | f02621a362ea | push | success | 58 | 57 | 6/17 | 7/50 | gate-macos/Gate 43s |
| 2026-11-10 | 42922365743 | - | - | deferred | - | - | - | - | deferred: Cancelled redundant manual dispatch before jobs started; push run retained |
| 2026-11-10 | 42922317641 | 5b277e93be62 | push | failure | 78 | 78 | 2/9 | 3/75 | gate-macos/Gate 67s |
| 2026-11-10 | 42922766163 | d32a1ba15b02 | push | failure | 84 | 82 | 2/12 | 3/79 | gate-macos/Gate 70s |
| 2026-11-10 | 42923015327 | 32de4f7b664c | push | failure | 82 | 80 | 1/23 | 2/78 | gate-macos/Gate 70s |
| 2026-11-10 | 42925122280 | 34b43f760e08 | push | success | 205 | 204 | 2/38 | 146/58 | gate-macos/Gate 52s |

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
| F-013 | 42751487155 | Fork-PR workflow approval policy (A4, threat T-01-46): read first_time_contributors, set with PUT actions/permissions/fork-pr-contributor-approval and approval_policy=all_external_contributors (the assumed body field was accepted), read back all_external_contributors. No other repository setting was touched. | taken | Protective and locked by CONTEXT: every outside contributor's workflow run needs approval. The workflow also has permissions {} at top level, contents read per job and no secrets. Before: first_time_contributors. After: all_external_contributors. Assumption A4 is closed. |
| F-014 | 42751487155 | ci.yml changes from the findings above: none of F-001 to F-012 calls for a workflow change. | declined | No taken finding changes .github/workflows/ci.yml, so it stays untouched and check-ci-yml.sh and its selftest still pass. The one taken change (jobs 4) is in CMakePresets.json and was already measured. |
| F-015 | 42751487155 | sha_pinning_required (repository-level enforcement that every action reference is a full commit SHA) is false. | declined | Owner decision. The rule is not in CONTEXT; the one action in use is already pinned to a full SHA and check-ci-yml.sh asserts it. Enabling the setting is a further repository-setting change for the owner to make. |
| F-016 | 42751487155 | Workflow dependency lockfiles are not used. | declined | Owner decision, deferred by CONTEXT. The workflow has one SHA-pinned action and no third-party actions, so a lockfile would add maintenance without changing what is pinned today. Revisit when a second action or a cache action is added. |
| F-017 | 42751487155 | Sanitizer lane cost, measured on the dev host (Apple M5 Pro, 18 cores) at commit ef908b9, not on a hosted run: `ctest --preset asan -j4` takes 44 to 45 s wall for all 61 tests (cold configure and build under 1 s), against 12 s for `ctest --preset gate -j4` and 15 to 17 s for the whole gate. The time is in six shell-driven selftests that run ASan-built tools many times: gov.rights.selftest 22 s, controls.suite-truncated, controls.suite-dropped and controls.inventory-drift about 20.5 s each, controls.suite 18.7 s, gov.provenance.selftest 17.6 s. The same lane takes 23 s at -j8, and the other 54 tests take 11 s at -j4. The asan preset is not a gate step today. In Phase 1, code review, not the gate, found the memory-safety bugs CR-01 and WR-07. | declined | Over the 25 s budget: the full lane would add about 45 s to scripts/gate.sh on this host, about 3 times the gate, and more on the hosted runners, which have fewer cores. Not added. Recommendation: add an asan gate step that excludes the six slow selftests (11 s here) and keep the full asan lane as a local and pre-release check, or cut the per-case cost of those selftests and re-measure. Any new gate step is confirmed only by the next hosted CI run. |
| F-018 | 42764219989 | Run 42764219989 (35 commits: review fixes, 61 tests): gate-linux 17 s, gate-macos 50 s, wall 58 s, critical path gate-macos (Gate step 43 s versus 27 s in the first run). The macOS leg is now the critical path because shell selftests grew (rights-scan 87 cases, provenance 95, claims 49) and macOS runners have fewer cores for ctest jobs 4. | declined | No change now: the measured critical path (57 s) is still under the 60 s target and no distinct failure class is being added or removed. Revisit at the next review if macOS exceeds 60 s: candidates are splitting the six slowest selftests into a scheduled lane and a gate step that runs the asan preset without them (F-017). Hosted runs confirmed the review fixes (CR-13 section handling, gawk 5.2.1 on x64 ELF) with no platform-specific failure. |
| F-020 | 42922365743 | Actions run listing lag prompted redundant manual dispatch | taken | Cancelled pending duplicate; query REST exact-head state before any future dispatch |
| F-021 | 42922317641 | Linux GCC rejected a provably false mixed-width overflow comparison | taken | Preserve checked addition using size_t intermediate; retain existing two lanes and strict warnings |
| F-022 | 42922766163 | GCC exposed two test-only warning errors after production build passed | taken | Remove unused shared test header and parenthesize expected masked arithmetic; retain strict warnings and focused retests |
| F-023 | 42923015327 | Run 42923015327 measured 82 s wall and an 80 s critical path: Linux x86_64 GCC 13.3 fails at test compile after production core build; macOS arm64 Apple Clang completes all 73 tests. These remain distinct OS/toolchain checks. | declined | Keep both existing lanes. The Linux format-truncation diagnostic is in test path construction, so caching, added jobs, or concurrency changes would not address it; the gate duration does not justify those changes. |
| F-024 | 42925122280 | Run 42925122280 succeeded on both distinct OS/toolchain lanes. Measured Linux queue/run was 2/38 s; macOS was 146/58 s, so the 204 s critical path was dominated by macOS queue time, not gate execution. | declined | Keep both compiler/platform checks. The long macOS wait is a queue delay, while its gate took 58 s; caching cannot reduce queue time and no new job or concurrency change is justified by this run. |
