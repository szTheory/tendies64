---
phase: 10-carry-in-closure-before-the-freeze
plan: 15
subsystem: runner
tags: [run-07, core-06, composite-guest, two-run, independent-oracle, wrong-value-controls, container-v2, format-string-pairing, prelaunch-refusal, si-v2-s32k]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: the composite guest, its two manifest rows and the runner admission rows on ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k, the twin oracle fixtures/composite/composite_oracle.py (plan 10-14); state 26 export with container version word 2 and the independent decoder fixtures/state/decode_container_v2.py (plan 10-11); the preflight (plan 10-01)
provides:
  - runner.composite-two-run, controls.real.composite-wrong-rom, runner.composite-state-v2 (one binary, runner/tests/test_composite_two_run.c), harness.composite-oracle and runner.state-format-v26 (runner/tests/test_state_format_v26.c)
  - fixtures/composite/composite.expected.json, the committed independent expectation, with --write and --check modes in composite_oracle.py
  - T64_STATE_CONTAINER_FORMAT_V2 ("t64.state-container.v2") and the format string pairing: closed set {v1, v2} at the supervisor and the runner parser, profile pairing (v1 with state 25, v2 with state 26, import state 25 only) at the runner prelaunch
affects: [10-16, 10-17, 10-18, 10-19, phase-11]

actuals:
  tokens: 22395
  tasks: 4
  commits: 4

tech-stack:
  added: []
  patterns:
    - "an expectation file is committed only as the output of an independent oracle script and a CTest name (--check) proves the two equal byte for byte; a runner test reads it from an environment path and never from the build under test"
    - "a wrong-value control reuses the comparator on the same actual bytes with one expected byte changed in a copy and requires the exact changed offset as the first difference; a mutated expectation file (checked by hand for all four outputs) makes the whole case fail"
    - "a pairing refusal is attributable because it runs next to an accepted baseline of the same setup that differs only in the option under test (the v2 string on state 26, the same ROM without the state option on state 24, the same import on state 25)"
    - "a CMake helper registers literal TIMEOUT numbers only: harness.phase09-qualification scans every tracked CMakeLists.txt and refuses a TIMEOUT that is not a literal"

key-files:
  created:
    - runner/tests/test_composite_two_run.c
    - runner/tests/test_state_format_v26.c
    - fixtures/composite/composite.expected.json
  modified:
    - fixtures/composite/composite_oracle.py
    - core/include/tendies64/t64_machine.h
    - runner/t64_supervisor.c
    - runner/t64_real.c
    - runner/tests/test_state_runner.c
    - runner/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase10-regexes.txt
    - docs/spec/state-container.md

key-decisions:
  - "The pairing is one function, state_request_admitted in runner/t64_real.c, called from the prelaunch guard that replaced the old state-25-only check (the former lines 1272-1273): an export is admitted for (25, v1) and (26, v2); an import only for state 25; everything else, a state-24 profile included, is the existing prelaunch refusal at guest tick 0 with nothing published"
  - "The supervisor validator stays profile-blind: it accepts the closed set {v1, v2} from its arguments alone, and the parser of the runner accepts the same set; the profile-aware part is the runner's alone (checker B-1)"
  - "The wrong-byte ROM cannot publish a save (the supervisor exports only after a pass outcome, and the wrong ROM fails the pinned transcript), so controls.real.composite-wrong-rom compares the record the guest read back from the save device (the SR line of the captured transcript) through the same save comparator: first difference at offset 271 (0x10F), the oracle's wrong.save_first_difference"
  - "The raw-capture run has no --capture-out (that combination ended capture-invalid when tried by hand), so the transcript is compared from the save-out run; video frames are compared in manifest order after asserting that end ticks and sequences are non-decreasing and increasing, and audio packets after asserting the sequences 1 and 2"
  - "TIMEOUT 300 for runner.state-format-v26 (33.9 s under the sanitizer at load 12, six blob exports in one process) and the helper's literal 120 for the composite names (sanitizer 17.5, 6.4 and 4.5 s)"

