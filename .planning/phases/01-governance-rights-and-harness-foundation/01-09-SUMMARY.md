---
phase: 01-governance-rights-and-harness-foundation
plan: "09"
subsystem: governance
tags: [c17, json-schema-subset, posix-regex, manifest, rights, ctest, require-fixtures, gov-03, gov-05, gov-07]
status: complete

requires:
  - phase: 01-governance-rights-and-harness-foundation
    provides: "strict JSON parser, t64_sha256, run-expect.sh (01-05); rights-scan.sh and t64-blobscan allowlist (01-06); gate.sh and the tools glob (01-01); rights policy (01-04)"
provides:
  - "tools/manifest/t64_schema.[ch]: dependency-free validator for a declared JSON Schema keyword subset; a keyword outside the subset is refused before any instance is read"
  - "t64-manifest-check [--schema] [--require-fixtures] [--emit-allowlist FILE] [--root DIR] MANIFEST: schema findings plus 11 cross-field rights rules, exit 0/1/2"
  - "fixtures/manifest.schema.json and fixtures/manifest.json (nemu64-test-base pending/local-only, two forbidden entries without digests)"
  - "tests/seeded/manifest (23 bad, 2 good) and tests/seeded/manifest-schema (11 bad), both asserted by rule id through run-expect.sh"
  - "tests/seeded/run-require-fixtures.sh: 14 cases proving --require-fixtures in both modes, --emit-allowlist, symlink refusal and subset refusal"
  - "scripts/gate.sh: exactly one CI-conditional block (CI=true adds --require-fixtures and nothing else)"
  - "scripts/rights-scan.sh: manifest-listed cleared fixtures feed the GOV-07 allowlist; a manifest with any finding stops the scan (exit 2)"
  - "CTest gov.manifest.seeds, gov.manifest.schema-seeds, gov.manifest.repo, gov.manifest.require-fixtures (label gov)"
affects: [01-10, 01-11, 01-12, 01-13]

actuals:
  tokens: 24200
  tasks: 3
  commits: 3
plan_head_before: adb552e2e26a369f737ab803bae4fb18d4623c72
plan_head_after: 2dcc96919f22e8f68b87df16dccc86042532a293

tech-stack:
  added: []
  patterns:
    - "A schema file states only constraints the validator enforces: unknown keywords, non-false additionalProperties, unanchored patterns and uncompilable regexes are harness errors (exit 2), found by walking the schema before the instance"
    - "oneOf tries every branch silently; exactly one clean branch passes; otherwise the closest branch's findings are printed, then one M-SCHEMA-ONEOF line"
    - "Cross-field rules run only on a structurally valid manifest, so they never read a malformed shape"
    - "A manifest feeds the rights allowlist only when it has zero findings (the allowlist file is left empty otherwise)"
    - "Fixture paths are checked lexically first, then every component is lstat-ed (no symlink, regular file last), then opened with O_NOFOLLOW"

key-files:
  created:
    - tools/manifest/CMakeLists.txt
    - tools/manifest/t64_schema.h
    - tools/manifest/t64_schema.c
    - tools/manifest/t64_manifest_check.c
    - fixtures/manifest.json
    - fixtures/manifest.schema.json
    - tests/seeded/manifest/
    - tests/seeded/manifest-schema/
    - tests/seeded/run-require-fixtures.sh
  modified:
    - scripts/rights-scan.sh
    - scripts/gate.sh

key-decisions:
  - "The non-forbidden entry shape also permits an optional forbidden_reason, so an entry whose source_rights is forbidden can carry its reason; M-FORBIDDEN-REASON fires when either right is forbidden and the reason is absent or blank"
  - "The forbidden shape lists sha256, size, source, build, expected and path as accept-anything members so the cross-field rule M-FORBIDDEN-DIGEST (not M-SCHEMA-ADDITIONAL) reports them; all six keys are refused, not only the digest"
  - "M-BYTES-NOT-PERMITTED has two triggers: commit or artifact in distribution without authored-here/cleared binary_rights, and a path without authored-here/cleared plus commit"
  - "--root defaults to the current directory and the schema defaults to ROOT/fixtures/manifest.schema.json; the seeds run from the repository root, so a named fixture that is not there counts as skipped"
  - "A manifest with findings makes rights-scan.sh exit 2, not 1: the scan cannot trust the allowlist, which is a harness condition, not a finding about a blob"

