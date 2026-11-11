---
phase: 01-governance-rights-and-harness-foundation
verified: 2026-11-11T00:03:21Z
status: passed
score: 4/4 must-haves verified
covered_files:
  - .github/workflows/ci.yml
  - .planning/phases/01-governance-rights-and-harness-foundation/01-01-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-01-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-02-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-02-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-03-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-03-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-04-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-04-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-05-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-05-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-06-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-06-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-07-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-07-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-08-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-08-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-09-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-09-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-10-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-10-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-11-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-11-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-12-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-12-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-13-PLAN.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-13-SUMMARY.md
  - .planning/phases/01-governance-rights-and-harness-foundation/01-REGRESSION-EVIDENCE.md
  - runner/t64_classify.c
  - scripts/check-ci-yml.sh
  - scripts/gate.sh
  - scripts/rights-scan.sh
  - tools/lint/core-lint.sh
covered_digest: "v4:sha256:f3c117c3b1ec0a151005d006bd081fe3016a18342c46839fe726316b6030ba07"
behavior_unverified: 0
overrides_applied: 0
gaps: []
re_verification:
  previous_status: passed
  previous_score: 4/4
  gaps_closed: []
  gaps_remaining: []
  regressions: []
---

# Phase 1: Governance, Rights and Harness Foundation Verification Report

**Phase Goal:** Before any emulator code exists, the maintainer can run one gate command that enforces the provenance, rights and determinism rules, plus a runner skeleton that classifies every outcome correctly
**Verified:** 2026-11-11
**Status:** passed (human items resolved, see "Human verification resolution")
**Re-verification:** Yes — regression evidence refreshed after Phase 2

The phase goal remains achieved. The initial code verification found no failed truth; its three human items were later resolved as recorded below. This refresh checks the Phase 2 changes that expand the gate and lint coverage, while preserving the original evidence and measurements.

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Accepted ADRs for language/build/ABI (with reversal evidence), CPU timing tiers, extension opcodes, wall-clock timeout, determinism-lint mechanism; copy-provenance policy; claims/evidence-tier template | VERIFIED | ADRs 0001-0007 all `status: accepted`, each with a non-empty `## Reversal evidence` section (3-10 lines). ADR-0002 states C17/CMake 3.25/Ninja/CTest, the spike switch criteria, and an "Evidence limits" section that is present and accurate. `docs/policy/copy-provenance.md` lists ISC, MIT, BSD-3-Clause, Unlicense, the never-copy rule (GPL, Angrylion-Plus), the flat reading ban, the per-copy record fields. `docs/claims/TEMPLATE.md` has six tiers, reserved `synthetic`, the sentence template and the claim-block format. Negative controls run by hand: blanking ADR-0003's reversal section gave `DOC-ADR-STRUCT`; setting ADR-0005 to `proposed` gave `DOC-STATUS-MISMATCH` and `DOC-ADR-NOT-ACCEPTED`. |
| 2 | One gate command locally; CI runs it verbatim on explicit runners with SHA-pinned actions; first CI run has a recorded critical-path review | VERIFIED | `./scripts/gate.sh` exits 0 in 16 s (61 tests). `CI=true ./scripts/gate.sh` exits 0 and prints `CI mode: --require-fixtures`. `.github/workflows/ci.yml` has two jobs, each with the single run line `./scripts/gate.sh`, on `ubuntu-24.04` and `macos-26`. The checkout pin `3d3c42e5...` was confirmed against the upstream tag `actions/checkout@v7.0.1`. Hosted run 42751487155 (SHA 79a1301b8e0d, push) succeeded on both jobs: Linux x86_64 (gcc 13.3, 49/49 tests) and Darwin arm64 (49/49). `docs/ci-reviews/LOG.md` has a Runs row for it. The row's numbers were re-derived from the API and match (wall 36 s, linux queue/run 2/21, macos 2/33, Gate step 27 s). 16 findings are recorded with taken/declined and reasons. Hand mutations of ci.yml (`ubuntu-latest`, an unpinned `@v7`, an extra flag on the run line) each failed `check-ci-yml.sh` by rule id. Fork-PR approval policy reads back `all_external_contributors`. See Warning W1 on the unpushed commits. |
| 3 | Runner skeleton classifies synthetic cases into exactly one of six outcomes; wrong expected value fails; missing, malformed, timed-out, crashed never pass; changed inventory detected | VERIFIED | `tests/controls/matrix.tsv` has 19 live cases covering all six outcomes: pass, guest-fail, boot-fail, timeout, missing-fixture, harness-error. `INVENTORY` pins the 19 ids. Hand mutations in a scratch copy: a changed golden outcome failed `controls.positive`; a changed expected count in the matrix failed `controls.positive`; deleting `spin` from INVENTORY failed `controls.inventory` and `controls.suite`; a one-line mutation of `t64_classify.c` that lets an empty report skip the no-result check failed `unit.classify`, `controls.exit0-no-result`, `controls.suite`, `-dropped`, `-truncated` and `inventory-drift`. ADR-0005 and `docs/spec/result-protocol.md` document the precedence and exit codes. |
| 4 | Gate goes red on each seeded violation: core reads time/files/threads/entropy/mutable global (both layers); protected or unmanifested binary in the tree or in a CI artifact; manifest entry missing a required field | VERIFIED | Core lint: a scratch core file with `#include <time.h>`, `time()`, a mutable global and a mutable static was reported by Layer 1 (`L1-INCLUDE-BANNED`, `L1-GLOBAL-MUTABLE`, `L1-STATIC-MUTABLE`, `L1-NAME-TIME`) and Layer 2 (`L2-W-SECTION`, `L2-UNDEF-SYMBOL`). Placed in the real `core/` of the clone, it made the gate fail at `core-lint`. A macro-hidden global, which Layer 1 cannot see, was still caught by Layer 2. Deleting the only seed for `L1-ASM` made `--selftest` fail with `SELFTEST-RULE-UNPROVEN`. Rights: a committed file with ROM magic plus random bytes failed both the standalone scan (`R-ROM-MAGIC`, `R-NUL-HEAD`, `R-INVALID-UTF8`) and the gate; the same under `* -diff` / `* binary` attributes (CR-12) was still caught; an add-then-delete pair in history was caught by `--history`. Staging leg: `--staging DIR` failed on ROM magic, NUL head and a symlink, passed on clean text, and returned exit 2 (`R-ZERO-SCANNED`) on an empty directory. Manifest: removing each of recipe, revision, sha256, profile, expected, notices and source_rights from the manifest produced `M-SCHEMA-REQUIRED` naming the missing pointer. See Warning W2 on the CI leg of the artifact check. |