patterns-established:
  - "The v2 string and container version 2 belong to state 26 only; a future state version adds one arm to state_request_admitted and one row to runner.state-format-v26, never a new refusal path"
  - "Runner tests for the composite pass the input triple (size and SHA-256 of fixtures/composite/composite.input.json) for the script and omit it for the empty default; a state import accepts no custom schedule at all"

requirements-completed: [RUN-07, CORE-06]

coverage:
  - id: D1
    description: "The composite guest runs through the real supervised runner twice, a raw-capture run and a save-out run as two separate invocations, and both result records report the same final state_digest (97f27027...)"
    requirement: "RUN-07"
    verification:
      - kind: integration
        ref: "ctest -R '^runner\\.composite-two-run$' (case two-run prints state_digest raw-run=97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c save-run=97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Every output equals the independent expectation: three 48-byte video frames in end_tick order, the two 16-byte audio packets in sequence order 1 then 2, the 32,768-byte save image (bytes, SHA-256 f0b7fe88... and the one nonzero range at 256) and the 188-byte transcript; the expectation file equals the output of composite_oracle.py byte for byte"
    requirement: "RUN-07"
    verification:
      - kind: integration
        ref: "ctest -R '^(runner\\.composite-two-run|harness\\.composite-oracle)$': compare video, audio, save and transcript lines; harness.composite-oracle runs python3 -B composite_oracle.py --check fixtures/composite/composite.expected.json"
        status: pass
    human_judgment: false
  - id: D3
    description: "Each output has a same-setup wrong-value control that reports the changed offset (video frame 0 offset 17, audio packet index 1 offset 5, save offset 12345, transcript offset 100), a mutated expectation file fails the case for each of the four outputs, and the wrong-byte ROM fails the transcript pin and the save comparator at offset 271"
    requirement: "RUN-07"
    verification:
      - kind: integration
        ref: "ctest -R '^(runner\\.composite-two-run|controls\\.real\\.composite-wrong-rom)$' (printed control lines); by hand: expectation copies with one changed value for video, audio, save and transcript make case two-run FAILED; oracle --check of a scratch copy with one changed byte exits 1 naming byte 1176"
        status: pass
    human_judgment: true
    rationale: "The wrong ROM never publishes a save, so its save comparison runs on the record the guest read back from the save device (the SR transcript line placed into the accepted image); whether that counts as the save comparison the plan names is for the orchestrator to confirm"
  - id: D4
    description: "RUN-07 edges: one invocation with --raw-capture-dir and the save-out options is the supervisor's usage refusal (not relaxed), a second run pair in fresh directories gives identical digest, video, audio, manifest bytes, save and transcript, and every run uses its own mkdtemp directory under a per-process scope of build/gate/10-15-evidence"
    requirement: "RUN-07"
    verification:
      - kind: integration
        ref: "case two-run prints 'exclusion: raw-capture with save-out refused as harness-error/usage at tick 0' and 'idempotency: a second run pair in fresh directories equals the first'; git diff of docs/spec/raw-capture.md, result-protocol.md and every raw-capture line of the supervisor and runner against the plan start is empty"
        status: pass
    human_judgment: false
  - id: D5
    description: "The format string pairing: the closed set {v1, v2} (any other string is usage, guest never started) and the profile pairing at the runner prelaunch; runner.state-format-v26 asserts v1-on-25 accepted, v2-on-25, v1-on-26, v1-on-24, v2-on-24 and an import on a state-26 profile refused at guest tick 0 with nothing published, and t64.state-container.v3 refused as usage, each next to an accepted baseline of the same setup"
    requirement: "CORE-06"
    verification:
      - kind: integration
        ref: "ctest -R '^(runner\\.state-format-v26|runner\\.state-export|runner\\.save-roundtrip)$' 3 of 3; RED against the pre-change runner: v2-on-25 and v2-on-24 gave the usage refusal instead of the prelaunch refusal and the v2 baseline on state 26 was refused (three cases FAILED); GREEN after"
        status: pass
    human_judgment: false
  - id: D6
    description: "Container v2 positive path on the composite: the state export with t64.state-container.v2 passes the pairing guard, the blob has container version word 2 and state word 26 (read from the file) and the independent decoder decode_container_v2.py accepts it with the run's digest, guest tick and profile digest and the ROM's SHA-256 and length"
    requirement: "CORE-06"
    verification:
      - kind: integration
        ref: "ctest -R '^runner\\.composite-state-v2$' (10,578,907 bytes at guest tick 599,357, accepted/ok, container version 2, state 26); the same case fails with the pinned decode_container.py"
        status: pass
    human_judgment: false
  - id: D7
    description: "Nothing pinned moved: fixtures/manifest.json is unchanged since the plan start (the one manifest-moving commit since the Wave 3 base is f027b05), PINNED_REQUEST_SHA256 is still e131cbe7..., the 13 oracle-bound files, decode_container.py, the Phase 9 claims and evidence, phase09-cases.json and F2_PINS are unchanged, and the digest sweep against the Wave 3 base shows no difference"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "git diff --quiet aabd6d5 HEAD over the listed files; python3 -B scripts/digest-sweep.py --base 1cded8de34b0c2f5c5bea903095ffc7f1e55d6fc --head WORKTREE with four --head-extra-id: 1599 shared lines, 0 differences, 296 head-only lines; ctest --preset gate 253 of 253 (101 s); sh scripts/preflight-phase10.sh PASS (82 s, 11 post-Gate steps replayed, 229 units compiled with the act image cc)"
        status: pass
    human_judgment: false

