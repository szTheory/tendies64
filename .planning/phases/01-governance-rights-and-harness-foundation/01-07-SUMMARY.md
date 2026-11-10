---
phase: 01-governance-rights-and-harness-foundation
plan: "07"
subsystem: infra
tags: [c17, result-protocol, suite, inventory, consumer, canonical-json, seeded-cases, ctest, run-02, run-04]
status: complete

requires:
  - phase: 01-governance-rights-and-harness-foundation
    provides: "t64.result v1 supervisor, classifier name helpers and goldens (01-02); strict parser, canonical writer and run-expect.sh (01-05)"
provides:
  - "t64-result-check result|suite|seal|identity: the t64.result v1 and t64.suite consumer"
  - "Rule ids R-JSON, R-SCHEMA, R-VERSION, R-UNKNOWN-FIELD, R-MISSING-FIELD, R-TYPE, R-ENUM, R-DETAIL-OUTCOME, R-SYNTHETIC-STAMP, R-NULL-RULES, R-NOT-CANONICAL, S-TRUNCATED, S-EMPTY, S-ORDER, S-DUPLICATE, S-INVENTORY"
  - "tests/seeded/results/ (10 seeds) and tests/seeded/suites/ (INVENTORY plus 10 seeds), directory-owned with EXPECT.tsv"
  - "CTest unit.result-goldens, unit.result-seeds, unit.suite-seeds (label unit)"
  - "docs/spec/result-protocol.md section 'Suites and the consumer'"
affects: [01-11, 01-12, 01-13]

actuals:
  tokens: 17000
  tasks: 2
  commits: 2
plan_head_before: a2f286118b3f76fcf408c3a2c2afb9aaccdc6612
plan_head_after: 27999b8febeabfa2012c4bd4cf5626058943cfed

tech-stack:
  added: []
  patterns:
    - "Consumer = strict parse, exact key sets, type and enum checks, cross-field rules, then byte comparison with the record's own canonical re-emission"
    - "First failure stops the check and prints RULE-ID path[:line]: message on stderr; identity prints nothing unless every line validated"
    - "seal builds the whole suite in memory, runs the suite check on it, and writes FILE.tmp then renames only on success"
    - "Seeded corpora are hand-edited or independently constructed (printf plus shasum), never produced by the code under test"

key-files:
  created:
    - runner/t64_result_check.c
    - tests/seeded/results/EXPECT.tsv
    - tests/seeded/results/good.json
    - tests/seeded/results/bad_unknown_field.json
    - tests/seeded/results/bad_missing_field.json
    - tests/seeded/results/bad_version.json
    - tests/seeded/results/bad_float.json
    - tests/seeded/results/bad_dup_key.json
    - tests/seeded/results/bad_not_canonical.json
    - tests/seeded/results/bad_detail_outcome.json
    - tests/seeded/results/bad_synthetic_stamp.json
    - tests/seeded/results/bad_u64_number.json
    - tests/seeded/suites/EXPECT.tsv
    - tests/seeded/suites/INVENTORY
    - tests/seeded/suites/good.jsonl
    - tests/seeded/suites/truncated.jsonl
    - tests/seeded/suites/dropped.jsonl
    - tests/seeded/suites/duplicate.jsonl
    - tests/seeded/suites/bad_order.jsonl
    - tests/seeded/suites/bad_empty.jsonl
    - tests/seeded/suites/bad_hash.jsonl
    - tests/seeded/suites/bad_set.jsonl
    - tests/seeded/suites/bad_unterminated.jsonl
    - tests/seeded/suites/bad_suite_first.jsonl
  modified:
    - runner/CMakeLists.txt
    - docs/spec/result-protocol.md

