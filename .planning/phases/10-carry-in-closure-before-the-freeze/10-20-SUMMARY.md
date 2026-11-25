---
phase: 10-carry-in-closure-before-the-freeze
plan: 20
subsystem: digest-completeness
tags: [gap-closure, vi-completeness, w2-03, core-06, mutant-red-green, flip-every-member, digest-v26]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: the v26 digest walker and the RDP completeness case (10-09), the out-of-tree mutant driver (10-01), the independent Wave 2 review with the VI probe and finding W2-03 (10-13), the qualifier and its passing record (10-17, 10-19), the verification report with the SC2 VI gap
provides:
  - committed flip-every-member rows for t64_vi_state (120 rows, 1,208 bytes, 791 member / 400 excluded / 17 padding) inside the existing case unit.core.digest.v26-completeness, with static asserts on the two struct sizes
  - mutant row v26-digest-omit-vi-phase-cycles (SURVIVED before the rows, KILLED after them) and a driver that lists 26 mutants and 2 controls
  - qualifier derivation text for the mutants, gate and asan stages refreshed (limits unchanged)
  - W2-03 recorded CLOSED in 10-REVIEW.md and PIN-IMPACT.md section 16
affects: [10-21, phase-10-verification, phase-11]

plan_head_before: cfc5ee0a30ea56ae26808ff78dd4c3937f434ca1
plan_head_after: c16c90b2439a88216c56c2a6378a5feb55659bd4

actuals:
  # chars/4 over the added lines of the realized diff at SUMMARY write time (this summary and the STATE edit are not in it)
  tokens: 8900
  tasks: 4
  commits: 6

tech-stack:
  added: []
  patterns:
    - "a RED taken by behavior: the committed mutant row is classified SURVIVED by the committed case before the new rows exist, and the SURVIVED is made attributable by a scratch stream-equality probe (flip equal on the mutated build, different on the unmutated one)"
    - "completeness by byte class: rows claim bytes (a row that overruns or double-claims fails), unclaimed bytes are padding, and the observed inert count must equal named exclusions plus padding exactly"
    - "a _Static_assert on the struct size sends a new member to the completeness rows before it can ship unseen"

key-files:
  created:
    - .planning/phases/10-carry-in-closure-before-the-freeze/10-20-SUMMARY.md
  modified:
    - core/tests/test_digest_v26.c
    - scripts/p10-mutants.py
    - scripts/qualify-phase10.py
    - .planning/phases/10-carry-in-closure-before-the-freeze/10-REVIEW.md
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md

key-decisions:
  - "The VI rows live in the existing case v26-completeness (no new CTest name, no new case): 254 gate names, 30 phase10 names and two cases of test_digest_v26 are unchanged, so every count row and the inventory stay as qualified."
  - "The two named exclusion reasons were re-derived from core/t64_internal.h and core/t64_digest.c, not copied from the review: host observer cursors (packet_head, packet_count) and the per-slot identity strings (hashed only under !has_v25_framing)."
  - "Every byte of the VI state is flipped on the descriptor 13 stem only (exactly 417 inert of 1,208); the three save ids and the state 25 stem get the representative sweep, matching the RDP precedent and the 10-13 probe ids."
  - "The qualifier limits stay 600, 600 and 120/600/600; only derivation text and measured seconds of the mutants, gate and asan stages moved, and the asan serial-sum factor is stated as 1.97 (not rounded up to 2)."
  - "W2-03 was not carried to Phase 11: it is closed here, so the G3 count stays at five."

patterns-established:
  - "Record the observed stream-equality lines beside a SURVIVED classification so the RED names the omitted member"

requirements-completed: [CORE-06]

