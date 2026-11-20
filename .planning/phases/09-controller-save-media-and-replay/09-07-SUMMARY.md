---
phase: 09-controller-save-media-and-replay
plan: "07"
subsystem: input-replay
tags: [core-05, t64-replay, replay-report, recorder, player, identity-binding, replay-mix, independent-oracle, goldens, mutation-matrix]

requires:
  - phase: 09-controller-save-media-and-replay
    provides: "09-01 the four ids and the v25 digest (build_id and profile_id excluded from hashed metadata); 09-02 the private no-clobber artifact publisher, per-row cap and transcript pin, the checked child-argument helper and PHASE09_TESTS; 09-03 the SI/PIF controller, the strictly-after input latch and the pad-poll copy sources; 09-05 the save export used for the live save_init; 09-06 the F1 state content freeze"
provides:
  - "runner/t64_replay.[ch]: the closed bounded t64.replay v1 parser and canonical writer, the recorder over the real accepted input call, live identity composition, three-family identity binding with the field name, the stepwise player, the in-process driver, the closed t64.replay-report v1 writer and checker and a private-file publication helper"
  - "runner record and play: supervisor rows (--replay-record, --replay-checkpoint-interval, --replay-play, --replay-play-size, --replay-play-sha256, --replay-report), a verified replay snapshot, child forwarding through the checked helper (slot sum +8), op-aligned stops, binding before the first advance"
  - "the authored replay-mix ROM, its independent oracle and an independently authored t64.input script (72-byte transcript, oracle value equals shasum)"
  - "build-independent goldens (ops and checkpoint digests only) for the scripted pre-queue and the just-in-time submission modes, with python-derived canonical identity literals"
  - "two gate names: unit.replay.suite (nine in-process cases) and runner.replay-mix (six real supervised cases)"
  - "docs/spec/replay.md"
affects: [09-08, 09-09, 09-09.1, 09-10, 09-11]

actuals:
  tokens: 71300
  tasks: 4
  commits: 12
plan_head_before: b107b40be2f2128f342e3ab97c332ff179b1a3d9
plan_head_after: a3818955e3c0dc98c13f82642c7a65b85ffc93d1

tech-stack:
  added: []
  patterns:
    - "one authored ROM, one independently authored input script, one oracle outside the ROM; expected ops are literals from the script and checkpoint digests are regression pins from a reference path that does not link the module under test"
    - "canonical identity literals derived with a separate script (json.dumps over the same literals) and compared with the C encoder output"
    - "out-of-tree mutants: throwaway worktree, -Werror build, accepted baseline first, failure required at a named assertion"
    - "named assertions (check_named) so a mutant driver can prove which comparator it reached"
    - "op-aligned loop targets: min(slice, next replay op) with the player stepping only at op ticks and the terminal stop"

key-files:
  created:
    - runner/t64_replay.h
    - runner/t64_replay.c
    - runner/tests/test_replay.c
    - runner/tests/replay_mix_golden.h
    - fixtures/replay/mix.S
    - fixtures/replay/mix.ld
    - fixtures/replay/mix_oracle.py
    - fixtures/replay/mix.input.json
    - scripts/build-replay-fixture.sh
    - docs/spec/replay.md
  modified:
    - runner/t64_real.c
    - runner/t64_supervisor.c
    - runner/t64_real_fixture.c
    - runner/CMakeLists.txt
    - core/CMakeLists.txt
    - fixtures/manifest.json
    - scripts/check-test-inventory.sh
    - docs/spec/phase09-cases.json

key-decisions:
  - "Record runs the ordinary result and publishes the replay only when the loop ended at guest-completion or guest-bound; play runs the same row with an empty t64.input schedule and publishes the report in every ending"
  - "A mismatch before the guest starts is the existing prelaunch refusal; a divergence or driver refusal after the guest started is the existing harness-error host-error record; no t64.result key or detail value is added"
  - "The checkpoint interval is validated against the runner slice of 16 ticks (a positive multiple, not above the bound); the loop target is the minimum of the slice target and the next replay op, so the advance step of the test twin is free"
  - "The report publisher is opened before the first advance (record and play) so a bad leaf or directory ends at tick 0"
  - "The replay publication helper lives in the replay module (the key link names the publisher) and test_replay links the publisher source"

requirements-completed: []

