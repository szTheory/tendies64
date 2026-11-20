---
phase: 09-controller-save-media-and-replay
plan: "09"
subsystem: state-import
tags: [core-04, validate-then-apply, closed-handler-table, machine-owned-scratch, same-build-ladder, write-generation, runner-state-import, verified-input, option-pair-matrix]

requires:
  - phase: 09-controller-save-media-and-replay
    provides: "09-01 the v25 walker, section ids and the closed event ledger; 09-03 and 09-04 the SI and save sections; 09-06 the F1 state content; 09-08 the container export, its layout constants and the reference decoder offsets"
provides:
  - "t64_machine_state_import with a closed (status, reason, section, rule, position) report: arguments, header, trailer, identity binding, section table, per-field rules, queue and per-device invariants, declared digest, one infallible commit"
  - "one reader function per canonical section used in validate and apply mode; a 600-byte machine-owned scratch instead of a stack array; the closed (device, kind) handler table equal to the ten ledger pairs"
  - "the same-build ladder (T1 round trip, T2 byte identity, T3 partitions, dirty peer, rewind, observer flag, write_generation rule, EEPROM deadline bound)"
  - "runner --state-import triple through the verified-input path, --state-import-fd through the checked argv helper, five runner cases and the option-pair matrix"
  - "scripts/check-state-field-use.py --list (D-45 extractor)"
affects: [09-09.1, 09-10, 09-11]

actuals:
  tokens: 73061
  tasks: 5
  commits: 9
plan_head_before: a4a30375e49a8b2fd734379f9d14528e8062b1a6
plan_head_after: 1b9d6d34dc59f3d9527f22a29f09337ba9332e47

tech-stack:
  added: []
  patterns:
    - "validate then commit with the same reader function in both modes: checks evaluate only in validate mode, apply stores only, apply cannot fail"
    - "scratch in the machine (600 bytes) instead of a 256-slot stack array; per-(device, kind) aggregates (count, first tick, token and value, largest token and its count) are enough for every device rule"
    - "commit zeroes every state structure the container describes and then overlays the blob, so non-canonical members and a dirty peer cannot survive"
    - "independent re-signing helper and structural encoder written from the container definition; every row of a refusal table reaches its own (status, reason, section, rule)"

key-files:
  created:
    - core/tests/test_state_import.c
    - scripts/check-state-field-use.py
  modified:
    - core/include/tendies64/t64_machine.h
    - core/t64_internal.h
    - core/t64_state.c
    - core/t64_rsp.c
    - core/CMakeLists.txt
    - runner/t64_supervisor.c
    - runner/t64_real.c
    - runner/tests/test_state_runner.c
    - scripts/check-test-inventory.sh
    - docs/spec/state-container.md
    - docs/spec/machine-contract.md
    - docs/spec/phase09-cases.json
    - docs/spec/profile-membership-sites.txt

key-decisions:
  - "write_generation: one successful state import into a profile with a save device adds exactly 1 (saturating) as the last write of the commit, a refused import never touches it, replacement resets it, it never travels in the container (test: save-media-roundtrip)"
  - "busy_until: an imported EEPROM deadline is at most the blob tick plus 375,000 (SRAM: the blob tick), stale values at or below the tick are accepted and kept; rule TICK_LE (tests: field-range-refusals rows, save-media-roundtrip deadline window)"
  - "the commit zeroes the structures the v25 state does not fully carry (RDP combiner, depth and blender registers, hidden coverage tables, the RSP observation record) and regenerates the retained packet identity strings from the live machine"
  - "a resumed runner run never reports pass: the verdict comes from milestones and a retirement count observed through the run's own trace; its guest tick, final state digest and log bytes still equal the uninterrupted run"

requirements-completed: []

duration: 65min
completed: 2026-11-20
status: complete
---

# Phase 9 Plan 09: State Import Summary

**A validate-then-apply state importer (closed handler table, 600-byte machine-owned scratch, one reader per section in two modes) round-trips an SI-read-in-flight state exactly, refuses 79 crafted hash-valid blobs each with its own closed reason while leaving the machine bit-identical, and reaches the runner through the verified-input path with the one-producer option matrix.**

## Performance