**Score:** 4/4 truths verified (0 behavior-unverified; no overrides)

### Additional plan must-haves spot-verified

| Must-have | Status | Evidence |
|-----------|--------|----------|
| Exactly two files under `core/` (CONTEXT A-1), no emulator code | VERIFIED | `git ls-files core` lists only `t64_core_stub.c` and `include/tendies64/t64_facade.h` |
| Rust spike pre-registered before results; ADR-0002 accepted only on "stay C17"; spike frozen | VERIFIED with caveat | PREREG commit `9390f0b` (2026-11-09 21:38) precedes RESULTS `0063abd` (21:39); `run-spike.sh --verify-frozen` reports ok (42 files). The gap between the two commits is one minute and PREREG was later amended (Amendment 1, WR-21), so the git history cannot prove that no result data existed when the rules were frozen. The decision stays stay-c17 and ADR-0002 discloses the weak evidence. |
| Provenance checker | VERIFIED | A copied-looking MIT-headed C file with no record gave `P-UNRECORDED-COPY`. The gate's own scan found 1 copied-looking file (the MADR template) and 1 entry, 0 errors |
| Claims gate | VERIFIED | Synthetic-evidence seed gave `C-EVIDENCE-SYNTHETIC`; a prose line "Tendies64 supports ..." gave `C-RESERVED-WORD`; the good seed passed |
| PII scan | VERIFIED | Home path and a gmail address in a staged file gave `P-PRIVATE-PATH` and `P-EMAIL`, redacted in output. The single history waiver covers exactly 5 hits in commit 097cc7e and nothing else |
| Hosted CI settings | VERIFIED | Repository is public; main has no branch protection (404), so CI does not yet block merges (disclosed as F-007) |

### Artifacts and Key Links

All `must_haves.artifacts` paths exist, are substantive (exercised by the gate, 61 CTest tests across the labels controls 34, gov 13, repo 5, unit 9) and are wired:

