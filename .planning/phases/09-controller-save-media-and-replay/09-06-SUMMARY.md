---
phase: 09-controller-save-media-and-replay
plan: "06"
subsystem: scheduler-ordering
tags: [core-02, ordering-matrix, tie-labels, mi-producers, count-wrap, quantum-binding, k5-mutants, f1-state-freeze, evidence-limit]

requires:
  - phase: 09-controller-save-media-and-replay
    provides: "09-01 the four ids, the v25 digest walker with span classes, the closed event ledger, the pair tests and PHASE09_TESTS; 09-02 the case inventory and its checker; 09-03 the SI device, the joybus parser, the input latch and the pad-poll frame literals; 09-04 the EEPROM busy deadline and section 11; 09-05 the SRAM window and the save API"
provides:
  - one authored matrix binary (core/test_sched_matrix, nine cases) that orders CPU, RSP, RDP, VI, AI, PI, SI and host input through the single guest-time scheduler under coincident, partition, interleave, resource-limit, MI-producer and Count-wrap schedules
  - fifteen tie labels each owned by a named case, with literal expected orders and comparator labels printed as COMPARATOR lines
  - scripts/k5-mutants.py, an out-of-tree mutant driver (five K5 mutants, two supplemental mutants, an equivalent control that survives and a compile control that is invalid) and its anchor check registered as repo.sched-k5-anchors
  - the F1 state content record: 58 per-section length and SHA-256 goldens of six authored states and an audit of all 67 members of struct t64_machine
  - the CORE-02 ordering evidence section with the success criterion 4 map and the evidence limit
affects: [09-07, 09-08, 09-09, 09-09.1, 09-10, 09-11]

actuals:
  tokens: 45100
  tasks: 6
  commits: 7
plan_head_before: 503d95d91ce55c4b6861e487c89ddef93cca0dc3
plan_head_after: 025cea9ee2331b17c9911da01eddffb58155a1ab

tech-stack:
  added: []
  patterns:
    - "a schedule driver that resumes a resource-limit stop under a call cap and fails with resume-liveness when a call makes no progress"
    - "observation = digest, queue snapshot, next sequence and trace hash; every schedule run is compared with the whole advance and with the literal order"
    - "comparator labels printed as COMPARATOR <label> case=<name> so a mutant driver can prove which comparator was reached"
    - "out-of-tree mutants: throwaway worktree, core library built with the gate flags and Werror, accepted baseline first, KILLED only with the named label"
    - "section lengths derived by hand from the field tables before any capture, hashes captured as regression pins"

key-files:
  created:
    - core/tests/test_sched_matrix.c
    - scripts/k5-mutants.py
  modified:
    - core/CMakeLists.txt
    - runner/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - docs/spec/machine-contract.md
    - docs/spec/phase09-cases.json

key-decisions:
  - "SP DMA raises no MI bit: its completion handler never sets pending, so mi.sp-dma records an absence; the RSP break raises MI SP only with interrupt-on-break set, and the AI raises MI AI only when its descriptor queue falls from two entries to one"
  - "The all-devices workload runs the VI with v_sync 16: the video queue holds two packets and nothing drains it, so with v_sync 8 the third field boundary at tick 34,016 stalls the VI with no progress"
  - "tie.pi-sp runs on the skeleton profile where the legacy SP DMA completion event (4/1) exists; the RSP-functional profiles carry SP DMA on RSP issue events (4/2)"
  - "The resource-limit case compares the budget runs at seven checkpoints inside the SI read tail, not only at the horizon, because the completion-from-budget mutant is equivalent at 4,096 and converges after the read completes"
  - "The mutant driver reuses the main tree openIPL3 asset (SHA-256 checked against the pin) instead of building the boot fixtures in the worktree, which would rebuild the pinned MIPS toolchain"

requirements-completed: []