- **Commits:** 9 measured from the ledger (`a4a3037..HEAD` before this summary); the plan-base ledger is `gsd-plan-head-before-09-09` in the git directory.
- **Estimate:** 300,000 tokens (low confidence); actual 73,061 (chars/4 over the added lines of the diff `a4a3037..HEAD`, 6,941 added lines, tests about half).

## OPEN ISSUES FOR THE ORCHESTRATOR (read first)

1. **CORE-04 is not marked complete.** Import is proven on the same build; the boundary matrix, the structure-aware mutants, the fuzz campaign and the mapped field audit are 09-09.1 (declared split). `requirements-completed` is empty.
2. **The plan's runner premise does not hold: a resumed run cannot report `pass`.** The runner derives its verdict from milestones (IPL3 entry, loader DMA, hello entry) and a retirement count that it observes through its own trace, which a resumed run does not have. The accepted-continues control therefore asserts what is true: the resumed run reaches the same completion tick (838,791), the same final state digest and the same log bytes as the uninterrupted run, and ends in the closed `harness-error` / `host-error` record. A state published by the runner itself (observer cursor at the log length) resumes into the closed `report-error` / `report-invalid` record (record total 20 against 0 captured bytes). Whether a resumed run should carry its milestones is a design question for 09-10 or Phase 10; nothing here weakens the pass rule.
3. **The runner exports only at authored completion** (09-08), so the plan's "run B with a bound inside the spin publishes a blob" cannot happen; the mid-run state of the accepted control is exported in process from the same core library (the runner test binary links it).
4. **`--input` is mandatory in real mode**, so "STATE-IN refused with INPUT" is implemented for the custom schedule options `--input-size` and `--input-sha256` (the path itself keeps its default).
5. **Not pushed.** No hosted run exists for this plan. `harness.rdp-oracle-authority` passes (no manifest row changed).
6. K13: cumulative Phase 9 gate names are 44 of about 45 (43 through 09-08 plus `unit.core.state.import`); the 09-09.1 name `harness.state-field-use-audit` is the 45th.

## Commits

| Hash | Message |
| --- | --- |
| `6ed42db` | D-45 field-use extractor in `--list` mode |
| `3797ce4` | RED: import surface, stub importer, S1 round-trip case, registration |
| `49a904a` | validate-then-apply importer, closed handler table, tracer GREEN |
| `0a433a6` | refusal tables: binding, container, field-range, queue, per-device |
| `f63a9cf` | rows for the stale declared digest, the shared bool reader and the observer ranges (found by surviving mutants) |
| `1f2e892` | ladder, write_generation, observer flag, identity-string regeneration |
| `324e45c` | runner state import through the verified-input path |
| `00b6660` | Linux-only format-truncation fix found by the Ubuntu pre-flight |
| `1b9d6d3` | two membership sites in the generated list |
| (this summary) | docs: complete the plan |

## Accomplishments

- **Importer.** `core/t64_state.c` gained about 2,150 lines: checked arithmetic helpers, `state_check_arguments`, `state_parse_header`, `state_verify_trailer`, `state_check_binding`, `state_parse_table`, one `state_section_*` reader per id (1 to 11 and 16), `state_check_devices`, `state_recompute_digest`, `state_commit`. No function allocates, calls the host or holds a frame above 2,048 bytes (gate build under `-Werror -Wframe-larger-than=2048`, the gcc:13 closure and the Ubuntu cc pre-flight are clean).
- **Evidence.** 77 table rows over five families, a whole-allocation snapshot after every refusal, eight out-of-source mutants of the importer each failing its named case, and a mutant-found hole closed with three rows.
- **Runner.** `--state-import`, `--state-import-size`, `--state-import-sha256` through `t64_verified_input_prepare`; the child reads the exact size from the inherited descriptor and imports after admission and before the first advance.

## RED records