| From | To | Status |
|------|----|--------|
| `scripts/gate.sh` | `cmake --preset gate`, `ctest --preset gate`, `core-lint.sh` | WIRED (step list read, gate output confirms each) |
| `.github/workflows/ci.yml` | `scripts/gate.sh` | WIRED (only run line; hosted log shows the gate banner and `[gate] PASS`) |
| `scripts/check-claims.sh` | `t64-result-check identity` | WIRED (C-EVIDENCE-SYNTHETIC fired through it) |
| `scripts/rights-scan.sh` | `t64-blobscan` and manifest `--emit-allowlist` | WIRED (scan output shows manifest checked before allowlist use) |
| `tests/controls/run-suite.sh` | `t64-result-check seal/suite --inventory` | WIRED (INVENTORY mutation turned the suite red) |

### Data-Flow Trace (Level 4)

Not applicable: Phase 1 has no UI and no dynamic rendered data. The equivalent check is that classifier inputs flow from real observations. This was confirmed by the classifier mutation above, where the supervisor-to-classifier path changed the live control results.

### Behavioral Spot-Checks and Probes

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Local gate | `./scripts/gate.sh` | exit 0, 61/61, 16 s | PASS |
| CI-mode gate | `CI=true ./scripts/gate.sh` | exit 0, 61/61, `--require-fixtures` | PASS |
| Linux gate on current HEAD | scratch clone of HEAD in `ubuntu:24.04` (aarch64, gcc 13.3, GNU Awk 5.2.1, cmake 3.28.3) with `CI=true ./scripts/gate.sh` | exit 0, 61/61 | PASS (not the hosted x64 lane) |
| Rights scan | `sh scripts/rights-scan.sh` | tree=326 history=497 findings=0, exit 0 | PASS |
| PII scan | `sh scripts/pii-scan.sh` | tree_files=326 commits=102 findings=0, exit 0 | PASS |
| Spike frozen inputs | `sh docs/spikes/rust-vs-c/run-spike.sh --verify-frozen` | ok (42 files), exit 0 | PASS |
| Probe scripts (`scripts/*/tests/probe-*.sh`) | none declared by the phase | no probes in this phase; the gate's CTest controls are the equivalent | SKIPPED |

### Requirements Coverage

Plan frontmatter IDs were cross-checked against REQUIREMENTS.md and the ROADMAP Phase 1 list. All 11 are accounted for; there are no orphaned requirements mapped to Phase 1.

| Requirement | Verdict | Evidence |
|-------------|---------|----------|
| GOV-01 | SATISFIED | ADR-0002 accepted, default C17/CMake 3.25/Ninja, reversal evidence, evidence limits disclosed |
| GOV-02 | SATISFIED | policy file plus `provenance/copies.json` (one real record) and a checker with rule ids, negative control re-run |
| GOV-03 | SATISFIED | `fixtures/manifest.json` has 3 entries (1 pending, 2 forbidden), with schema and cross-field checker. Every required field is enforced and I confirmed 7 omissions fail. The only real entry has a null digest by design (D2) |
| GOV-04 | SATISFIED | template plus checker and 17 seeded claims. Phase 1 ships no real claims, as the template says |
| GOV-05 | SATISFIED | one gate command, called verbatim by CI, hosted evidence at 79a1301 |
| GOV-06 | SATISFIED | ADRs 0003 to 0006 accepted with reversal evidence |
| GOV-07 | SATISFIED with caveat (W2) | tree, full-history and staging scans are enforced in every gate run, including full-history CI checkout. The CI upload leg is vacuous because Phase 1 CI uploads nothing, and nothing would force a scan if an upload step were added |
| RUN-02 | SATISFIED | six outcomes, one pure classifier, exit codes pinned, 19 live controls. This is a synthetic runner only; it makes no guest claim |
| RUN-04 | SATISFIED | wrong-value, missing, tampered, malformed, hung, crashed and inventory-drift controls all verified by mutation |
| PROF-07 | SATISFIED | two-layer lint, both layers shown red on seeded leaks. Note the live core is a 1-line stub, so real-code coverage begins in Phase 2 |
| VER-04 | SATISFIED | explicit labels, pinned SHA verified upstream, review row verified against the API. Only one CI run exists to review so far |

REQUIREMENTS.md marks all 11 `[x]` and "Complete". That is supported by the evidence above. GOV-07 and VER-04 are the two to read with the caveats.