coverage:
  - id: M1
    description: "Equal-tick order stays tick-then-sequence FIFO on the new family; both insertion orders of every authored coincident pair give the literal order and identical digest, queue snapshot and trace across whole, split and step schedules with the trace on and off"
    requirement: "CORE-02"
    verification:
      - kind: unit
        ref: "unit.core.sched.matrix (cases tracer-coincident, cross-device)"
        status: pass
    human_judgment: false
  - id: M2
    description: "One all-devices workload reaches every scheduler ledger row and its final digest is identical across schedules, event budgets, mid-tick counter stops and an output-backpressure stop; two and three interleaved machines equal their solo runs"
    requirement: "CORE-02"
    verification:
      - kind: unit
        ref: "unit.core.sched.matrix (cases partition, interleave, resource-limit-resume)"
        status: pass
    human_judgment: false
  - id: M3
    description: "MI is ordered through its producers' events: the producer record is directly followed by the MI pending record, and a same-tick CPU step sees the interrupt exactly when the producer was sequenced first"
    requirement: "CORE-02"
    verification:
      - kind: unit
        ref: "unit.core.sched.matrix (case mi-producers)"
        status: pass
    human_judgment: false
  - id: M4
    description: "Count wrap by direct state over spans beyond 2^33 ticks with independently derived literals; the quantum is bound through the profile digest with no result key"
    requirement: "CORE-02"
    verification:
      - kind: unit
        ref: "unit.core.sched.matrix (cases count-wrap, quantum-binding)"
        status: pass
    human_judgment: false
  - id: M5
    description: "Five required non-equivalent mutants and two supplemental scheduler mutants are killed at their named comparators; the equivalent control survives and the compile control is invalid"
    requirement: "CORE-02"
    verification:
      - kind: integration
        ref: "python3 -B scripts/k5-mutants.py --run (exit 0, seven KILLED, one SURVIVED, one INVALID); repo.sched-k5-anchors"
        status: pass
    human_judgment: false
  - id: M6
    description: "State content frozen at F1: per-section goldens, hand-derived lengths and the member audit"
    requirement: "CORE-02"
    verification:
      - kind: unit
        ref: "unit.core.sched.matrix (case state-content-f1)"
        status: pass
    human_judgment: false
  - id: M7
    description: "Specification wording: the evidence limit, the MI statement, the quantum statement and the criterion 4 map use no reserved claim word and no untested sentence"
    requirement: "CORE-02"
    verification: []
    human_judgment: true
    rationale: "Wording review against the claim boundary is a judgment; the reserved-word grep, the claims scan and the cited-test existence checks are automated"

duration: 36min
completed: 2026-11-20
status: complete
---

# Phase 9 Plan 06: CORE-02 ordering matrix, K5 mutants and the F1 state freeze Summary

**One authored matrix binary orders every in-scope device through the single tick-then-sequence scheduler under coincident, partition, interleave, resource-limit, MI-producer and Count-wrap schedules, five non-equivalent K5 mutants and two supplemental ones are killed at their named comparators out of tree while an equivalent control survives and a compile control is invalid, the quantum is shown bound through the profile digest, and state content is frozen at F1 with 58 section goldens and a 67-member audit, all with no production source change.**

## OPEN ISSUES FOR THE ORCHESTRATOR (read first)

1. **Nothing blocks the next plan.** No `core/t64_*.c` file, public header, guard, qualifier, `ci.yml`, `gate.sh` or runner source changed; `check-rsp-no-hle.py` is green on both builds with no pin refresh. `harness.rdp-oracle-authority` passes (this plan does not touch `fixtures/manifest.json`), so no rebind is needed before a push.
2. **Not pushed.** The plan forbids a push and a hosted run here. Local main now holds the 09-04 and 09-05 commits, the two oracle rebind commits and this plan's seven commits plus the closeout commit.
3. **The ASan run of `unit.core.sched.matrix` takes 85 s** (native 4.6 s) against its `TIMEOUT 300`. That fits locally with a 3.5 times margin; a slower hosted runner is the first thing to watch in 09-11 (see the CI/CD notes).
4. Deviations 1 to 12 below are plan-text inconsistencies or measured corrections resolved without a design change; none touches a frozen file.
5. `requirements.mark-complete CORE-02` returned `not_found` (the verb did not match the REQUIREMENTS.md line), so the checkbox and traceability row were not changed by hand. The CORE-02 evidence of this plan is complete locally; whether the requirement closes before the phase review (09-10) and the hosted qualification (09-11) is the orchestrator's call.

