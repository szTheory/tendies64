---
phase: 10-carry-in-closure-before-the-freeze
plan: 09
subsystem: core
tags: [identity-bump, descriptor-13, state-26, rdp-depth, digest-v26, isviewer, completeness-test, red-green, vid-07, core-06]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: PIN-IMPACT.md one-way identity record (ids, tuple 13/21/26, domain), the K1 refusal tests (plan 10-02), the digest sweep and the Wave 2 base 6dd7859 (plans 10-02 and 10-08), the mutant driver (plan 10-01), the preflight (plan 10-01)
  - phase: 09-controller-save-media-and-replay
    provides: the descriptor 12 family, the v25 walker and framing, the completeness member table of test_digest_v25.c (copied, never edited)
provides:
  - four registered ids ntsc-cart-4m-openipl3-rdram-b-t1-si-v2 (+ -e4k, -e16k, -s32k) with tuple 13/21/26, pinned profile hashes and RDP depth active
  - t64_has_si_v25_profile and t64_has_si_v26_profile (exact), t64_has_si_profile widened to the SI device family, t64_has_rdp_depth_profile with the v26 term outside the descriptor 10 conjunction
  - digest domain t64.state.si.26 with the five depth scalars inside the RDP block (new-span class) and the hidden depth table in the section 8 tail
  - ISViewer window active on the eight SI-family ids and still refused outside the family
  - unit.core.profile.v26-identity, unit.core.profile.v26-isviewer, unit.core.digest.v26-completeness, unit.core.digest.v26-strip-differential
affects: [10-10, 10-11, 10-12, 10-13, 10-14, phase-11]

actuals:
  tokens: 26450
  tasks: 3
  commits: 5

tech-stack:
  added: []
  patterns:
    - "exact tuple predicates for digest, registry and exporter; one widened family predicate for device gates, so no sealed .c body text changes"
    - "completeness flip test written and run RED against the registered but depth-inactive family before the walker change"
    - "a new descriptor gets the full identity mutation sweep (every text, boot and numeric field through public hash, serialize and create) in the same commit as its registration"

key-files:
  created:
    - core/tests/test_profile_v26.c
    - core/tests/test_digest_v26.c
  modified:
    - core/t64_internal.h
    - core/t64_machine.c
    - core/t64_digest.c
    - core/t64_state.c
    - core/t64_isviewer.c
    - core/CMakeLists.txt
    - docs/spec/profile-membership-sites.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase10-regexes.txt
    - scripts/p10-mutants.py
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md

key-decisions:
  - "The four si-v2 profile hashes and serialized lengths are pinned as literals (PIN-IMPACT.md section 2.1); the body equals si-v1 except the id digit, the three version words and boot_status, which reads prepared-rdram-b-t1-si-v2; rdp_depth_policy keeps the shared depth policy text"
  - "t64_has_si_profile is the union of the exact 12/20/25 and 13/21/26 tuples; the digest domain chain, state_version_registered and t64_machine_state_export use the exact predicates, so a state 26 export is refused (INVALID_ARGUMENT) and a state 26 import reports the state-version reason until plans 10-11 and 10-12"
  - "The v26 walker keeps the v25 body and framing: has_v21_body (v21 and not v26) keeps the descriptor 10 block byte-identical, the v26 depth scalars are new-span members before work_policy_version and the hidden table, length-prefixed in the new class, follows the coverage table in the section 8 tail"
  - "t64_has_rdp_depth_profile is now written as (descriptor 10 conjunction) || t64_has_si_v26_profile; the k1-predicate-widened mutant anchor was updated in the same commit and the mutant is KILLED by test_k1_refusal:apply-refusal"

patterns-established:
  - "A RED completeness test is valid only when its first failures name the missing members and nothing else: 32 failures here are exactly five depth members and three hidden-table bytes on four ids"
  - "A strip differential needs a wrong-value control and a direct layout test; two throwaway mutants (scalars in the body class, work_policy_version before the scalars) fail it"

requirements-completed: [VID-07, CORE-06]

