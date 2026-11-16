---
phase: 01-governance-rights-and-harness-foundation
plan: "05"
subsystem: infra
tags: [c17, json, parser, canonical-json, fuzz, asan, ubsan, seeded-cases, run-expect, ctest, run-04]
status: complete

requires:
  - phase: 01-governance-rights-and-harness-foundation
    provides: "t64_common (UTF-8 validator, bounded canonical JSON writer), t64_test.h, gate.sh and presets (01-01)"
provides:
  - "tools/common/t64_json.[ch]: hand-written strict bounded JSON parser (arena of nodes, 19-category closed error set, limits struct)"
  - "t64_json_get, t64_json_check_keys, t64_json_u64_string helpers; t64_jsonw_node canonical tree emitter"
  - "t64-json-canon [--check] FILE: canonical round-trip oracle (exit 0/1/2/3)"
  - "tests/seeded/run-expect.sh: shared seeded-case driver (--tool, --dir, --ext) with UNLISTED, MISSING and zero-row failures"
  - "tests/seeded/json/: six directory-owned seeds plus EXPECT.tsv"
  - "CTest unit.json-seeds, unit.json, unit.json-fuzz (label unit); configure/build/test preset asan"
affects: [01-07, 01-09, 01-10, 01-11, 01-12, 01-13]

actuals:
  tokens: 16900
  tasks: 2
  commits: 3
plan_head_before: bc8edc7e12b2b56b0169d22ec39602c1b328b25f
plan_head_after: 89f29a52898b557a43cf81b7693dd939d51ecf7a

tech-stack:
  added: []
  patterns:
    - "Parser failures are one closed category plus a byte offset; consumers print JSON-<category> offset=N"
    - "Tests parse from an exact-size heap copy so ASan sees any over-read"
    - "A seeded-case directory owns its corpus: run-expect.sh derives the seed set from the extension list and fails on any unlisted seed"
    - "Positive and negative cases are hand-derived from RFC 8259; the parser output is never used to produce an expected value"

key-files:
  created:
    - tools/common/t64_json.h
    - tools/common/t64_json.c
    - tools/common/t64_json_canon.c
    - tools/common/tests/test_json.c
    - tools/common/tests/test_json_fuzz.c
    - tests/seeded/run-expect.sh
    - tests/seeded/json/EXPECT.tsv
    - tests/seeded/json/good_canonical.json
    - tests/seeded/json/bad_spaces.json
    - tests/seeded/json/bad_unsorted.json
    - tests/seeded/json/bad_trailing_comma.json
    - tests/seeded/json/bad_duplicate.json
    - tests/seeded/json/bad_float.json
  modified:
    - tools/common/t64_jsonw.h
    - tools/common/t64_jsonw.c
    - tools/common/CMakeLists.txt
    - CMakePresets.json

key-decisions:
  - "Object keys longer than 127 bytes are rejected by the parser as string-length, because the canonical writer cannot emit them; this keeps 'every accepted document canonicalizes' true"
  - "The depth limit is clamped to a hard maximum of 64 even when a caller raises it; the writer itself stops at 16 nested containers"
  - "Allocation failure is reported as category size (the category set is closed)"
  - "A node is any value, containers included; 4096 is the most the parser accepts (an array plus 4095 integers), the 4097th is nodes"
  - "run-expect.sh matches rule_id ok as a whole word (not a substring of a path or word); every other rule_id is a plain substring"
  - "t64-json-canon --check prints its exit-2 diagnostic on stdout so the driver sees it; plain mode prints it on stderr so stdout only carries canonical bytes"

patterns-established:
  - "EXPECT.tsv columns file, exit, rule_id, needle; support files (README, INVENTORY, *.sh) are not seeds because only the extension list defines seeds"
  - "Child sort in t64_jsonw_node: linear when already ascending, otherwise repeated minimum-above-previous selection (no allocation)"

requirements-completed: [RUN-04]

coverage:
  - id: D1
    description: "The strict parser rejects 90 hand-authored malformed inputs, each by its category (and offset for most), and enforces the 1 MiB input, depth 16, 4096-byte string, 4096-node and 127-byte key limits, including at-limit positives"
    requirement: "RUN-04"
    verification:
      - kind: unit
        ref: "ctest --preset gate -R unit.json (json negatives: 90, checks=770 failures=0)"
        status: pass
    human_judgment: false
  - id: D2
    description: "A deterministic 200000-mutation run never crashes the parser, every accepted mutant round-trips canonically, and the unit tests are clean under ASan and UBSan"
    requirement: "RUN-04"
    verification:
      - kind: unit
        ref: "ctest --preset gate -R unit.json-fuzz (accepted=15668 rejected=184332, checks=462683 failures=0)"
        status: pass
      - kind: integration
        ref: "ctest --preset asan -R 'unit\\.(json|common)' (4 of 4 passed, no AddressSanitizer or runtime error output)"
        status: pass
    human_judgment: false
  - id: D3
    description: "t64-json-canon --check accepts a canonical file and rejects non-canonical and malformed ones, driven by the shared seeded-case driver"
    requirement: "RUN-04"
    verification:
      - kind: integration
        ref: "ctest --preset gate -R unit.json-seeds (6 rows, 0 failures)"
        status: pass
    human_judgment: false
  - id: D4
    description: "run-expect.sh fails on an unlisted seed, a missing listed seed and zero rows, and treats support files as non-seeds"
    requirement: "RUN-04"
    verification:
      - kind: other
        ref: "temp-copy runs recorded under Measured results (UNLISTED, MISSING, zero rows, wrong expectation each exit 1; README ignored)"
        status: pass
    human_judgment: false