patterns-established:
  - "Seeds are one valid entry with exactly one defect; M-SCHEMA-REQUIRED needles are the missing pointer followed by a colon so /entries/0/source does not also match /entries/0/source_rights"
  - "A CTest wrapper that must prove a mode through the log builds its label at run time, because CTest echoes the command line into the same log"

requirements-completed: [GOV-03, GOV-05, GOV-07]

coverage:
  - id: D1
    description: "fixtures/manifest.json records source and revision, recipe and toolchain, digest, profile, both rights, distribution, evidence, notices and expected-result provenance, holds only pending and forbidden entries, and no forbidden entry carries a digest"
    requirement: "GOV-03"
    verification:
      - kind: integration
        ref: "ctest --preset gate -R gov.manifest.repo (entries=3 permitted=0 errors=0); grep -cE '\"sha256\": *\"[0-9a-f]' fixtures/manifest.json prints 0"
        status: pass
    human_judgment: false
  - id: D2
    description: "A missing source, revision, recipe, toolchain, digest, profile, source rights, binary rights, notices or expected result fails with M-SCHEMA-REQUIRED naming the pointer; every cross-field rule and the enum, pattern and unknown-field rules have their own seeded failure"
    requirement: "GOV-03"
    verification:
      - kind: integration
        ref: "ctest --preset gate -R gov.manifest.seeds (25 rows, 11 M-SCHEMA-REQUIRED, 23 bad and 2 good files) and gov.manifest.schema-seeds (11 rows)"
        status: pass
    human_judgment: false
  - id: D3
    description: "A schema keyword outside the declared subset makes the checker exit 2, and a permitted-but-absent fixture fails only under --require-fixtures"
    requirement: "GOV-03"
    verification:
      - kind: integration
        ref: "ctest --preset gate -R gov.manifest.require-fixtures (14 cases: schema-keyword, schema-open-additional, schema-unanchored-pattern exit 2; absent-require-fixtures exit 1; absent-no-flag skipped=1)"
        status: pass
    human_judgment: false
  - id: D4
    description: "CI=true adds exactly one flag: the gate prints one CI-mode line and gov.manifest.repo runs with --require-fixtures; a run with CI unset prints no such line and runs without it; gate.sh has exactly one CI-conditional block"
    requirement: "GOV-05"
    verification:
      - kind: integration
        ref: "plan Task 3 verify run from a script file (STEP-ON-OK, STEP-OFF-PASS, STEP-OFF-LOG-OK, rc 0); awk count of non-comment CI lines in gate.sh is 2"
        status: pass
    human_judgment: false
  - id: D5
    description: "A cleared NUL-bearing fixture listed in the manifest passes the rights scan (tree and history), the same bytes unlisted fail with R-NUL-HEAD, and a manifest with a digest mismatch or a syntax error stops the scan"
    requirement: "GOV-07"
    verification:
      - kind: integration
        ref: "sh scripts/rights-scan.sh --selftest (cases manifest-unlisted, manifest-allowlisted, manifest-allowlisted-history, manifest-digest-mismatch, manifest-invalid; cases=54 failures=0)"
        status: pass
    human_judgment: false

duration: 11min
completed: 2026-11-10
---

# Phase 1 Plan 09: Fixture manifest, schema-subset validator and CI require-fixtures Summary

**A dependency-free validator for a declared JSON Schema subset (unknown keyword exits 2) plus eleven cross-field rights rules validates `fixtures/manifest.json` in every gate run, proven by 34 rule-id-asserted seeded manifests, a 14-case temporary-root driver for `--require-fixtures`, and a single `CI=true` block in `gate.sh`; manifest-listed cleared fixtures feed the GOV-07 allowlist and a manifest with any finding stops the rights scan.**

## Accomplishments