key-decisions:
  - "The consumer checks schema, then schema_version, then the exact key set, so an unknown future major version reports R-VERSION rather than a pile of unknown-field errors"
  - "R-NOT-CANONICAL is checked last: a seed with one semantic defect reports that rule, and only a semantically valid but byte-different record reports R-NOT-CANONICAL"
  - "S-DUPLICATE is checked before S-ORDER and S-INVENTORY, so a repeated line reports as a duplicate rather than as a count mismatch"
  - "Only S-TRUNCATED and S-INVENTORY echo an outcome=harness-error detail line (the two details the plan names); the other suite rules just exit 1"
  - "identity validates every line and prints identities only when all validated; the suite record is optional there and no inventory is compared"
  - "profile_id and build_id are restricted to [A-Za-z0-9._+-]{1,128} so identity output cannot carry a TAB or newline"
  - "An invalid or unreadable INVENTORY is a usage error (exit 2); an oversize suite (over 16 MiB) is R-JSON JSON-size"

patterns-established:
  - "A suite is sorted results plus one final canonical suite record; INVENTORY is the sorted fixture_id list and its SHA-256 is pinned in the suite record"
  - "Consumers refuse a synthetic stamp that is only partly applied (tier, profile and build must agree)"

requirements-completed: [RUN-02, RUN-04]

coverage:
  - id: D1
    description: "t64-result-check result accepts every supervisor golden and rejects unknown fields, unknown schema_version, floats, duplicate keys, non-canonical bytes, outcome/detail mismatches, inconsistent synthetic stamps and u64 numbers, each by rule id"
    requirement: "RUN-02"
    verification:
      - kind: integration
        ref: "ctest --preset gate -R unit.result (unit.result-goldens: 2 goldens validated; unit.result-seeds: rows=10 pass=10 fail=0)"
        status: pass
    human_judgment: false
  - id: D2
    description: "A suite missing its suite record is harness-error suite-truncated; a suite that differs from the pinned inventory (count, hash or id set) is harness-error inventory-mismatch; duplicates, wrong order and empty suites are rejected by rule id"
    requirement: "RUN-04"
    verification:
      - kind: integration
        ref: "ctest --preset gate -R unit.suite-seeds (rows=10 pass=10 fail=0)"
        status: pass
    human_judgment: false
  - id: D3
    description: "identity prints fixture, profile, build, tier and outcome per record for the claims gate, and prints nothing for an invalid file"
    requirement: "RUN-02"
    verification:
      - kind: other
        ref: "build/gate/runner/t64-result-check identity tests/seeded/suites/good.jsonl prints 2 lines, both containing synthetic-stub; scratch runs on invalid and empty files exit 1 with no identity lines"
        status: pass
    human_judgment: false
  - id: D4
    description: "docs/spec/result-protocol.md states the suite record, subcommands, rule ids and the harness-error statement"
    requirement: "RUN-04"
    verification:
      - kind: other
        ref: "grep -c 't64.suite' docs/spec/result-protocol.md = 3; repo.docs passes in the gate"
        status: pass
    human_judgment: false

duration: 4min
completed: 2026-11-10
---

# Phase 1 Plan 07: Result and suite consumer Summary

**`t64-result-check` validates t64.result v1 records and t64.suite files by 16 rule ids (strict parser, exact key sets, canonical bytes, synthetic-stamp consistency, pinned-inventory comparison), seals suites, and extracts identities for the claims gate, proven by three CTest tests over 30 hand-built seeds and goldens.**

## Accomplishments

- Tracer slice proven end to end (supervisor golden, strict parser, `t64-result-check result`, CTest) with the ten single-result seeds, then expanded to suites.
- `result`: schema, version, exact key sets for the record and `counts`, shape checks, closed enums for outcome, detail, tier and stop_reason, detail-to-outcome agreement, the all-or-none synthetic stamp, null and zero rules per outcome class, and a byte comparison with the canonical re-emission. Reads at most 64 KiB.
- `suite`: at most 16 MiB; every line validated; mandatory final suite record; S-TRUNCATED, S-EMPTY, S-ORDER, S-DUPLICATE and S-INVENTORY (count, hash of INVENTORY, id set), echoing `outcome=harness-error detail=suite-truncated|inventory-mismatch` for the two harness-error classes.
- `seal`: validates each result, sorts, appends the canonical suite record built with the JSON writer, runs the suite check on its own output, then writes through `FILE.tmp` and rename. A partial set is refused and nothing is written.
- `identity`: tab-separated `fixture_id profile_id build_id evidence_tier outcome`, printed only when every line validated.
- `docs/spec/result-protocol.md` gained "Suites and the consumer" (suite record, INVENTORY, subcommand table, both rule tables).