duration: 30min
completed: 2026-11-10
---

# Phase 1 Plan 05: Strict JSON parser, canonical oracle and seeded-case driver Summary

**A hand-written RFC 8259 subset parser (integers only, no duplicates, no comments, hard limits, 19 closed error categories) feeds `t64_jsonw_node` and `t64-json-canon`, proven by 90 independent negatives, a 200000-mutation deterministic run and a clean ASan/UBSan pass, with the shared `run-expect.sh` driver owning the seed set.**

## Accomplishments

- Tracer slice proven end to end (seed file, strict parser, canonical writer, `t64-json-canon --check`, `run-expect.sh`, CTest `unit.json-seeds`), then expanded.
- `t64_json.c`: recursive descent bounded by the depth limit, one node array (capacity is the smaller of the node limit and the input length) and one string pool per document, UTF-8 validated per sequence through `t64_utf8_valid`, surrogate-pair decoding, duplicate keys compared after unescaping, `-0`, floats, exponents, leading zeros, BOM, comments, trailing commas, raw controls and trailing data all rejected by category and byte offset.
- `t64_jsonw_node` emits a parsed tree canonically, sorting members by unsigned byte order (linear when already ascending, otherwise repeated minimum selection with no allocation).
- `t64-json-canon`: exit 0 with `ok`, 1 with `NOT-CANONICAL FILE`, 2 with `JSON-<category> offset=N`, 3 for usage or I/O. Reads at most limit+1 bytes so an oversized file reports `size`, not memory use.
- `tests/seeded/run-expect.sh`: POSIX sh, `mktemp -d` for output capture, no pipelines, `set -f` so the tool words are split but never globbed.
- `asan` configure/build/test presets (inherit `gate`, Debug, `-fsanitize=address,undefined -fno-sanitize-recover=all`); not used by the gate.

## Measured results

| Item | Value |
|------|-------|
| `./scripts/gate.sh` (Darwin arm64, Apple clang 21.0.0, CMake 4.4.3, Ninja 1.13.2) | `[gate] total 8s`, `[gate] PASS` (sh-n 0s, lint-selftest 4s, configure 0s, build 0s, ctest 4s, core-lint 0s) |
| Gate tests | 11 of 11 passed: unit.common, unit.json-seeds, unit.json, unit.json-fuzz, unit.classify, repo.docs, repo.docs-selftest, repo.ci-yml, repo.ci-yml-selftest, controls.positive, controls.empty-halt |
| `unit.json` | `json negatives: 90` (floor 40), `checks=770 failures=0` |
| `unit.json-fuzz` | `mutants=200000 accepted=15668 rejected=184332`, 15 of the 19 error categories seen, `checks=462683 failures=0`, 0.11 s |
| ASan/UBSan (`ctest --preset asan -R 'unit\.(json|common)'`) | 4 of 4 passed in 1.6 s; the output contained no `AddressSanitizer` and no `runtime error`; cache confirms `CMAKE_C_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`, `CMAKE_BUILD_TYPE=Debug` |
| `grep -c 'bad_duplicate.json' tests/seeded/json/EXPECT.tsv` | 1 |

Acceptance check, a temp copy of `tests/seeded/json/` with one extra unlisted `.json` file and a `README` support file:

```
ok good_canonical.json ok
ok bad_spaces.json NOT-CANONICAL
ok bad_unsorted.json NOT-CANONICAL
ok bad_trailing_comma.json JSON-trailing-comma
ok bad_duplicate.json JSON-duplicate-key
ok bad_float.json JSON-float
UNLISTED extra.json
run-expect: rows=6 pass=6 fail=1
rc=1
```

The `README` was not reported (support file). Other driver checks on temp copies: a listed file removed printed `MISSING bad_float.json` and exited 1; an `EXPECT.tsv` with only comments printed `FAIL zero rows in EXPECT.tsv` and exited 1; a wrong expected rule id (`JSON-exponent` for a float seed) printed `FAIL bad_float.json JSON-exponent` with the tool output and exited 1.

Mutation sanity checks (scratch copies of the parser and writer, not committed; "unit rc / fuzz rc" nonzero means the test notices):

| Mutation | unit.json | unit.json-fuzz |
|----------|-----------|----------------|
| duplicate-key check removed | 1 | 1 |
| depth limit off by one | 1 | 0 |
| float check removed | 1 | 0 |
| exponent check removed | 1 | 0 |
| lone low-surrogate check removed | 1 | 1 |
| BOM check removed | 1 | 0 |
| `-0` check removed | 1 | 0 |
| hard depth clamp removed | 1 | 0 |
| writer key sort disabled | 1 | 1 |
| `hex4` bounds check removed | 0 | 0 (but ASan build of the fuzz run reports `heap-buffer-overflow` in `hex4`) |