- Tracer slice first (Task 1): schema file, schema engine, `t64-manifest-check`, CTest `gov.manifest.seeds` and `gov.manifest.repo`, the real manifest and one missing-revision seed, verified end to end before expansion.
- `t64_schema.c`: `t64_schema_check` walks the schema first (keywords, value shapes, anchored patterns that compile, scalar-only `enum`/`const`), then `t64_schema_validate` applies it with JSON-pointer diagnostics. `oneOf` selects the entry shape by trying every branch; a NUL byte inside a string that has a `pattern` is a pattern finding, because `regexec` would stop at it. Pointers and echoed values are sanitized so a hostile key cannot forge a `RULE` line.
- `t64_manifest_check.c`: cross-field rules `M-ZERO-ENTRIES`, `M-DUP-ID`, `M-FORBIDDEN-DIGEST`, `M-FORBIDDEN-REASON`, `M-NULL-DIGEST`, `M-BYTES-NOT-PERMITTED`, `M-PATH` (lexical, then per-component `lstat`, then `O_NOFOLLOW`), `M-DIGEST-MISMATCH` (streaming SHA-256, plus size), `M-FIXTURE-ABSENT` (only with `--require-fixtures`), `M-HW-PROVENANCE`, `M-TOOLCHAIN-PIN`. `--emit-allowlist` writes rows only for a clean manifest.
- `fixtures/manifest.json`: `nemu64-test-base` pinned to `9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c` (pending/pending, `local-only`, digest and size null, rights evidence names D2), plus forbidden `nintendo-pif-boot-rom` and `commercial-cartridge-roms` with reasons and no digest or source.
- `gate.sh` carries exactly one CI block; `scripts/rights-scan.sh` runs the checker with `--emit-allowlist` and passes the result as an extra `--allow`.

## Measured results

| Item | Value |
|------|-------|
| `git ls-remote https://github.com/thelemmy/nemu64-test HEAD` (retrieved 2026-11-10 UTC, 2026-11-09 local) | `9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c`, equal to the pin; HEAD did not move |
| `./scripts/gate.sh` (Darwin arm64, Apple clang 21.0.0, CMake 4.4.3, Ninja 1.13.2, git 2.41.0, BWK awk 20200816) | `[gate] total 12s`, `[gate] PASS`; sh-n 0s (10 scripts), lint-selftest 3s, configure 0s, build 0s, ctest 9s (22 of 22 passed), core-lint 0s |
| `CI=true ./scripts/gate.sh` | exit 0, `[gate] total 14s`, exactly one `[gate] CI mode: --require-fixtures` line; the CTest log shows `manifest-check argv: --schema fixtures/manifest.schema.json --require-fixtures fixtures/manifest.json` |
| `env -u CI ./scripts/gate.sh` | `[gate] PASS`, no CI-mode line, log argv line without the flag |
| `ctest --preset gate -L gov -N` | 8 tests: 4 manifest, plus gov.rights.selftest, gov.rights.repo, gov.pii.selftest, gov.pii.repo (plan floor: 7) |
| Manifest summary | `manifest: entries=3 permitted=0 present=0 skipped=0 errors=0` then `ok fixtures/manifest.json` |
| Plan acceptance counts | `M-SCHEMA-REQUIRED` rows 11; `bad_*.json` 23; `good*.json` 2; `forbidden_reason` occurrences 2; revision pin occurrences 3; sha256 digests in the manifest 0 |
| `sh scripts/rights-scan.sh --selftest` | cases=54 failures=0 (49 before this plan, 5 added); `sh scripts/rights-scan.sh` on the repository: `tree=225 history=287 findings=0`; `sh scripts/pii-scan.sh`: `tree_files=225 commits=50 findings=0` |
| ASan/UBSan (`ctest --preset asan -R gov.manifest`, and `-R gov.rights.selftest`) | all passed, no sanitizer output |

Mutation sanity checks on scratch copies (not committed). "caught" means at least one of the three seeded drivers failed:

