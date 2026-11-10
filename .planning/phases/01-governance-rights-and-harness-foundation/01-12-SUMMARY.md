---
phase: 01-governance-rights-and-harness-foundation
plan: "12"
subsystem: governance
tags: [posix-sh, awk, claims, evidence-tiers, synthetic-refusal, ctest, seeded-cases, gov-04]
status: complete

requires:
  - phase: 01-governance-rights-and-harness-foundation
    provides: "t64-result-check identity and the suite goldens (01-07), run-expect.sh (01-05), the tools glob and gate (01-01), the writing rule for docs (01-04), the profile id format (01-09)"
provides:
  - "docs/claims/TEMPLATE.md: tier enum with the reserved synthetic tier, reserved-word rule, claim-block keys, rendered sentence with boot-fail (A-4), per-tier phrases, two checked examples"
  - "scripts/check-claims.sh [--root DIR] [--files F...] [--selftest]: POSIX sh + awk claims gate (A-5) with 14 rule ids"
  - "Evidence refusal through t64-result-check identity: synthetic results, profile/build/pass mismatch, invalid or unsafe paths"
  - "tests/seeded/claims (17 seeds plus EXPECT.tsv), directory-owned (A-6)"
  - "CTest gov.claims.selftest, gov.claims.repo, gov.claims.seeds (label gov)"
affects: [01-13]

actuals:
  tokens: 13600
  tasks: 2
  commits: 2
plan_head_before: b5a23741a2aeafa8239dace338cf9a342ce2500d
plan_head_after: 6b82d984c7de163c62541b0170ec10420979883a

tech-stack:
  added: []
  patterns:
    - "One awk program parses every file (file list read inside BEGIN, so an odd file name is never taken for an awk assignment), writes RULE lines, evidence requests and counts to scratch files; the shell then runs the C tool per evidence request, so no pipeline masks a status"
    - "The selftest builds valid claim files from variables (mkclaim) and overrides one field per case in a subshell; evidence cases use a temporary root with --root"
    - "Rules that need the built C tool or a temporary root are proven by selftest cases; rules reachable from one static file are proven by seeds"

key-files:
  created:
    - docs/claims/TEMPLATE.md
    - scripts/check-claims.sh
    - tools/claims/CMakeLists.txt
    - tests/seeded/claims/EXPECT.tsv
    - tests/seeded/claims/good_self_checking.md
    - tests/seeded/claims/good_hardware_scope.md
    - tests/seeded/claims/bad_tier.md
    - tests/seeded/claims/bad_synthetic_tier.md
    - tests/seeded/claims/bad_synthetic_profile.md
    - tests/seeded/claims/bad_synthetic_build.md
    - tests/seeded/claims/bad_missing_field.md
    - tests/seeded/claims/bad_unknown_field.md
    - tests/seeded/claims/bad_counts.md
    - tests/seeded/claims/bad_reserved_in_block.md
    - tests/seeded/claims/bad_reserved_prose.md
    - tests/seeded/claims/bad_statement.md
    - tests/seeded/claims/bad_emulator_missing.md
    - tests/seeded/claims/bad_hw_no_scope.md
    - tests/seeded/claims/bad_id_format.md
    - tests/seeded/claims/bad_unterminated.md
    - tests/seeded/claims/bad_evidence_synthetic.md
  modified: []

key-decisions:
  - "synthetic is reported only as C-SYNTHETIC, never also as C-TIER-ENUM; a synthetic evidence suite suppresses the mismatch check, so the refusal is the single reported rule"
  - "An empty required value is C-FIELD-MISSING, except oracle_kind and oracle_revision, where an empty value is C-ORACLE (an absent key is still C-FIELD-MISSING)"
  - "A conditional key on the wrong tier (scope on a smoke claim) is C-FIELD-UNKNOWN; hardware-derived needs a non-empty scope and capture_provenance, emulator-differential needs both emulator keys"
  - "Evidence must end with a t64.suite record in addition to passing identity, because identity treats the suite record as optional (see Deviations)"
  - "The statement is checked only when every required key is present and the tier is claimable, so a bad seed reports one rule rather than a cascade"
  - "Prose rule strips leading whitespace, quote and list markers before testing for the project-name prefix, so a bullet or a blockquote cannot hide a claim"

patterns-established:
  - "Every fence is tracked (claim, example, other); an unterminated fence of any kind is C-BLOCK-SYNTAX, so an open example block cannot hide a real claim below it"
  - "Seed files carry a one-line header naming the rule they exercise; EXPECT.tsv names the rule id and a message needle"

requirements-completed: [GOV-04]