## Performance

- **Duration:** 36 min (2026-11-20 00:46Z to 01:22Z, plan start to the last verification) for six tasks.
- **Commits:** 7 measured from the ledger (`503d95d..025cea9`, before this summary).
- **Files:** 7 changed (4,258 insertions, no deletion), 2 of them created.
- **Realized cost:** about 45,100 tokens on the plan's chars/4 scale over the added diff text (180,536 characters), against the estimate of 180,000; the estimate was low-confidence and over-counted.

## Commits

| Commit | Task | Subject |
| --- | --- | --- |
| `4696bc4` | 1 (tracer) | schedule driver, observation, comparator labels, VI and AI tie against literal orders, suite registered |
| `49d5f8f` | 2 | cross-device pair table, W-ALL partition, interleave |
| `c3d9e95` | 3 | resource-limit and mid-tick resume, MI producers, Count wrap, quantum binding |
| `eb37f3b` | 4 | `scripts/k5-mutants.py` and `repo.sched-k5-anchors` |
| `f60bfb1` | 5 | F1 goldens, hand-derived lengths, the 67-member audit |
| `b030e55` | 5 follow-up | relative asset fallback so the plan's direct-run verify commands work |
| `025cea9` | 6 | ordering evidence section, criterion 4 map, `sched-matrix` case row |

## Accomplishments

- **Matrix (Tasks 1 to 3).** `core/test_sched_matrix` has nine cases in `--list-cases` order: `tracer-coincident`, `cross-device`, `partition`, `interleave`, `resource-limit-resume`, `mi-producers`, `count-wrap`, `quantum-binding`, `state-content-f1`. Native 4.6 s for the suite (about 1.7 million checks); 85 s under ASan.
- **Tie rows.** Seven pair rows run every schedule (whole, SPLIT3 at the tied tick and at a second centre, steps 1, 3, 7 and 4,096, one-tick slices from `T - 3`, trace on and off) in both insertion orders against literal dispatch orders, each behind a reached assertion that both events are queued at the tied tick in the stated order. The SI rows use E = 2,500 + 16,971 = 19,471 and C = E + 4,096 = 23,567 written from the policy literals. A same-setup control stamps the input at E - 1 and reads 80 00 01 FF in both orders; the input stamped E reads 00 00 00 00.
- **W-ALL.** CPU delay loop, staged WR64B, PI DMA, SP DMA and an RSP program that retires an add and breaks, RDP full syncs, two AI frames, the VI, and three host inputs at E - 1, E and E + 1. The store tick S = 3,008 (5 setup instructions, 3 x 1,000 loop, 3 more) is derived and equals the calibration run. The census (one-tick calls, queue scanned before each, trace drained after each, zero dropped) sees the nine rows the stem carries. The final digest and queue are identical across steps 1, 3, 7, 4,096, SPLIT3 at S, E, C and the VI and AI tick 1,418, trace on with 16-tick steps, and event budgets 4,096, 1, 2 and 3 compared at seven checkpoints. The three save ids equal themselves across whole, 7-tick and 4,096-tick runs.
- **Mid-tick and backpressure stops.** Eleven events share tick 4,096 (SI complete, five instruction, three frame, a PI completion, a trailing no-op); instruction targets 1 to 5 and frame targets 1 to 3 each stop with the queue front at 4,096 and resume to the uninterrupted digest. An AI output stall leaves tick and queue unchanged on a repeated advance and resumes after a drain to the digest of a run drained at the same tick.
- **MI.** PI, RSP break, RDP full sync, VI, AI and SI each show their event record directly followed by the MI pending record; a CPU step placed at the SI completion tick takes the exception at 4,096 when the producer was sequenced first and at 4,097 when the step was.
- **Count wrap.** Spans 2^33 + 10 and 2^33 + 9 on the model 3 profile and the stem id give Count 5, generation 2, latched, phase 0, deadline 2^34 + 10 and Count 4, generation 1, not latched, phase 1, deadline 2^33 + 10; splits at 2^33 - 1, 2^33, 2^33 + 1 and 2^32 + 1 and two direct halves agree; a generation 3 literal is reported as a mismatch.
- **Quantum.** The stored profile digest equals the profile hash, the quantum 2 hash of a descriptor 11 copy differs from the canonical AI hash and equals an external SHA-256 of the serialized bytes, swapping it changes the canonical digest and restoring it restores the digest. `git diff --name-only 503d95d..HEAD -- runner docs/spec/result-protocol.md` prints only `runner/CMakeLists.txt` (the anchor-check registration); no result key was added.
- **F1.** Section lengths for 2 to 7, 10 and 11 were derived by hand before capture and equal the walker; 8 plus 9 is cross-checked against the stripped v24 stream; 58 golden rows are in the spec; the audit lists all 67 members.