### Decision Gates (state as of verification)

| Gate | Status | Note |
|------|--------|------|
| D1 language/build/ABI | Closed in Phase 1: ADR-0002 accepted, C17 | One-way once core code merges; revisit window is in the ADR |
| D4 copy allowlist | Recorded as written (ISC, MIT, BSD-3-Clause, Unlicense); MIT-0 and others refused | |
| D2 `nemu64-test` rights | OPEN, owner stop | manifest entry stays `pending`, local-only, no digest, no upload. Does not block Phase 1 |
| D6 release channel | OPEN | blocks releases only |
| D7 name clearance | OPEN | no branding |
| D3, D5 | OPEN, not Phase 1 gates | Phase 3 and Phase 7 respectively |

### Anti-Patterns Found

No TBD, FIXME or XXX marker without a follow-up reference was introduced by the checks I ran. `core/t64_core_stub.c` is an intentional, disclosed placeholder (CONTEXT A-1) and is what keeps the lint from scanning zero files. It is not a goal-blocking stub.

### Disclosed Items: Accuracy Check

| Disclosure | Accurate? |
|------------|-----------|
| 01-REVIEW.md and 01-REVIEW-DISPOSITION.md (37 findings: 4 critical, 19 warning, 14 info) | Yes. The table has 37 rows. Sampled fixes behave as claimed: CR-12 (`-diff` attribute), CR-13 (custom section), the add-then-delete history case, and the `unit.schema-sanitize` guard exists and runs. 5 deferrals and 1 recorded are stated as such |
| ADR-0002 "Evidence limits" | Yes, present, and its claims about the spike parser and the memory-safety bugs match the review |
| WR-06 fixture-digest TOCTOU deferred to Phase 3 | The deferral exists as a "Known limitation (to resolve in Phase 3)" paragraph in `docs/spec/result-protocol.md`. The disposition ledger calls it "recorded as a Phase 3 requirement", but it is not in ROADMAP Phase 3 success criteria or in REQUIREMENTS.md (W3) |
| PII history waiver for 097cc7e | Yes. One row, count 5, applies only to that commit and path, and the scan rejects any other count. The commit is unpushed, as stated |
| CI staging-scan step deferred | Yes, stated in `docs/policy/rights-and-fixtures.md` (see W2) |
| Hosted run green for 79a1301 on ubuntu-24.04 x64 and macos-26 | Yes, confirmed with `gh`. It is the only run that exists |
| Later local commits not yet through hosted CI | Yes. `git rev-list origin/main..HEAD` is 33 commits; origin/main is db2a45f (two `[skip ci]` doc commits past 79a1301). Hosted CI has never run any of the review fixes |

### Warnings (non-blocking)

- **W1. Unpushed work has no hosted CI evidence.** 33 commits, including every code-review fix and the move from 49 to 61 tests, exist only locally. The compensating evidence is a green local macOS gate, a green `CI=true` gate, and a green `CI=true` gate in an ubuntu:24.04 aarch64 container. It is not hosted and not x86_64. CR-13 changed Layer 2 to allow sections by name per object format, so an x64 ELF run is the thing that has not been seen. Listed as a human item.
- **W2. GOV-07's "uploaded artifacts" leg is a capability, not a CI control.** `rights-scan.sh --staging` works (verified red and green by hand) and is exercised by seeded cases in every gate run, matching the disclosure. But `check-ci-yml.sh` does not reject an `actions/upload-artifact` step: I added one to a copy of ci.yml and the checker printed `ok`. `CI-RUN-LINE` permits only `./scripts/gate.sh` as a run line, so a later upload step could not add a scan step without editing the checker, but nothing forces that edit. Suggested follow-up: a `CI-UPLOAD-ARTIFACT` rule that fails any upload step until a staging-scan step exists. Not a Phase 1 blocker, since today nothing is uploaded.
- **W3. WR-06 is not tracked in the roadmap.** Add the digest-handoff rule to Phase 3 (RUN-01) success criteria or requirements so it cannot be lost, because `fixture_digest` is meant to become the identity claim there.
- **W4. `Mode: mvp` with a non-story goal.** Reported above as a human item.
- **W5. Main has no branch protection.** CI results do not gate merges. Already disclosed as F-007 and left as an owner repository setting.

### Info