coverage:
  - id: R1
    description: "The authored replay-mix ROM runs on the stem id through the real supervised runner from an independently authored script, prints the 72-byte transcript equal to the oracle value, and its recorded t64.replay equals the independent literal ops; the same artifact replays to a match report (also at advance step 7)"
    requirement: "CORE-05"
    verification:
      - kind: integration
        ref: "runner.replay-mix (case record-play-match)"
        status: pass
    human_judgment: false
  - id: R2
    description: "The t64.replay and t64.replay-report parsers are closed and capped before allocation; 62 mutation rows and 22 report rows each reach their own status after an accepted baseline; caps are exercised at the limit and one above through lowered limits and at the 4,097th checkpoint at the defaults; a 403-flip sweep never crashes"
    requirement: "CORE-05"
    verification:
      - kind: unit
        ref: "unit.replay.suite (cases mutation-matrix, report-record, format-roundtrip)"
        status: pass
    human_judgment: false
  - id: R3
    description: "Replay is bound to profile, build, content and save_init with separate statuses and field names before any guest advance; the three real controls refuse at guest tick 0 and name profile-digest, build-id and content-sha256"
    requirement: "CORE-05"
    verification:
      - kind: unit
        ref: "unit.replay.suite (case identity-binding)"
        status: pass
      - kind: integration
        ref: "runner.replay-mix (cases mismatch-profile, mismatch-build, mismatch-content)"
        status: pass
    human_judgment: false
  - id: R4
    description: "The submission tick matters and the driver obeys it; recorded and replayed checkpoints agree across advance steps 1, 7 and 4,096, with the trace on and with a second machine interleaved; a refused driver op ends in a closed report at the independently counted op index"
    requirement: "CORE-05"
    verification:
      - kind: unit
        ref: "unit.replay.suite (cases at-matters, partition, queue-full-divergence, independent-original-run)"
        status: pass
      - kind: integration
        ref: "runner.replay-mix (cases play-diverged, flag-combinations)"
        status: pass
    human_judgment: false
  - id: R5
    description: "Out-of-tree mutants fail their named comparators: port 1 accepted (port-1 row), recorder omits the first input (literal op comparison), driver ignores at (jit-golden-match), bind skips the build comparison (build-mismatch-refused), runner bind accepts every header (the three real mismatch controls)"
    requirement: "CORE-05"
    verification:
      - kind: integration
        ref: "scripts and commands recorded in this summary (throwaway worktrees, removed)"
        status: pass
    human_judgment: false
  - id: R6
    description: "Specification wording: evidence limits, the save_init image form verified in process only, no reserved claim word, no hardware statement"
    requirement: "CORE-05"
    verification: []
    human_judgment: true
    rationale: "Wording review against the claim boundary is a judgment; the reserved-word grep, the claims scan and the row-list equality check are automated"

duration: 42min
completed: 2026-11-20
status: complete
---

# Phase 9 Plan 07: Input replay, identity binding and the replay-mix fixture Summary

**One authored ROM, driven by an independently authored input script, is recorded through the real supervised runner into a private t64.replay whose ops equal independent literals, and the same file replays to every recorded checkpoint digest and the end digest with a match report; the parser and the report are closed and capped before allocation, replay is bound to profile, build, content and save_init with separate named refusals before any guest advance, and the submission tick is proven to matter, with no core source, qualifier, ci.yml, check script, gate, scanner or guard changed.**

## OPEN ISSUES FOR THE ORCHESTRATOR (read first)

1. **`harness.rdp-oracle-authority` fails and needs the owner-approved identity-only rebind (not applied here).** The manifest row added by this plan changes `fixtures/manifest.json` (last touched by `9b4a9b6`); the failure is `oracle-authority: fixture manifest SHA-256 changed`. Computed with the checker's own `_canonical_sha256` after reproducing the old pin (`True`), at the current manifest:

   | Field | Old | New |
   | --- | --- | --- |
   | manifest `sha256` (two places in the request) | `c1788a2b13daf3c5aa4f0fd5f28c225155fad14d519d2c4ed5b1eaa1786a3034` | `b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0` |
   | manifest `git_blob_id_sha1` (two places) | `622b4f0cca6007efb514f84b7b3e133cdec73ff0` | `f711f391aebb728c4aabb9135a49169a55de02fa` |
   | `request_sha256` and `PINNED_REQUEST_SHA256` | `6d05bd4f0a5b067c5f541d82d79994335bd00dc0bef2d7036fde53c57a8caa61` | `a4bb7774b72321261e37a44bbb518161b9b564e4beb076dfebe06881aab67733` |

   The values stay valid only while `fixtures/manifest.json` is unchanged; a later Phase 9 plan that adds a row moves them again. Neither `docs/evidence/phase07-oracle-authority.json` nor `scripts/check-rdp-oracle-authority.py` was edited, and no guard was weakened.