coverage:
  - id: D1
    description: "Four registered descriptor 13 ids with tuple 13/21/26, save kinds none / EEPROM 4K / EEPROM 16K / SRAM 32K, id lengths 38/42/43/43, pinned profile hashes and serialized lengths; the serialization differs from the si-v1 sibling in exactly four bytes outside boot_status; every text, boot and numeric field mutation is refused or pinned at create"
    requirement: "VID-07"
    verification:
      - kind: unit
        ref: "ctest unit.core.profile.v26-identity (5567 checks, 0 failures)"
        status: pass
    human_judgment: false
  - id: D2
    description: "t64_machine_create refuses a descriptor 13 tuple under an unregistered id (and the si-v1 body under an si-v2 id and the reverse) with zero allocations"
    requirement: "VID-07"
    verification:
      - kind: unit
        ref: "unit.core.profile.v26-identity check_unregistered_ids (counting allocator, allocations 0)"
        status: pass
    human_judgment: false
  - id: D3
    description: "The widening design leaves every sealed body unchanged: check-rsp-no-hle passes with no re-pin and the membership list is regenerated"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "python3 -B scripts/check-rsp-no-hle.py --build build/gate (exit 0, no FAIL line); ctest repo.si-membership-sites"
        status: pass
    human_judgment: false
  - id: D4
    description: "ISViewer is active on the eight SI-family ids and refused outside the family; RED recorded before the edit"
    requirement: "VID-07"
    verification:
      - kind: unit
        ref: "unit.core.profile.v26-isviewer GREEN (accepted ids=8 refused ids=1); RED before core/t64_isviewer.c edit: ISViewer inactive on all four si-v2 ids, write status 19 and read status 16, accepted ids=4, 17 failures"
        status: pass
    human_judgment: false
  - id: D5
    description: "The completeness flip test was RED for exactly the depth members before activation and is GREEN after it"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "RED at 42f64db: first failure 'digest completeness: depth_image_address is not in the canonical stream', 32 failures = five depth members and three hidden-table bytes on four ids; GREEN at 831c947 (unit.core.digest.v26-completeness, 0.32 s native, 3.6 s ASan)"
        status: pass
    human_judgment: false
  - id: D6
    description: "Depth is active only on the four new ids: SetZImage, SetPrimDepth and the four Z triangle opcodes are accepted with the depth-test literals (0x09a8 and 0x83, 0x0004 and 0x80 for opcode 0x0f); the five shipped composite ids still refuse"
    requirement: "VID-07"
    verification:
      - kind: unit
        ref: "unit.core.profile.v26-identity (accepted effect on four ids); unit.core.k1.dispatch-refusal and unit.core.k1.apply-refusal unchanged and green; unit.core.profile.predicate-matrix green"
        status: pass
    human_judgment: false
  - id: D7
    description: "The v26 stream strips to the v25 stream in five states, ascending sections 1 to 10 (11 for save ids), depth scalars immediately before work_policy_version, hidden table after the coverage table"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "unit.core.digest.v26-strip-differential (idle on four id pairs, mid-render, mid-scan-out, after an SI poll, after a save write on two EEPROM ids; wrong-value control; layout check on four ids)"
        status: pass
    human_judgment: false
  - id: D8
    description: "Shipped ids are byte-identical: digest sweep of the 21 shipped ids against the Wave 2 base, the si-v1 F2 profile-hash pins, K1 and the k1-predicate-widened mutant"
    requirement: "VID-07"
    verification:
      - kind: other
        ref: "python3 -B scripts/digest-sweep.py --base 6dd7859 --head WORKTREE with four --head-extra-id: 1599 shared lines, 0 differences, 64 head-only digest lines per si-v2 id; ctest harness.phase09-qualification pass; p10-mutants.py --run k1-predicate-widened KILLED"
        status: pass
    human_judgment: false
  - id: D9
    description: "Choice of the boot_status label of the new family and of leaving the exact predicates outside the membership token list"
    requirement: "CORE-06"
    verification: []
    human_judgment: true
    rationale: "A label text and a tooling-coverage trade-off; no test asserts that they are the best choices, only that the hashes pin the chosen text and the list still passes its own check"

duration: 23min
completed: 2026-11-24
status: complete
commits: 5
plan_head_before: a895ec8689dbd2d14da165d984b663889b2e1e10
plan_head_after: 2649a715e7d40dce15abd5edb73a5598bbc82a07
---

# Phase 10 Plan 09: Descriptor 13 family, depth activation and the v26 walker Summary

**Four new registered ids (si-v2, tuple 13/21/26) carry RDP depth under the digest domain t64.state.si.26: the completeness flip test was RED for exactly the five depth members and the hidden table, then GREEN after activation; shipped ids, K1 refusal, the four si-v1 F2 pins and the sealed bodies did not move (sweep 1599 lines, 0 differences).**