## RED-equivalent record (start-green rationale and controls)

Every task characterizes behavior that already exists (the scheduler order from 09-01, the SI, latch and joybus code from 09-03, the save deadline from 09-04), so every case started green by design. Their discriminating power comes from the in-test controls below and from the out-of-tree mutants, which are the RED of this plan.

- **Task 1.** First green run 53,008 checks. A deliberately transposed literal in a scratch copy produced `COMPARATOR trace-order case=tracer-coincident` (7 lines) and `compare_order(swapped...) got 0 want 2`. In the test: the swapped expectation must return the mismatch kind and a truncated expectation the length kind, after the accepted baseline of the same setup.
- **Task 2.** Two first-run failures that were builder defects, not behavior: the VI stall at tick 34,016 (`COMPARATOR resume-liveness`, Deviation 3) and a trace drop in the calibration run (a single 3,009-tick call overflows the 256-record ring; the calibration now steps one tick at a time). The wrong-horizon interleave control reports `solo-reference` after the accepted baseline.
- **Task 3.** The Task 3 first run passed except the quantum case, where the stem id refuses a quantum 2 hash (the 09-01 refusal, so the contrast is a descriptor 11 copy). The expected-literal generation 3 control reaches `count-state`.
- **Task 5.** The hand-derived lengths matched the walker on the first capture. Controls: a flipped byte in the section 6 body reports `section-golden`; a pinned length plus one in section 2 reports `section-length`; each after an accepted baseline.

## Mutant results (Task 4)

Driver `python3 -B scripts/k5-mutants.py --run`, run twice (after Task 4 and again at the final source `b030e55`), exit 0 both times. Retained table of the second run (`build/gate/09-06-k5-results.keep.txt`, ignored):

| Id | Class | Expected | Seconds | Failing check |
| --- | --- | --- | --- | --- |
| `latch-visible-from-wrong-tick` (K5-1) | KILLED | KILLED | 4.4 | `test_sched_matrix:cross-device` (`si-sample`) |
| `eeprom-busy-from-store-tick` (K5-2) | KILLED | KILLED | 1.5 | `test_save:eeprom-busy` |
| `cpu-successor-stale-sequence` (K5-3) | KILLED | KILLED | 4.2 | `test_sched_matrix:cross-device` (`queue-snapshot`) |
| `si-completion-from-event-budget` (K5-4) | KILLED | KILLED | 2.1 | `test_sched_matrix:resource-limit-resume` (`resume-digest`) |
| `latch-promote-sample-reversed` (K5-5) | KILLED | KILLED | 4.5 | `test_sched_matrix:cross-device` (`si-sample`) |
| `tie-break-lifo` (T-09-46) | KILLED | KILLED | 2.4 | `test_sched_matrix:tracer-coincident` (`trace-order`) |
| `resource-stop-time-jump` (T-09-47) | KILLED | KILLED | 1.8 | `test_sched_matrix:resource-limit-resume` (`stop-result`) |
| `control-equivalent-handler-tick` | SURVIVED | SURVIVED | 6.6 | every case and `test_scheduler` pass |
| `control-no-compile` | INVALID | INVALID | 0.9 | the build fails |