| Mutation | Result |
|----------|--------|
| M-FORBIDDEN-DIGEST, M-NULL-DIGEST, `..` segment check, toolchain pin, duplicate id, M-HW-PROVENANCE, M-FORBIDDEN-REASON, M-ZERO-ENTRIES, `--require-fixtures` ignored, allowlist written despite findings | caught by seeds or the temp-root driver |
| M-BYTES-NOT-PERMITTED first trigger removed; digest comparison removed; size comparison removed | first survived the plan's seeds (the path trigger caught the seed), the other two survived because tampering also changed the size; closed by the extra cases `commit-without-rights`, `digest-only`, `size-only` |
| schema: required, additionalProperties, pattern, enum, const, string length (max and min), minItems, uniqueItems, minimum, maximum, oneOf, items recursion | caught |
| schema: NUL guard before `regexec` | survived until `bad_nul_in_id.json` was added, then caught |
| lstat symlink check removed; early return after a type mismatch removed | equivalent mutants: `O_NOFOLLOW` and the not-a-directory test still refuse the path, and a type mismatch adds no further finding on the seeds |
| rights-scan feed: allowlist row not appended; checker failure ignored | each caught by two selftest cases |

## Task Commits

1. **Task 1 (tracer): manifest, schema engine, checker, two tests** - `8323be5` (the checker already contains every cross-field rule, because they share one source file; Task 2 added their seeds and driver)
2. **Task 2: 34 seeds, `run-require-fixtures.sh`, `gov.manifest.schema-seeds` and `gov.manifest.require-fixtures`** - `7ca9786`
3. **Task 3: CI block in gate.sh, manifest allowlist feed, five selftest cases** - `2dcc969`

Tracer feedback gate (auto mode): `<verify>` re-run end to end after Task 1 with only the tracer's two seeds present, passed, logged `Tracer verified end-to-end - expanding`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The plan's Task 3 verify cannot pass against a literal `sh -c` wrapper**
- **Found during:** Task 3 verify
- **Issue:** CTest writes each test's command line into `build/gate/Testing/Temporary/LastTest.log`. The wrapper script text contained both `manifest-check argv:` and `--require-fixtures`, so the log held two `manifest-check argv:` lines and the verify's non-CI check (no such line may carry the flag) failed on the command line itself, and its CI check could pass on it too.
- **Fix:** the wrapper builds the label at run time (`n=manifest-check; echo "$n argv: ..."`), so the log holds exactly one argv line, the one the test prints. No design change; the printed line is as the plan specifies.
- **Files modified:** tools/manifest/CMakeLists.txt
- **Commit:** 2dcc969

### Additions beyond the plan (Rule 2, proof that every rule is live)

- `tests/seeded/manifest-schema/` (11 seeds) and `gov.manifest.schema-seeds`: the 25 plan seeds do not reach `M-SCHEMA-TYPE`, `-LENGTH`, `-ITEMS`, `-RANGE`, `-ONEOF`, a `const` mismatch, the NUL guard or `M-JSON`. The mutation table above shows each of these was a surviving mutant without a seed. The plan's counts for `tests/seeded/manifest/` (23 bad, 2 good, 11 required rows) are untouched.
- `run-require-fixtures.sh` has 14 cases, not 6: the plan's six plus `emit-allowlist-empty-on-findings`, `symlink-refused`, `symlink-parent-refused`, `digest-only`, `size-only`, `commit-without-rights`, `schema-open-additional`, `schema-unanchored-pattern`.
- `rights-scan.sh` selftest gained `manifest-allowlisted-history`, `manifest-digest-mismatch` and `manifest-invalid` beside the plan's `manifest-allowlisted`, plus an unlisted control named `manifest-unlisted`.

### Interpretation notes (no conflict with a measured fact)