2. **Not pushed.** The plan forbids a push and a hosted run. Local main holds the 12 commits below plus the closeout commit. Before a push: the Linux compile pre-flight below already passed on Ubuntu 24.04 `cc` 13.3 (run without forcing the platform; the image platform notice is only a warning).
3. **ASan time of the two new names is about 86 s each when run alone** (native 5 s each; `TIMEOUT 300`). Run together with other work they took 156 s and 173 s (before the trim commit `a381895`). The replay suite is native-only by design (D-40) but both names are in the ASan CTest selection, so 09-11 should watch the ASan CTest total.
4. `requirements.mark-complete CORE-05` was not run: this plan makes no behavior claim beyond `regression-pinned` plus `unit-tested` (the claim block is 09-11), so the checkbox and traceability row are left for the orchestrator after the phase review.
5. Deviations 1 to 9 below are plan-text inconsistencies or measured corrections; none touches a frozen file.

## Performance

- **Duration:** 42 min (2026-11-20T01:24Z to 02:06Z, plan start to the last verification).
- **Commits:** 12 measured from the ledger (`b107b40..a381895`, before this summary).
- **Files:** 18 changed (6,713 insertions, 11 deletions), 10 of them created.
- **Realized cost:** about 71,300 tokens on the plan's chars/4 scale over the added diff text (285,138 characters), against the estimate of 240,000 (low confidence; over-counted).

## Commits

| Commit | Step | Subject |
| --- | --- | --- |
| `607b3ca` | Task 1 step 2 | replay-mix ROM source, independent oracle, authored input script and fixture build |
| `9b4a9b6` | Task 1 step 2 | runner row, manifest row (`source.revision` = `607b3ca`) and the rom-timing case |
| `c828dc7` | Task 1 step 3 | module API, stub, golden literals, first two RED cases |
| `9bfeecf` | Task 1 step 3 | tracer-scope parser, canonical writer, recorder, live identity, player, report writer (GREEN) |
| `e329216` | Task 1 step 4a | record and play options, verified snapshot, record header and end (RED for the op list) |
| `22c7c14` | Task 1 step 4b | record wraps the pre-queue loop and checkpoints (record GREEN) |
| `8964ca4` | Task 1 steps 4c and 4d | play parse, bind call, player stepping, report publication (play GREEN) |
| `0bdebd1` | Task 2 | caps, ordering, recorder cap refusal, closed report record, mutation and report cases |
| `e68aba5` | Task 3 | identity binding, queue-full mapping, identity-binding, at-matters, partition, queue-full cases |
| `d380d1a` | Task 3 | real controls: three mismatches, play-diverged, flag-combinations |
| `e3a72b2` | Task 4 | docs/spec/replay.md, the `replay` inventory suite, the plan case order |
| `a381895` | Task 4 | trim redundant full runs (ASan time) |

## Accomplishments

- **Fixture chain.** `fixtures/replay/mix.S` polls the controller six times with a delay loop and prints six 12-byte lines (72 bytes). `fixtures/replay/mix_oracle.py` derives the transcript from the strictly-after rule outside the ROM and cross-checks `mix.input.json`. The oracle value and the independent `shasum` of the hand-derived text agree: `b16f6575b5f94bee4018f7ef12ad0ca0e1c8134a033479b78e7d8527d20eabe2`, 72 bytes (`M1 00000000`, `M2 800001ff`, `M3 40217f80`, `M4 1804fe02`, `M5 01020040`, `M6 0c0305fb`; the oracle reproduced the hand-derived text exactly, no adjustment).
- **Module.** A closed key parser under explicit JSON limits, a canonical writer whose bytes equal the python-derived identity literals, a recorder over the real accepted call, three-family binding, a stepwise player and an in-process driver, and a report writer and checker that share one cross-field validation.
- **Runner.** Op-aligned loop targets, a checkpoint after each bound stop at a multiple of the interval, the end op at guest-completion or guest-bound, publication as one private 0600 file; play binds before the first advance, steps the player only at op ticks and at the terminal stop, and publishes the report in every ending. A divergence ends through the existing harness-error host-error record.
- **Evidence.** The reference path (events queued through the real call, inclusive bounds, digest) reproduces the golden literals at advance steps 7 and 4096 in both submission modes; the supervised end digest (`3a72b2a9...`) equals the golden end digest.

