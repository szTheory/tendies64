---
phase: 11-installed-consumer-and-freeze-candidate
plan: 07
subsystem: api
tags: [init-macros, codemod, core-tests, group-b, shape-wave]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: plan 11-05 (the ten public init macros of DR-09 expanding to today's layouts, scripts/codemod-phase11.py with its bound-path refusal); plan 11-06 (group A and the member-fill pattern)
provides:
  - the 14 files of core/tests group B (RSP, DMA, RDRAM and devices) on the init macros, with unchanged results
  - per-file converted counts for the PIN-IMPACT consolidation of 11-10
affects: [11-08, 11-09, 11-10, 11-12, 11-13, 11-14, 11-15]

actuals:
  tokens: 5800
  tasks: 2
  commits: 1
plan_head_before: 9fcc2e7d5c6b0011a7cbf2051774e8c79c496d73
plan_head_after: a435fa1db196119e1cd94951ca2194e81423cbd8

tech-stack:
  added: []
  patterns:
    - "a byte-wise sentinel fill of a macro-type result (memset 0xa5) becomes a test-local poison helper that assigns the INIT form first and then sets every member by name, so the sentinel keeps its power and the leading struct_size of the flip survives"
    - "a host allocator held as a struct member is filled by one T64_HOST_ALLOCATOR_INIT assignment instead of two or three member assignments (same pattern as 11-06)"
    - "an array of a macro type is initialized with one INIT element per entry"

key-files:
  created: []
  modified:
    - core/tests/test_rsp_compare.c
    - core/tests/test_rsp_control.c
    - core/tests/test_rsp_divider.c
    - core/tests/test_rsp_fuzz.c
    - core/tests/test_rsp_multiply.c
    - core/tests/test_rsp_scalar.c
    - core/tests/test_rsp_transfers.c
    - core/tests/test_rsp_vector.c
    - core/tests/test_sp_dma.c
    - core/tests/test_pi_dma.c
    - core/tests/test_rdram.c
    - core/tests/test_ai.c
    - core/tests/test_isviewer.c
    - core/tests/test_joybus.c

key-decisions:
  - "The six 0xa5 sentinel fills of t64_output_drain_result in test_isviewer.c (five the tool reported as memset, one on a union member it did not) are replaced by a file-local poison_drain_result helper that assigns T64_OUTPUT_DRAIN_RESULT_INIT and then sets every member to a non-zero value by name. A plain INIT assignment would also remove the memset, but it would let a callee that writes zeros pass the refusal tests (result left unchanged) and the short-buffer test (result overwritten), so the sentinel strength of the original is kept"
  - "The ten allocator member fills (nine files: seven RSP fixtures, test_rsp_control.c, test_sp_dma.c with two sites) become one assignment from T64_HOST_ALLOCATOR_INIT each; context stays NULL where it was never set and is the fixture where it was"
  - "The four t64_audio_packet_info arrays of test_ai.c are initialized with one T64_AUDIO_PACKET_INFO_INIT element per entry (2 and 2x2), so each entry carries the leading struct_size of the flip"

patterns-established:
  - "After hand conversion the codemod still reports an initialized array as a hand site (it reports every array declaration); the four test_ai.c lines are therefore the expected remainder of --check, exit 1 by design, and the proof is the grep of T64_AUDIO_PACKET_INFO_INIT on those declarations"

requirements-completed: []

coverage:
  - id: D1
    description: "The 14 files of core/tests group B use the DR-09 init macros: positional brace initializers, compound literals and uninitialized declarations are converted, four arrays, ten allocator members and six drain-result sentinel fills are done from the INIT form, and no oracle-bound file, expected value, check, case name or control flow changed"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "python3 -B scripts/codemod-phase11.py --check over the 14 files: RED exit 1 (157 remaining: 148 convertible, 9 hand) at plan_head_before; after conversion only the 4 initialized test_ai.c arrays (hand by tool design) and 10 member notes remain; python3 -B scripts/codemod-phase11.py --dir core/tests/test_rdp.c exits 2 (refused: oracle-bound)"
        status: pass
      - kind: integration
        ref: "cmake --build --preset gate; ctest -R group regex (21 of 21, 7.4 s); ctest -L unit (128 of 128, 135.6 s); sampling set (5 of 5, 51.9 s); ctest --preset gate 268 of 268 in 130.5 s; sh scripts/pii-scan.sh --tree findings=0"
        status: pass
    human_judgment: false

duration: 10min
completed: 2026-11-25
status: complete
---

# Phase 11 Plan 07: Core Tests Codemod, Group B (RSP, DMA, RDRAM and Devices) Summary

**148 initializer sites in the 14 core/tests group B files moved to the DR-09 init macros by `scripts/codemod-phase11.py` in one compiling commit, plus 20 hand conversions from the INIT form (4 arrays, 10 allocator members, 6 drain-result sentinel fills); the full gate (268 tests) passes unchanged.**

## Performance

- **Duration:** 10 min (the host stayed at load 16 to 26 throughout, unrelated to this project, so nothing waited)
- **Started:** 2026-11-25T22:07Z
- **Completed:** 2026-11-25T22:17Z (the summary and the state update follow the commit)
- **Tasks:** 2 of 2 (Task 1 is the tracer and did not commit by plan; Task 2 committed once)
- **Files modified:** 14 (0 created); 255 insertions and 188 deletions

## Accomplishments

- **Tracer (Task 1).** `--check` over exactly the 14 files reported 157 remaining sites (148 convertible, 9 hand: 4 arrays in `test_ai.c`, 5 memset in `test_isviewer.c`) and 10 struct-member notes. `--dir` converted all 148 (brace 114, uninitialized 29, compound literal 5; per-file counts below). The hand sites were then done from the INIT form (next section). The gate build is warning-free and the 21 CTests built from these files pass.
- **Commit (Task 2).** One commit, `a435fa1`, lists exactly the 14 files; no bound file, no deletion. The full `ctest --preset gate` (268 tests), the sampling set (5) and the PII scan pass before it.

## Task Commits

1. **Task 1: Tracer, codemod over group B, hand conversions, build and run** - no commit (plan: Task 1 does not commit)
2. **Task 2: Full gate, sampling-set guards and the group B commit** - `a435fa1` (refactor)

**Plan metadata:** the summary and state commits that follow this file. The measured `commits:` count of 1 is `git rev-list --count 9fcc2e7..a435fa1` at write time.

## Per-file converted counts (consolidated into PIN-IMPACT by plan 11-10)

| File | Converted by tool | Brace | Uninit | Compound | Hand (from the INIT form) | memset hand |
|------|-------------------|-------|--------|----------|---------------------------|-------------|
| core/tests/test_rsp_compare.c | 10 | 10 | 0 | 0 | 1 (`fixture->allocator`) | 0 |
| core/tests/test_rsp_control.c | 8 | 8 | 0 | 0 | 1 (`fixture->allocator`, context = fixture) | 0 |
| core/tests/test_rsp_divider.c | 4 | 4 | 0 | 0 | 1 (`fixture->allocator`) | 0 |
| core/tests/test_rsp_fuzz.c | 6 | 3 | 3 | 0 | 1 (`fixture->allocator`) | 0 |
| core/tests/test_rsp_multiply.c | 8 | 8 | 0 | 0 | 1 (`fixture->allocator`) | 0 |
| core/tests/test_rsp_scalar.c | 16 | 16 | 0 | 0 | 1 (`fixture->allocator`) | 0 |
| core/tests/test_rsp_transfers.c | 4 | 4 | 0 | 0 | 1 (`fixture->allocator`) | 0 |
| core/tests/test_rsp_vector.c | 2 | 2 | 0 | 0 | 1 (`fixture->allocator`) | 0 |
| core/tests/test_sp_dma.c | 12 | 12 | 0 | 0 | 2 (`value->allocator`, two fixture init functions, context = value) | 0 |
| core/tests/test_pi_dma.c | 22 | 14 | 7 | 1 | 0 | 0 |
| core/tests/test_rdram.c | 5 | 5 | 0 | 0 | 0 | 0 |
| core/tests/test_ai.c | 30 | 12 | 15 | 3 | 4 (arrays `whole_info`, `split_info`, `interleaved_info`, `reference_info`) | 0 |
| core/tests/test_isviewer.c | 18 | 13 | 4 | 1 | 6 sentinel fills (1 in `test_public_output_and_input_seams`, 4 in `test_output_drain_refusals_and_isolation`, 1 on `overlap.result`) | 5 (reported by the tool; the sixth, on the union, was not) |
| core/tests/test_joybus.c | 3 | 3 | 0 | 0 | 0 | 0 |
| **Total** | **148** | 114 | 29 | 5 | 20 (4 arrays, 10 allocators, 6 sentinel fills) | 5 reported (+1 on a union) |

Brace plus uninitialized plus compound equals 148 (114 + 29 + 5). The tool's own per-file lines are the source. The commit message of `a435fa1` states the breakdown as "brace 107, uninitialized 33, compound literal 8", which is wrong (a miscount while writing the message); the totals of 148 and of the 20 hand conversions in it are right, and this table is the record. The commit is not amended, because the project forbids rewriting history.

State at the end of this plan for the later plans (`--check`, measured): core/tests remaining 320, hand 50 (43 memset of a macro-type object, 3 arrays in `test_vg_gaps.c`, and the 4 already converted `test_ai.c` arrays that the tool still lists), 9 bound files excluded; runner/tests remaining 30, hand 2 (both memset), 1 bound file excluded. The 320 are the 473 of the end of 11-06 less the 148 converted here and the 5 memset sites replaced by hand.

## Hand conversions in detail

- **Allocator members (10 fills in 9 files).** The fixtures of the RSP files (`compare`, `divider`, `multiply`, `vector`, `transfers`, `fuzz`, `scalar`, `control`) and `test_sp_dma.c` filled `t64_host_allocator allocator` member by member after a `memset` of the fixture and pass its address to `t64_machine_create`. Each is now one assignment `fixture->allocator = (t64_host_allocator)T64_HOST_ALLOCATOR_INIT(fixture_allocate, fixture_release, NULL);` (context is the fixture in `test_rsp_control.c` and in the two `test_sp_dma.c` init functions, where the old code set it). The values are identical; the flip changes only the macro body.
- **Audio info arrays (4 in `test_ai.c`).** `whole_info[2]`, `split_info[2]`, `interleaved_info[2][2]`, `reference_info[2][2]` were uninitialized and filled by `t64_audio_info`/`t64_audio_copy`. Each entry now starts from `T64_AUDIO_PACKET_INFO_INIT`. The `memcmp` comparisons of two entries stay valid because both sides are filled by the same call path.
- **Drain-result sentinel fills (6 in `test_isviewer.c`).** The tool reported five `memset(&x, 0xa5, ...)` calls as hand sites (the short-buffer test and four refusal tests); a sixth fill, `memset(&overlap, 0xa5, ...)` on a union whose member `result` is a `t64_output_drain_result`, was not reported (the tool lists it as a struct member). After the flip, `0xa5a5a5a5` in `struct_size` would make the callee refuse for a reason the test does not intend (for the overlap test, an alias refusal and a size refusal would both give `T64_STATUS_INVALID_ARGUMENT`, so the test would pass without exercising the alias check). All six now call `poison_drain_result`, which assigns the INIT form and then sets every member by name (`copied_bytes`, `required_bytes`, `total_attempted_bytes` to `0xa5a5a5a5a5a5a5a5`, `captured_sha256` bytes to `0xa5`, both flags to `true`). The refusal tests still compare the result against a copy taken after the poisoning, and the short-buffer test still proves the callee overwrote every field it reports. No expected value changed.

## New non-bound callers (grep first)

The 14 listed files were checked against the codemod's own report: every group B file that uses a macro type is in the plan's list; no additional group B file was found. The three members of the 11-04 running note (`test_api_surface.c`, `test_api_poison.c`, `test_api_lifecycle.c`) belong to 11-09's list and were not touched.

## Decisions Made

Binding decision is DR-09. The choices inside the plan's discretion are in the frontmatter (the poison helper, the single allocator assignment, one INIT element per array entry).

## Deviations from Plan

None - plan executed exactly as written.

Two observations that are not deviations, recorded for transparency:

- The Task 1 automated verify runs `python3 -B scripts/codemod-phase11.py --check` over the 14 files and expects exit 0. The tool reports every array declaration of a macro type as a hand site whether or not it is initialized, so the four converted `test_ai.c` arrays still print as `hand` and `--check` exits 1 by tool design. The ten member notes do not affect the exit status (the 11-06 pattern). Nothing remains that is convertible: after conversion `--check` reports `remaining 4 (convertible 0, hand 4)`, all four being the initialized arrays, and `grep -n T64_AUDIO_PACKET_INFO_INIT core/tests/test_ai.c` shows each of them carrying the INIT form. The plan forbids writing outside the 14 files, so `scripts/codemod-phase11.py` was not changed to recognize initialized arrays; the later plans (11-08 to 11-10) hit the same remainder with `test_vg_gaps.c`.
- The plan action speaks of "arrays or struct-member sites the tool reported"; the poison helper for the sentinel fills of `test_isviewer.c` goes one step beyond a plain INIT assignment, to keep the sentinel's detection power (see Decisions Made). The sites are the plan's stated hand conversions, so this is not tracked as a deviation.

**Total deviations:** 0. **Impact:** none on scope.

## Authentication Gates

None.

## Issues Encountered

- **Unrelated host load.** The host load average was 16 to 26 during the whole run (other projects' processes). No run was repeated and no limit was widened; the unit label took 135.6 s and the full gate 130.5 s, close to the 123 s the plan quotes.
- **zsh array expansion.** The first `--check` invocation passed the 14 paths as one word (an unquoted scalar variable in zsh), and the tool refused it as a nonexistent path (exit 2, nothing written); re-run with a real array.

## Open items for later plans (not acted on here)

- Initialized arrays of a macro type stay in the tool's `--check` output as hand sites (see Deviations). The init-form guard of 11-10 (`scripts/check-api-init.py`) must accept an array whose declaration initializes every entry from the INIT form, or `test_ai.c` and `test_vg_gaps.c` will fail it.
- The sentinel poison pattern of `test_isviewer.c` is a candidate for the other groups that fill a macro-type result with a non-zero byte (grep `memset(&` with a non-zero value in 11-08 to 11-10); each such site needs the same treatment, because the tool's memset detection does not tell a zeroing call from a sentinel fill.
- Struct members of macro type in unions (here `overlap.result`) are listed as members by the tool and are easy to leave behind; the later groups should grep their `union {` declarations.

## Verification Results

| Check | Result |
|-------|--------|
| `python3 -B scripts/codemod-phase11.py --check` over the 14 files | RED: exit 1, 157 remaining (148 convertible, 9 hand); after conversion: exit 1, remaining 4, convertible 0 (the four initialized arrays, hand by tool design), 10 member notes |
| `python3 -B scripts/codemod-phase11.py --dir core/tests/test_rdp.c` | exit 2, "refused core/tests/test_rdp.c: oracle-bound" |
| `cmake --build --preset gate` | clean |
| group CTest regex `^(unit\.core\.(isviewer\|rdram-protocol\|ai\.(tracer\|profile\|old-profile-refusals\|registers\|cadence\|output\|determinism)\|joybus\.parser\|pi-dma\|sp-dma\|rsp-(tracer\|control\|scalar\|transfers\|vector\|multiply\|compare\|divider\|fuzz)))$` | 21 of 21 passed (7.4 s) |
| `ctest --test-dir build/gate -L unit` | 128 of 128 passed, 135.6 s (load about 17) |
| sampling set (`harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `repo.rsp-no-hle`) | 5 of 5 passed, 51.9 s |
| `ctest --preset gate` | 268 of 268 passed, 130.5 s (load 20 at start) |
| `sh scripts/pii-scan.sh --tree` | findings=0 |
| `git show --name-only --format= HEAD` | exactly the 14 files; no bound file, no deletion |

## Known Stubs

None.

## Threat Flags

None. T-11-60 (test semantics of group B) is mitigated by one compiling commit, the group's binaries, the unit label, the full gate and the sampling set before the commit, initializer forms only, and the diff left for the 11-15 review.

## Next Phase Readiness

Plan 11-08 can apply the same tool to the next core/tests group, with the sentinel-fill and union notes above. API-01 is not complete: the remaining codemod groups, the flip, the `struct_size` prologues and the installed consumer are ahead. Nothing in this plan is orchestrator-only and no owner decision is pending.

## Self-Check: PASSED

- FOUND: all 14 modified files, `core/tests/test_rsp_compare.c` containing `_INIT`
- FOUND commit: a435fa1 (ancestor of HEAD)
