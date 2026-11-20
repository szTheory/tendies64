# Phase 9 plan cross-check (parallel-authoring consistency)

Scope: 09-01, 09-02, 09-03, 09-03.1, 09-04, 09-05, 09-06, 09-07, 09-08, 09-09, 09-09.1, 09-10, 09-11,
read against 09-CONTEXT.md (D-xx and K1-K16), 09-RESEARCH.md, 09-PATTERNS.md, 09-PLAN-OUTLINE.md and HEAD
(`git grep` of core/ runner/ scripts/ tools/). Method: for each plan the frontmatter, "Artifacts this phase
produces", must_haves, threat register, verification and the tasks that touch shared files were read;
shared identifiers were grepped across all thirteen plans. The deep task text of every plan was not read line by
line (about 1.2 MB of plan text); findings below are those that survive grep and quoted-text confirmation.

VERDICT: FIX BEFORE EXECUTION. One executor-stopping contradiction (B1), five HIGH items, and a set of
MEDIUM/LOW items. Every fix is a text edit to a plan; none needs re-planning or a new plan. The split itself
(13 plans, strictly sequential) is sound and the dependency graph is acyclic and ordered correctly.

Tooling result (`gsd-tools.cjs query phase-plan-index 9`): all 13 plans are accepted, decimal ids included;
`depends_on` is honored and the computed waves are 1:09-01, 2:09-02, 3:09-03, 4:09-03.1, 5:09-04, 6:09-05,
7:09-06, 8:09-07, 9:09-08, 10:09-09, 11:09-09.1, 12:09-10, 13:09-11; ready_plans = [09-01]. The tool warns for
ten plans that the declared `wave:` differs from the DAG wave (see M7).

---

## BLOCKER

### B1. 09-09 Task 4 verification command can never pass (names a CTest name that does not exist)

- 09-09 Task 4 `<automated>`: `ctest --preset gate -R '^(runner\.state-export|controls\.real\.state-export-fsize|runner\.supervisor-argv-bound|runner\.guest-tick-cap|runner\.fixture-transcript-pin|runner\.phase09-admit|runner\.artifact-publisher)$'` with `<fails_when>... fewer than 7 tests matched ...</fails_when>`.
- 09-08 "Artifacts this phase produces": "`controls.real.state-export-fsize` is the pair of cases `export-size-cap` and `export-fsize-at-state-size` inside `runner.state-export` (K13 consolidation ...)". It is a 09-VALIDATION mapping, not a registered name; 09-08 registers exactly `unit.core.state.export` and `runner.state-export`.
- Result: the regex has 7 alternatives, 6 exist, the plan's own `fails_when` triggers on the first correct execution.
- Minimal edit: 09-09 Task 4 `<automated>` regex: delete `controls\.real\.state-export-fsize|`; `<fails_when>`: "fewer than 6 tests matched". (Also grep 09-09/09-09.1/09-10 for any other 09-VALIDATION-only name used as a ctest `-R` token; none other was found.)

---

## HIGH

### H1. `t64_machine_replace` does not exist at HEAD but 09-03, 09-04 and 09-09 build tests and read_first on it