## Performance

- **Duration:** 23 min (2026-11-24T03:53:35Z to about 04:16Z); host 1-minute load 6 to 10, so seconds are upper bounds
- **Tasks:** 3 (Task 1 tracer, Tasks 2 and 3 tdd), 5 commits
- **Files:** 13 (11 planned plus `scripts/p10-mutants.py` and `PIN-IMPACT.md`); 2 new test binaries

## Accomplishments

- **Task 1 (tracer, `286ccbb`).** `t64_has_si_v25_profile` and `t64_has_si_v26_profile` are exact; `t64_has_si_profile` is their union, so every device gate and the sealed bodies that call it follow the family without a text change. `core/t64_machine.c` has the `T64_SI_V2_FAMILY_COMMON_FIELDS` body (si-v1 body, tuple 13/21/26, `boot_status` `prepared-rdram-b-t1-si-v2`), four descriptors, id helpers, and the family wired into `is_ai_family_profile` (ring storage and the 2 MiB depth sidecar), `has_pinned_openipl3_profile_id`, `is_rdram_b_t1_profile`, `profile_fields_are_valid`, `write_profile` (descriptor 13 added to every descriptor 5 to 12 list), `t64_profile_lookup` and a create refusal of descriptor 13 under an unregistered id before any allocation. The digest takes the domain `t64.state.si.26` first in its chain with v25 framing; `state_version_registered` and `t64_machine_state_export` use the exact v25 predicate. `core/t64_isviewer.c` uses `t64_has_si_profile` plus eight id arrays. `test_profile_v26.c` pins the four profile hashes (`3bc028bd...`, `0a4bfc45...`, `34f767b0...`, `9dfb90d8...`), the serialized lengths 1358, 1362, 1363, 1363, proves the serialization differs from the si-v1 sibling in exactly four bytes, and runs the full identity mutation sweep. `v26-isviewer` was run RED before the `t64_isviewer.c` edit.
- **Task 2 (RED, `42f64db`).** `test_digest_v26.c` copies the v25 member table and flip machinery, turns the five depth members into plain rows, adds first, middle and last byte flips of the hidden table, and a strip differential. On the Task 1 tree the completeness test fails with its first line `digest completeness: depth_image_address is not in the canonical stream` and 32 failures, which are the five depth members and three hidden-table bytes on each of the four ids and nothing else (1556 checks). The strip differential passed on that tree (the v26 stream equals the v25 stream while depth is inactive).
- **Task 3 (GREEN, `831c947`).** `t64_has_rdp_depth_profile` gains `|| t64_has_si_v26_profile` outside the descriptor 10 conjunction. The walker keeps the descriptor 10 depth block byte-identical (`has_v21_body`), streams the five scalars at the v21 position as new-span members for v26, and ends section 8 with the length-prefixed hidden table after the coverage table; a missing hidden table refuses with BAD_STORAGE. `v26-identity` now asserts the accepted effect of SetZImage, SetPrimDepth and the four Z triangle opcodes with the depth-test literals on each id. Completeness is GREEN.
- **Layout pin (`f9d8f1f`).** A direct layout test proves the 20-byte scalar block sits immediately before `work_policy_version` and the section 8 tail is the 88-byte combiner block, the coverage table, then the hidden table ending at the section 9 marker; two throwaway mutants (scalars in the body class; `work_policy_version` before the scalars) both fail the strip differential.
- **Record (`2649a71`).** PIN-IMPACT.md section 2.1 holds the four hashes and lengths, the stream layout, the predicate design and the unmoved pins.

## Verification

| Check | Result |
|-------|--------|
| Task 3 regex (digest, profile, k1, rdp.depth, membership, phase09 qualification) | 16 of 16 pass (count derived with `ctest -N` = 16) |
| `check-rsp-no-hle.py --build build/gate` | exit 0, no re-pin |
| digest sweep, Wave 2 base `6dd785986f6aa78c6da5573a558b15b386a4edee` (40-hex checked, `git cat-file -e`), `--head WORKTREE`, four `--head-extra-id` | 1599 shared lines, 0 differences; 64 head-only digest lines per si-v2 id (W0-02) |
| `p10-mutants.py --run k1-predicate-widened --no-controls` (on the committed tree) | KILLED by `test_k1_refusal:apply-refusal` |
| `sh scripts/preflight-phase10.sh` (clean committed tree) | PASS 67 s: 29 regex rows, 16 phase10 labels, 223 units compiled with the act image cc 13.3.0, 11 post-Gate steps replayed, PII clean |
| `ctest --preset gate` | 240 of 240 pass in 78 s; `check-test-inventory.sh --preset gate` PASS count 240 |
| ASan (`ctest --preset asan` on the six new and K1 names) | pass (completeness 3.6 s, apply-refusal 6.5 s) |
| new names, native seconds from `CTestCostData.txt` | 0.0052 + 0.0017 + 0.246 + 0.013 = 0.27 s |
| acceptance counts | `unit.core.profile` 8, `unit.core.digest` 6, `grep -c t64_has_si_profile core/t64_isviewer.c` 2, `grep -c si-v2 core/t64_machine.c` 12, `t64.state.si.26` present |

