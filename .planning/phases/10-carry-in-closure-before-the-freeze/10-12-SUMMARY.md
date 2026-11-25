---
phase: 10-carry-in-closure-before-the-freeze
plan: 12
subsystem: core
tags: [state-import, state-26, depth, hidden-table, render-proof, validate-then-apply, fuzz, mutants, core-06]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: the four si-v2 ids and the t64.state.si.26 walker with the depth scalars and the hidden table in section 8 (plan 10-09), state 26 export with container version word 2, the version pairing and the independent v2 decoder (plan 10-11), the mutant driver and the preflight (plan 10-01), the Wave 2 base 6dd7859 (plan 10-08)
  - phase: 09-controller-save-media-and-replay
    provides: the state container importer (validate pass, then one infallible commit), the import helper header, the fuzz campaign, the frozen hostile rows and the field-use audit

provides:
  - "the state 26 importer: the domain text follows the machine, the five depth scalars are read before the triangle-render proof, the hidden table is closed per byte and written by the commit, a state 25 machine keeps both zero"
  - "depth-inclusive mid Z-triangle and mid scan-out round trips that continue to the same digests, video and audio packet bytes and save bytes (ROADMAP SC2)"
  - "docs/spec/state-mutants-v26.txt (16 hostile rows), the v26 fuzz entry and windows, and four importer mutants"
affects: [10-13, 10-15, phase-11]

actuals:
  tokens: 21600
  tasks: 4
  commits: 6

tech-stack:
  added: []
  patterns:
    - "a reader whose proof needs members that sit later in the stream must read them first: the depth scalars are inside the RDP block precisely so the render proof sees them (Research Pitfall 3)"
    - "a validate pass that must not write the machine re-proves an acceptance check over the blob values with a written-out copy of its arithmetic and pins the copy against the live function with a parity test"
    - "a new inventory file next to a frozen one, read through its own environment variable and compared name by name with a C table, leaves the frozen file and its count sentence untouched"

key-files:
  created:
    - core/tests/test_state_import_v26.c
    - docs/spec/state-mutants-v26.txt
  modified:
    - core/t64_state.c
    - core/tests/fuzz_state_import.c
    - core/CMakeLists.txt
    - docs/spec/state-field-rules.txt
    - docs/spec/state-container.md
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase10-regexes.txt
    - scripts/p10-mutants.py
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md

key-decisions:
  - "The canonical domain text is a function of the machine (state_domain_for): t64.state.si.26 for the v26 tuple, t64.state.si.25 for the v25 tuple. The digest recomputation and the domain check use it, so a blob forged to the other text and fully re-signed is refused at the domain bytes (SECTION_BOUNDS) and a v26 machine still refuses a v25-domain blob and the reverse"
  - "The scalars use ADDR_RANGE (aligned to 8, first halfword inside RDRAM: what SetZImage accepts), BOOL_01 and a 16-bit INDEX_LT range (SetPrimDepth keeps the low halves); the hidden table length is PROFILE_DERIVED and each byte ENUM_CLOSED to 0 or 0x80 to 0x83. The pixel extent of a Z triangle in flight stays part of the render proof (LEN_LE) rather than a second address rule"
  - "The Z product bound of the acceptance preflight is re-proved from the blob values by a written-out copy (state_z_products_bounded) because the preflight reads the live registers and a validate pass must not write them; the frame limit of 2,048 bytes rules out a machine copy. A parity case compares the verdict with the live preflight"
  - "state 25 behavior is unchanged: both depth members stay zero after a state 25 import (case v25-hidden-zero), unit.core.state.import passes unchanged, and no frozen or oracle-bound file moved"

patterns-established:
  - "A RED that a setup refusal cannot fake: the layout step landed first and the idle v26 blob had to import in the same run before the in-flight Z triangle was allowed to be the failing case"
  - "A packet-bearing field boundary is the unit of continuation comparison when the VI retains a packet on only some field events; digests are compared at the first three and at the end because a digest per event costs about 30 ms"

requirements-completed: [CORE-06]