- The plan lists the optional entry keys as `path` and `capture_provenance`; the non-forbidden shape also allows `forbidden_reason` (see key-decisions). The forbidden shape accepts `sha256 size source build expected path` as unconstrained members so the checker, not `additionalProperties`, reports them under `M-FORBIDDEN-DIGEST`.
- `type` accepts a name or an array of names (`["string","null"]` for `sha256`); the plan's keyword list names `type` without saying which forms.
- `size` has `maximum` 68719476736 (64 GiB), so `maximum` is a used and tested keyword; `distribution` and `toolchain` use `maxItems`, `maxLength` bounds the strings.
- Path rule (`M-PATH`): printable ASCII without space or backslash, no empty, `.` or `..` segment, must start `fixtures/` with at least one more segment, and a committed fixture needs a path. `M-TOOLCHAIN-PIN`: `<name><space|@|=><version>` where the version starts with a digit or is `nightly-YYYY-MM-DD`, and `.x` suffixes are refused.
- A malformed manifest is a finding (`M-JSON`, exit 1); a malformed or out-of-subset schema is a harness error (exit 2).
- Profile ids use the claims-check format `^[a-z0-9][a-z0-9-]*$` (plan 01-12); the placeholder profile `first-profile-unnamed` stands until the first machine profile is named.

**Total deviations:** 1 auto-fixed (blocking) plus the additions and interpretation notes above. **Impact:** none on scope or on any acceptance criterion.

## Known Stubs

None.

## Threat Flags

None beyond the plan's register. Mitigations applied: T-01-32 (lexical path check, per-component `lstat`, `O_NOFOLLOW`; seeds `bad_path_traversal` and the `symlink-*` cases), T-01-33 (forbidden entries carry no digest, size, source, build, expected or path; null digests only for pending; the committed manifest has zero digests), T-01-34 (unknown keywords, open `additionalProperties`, unanchored patterns exit 2; seeded temp schemas), T-01-35 (only authored-here/cleared commit-channel entries with a path and digest are emitted, only from a clean manifest, matched on path and sha256; `manifest-digest-mismatch` and `manifest-invalid` selftest cases), T-01-SC-09 (no installs; `git ls-remote` was read-only). A scan of the new files for home paths, temp paths and email addresses found nothing.

## Issues Encountered

- The CTest command-line echo (deviation 1).
- The plan's multi-line Task 3 verify is bash; zsh quoting broke it when passed inline, so it was run from a scratch script file, text unchanged.

## Next Phase Readiness

- Plan 01-12 (claims) can read `fixtures/manifest.json`; this plan defines no manifest digest format, so `manifest_digest` is still that plan's to define (`sha256:` plus 64 hex).
- To add a fixture: add an entry whose `binary_rights` is `authored-here` or `cleared` with `distribution` containing `commit`, a `path` under `fixtures/`, and the `sha256` and `size` of the committed text file. `rights-scan.sh` then allowlists exactly that path and digest, and only while the whole manifest has zero findings.
- `rights-scan.sh` now needs `t64-manifest-check` built whenever the scanned repository has `fixtures/manifest.json` (same `T64_BUILD_DIR` rule as `t64-blobscan`).
- First CI run (plan 01-13): confirms POSIX ERE (`{40}`, `{4}`, `[ @=]`) and `O_NOFOLLOW` on ubuntu-24.04 with glibc; they were measured on macOS here. With `CI=true` the gate adds `--require-fixtures`; today the manifest has no permitted bytes, so it cannot fail on that.
- D2 stays open: `nemu64-test-base` is pending and local-only until the owner decides after a per-asset rights audit. The reproducibility of the built ROM (two clean builds, same digest) is still unmeasured, which is why its digest is null.

## Self-Check: PASSED

- FOUND: tools/manifest/{CMakeLists.txt,t64_schema.h,t64_schema.c,t64_manifest_check.c}, fixtures/{manifest.json,manifest.schema.json}, tests/seeded/manifest/EXPECT.tsv (25 seeds), tests/seeded/manifest-schema/EXPECT.tsv (11 seeds), tests/seeded/run-require-fixtures.sh, scripts/gate.sh, scripts/rights-scan.sh
- FOUND commits: 8323be5, 7ca9786, 2dcc969 (all ancestors of HEAD); `git rev-list --count adb552e..HEAD` is 3
- Acceptance re-run: all three tasks' acceptance criteria and `<verify>` commands passed (Task 3 verify from a script file), `./scripts/gate.sh` and `CI=true ./scripts/gate.sh` exit 0