## Measured replay-mix values (not pinned by any test)

Neutral run, stem id, slices of 16 ticks, T64_MIX_SPIN 28312 (first candidate 28000 measured spacing 99,064; adjusted by 3 ticks per iteration to 100,000):

| Poll | RD64B start S | Execute tick E = S + 10,876 |
| --- | --- | --- |
| 1 | 541,429 | 552,305 |
| 2 | 641,429 | 652,305 |
| 3 | 741,429 | 752,305 |
| 4 | 841,429 | 852,305 |
| 5 | 941,429 | 952,305 |
| 6 | 1,041,429 | 1,052,305 |

Completion tick 1,141,425 (the supervised run with the script completes at the same tick). Stamps (nearest multiple of 1,000 to the gap midpoint): 602,000, 702,000, 802,000 (two rows), 902,000, 1,002,000, each at least 49,695 ticks from every E(n) against the asserted margin of 8,192. The linked completion address is 0x80000560. ROM SHA-256 `79c59149052719baa7080c22d44a21c8d924add1f45b16ced31bb9810bf37df0` (4,616 bytes), differing from every other row.

Golden identities (fixed-header documents, derived with a short script, not the C encoder): pre-queue 2,013 bytes `10d20e4aae1ce96445fbad417c63e668be4dfce56a7a9ce23bd7a649b488d06a`; just-in-time 1,822 bytes `dd7d36c15d9ef114431c84e6cc880fc3359b9e8d46d77510bdd1ee7dc920eafe`.

## RED records

Recorded before the GREEN edits (the first behavioral failure, not a setup failure):

- **Task 1 step 2 (fixture refusal).** ROM built, runner row absent: `exit=1`, record `{"detail":"refusal","fixture_id":null,"guest_stop":{"bound_tick":"1500000","kind":"refused"},"guest_tick":"0","outcome":"harness-error",...}`. With the row: `exit=0`, `outcome pass`, `detail positive-match`, 72 log bytes, transcript digest equal to the oracle value.
- **Task 1 step 3 (stub).** `FAIL line 717: status == T64_REPLAY_OK` with `format-roundtrip: encode status allocation-failed` (the canonical writer returned the stub status at the first canonical-identity assertion) and `FAIL line 870/879: record_prequeue(...)`; `case rom-timing: pass`. The reference path already reproduced the golden literals.
- **Task 1 step 4a.** `published: op count 1 want 14` and `FAIL line 1353: ops_equal(replay, want, want_count, "published")` (header and end only); the play half also failed because the play options were not yet applied (the run completed with an empty schedule and the wrong transcript).
- **Task 1 step 4c.** Not reproduced as written, see Deviation 4.
- **Task 2 (`LastTest.log` retained as `build/gate/09-07-t2-red.LastTest.log`).** Parser rows red: `event-before-at`, `at-reversed`, `end-missing`, `end-not-last` and `second-end` all `status ok` against the expected statuses; caps rows `parse_checkpoint_doc(2u, 6u, ...) == OP_LIMIT`, `(3u, 0u, ...) == CHECKPOINT_LIMIT` and `(4097u, 0u, NULL, ...) == CHECKPOINT_LIMIT` failed. Rows already satisfied by the tracer parser (closed keys, duplicate keys, ranges, port 0, header grammars, depth, strings, trailing bytes, the byte cap, caps at the limit) started green by design; the sweep reported `403 flips, 70 accepted, 333 refused`. Report: every cross-field row returned `status ok` from the pass-through checker (21 rows plus the oversize row and the round trip).
- **Task 3 (`build/gate/09-07-t3-red.LastTest.log`).** The mismatch rows were red (`profile-mismatch-refused` and `build-mismatch-refused` named; `refused`, the report shape, zero advance calls, tick 0 and the post-admission digest each failed per row, the first rows shown before the output limit), `queue-full-divergence: report_is(... QUEUE_FULL ...)` red (the tracer player reported `input-refused`). The just-in-time golden rows, `partition` and the pre-queue rows started green by design (the JIT literals existed before the stub commit because the reference path was run first, Deviation 3, and the driver already existed).