coverage:
  - id: D1
    description: "A Z triangle in flight (opcode 0x09, first row behind it, depth live) exports and imports on the state 26 stem id; the importer reads the five depth scalars before the render proof; digests, color and depth bytes and the 2,097,152 hidden bytes equal at import and after 2,000 more ticks on both machines"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "ctest unit.core.state.import-v26-roundtrip (case roundtrip, mid-z-triangle; 19,760 checks, 0 failures; ASan 30.8 and 39.5 s)"
        status: pass
      - kind: other
        ref: "RED at 5e4aa1e: idle-v26-precheck passed in the same run and mid-z-triangle was refused with status 32 reason 26 section 8 rule 11 (the triangle-render proof, LEN_LE); GREEN at 1083257"
        status: pass
    human_judgment: false
  - id: D2
    description: "A Z-tested 4x3 frame inside a scan-out with a retained video packet and two PCM DMAs in flight imports on -s32k (one SRAM write) and -e4k (one EEPROM write) and continues equal: digests at three packet-bearing field boundaries and at the end, 138 video packets and 2 audio packets drained equal, save bytes and busy flag equal"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "ctest unit.core.state.import-v26-roundtrip (mid-scan-out lines for both ids: 3 field boundaries compared with digests, 1,096 field events run)"
        status: pass
    human_judgment: false
  - id: D3
    description: "CORE-06 empty edge: an idle v26 state with an all-zero hidden table round-trips on all four si-v2 ids, and a state 25 blob imported into a state 25 machine whose hidden table was dirtied byte by byte leaves all 2,097,152 hidden bytes and the depth address zero"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "ctest unit.core.state.import-v26-roundtrip (idle-v26-precheck lines for the four ids; v25-hidden-zero: 2097152 of 2097152 bytes zero)"
        status: pass
    human_judgment: false
  - id: D4
    description: "CORE-06 adjacency edge: a blob whose Z triangle is in flight but depth_image_valid is false refuses with the named rule LEN_LE of the render proof, and the depth scalars are read before that proof"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "ctest unit.core.state.import-v26-hostile (row v26-z-triangle-without-depth-image: STATE_RANGE FIELD_RANGE section 8 LEN_LE)"
        status: pass
      - kind: other
        ref: "python3 -B scripts/p10-mutants.py --run v26-import-depth-after-proof: KILLED by test_state_import_v26:roundtrip"
        status: pass
    human_judgment: false
  - id: D5
    description: "Every new member has a named rule checked by STATE_CHECK, audit rows (depth_image_address ADDR_RANGE, rdp_depth_hidden ENUM_CLOSED), 16 hostile rows in docs/spec/state-mutants-v26.txt matched name by name with the C table, and v26 fuzz windows over the scalars, the hidden length, the hidden entries and the pending Z packet words"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "ctest unit.core.state.import-v26-hostile (16 rows with their own tuple; fuzz-v26-idle 24 iterations and fuzz-v26-mid-z-triangle 24 iterations, 0 violations; 16,689 checks; ASan 119.4 and 111.9 s); ctest harness.state-field-use-audit (findings=0)"
        status: pass
    human_judgment: false
  - id: D6
    description: "Every refused v26 import leaves the machine allocation byte identical and the allocator counters unchanged (validate-then-apply), for the 16 rows and for the fuzz campaigns"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "run_hostile_row snapshot_matches and t64_state_summary_is_clear per row; state_fuzz_one contract per iteration"
        status: pass
    human_judgment: false
  - id: D7
    description: "The four importer mutants are killed and the whole mutant set and both controls behave as expected"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "python3 -B scripts/p10-mutants.py --run (committed tree 4ac1621): 25 mutants KILLED (v26-import-skip-hidden and v26-import-depth-after-proof by import-v26-roundtrip, v26-import-addr-unchecked and v26-import-z-products-unchecked by import-v26-hostile), equivalent control SURVIVED, non-compiling control INVALID; --check-anchors exit 0"
        status: pass
    human_judgment: false
  - id: D8
    description: "State 25 and the shipped ids are byte-identical and the Phase 9 import inventory is unchanged: digest sweep against the Wave 2 base shows 0 differences, unit.core.state.import passes unchanged, the phase09 state-import case inventory validates, and no frozen, pinned or oracle-bound file moved"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "python3 -B scripts/digest-sweep.py --base 6dd785986f6aa78c6da5573a558b15b386a4edee --head WORKTREE: 1599 shared lines, 0 differences, 0 head-only lines (296 with the four --head-extra-id); python3 -B scripts/check-phase09-cases.py --build build/gate --suite state-import: validated nonempty state-import phase09 case inventory; git diff --quiet 2c70668..HEAD on test_state_import.c, state-mutants.txt, state_import_helpers.h, phase09-cases.json and decode_container.py exits 0; empty intersection with the 13 EXPECTED_CORPUS_PATHS"
        status: pass
    human_judgment: false
  - id: D9
    description: "ASan seconds, final TIMEOUTs and the importer documentation: gate TIMEOUT 300 s for both names (native 1.9 and 6.8 s), sanitizer TIMEOUT 600 and 900 s against 30.8 to 39.5 and 111.9 to 119.4 s; state-container.md documents the domain per machine, the reader order, the per-member rules and the v26 inventory beside the frozen v25 one"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "sh scripts/preflight-phase10.sh at 5cc66ab: PASS 63 s (timeout-audit 101 TIMEOUTs 0 too low, 227 units compiled with the act image cc 13.3.0, 11 post-Gate steps replayed); sh scripts/check-docs.sh ok; ctest --test-dir build/gate -j4: 248 of 248 pass"
        status: pass
    human_judgment: false
  - id: D10
    description: "Whether re-proving the Z product bound by a written-out copy in core/t64_state.c (guarded by a parity case and a mutant) is the right design, against refactoring the sealed t64_rdp_depth_triangle_preflight to take a register view"
    requirement: "CORE-06"
    verification: []
    human_judgment: true
    rationale: "The copy keeps the sealed depth bodies and their pins untouched and is tied to the live function by one parity case on two packets; a view-taking preflight would remove the duplication but moves sealed bodies and their re-pin, an orchestrator decision"