coverage:
  - id: D1
    description: "The claims template defines the tier enum, the reserved synthetic tier, the reserved words, the claim-block keys and the rendered sentence including boot-fail, with two fully valid examples that the selftest re-checks as real blocks"
    requirement: "GOV-04"
    verification:
      - kind: integration
        ref: "ctest --preset gate -R gov.claims.selftest (template has 2 examples; both pass as t64-claim blocks); grep -c t64-claim-example docs/claims/TEMPLATE.md = 3; grep -c boot-fail = 4"
        status: pass
    human_judgment: false
  - id: D2
    description: "The gate validates ids, tier, counts, oracle fields, the rendered statement and the reserved words, each rule with a failing seed or selftest case"
    requirement: "GOV-04"
    verification:
      - kind: integration
        ref: "ctest --preset gate -R gov.claims.seeds (run-expect rows=17 pass=17 fail=0) and gov.claims.selftest (cases=49 failures=0)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Synthetic tiers, profiles, builds and evidence suites containing synthetic results are refused end to end through t64-result-check identity"
    requirement: "GOV-04"
    verification:
      - kind: integration
        ref: "sh scripts/check-claims.sh --files tests/seeded/claims/bad_evidence_synthetic.md exits 1 with C-EVIDENCE-SYNTHETIC; seeds bad_synthetic_tier, bad_synthetic_profile, bad_synthetic_build"
        status: pass
    human_judgment: false
  - id: D4
    description: "The repository scan finds no claim blocks and no flagged existing document; the claim seeds and .planning are excluded"
    requirement: "GOV-04"
    verification:
      - kind: integration
        ref: "sh scripts/check-claims.sh prints claims: files=17 blocks=0 errors=0 and ok (gov.claims.repo); selftest cases for the exclusions"
        status: pass
    human_judgment: false

duration: 16min
completed: 2026-11-10
---

# Phase 1 Plan 12: Claims template and gate Summary

**`scripts/check-claims.sh` (POSIX sh + awk) validates claim blocks against `docs/claims/TEMPLATE.md` by 14 rule ids, refuses synthetic tiers, profiles, builds and evidence suites (through `t64-result-check identity`), and is proven by 17 seeded claim files and a 49-case selftest registered as three CTest tests.**

## Accomplishments

- Tracer slice end to end first: template, checker, evidence refusal via the C tool, CTest, verified before the seeds were added.
- `docs/claims/TEMPLATE.md`: the unordered tier enum plus reserved `synthetic`, reserved words, the key list with conditional keys, the rendered sentence with `boot-fail` (CONTEXT A-4), the per-tier phrases, what "Not claimed" must cover, evidence rules, scan exclusions, and two example blocks fenced `t64-claim-example` (self-checking-fixture and hardware-derived with a scope).
- `check-claims.sh`: C-BLOCK-SYNTAX, C-FIELD-MISSING, C-FIELD-UNKNOWN, C-TIER-ENUM, C-SYNTHETIC, C-ID-FORMAT, C-COUNTS, C-ORACLE, C-STATEMENT, C-RESERVED-WORD, C-EVIDENCE-SYNTHETIC, C-EVIDENCE-MISMATCH, C-EVIDENCE-INVALID, C-ZERO-INPUT. Exit 0 clean, 1 findings, 2 harness error. No regex interval expressions, so it runs on BWK awk and mawk.
- Evidence path: lexical check in awk (repo-relative, no `..`, no empty or dot segment, charset `A-Za-z0-9._/-`), then per-component symlink check and a regular-file check in sh, then `t64-result-check identity`. A non-zero exit is C-EVIDENCE-INVALID; any synthetic result is C-EVIDENCE-SYNTHETIC; a differing profile, build or pass count is C-EVIDENCE-MISMATCH.
- `--selftest` re-checks each template example as a real block, checks the synthetic suite `tests/seeded/suites/good.jsonl` is refused, derives a non-synthetic suite from a golden (sed on profile, build and tier, plus a suite record) in a temporary root to prove the match, the three mismatches and twelve invalid-evidence cases, and covers the rules the seeds cannot reach (C-ORACLE, C-ZERO-INPUT, scan exclusions, wrong-tier conditional keys, profile length 64 vs 65).
- Seeds: 17 files, each bad one violating one rule id. `good_hardware_scope.md` uses `accurate` inside a hardware-derived block with a scope to prove the allowance.

## Measured results

