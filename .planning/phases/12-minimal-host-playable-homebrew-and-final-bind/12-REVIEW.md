# Phase 12 independent review record

Independent reviews of the Phase 12 waves (AUTONOMY v1.2: review per wave in a scratch clone, plus
the final audit). Each section is written by its review plan: "TITLE" by 12-06 (the W0 plan 12-01
with its Wave 0 checklist and the TITLE wave 12-02 to 12-05), "HOST" by 12-12 and "FINAL" by 12-18.
The reviewer fixes nothing: a finding gets a severity (blocking stops the plan before the push,
warning is a real gap that does not block the push, info is a recorded observation or accepted
limit) and a disposition; a fix beyond the plan's own list is a new numbered plan appended after
12-18. Evidence tier: unit-tested, authored scope; a software-interface claim only, with no
compatibility, accuracy, hardware, real-time, latency or throughput claim. Logs stay under the
ignored `build/12-review/`; only sanitized lines are quoted here.

## TITLE (12-01 Wave 0 checklist and 12-02 to 12-05)

| Item | Value |
|------|-------|
| Wave 0 base | `60b9e3c34fc0efc0ff64296cfa31415565b847b2` (extracted from PIN-IMPACT.md "Wave bases" with the base idiom: exactly one 40-hex line, an existing commit, an ancestor of the head) |
| Reviewed head | `9ea70f769ca90f69206eeb1cf5db1660bfde9272` (28 commits after the Wave 0 base; 37 commits after `origin/main` `dc247fa`, the Phase 11 closeout and Phase 12 planning commits included; 29 files changed outside `.planning/`) |
| Host | Darwin arm64, Apple clang 21.0.0, cmake 4.4.3, ninja 1.13.2; 1-minute load 6.8 at the start of the scratch gate and 12 to 17 at its end (unrelated processes on the same machine: a virtual machine and a desktop application), so the seconds are upper bounds for a quiet host; no limit was widened |

### Task 1: scratch gate and preflight of the W0 and TITLE head

Method (the 11-21 form): `git clone --no-checkout` of the main repository into
`build/12-review/title/src`, `git checkout --detach 9ea70f769ca90f69206eeb1cf5db1660bfde9272`, and a copy
(not a symlink) of `build/tools/install` into the clone's `build/tools/install`. `git archive` of the head
in the clone and in the main repository have the same SHA-256
(`4d2d594c516c12d6f6cda3af905ca0991b4735b0281f5de23ef96b96a1fa2738`), so the gated tree is exactly the
committed head; `git status --short` of the clone is empty before and after the gate. The clone's own
`build/gate` holds the title fixtures, the install fixture (`install.setup`), the consumer builds and the
control scratch directories (its CMake home directory is the clone); the gate output was written outside the
clone tree (`build/12-review/title/gate.out`, ignored, not committed).

Command: `sh scripts/gate.sh` inside `build/12-review/title/src`, default environment (no `CI` variable).

Result: `[gate] PASS`, `[gate] total 236s`, exit 0. Steps in seconds: sh-n 1 (40 scripts), lint-selftest 8,
fixture-qualification 2, configure 0, build 22, boot-evidence-selftest 19, test-inventory-selftest 48,
test-inventory 1, ctest 124 (`100% tests passed out of 336`, `Total Test time (real) = 124.52 sec`),
core-lint 11. No name was excluded and no limit was widened. Phase 12 names: `ctest -N -L phase12 -FA .`
lists 7 (`harness.title-oracle`, `runner.title-two-run`, `controls.real.title-wrong-input`,
`controls.real.title-refused-frame`, `installed.consumer.title`, `installed.consumer.title-persist`,
`installed.consumer.title-refused`), equal to the `PHASE12_TESTS` block; the preset has 336 names, 7 more
than the 329 of the Phase 11 close. Their seconds in this gate run (parallel, loaded host): 3.44, 3.01, 0.51
(the three consumer names), 0.70, 0.44, 0.52 (the runner name and the two controls) and 0.10 (the oracle),
8.72 s summed.

Preflight: `sh scripts/preflight-phase12.sh --no-replay` in the clone, `[preflight] PASS`, total 51 s:
oracle-authority (blocked as required), ctest-counts 23 rows 0 mismatches, `phase12 label: 7 tests carry the
label phase12, matching the PHASE12_TESTS block`, abi-cheap 16 names 5 s, host-cheap 1 name
(`harness.title-oracle`), guards (check-api-init.py, check-consumer-includes.sh, check-stale-freeze-text.sh
passed; check-host-includes.sh not yet present, as expected before plan 12-07), host-load WARN at 9.3,
head-ci `origin/main` `dc247fa` completed success, timeout-audit 189 TIMEOUTs 0 too low (lowest ratio 2.9),
pii (1483 tree files, 37 commits, 0 findings), linux-compile 234 units with the act image cc 13.3.0 (42 s;
first C-relevant path `core/CMakeLists.txt`), post-gate-replay skipped by `--no-replay`. A first attempt
in the same clone exited 1 at the pii step (`RULE P-ZERO-SCANNED history`, 0 commits) and printed
`head-ci: gh unavailable`: a clone of the local repository has the source repository as `origin`, so
`origin/main` equals the head and the range `origin/main..HEAD` is empty. The clone's fetch URL was set to
the public repository (push URL disabled) and `origin/main` fetched (`dc247fa`); the rerun passed. This is a
review-procedure fact, not a defect of the preflight (finding T-05).