duration: 38min
completed: 2026-11-25
status: complete
commits: 6
plan_head_before: 2c70668f942b650e49884f3f6315943ee1ced8f4
plan_head_after: 5cc66ab44844e66eb3168f7c1e0b030e869f0334
---

# Phase 10 Plan 12: State 26 import, depth-inclusive round trips and hostile coverage Summary

**A state 26 machine now imports what it exports: the domain text follows the machine, the five depth scalars are read before the triangle-render proof so a Z triangle in flight imports, the 2,097,152-byte hidden table is closed per byte and written by the commit, every refusal leaves the allocation byte for byte as it was, and 16 hostile rows, v26 fuzz windows and four killed mutants bound every new member (state 25 stays byte-identical, sweep 1599 lines and 0 differences).**

## Performance

- **Duration:** 38 min (2026-11-25T01:03Z to 01:41Z); the host 1-minute load stayed at 8 to 13, so seconds are upper bounds
- **Tasks:** 4 (Task 1 tracer with RED and GREEN commits, Tasks 2 to 4 auto), 6 commits before this file
- **Files:** 11 changed (2 created, 9 modified); 1 new test binary, 2 new CTest names, 4 new mutants

## Accomplishments

- **Task 1 (tracer, RED `5e4aa1e`, GREEN `1083257`).** The reader first learned the state 26 layout while keeping the state 25 semantics: `state_domain_for` gives the domain text per machine (parse table, digest recomputation), `state_read_rdp` consumes the five scalars between the triangle coverage and `work_policy_version`, `state_read_rdp_carried` consumes the hidden length and table. With that setup in place `unit.core.state.import-v26-roundtrip` passed `idle-v26-precheck` (10,545,971 bytes, same run) and failed `mid-z-triangle` with `status 32 reason 26 section 8 rule 11` (`STATE_RANGE`, `FIELD_RANGE`, `LEN_LE`, position 0x60d277): the proof of `state_check_triangle_render` saw `depth_valid` false. GREEN reads the scalars first with named rules (`ADDR_RANGE`, `BOOL_01`, 16-bit `INDEX_LT`), hands the blob values to the proof, stores them in apply mode, re-proves the Z product bound, closes the hidden table per byte and writes it from the blob; a state 25 machine still zeroes the depth address and the table. Tracer gate: both `<verify>` commands re-run on the committed tree (automated-only verify, `end-of-phase` mode) and passed, so expansion went ahead.
- **Task 2 (`a40c84e`).** The roundtrip case runs an idle round trip on all four ids; the mid Z-triangle; a Z-tested 4x3 frame in a scan-out on `-s32k` (SRAM word 0xCAFEBABE) and `-e4k` (EEPROM block, busy window open), each continued for 1,096 field events with 3 packet-bearing boundaries compared by digest, 138 video and 2 audio packets drained and compared byte for byte, save bytes and busy flag compared; and `v25-hidden-zero` (a hidden table dirtied to `0x80 | (i & 3)` on `si-v1-s32k`, all 2,097,152 bytes zero after a state 25 import).
- **Task 3 (`6634d8e`).** `docs/spec/state-mutants-v26.txt` (16 rows: six scalar rows, three hidden-table rows, three Z-triangle rows, two domain rows, two state-pairing rows) and `unit.core.state.import-v26-hostile`: the file and the C table agree name by name and tuple by tuple, each row is applied to a re-signed blob and reaches exactly its tuple with the allocation and counters unchanged, one parity case compares the importer's Z product proof with the live preflight, and `state_fuzz_campaign_v26` (si-v2 peer, state 26 domain, v26 windows) runs 24 iterations on the idle blob and 24 on the mid Z-triangle blob with 0 violations. The Phase 9 entry and the phase09 case inventory are unchanged.
- **Task 4 (`4ac1621`, `5cc66ab`).** Four mutants (`v26-import-skip-hidden`, `v26-import-depth-after-proof`, `v26-import-addr-unchecked`, `v26-import-z-products-unchecked`), a multi-source test binary in the mutant driver, the ASan run of both names, the final TIMEOUTs and the importer section of `state-container.md`.