Per-step seconds of the second run: fixtures 0.0 (not built, Deviation 9), configure 0.4, core build 1.4, test compile 1.1, baseline 5.0 (13 checks), mutants as above; about 37 s in all. Recorded secondary results: K5-1 and K5-5 also fail `core/test_si` case `latch-strictly-after`; K5-2 also fails `core/test_save` case `eeprom-timing`. Each mutant compiled under the gate flags with Werror in the worktree and the baseline passed in the same worktree first; `git worktree list` shows the main checkout only.

**Survivor-fixed record.** No mutant survived. One strengthening happened before the first run: the completion-from-budget mutant is equivalent at the canonical budget 4,096 and converges after the read completes, so part a of `resource-limit-resume` compares the budget runs at seven checkpoints inside the read (S, E - 1, E, E + 100, C - 1, C and the horizon) rather than only at the horizon. Without that the mutant would have survived the horizon comparison.

**Anchors authored (committed source, line at authoring).**

| Mutant | File and line | Anchor (single line unless stated) |
| --- | --- | --- |
| K5-1 | `core/t64_si.c:72` | `latch->next_visible_from = tick == UINT64_MAX ? UINT64_MAX : tick + 1u;` replaced by `machine->tick` |
| K5-2 | `core/t64_save.c:147` | `machine->save.busy_until = tick + T64_SAVE_EEPROM_BUSY_TICKS;` replaced by `machine->si.start_tick + ...` |
| K5-3 | `core/t64_scheduler.c:192` and `core/t64_cpu.c:1554` | the two-line pair `expected_queue_count != queue_count ||` and `event.sequence != next_sequence` (ends `||` in the scheduler, `) {` in the CPU) |
| K5-4 | `core/t64_si.c:173` | `complete_delay = handshake_ticks + T64_SI_READ_TAIL_TICKS;` replaced by `... + machine->profile.advance_event_budget;` |
| K5-5 | `core/t64_si.c:87` | the six-line tail of `t64_si_latch_sample` from `latch_promote(...)` to the closing brace, reordered |

## Producers without an in-repo MI builder

None is missing a builder. One producer does not exist: **SP DMA** never sets an MI bit (its completion handler does not call `t64_mi_set_pending`; the only SP-related bits are the RSP break with interrupt-on-break and a host or CPU write of the set-interrupt command to SP_STATUS, which is a bus write, not an event). The row `mi.sp-dma` therefore asserts the absence on the skeleton profile: the completion record is dispatched and no MI record or pending bit follows.

## CPU step interval and its source

The CPU step interval is 1 guest tick for every non-multiply, non-divide opcode: `T64_CPU_BASE_ISSUE_INTERVAL_TICKS` is `UINT64_C(1)` at `core/t64_internal.h:15`, and `t64_cpu_instruction_cost` (`core/t64_cpu.c:1454`) returns it for any opcode outside the multiply and divide function codes (`core/t64_cpu.c:1460` and `1479`). The expected exception tick of `mi.cpu-sampling` is the SI completion tick (4,096) when the producer was sequenced before the CPU step and the completion tick plus 1 (4,097) when the step was sequenced first.

## F1 record