| Item | Value |
|------|-------|
| `sh scripts/check-claims.sh` on the repository | `claims: files=17 blocks=0 errors=0`, `ok`; no existing document was flagged, so nothing outside this plan needed editing |
| `ctest --preset gate -R 'gov\.claims'` | 3 of 3 passed (selftest about 2.4 s, repo 0.05 s, seeds about 0.8 s) |
| `check-claims.sh --selftest` | `cases=49 failures=0` (macOS sh/BWK awk, and dash on the host) |
| `run-expect` over `tests/seeded/claims` | `rows=17 pass=17 fail=0`; every bad seed prints exactly one distinct rule id (two C-FIELD-MISSING lines for the missing emulator keys and two C-RESERVED-WORD lines for the block that repeats the word in `not_claimed` and `statement`) |
| Acceptance: `ls tests/seeded/claims/*.md \| wc -l` | 17 |
| Acceptance: `--files tests/seeded/claims/bad_evidence_synthetic.md` | exit 1, `RULE C-EVIDENCE-SYNTHETIC ... holds 2 synthetic result(s) of 2` |
| Acceptance: `grep -c 't64-claim-example'` / `grep -c 'boot-fail'` on the template | 3 / 4 |
| Acceptance: `ctest --preset gate -L gov -N` | 13 tests (floor 12): 4 manifest, 2 rights, 2 pii, 2 provenance, 3 claims |
| Linux parity (`docker run rust:1.91`, aarch64, dash, mawk 1.3.4; `t64-result-check` compiled with `cc`, no CMake in the image) | selftest `cases=49 failures=0`, repository scan clean, seeds `rows=17 pass=17 fail=0` |

### Gate timing

| Run | Total | Steps |
|-----|-------|-------|
| `./scripts/gate.sh` (first run after the last commit's files were staged) | 79 s | sh-n 0, lint-selftest 7, configure 1, build 0, ctest 69, core-lint 1 |
| `CI=true ./scripts/gate.sh` (immediately after) | 39 s | sh-n 0, lint-selftest 3, configure 0, build 0, ctest 35, core-lint 0 |

Both exit 0 with `[gate] PASS`; 49 of 49 tests passed in the first run. The first total is over the 60 s target, the second under it, so the same tree measured 79 s and 39 s: expect the 77 s figure the orchestrator saw after 01-11 to be a noisy upper bound. ctest dominates and runs the 49 tests serially (the sum of per-test times equals the wall time: 69.4 s). The slowest tests in the first run were `gov.rights.selftest` 12.6 s, `gov.provenance.selftest` 8.7 s, `repo.docs-selftest` 7.0 s, the four `controls.suite*`/`controls.inventory-drift` tests about 6 s each, `gov.pii.selftest` 5.0 s. This plan adds about 3.2 s (selftest 2.4 s, seeds 0.8 s, repo 0.05 s). The host reports 18 cores; the `gate` ctest preset sets no parallelism, which belongs to plan 01-01's `CMakePresets.json` and was not changed here.

Mutation sanity checks on scratch copies (not committed; "caught" means the seeds driver or the selftest failed): 32 mutations, 28 caught, listed below with the 4 survivors.

| Mutation | Result |
|----------|--------|
| synthetic tier, profile or build check removed; synthetic-evidence refusal removed; reserved-word scope allowance always on; reserved word in block or in prose removed; whole-word match removed; statement check removed; count-sum check removed; symlink, dot-segment and digest-length checks removed; `.planning` and seed exclusions removed; zero-input check removed; evidence pass and build/profile mismatch removed; suite-record check removed; tier enum, unknown key, duplicate key, unterminated block, oracle, missing key and tier-conditional checks removed | caught |
| `scope` on a non-hardware tier allowed; profile length limit removed | survived the first selftest; closed with two cases (`scope on a tier that is not hardware-derived`, profile of 64 vs 65 characters) |
| `pass > total` check removed | equivalent: the sum check still reports C-COUNTS (only the message differs) |
| identity non-zero exit ignored | equivalent: identity prints nothing on failure, so the empty-results check still reports C-EVIDENCE-INVALID |
| `profile synthetic-stub` and `+synthetic` build detection inside the evidence stage removed | equivalent: `t64-result-check` refuses a partly applied synthetic stamp (R-SYNTHETIC-STAMP), so the tier column already catches every synthetic record; the two column tests stay as defence in depth |

## Task Commits

1. Task 1 (tracer): `fde1f8d` - template, `check-claims.sh`, `gov.claims.selftest`, `gov.claims.repo`
2. Task 2: `6b82d98` - 17 seeds with `EXPECT.tsv`, `gov.claims.seeds`, selftest cases for the mutants the first selftest missed

Tracer feedback gate (auto mode): `<verify>` (`cmake --preset gate && ctest --preset gate -R 'gov\.claims'`) re-run end to end after Task 1, passed (2 tests), expanded.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical] Evidence must end with a t64.suite record**
- **Found during:** Task 1 design, reading the 01-07 summary
- **Issue:** `t64-result-check identity` treats the suite record as optional and compares no inventory, so a single result file or a suite whose suite record was cut off would pass `identity` and could back a claim (T-01-42).
- **Fix:** after `identity` exits 0, the last line of the evidence file must contain `"schema":"t64.suite"` and `identity` must have printed at least one result; otherwise C-EVIDENCE-INVALID. The template says so and the selftest has `not-a-suite` and `invalid-suite` cases. The check does not compare the pinned inventory (`suite --inventory` does that where the suite is sealed).
- **Files modified:** scripts/check-claims.sh, docs/claims/TEMPLATE.md
- **Commit:** fde1f8d