## Verification

| Check | Result |
|-------|--------|
| RED (`5e4aa1e`, `build/gate/10-12-task1-red.txt`) | `idle-v26-precheck: ... si-v2 imported, 10545971 blob bytes`; `mid-z-triangle: status 32 reason 26 section 8 rule 11 position 0x60d277`; 1 failure of 95 checks, the import status of the in-flight Z triangle |
| `cmake --build --preset gate && ctest -R '^unit\.core\.state\.import-v26-roundtrip$'` | pass, 19,760 checks, 1.9 to 2.0 s |
| `ctest -R '^unit\.core\.state\.import$'` | pass, 44 to 54 s (the v25 importer, unchanged) |
| `ctest -R '^(unit\.core\.state\.import-v26-.*\|unit\.core\.state\.import\|harness\.state-field-use-audit)$'` | 4 of 4 pass; `harness.state-field-use-audit` findings=0 (93 used members) |
| `ctest -N -R '^unit\.core\.state\.import-v26'` | 2 tests (HEAD 0); `docs/spec/state-mutants-v26.txt` 16 data rows |
| `check-phase09-cases.py --build build/gate --suite state-import` | validated nonempty state-import phase09 case inventory |
| digest sweep, Wave 2 base `6dd785986f6aa78c6da5573a558b15b386a4edee` (40-hex checked, `git cat-file -e`) | 1599 shared lines, 0 differences, 0 head-only; 296 head-only with the four `--head-extra-id` |
| `python3 -B scripts/p10-mutants.py --run` (committed tree) | 25 KILLED, equivalent control SURVIVED, non-compiling control INVALID; `--check-anchors` exit 0 |
| ASan (`ctest --test-dir build/asan -R '^unit\.core\.state\.import-v26-'`, two runs) | roundtrip 39.48 and 30.82 s, hostile 119.44 and 111.90 s, no sanitizer report |
| `sh scripts/preflight-phase10.sh` (clean committed tree `5cc66ab`) | PASS 63 s: 40 regex rows 0 mismatches, 24 `phase10` labels, timeout audit 101 TIMEOUTs 0 too low, 227 units compiled with the act image cc 13.3.0, 11 post-Gate steps replayed, PII clean (host load 8.1, WARN) |
| `ctest --test-dir build/gate -j4` (whole gate preset) | 248 of 248 pass, 70 s; `sh scripts/check-test-inventory.sh --preset gate` PASS count 248 |
| `sh scripts/check-docs.sh`, `sh scripts/pii-scan.sh --tree` | ok, findings 0 |
| native seconds of the new names | 1.87 to 2.03 s and 6.80 to 7.92 s; final TIMEOUTs gate 300 s and 300 s, sanitizer 600 s and 900 s |