| Step | Observation |
| --- | --- |
| Task 1 step 3 (`3797ce4`) | import returned status 7 (NOT_READY) where 0 is required, report fields 0, source digest `e98fb733...c026a`, peer digest `5ed31137...d9b0` (the peer was blank); `checks=119 failures=5` (`build/gate/09-09-t1-red.LastTest.log`) |
| Task 2 | the importer of Task 1 already implemented every rule, so the table started green by design (the only two first failures were test defects: a row that rewrote a live valid SP descriptor and an allocator-count comparison that included the test's own machine creation). The proof is the mutant record below, not a manufactured RED |
| Task 3 generation | `save_generation` was 0 after a successful import where exactly 1 is required on the `-s32k` peer (`CHECK_INT failed ... got 0 want 1`, then 2 and 3), before the single increment was added |
| Task 3 observer | `t64_video_info` of the default-imported peer differed from the source in the profile and build identity strings (the first retained packet carries sequence 0, so a sequence test missed it); fixed by regenerating the strings for every slot with a non-zero length |
| Task 4 | the supervisor refused `--state-import` as an unknown option (usage) and, once the child accepted it, the first advance ended in `advance-invalid` with guest tick 0 because the loop started at tick 0 while the imported machine sat at 700,000 (`stop.tick > target`); fixed by resuming from the header tick |

## Spelling map against 09-08

09-08 declared no import-side name, so every spelling of "Artifacts this phase produces" is as planned: `t64_machine_state_import(machine, bytes, length, flags, report_out)`, `T64_STATE_IMPORT_DROP_OBSERVERS`, `t64_state_import_report {status, reason, section_id, rule, position}`, statuses `T64_STATUS_STATE_FORMAT` (26) to `T64_STATUS_STATE_INVARIANT` (33), `T64_STATE_REASON_*` (0 to 40) and `T64_STATE_RULE_*` (0 to 16). 09-08 names used as they stand: `T64_STATE_HEADER_BYTES`, `T64_STATE_ENTRY_BYTES`, `T64_STATE_TRAILER_BYTES`, `T64_STATE_OBSERVER_BYTES`, `T64_STATE_MAX_SECTIONS`, `T64_STATE_MAX_BYTES`, `T64_STATE_BUILD_ID_BYTES`, `T64_STATE_SECTION_FLAG_OUTSIDE_DIGEST`, `T64_STATE_CONTAINER_MAGIC`, `T64_STATE_CONTAINER_VERSION`.

Differences from the plan text: (1) the scratch holds per-pair aggregates and scalar facts (600 bytes, `t64_state_summary` with `t64_state_pair pairs[10]` and `t64_state_device_facts`), not 256 event slots; (2) `t64_rsp_issue_event_handler` was added in the RED commit so the test links (added lines only, `7 0`, guard green); (3) the reader struct has `base, length, position, limit, mode, failed, drop_observers, section_id, machine, report`; (4) the queue reasons record a rule (`COUNT_CAP`, `SEQ_LT`, `TICK_GE`, `ENUM_CLOSED`), the plan only required rules for the field reasons; (5) the commit zeroes `rsp_observed_*` although replacement does not (the plan said "exactly as admission does").

## Extractor and the unbounded-member list

`python3 -B scripts/check-state-field-use.py --list` prints 72 member names of 470 reachable structure members (D-45 estimated about 500 used fields; the lexical scan sees index, length, shift and loop uses and one level of local-variable carry, and most state is consumed by arithmetic and comparison, so the usable list is 72 names). Listed members that no reader bounds, with the reason (the 09-09.1 audit maps each to a rule or a named consuming site): `vi.registers`, `vi.active_registers`, `vi.field`, `dpc_current`, `dpc_xbus`, `rdp.color_image_address`, `rdp.color_image_width`, `rdp.render_x_begin`, `rdp.render_x_end`, `rdp.render_y_*` (validated at use against the image and RDRAM before any write: `command_span_valid` and the row handler in `core/t64_rdp.c`; fuzz in 09-09.1 is the test), `sp.mem_address`, `sp.dram_address` and `rsp.*` register files (`MASKED_AT_USE`, sites in the spec), `cpu.*` registers and the TLB (`page_size_shift` is a switch). Everything else on the list is bounded: queue (`COUNT_CAP`, order, sequence), descriptors (`DERIVED_EQ` from the raw latches), joybus entries (counts, channel, offset, lengths), audio descriptors and slots (`INDEX_LT`, `LEN_LE`, `ADDR_RANGE`), RDP tile fields and the texture tile (widths), packet lengths (`LEN_LE`), observer head, count and cursor.

## Reader to walker line map

| Section | `core/t64_digest.c` lines | Reader |
| --- | --- | --- |
| 1 identity | 387-403 | `state_section_identity` |
| 2 machine core | 404-409 | `state_section_machine_core` |
| 3 RDRAM | 410-412 | `state_section_rdram` |
| 4 SP memory | 413-417 | `state_section_sp_memory` |
| 5 interrupt, PI, SP | 418-453 (and 118-135 descriptor) | `state_section_interrupt_pi_sp`, `state_read_sp_descriptor` |
| 6 scheduler | 454-463 (and 109-116 event) | `state_section_scheduler` |
| 7 warm devices | 464-602 (and 137-248) | `state_section_warm_devices`, `state_read_cpu`, `state_read_cpu_extended`, `state_read_rsp`, `state_read_rdram_protocol` |
| 8 RDP and VI | 603-857 | `state_section_rdp_vi` (`state_read_rdp`, `state_read_rdp_tiles`, `state_read_vi`, `state_read_video_slot`) |
| 9 audio | 859-934 | `state_section_audio`, `state_read_audio_slot` |
| 10 SI and PIF | 935-977 | `state_section_si_pif` |
| 11 save media | 978-996 | `state_section_save_media` |
| 16 observer | (not in the walker) | `state_section_observer` |

The v25 branch flags that apply to this family (confirmed against the predicates): the VR4300 block, ISViewer totals, RSP, RDRAM protocol, the triangle block with its 2 MiB coverage array and the VI active registers; no combiner, depth or memory-coverage block (the depth predicate is guarded by descriptor version 10). The idle section 8 length of the reader layout equals 2,103,016 (asserted by the test from the exported blob).

## Reasons, rules and rows

Closed reason list `T64_STATE_REASON_*` (41 names) and rule list `T64_STATE_RULE_*` (16 names) are in `core/include/tendies64/t64_machine.h` and `docs/spec/state-container.md`. Rows (tables in `core/tests/test_state_import.c`): binding 5, container 20, field 30 (plus 4 accepted deadline neighbours), invariant 25.

| Rule | Row names (field-range-refusals unless noted) |
| --- | --- |
| `COUNT_CAP` | audio descriptors, triangle words, joybus entries, observer video and audio counts; queue count (invariant row) |
| `INDEX_LT` | audio reservation slot, texture tile, joybus channel, observer heads |
| `ENUM_CLOSED` | SI direction; closed (device, kind) pairs (invariant rows) |
| `BOOL_01` | PI error, and one through the shared reader (PI active_valid) |
| `ALIGN` | SI active address, AI staged address |
| `ADDR_RANGE` | SI active address beyond RDRAM |
| `TICK_GE` | SP descriptor end tick; queue tick (invariant row) |
| `TICK_LE` | EEPROM deadline above the window, maximum, SRAM deadline above the tick |
| `SEQ_LT` | queue sequence rows (invariant) |
| `SHIFT_LT` | tile shift |
| `LEN_LE` | video packet length, audio packet length, observer drain cursor |
| `PROFILE_DERIVED` | RDRAM size, save kind |
| `DERIVED_EQ` | interrupt line, header tick |
| `PAD_ZERO` | text after the terminator |
| `RESERVED_ZERO` | interrupt pending bits |
| `MASKED_AT_USE` | no row: documentation only, sites listed in the spec |

Binding rows: state version 24 (`STATE_VERSION`), profile digest (`STATE_PROFILE`), content digest and length (`STATE_CONTENT`), build id (`STATE_BUILD`): five distinct reasons, each after an accepted baseline in the same setup.

## Per-device derivation anchors

CPU: `t64_cpu_step`, `cpu_commit_preflighted_successor` (`core/t64_cpu.c`), exception re-reserve (`core/t64_exception.c` 91); a refused re-reserve leaves the host-error outcome and no event, so "exactly one" holds only while the CPU is running. PI: `start_dma` and `t64_pi_complete` (the idle PI owns none, the plan's "stale token" case cannot arise because the active flag clears only at the event). SP: `start_rsp_dma` (no scheduled completion on this profile), `t64_rsp_schedule_next` (token is the issue sequence, tick is the next issue tick, a halted RSP can still hold the pending event until its tick). RDP: `schedule_next_committed`. VI: `t64_vi_write_register` (older generations stay queued and harmless). AI: `t64_ai_write_register` (pause cancels the event), `t64_ai_event`. SI: `start_dma`, `t64_si_execute_event`, `t64_si_complete_event`. The spec table lists each rule.

## Out-of-source mutant results (the working tree restored after each)

Each mutant edited one anchored text of `core/t64_state.c`, was built under the gate preset (`-Werror`) and ran the named case.

| Mutant | Case | Observed |
| --- | --- | --- |
| m1 trailer check skipped | `container-refusals` | failures 5 |
| m2 build id not compared | `binding-refusals` | failures 3 |
| m3 boolean rule dropped from the shared reader | `field-range-refusals` | survived (no row used the shared reader); row added; then failures 1 |
| m4 sequence compare relaxed | `invariant-refusals` | failures 5 |
| m5 SI pass skipped | `invariant-refusals` | failures 5 |
| m6 tick written in validate mode | `refusal-untouched` | failures 5 |
| m7 scratch left dirty on exit | `scratch-storage` | failures 4 |
| m8 declared digest never compared | `roundtrip` survived (accepted blobs only); a stale-declared-digest row added; then `container-refusals` failures 4 |
| m9 generation never incremented | `save-media-roundtrip` | failures 8 |
| m10 generation incremented before validation | `save-media-roundtrip` | failures 9 |
| m11 drop flag ignored | `observer-flag` | failures 4 |
| m14 trace not cleared | `roundtrip` survived (the peer had no records); a peer with its own records added; then failures 1 |
| m15 identity strings not regenerated | `observer-flag` | failures 1 |

## Runner record

- Long authored ROM (`phase09-admit-long-si-v1`), bound 1,200,000. Run A (uninterrupted) passes, completion tick **838,791**, log 20 bytes. Run C imports an in-process state exported at tick **700,000** (a stem-state container of about 6.35 MB): resumed record `harness-error / host-error / host-error`, guest tick 838,791, final state digest and log bytes equal to run A, result checker accepts the record. Run B (runner export at authored completion of the long ROM) publishes a stem-state container of about 6.35 MB; its import is the `report-invalid` record described above.
- Times (native, gate preset): `runner.state-export` with the five new cases 8.2 s (the five cases together about 2 s); each import case 0.2 to 0.7 s. The result record carries the final canonical digest key (`state_digest`), which the accepted control compares.

## Option-pair matrix as implemented

STATE-IN is refused (usage, guest tick 0, decided in the supervisor before the snapshot) with: the custom schedule options, raw capture, synthetic mode, every save flag, every replay flag and both state export flags; an incomplete triple, size 0 or above 64 MiB and a hash that is not 64 lowercase hex digits are refused the same way. The nine one-producer rows pass a nonexistent import path; the control without a conflicting flag reports the snapshot refusal (`refusal`), which proves the order. The child repeats the rule for a directly started runner (usage, no reason line on the diagnostic stream, so no import call) and refuses a profile outside the composite family (`refusal`). The per-option slot sum is 45 of 64 (46 with the terminating NULL); the `_Static_assert` counts every family as if all co-occurred. Full table in `docs/spec/state-container.md` ("Option-pair matrix").

## Checks and measured seconds

| Block | Result | Seconds |
| --- | --- | --- |
| Task 1 verify (configure, build, 12 names, lint, inventory, guard, ast, extractor) | 12 of 12 | about 10 |
| `unit.core.state.import` native (all 14 cases) | pass | 16.3 to 17 |
| per case native | roundtrip 1.0, binding 0.6, container 0.9, field-range 2.3, invariant-accepts 0.6, invariant-refusals 2.2, scratch-storage 0.7, refusal-untouched 6.4, idempotence 0.4, continuation-partitions 1.7, dirty-peer 0.4, observer-flag 0.2 | |
| `unit.core.state.import` ASan | pass, no sanitizer report (TIMEOUT 600; about 22 times the native time, dominated by the SHA-256 passes over the 6.3 MB blob per refusal row; ASan is not part of the gate or the hosted workflow) | 367.4 |
| Task 4 six-name group (gate) | 6 of 6 | 8.2 |
| `runner.state-export` + `runner.supervisor-argv-bound` ASan | pass, no sanitizer report | 98.0 and 3.0 |
| gcc:13 `-Werror` closure over `core/t64_state.c`, `core/t64_rsp.c`, `core/tests/test_state_import.c` | compiled | 7.6 |
| Ubuntu 24.04 cc pre-flight (eight units: core importer, test, supervisor and runner in both variants, runner test) | one defect found and fixed, then clean | 14 |
| `repo.*`, `harness.*`, `gov.*` (41 names) | one failure (`repo.si-membership-sites`), regenerated, then 41 of 41 | 26 |
| `check-rsp-no-hle.py` gate and asan, `check-test-inventory.sh` gate and asan | pass, counts 223 and 225 | under 1 each |
| `core-lint.sh` | `files=98 violations=0` | 9 |

## Task 5 guard record

`python3 -B scripts/check-rsp-no-hle.py --build build/gate` stayed green after the single non-static accessor appended to `core/t64_rsp.c` (numstat `7 0`, no deleted line, no static helper added); `core/t64_state.c` was already on the roster (09-08). **No refresh required**, no pin changed, no guard edited. `scripts/check-test-inventory.sh` changed by one data row (numstat `2 1`).

## ASAN_ONLY decision and the name count

No ASan-only CTest name is added by 09-09 or 09-09.1 (K13 budget of two names), so no `ASAN_ONLY` row exists; the sanitizer-scaled fuzz campaign of 09-09.1 runs inside `unit.core.state.import`. This plan adds exactly one name (`unit.core.state.import`); the other 9-V22 names map to cases: `unit.core.state.roundtrip` = `roundtrip`, `.idempotence` = `idempotence`, `.continuation` = `continuation-partitions` (and the `roundtrip` continuation part), `.refusal` = the five refusal cases, `controls.real.state-import-refused` = the runner cases `import-refused-integrity`, `import-refused-build`, `import-option-refusals`, `import-bound-below-tick` inside `runner.state-export` (not renamed, no new name); `.boundary-matrix`, `.stalled-observer-stop` and `.mutation` are 09-09.1. Cumulative Phase 9 names: 44 of about 45.

## Prohibition checks (flagged-unverified in the plan; clean scanner output is evidence, not a dismissal)

- Rights and fixtures: `sh scripts/rights-scan.sh --tree` reports `tree=1214 history=skipped findings=0`; `git ls-files '*.sta' '*.sav' '*.eep' '*.sra' '*.rpl'` prints nothing; the in-process runner blobs exist only under `build/` (ignored) and the test's private directory, and are removed.
- Paths, user names, emails: `sh scripts/pii-scan.sh --tree` reports `tree_files=1214 commits=skipped findings=0`.
- Guard weakening: guard green on the gate build (and the asan build, see the table); the guard, its test and the inventory logic are unchanged (inventory: one data row).
- Frozen files: `git log --name-only <base>..HEAD -- scripts/qualify-phase09.py .github/workflows/ci.yml scripts/check-ci-yml.sh scripts/gate.sh scripts/rights-scan.sh tools/rights runner/t64_artifact_publisher.c runner/t64_artifact_publisher.h runner/t64_raw_capture.c runner/t64_real_fixture.c runner/t64_real_fixture.h scripts/build-boot-fixtures.sh` prints nothing; `git diff --quiet <base> -- runner/t64_real_result.c runner/t64_real_result.h runner/t64_real_fixture.c runner/t64_real_fixture.h docs/spec/result-protocol.md` exits 0.
- Claim wording: `sh scripts/check-claims.sh --files docs/claims/phase09.md` passes; the spec states that hashes are integrity checks and that reachability is not promised.

## Handoff for 09-09.1

- **Public spellings and lists:** as above and in `t64_machine.h`; the closed handler table is `t64_state_handler_lookup`; the scratch is `t64_state_summary` (600 bytes, `state_summary` member, `t64_state_summary_is_clear`).
- **Test-side helpers in `core/tests/test_state_import.c`:** `blob_resign(blob, length, RS_SECTIONS|RS_DECLARED|RS_TRAILER)`, `blob_resign_entry`, `blob_consistent`, the structural `tblob_decode` and `tblob_encode` (queue and section edits that change a length), the target machines `T_S1`, `T_E4K`, `T_S32K` with `targets_prepare`, the row struct and `run_rows` with `CHECK_UNTOUCHED` and `CHECK_SCRATCH`, `snapshot_take` and `snapshot_matches` (whole machine allocation and both packet storages), `build_s1`, `advance_pair`, `export_blob` and the reference header offsets. Add 09-09.1 cases to the `cases`, `suites` and `runners` arrays in `main` and the names to the `state-import` suite of `docs/spec/phase09-cases.json`.
- **Facts the matrix will meet:** the S1 machine (boot code running) already has a valid RSP DMA descriptor in flight at tick 13,000; the first retained video or audio packet has sequence 0; `vi`, `ai` and `rdp` carry combiner, depth and coverage members outside the v25 state, which the commit zeroes; the per-device rule for the RSP allows one pending issue event until its tick and for the VI one event of the current generation.
- **Extractor:** 72 member names; the mapping data, `--check`, self-test and `harness.state-field-use-audit` are 09-09.1.
- **Cost:** about 17 s native for the whole name, 6.4 s of it for the per-row whole-allocation snapshot; hashing 6.3 MB three times per refusal dominates (about 65 ms per row).

## Deviations from Plan

**1. [Plan text - scratch shape] The scratch holds per-pair aggregates, not 256 event slots.** A (device, kind) summary of count, first tick, token and value, largest token and its count is enough for every device rule and takes 600 bytes (limit 8,192). Files: `core/t64_internal.h`, `core/t64_state.c`.

**2. [Plan text - commit split] Steps 4 to 7 of Task 1 are one commit** (`49a904a`): the readers, the handler table, the digest recompute and the commit share one file and one test, so partial commits would reference code that does not link (AUTONOMY "coupled commit boundaries"). The accessor in `core/t64_rsp.c` moved into the RED commit so the test links.

**3. [Plan text - Task 2 RED] Task 2 started green by design** (see the RED table); the mutant record is the evidence.

**4. [Plan text - runner premises] Three plan premises do not hold** and are recorded in the open issues: the runner exports only at authored completion, a resumed run cannot report pass, and `--input` is mandatory in real mode. The `import-bound-below-tick` case asserts the existing prelaunch refusal (a state tick above the bound is refused before any advance) instead of an `advance-invalid` outcome, because the loop now resumes from the header tick.

**5. [Plan text - `argv-bound`] `runner/tests/test_phase09_runner.c` is not edited.** Its maximum-option control uses an audio fixture outside the composite family, so the import triple cannot join it; the slot sum is asserted at compile time (45 of 64) and the rows of `import-option-refusals` cover the option set.

**6. [Rule 3 - blocking] `docs/spec/profile-membership-sites.txt` regenerated** (two sites of the importer that carry the serialization version token); `repo.si-membership-sites` failed until then. The file is not on the plan's list.

**7. [Rule 1 - Linux] A glibc format-truncation warning in the new runner test** (a path built with `snprintf` into a fixed buffer) was found by the pre-push Ubuntu compile and fixed before any push.

**8. [Plan text - supervisor profile check] The profile-family refusal is the child's** (detail `refusal`, guest tick 0); the supervisor does not look the profile up, because the plan's own text keeps the family check in the child ("the child repeats the family check").

**9. [Assumption] `MASKED_AT_USE` fields are read unconstrained** and listed with their consuming sites in the spec; the field-use audit of 09-09.1 owns the mapping.

**Total deviations:** 9. **Impact:** no guard weakened or refreshed, no frozen file edited, no digest literal or F0/F1 pin touched; the only edits outside the plan's file list are the generated membership list and the two spec documents.

## Assumptions of this plan

1. A blob accepted by the importer is not claimed to be reachable, authentic or portable; the contract is memory safety and liveness for the cases the tests and 09-09.1 exercise.
2. The per-device rules are derived from reading the device sources named above; a rule the source permits that the rules refuse would be an over-refusal that the 09-09.1 matrix is designed to find.
3. The runner evidence limit of a resumed run (open issue 2) is a property of the existing result design, asserted and not changed.

## Authentication gates

None.

## Known Stubs

None. No placeholder text or empty wiring exists in the created or modified files.

## Threat Flags

None beyond the plan's register (T-09-66 to T-09-72): the new surface is the importer, three supervisor options with a forwarded descriptor, and one diagnostic line on the child stream that carries only a closed reason name.

## Self-Check: PASSED

- `git ls-files` lists `core/tests/test_state_import.c` and `scripts/check-state-field-use.py`; the nine commit hashes of the table are ancestors of HEAD.
- `git worktree list` shows the main checkout only; no `__pycache__` remains outside `build/`; no `.sta` file is tracked.
- No ROM, save, PIF or IPL image, 64DD media, hardware measurement or oracle was used; the only external input is the already cleared openIPL3 asset and the authored admit ROMs. No state blob is committed or uploaded. No push was made.