## Measured results

| Item | Value |
|------|-------|
| `./scripts/gate.sh` (Darwin arm64, Apple clang, CMake 4.4.3, Ninja) | `[gate] total 13s`, `[gate] PASS` (sh-n 0s, lint-selftest 3s, configure 0s, build 0s, ctest 10s, core-lint 0s) |
| Gate tests | 18 of 18 passed, including unit.result-goldens, unit.result-seeds, unit.suite-seeds |
| `unit.result-goldens` | `goldens validated: 2` |
| `unit.result-seeds` | `run-expect: rows=10 pass=10 fail=0` |
| `unit.suite-seeds` | `run-expect: rows=10 pass=10 fail=0` |
| Acceptance: `result tests/seeded/results/bad_unknown_field.json` | exit 1, `R-UNKNOWN-FIELD ...: unknown field in record: wall_ms` |
| Acceptance: `ls tests/seeded/results/*.json \| wc -l` | 10 |
| Acceptance: `suite ... truncated.jsonl` | exit 1, `S-TRUNCATED` and `outcome=harness-error detail=suite-truncated` |
| Acceptance: `identity tests/seeded/suites/good.jsonl` | 2 lines, both `synthetic-stub` |
| Acceptance: `grep -c 't64.suite' docs/spec/result-protocol.md` | 3 |

`tests/seeded/suites/good.jsonl` was produced by `seal` and then confirmed against an independent construction: `cat empty-halt.json positive.json` plus a suite line built with `printf` and the host `shasum -a 256` over INVENTORY (`05717801...3460`), which matched byte for byte (`cmp`). `seal` with the goldens given in the opposite order produced identical bytes.

Mutation sanity checks (scratch copies compiled outside the repo, not committed; rc nonzero means a seed run noticed):

| Mutation | results seeds | suite seeds |
|----------|---------------|-------------|
| synthetic-stamp check disabled | 1 | 0 |
| canonical-bytes comparison disabled | 1 | 0 |
| detail/outcome check disabled | 1 | 0 |
| unknown-field rejection disabled | 1 | 0 |
| inventory hash check disabled | 0 | 1 |
| duplicate check disabled | 0 | 1 |
| missing-suite-record (last line is a result) check disabled | 0 | 1 |

Additional scratch checks (not seeds): an invalid INVENTORY is exit 2; `seal` with one of two inventory cases or with an invalid result writes no file; `result` on a suite file reports a trailing-data R-JSON error; no argument prints usage with exit 2; a missing file is exit 2.

## Task Commits

1. Task 1 (tracer): `cf4c36f` - `t64-result-check result`, ten seeds, `unit.result-goldens`, `unit.result-seeds`
2. Task 2: `27999b8` - `seal`, `suite`, `identity`, ten suite seeds with INVENTORY, `unit.suite-seeds`, spec update

Tracer feedback gate (auto mode): `<verify>` (`cmake --preset gate && cmake --build --preset gate && ctest --preset gate -R 'unit\.result'`) re-run end to end after Task 1, passed, expanded.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Uninitialised line number in diagnostics**
- **Found during:** Task 1 first manual run of `result`
- **Issue:** `set_diag` did not reset the `line` field, so a single-file failure printed garbage as `path:LINE`.
- **Fix:** `set_diag` zeroes `line`; scan sets it per line.
- **Files modified:** runner/t64_result_check.c
- **Commit:** cf4c36f (fixed before the commit)

### Interpretation notes (no conflict with a measured fact)