## Task Commits

1. **Task 1: mid Z-triangle round trip tracer** - `5e4aa1e` (test, RED with the layout step) and `1083257` (feat, GREEN)
2. **Task 2: mid scan-out, idle and v25 hidden-zero round trips** - `a40c84e` (test)
3. **Task 3: hostile rows, field rules and fuzz windows** - `6634d8e` (test)
4. **Task 4: importer mutants, TIMEOUT audit and docs** - `4ac1621` (test), `5cc66ab` (docs)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical] The Z product bound of the acceptance preflight is re-proved from the blob values**
- **Found during:** Task 1 (reading `t64_rdp_triangle_bounds` and `t64_rdp_depth_triangle_preflight`)
- **Issue:** the plan feeds the blob scalars to the render proof (`t64_rdp_triangle_extent`), but acceptance runs a second function, `t64_rdp_depth_triangle_preflight`, whose product bound keeps `dzdx * dx_delta` and its two siblings inside the signed 64-bit range. The extent function does not run it, so a crafted packet with maximal Z derivatives and a maximal major slope would import and overflow in `t64_rdp_depth_evaluate_pixel` (undefined behavior). The shade and texture products are already proved by the extent (Phase 9 FX-01); the Z products were not. Research row T-10-26 says "re-run the triangle extent/preflight proof from blob values before commit".
- **Fix:** `state_z_products_bounded` (a written-out copy of the preflight arithmetic over the packet words, with the pixel span of the acceptance) runs in `state_check_triangle_render` for the four Z opcodes. The preflight itself cannot be called: it reads the live registers (a validate pass must not write them) and an 80 KB machine copy breaks the 2,048-byte frame limit. A parity case in the hostile test compares the importer's verdict with the live preflight on the benign packet (accepted) and the packet of the row `v26-z-triangle-products` (refused), and a fourth mutant removes the check.
- **Files modified:** `core/t64_state.c`, `core/tests/test_state_import_v26.c`, `docs/spec/state-mutants-v26.txt`, `scripts/p10-mutants.py`
- **Commits:** `1083257`, `6634d8e`, `4ac1621`

**2. [Rule 3 - Blocking] Task 1 is two commits and carries the field-use rows**
- **Issue:** the tracer is a RED then a GREEN, and the audit gate (`harness.state-field-use-audit`) must pass at every commit once the reader uses `rdp_depth_hidden` in a loop and reads `depth_image_address`.
- **Fix:** `5e4aa1e` is the RED test plus the layout step, `1083257` is the GREEN reader plus the two audit rows (`depth_image_address :: ADDR_RANGE :: state_read_rdp` replacing `PROFILE_DERIVED :: state_commit`, `rdp_depth_hidden :: ENUM_CLOSED :: state_read_rdp_carried`). Only the members the scan lists need rows (the audit rejects stale rows), so the other three scalars are covered by `state-container.md` and the hostile rows.
- **Files modified:** `docs/spec/state-field-rules.txt`, `core/t64_state.c`
- **Commit:** `1083257`

**3. [Rule 3 - Blocking] The mutant driver needs a multi-source test binary and the hostile inventory path**
- **Issue:** `test_state_import_v26` links `fuzz_state_import.c` and reads `T64_STATE_MUTANT_INVENTORY_V26`; the driver compiled one source per binary and set no such variable.
- **Fix:** `TEST_SOURCES` values may be lists (`source_list`, `object_name`), and the worktree environment names the v26 inventory.
- **Files modified:** `scripts/p10-mutants.py`
- **Commit:** `4ac1621`

### Plan and code observations (no behavior deviation)