## Task Commits

1. **Task 1: register the descriptor 13 family, depth inactive** - `286ccbb` (feat)
2. **Task 2: RED v26 completeness and strip differential** - `42f64db` (test)
3. **Task 3: activate depth and stream it under t64.state.si.26** - `831c947` (feat)
4. **Layout pin and throwaway mutants** - `f9d8f1f` (test)
5. **PIN-IMPACT.md section 2.1** - `2649a71` (docs)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The k1-predicate-widened anchor follows the new predicate text**
- **Found during:** Task 3
- **Issue:** writing the v26 term outside the descriptor 10 conjunction needs parentheses around the descriptor 10 conjunction (gcc and clang warn on `&&` inside `||`), so the anchored first lines of `t64_has_rdp_depth_profile` changed.
- **Fix:** the anchor and replacement of the row in `scripts/p10-mutants.py` now read `return (machine != NULL && ... == 10u &&`; `--check-anchors` and `repo.p10-mutant-anchors` pass and the mutant is KILLED. The dispatch allowed this ("keep matching or be updated in the same commit"); it is the 12th file.
- **Commit:** `831c947`

**2. [Rule 3 - Premise] The k1 mutant needs a committed tree**
- **Found during:** Task 3
- **Issue:** the plan lists the mutant in the Task 3 verify block, but the driver refuses a worktree with tracked changes outside `.planning/`.
- **Fix:** it ran on `831c947` after the commit (KILLED); the sweep, which supports `--head WORKTREE`, ran before it. No evidence was lost; a SURVIVED result would have led to a fix commit.

**3. [Rule 2 - Missing critical] Exporter uses the exact predicate**
- **Found during:** Task 1
- **Issue:** the plan's action names `state_version_registered` only, but `t64_machine_state_export` called `t64_has_si_profile` and would have exported a state 26 machine into the container version 1 / state version 26 header the importer refuses.
- **Fix:** it uses `t64_has_si_v25_profile`; state 26 export stays refused (INVALID_ARGUMENT) until plan 10-11, as the plan's truths state ("... used for the digest domain, the state version registry and the exporter").
- **Commit:** `286ccbb`

**4. [Rule 2 - Missing critical] Full identity mutation sweep for the new descriptor**
- **Found during:** Task 1 (AUTONOMY section 1, "New-descriptor identity check")
- **Issue:** the plan's v26-identity case names the tuple, id lengths, hash literal and the unregistered-id refusal only.
- **Fix:** the case also bit-flips every text and boot field, makes every text field empty and unterminated, mutates the three version words and the numeric fields (including the si-v1 body under the si-v2 id and the reverse), and compares the serialization with the si-v1 sibling byte by byte (exactly four differing bytes). A descriptor falling through a branch that stops at 12 would fail it.
- **Commit:** `286ccbb`

**5. [Rule 2 - Missing critical] Direct layout test, wrong-value control and hidden-table guard**
- **Found during:** Tasks 2 and 3
- **Fix:** `check_v26_depth_layout` and `check_differential_detects_body_change` in `test_digest_v26.c` (`f9d8f1f`, `42f64db`); the v26 walker refuses BAD_STORAGE when `rdp_depth_hidden` is NULL, as it already does for the coverage table (`831c947`).

**6. [Rule 1 - Premise] Names and shape of the walker predicates**
- **Found during:** Task 1
- **Issue:** the plan says "change has_v25_state to the exact v25 predicate"; every framing site (section markers, no packet identity strings, the new-span tail, section 10 and 11) must still apply to v26.
- **Fix:** `has_v25_state` and `has_v26_state` are exact and select the domain; `has_v25_framing` (their union) drives the framing sites; `has_v21_body` (depth profile and not v26) keeps the v21 body blocks (memory coverage, combiner, depth) off v26 so they are not hashed twice.