coverage:
  - id: D1
    description: "unit.core.digest.v26-completeness flips every member of t64_vi_state (both t64_video_packet_info slots included) and requires each member flip to change the canonical stream; the named exclusions and padding must not change it; on the stem id every byte of the 1,208 is flipped and exactly 417 flips leave the stream unchanged"
    requirement: "CORE-06"
    verification:
      - kind: test
        ref: "ctest --preset gate -R '^unit\\.core\\.digest\\.v26-' (2 of 2 passed); build/gate/core/test_digest_v26 --case v26-completeness: checks=10132 failures=0 (1,556 before the VI rows, 4,124 after the stem rows alone)"
        status: pass
    human_judgment: false
  - id: D2
    description: "the mutant v26-digest-omit-vi-phase-cycles SURVIVED the committed case before the VI rows existed (behavioral RED) and is KILLED by the case after them (GREEN); the driver lists 26 mutants and 2 controls and the full run classifies all 28 rows as expected"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "RED be683bf: --run v26-digest-omit-vi-phase-cycles --no-controls exit 1, SURVIVED KILLED 3.2 s, probe lines existing-case-under-mutant exit 0, stream-equal-under-mutant yes, stream-equal-unmutated no; GREEN 0ed0d75: exit 0, KILLED by test_digest_v26:v26-completeness 5.9 s; full run at 49c935a: 28 rows, 26 KILLED, control-equivalent-depth-cap SURVIVED, control-no-compile INVALID, 147 s"
        status: pass
    human_judgment: false
  - id: D3
    description: "static asserts pin sizeof(t64_vi_state) 1208 and sizeof(t64_video_packet_info) 488; no production source, protected path or CTest name changed"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "git diff --name-only ad39b6be HEAD -- core runner tools lists core/tests/test_digest_v26.c only; protected paths git diff --quiet exit 0; ctest -N counts 254 and 30; test_digest_v26 --list-cases 2 lines; digest sweep 1599 shared lines, 0 differences"
        status: pass
    human_judgment: false
  - id: D4
    description: "W2-03 is recorded CLOSED in 10-REVIEW.md (closing note, corrected SC2 row, Gap closure section) and PIN-IMPACT.md (section 16 and the 13.1 note); the qualifier text names 26 mutants and 28 rows with limits unchanged"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "Task 3 verify greps (V1 to V3 and the records), ctest -R '^(harness\\.phase10-qualification|repo\\.p10-mutant-anchors)$' 2 of 2 passed, sh scripts/pii-scan.sh --tree findings=0"
        status: pass
    human_judgment: false
  - id: D5
    description: "local gate, K1, sweep and the full preflight pass on the clean committed tree"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "sh scripts/gate.sh [gate] PASS 136 s (254 of 254); ctest -R '^unit\\.core\\.k1\\.' 2 of 2; sh scripts/preflight-phase10.sh [preflight] PASS 73 s (linux-compile 228 units, post-gate-replay 11 steps)"
        status: pass
    human_judgment: false

duration: 17min
completed: 2026-11-25
status: complete
---

# Phase 10 Plan 20: VI completeness rows (gap closure of W2-03) Summary

**The committed flip-every-member case now covers every member of `t64_vi_state`: 120 rows over 1,208 bytes (791 member, 400 named-exclusion and 17 padding bytes), every byte flipped on the stem id with exactly 417 inert, a representative sweep on four more ids, and a mutant that drops `phase_cycles` from the walker SURVIVED the case before the rows and is KILLED after them.**

## Performance

- **Duration:** about 17 min of execution on a host at 1-minute load 6 to 16 (the plan estimated about 45 min and 75,000 tokens; the realized diff is about 8,900 tokens on the same chars/4 scale, 434 lines added)
- **Tasks:** 4 of 4 (Task 1 tracer: RED then GREEN, passed end to end; Tasks 2 to 4 auto)
- **Files:** 5 modified plus this summary; no production source, CMake file, inventory file or pin changed
- **New CTest names:** none (254 gate names, 30 `phase10`, two cases of `test_digest_v26`)

## Accomplishments

- **Task 1 (tracer), RED then GREEN.** RED commit `be683bf` adds only the driver row `v26-digest-omit-vi-phase-cycles` (the `hash_u64(sink, vi->phase_cycles)` call of the VI section replaced by a cast-to-void read). On the clean tree `--run v26-digest-omit-vi-phase-cycles --no-controls` exited 1 and the row was **SURVIVED** against the expected KILLED (3.2 s). A scratch probe (not committed) made it attributable: on the mutated build the canonical stream (10,545,043 bytes) is unchanged by flipping bit 0 of `vi.phase_cycles` and the committed case passes there (exit 0, 1,556 checks); on the unmutated tree (10,545,051 bytes) the same flip changes it. GREEN commit `0ed0d75` adds `vi_members` (120 rows), the two exclusion reasons, `VI_STATE_SIZE 1208u` and `VIDEO_PACKET_INFO_SIZE 488u` static asserts, `check_byte_flip` returning the changed flag and `check_vi_completeness`, called on the stem id with every byte flipped; the same mutant run then exits 0 and the row is **KILLED** by `test_digest_v26:v26-completeness` (5.9 s). Under the mutant the case reports all eight bytes of `phase_cycles`, an inert count of 425 against 417 and a failed digest tie.
- **Task 2.** Commit `49c935a` adds the representative mode (first byte of every row, last byte of every excluded row, every padding byte) and calls it for `-e4k`, `-e16k`, `-s32k` and the state 25 stem `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1`; header and case comments describe the rows. Native 1.08 to 1.10 s (Wave 2 row 0.40 s) at load about 15, ASan 7.92 s with no sanitizer report; a scratch control with the mutated digest object fails on all five ids (12 member-byte failures, 5 digest-tie failures).
- **Task 3.** Full driver run on the clean tree: 28 rows, 26 KILLED, the equivalent control SURVIVED, the non-compiling control INVALID, each as expected, 147 s at load 10.7 to 13.5. Commit `7e9f3b8`: the mutants stage derivation says "26 mutants and 2 controls, 28 rows" with 147 s, and the gate (129.41 s) and asan (0.19, 7.71, 129.05 s) derivations carry the figures of the passing plan 10-19 record; every limit literal is unchanged. Commit `e705ce1`: 10-REVIEW.md (closing note, corrected SC2 row, "Gap closure" section, "W2-03 is CLOSED") and PIN-IMPACT.md (13.1 note, section 16).
- **Task 4.** Plain `sh scripts/gate.sh` PASS in 136 s (254 of 254), K1 2 of 2, digest sweep `1599 shared lines, 0 differences, 0 head-only lines`, production diff since the Qualification base is `core/tests/test_digest_v26.c` only, protected paths unchanged, oracle request blocked, full preflight PASS in 73 s. Recorded in PIN-IMPACT.md section 16.3 (commit `c16c90b`).