## Out-of-tree mutant controls (nothing committed, worktrees removed)

Driver `build/09-07-mutants/` (a throwaway `git worktree add --detach` of HEAD, one anchored edit, `cc` with the gate warning flags and `-Werror`, linked to the main-tree core and common libraries, accepted baseline first, `git worktree remove --force`; `git worktree list` shows the main checkout only). Final-source results:

| Mutant | Edit | Build | Failing comparator |
| --- | --- | --- | --- |
| port-1 accepted | `node_int(... "port" ..., 0, 0, ...)` to `(0, 1)` | `-Werror` 1.2 s | `row port-1: status ok ... want invalid-op offset 0` only (all other rows and cases pass) |
| recorder omits the first input | `recorder->count` not incremented for the first accepted input | `-Werror` 0.6 s | `recorded-4096: op count 13 want 14` at the literal op comparison of `independent-original-run` |
| driver ignores `at` | an input op no longer stops the player loop at a later `at` | `-Werror` 0.5 s | `FAIL assertion jit-golden-match` in `at-matters` (also the JIT partition rows) |
| bind skips the build comparison | the build id comparison made unreachable | `-Werror` 0.8 s | `FAIL assertion build-mismatch-refused` after `profile-mismatch-refused` and the baseline passed |
| runner bind accepts every header | `t64_replay_bind` returns OK first, runner binaries built out of tree and used through the supervisor | `-Werror` | the three real mismatch controls fail at the supervised refusal record, the report `result`, `field` and `live_tick` (baselines pass; `play-diverged` and `flag-combinations` still pass) |

The per-run seconds were 3.6 to 5.0 for the unit mutants.

## Parser memory (T-09-51)

`python3` generated a 49,000-op synthetic document of 3,626,654 bytes (minimal canonical input ops, the largest that fits the 4,194,304 byte cap) and the parser (gate build, `-O2`) was run once under `/usr/bin/time -l`: **peak resident size 43,220,992 bytes** (a 2-op document of the same header: 1,572,864 bytes). The node array is bounded by the derived node limit (8 per op plus 64) before allocation, so no unbounded growth exists. The default op cap of 65,536 is not reachable with input ops at the default byte cap (a minimal canonical input op is about 75 bytes; 55,900 would fit at best), which is why the cap rows run through lowered limits on the same code path and the default constants are asserted by value.

## Checks and measured seconds

| Block | Result | Seconds |
| --- | --- | --- |
| Step 1 preflight: gate build, N0 regression batch (38 names), `check-rsp-no-hle.py`, inventory | 38 of 38, guard clean, count 218 | 4.1 (batch), 0.2, 0.8 |
| Task 1 verify block 1 (build, 2 names, regression 38, guard, inventory, oracle, scanners), final source | all pass | 12.3 |
| Task 3 selection (8 names) | 8 of 8 | 4.3 |
| Regression batch against N0 at the end | 38 of 38 | 5.1 |
| `unit.replay.suite` and `runner.replay-mix`, native | pass | 5.3 and 5.2 (before the trim 6.1 and 6.5) |
| Same, ASan, serial, final | pass | 87.1 and 85.5 |
| Task 4 case checker (`--suite replay --run`) | validated, nonempty | 10.5 |
| `check-claims.sh --files docs/claims/phase09.md` | `claims: files=1 blocks=0 errors=0` | 0.02 |
| `tools/lint/core-lint.sh` | `files=97 violations=0` | 14.1 |
| gcc:13 `-Werror` closure over `t64_replay.c`, `t64_real.c`, `t64_supervisor.c`, `t64_real_fixture.c`, `tests/test_replay.c` | compiled, no warning | 15.7 |
| Linux pre-flight (Ubuntu 24.04 `cc` 13.3, `-O2 -Werror`, six units, plus the two testing twins, then code generation for four units) | all compiled | 2.6 and 6.6 |
| `check-test-inventory.sh --selftest` | PASS `removal_controls=442 malformed_controls=5` | 31.4 (run next to an ASan build; the 09-06 figure was 20.5) |
| inventory gate / asan | PASS counts 220 / 222 | under 1 |
| `gov.manifest.*` and `gov.rights.selftest` (5 names) | pass | 8.3 |
| `check-rsp-no-hle.py` on `build/gate` and `build/asan` | clean, no pin refresh | 0.2 each |
| rights scanner `--staging` on a copy of a real published `mix.rpl` | `RULE R-DENIED-EXT mix.rpl`, exit 1; the copy was deleted | under 1 |