- HEAD: `git grep machine_replace core` is empty; the real entry is `t64_machine_admit_rom` on an admitted machine (`core/t64_machine.c:2445`, internal `reset_guest_state`, line 1931).
- 09-03 Task 4: "Case reset-replace: after a poll, `t64_machine_replace` with the same ROM yields a digest equal to a freshly created and admitted machine ..." and "`t64_machine_replace` and reset: add `t64_si_reset_machine_state` beside the audio reset in `core/t64_machine.c`".
- 09-04 Task 6: "Case reset-blank: ... a successful `t64_machine_replace` with the same image ..." and read_first "core/t64_machine.c (t64_machine_replace and the reset site ...)".
- 09-09 read_first "t64_machine_admit_rom and t64_machine_replace ..." and the commit text "clears the RSP observation record exactly as `t64_machine_replace` does".
- 09-05 already carries the correction ("neither 09-04 nor this plan can call a function named `t64_machine_replace` (the tree has none; replacement is `t64_machine_admit_rom` on an admitted machine, `reset_guest_state` ...)") and 09-10 negative-checks the name with `! git grep -q -w t64_machine_replace -- core runner docs scripts tests tools fixtures`. So the authors know; only the executors of 09-03/09-04 (before 09-05 is read) and 09-09 are exposed. An executor of 09-03 writes a test that cannot compile, or invents the function (which 09-10's grep then flags as drift after the fact).
- Minimal edit: in 09-03 (reset-replace case, Task 4 text), 09-04 (reset-blank case and read_first) and 09-09 (read_first and commit sentence) replace `t64_machine_replace` by "`t64_machine_admit_rom` on an already admitted machine (reset in `reset_guest_state`, `core/t64_machine.c`)". Keep 09-05's note and 09-10's check as is.

### H2. The bound SOURCE_PATHS / PENDING_SOURCE_PATHS tuple omits three files created outside core/runner/fixtures/tools/rights

- 09-02 Task 7: "The tuple must be finalized by 09-03 with every file created in 09-04 to 09-09 (declared up front ...), because a later logic edit to a frozen file is an FH deviation".
- 09-03 final-tuple input: "the 09-02 tuple plus `scripts/build-phase09-admit-fixture.sh`, `scripts/build-pad-fixture.sh`, `scripts/build-save-fixtures.sh` (09-04), `scripts/build-replay-fixture.sh` (09-07), `scripts/check-state-field-use.py` (09-09), ... any script created later outside those directories must be declared in the tuple now or is a recorded FH deviation."
- 09-03.1: PENDING_SOURCE_PATHS "mapping `scripts/build-save-fixtures.sh` to 09-04, `scripts/build-replay-fixture.sh` to 09-07, `scripts/check-state-field-use.py` to 09-09" and case `rehearsal-flag-and-pending-sources`: "`PENDING_SOURCE_PATHS` holds exactly the three files".
- Not listed anywhere (grep of 09-01..09-05 and 09-03.1 returns zero for each): `scripts/k5-mutants.py` (created by 09-06, executed by CTest `repo.sched-k5-anchors`), `docs/spec/state-field-rules.txt` and `docs/spec/state-mutants.txt` (created by 09-09.1; the first is the input of `harness.state-field-use-audit`, the second is read by `unit.core.state.import` through `T64_STATE_MUTANT_INVENTORY`). `docs/evidence/phase09-first-hosted-pair.json` (09-03.1, input of `harness.phase09-collect-real`) is also unbound.
- Effect: gate-executed scripts and test data outside the qualifier's source binding, or a forced FH deviation (a qualifier edit after the freeze, which costs a native plus hosted cycle by D-06).
- Minimal edit: 09-03 final-tuple input and 09-03.1 `PENDING_SOURCE_PATHS` (and the 09-03.1 case text "exactly the three files" -> "exactly the five files") add `scripts/k5-mutants.py` -> 09-06, `docs/spec/state-field-rules.txt` -> 09-09.1, `docs/spec/state-mutants.txt` -> 09-09.1; add `docs/evidence/phase09-first-hosted-pair.json` to SOURCE_PATHS (it exists at the end of 09-03.1). 09-06 and 09-09.1 then need no qualifier edit.

### H3. Hosted time is measured once, before 62 percent of the new tests exist, and 09-11 cannot repair an overrun

- 09-03.1 gate: "Require both at or below 540 seconds (90 percent of the 600-second limit)", measured with 32 names ("cumulative Phase 9 gate name count (32)") against "the D-40 estimate of about 25 s".
- D-40 treats the +25 s as the whole-phase hosted addition; plans 09-04..09-09.1 then add 13 more names, the 11.5 MiB state export runner control (09-08), the EEPROM 440,000-tick fixtures, `unit.core.sched.matrix` (TIMEOUT 300, 09-06) and `unit.core.state.import` whose own budget is "the whole test stays within about 25 seconds on the native machine" (09-09.1; native, not hosted macOS). Each plan only records the native inventory-selftest seconds.
- 09-10 Task 1/PD-6: "the hosted pair of 09-03.1 is the only hosted data; ... the orchestrator may choose one hosted rehearsal after this plan (recorded as a risk, not planned)". 09-10 also says "No CTest name, CI job, dependency or hosted run is added; no push is made".
- 09-11 forbids logic edits; the only levers after an overrun are test-source edits, which restart the cycle. Cold macOS baseline is already 461-507 s of 600 s (D-40), so a +25 s underestimate can break the 600 s limit only visible at the end.
- Minimal edit: 09-09.1 (last plan that adds tests) and 09-10 Task 6 must add a required, non-qualifying hosted rehearsal run (push of the FR candidate, `--rehearsal` collect only, results logged by `scripts/ci-review.sh --append`) with the gate "warm <= 540 s and cold projection <= 600 s on both jobs; otherwise cut sampled partitions/iterations in 09-10 now". Change 09-10's "No hosted run" boundary and PD-6 from "risk, not planned" to "planned". Also have 09-06 and 09-09.1 record the per-CTest-name native seconds so the macOS cost can be scaled from the 09-03.1 pair. (This is the same hosted rehearsal asked for in the verifier recommendation below.)

### H4. The D-36 state-import mid-tick row and the write_generation behavior are owned by 09-10 (the review/fix plan), not by 09-09 or 09-09.1

- 09-06: "Handoff to 09-09: the mid-tick stop recipe ... and the all-devices workload W-ALL ... are the construction sources for the D-36 state-import row; 09-09 copies them locally" and "That row is handed to 09-09 ... it is not dropped."
- 09-09 and 09-09.1: zero mentions of W-ALL, mid-tick or H_ALL (grep). 09-09.1's B01-B16 boundaries and 09-09's fourteen cases do not contain a resume-at-mid-tick row.
- 09-10 PD-1/PD-3 and Task 3 ("Mid-tick export and import resume to the uninterrupted digest, and the write_generation decision implemented"): adds cases `mid-tick-resume` and `import-write-generation` to `core/test_state_import` and edits production `core/t64_state.c` ("in the single commit, after the readers have been applied, increment `write_generation`"); 09-10 states "09-09's commit does not mention it".
- Problems: (a) a locked decision row (D-36) and a production behavior change land in a plan whose purpose is independent review of a reconciled tree, so the review no longer reads a stable tree and a RED in Task 3 competes with the single budgeted fix cycle (D-46); (b) 09-05 and 09-08 say 09-09 owns the write_generation question implicitly (09-08: "`write_generation` ... per D-29"; 09-05 follow-up "generation travel"), but 09-09 never resolves it.
- Minimal edit: move 09-10 Task 3 content into 09-09.1 (new cases `mid-tick-resume` and `import-write-generation` in the `adversarial` suite, count 26 -> 28; the `core/t64_state.c` increment into 09-09 Task 3's commit with a case in `save-media-roundtrip`), and reduce 09-10 Task 3 to a recheck of those rows by the review lanes. If the move is declined, 09-09 and 09-09.1 must at least say "mid-tick row and write_generation are deliberately owned by 09-10 Task 3" so the 09-09 acceptance is not read as covering D-36.

### H5. Runner option-pair matrix is deferred to the 09-10 review, and one silent-invalid pair is not refused by anyone

- 09-07 cross-validation: "replay options are refused with `--raw-capture-dir`, with the 09-05 save import and export options, and in synthetic mode".
- 09-08 cross-validation: "refused with `--raw-capture-dir`, refused in synthetic mode ... both require the artifact pair" (no mention of replay or save flags).
- 09-09: "combination with `--input` or `--raw-capture-dir`" refused, "combination with the 09-02 `--artifact-dir` pair is allowed"; no statement about `--replay-record`, `--replay-play`, `--save-import`, `--save-export-leaf`, `--state-export`.
- 09-07 header: "`boot` ... only `cold-admit`; `state` is reserved and refused" and D-34 "Boot is `cold-admit` only". A run with `--state-import` plus `--replay-record` would record a `cold-admit` replay of a run that did not start from cold admit; replay then cannot reproduce it (silent wrong artifact). 09-10 only plans "every pair of Phase 9 runner options has a recorded refusal or allowance" as a late drift check, i.e. after the code froze.
- Minimal edit: 09-09 Task 4 cross-validation list: refuse `--state-import` together with `--replay-record`, `--replay-play`, `--save-import` and `--save-export-leaf` (usage refusal before guest start) and add the cases to `import-option-refusals`; state in 09-08 that `--state-export` is allowed with `--replay-record` and `--save-export-leaf` (distinct leaves, single `--artifact-max-bytes` at least the largest artifact; the FSIZE rule M + 65,536 applies per file). Record the full pair table once in `docs/spec/state-container.md` (09-09) so 09-10 verifies rather than invents it.

---

## MEDIUM

### M1. 09-08 contradicts 09-05 about `docs/spec/result-protocol.md`

- 09-05 edits it: files_modified includes `docs/spec/result-protocol.md`; gate check "`grep -c 'artifact-exists' docs/spec/result-protocol.md` -ge 1"; the nine `artifact-*` detail names are added to `runner/t64_result_check.c` (host-error stop only).
- 09-08 Task 4 spec: "`docs/spec/result-protocol.md` lists no raw-capture or artifact detail vocabulary so it is not edited here and is flagged for the 09-10 review" and the SUMMARY must carry "the finding that `docs/spec/result-protocol.md` omits the raw-capture and artifact detail vocabularies".
- After 09-05 the artifact vocabulary is present (the `state-export-*` and raw-capture names are not). The 09-08 SUMMARY item is false for artifact names and 09-09 requires "result-protocol document ... unchanged".
- Minimal edit: 09-08 spec bullet and SUMMARY list: "result-protocol.md gained the artifact-* names in 09-05; it still omits the raw-capture and the six `state-export-*` names; flagged for 09-10, not edited here".
- Related ownership note: 09-02 publishes the nine error names but registers none in `runner/t64_result_check.c`; the first runner path that can emit them is 09-05. Acceptable, but state in 09-02 "no runner result path emits publisher errors before 09-05" so the 09-02 checker pass is not expected to cover them.

### M2. 09-09.1 hedges the inventory representation of a Python test ("if the checker rejects a python entry, omit the binary entry")

- 09-09.1 Task 4: "add a `state-audit` suite ... (ctest `harness.state-field-use-audit` and the five self-test case names; if the checker rejects a python entry, omit the binary entry and record the decision)".
- The checker `scripts/check-phase09-cases.py` is copied in 09-02 and frozen at FH; its suite shape is `{ctest:[...], binaries:[{path, cases}]}` (see `docs/spec/audio-cases.json`), and 09-02's `inventory-shape-refused`/`zero-case-inventory-refused` may refuse an empty `binaries` list.
- Minimal edit: 09-02 (inventory validator and checker behavior list) states explicitly whether a suite with `ctest` only and `binaries: []` is accepted; 09-09.1 then states one definite shape. Same question applies to any suite that lists a `repo.*`/`harness.*` name (09-01 `repo.si-membership-sites`, 09-06 `repo.sched-k5-anchors`).

### M3. 09-11 claim block 7 counts `repo.sched-k5-anchors`, which is in no inventory suite

- 09-11 block 7: "(`unit.core.sched.matrix`, `unit.core.sched.ledger`, `unit.core.sched.characterize-fifo`, `unit.core.profile.quantum-identity`, `repo.sched-k5-anchors`)".
- 09-06: suite `sched-matrix` is "(CTest name `unit.core.sched.matrix`, binary ...)" only; `repo.sched-k5-anchors` is registered in `runner/CMakeLists.txt` but placed in no suite, so the native record built from the case inventory does not verify that it passed.
- Minimal edit: 09-06 `docs/spec/phase09-cases.json` row: add `repo.sched-k5-anchors` to the `sched-matrix` suite `ctest` list (subject to M2), or 09-11 block 7 drops it from the counted names and keeps it as prose.

### M4. `t64_machine_save_info` check order disagrees with D-24 and with 09-05's R1/R2

- D-24: "Check order ...: admitted, pointer/length, profile has a device, ...". 09-05 table: "R1 no admitted image, NOT_READY; R2 pointer or span ... INVALID_ARGUMENT".
- 09-04: `t64_machine_save_info` "running the D-24 check prefix: argument pointers (INVALID_ARGUMENT), admitted (NOT_READY), ..." (pointers first); 09-05 states the info body keeps "the info signature, statuses and order exactly as 09-04 pinned them (its existing cases must stay green)" while sharing one prefix helper for R3-R6.
- Effect: for the same bad call (NULL `info_out` on a machine with no image) info returns INVALID_ARGUMENT, import/export return NOT_READY; the 09-05 refusal matrix covers only import/export.
- Minimal edit: 09-04 Task 1/6 info order -> "machine NULL, admitted, pointers, ..." (matches D-24 and R0-R2), or 09-05 explicitly records info as the one documented exception in `docs/spec/save-media.md` and adds one info row to `refusal-matrix`.
- Related (LOW): 09-08 export and 09-09 import check pointers before NOT_READY ("mirroring `t64_machine_digest`"), the opposite of D-24 for the save family; document both orders in the specs so 09-10 does not file it as drift.

### M5. 09-09 leaves `busy_until` "unrestricted" while 09-04/09-05 give it a hard reachable bound

- 09-09 section 11 reader: "`busy_until` unrestricted and a stale value kept".
- 09-04: busy deadline computed from the execute tick E with `T64_SAVE_EEPROM_BUSY_TICKS` 375000, so a reachable value is at most `tick + 375000`. 09-05 R9 refuses save import while `busy_until` is above `machine->tick`.
- An accepted blob with `busy_until` near the 64-bit maximum makes every later save import fail with `T64_STATUS_SAVE_BUSY` for the life of the machine; 09-09.1 `liveness-bounded-advance` only proves the advance terminates.
- Minimal edit: 09-09 section 11 reader adds the derived rule `busy_until <= tick + T64_SAVE_EEPROM_BUSY_TICKS` (rule `DERIVED_EQ`/`TICK_GE` family, reason `FIELD_RANGE`) and 09-09.1 `state-mutants.txt` gets a `busy-until-future` row; a stale value at or below the tick stays accepted.

### M6. Replay `save_init: image` cannot be produced or replayed through the runner

- 09-07: replay options "refused ... with the 09-05 save import and export options"; header `save_init` "kind `blank`, or kind `image` with `length` and `sha256`"; `t64_replay_live_save_init` "reads the live save content through the 09-05 export".
- D-34: header carries `save_init` (`blank` or SHA-256 plus length); D-35: runner flags "refuse undefined combinations". With save import refused next to replay, the `image` form is reachable only in-process (`t64_replay_play`), never in a real supervised run; `runner.replay-mix` only covers `blank`.
- Minimal edit: either 09-07 allows `--save-import*` with `--replay-record/--replay-play` (the import happens before the first advance, so D-34's "mid-run save import is not replayable" still holds) and adds a real-run row, or 09-07 `docs/spec/replay.md` and 09-11 claim block 10 state that `save_init image` is verified in-process only.

### M7. Declared `wave:` frontmatter disagrees with the DAG for ten plans

- Declared: 09-03.1 `wave: 3.1`, 09-04 `wave: 4` ... 09-11 `wave: 11`; 09-09 and 09-09.1 both declare `wave: 9`. `gsd-tools query phase-plan-index 9` prints ten "declared wave ... but depends_on DAG places it in wave N" warnings; `roadmap.cjs` groups by `parseInt(fm.wave)`, so 09-03/09-03.1 (both 3) and 09-09/09-09.1 (both 9) are listed in one wave in roadmap progress. Execution order from the index is correct (it uses the DAG).
- Minimal edit: set the declared waves to the computed values: 09-03.1 `wave: 4`, 09-04 5, 09-05 6, 09-06 7, 09-07 8, 09-08 9, 09-09 10, 09-09.1 11, 09-10 12, 09-11 13. Update the phrases "wave 3.1" and the outline table only if desired (the outline is historical).

### M8. CTest-name budget is exactly exhausted with zero headroom and the ASan-only name is outside the count

- Sum of the stated additions: 10 (09-01) + 14 (09-02) + 7 (09-03) + 1 (09-03.1) + 3 (09-04) + 2 (09-05) + 2 (09-06) + 2 (09-07) + 2 (09-08) + 1 (09-09) + 1 (09-09.1) = 45 = K13's "45 or fewer"; 09-10 PD-2 "(K13 budget 45, exhausted) plus one ASan-only name; this plan and 09-11 add zero". The arithmetic is consistent across plans (checked), but `fuzz.joybus` is a 46th inventory name and any fix-cycle test in 09-10 that needs a name has none.
- Minimal edit: 09-10 PD-2 states "if a fix needs a new name, fold it into an existing suite; the K13 figure is 45 gate names plus `fuzz.joybus`, accepted at 46 inventory names". No plan edit otherwise.

---

## LOW

- L1. 09-04: "the cumulative Phase 9 gate name count is the 09-03 SUMMARY figure plus exactly these three" (31 + 3 = 34); 09-03.1 already added one (32), so the correct figure after 09-04 is 35. Edit 09-04 lines 181 and 425: "the 09-03.1 SUMMARY figure (32) plus three".
- L2. 09-06 lists "whether section 11 is emitted for the stem id" among things "no sibling plan provides"; 09-04 states it ("for the three save ids only, the stem id stream ... stay byte-identical"; "the stem id stream keeps markers 1 to 10"). Edit 09-06: cite 09-04 as the source; 09-08/09-09 already read it.
- L3. 09-05 fixture id `phase09-save-roundtrip-s32k` lacks the `-v1` suffix every sibling carries (`phase09-pad-poll-si-v1`, `phase09-eeprom-rw-e4k-v1`, `phase09-sram-rw-s32k-v1`, `phase09-replay-mix-si-v1`). Harmless as long as 09-11 F2 pins copy the manifest id; add the exact id to 09-11's `fixtures` pin note.
- L4. The 09-02 `argv-bound` maximum-option control is extended only by 09-09 ("gains the new options in its maximum-option control"); 09-05 has its own `argv-max-save`, 09-07/09-08 add options (+8, +4) without extending the 09-02 control. Slot arithmetic is fine (23 + 8 + 8 + 4 + 2 = 45 of 64 including the NULL), but the maximum-set control at 09-09 must include save, replay and state flags together only if the options can co-occur; per H5 they cannot (replay excludes save; state-import excludes replay), so the control should use the largest legal subset and say so.
- L5. 09-11 edits `scripts/tests/test_phase09_qualification.py`, which is inside SOURCE_PATHS, and removes the case `claim-skeleton-has-no-real-block`; 09-11's freeze check says "scripts (other than the one bound test file)". This is documented and acceptable, but 09-03.1 should record in its FH note that this one test-file edit is the sanctioned F2 exception so 09-10's `09-10-freeze-check.py` does not file it as FD.
- L6. 09-02 says no inventory hash pin and that F2 literals are "data rows in 09-11", yet the qualifier (09-03.1 constant list) has no `F2_PINS` constant; 09-11 puts `F2_PINS` in the test file. The qualifier therefore does not enforce the F2 pins at `--verify-evidence` time. Either accept (pins are test-enforced, 09-11 already says "qualifier unchanged") and say so in 09-11's evidence statement, or add an empty `F2_PINS` constant in 09-03.1 (the 09-11 text already permits filling "an empty F2 pin constant").
- L7. HOSTED_ROWS (09-03.1) contains only the two pad-poll names; the other claimed fixtures (eeprom, sram, roundtrip) run only inside the ordinary hosted gate and are not in the cross-platform pair comparison. 09-11 claim blocks 2-5 say "accepted authored ROM rows that reported pass in the real runner"; make the source of that count (native record) explicit in the claim prose, or add `hosted` suite rows for the save fixtures in 09-04/09-05 (data only, but then the 09-03.1 collector needs generic rows; today "plans 09-04 to 09-11 add data rows only" has no row owner).

---

## Recommendation: run the goal-backward verification before 09-11's qualifying cycle

Evidence: 09-11 line "no second independent review is scheduled here (09-10 owns it) and the goal verification is the orchestrator's phase-completion gate" places the gsd-verifier role after the exact-SHA native plus hosted cycle. That is the Phase 8 failure mode: an unpinned behavior found after qualification forces a new candidate commit and a full native plus hosted cycle ("A logic defect found here ... forces a new candidate commit and a new native plus hosted cycle" - 09-11).

Recommendation: yes, run it inside 09-10 (new Task 5a, between the review/audit and the single fix cycle), not after 09-11.

What it requires:
1. 09-10: a task "Goal-backward verification pass" using the gsd-verifier role on the FH..current tree. Input: ROADMAP success criteria 1-4, D-xx decisions mapped to named tests, 09-VALIDATION rows, the 09-VALIDATION-to-case mapping notes (every plan records one), the claim skeleton, and the drift table. Output: a draft `09-VERIFICATION.md` (or section of 09-REVIEW.md) with a criteria-to-test map; every GAP becomes a finding in the same single budgeted fix cycle (D-46), so goldens regenerate once.
2. 09-10: move the success-criteria map currently authored in 09-11 Task 6 (`docs/evidence/phase09-success-criteria.md`) into a pre-F2 draft that the verifier checks; 09-11 Task 6 then only binds it to the SHA. Add to the 09-10 exit checks "verifier status passed, no open gap, or each gap has an FX record".
3. 09-10: the hosted rehearsal from H3 sits right after the fix cycle, so a verifier-driven fix and a time overrun are both discovered before F2.
4. 09-11: replace "the goal verification is the orchestrator's phase-completion gate" by "the verifier's pre-F2 report is an input; the phase-complete verification is confirmatory and must report no new gap; a new gap there is a recorded deviation". 09-11's freeze check (FR..X product diff empty) already guarantees the verified tree is the qualified tree.
5. Check items the verifier will find that no plan currently pins (from this review): H4 (D-36 mid-tick row), H5 (option pairs), M5 (busy_until), M6 (replay save_init), L7 (hosted rows for save fixtures).

Cost: one agent pass plus its fixes inside the already budgeted cycle; against a possible 15-minute-plus hosted cycle and a re-run of native normal + ASan/UBSan + gcc:13.

---

## Attacks that found nothing (checked, consistent)

- Dependency graph and ordering: every `depends_on` points to an earlier plan; 09-04 depends on both 09-03 and 09-03.1; no plan reads a symbol from a later plan except the items listed above.
- Threat IDs: T-09-01..05, 06..15, 16..23, 24..27, 28..35, 36..45, 46..49, 50..58, 59..65, 66..72, 73..77, 78..82, 83..86 are contiguous with no overlap or gap; 09-10 reserves T-09-87+ and 09-11 states the continuation; the 09-10 disposition table covers T-09-01..82 and lists 09-11's as pending.
- Status values: 20, 21, 22 (09-04), 23, 24, 25 (09-05) are contiguous after the 19 present at HEAD (`T64_STATUS_BUS_DEVICE_REFUSED` = 19); 09-08 adds none; 09-09 says "continue after the highest in use"; 09-10 expects 26-33.
- Section ids: 1-9 (v24), 10 SI_PIF (09-03), 11 SAVE_MEDIA (09-04, three save ids only), 16 OBSERVER_CONTINUITY (09-08) agree across 09-01/03/04/08/09; domain string `t64.state.si.25` agrees; observer body (five u64: drain cursor, video head/count, audio head/count) agrees between 09-08 and 09-09, including the DROP_OBSERVERS policy.
- Handler table: 09-09's ten (device, kind) pairs equal 09-01's closed ledger without the synthetic device; `t64_input_event_handler` exists at HEAD, `t64_rsp_issue_event` is file-static and 09-09 adds the accessor.
- Supervisor argv slots: 09-02 23 (+1) of 64, +8 (09-05 save), +8 (09-07 replay), +4 (09-08 state export), +2 (09-09 state import) = 45; every plan extends the sum in its own commit.
- FSIZE: 09-02 rule (artifact maximum + 65,536 with the pair, otherwise 69,632; report/capture bounds unchanged), 09-05 (maximum at least the kind size) and 09-08 (state size under the raised limit, 32 MiB artifact maximum vs 64 MiB container ceiling) do not conflict.
- Latch versus replay `at`: 09-03 "a sample at E sees the last input stamped below E", `next_visible_from` = stamp + 1; 09-06 K5-1 mutant replaces exactly that expression; 09-07 `at` is the submission tick, `event_tick >= at`, with JIT versus pre-queue digests differing only through pending queue content (explicitly a non-claim). No contradiction.
- EEPROM busy: 09-04 deadline from execute tick E, K5-2 in 09-06 targets the same expression, 09-05 R9 and 09-09.1 B15 use the same `busy_until` boundary (busy at deadline minus 1, idle at the deadline); section 11 carries `busy_until` and `pi_save_direction`.
- Refusal postconditions: save (09-05, includes `write_generation`), state import (09-09 `refusal-untouched`, digest and storage bit-identical), runner prelaunch refusal before the first advance (09-09), replay identity mismatch (09-07) are each specified.
- write_generation inside 09-05/09-08: observer-only, outside digest and container, reset by replacement; the only gap is the import behavior (H4).
- Frozen files: from the frontmatter lists, no plan after 09-03.1 edits `scripts/qualify-phase09.py`, `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh` or `scripts/gate.sh`; `scripts/rights-scan.sh` is edited only by 09-05 as added selftest rows (stated), `tools/rights` only by 09-02; `scripts/check-test-inventory.sh` and `scripts/check-rsp-no-hle.py` are edited as data rows. FR/F2 handling in 09-10/09-11 is consistent with the outline's bound-source map.
- Tags `phase09-fh`, `phase09-fr`, `phase09-f2` are spelled identically in every plan; no F0/F1 tag is referenced anywhere.
- Profile ids and lengths: 38/42/43/43 characters verified; `T64_REAL_GUEST_TICKS_LONG_CAP` and the fixture row field names (`guest_tick_cap`, `expected_log_bytes`, `expected_log_sha256`) are used identically in 09-02/03/04/07.
- Rights scanner: `.eep`, `.sra`, `.sav` are already denied at HEAD (`tools/rights/t64_blobscan.c`), matching 09-05's "already denied" rationale; `.sta` and `.rpl` plus the T64STATE magic are added by 09-02 before FH; the replay report leaf `.rpr` is private and not in any allowlist.
- Existing CTest names used in 09-11's `-R` (`gov.claims.repo`, `gov.claims.seeds`, `gov.rights.repo`, `gov.rights.selftest`) exist at HEAD.
- Roadmap success criteria 1-4 each have an owning plan (1: 09-03; 2: 09-04/09-05/09-09; 3: 09-07/09-08/09-09; 4: 09-01/09-03/09-06) and a claim block in 09-11; the gaps are in the details above, not at criterion level.