- **Extraction command** (run under `sh`; zsh does not word-split the result): `awk '/^struct t64_machine \{/{f=1;next} f&&/^\};/{exit} f&&!/^[[:space:]]*\/\*/{s=$0; sub(/;.*/,"",s); sub(/\[.*/,"",s); n=split(s,a,/[ *]+/); if (a[n] != "") print a[n]}' core/t64_internal.h`. Member count **67**; every member has exactly one audit row.
- **Disagreement between the spec section table and the walker:** none. The stem id emits markers 1 to 10 and the three save ids 1 to 11, as 09-04 settled.
- **Hand-derived lengths** (2 to 7, 10, 11): 32; 4,194,308; 8,200; 276; 16 + 40 per queued event (56 idle, 96 at the end of W-ALL); 43,724; 404; 20 + storage (532, 2,068, 32,788). Sections 8 and 9 are captured (2,103,016 and 336 idle; 2,103,112 and 344 at the end of W-ALL) and cross-checked against the v24 stream.
- **Evidence limit.** The section hashes are regression pins (the walker compared with itself); the lengths of 1 to 7, 10 and 11 are independent of the capture; 8 and 9 rest on the v24 differential; nothing is hardware evidence. The hosted jobs of 09-11 add a second platform.

## Measurements

| Block | Seconds |
| --- | --- |
| `unit.core.sched.matrix`, native (gate preset) | 4.6 to 4.9 |
| same, ASan, at Task 2 / Task 3 / Task 6 | 71.5 / 77.7 / 85.0 |
| Task 6 verify block 1 (build, six ctest names, case checker, spec cross-checks, lint, scanners) | 28 |
| gcc:13 `-Werror` closure over `core/tests/test_sched_matrix.c` | 1.7 (no warning) |
| `sh scripts/check-test-inventory.sh --selftest` | 20.5 (`removal_controls=438 malformed_controls=5`) |
| mutant procedure | about 37 |
| **full local `sh scripts/gate.sh`** | **99 (PASS, 218 of 218)**; ctest 49, test-inventory-selftest 21, boot-evidence-selftest 13, core-lint 9 |

Inventory: gate 218, ASan 220. **Cumulative Phase 9 gate names: 39** (09-01 10, 09-02 14, 09-03 7, 09-03.1 1, 09-04 3, 09-05 2, this plan 2: `unit.core.sched.matrix` and `repo.sched-k5-anchors`); the Phase 9 total stays at 45, so six remain for 09-07 to 09-11. The VALIDATION names `unit.core.sched.cross-device`, `.partition`, `.interleave`, `.resource-limit-resume` and `.mi-producers` and the proposal's `.required-mutants` are cases of the suite and of the driver. The checks run on `unit.core.sched.matrix` are 1.7 million natively because the schedule driver counts one check per resumed call.

## Mapping of the validation names

`unit.core.sched.cross-device` = case `cross-device`; `.partition` = `partition`; `.interleave` = `interleave`; `.resource-limit-resume` = `resource-limit-resume`; `.mi-producers` = `mi-producers`; the required-mutants row = `python3 -B scripts/k5-mutants.py --run` with `repo.sched-k5-anchors` keeping the anchors current.

## Deviations from Plan

**1. [Plan text - profile] tie.pi-sp runs on the skeleton profile.** The plan says the stem id. On the RSP-functional profiles `start_dma` returns `start_rsp_dma` before it schedules the legacy completion, so SP DMA has no 4/1 event there (`core/t64_sp.c`); the 09-01 pair also uses the skeleton. No construction was found for the second insertion order with a PI completion and an RSP issue event (the issue chain re-reserves one tick ahead, so the issue event for a tick is sequenced after anything reserved earlier); this was reasoned from `core/t64_sp.c` and `core/t64_rsp.c`, not exercised. The row keeps its label, both insertion orders and the literal tick 13 (1 + 24 / 2). Consequence: the W-ALL census on the stem observes nine rows (SP only as 4/2); the legacy 4/1 row is observed by the skeleton row. Files: `core/tests/test_sched_matrix.c`.

**2. [Plan text - sequence] tie.cpu-si-store-sequence has one insertion order and a budget stop.** The store itself commits the SI execute event after the CPU step that performs it, so there is no second order. "Advance to S" would also dispatch the SI execute event (tick S, sequence q + 1) in the same advance, so the case reads q at S - 1 and edits the in-memory event budget to 1 (N6) so the advance stops right after the store step; the queue then holds execute q + 1, successor q + 3 and complete q + 2 with next_sequence q + 4. The generic rows (whole, S - 1, S, S + 1) still run.