duration: 41min
completed: 2026-11-25
status: complete
commits: 4
plan_head_before: aabd6d545290958daa9293a48db27dcc804311a5
plan_head_after: 48c872e1ce4b20f2f7d2ec7071c217f86866d9d5
---

# Phase 10 Plan 15: RUN-07 two-run evidence and the container v2 format string in the runner Summary

**The composite guest runs twice through the real supervised runner (a raw-capture run and a save-out run) with the same final state digest, every output equals an independent expectation committed from the twin oracle and each has a wrong-value control, and the runner now pairs `t64.state-container.v2` with state 26 (closed set {v1, v2} at the supervisor and the parser, profile pairing at the prelaunch guard) with every wrong pair refused at guest tick 0.**

## Performance

- **Duration:** 41 min (2026-11-25T02:44Z to 03:25Z), host 1-minute load 11 to 15 throughout, so seconds are upper bounds
- **Tasks:** 4 (Task 1 tracer, Tasks 2 to 4 auto), one commit each
- **Files:** 3 created, 9 modified (about 2,300 added lines, 89,580 characters)
- **New CTest names:** 5 (`runner.composite-two-run`, `controls.real.composite-wrong-rom`, `runner.composite-state-v2`, `harness.composite-oracle`, `runner.state-format-v26`), all labelled `phase10`; the gate preset went from 248 to 253 names, the preflight regex file gained one row per name and the row `^runner\.composite` went from 0 to 2

## Accomplishments