The full `sh scripts/gate.sh` was not run (D-40: full gate only at the end of 09-03, 09-06 and 09-11).

**K13 count.** This plan adds exactly 2 gate names (`unit.replay.suite`, `runner.replay-mix`). Cumulative Phase 9 gate names: 39 (through 09-06) plus 2 = **41** of the K13 budget of about 45, leaving four for 09-08 to 09-11.

**Mapping note for 09-VALIDATION rows 9-V23 and 9-V24** (K13 consolidation): `unit.replay.format`, `.mutation`, `.partition`, `.at-matters`, `.independent-original-run` and `.queue-full-divergence` are the cases `format-roundtrip`, `mutation-matrix`, `partition`, `at-matters`, `independent-original-run` and `queue-full-divergence` of `unit.replay.suite`; `runner.replay-mix` is the suite `real`; `controls.real.replay-mismatch-profile`, `-build` and `-content` are the cases `mismatch-profile`, `mismatch-build` and `mismatch-content` of `runner.replay-mix`.

## Handoff

- **Golden regeneration.** The literals in `runner/tests/replay_mix_golden.h` regenerate only through a recorded F1 defect fix (`test_replay --print-goldens --mode prequeue|jit --step N`); the canonical sizes and identities were derived separately in Python and must be recomputed the same way.
- **Native-only.** The `replay` suite is native-only by design (D-40, replay soak stays native): no hosted run line, `HOSTED_ROWS` or `hosted` suite change was made.
- **09-09 continuation digests.** The seven pre-queue checkpoint digests (ticks 160,000 to 1,120,000) and the end digest are independent regression pins from the reference path; 09-09 may reuse them as the independent earlier run for state continuation. The two submission modes have the same end digest (observed, not claimed; their checkpoint digests differ where an event is pending).
- **09-11 pins.** The row `phase09-replay-mix-si-v1` (ROM SHA-256, 4,616 bytes), the golden header literals and the oracle SHA-256 `b16f6575...eabe2` are the literals to pin; the `save_init` image form is verified in process only.
- **Frozen after this plan.** Replay schema keys, the module API and the flags stay provisional until Phase 10; a later change is a recorded fix with regenerated goldens.

## CI/CD efficiency notes (AUTONOMY section 2)

- **Cheap checks that paid off.** The python-derived canonical identity literal caught nothing but proved the C encoder equals an independent derivation before any run; the oracle ran before the ROM was measured; the unlinked-snapshot rule of the runner was found by one direct run (a refusal record) rather than a long suite; the Linux compile pre-flight and the gcc:13 closure cost 2.6 to 15.7 s and passed first time.
- **ASan cost.** Each replay run is about 1.14 million ticks of interpretation: 20 to 26 times slower under ASan. The first draft of the two suites took 156 s and 173 s (concurrent); sharing one record across the real controls, using the prefix through the first checkpoint as the same-setup baseline and merging two partition runs cut them to 87 s and 86 s serial with every row kept. If hosted ASan time matters, the next candidates are the 7-tick JIT partition run and the direct-runner baselines of the three mismatch controls.
- **Mutant driver.** Building only the changed units in a worktree and linking the main-tree libraries costs about 4 s per mutant (a CMake configure in the worktree would rebuild the fixture toolchain).
- **Tooling.** `sed -i` on macOS needs a suffix argument (the edits used python instead); a foreground `sleep` is blocked, use an until loop on a marker file.

## Deviations from Plan

**1. [Plan text - count] The root has fifteen keys, not fourteen.** The behavior list says "each of the fourteen root keys"; the key table in the same plan names fifteen. The matrix has fifteen `missing-*` rows.

**2. [Plan text - interval rule] The interval is checked against the runner slice of 16, not the advance step.** The plan says "a positive multiple of the advance step" while the test twin runs at advance step 7 with interval 160,000 (not a multiple of 7) and an interval of 17 must be refused at the production slice. The supervisor and the runner require a positive multiple of 16 not above the bound, and the loop target is the minimum of the slice target and the next replay op, so any advance step works. Files: `runner/t64_supervisor.c`, `runner/t64_real.c`.

**3. [Plan text - order] The goldens were printed before the stub commit.** Step 3b prints them after the GREEN; the reference path (no module linked) was run first so the literal file and the python-derived identity values exist in the RED commit `c828dc7`. The just-in-time rows therefore started green by design.