**7. [Rule 2 - Added record] PIN-IMPACT.md section 2.1**
- The plan text asks for the hash pin to be recorded in PIN-IMPACT.md section 2; that file is not in the plan's file list, so the record is its own docs commit (`2649a71`).

**Total deviations:** 7 (2 Rule 3, 4 Rule 2, 1 Rule 1). **Impact:** none on scope; two files beyond the planned 11.

## Review findings and observations (information only)

- **Info (tooling coverage):** `scripts/list-membership-sites.py` has no tokens for the new exact predicates, so after the regeneration `t64_machine_state_export` and the `t64_has_si_profile` detail of `t64_digest_stream` no longer appear in `docs/spec/profile-membership-sites.txt` (the diff drops those two details and adds the two predicate bodies, `is_si_v2_profile` and the isviewer detail). `repo.si-membership-sites` still passes because it only compares the regenerated list. Adding the two tokens would restore the sites to the list; the script is a Phase 9 bound source path of `scripts/qualify-phase09.py`, so the edit was not made here. A later plan can add `t64_has_si_v25_profile` and `t64_has_si_v26_profile` to `MEMBERSHIP_TOKENS`.
- **Info (mid-render state):** the mid-render state of the strip differential is a real active fill render (`render_active`, not a pending command) built by seeding the fill state and dispatching two RDP events; the mid-scan-out state is the v25 VI frame builder.
- The `boot_status` of si-v1 still names the ai-v1 family; it is not changed (a changed text would move a pinned F2 hash).

## Issues Encountered

- First run of `v26-strip-differential` failed in the new SI poll builder (a 40,000 tick advance exceeds the 4096-event advance budget with the spin loop running); it was a setup defect, fixed by stepping 2,000 ticks at a time; the RED evidence above was recorded after that fix, so it names depth members only.
- macOS `sed -i` needs an empty suffix argument; one edit was redone with python.

## Authentication Gates

None.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-18 mitigated (counting-allocator refusal before allocation, five unregistered names and both cross-bodies), T-10-19 mitigated (exact predicates, sweep 0 differences, K1 and F2 green, mutant killed), T-10-20 mitigated for this plan (RED then GREEN completeness; the two omission mutants belong to plan 10-10 Task 2), T-10-51 mitigated (vi-filter-v9 refuses the ISViewer window with BUS_DEVICE_REFUSED).

## Next Phase Readiness

- Plan 10-10 (depth rendering on the new ids, the two v26 digest omission mutants in `scripts/p10-mutants.py`): the family is registered with depth active; `rdp_depth_hidden` and the scalars are in the stream.
- Plans 10-11 and 10-12: `t64_machine_state_export` refuses a state 26 machine (`t64_has_si_v25_profile`) and `state_version_registered` knows 25 only; the container version word and `t64.state-container.v2` are still to be added; the importer must read the five scalars inside the RDP block before the render proof and the hidden table in the tail, as `check_v26_depth_layout` fixes.
- Plan 10-13: the head-only digest lines of the four si-v2 ids are in the sweep report (`build/gate/10-sweep/6dd785986f6a-worktree.txt`, local); every later sweep passes `--head-extra-id` for the four ids.
- Orchestrator items: (1) all commits of this plan are local and unpushed (plan 10-13 pushes; the gate is green at the head, the RED window of `42f64db` is closed by `831c947`); (2) `requirements.ready-ids` decides VID-07 and CORE-06 (both are also named by later plans); (3) local scratch under `build/gate/10-09-task1`, `10-09-task2`, `10-09-task3` and `build/gate/10-sweep` may be pruned; (4) the Gate-growth ledger row for Wave 2 is plan 10-13's, with this plan's four names costing 0.27 s native.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-24*

## Self-Check: PASSED

The two new test files, the five edited core files, the regenerated membership list and this file exist; the five plan commits (`286ccbb`, `42f64db`, `831c947`, `f9d8f1f`, `2649a71`) are ancestors of HEAD; every acceptance criterion of the three tasks was re-run (profile count 8, digest count 6, `t64_has_si_profile` in `core/t64_isviewer.c`, si-v2 id text in `core/t64_machine.c`, `t64.state.si.26` in `core/t64_digest.c`, 16 of 16 on the Task 3 regex, sealed-body guard, sweep, mutant, preflight).