The last row is the reason the ASan run exists: a bounds bug in escape decoding is invisible to the plain build and caught by the sanitizer build under the same fuzz input. Mutations that only change accept versus reject are caught by `unit.json`; the fuzz oracle (accepted implies round trip) is aimed at crashes, over-reads and writer/parser disagreement.

## Task Commits

1. Task 1 (tracer): `7d68d8f` - parser, `t64_jsonw_node`, `t64-json-canon`, `run-expect.sh`, six seeds, `unit.json-seeds`
2. Task 2: `50702cd` - `unit.json` and `unit.json-fuzz`
3. Task 2: `89f29a5` - `asan` presets

Tracer feedback gate (auto mode): `<verify>` re-run end to end after Task 1, passed, expanded.

## Deviations from Plan

### Interpretation notes (no conflict with a measured fact)

- **No RED state for Task 2 (`tdd="true"`).** The plan puts the full parser in Task 1 (tracer) and the tests in Task 2, so the Task 2 tests passed on first run: 90 negatives and the whole mutation run were green against the Task 1 parser, and no GREEN fix commit was needed. Per the unexpected-green rule I did not accept that on trust: the mutation table above shows the tests fail when the parser is wrong. The commit is typed `test`, not a RED/GREEN pair. Task 1's own tracer verify (`unit.json-seeds`) covered the six seeds only.
- The writer cannot emit keys over 127 bytes (T64_JSONW_MAX_KEY) or nesting over 16, so the parser rejects such keys as `string-length` and documents the 16 default. A caller that raises `max_depth` above 16 gets a writer error from `t64_jsonw_node` (`canon` exits 3), not a crash.
- `run-expect.sh` requires `ok` as a whole word for exit-0 rows, a little stricter than "the output contains ok". The plan text is otherwise followed exactly, including hidden files counting as seeds.
- `t64_jsonw.h` forward-declares `struct t64_json_node` so the writer header does not include the parser header.

## Known Stubs

None.

## Threat Flags

None beyond the plan's register. Mitigations applied: T-01-17 (1 MiB input, depth 16, 4096-byte strings, 4096 nodes and a 127-byte key cap, each with a negative test plus an at-limit positive; recursion bounded by depth, clamped to 64), T-01-18 (all reads bounds-checked; `unit.json` and the 200000-mutation `unit.json-fuzz` run in the gate; the ASan/UBSan preset with `-fno-sanitize-recover=all` ran clean and was shown to catch a seeded over-read), T-01-19 (floats, exponents, `-0` and integers beyond plus or minus 2^53-1 rejected; `t64_json_u64_string` accepts only `0|[1-9][0-9]*` up to 2^64-1), T-01-SC-05 (hand-written, nothing vendored, no installs). A scan of the new files for home paths, temp paths and email addresses found nothing.

## Notes for later plans

- Parser API for 01-07, 01-09, 01-10 and 01-12: `t64_json_parse(buf, len, NULL, &doc, &err)` returns 0 or 1; always `t64_json_free(&doc)` on success. Print failures as `JSON-<t64_json_category_name(err.category)> offset=<err.offset>`. Members of a container are `node->first` then `->next`, each carrying `key`/`key_len`. Use `t64_json_check_keys` for "unknown fields rejected" (returns UNKNOWN, MISSING or NOT_OBJECT with the key in `*bad_key`).
- u64 values travel as decimal strings only; use `t64_json_u64_string`.
- To build a canonical file from a tree, use `t64_jsonw_node` then `t64_jsonw_finish`; the output is never longer than the input plus one LF (the fuzz run asserts this), so `len + 16` is a safe buffer.
- `run-expect.sh --tool "<words>" --dir DIR [--ext ".json .jsonl .md"]`: rows with exit 0 need rule_id `ok`, and the tool must print `ok` as a standalone word. Put new corpora in their own directory with an `EXPECT.tsv`; add a seed and the row in the same change or the run reports `UNLISTED`.
- `t64-json-canon` builds to `build/gate/tools/common/t64-json-canon`; later plans reference it by target name (`$<TARGET_FILE:t64-json-canon>`).
- The `asan` preset exists for plans that add C tools to run `ctest --preset asan`; building it takes about 3 s from clean on this machine. It is deliberately not a gate step.

## Self-Check: PASSED

- FOUND: tools/common/{t64_json.h,t64_json.c,t64_json_canon.c,t64_jsonw.h,t64_jsonw.c,CMakeLists.txt,tests/test_json.c,tests/test_json_fuzz.c}, tests/seeded/run-expect.sh, tests/seeded/json/{EXPECT.tsv,good_canonical.json,bad_spaces.json,bad_unsorted.json,bad_trailing_comma.json,bad_duplicate.json,bad_float.json}, CMakePresets.json
- FOUND commits: 7d68d8f, 50702cd, 89f29a5 (all ancestors of HEAD)
