# Phase 11 independent review record

One section per phase-wave, written by the review plans (11-15 for W0 and the SHAPE wave, 11-21 for
the INSTALL wave, 11-25 for the final audit). Each review reads the wave's diff fresh in a scratch
clone of the committed head under `build/11-review/` (never in the shared build tree: no
`ninja -t clean`, no fixture rebuild, no mutant run there) and re-runs the cited commands instead of
trusting the plan summaries. Evidence tier: code observation and command results at the commit named
in each section; a software-interface claim only, nothing here is a compatibility, accuracy or hardware
claim. No oracle material (D5) was used. All paths are repo-relative.

## SHAPE (11-01 to 11-14)

W0 (plan 11-01, records only) and the SHAPE wave (plans 11-02 to 11-14). Reviewed by plan 11-15.

| Item | Value |
|------|-------|
| Wave 0 base | `f9124ae9fce134f31b645c6f2911a6f27fa41b9a` (extracted from PIN-IMPACT.md "Wave bases" with the base idiom: exactly one 40-hex line, an existing commit, an ancestor of the head) |
| Reviewed head | `1d451b9b250f3707d8d055c590cc5e87bcff045d` (74 commits after the base; 158 files changed, 138 of them outside `.planning/`) |
| Host | Darwin arm64, Apple clang 21.0.0, cmake 4.4.3, ninja 1.13.2; 1-minute load 5 to 12 during the scratch gate (unrelated processes on the same machine, so seconds are upper bounds for a quiet host) |

### Task 1: scratch gate of the SHAPE head

Method (the Phase 10 W0-01 form): `git clone --no-checkout --no-hardlinks` of the main repository into
`build/11-review/shape/src`, `git checkout --detach 1d451b9b250f3707d8d055c590cc5e87bcff045d`, and a copy
(not a symlink) of `build/tools/install` into the clone's `build/tools/install`. `git archive HEAD` of the
clone and of the main repository have the same SHA-256 (`94296d41599a91b2d1c5b9d9a01b4b09d7deb98ea6dc1c7e88a62d92a7eee727`),
so the gated tree is exactly the committed head; `git status --short` of the clone is empty. The gate
output was written outside the clone tree (`build/11-review/shape/gate.out`, ignored, not committed), so
`gov.pii.repo` sees no untracked output file (the 11-14 lesson).

Command: `sh scripts/gate.sh` inside `build/11-review/shape/src`, default environment (no `CI` variable).

Result: `[gate] PASS`, `[gate] total 162s`, exit 0. Steps in seconds: sh-n 0 (35 scripts), lint-selftest 5,
fixture-qualification 1, configure 1, build 5, boot-evidence-selftest 14, test-inventory-selftest 34,
test-inventory 1, ctest 92 (`100% tests passed out of 275`, `Total Test time (real) = 91.39 sec`), core-lint 9.
No name was excluded; `harness.rdp-oracle-authority` and `harness.phase10-qualification` pass (the
identity block of 11-12 to 11-14 is cleared by the bound commit). No limit was widened.