## Task Commits

1. **Task 1 RED: mutant row** - `be683bf` (test, `scripts/p10-mutants.py`)
2. **Task 1 GREEN: VI rows on the stem id** - `0ed0d75` (test, `core/tests/test_digest_v26.c`)
3. **Task 2: VI rows on the save ids and the state 25 stem** - `49c935a` (test, `core/tests/test_digest_v26.c`)
4. **Task 3: qualifier derivation counts** - `7e9f3b8` (chore, `scripts/qualify-phase10.py`)
5. **Task 3: W2-03 records** - `e705ce1` (docs, `10-REVIEW.md`, `PIN-IMPACT.md`)
6. **Task 4: gate, sweep and preflight results** - `c16c90b` (docs, `PIN-IMPACT.md` section 16.3)

**Plan metadata:** this summary with the STATE, ROADMAP and REQUIREMENTS updates follows in one docs(10-20) commit.

## Deviations from Plan

None of substance; the plan executed as written. Four small implementation notes, none changing a result or a limit:

1. **Fifth macro (Rule 3, trivial).** The plan names VM, VMX, VMI and VMT; the per-slot excluded strings (`profile_id`, `build_id`) need a nested-offset excluded row, so a fifth macro `VMTX` and a slot macro `VMSLOT` were added. `M`, `MX`, `MI` and `MT` are untouched (one definition each).
2. **Probe method.** The plan says the probe is built "against a scratch copy of the tree" with the one-line edit. The probe was built from the committed test helpers (`#include` of the test source with `main` renamed) and linked once with a scratch copy of `core/t64_digest.c` carrying the row's one-line edit (compiled with the production flags, linked ahead of the main tree's `libt64_core.a`) and once against the main tree. The edited unit is the only difference; the result is the same three lines.
3. **Docstring sentence.** One sentence was added to the `_stage_table` docstring of `scripts/qualify-phase10.py` saying that plan 10-20 refreshed the gate, asan and mutants figures (the docstring still lists the plan 10-17 figures and would otherwise read as stale). No code, limit, regex, marker or check moved.
4. **Task 2 carries no `tdd="true"` marker**, as the plan states; the RED of the feature is Task 1's mutant classification.

### Auto-fixed Issues

None. No auto-fix attempt was used.

## Authentication Gates

None.

## Known Stubs

None. The probe programs live under `build/10-20-probe` (ignored, not committed).

## Threat Flags

None. The plan adds test source, a driver row and text only; no endpoint, auth path, file access pattern or schema.

Threat register: T-10-53 (named exclusions with code-derived reasons, exact 417 count and 791/400/17 tallies), T-10-54 (the RED is the driver's SURVIVED classification on a clean tree with the results file kept, a KILLED RED would have stopped the plan), T-10-55 (the two `_Static_assert` size pins) and T-10-56 (every-byte sweep on one id, native 1.1 s against the 5 s budget) are mitigated as planned; T-10-SC accepted (no dependency added).

## Items for the orchestrator and plan 10-21

- **The plan 10-19 qualification record is superseded by design.** `scripts/qualify-phase10.py` and `core/tests/test_digest_v26.c` are bound inputs; the record binds the stage table text, so it no longer verifies. Plan 10-21 re-qualifies.
- **`docs/evidence/phase10-qualification.md` still says "27 rows ... 25 KILLED"** and `docs/claims/phase10.md` binds the CORE-06 names; both are plan 10-19 outputs outside this plan's file list and were not edited. Plan 10-21 must regenerate or amend them for 26 mutants and 28 rows (its own verify already greps for `v26-digest-omit-vi-phase-cycles` in the evidence document).
- **ASan stage margin.** With the figures of the passing 10-19 record the 600 s CTest limit is 4.6 times the wall (129 s, the rule the hermetic test enforces) but 1.97 times the serial sum (305.08 s). The text says so; no limit was widened. Watch it in the 10-21 qualification.
- **Host load** was 6 to 16 throughout (above the quiet-host threshold of 8 for most runs), so every measured second is an upper bound; no wall-backstop failure occurred in the gate or the preflight.
- **Nothing was pushed and no tag was created** (plan 10-21 pushes). `origin/main` is still `f081d39`.

## Self-Check: PASSED

Files found: `core/tests/test_digest_v26.c`, `scripts/p10-mutants.py`, `scripts/qualify-phase10.py`, `10-REVIEW.md`, `PIN-IMPACT.md`. Commits `be683bf`, `0ed0d75`, `49c935a`, `7e9f3b8`, `e705ce1` and `c16c90b` are ancestors of HEAD.