- **Task 1 (tracer).** Case `two-run` of `test_composite_two_run`: a raw-capture run (3 video frames, 2 audio packets, 2,449-byte bundle) and a save-out run (`composite.sra`, 32,768 bytes) of `phase10-composite-s32k` with the input triple of `composite.input.json`; both pass and both report state digest `97f27027...1267c`. The combined raw plus save-out invocation is the supervisor's usage refusal (exit 2, not started, nothing created). `⚡ Tracer verified end-to-end — expanding` (the verify command was re-run: build plus the one name, 0.26 s).
- **Task 2.** `composite_oracle.py --write FILE` and `--check FILE` (the committed file also carries a `wrong` section: the first-difference offset 271 and the wrong transcript); `fixtures/composite/composite.expected.json` (2,737 bytes) is its output and `harness.composite-oracle` proves it. The two-run case compares video, audio, save and transcript against the expectation; each has a control (video frame 0 offset 17, audio packet index 1 offset 5, save offset 12,345, transcript offset 100); a second run pair gives identical digest, video, audio, manifest bytes, save and transcript. By hand: a scratch copy of the expectation with one changed value makes the case FAIL for each of the four outputs, and `--check` on a scratch copy with one changed byte exits 1 (byte 1176). Case `wrong-rom` is registered as `controls.real.composite-wrong-rom`.
- **Task 3.** `T64_STATE_CONTAINER_FORMAT_V2` beside the v1 macro (comment: a state-format constant outside the ABI promise, not public API work); supervisor validator and runner parser accept the closed set; the former state-25-only guard of `run_real` now calls `state_request_admitted` (state 25 with v1 and 26 with v2 for export, state 25 only for import). `runner.state-format-v26` has seven cases (one accepted, five prelaunch refusals at guest tick 0, one usage refusal), each next to a same-setup baseline. RED against the pre-change runner failed on behavior (v2 string refused as usage instead of at prelaunch on states 25 and 24, and the v2 baseline on state 26 refused), GREEN after. `docs/spec/state-container.md` states the closed set, the pairing and the import-on-26 refusal.
- **Task 4.** Case `state-v2`: the composite on the si-v2 id exports 10,578,907 bytes with the v2 string at guest tick 599,357; the file has container version word 2 and state word 26 (read directly, big-endian) and `decode_container_v2.py` accepts it with the run's digest, tick, profile digest and the ROM's SHA-256 and length. TIMEOUTs set from measurement (below), then the full preflight on the clean committed tree.

## Task Commits

1. **Task 1:** two-run tracer - `fa3face` (feat)
2. **Task 2:** every output against the independent expectation, with controls - `a4fcb24` (feat)
3. **Task 3:** the v2 format string in the runner contract - `925aca4` (feat)
4. **Task 4:** container v2 positive path and TIMEOUTs by measurement - `48c872e` (feat)

**Plan metadata:** this SUMMARY and the STATE and ROADMAP updates follow in docs(10-15) commits.

## Files Created/Modified

- `runner/tests/test_composite_two_run.c` - cases `two-run`, `wrong-rom`, `state-v2` (new binary)
- `runner/tests/test_state_format_v26.c` - seven pairing cases (new binary)
- `fixtures/composite/composite.expected.json`, `composite_oracle.py` - the committed expectation and the `--write` and `--check` modes
- `core/include/tendies64/t64_machine.h` - the V2 macro and the export comment (state 25 and 26)
- `runner/t64_supervisor.c`, `runner/t64_real.c` - the closed set, the `format` member of `state_request` and `state_request_admitted`
- `runner/tests/test_state_runner.c` - one literal (see Deviations 1)
- `runner/CMakeLists.txt`, `scripts/check-test-inventory.sh`, `scripts/preflight-phase10-regexes.txt` - registrations, inventory and count rows
- `docs/spec/state-container.md` - Runner surface, the option table rows, the import surface and a container v2 bullet

## Measured seconds and TIMEOUTs (F-108)

| Name | Native (load 12) | Sanitizer (load 12) | TIMEOUT |
|------|------------------|---------------------|---------|
| runner.composite-two-run | 0.6 to 0.8 s | 17.5 s | 120 (6.9 x) |
| controls.real.composite-wrong-rom | 0.3 to 0.4 s | 6.4 s | 120 |
| runner.composite-state-v2 | 0.3 to 0.4 s | 4.5 s | 120 |
| harness.composite-oracle | 0.06 s | 0.05 s | 60 |
| runner.state-format-v26 | 1.5 to 2.1 s | 33.9 s | 300 (8.9 x) |