### Additions beyond the plan

- `--root DIR` on `check-claims.sh` (not in the interfaces block): the selftest needs a temporary repository root for the evidence and default-scan cases. Without it the symlink and traversal cases could not be proven without writing into the repository.
- 49 selftest cases rather than the two the plan asks for. The plan's 17 seeds cannot reach C-ORACLE, C-EVIDENCE-MISMATCH, C-EVIDENCE-INVALID or C-ZERO-INPUT (they need the built C tool or a temporary root), so those are proven by selftest cases; the plan's done line ("every claims rule is proven") is met by seeds plus selftest together. The seed count stays exactly 17.

### Interpretation notes (no conflict with a measured fact)

- The interfaces block lists `C-ORACLE` without a definition: it fires when `oracle_kind` or `oracle_revision` is present but empty. A conditional key on the wrong tier is C-FIELD-UNKNOWN, and an empty required value (other than the oracle pair) is C-FIELD-MISSING; none of these are spelled out in the plan.
- `bad_emulator_missing.md` and `bad_hw_no_scope.md` expect C-FIELD-MISSING; the empty-scope reserved-word clause is proven by the selftest case `reserved word with an empty hardware scope`.
- Another unlisted behaviour: an unterminated example or ordinary code fence is C-BLOCK-SYNTAX, so an open fence cannot swallow a real claim below it. The repository scan found no such fence in any existing document.
- The `.planning/` exclusion and the `tests/seeded/claims/` exclusion apply to the default scan only; `--files` checks whatever it is given.
- `not_claimed` is written without a trailing period because the renderer appends one (documented in the template).

**Total deviations:** 1 auto-fixed (Rule 2) plus the additions above. **Impact:** none on scope or on any acceptance criterion.

## Known Stubs

None.

## Threat Flags

None beyond the plan's register. Mitigations applied: T-01-42 (C-SYNTHETIC on tier, profile and build; C-EVIDENCE-SYNTHETIC and C-EVIDENCE-MISMATCH via `t64-result-check identity`; seeds and selftest cases, and mutation checks that fail when each is removed), T-01-43 (C-RESERVED-WORD in blocks and on prose lines, and C-STATEMENT forcing the sentence to equal the rendering), T-01-44 (lexical check, per-component symlink check and regular-file check before use; selftest cases `dotdot`, `absolute`, `symlink-file`, `symlink-dir`, `directory`, `missing`), T-01-SC-12 (sh and awk only, no installs). A scan of the new files for home paths, temp paths and email addresses found nothing (`gov.pii.repo` and `gov.rights.repo` pass in the gate).

## Issues Encountered

- Plan 01-04's writing rule held: no existing document was flagged by `gov.claims.repo`, so no file outside this plan was edited.
- `check-claims.sh` needs `t64-result-check` built (`T64_BUILD_DIR`, default `build/gate`) for `--selftest` and for any claim with `evidence`. The plan's verify (`cmake --preset gate && ctest --preset gate -R 'gov\.claims'`) does not build, so on a fresh checkout it needs a prior `cmake --build --preset gate` (the gate always builds first). The missing tool is a clear exit 2 message, never a silent pass.

## Next Phase Readiness

- Plan 01-13 (CI and phase close): the first CI run also confirms gawk on ubuntu-24.04 x64 and macos-26 sh/awk for `check-claims.sh`; it was measured here on macOS (BWK awk, dash) and Linux aarch64 (mawk 1.3.4, dash). `git ls-files` is needed by the default scan, so the CI checkout must be a git work tree (it is; `actions/checkout`).
- Phase 1 has no real claims (`blocks=0`). The first real claim needs a real profile id (the placeholder in the manifest is `first-profile-unnamed`), a non-synthetic result suite, and a manifest digest; `manifest_digest` is `sha256:` plus 64 hex, and this plan does not define how the digest of the manifest file is computed.
- Owner-visible: the gate's ctest step runs 49 tests serially (about 69 s in one run, 35 s in another). Adding `-j` via the preset would shorten it, but that file is plan 01-01's and is left alone.

## Self-Check: PASSED

- FOUND: docs/claims/TEMPLATE.md, scripts/check-claims.sh, tools/claims/CMakeLists.txt, tests/seeded/claims/EXPECT.tsv and 17 `.md` seeds (`ls tests/seeded/claims/*.md | wc -l` prints 17)
- FOUND commits: fde1f8d, 6b82d98 (both ancestors of HEAD); `git rev-list --count b5a2374..HEAD` is 2
- Acceptance re-run: all criteria for both tasks pass; `./scripts/gate.sh` and `CI=true ./scripts/gate.sh` exit 0 with `[gate] PASS`