- **Extra suite seeds.** The plan lists four suite seeds (good, truncated, dropped, duplicate). Six more were added (`bad_order`, `bad_empty`, `bad_hash`, `bad_set`, `bad_unterminated`, `bad_suite_first`) so S-ORDER, S-EMPTY, the hash and set halves of S-INVENTORY, the LF-terminated rule and the suite-not-last rule are each seeded, as the T-01-28 mitigation says ("each seeded"). The plan's acceptance checks are unaffected. `ls tests/seeded/suites` now holds 11 files.
- **Result key count.** The plan text mentions the exact key sets "from result-protocol.md"; the record has 15 top-level keys (the spec table lists them all) and 6 `counts` keys. Both sets are exact.
- **Which suite failures echo a harness-error line.** The interfaces block names only `suite-truncated` and `inventory-mismatch`, so S-EMPTY, S-ORDER, S-DUPLICATE and invalid result lines exit 1 with their rule diagnostic only. S-INVENTORY covers a mismatching count, hash or set; S-TRUNCATED covers an empty file, an unterminated final line and a last line that is a result.
- **`unit.result-goldens` is an inline `sh -c` in `runner/CMakeLists.txt`** (a CMake bracket argument), so no helper script was added outside `files_modified`.
- **Messages and the public repo.** Diagnostics print the path exactly as given on the command line and never an absolute path of their own; the committed seeds, spec and source contain no home or temp paths (scanned).
- **`identity` on a suite whose hash or counts disagree with an inventory** succeeds: it never compares an inventory. Use `suite` first; plan 01-12 should run `suite` before `identity`.

**Total deviations:** 1 auto-fixed (Rule 1), 0 plan conflicts.

## Known Stubs

None.

## Threat Flags

None beyond the plan's register. Mitigations applied: T-01-27 (exact key sets, schema_version 1, canonical-bytes comparison and R-SYNTHETIC-STAMP, each with a seed and a mutation check showing the seed run fails when the check is removed), T-01-28 (mandatory final suite record, count, inventory hash and set comparison; S-TRUNCATED, S-INVENTORY, S-DUPLICATE, S-ORDER and S-EMPTY each seeded), T-01-SC-07 (no installs; the in-tree parser only).

## Notes for later plans

- Plan 01-11: seal the live 19-case matrix with `t64-result-check seal --inventory INV --out FILE RESULT...`, with INVENTORY the sorted (`LC_ALL=C`), LF-terminated fixture_ids. The sealed file must pass `suite` against the same INVENTORY. Result file names are free; ordering is by `fixture_id`. Harness-error results for fixtures never read still carry a `fixture_id` of the pinned set.
- Plan 01-12: `t64-result-check identity FILE` prints one tab-separated line per result and nothing on failure; refuse any line whose `evidence_tier` is `synthetic` (or whose profile is `synthetic-stub`). Exit 1 means the file is invalid, so the claims gate must treat a non-zero exit as refusal.
- Exit codes: 0 ok, 1 invalid or harness-error, 2 usage or unreadable input (including an invalid INVENTORY).
- `run-expect.sh` was used unchanged (`--ext .json` for results, `--ext .jsonl` for suites); INVENTORY is a support file there.

## Self-Check: PASSED

- FOUND: runner/t64_result_check.c, runner/CMakeLists.txt, docs/spec/result-protocol.md, tests/seeded/results/{EXPECT.tsv,good.json,bad_unknown_field.json,bad_missing_field.json,bad_version.json,bad_float.json,bad_dup_key.json,bad_not_canonical.json,bad_detail_outcome.json,bad_synthetic_stamp.json,bad_u64_number.json}, tests/seeded/suites/{EXPECT.tsv,INVENTORY,good.jsonl,truncated.jsonl,dropped.jsonl,duplicate.jsonl,bad_order.jsonl,bad_empty.jsonl,bad_hash.jsonl,bad_set.jsonl,bad_unterminated.jsonl,bad_suite_first.jsonl}
- FOUND commits: cf4c36f, 27999b8 (both ancestors of HEAD)
- Acceptance criteria re-run after Task 2: all pass (see Measured results). `./scripts/gate.sh` exits 0 with `[gate] PASS`.