**4. [Plan text - staging] Steps 4c and 4d are one commit, and the altered-digest RED of 4c is not reproduced.** The player module compares every checkpoint digest from the tracer commit, so a 4c-only state that "compares only the end digest" would have required crippling the module. The case `play-diverged` (an altered checkpoint digest, a diverged report at op 8 and tick 480,000) started green by design; the comparator is shown by the unit `at-matters` rows and by the runner-side mutant above (for the mismatch controls). 4a records only the header and the end op (the wrapper is in 4b) so that the plan's "1 op" observation holds.

**5. [Plan text - supervised classification] The mismatch controls assert two records.** A runner prelaunch refusal reaches the supervisor as a record that does not carry the identities the supervisor holds, so its supervised result is harness-error, stop kind report-error, detail capture-invalid, guest tick 0 (the 09-02 rule); the plan asks for "detail refusal", which only the runner binary's own record carries. Each control asserts the supervised classification (with the published report) and the runner's own refusal record from a direct run using unlinked snapshots.

**6. [Plan text - names] The real controls were written after the unit RED/GREEN of the same comparator.** Their RED is shown by the runner-side bind mutant (all three fail at the refusal record and the report fields after accepted baselines) rather than by a pre-implementation run.

**7. [Test defect] The first recorder-cap check counted the cap wrongly and leaked a replay.** A cap of 4 admits three inputs because one slot is kept for the end op, so the refused third input did not occur; the test now uses a cap of 3, and the stray replay (a finish that succeeded in the first block) no longer leaks. Fixed before commit `0bdebd1`; no module change was needed.

**8. [Plan text - cost] The suites were trimmed for the ASan time** (see CI/CD notes); no row was removed.

**9. [Plan text - commit message] Commit `0bdebd1` says "63 rows"; the table has 62 mutation rows** (and 22 report rows). The spec and this summary state 62.

**Total deviations:** 9. **Impact:** no guard weakened and no frozen file changed: `git diff --name-only b107b40..HEAD` lists only the ten new files plus `core/CMakeLists.txt` (the fixture block only), `runner/CMakeLists.txt`, `runner/t64_real.c`, `runner/t64_supervisor.c`, `runner/t64_real_fixture.c`, `fixtures/manifest.json`, `scripts/check-test-inventory.sh` (data rows only) and `docs/spec/phase09-cases.json`; the qualifier, `ci.yml`, `check-ci-yml.sh`, `gate.sh`, the rights scanner, `tools/rights`, `check-rsp-no-hle.py`, `build-boot-fixtures.sh`, `t64_raw_capture.c`, `t64_artifact_publisher.c`, `t64_result_check.c`, `result-protocol.md` and `docs/claims/phase09.md` show no change.

## Assumptions of this plan

1. The strictly-after rule, the 10,876-tick handshake figure and the 8,192-tick stamp margin are software-profile policy literals written in the tests and the oracle; the stamps do not depend on the exact figure.
2. A completion at exactly the advance target is reported as guest-completion (observed on the real run), which the op-aligned play loop relies on.
3. The build id and the source revision of the replay header come from the same core library the test binary links, so the header composed in the test equals the runner's.

## Authentication gates

None.

## Known Stubs

None. The tracer stub of the replay module was replaced in `9bfeecf`; no placeholder text exists in the created or modified files.

## Threat Flags

None beyond the plan's register (T-09-50 to T-09-58): the new surface is two file descriptors and two leaf names on the existing runner argument list, covered by the T-09-54 and T-09-56 mitigations (private publisher, flag-combination refusals).

## Self-Check: PASSED

- `git ls-files` lists the ten new files; all twelve commit hashes above are ancestors of HEAD; `git worktree list` shows the main checkout only; no `__pycache__` remains outside `build/`.
- `git diff --name-only b107b40..HEAD -- core` prints only `core/CMakeLists.txt` and its diff touches only the replay fixture block; `python3 -B scripts/check-rsp-no-hle.py` is green on `build/gate` and `build/asan`.
- No ROM, save, PIF or IPL image, 64DD media, hardware measurement or oracle was used; the only external input is the already cleared openIPL3 asset embedded by the authored ROM. No replay, report, save or state blob is committed; the goldens hold ops and digests only. `sh scripts/pii-scan.sh --tree` and `sh scripts/rights-scan.sh --tree` report no finding. No push was made.