**3. [Rule 1 - builder defect, plan wording] W-ALL uses v_sync 16, not 8.** The video queue holds two packets and nothing drains it; the third field boundary at tick 34,016 stalled the VI with no progress and the resume loop reported `resume-liveness`. Fields recur (the plan expected one VI field in H_ALL). With v_sync 16 the third boundary is at line 48, tick 68,034, after H_ALL. The tracer pair keeps v_sync 8.

**4. [Plan text - program] W-ALL has its own CPU program.** The plan's six-opcode program stores to RD64B at tick 4, before the staged WR64B completes at 2,500. W-ALL uses a 1,000-iteration delay loop so the store lands at S = 3,008; the six-opcode program is used for tie.cpu-si-store-sequence (store at tick 4, calibrated).

**5. [Plan text - producer] mi.sp-dma records an absence** (see the producers section). The plan expected a producer record followed by an MI record.

**6. [Plan text - census] The census scans the queue.** Dispatch records and device trace records share (device, kind) pairs (for example PI start and PI completion are both 3/1), so a trace-pair census would also pass on a start record. The census observes the queue before each one-tick call, which is what the 09-01 ledger test does, and still drains the trace after every call with zero drops.

**7. [Rule 2 - discriminating power] Budget runs are compared at seven checkpoints** (see the mutant section), so K5-4 bites.

**8. [Plan text - name] The K5-2 check is `core/test_save` case `eeprom-busy`.** No CTest name `unit.core.save.eeprom-busy-triple` exists; the 09-04 cases are in the `save-device` suite (`unit.core.save.device`).

**9. [Plan text - fixtures] The mutant worktree does not build `t64-boot-fixtures`.** The producer needs the pinned MIPS toolchain under the main tree's ignored `build/tools`; a fresh worktree would rebuild Binutils. The driver reuses the main tree asset after checking its SHA-256 against `EXPECTED_ASSET_SHA256` in `scripts/build-boot-fixtures.sh`, builds `t64_core` and `t64_sha256` with the gate preset and compiles the test sources with `cc`, the gate warning flags and `-Werror` (as 09-03 to 09-05 did); an anchor-check clause keeps the flag list in sync with `t64_target_warnings`.

**10. [Plan text - frozen list] `runner/CMakeLists.txt` changed.** Task 4 names it in `files_modified` for the `repo.sched-k5-anchors` registration, while the plan-level exit check lists `runner/` as untouched. No runner source changed (`git diff --name-only 503d95d..HEAD -- runner/t64_*.c` prints nothing).

**11. [Rule 3 - verify command] Direct runs find the asset.** The plan's verify commands run `core/test_sched_matrix --print-sections` from the repository root with no environment; the file now falls back to `build/fixtures/ipl3_compat.z64` when `T64_TEST_IPL3_PATH` is unset (the same fix as 09-05, commit `b030e55`).

**12. [Plan text - control] The equivalence control expands to every case the binary lists** at run time rather than a fixed list of nine, so the control kept working between Tasks 4 and 5.

**Total deviations:** 12. **Impact:** no guard weakened and no frozen file logic-edited: `git diff --name-only 503d95d..HEAD` lists only `core/tests/test_sched_matrix.c`, `core/CMakeLists.txt`, `runner/CMakeLists.txt`, `scripts/k5-mutants.py`, `scripts/check-test-inventory.sh` (two added name rows, no deleted line), `docs/spec/phase09-cases.json` and `docs/spec/machine-contract.md`.

## Assumptions of this plan

1. Section 11 exists only for the three save ids (D-27, settled by 09-04): the F1 literals and the walker agree.
2. The mutant anchors M1, M2, M4 and M5 were authored from the committed 09-03 and 09-04 source and are unique single occurrences; `repo.sched-k5-anchors` retains them.
3. `scripts/k5-mutants.py` is at exactly the path 09-03.1 declared in `PENDING_SOURCE_PATHS`; it was not renamed and no qualifier edit was made.
4. The ordering results are regression behavior of a stated software-profile policy and self-comparisons: a consistently wrong tie would pass.