Sum of the five names: about 3.6 s native, 62.4 s sanitizer, inside the hosted 2 to 22 times native range (at most about 80 s hosted). The preflight timeout audit passes (106 TIMEOUTs, none too low, none unmeasured); the full gate preset ran 101 s for 253 names.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Premise correction] The Phase 9 wrong-format control of runner.state-export used the v2 string**
- **Found during:** Task 3 verify (`runner.state-export` case `export-refusals`)
- **Issue:** the plan widens the closed set to {v1, v2} and requires `runner.state-export` to stay green, but case `export-refusals` of the Phase 9 binary `test_state_runner` used `t64.state-container.v2` as its "wrong format string" and expected the supervisor's usage refusal; with v2 in the closed set the run now reaches the runner's prelaunch refusal instead.
- **Fix:** that one literal is now `t64.state-container.v3` (a string outside the set), with a comment. The case, its name and the case list are unchanged (`phase09-cases.json` is unchanged), the file is not among the 13 oracle-bound files and no pin hashes it; `harness.phase09-qualification` passes.
- **Files modified:** `runner/tests/test_state_runner.c`
- **Verification:** `runner.state-export` 1 of 1, the full gate 253 of 253
- **Commit:** `925aca4`
- **For the orchestrator:** this is an edit of a Phase 9 inventoried test file (a body, not a case list); the plan did not name it. Decide whether the Wave 3 review (plan 10-18) accepts it, otherwise the alternative is a supervisor validator that knows the profile, which checker B-1 ruled out.

**2. [Rule 1 - Premise correction] The wrong-byte ROM publishes no save image**
- **Found during:** Task 2
- **Issue:** the plan runs the wrong ROM "through the same save comparator", but the wrong ROM carries the accepted pin, fails it (`guest-fail`, `wrong-transcript`) and the runner exports a save only after a pass, so no image exists to compare.
- **Fix:** case `wrong-rom` runs the accepted ROM in the same setup first (image equal to the expectation), then the wrong ROM; it asserts the closed failure record and an empty artifact directory, compares the captured transcript with the oracle's wrong transcript, takes the 16-byte record the guest read back from the save device (the `SR` line), places it into the accepted expected image and runs the same first-difference comparator: offset 271 (expected 0x27, observed 0x26), equal to the oracle's `wrong.save_first_difference`.
- **Commit:** `a4fcb24`

**3. [Rule 3 - Blocking] The CMake helper cannot take a TIMEOUT argument**
- **Found during:** Task 2 (`harness.phase09-qualification` failed: "runner/CMakeLists.txt: a TIMEOUT is not a literal number")
- **Issue:** that Phase 9 control scans every tracked `CMakeLists.txt` and refuses a `TIMEOUT` that is not a literal number; the plan's helper mirrors `t64_phase09_runner_case`, which has a literal.
- **Fix:** the helper registers a literal 120; `runner.state-format-v26` has its own registration with a literal 300 and a comment recording the measurement.
- **Commit:** `a4fcb24`, `925aca4`

**4. [Rule 1 - Premise correction] The decoder is decode_container_v2.py and the save leaf is a .sra**
- **Found during:** Task 4 (carried correction of the dispatch)
- **Issue:** the plan names `fixtures/state/decode_container.py` (a pinned Phase 9 oracle that refuses container version 2) and the leaf `composite.sav`.
- **Fix:** `T64_STATE_DECODER` points at `decode_container_v2.py`; the same case fails with the pinned v1 decoder (checked by hand); the pinned file is unchanged. The save leaf is `composite.sra` with `--save-kind sram32k --save-format raw-v1` as 10-14 measured.
- **Commit:** `a4fcb24`, `48c872e`

**5. [Rule 2 - Added check] Stale statements corrected where the change made them false**
- **Found during:** Task 3
- **Issue:** the export comment of `t64_machine.h` said export is defined for state 25 only, and the Runner surface and import sections of `state-container.md` said the format must be the v1 string and that a profile outside state 25 is refused without naming the pair.
- **Fix:** the comment names states 25 and 26; the document states the closed set, the pairing, the import-on-26 refusal and the two CTest names.
- **Commit:** `925aca4`

**6. [Rule 1 - Design detail] Same-setup baselines inside the refusal cases**
- **Issue:** the plan lists seven cases with one pass; a refusal that has no baseline cannot be attributed to the pairing.
- **Fix:** each refusal case runs its accepted baseline first (silent apart from `CHECK`s): v1-on-25 for 25/v2 and the unknown string, the v2 export on state 26 for 26/v1, the same ROM without a state option for the two state-24 rows, the import of the same blob on state 25 (resumes at the exported tick with the exported digest) for the import row. The report still prints seven pairing lines: one accepted, five prelaunch refusals, one usage refusal.
- **Commit:** `925aca4`