- `.planning/STATE.md` is stale: progress shows 0%, completed plans 0 (the frontmatter says 13), and last activity names plan start. `01-VALIDATION.md` still says `status: draft`, `nyquist_compliant: false`. ROADMAP Phase 1 checkbox is unchecked. These are bookkeeping for the orchestrator at phase close, not code issues.
- `.planning/state.json` is untracked and has no private paths or email addresses in it.
- Deferred review items IN-03, IN-04, IN-14, IN-24 and IN-25 remain open and are recorded in the disposition ledger.

### Human Verification Items at Initial Pass (resolved below)

1. **Reword or accept commit 097cc7e before the first push.** The waiver becomes permanent public history once pushed. Expected: an owner decision, then either a reworded commit with the waiver row removed, or an accepted waiver.
2. **Push and review the next hosted run.** Expected: both jobs green with 61 tests and a new row in `docs/ci-reviews/LOG.md`.
3. **Confirm the verification mode.** Drop `Mode: mvp` or rewrite the goal as a user story.

### Gaps Summary

No gaps. All four roadmap success criteria and all 11 requirement IDs were supported by the original evidence. The three initial human items were resolved below. The five warnings remain recorded as follow-ups and do not reduce the goal.

---

_Verified: 2026-11-11_
_Verifier: Claude (gsd-verifier)_


## Human verification resolution (2026-11-10, orchestrator)

The three items that made the first verdict `human_needed` were resolved as follows. None was a code gap.

1. **Commit 097cc7e and the PII history waiver:** the waiver is kept. Rewriting the unpushed history would change
   every later commit hash and orphan the hashes cited in the plan summaries, the review ledger, ADR-0002 and
   RESULTS.md. The waiver is narrow (one commit, one file, one rule, a hit count of exactly 5, never applied to the
   tree, commit messages or metadata) and can be deleted by the owner. It is now in public history. Owner-visible.
2. **Hosted CI on the post-review commits:** pushed (`db2a45f..f02621a`, 35 commits) and run 42764219989 is green on
   gate-linux (ubuntu-24.04 x64, 17 s) and gate-macos (macos-26, 50 s), 61 tests. Its Runs row and finding F-018 are in
   `docs/ci-reviews/LOG.md`. This confirms CR-13 section handling and the new checks on hosted x64 ELF and macOS.
3. **Mode mismatch:** the `**Mode:** mvp` lines were removed from every phase in ROADMAP.md (an auto-mode default that
   does not fit infrastructure and emulator goals). Standard goal-backward verification stands.

Also applied after the verifier ran: verifier warning W2 (check-ci-yml.sh now refuses artifact-upload steps, seeded
case, selftest 38/38) and W3 (WR-06 added to Phase 3 success criterion 6). W5 (main has no branch protection) and the
D2/D6/D7 gates remain open owner items. The covered-input digest was regenerated with `verification.fingerprint` after
the LOG.md row was appended.

## Regression Re-check (2026-11-11)

The original four roadmap criteria remain satisfied. The phase-owned
[regression evidence snapshot](01-REGRESSION-EVIDENCE.md) records the bounded
diff review and Phase 2 hosted run evidence. The original Phase 1 run records
and initial measurements above are unchanged.

| # | Truth | Status | Current evidence |
|---|---|---|---|
| 1 | Governance, provenance and claims rules remain recorded | ✓ VERIFIED | The Phase 1 ADRs, copy-provenance policy and claims template are unchanged from baseline. |
| 2 | One gate still runs locally and CI invokes it on the reviewed runners | ✓ VERIFIED | The prior gate stages remain; inventory checks are added before CTest. The exact Phase 2 run passed both hosted lanes. |
| 3 | Synthetic runner classification and inventory controls remain effective | ✓ VERIFIED | The classifier is unchanged; the exact inventory includes all 61 Phase 1 names plus 12 Phase 2 names, and the hosted gate passes. |
| 4 | Seeded rights, manifest and deterministic-core violations remain detectable | ✓ VERIFIED | The rights scanner is unchanged; source/object lint is retained and expanded to shared SHA source. The hosted gate passed and the final local lint record reports 15 sources with zero violations. |

**Regression score:** 4/4. No new gap or human-verification item was found. The
current fingerprint replaces coverage of the shared append-only CI log with a
snapshot of the exact run evidence; historical Phase 1 CI provenance links
remain in this report.