- **Continuation unit.** The VI retains a packet on only some field events (field events are 10 to 11 ticks apart on the build_vi_frame register set) and an AI sample takes about 1,100 ticks, so "two more field boundaries" is read as packet-bearing boundaries: digests at the first three and at the end (a digest costs about 30 ms), packets drained and compared at every one until the two retained PCM DMAs have played out as audio packets (1,096 field events). A slower or faster DAC rate is not an option: the rate pair is bounded below by `66 * (bitrate + 1)`.
- **`rdp_depth_hidden` is a new audited member.** The audit lists 93 used members now (92 before); `state-container.md` quotes 93.
- **Depth rules.** The plan's `ADDR_RANGE` "image plus the scissor extent" is two layers: the base rule (aligned to 8, first halfword in RDRAM, as `SetZImage` accepts) for every state, and the pixel extent through the render proof for a Z triangle in flight; a generic extent rule for an idle state would refuse states the machine can legally reach.
- **RED evidence.** The classifier `gsd_run check tdd-red-evidence` has no adapter for CTest output (as in plans 10-09 and 10-10); the RED is recorded here and in PIN-IMPACT.md section 2.3, and the report text is kept in the ignored `build/gate/10-12-task1-red.txt`.
- **Mutants need a committed tree** (as in plans 10-10 and 10-11): the full set ran on `4ac1621`; no production line changed afterwards.
- **No windows ledger.** `.planning/WINDOWS.md` does not exist, so no entry was appended; there are no stubs, skipped tests or unrun verifies.

**Total deviations:** 3 (1 Rule 2, 2 Rule 3). **Impact:** one extra proof function and one extra mutant for the Z products, the audit rows in the GREEN commit, a driver extension; no change to any pin, sealed body, oracle-bound file, frozen inventory or phase09-inventoried binary.

## Issues Encountered

- The first hostile run failed one base assertion: the Z triangle's first pixel is hidden entry 0x3008, not 0x3000. The assertion now compares entries 0x3000 to 0x303F with the machine and requires a written entry only for the Z base.
- The first continuation attempt queued a PCM DMA at every boundary; the AI holds two descriptors and answered `T64_STATUS_QUEUE_FULL` (11), and the VI produces packets on some events only. The loop was rewritten around packet-bearing boundaries (see the observation above).
- A fast DAC rate experiment (63) is refused by the rate-pair rule and produced no packets; the proven 1103/15 pair stays.

## Authentication Gates

None.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-26 mitigated (`ADDR_RANGE`, the extent and the Z product proof from the blob values, hostile rows, v26 fuzz windows, two mutants). T-10-27 mitigated (`ENUM_CLOSED` per byte, exact length, rows `v26-hidden-byte-outside-set`, `v26-hidden-length-short`, `v26-hidden-table-truncated`). T-10-28 mitigated (two separate CTest names, sampled matrix, TIMEOUT 300 s native against 1.9 and 6.8 s, sanitizer 600 and 900 s against 39.5 and 119.4 s). T-10-29 mitigated (validate-then-apply; 16 rows and 48 fuzz iterations assert the allocation byte for byte and the counters). Blobs stay in process memory.

## Next Phase Readiness

Ready for plan 10-13 (the Wave 2 review and push). It starts from: a state 26 machine that imports its own exports (idle, mid Z-triangle, mid scan-out) on all four ids, `unit.core.state.import-v26-roundtrip` and `unit.core.state.import-v26-hostile`, the v26 inventory, four importer mutants and the preflight at `5cc66ab`. For the review: the Z product copy (`state_z_products_bounded`) and its parity case (deviation 1, coverage item D10) are the one design point for the orchestrator. The runner format-string pairing of plan 10-15 still uses `decode_container_v2.py` (plan 10-11, deviation 1); the importer now accepts the blobs that decoder reads.

## Self-Check: PASSED

- Files found: `core/tests/test_state_import_v26.c`, `docs/spec/state-mutants-v26.txt`, `core/t64_state.c`, `core/tests/fuzz_state_import.c`, `docs/spec/state-container.md`, `docs/spec/state-field-rules.txt`, `scripts/p10-mutants.py`.
- Commits found as ancestors of HEAD: `5e4aa1e`, `1083257`, `a40c84e`, `6634d8e`, `4ac1621`, `5cc66ab`; `git rev-list --count 2c70668..HEAD` is 6 before this file.
- `core/tests/test_state_import.c`, `docs/spec/state-mutants.txt`, `core/tests/state_import_helpers.h`, `docs/spec/phase09-cases.json` and `fixtures/state/decode_container.py` are unchanged against `2c70668`; none of the 13 oracle-bound paths changed.