**Total deviations:** 6 (4 Rule 1, 1 Rule 2, 1 Rule 3). **Impact on plan:** none on scope or evidence; deviation 1 touches a Phase 9 test body and deserves the orchestrator's explicit acceptance, deviation 2 weakens "the save comparison" of the wrong ROM to the record readback (see human_judgment on D3).

## Issues Encountered

- A raw-capture run with `--capture-out` ended `capture-invalid` when tried by hand (no observation file descriptors in the real mode), so the transcript is compared from the save-out run, not from the raw run.
- A first draft of `run_pair` returned the cumulative failure count, which made a second `CHECK(false)` follow every earlier failure; it now compares against the count at its start. Found by mutating the expectation file.
- `runner.state-export` is slow under the sanitizer (134.6 s measured at load 12, TIMEOUT 480) and `runner.save-roundtrip` 92.8 s (TIMEOUT 300); both are unchanged by this plan.

## Authentication Gates

None.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None. No placeholder values: every expected byte comes from the oracle script and every actual byte from a real run.

## Threat Flags

None new. T-10-23 mitigated (closed set at both layers, prelaunch pairing, seven cases with baselines, RED then GREEN). T-10-36 mitigated (`harness.composite-oracle` proves the committed expectation equals the oracle; controls reach the comparator; mutated expectation copies fail the case). T-10-37 mitigated (every run in a private mkdtemp directory under a per-process scope of `build/gate/10-15-evidence`, removed at exit; the PII scans have findings 0; no workstation path in the plan diff). T-10-38 mitigated (the combined invocation is asserted refused; `git diff` of the supervisor and runner shows no raw-capture line touched).

## Next Phase Readiness

- The composite fixture is ready for the Phase 11 installed consumer: the two runs, the expectation file and the container v2 positive path exist. Plans 10-16 to 10-19 can rely on `runner.composite-*` and `runner.state-format-v26` names (labels `runner;controls;phase10`).
- Orchestrator items: (1) nothing is pushed (10-18 pushes); the local commits are `fa3face`, `a4fcb24`, `925aca4`, `48c872e`, this SUMMARY and the STATE and ROADMAP updates; (2) `requirements.ready-ids` keeps RUN-07 and CORE-06 unchecked while plans 10-16 to 10-19 declare them; (3) accept or reject deviation 1 (one literal in `runner/tests/test_state_runner.c`) and the weaker wrong-ROM comparison of deviation 2; (4) plan 10-17 can take the five-name cost from the table above and the full-gate 101 s at load 12; (5) `fixtures/manifest.json`, `PINNED_REQUEST_SHA256` (e131cbe7...), the pinned decoder and the 13 oracle-bound files are unchanged, and the digest sweep against `1cded8d` shows 0 differences; (6) a local asan build tree now exists under `build/asan` (ignored, built for the measurements above).

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-25*

## Self-Check: PASSED

The three created files and the nine modified files exist; `fa3face`, `a4fcb24`, `925aca4` and `48c872e` are ancestors of HEAD; `git rev-list --count aabd6d545290958daa9293a48db27dcc804311a5..48c872e1ce4b20f2f7d2ec7071c217f86866d9d5` is 4; the acceptance greps hold (`ctest -N -R '^runner\.composite'` lists 2 names, `^runner\.state-format-v26$` 1, `T64_STATE_CONTAINER_FORMAT_V2` appears twice in `runner/t64_supervisor.c` and four times in `runner/t64_real.c`, `t64.state-container.v2` four times in `docs/spec/state-container.md`); the plan verify commands passed on the committed tree (the five names, `runner.state-export` and `runner.save-roundtrip`, `sh scripts/check-docs.sh`, `sh scripts/check-test-inventory.sh --preset gate` and `--selftest`, the full gate 253 of 253, `sh scripts/preflight-phase10.sh` PASS).