## Handoff

- **09-07 (replay):** checkpoint goldens are taken after F1; the F1 table in `docs/spec/machine-contract.md` freezes the canonical content they hash.
- **09-08 (container):** whether `write_generation` and `output_drain_cursor` travel in the observer-continuity section 16 is still open; the audit marks the drain cursor observer-continuity and `write_generation` observer-only.
- **09-09 and 09-09.1 (state import):** the D-36 row "state import at a mid-tick stop" is carried by 09-09.1 on production events. Its construction sources are part b of `resource-limit-resume` (`build_midtick`: SI complete, five instruction, three frame events, a PI completion and a trailing no-op at one tick, counter stops 1 to 5 and 1 to 3) and the W-ALL workload of case `partition` (`build_w_all`); the synthetic-event recipe is refused by the closed handler table and stays a refused control.
- **09-10 (review):** cite `build/gate/09-06-k5-results.keep.txt` and rerun `python3 -B scripts/k5-mutants.py --run` (about 40 s, needs a clean tracked tree and the built asset).
- **09-11 (qualification):** the 85 s ASan time of the matrix is the one number to watch on a hosted runner; the F1 goldens add their first second-platform check there.

## CI/CD efficiency notes (AUTONOMY section 2)

- **Cheap checks that paid off:** the plan's own verify command text caught that `--print-sections` needs the asset when run directly (Deviation 11); the anchor check (0.05 s) is registered so a refactor cannot retire a mutant without the gate noticing; `ctest -N` per cited name confirmed all thirteen map rows before the spec relied on them.
- **ASan cost:** the matrix is 85 s under ASan against 4.6 s native because W-ALL runs about twenty 60,000-tick schedules; the budget-1 sweep alone is 60,000 resumed calls. If hosted ASan time matters, the 4,096-tick and 7-tick save-id runs and the budget-2 run are the first to share.
- **Mutant driver:** 37 s for nine rows by reusing the main tree asset and building only the core library; a fixture build in the worktree would have cost a Binutils build.
- **Tooling:** a safety check refused a compound background command containing `rm -f` of a stale marker file; use a fresh marker name instead of removing one. zsh does not word-split a command substitution, so the member-audit loop must run under `sh -c`.
- **Gate:** the full local gate is 99 s; the 21 s test-inventory selftest and the 13 s boot-evidence selftest are its largest fixed costs after ctest.

## Authentication gates

None.

## Known Stubs

None. No placeholder text exists in the created or modified files; the empty golden placeholder of the first F1 draft was replaced by the 58 captured rows before the commit.

## Threat Flags

None beyond the plan's register (T-09-46 to T-09-49): the new surface is a developer script that applies text edits in a throwaway worktree under `build/` and removes it, with a clean-tree precondition, unique-anchor checks and a worktree cleanliness check after every mutant.

## Self-Check: PASSED

- `git ls-files` lists `core/tests/test_sched_matrix.c` and `scripts/k5-mutants.py`; all seven commit hashes above are ancestors of HEAD; `git worktree list` shows the main checkout only; no `__pycache__` remains outside `build/`.
- `git diff --name-only 503d95d..HEAD -- core/t64_*.c core/include scripts/check-rsp-no-hle.py scripts/tests/test_rsp_no_hle.py` prints nothing; `python3 -B scripts/check-rsp-no-hle.py` is green on `build/gate` and `build/asan`; the qualifier, `ci.yml`, `check-ci-yml.sh`, `gate.sh`, `rights-scan.sh` and `tools/rights` were not modified.
- No ROM, save, PIF or IPL image, 64DD media, hardware measurement or oracle was used; the only external input is the already cleared openIPL3 asset reached through `T64_TEST_IPL3_PATH` or its fixed relative path, and every state, frame and input is authored in the test. No blob, state file or replay is committed; the F1 goldens are per-section hashes. `sh scripts/pii-scan.sh --tree` and `sh scripts/rights-scan.sh --tree` report no finding. No push was made.
