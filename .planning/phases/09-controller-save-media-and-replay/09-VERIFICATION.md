---
phase: 09-controller-save-media-and-replay
verified: 2026-11-20T19:55:00Z
status: passed
score: 9/9 must-haves verified
covered_files:
  - ".github/workflows/ci.yml"
  - ".planning/phases/09-controller-save-media-and-replay/09-01-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-01-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-02-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-02-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-03-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-03-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-03.1-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-03.1-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-04-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-04-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-05-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-05-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-06-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-06-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-07-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-07-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-08-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-08-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-09-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-09-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-09.1-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-09.1-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-10-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-10-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-11-PLAN.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-11-SUMMARY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-CONTEXT.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-REVIEW-FIX.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-REVIEW.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-SECURITY.md"
  - ".planning/phases/09-controller-save-media-and-replay/09-VALIDATION.md"
  - "core/CMakeLists.txt"
  - "core/include/tendies64/t64_machine.h"
  - "core/t64_ai.c"
  - "core/t64_bus.c"
  - "core/t64_cpu.c"
  - "core/t64_digest.c"
  - "core/t64_internal.h"
  - "core/t64_isviewer.c"
  - "core/t64_joybus.c"
  - "core/t64_machine.c"
  - "core/t64_observe.c"
  - "core/t64_pi.c"
  - "core/t64_rdp_raster.c"
  - "core/t64_rsp.c"
  - "core/t64_save.c"
  - "core/t64_scheduler.c"
  - "core/t64_si.c"
  - "core/t64_state.c"
  - "core/t64_warm_devices.c"
  - "core/tests/fuzz_state_import.c"
  - "core/tests/state_import_helpers.h"
  - "core/tests/test_digest_v25.c"
  - "core/tests/test_joybus.c"
  - "core/tests/test_save.c"
  - "core/tests/test_save_transfer.c"
  - "core/tests/test_sched_ledger.c"
  - "core/tests/test_sched_matrix.c"
  - "core/tests/test_si.c"
  - "core/tests/test_si_profile.c"
  - "core/tests/test_state_export.c"
  - "core/tests/test_state_import.c"
  - "docs/claims/phase09.md"
  - "docs/evidence/phase07-oracle-authority.json"
  - "docs/evidence/phase09-first-hosted-pair.json"
  - "docs/evidence/phase09-qualification.md"
  - "docs/evidence/phase09-success-criteria.md"
  - "docs/spec/machine-contract.md"
  - "docs/spec/phase09-cases.json"
  - "docs/spec/profile-membership-sites.txt"
  - "docs/spec/raw-capture.md"
  - "docs/spec/replay.md"
  - "docs/spec/result-protocol.md"
  - "docs/spec/save-media.md"
  - "docs/spec/si-pif-controller.md"
  - "docs/spec/state-container.md"
  - "docs/spec/state-field-rules.txt"
  - "docs/spec/state-mutants.txt"
  - "fixtures/manifest.json"
  - "fixtures/pad/poll.S"
  - "fixtures/pad/poll.ld"
  - "fixtures/pad/poll_oracle.py"
  - "fixtures/phase09/admit.S"
  - "fixtures/phase09/admit.ld"
  - "fixtures/replay/mix.S"
  - "fixtures/replay/mix.input.json"
  - "fixtures/replay/mix.ld"
  - "fixtures/replay/mix_oracle.py"
  - "fixtures/save/eeprom.S"
  - "fixtures/save/eeprom.ld"
  - "fixtures/save/roundtrip.S"
  - "fixtures/save/roundtrip.ld"
  - "fixtures/save/roundtrip_oracle.py"
  - "fixtures/save/save_oracle.py"
  - "fixtures/save/sram.S"
  - "fixtures/save/sram.ld"
  - "fixtures/state/decode_container.py"
  - "runner/CMakeLists.txt"
  - "runner/t64_artifact_publisher.c"
  - "runner/t64_artifact_publisher.h"
  - "runner/t64_real.c"
  - "runner/t64_real_fixture.c"
  - "runner/t64_real_fixture.h"
  - "runner/t64_real_result.h"
  - "runner/t64_replay.c"
  - "runner/t64_replay.h"
  - "runner/t64_result_check.c"
  - "runner/t64_save_flags.h"
  - "runner/t64_supervisor.c"
  - "runner/t64_verified_input.h"
  - "runner/tests/replay_mix_golden.h"
  - "runner/tests/t64_test_tree.h"
  - "runner/tests/test_artifact_publisher.c"
  - "runner/tests/test_pad_poll.c"
  - "runner/tests/test_phase09_runner.c"
  - "runner/tests/test_replay.c"
  - "runner/tests/test_save_fixtures.c"
  - "runner/tests/test_save_roundtrip.c"
  - "runner/tests/test_state_runner.c"
  - "scripts/build-pad-fixture.sh"
  - "scripts/build-phase09-admit-fixture.sh"
  - "scripts/build-replay-fixture.sh"
  - "scripts/build-save-fixtures.sh"
  - "scripts/check-ci-yml.sh"
  - "scripts/check-phase09-cases.py"
  - "scripts/check-rdp-oracle-authority.py"
  - "scripts/check-rsp-no-hle.py"
  - "scripts/check-state-field-use.py"
  - "scripts/check-test-inventory.sh"
  - "scripts/collect-boot-evidence.sh"
  - "scripts/gate.sh"
  - "scripts/k5-mutants.py"
  - "scripts/list-membership-sites.py"
  - "scripts/qualify-phase09.py"
  - "scripts/qualify-rdp-vi.py"
  - "scripts/rights-scan.sh"
  - "scripts/tests/test_phase09_qualification.py"
  - "scripts/tests/test_rsp_no_hle.py"
  - "tools/rights/t64_blobscan.c"
covered_digest: "v4:sha256:df06d988aeb648b83c9d09df1e9a74b4ed94e5c2d7cbef0d2cc2819476cc7a1c"
behavior_unverified: 0
overrides_applied: 0
re_verification:
  previous_status: passed
  previous_score: 13/13
  previous_digest: "v4:sha256:955f5003487a7aa0743cc08beef7cbe2a0f58ecde5b31950c9a5f8634db6de43 (verified HEAD 944443f; that report is in git history)"
  gaps_closed: []
  gaps_remaining: []
  regressions: []
  accepted_limitations:
    - "VL-01: the literal `python3 -B scripts/qualify-phase09.py --verify-evidence ...` could not be re-run in the detached clone (see Verification limits): the retained records bind the absolute path of the tree that produced them and the hosted verification writes temporary directories inside the retained store, so a clone cannot host it and a run against the real store was refused by the permission classifier. Replaced by an unmodified-function replay of every part that does not depend on that path plus independent reconciliation of the originals; the executor's own final-mode output is retained and agrees"
    - "VL-02: 09-VALIDATION.md still carries status draft and nyquist_compliant false; this is the final verification the earlier limitation waited for and all 25 rows resolve (36 CTest names, 71 case references, 0 unresolved), so flipping the record is bookkeeping for the phase-complete step"
recorded_deviations:
  - item: "09-11 plan truth: warm at most 540 s and cold projection at most 600 s on both hosted jobs (D-40)"
    measured: "gate-linux warm 203 s, cold projection 287 s; gate-macos warm 487 s (inside 540 s), cold projection 673 s (73 s over 600 s; 619 s at the 09-10 rehearsal)"
    status: "not met as measured on gate-macos; recorded by 09-11 as an extension of DEV-17, not repaired (no logic edit is allowed after the freeze)"
    treatment: "not scored as a goal gap: it is not a roadmap success criterion or a requirement, and DEV-17 (09-REVIEW-FIX.md lines 97 and 756, orchestrator decision) already ordered that a hosted overrun found in 09-11 is a declared deviation, not a logic repair. I did not apply an override on anyone's behalf; to make it formal add the override block in the report body"
deferred: []
advisory:
  - finding: "The K20 matrix wording rule of the claim prose is not mechanically pinned: writing 'all 16 rows' into the block 8 prose is accepted by check-claims and by the f2 suite (my mutant D4 survived); the committed prose is correct (17 rows, 4 full, 13 sampled, full set only in the native step)"
    category: other
    reason: "a later edit could drift without a failing test; the spec sentence is pinned by harness.state-field-use-audit, the claim sentence is not"
    evidence_status: "reproduced by my own mutant"
  - finding: "A cold macOS hosted run (Binutils cache miss) would exceed the 10 minute job limit by the 673 s projection; both cache entries were fresh and hit"
    category: other
    reason: "operational risk to a green main, owned by the orchestrator (levers in docs/ci-reviews/LOG.md F-111 and F-114: start unit.core.state.import first, drop the duplicated audio records step, raise timeout-minutes of gate-macos); each lever is a bound-source edit and costs a candidate cycle"
    evidence_status: "measured warm run 487 s; cold figure is a projection, never spent"
  - finding: "scripts/qualify-audio.py keeps its own 2,400 s limits: a new Phase 8 native qualification on this tree would be refused closed by its own verifier (F-106); no Phase 8 requalification is claimed"
    category: other
    reason: "carried from the first pass; owner the next Phase 8 requalification"
    evidence_status: "stated in docs/evidence/phase09-qualification.md limitations"
  - finding: "REQUIREMENTS.md still shows SAVE-01..04, CORE-02, CORE-04 and CORE-05 as pending and 09-VALIDATION.md is draft"
    category: other
    reason: "bookkeeping left to the phase-complete step by plan 09-11 and by this report"
    evidence_status: "REQUIREMENTS.md lines 31-37 and 133-139"
coincidental_reliance_items:
  - truth: "A recorded input replay reproduces the original run's digests (SC3, CORE-05)"
    reason: fixture-only
    harden: "the replay-mix guest still uses no RDP combiner or coverage state, so its equality is not evidence for the carried members (the state tests and the mutants M05 to M07 are); add an RDP-active replay checkpoint in a later phase. Carried unchanged from the first pass"
flagged_prohibitions:
  - id: "09-01-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; only authored inputs and the already cleared openIPL3 asset pinned in earlier phases"
    verification: judgment
    verdict: "not violated against the final prose and evidence (see the prohibition table)"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "09-02-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; the admit ROMs are authored and embed only the cleared openIPL3 asset"
    verification: judgment
    verdict: "not violated"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "09-03-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; the PIF is a modeled 64-byte RAM with no PIF ROM and pad-poll is authored"
    verification: "command: sh scripts/rights-scan.sh --tree plus the manifest row review"
    verdict: "not violated (rights scan findings=0 at the tag and at HEAD; manifest, oracle authority record and first hosted pair have an empty diff since 944443f)"
    authoritative: "command part yes, manifest row review no"
    flag: "unverified-prohibition: human review recommended for the manifest row review (not an acknowledgment gate)"
  - id: "09-03.1-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; every input is the authored pad-poll fixture and the cleared openIPL3 asset"
    verification: "command: sh scripts/rights-scan.sh --tree (exit 0, no P- or R- finding)"
    verdict: "not violated (findings=0)"
    authoritative: true
    flag: "none"
  - id: "09-04-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; every EEPROM and SRAM content is an authored byte pattern"
    verification: "command: sh scripts/rights-scan.sh --tree plus the manifest row review"
    verdict: "not violated (findings=0; no tracked save, state, replay, bin or elf extension)"
    authoritative: "command part yes, manifest row review no"
    flag: "unverified-prohibition: human review recommended for the manifest row review (not an acknowledgment gate)"
  - id: "09-05-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; every save image is an index-derived authored pattern"
    verification: judgment
    verdict: "not violated"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "09-06-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; no tie or timing result is presented as hardware evidence"
    verification: "command: sh scripts/rights-scan.sh --tree plus the test input review"
    verdict: "not violated (findings=0; every tie, timing, busy ceiling, block mirror and button mapping is labeled a named software-profile policy in docs/claims/phase09.md and the evidence documents; the scheduler block adds 'a consistently wrong tie caught by the regression pins' to not_claimed)"
    authoritative: "command part yes, input review no"
    flag: "unverified-prohibition: human review recommended for the input review (not an acknowledgment gate)"
  - id: "09-07-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; replay-mix is authored"
    verification: judgment
    verdict: "not violated"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "09-08-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; every machine is built from authored inputs or the cleared openIPL3 asset"
    verification: judgment
    verdict: "not violated"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "09-09-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; every state blob is produced in memory from authored inputs"
    verification: judgment
    verdict: "not violated"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "09-09.1-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; every blob and boundary state is produced in memory"
    verification: judgment
    verdict: "not violated"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "09-10-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; every blob lives only under the untracked private directory"
    verification: judgment
    verdict: "not violated (the private directory is ignored build output; git status clean; every blob of my own scratch work was produced in memory or in the scratchpad)"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "09-11-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; every input is an authored fixture or the cleared openIPL3 asset"
    verification: judgment
    verdict: "not violated (plan now executed: the qualified rows are the 15 phase09 manifest rows, all authored; the hosted pair ran the five authored pad-poll rows only)"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "P2 of every plan (09-01 to 09-11)"
    statement: "No save, state blob or replay is committed, logged or uploaded; the only hosted upload from Phase 9 is the one public manifest.json per job and the committed evidence holds digests, sizes and counts only"
    verification: judgment
    verdict: "not violated: my own re-download of both hosted archives lists only JSON members and exactly one Phase 9 member (phase09/<job>/manifest.json, 4,922 bytes); git ls-files shows no ROM, save, state, replay, bin or elf extension; rights scan and PII scan findings=0 on the tag tree and on HEAD; the claims and evidence documents hold digests, sizes, counts and public run links"
    authoritative: "command parts yes, wording review no"
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "09-11-P3 to P7"
    statement: "No absolute path, user name or email in a committed file; no API or ABI freeze implied; no guard weakened and no frozen file changes logic; no claim states hardware, oracle or commercial-software agreement and the three reserved words are absent; Phase 7 VID-04 and Plan 13 stay stopped and no release, naming or channel decision is made"
    verification: judgment
    verdict: "not violated: no /Users, /home or /private/var line in the claims, evidence or SUMMARY files and PII scan findings=0; the prose calls every format provisional until the Phase 10 freeze; the freeze check classifies the 17 frozen files as 9 identical, 2 data and 6 logic each with its FD id, and the diff from the rehearsed commit FR to the qualified commit is one test file; the reserved words appear nowhere in the claims, evidence or criteria files and no claim line starts with the project name outside a block; harness.rdp-oracle-authority passes (blocked), docs/evidence/phase07-oracle-authority.json is unchanged, no GitHub release or non-phase tag exists, AGENTS.md and .planning/config.json are unchanged and the AUTONOMY.md edit since the last verified HEAD only appends lessons"
    authoritative: "command parts yes, wording review no"
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
---

# Phase 9: Controller, Save Media and Replay Verification Report

**Phase Goal:** Timed logical input drives the standard controller, explicitly selected save devices behave with defined refusal postconditions, and machine state and input replays reproduce runs exactly
**Verified:** 2026-11-20T19:55:00Z
**Status:** passed
**Re-verification:** Yes, the confirmatory verification after plans 09-10 (fix cycle FX-38 and FX-39, DEV-15 to DEV-17, K20) and 09-11 (F2 identity freeze, ten claim blocks, exact-SHA qualification). The earlier report (passed 13 of 13, digest `v4:sha256:955f5003...`, verified HEAD `944443f`) is in git history. No new review and no new audit were run, as scoped.
**Verified source:** the tag `phase09-f2` = `d3211e957315afbb6bf83c91c49a111d0adbeb58` (detached, clean `git clone --no-local` in the scratchpad) for all code and test evidence; the shared tree at `f18faad` for the documents added after the tag (the three document files that differ are `docs/evidence/phase09-qualification.md`, `docs/evidence/phase09-success-criteria.md`, `09-11-SUMMARY.md` plus planning records; no code path differs). The digest covers 140 files: the 137 of the previous list plus `09-10-SUMMARY.md`, `09-11-SUMMARY.md` and `docs/evidence/phase09-qualification.md` (the three files added between `944443f` and HEAD); `docs/claims/phase09.md` and `09-REVIEW-FIX.md` were already on the list. Computed twice by `verification.fingerprint`, never by hand, stable.

## Verdict

The goal is achieved for the authored scope the roadmap and the claim blocks define, and nothing regressed. All four roadmap success criteria and the seven requirement ids hold; every counted denominator of the ten claim blocks reconciles with the retained native originals and with the case inventory; the claim prose obeys K20 and the claim rules; the exact-SHA qualification evidence is internally consistent and unchanged; the frozen-tree facts hold; the 13 earlier truths still hold under a full inventory run and a fresh set of my own mutants.

Two things you must know before relying on the word "passed":

1. **VL-01: I did not re-run the literal `--verify-evidence` command.** It cannot be hosted by a clone (details under Verification limits). I replayed every part of it that does not depend on the producing tree's absolute path, with the qualifier's own unmodified functions at the tag, and reconciled the originals independently. That is strong evidence, but it is not the command itself. If you need the literal replay, run the documented reproduction in the shared tree (it writes transient directories into the retained hosted store).
2. **Recorded deviation (not scored as a gap): the macOS cold projection is 673 s against 600 s.** It fails the 09-11 plan truth as written; DEV-17 pre-ordered the treatment. See Recorded deviations; the decision to act on it is yours.

## What I ran (independent of the SUMMARY and FIX records)

- A clean detached clone: `git clone --no-local <repo-root> <scratchpad>/confirm/f2`, `git checkout --detach phase09-f2` (HEAD `d3211e9`, `git status` empty before and after every stage). The only thing copied in was `build/fixtures` (90 files, read-only copy of the private authored-fixture directory, as in the first pass). Nothing was built, cleaned or written in the shared tree; a marker-based `find -newer` over its `build` directory after the replays printed nothing.
- Gate preset (Release, `-Werror`) built in the clone: 338 steps, exit 0. `ctest --preset gate -j4` with the gate-script environment: **224 of 224 passed**, 86.9 s wall. `sh scripts/check-test-inventory.sh --preset gate`: PASS, 224 (45 `PHASE09_TESTS` names, `ASAN_ONLY` holds the two pre-Phase-9 names).
- `python3 -I -B build/gate/09-10-criteria-check.py` in the clone: `criteria map ok: rows=4 named_ctest=23 claim_pairs=10 cases_listed_by=15 binaries`. My own check that all 23 named pass-evidence tests are passed testcases in both the retained normal JUnit and the retained ASan JUnit: 23 of 23.
- My validation-map resolver over `09-VALIDATION.md`: 25 rows, 36 CTest names, 71 case references, **0 unresolved**.
- In the clone: `check-claims.sh --files docs/claims/phase09.md` (`files=1 blocks=10 errors=0`), the tree scan (68 files, 14 blocks, 0 errors; HEAD 69 files), `check-docs.sh`, `rights-scan.sh --tree` and `pii-scan.sh --tree` (findings=0 at the tag and at HEAD), `check-state-field-use.py --check` (findings=0), `check-rsp-no-hle.py --build build/gate`, `check-phase09-cases.py --selftest`, `check-ci-yml.sh`, `qualify-phase09.py --selftest`, and the `--suite f2` cases (`f2-identity-pins`, `claims-final-blocks`): all exit 0.
- 11 source mutants of my own and 6 claim-prose mutants (below), each built and run against the related names in the clone only, each reverted; the clone was clean at the end.
- `gh run view 44795894664`, the run and artifact APIs, a fresh download of both hosted archives into their own scratch directories, and a replay of the qualifier's own functions over the retained originals (below).

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
| --- | --- | --- | --- |
| 1 | SC1 (SAVE-01): a timed-input script drives the standard controller over SI/PIF joybus and an authored ROM reports the expected button states at the expected guest times | VERIFIED | Retained normal LastTest section of `runner.pad-poll`: four polls (start ticks 541,467 / 562,654 / 583,846 / 605,038), `case reached: pad-poll-accepted completion_tick=626225`, Test Passed; the two hosted jobs report the same accepted row (tick 626,225, 68-byte transcript, `log_sha256` `715aa385...` equal to the claim's `oracle_revision`). My mutants M01 (latch visible at T instead of T+1) and M02 (high button byte takes the low byte) are each killed by `runner.pad-poll`, `controls.real.pad-wrong-value` and the joybus/latch unit names |
| 2 | SC2 (SAVE-02, SAVE-03, SAVE-04): EEPROM 4 and 16 Kbit and SRAM only on explicit selection, refusals with defined postconditions, byte round trip with endianness, refused import leaves the digest unchanged | VERIFIED | LastTest markers: EEPROM 4 Kbit 1,567,331, 16 Kbit 1,567,338, SRAM 539,986, round trip 540,634 (`outcome=pass`, 144 bytes), 13 refused imports and the publication rows listed in the `runner.save-roundtrip` section. M08 (busy window one tick short) is killed by `unit.core.save.device`, `unit.core.save.import-export`; M09 (generation not saturating) by `unit.core.save.import-export`, `unit.core.state.import`; M10 (partial store budget not word aligned) by `unit.core.cpu.si-store-budget` |
| 3 | SC3 (CORE-04, CORE-05): export and re-import reproduces the canonical digest exactly; a replay bound to profile, build and content hash reproduces the original run's digests | VERIFIED (reliance note carried) | `unit.core.state.export`, `unit.core.state.import`, `runner.state-export`, `harness.state-field-use-audit`, `unit.replay.suite`, `runner.replay-mix` pass in the clone, in the normal JUnit and in the ASan JUnit of the native record. M05 (importer drops the carried combine mode) and M07 (importer triangle extent check always true) are killed by `unit.core.state.import`; M06 (digest walker omits the coverage table bytes) by `unit.core.digest.v25-reference`, `unit.core.state.export`, `runner.state-export`; M04 (replay content hash not compared) by `unit.replay.suite` and `runner.replay-mix`. The replay half keeps its fixture-only reliance note (frontmatter) |
| 4 | SC4 (CORE-02): CPU, RSP, RDP, VI, AI, PI, SI and MI activity ordered by the same guest-time scheduler, coincident and partition/interleave checks, the quantum part of profile identity | VERIFIED | M03 (tie by sequence reversed) killed by `unit.core.sched.characterize-fifo`, `unit.core.scheduler`, `unit.core.sched.matrix`; M11 (quantum not part of the profile hash) killed by `unit.core.profile.quantum-identity` and `unit.core.sched.matrix`. Claim block 7 carries the regression-pin limit in `not_claimed` |
| 5 | The ten claim blocks reconcile with the retained originals and the case inventory and obey K20 and the claim rules | VERIFIED | Claims section below |
| 6 | The exact-SHA qualification evidence (native record, ASan and GCC13, full-matrix step, both hosted platform records) is unchanged, bound to `d3211e9` and was verified in final mode | VERIFIED with VL-01 | Qualification section below |
| 7 | Frozen-tree facts: no production-path change between `phase09-fr` and `phase09-f2`; the frozen files differ from `phase09-fh` only by FD-01 to FD-06 and data rows | VERIFIED | Frozen-tree section below |
| 8 | No regression of the 13 truths of the first verification | VERIFIED | Full inventory 224 of 224 in the clone; criteria map ok; 11 of 11 source mutants killed; the code-bearing changes since `944443f` are test scaffolding only (see Regression) |
| 9 | The judgment-tier prohibitions hold against the final prose and evidence and no explicit stop was bypassed | VERIFIED (soft gate; non-authoritative judgment verdicts flagged) | Prohibitions section below |

**Score:** 9/9 truths verified (0 present and behavior-unverified, 0 failed; the recorded deviation is not a truth of this table, see Recorded deviations).

### Claims (scope item 1)

**Reconciliation of every counted denominator.** Retained originals read: `build/gate/09-11-phase09-private/native-d3211e95.../normal-full-ctest-junit.xml` (224 testcases, 0 failures), the retained ASan JUnit of the shared native stage (226 testcases, 0 failures; the two extra names are exactly `fuzz.rom-admission` and `sanitizer.negative-control`), the retained normal LastTest log, and the clone's `docs/spec/phase09-cases.json` (13 suites, 40 CTest names, 159 distinct (binary, case) pairs; 164 case entries with 5 listed by two suites, which is why the record says 159).

| Block | Pair (profile id suffix / fixture set) | Counted | Reconciliation |
| ---: | --- | --- | --- |
| 1 | stem / `phase09-pad-poll` | 1 of 1 row | LastTest `runner.pad-poll`: `pad-poll-accepted completion_tick=626225`, Test Passed |
| 2 | `-e4k` / `phase09-eeprom-rw` | 1 of 1 row | `runner.save-fixtures`: `eeprom-4k-accepted completion_tick=1567331` |
| 3 | `-e16k` / `phase09-eeprom-rw` | 1 of 1 row | `eeprom-16k-accepted completion_tick=1567338`; the claim states no wrong-value row exists for it (the only wrong-value rows are `eeprom-wrong-e4k` and `sram-wrong-s32k`, plus the profile mismatch control) |
| 4 | `-s32k` / `phase09-sram-rw` | 1 of 1 row | `sram-accepted completion_tick=539986` |
| 5 | `-s32k` / `phase09-save-roundtrip` | 1 of 1 row | `roundtrip: outcome=pass ... log_bytes=144 tick=540634`; the 13 `import-refused` lines and five format pairs match the "supporting, not counted" sentence |
| 6 | stem / `phase09-save-refusals` | 2 of 2 names | `unit.core.save.device`, `unit.core.save.import-export`: passed in normal and ASan JUnit, listed by `save-device` and `save-transfer` |
| 7 | stem / `phase09-scheduler-order` | 4 of 4 names | `unit.core.sched.matrix`, `.ledger`, `.characterize-fifo`, `unit.core.profile.quantum-identity`: passed in both JUnits and listed by `sched-matrix` and `identity`; `repo.sched-k5-anchors` is correctly named supporting, not counted (registered, listed by no suite) |
| 8 | stem / `phase09-state-roundtrip` | 7 of 7 names | the three digest names, `unit.core.state.export`, `unit.core.state.import`, `runner.state-export`, `harness.state-field-use-audit`: passed in both JUnits and listed |
| 9 | stem / `phase09-replay-mix-goldens` | 1 of 1 name (regression-pinned) | `runner.replay-mix`: the six cases of the prose (record-play-match, three mismatches, play-diverged, flag-combinations) are `pass` in the LastTest section |
| 10 | stem / `phase09-replay-format` | 1 of 1 name | `unit.replay.suite` passed in both JUnits; "three families" in the prose is the spec's profile, build and content statuses |

Totals: 15 counted names and 5 counted rows, 20 counted units over ten distinct pairs (`check-claims.sh` enforces pass equals total and the pair uniqueness). The `build_id` of every block equals the native normal build id (`t64-build-v1-b58d1ab6...2de22`, `0.0.0+` prefix); the manifest digest `sha256:b75864fd...` is the computed manifest digest bound by the F2 pin case. My mutants D1 (pass 2 of total 1), D2 (tier smoke), D5 (one manifest digit) and D6 (a counted name no suite lists) each fail `check-claims.sh` or the `claims-final-blocks` case after the accepted baseline; D3 (the reserved word `supports` in the prose) fails `claims-final-blocks` (check-claims alone accepts prose outside a block, the case closes that).

**K20 and claim rules, read against the final prose** (`docs/claims/phase09.md`, `docs/evidence/phase09-qualification.md`, `docs/evidence/phase09-success-criteria.md`):
- Matrix: "17 rows, B01 to B17; B01, B07, B10 and B14 run every partition size; the other thirteen are sampled ... the full set runs only in the native step `normal-state-import-full-matrix` ... unsealed originals" (claims block 8 prose and `not_claimed`, qualification limitations). I confirmed the 17 rows `B01` to `B17` in `core/tests/test_state_import.c:3223-3275`. "All 16 rows" appears nowhere; the only "every row at every partition size" is the native step's own table row. The matrix step JUnit shows 31,988 checks against 31,035 in the sampled run of the same record (the step's own verifier requires more); it is not used as a strength figure.
- Resume mechanism only: "A resumed runner run never reports `pass` ... Only the resume mechanism and the digest equality are claimed, never a runner pass", plus `not_claimed: a runner pass for a resumed run` in block 8.
- Publication contract: a failed publish publishes nothing; a supervisor failure after the runner published can leave the leaf beside a failure record; a caller accepts a leaf only with exit 0 and a `pass` record; a same-user swap or a kill between rename and withdrawal can leave a leaf, no protection claimed.
- Superseded digests: the first hosted pair's state digests are "valid for source `2db1be4d` only; the qualifying pair supersedes them" (claims limits paragraph and evidence document).
- No raw check totals as strength: stated in the claims counting paragraph and in the limitations; no `checks=` figure appears in either document.
- Tiers: nine `unit-tested`, one `regression-pinned` (block 9); no other tier. No hardware, oracle-agreement or commercial-software claim (every such word is in a `not_claimed` or a negation); the three reserved words appear in none of the three documents; no prose line starts with the project name.
- The word "oracle" in the claims names the in-repo authored scripts (`fixtures/*/..._oracle.py`); the prose says no hardware capture, other emulator or commercial software was used.

### Qualification evidence (scope item 2)

**Hosted.** `gh run view 44795894664`: `conclusion success`, `status completed`, `headSha d3211e957315afbb6bf83c91c49a111d0adbeb58`, `event push`, attempt 1; `gate-linux` success 18:46:17 to 18:49:40 (203 s), `gate-macos` success 18:46:18 to 18:54:25 (487 s). Attempt API record `head_sha` equals X. Run 44801811117 (the docs commit D, head `928c9d8`): both jobs success. `git diff --name-only d3211e9 928c9d8` lists only `docs/ci-reviews/LOG.md`, `docs/evidence/phase09-qualification.md` and `docs/evidence/phase09-success-criteria.md`. The remote tags are `phase09-f2` = X, `phase09-fh` and `phase09-fr`.

**Artifacts, re-downloaded by me.** Artifact ids 14659700422 and 14658583889, sizes 10,883 and 10,885, API digests `sha256:edaead89...` and `sha256:2381952d...`: my downloads hash to exactly those; both lists have the same seven JSON members and nothing else; the Phase 9 member of each is one `manifest.json` of 4,922 bytes with SHA-256 `a34e9839...` (Linux) and `724bde64...` (macOS), as the evidence document states. Both have `workflow_sha` = `source_revision` = X, equal rows (the five authored pad-poll rows; accepted baseline tick 626,225; wrong ROM 626,229; replacement refused at tick 0) and equal inventory field (`snapshot_sha256` `3f490463...`); the build identities differ by design. No local path appears.

**Native record.** `native-record.json` SHA-256 `4310bc4c...` and `hosted-record.json` SHA-256 `a91c5d54...` equal the documented digests; `rehearsal: false`, `status: complete`, `absent_pending: []`, `source_sha` X, 288 bound inputs, manifest `bc627e75...`, inventory 13 suites / 40 names / 159 cases. All 30 retained step records (shared stage, 26 inventory checks, 2 source-body checks, the full-matrix step) have return code 0 and no timeout; shared stage 1,816.5 s of 4,800 s, matrix step 53.6 s of 300 s, as documented.

**Final-mode statement.** The retained `build/gate/09-11-verify.out` is the single line `qualify-phase09: complete native, ASan, GCC13, and both hosted platform records verified` and contains zero occurrences of "rehearsal". The qualifier's own record validators, run by me in the detached tag checkout, accept both records in final mode and refuse both in rehearsal mode (`native record rehearsal label differs from the requested verification`, `hosted record rehearsal label differs ...`).

**What the replay covered (qualifier functions of the tag, unmodified, read-only over the retained originals; only one membership test needed its private root pointed at the retained store, noted in VL-01):** `_source_snapshot(X, require_clean=True)` in the clean checkout equals the 288-file snapshot of both records (manifest `bc627e75...`); `_owner_metadata_snapshot()` equals both records' excluded owner metadata (AGENTS.md, `.planning/config.json`, `.planning/AUTONOMY.md` at the tag); the hosted record binds the native record by SHA-256; the tree manifests of the shared-native receipt (26 files), the normal and ASan retained trees (139 files each), and the hosted collection (5), downloads (10), combined (14) and PII input (14) trees re-hash to the recorded manifests; `_verify_collected_pair` and `_verify_hosted_originals` (API run, attempt, jobs, artifact enumeration, artifact digests, ZIP strictness, Phase 9 projection equality) accept the retained originals; `_verify_public_pair` accepts the two manifests from my own downloads; and the 30 native steps' retained command, process and stdout records equal the plan the tag's own `_native_command_plan` derives (the only mapping applied: my clone path to the shared path), including the full-matrix step's marker and its more-checks rule.

### Frozen-tree facts (scope item 2)

- `git diff --name-only phase09-fr phase09-f2 -- core runner tools .github fixtures scripts provenance tests docs/spec` lists exactly `scripts/tests/test_phase09_qualification.py` (the sanctioned test-only edit: `claims-final-blocks` and `f2-identity-pins`). Between the tag and HEAD only `.planning/ROADMAP.md`, `.planning/STATE.md`, `09-11-SUMMARY.md`, `docs/ci-reviews/LOG.md` and the two evidence documents differ; no code, script, fixture or spec path.
- `build/gate/09-10-freeze-check.py` (copied into the clone, working tree = the tag) against `phase09-fh`: `files=17 identical=9 data=2 logic_count=6`. The six logic verdicts carry FD ids: `scripts/qualify-phase09.py` FD-05, `scripts/qualify-rdp-vi.py` FD-06, `scripts/check-rsp-no-hle.py` FD-04 (with the FD-02 data literals), `scripts/tests/test_rsp_no_hle.py` FD-01, `runner/t64_artifact_publisher.c` and `.h` FD-03. Identical: `ci.yml`, `check-ci-yml.sh`, `gate.sh`, `t64_blobscan.c`, `check-phase09-cases.py`, `qualify-audio.py`, `check-audio-cases.py`, `collect-boot-evidence.sh`, `build-boot-fixtures.sh`. Data: `rights-scan.sh` (self-test cases added), `check-test-inventory.sh` (15 bare test-name rows). No FD id beyond FD-01 to FD-06.

### Regression spot check (scope item 3)

Changes in code-bearing files since the last verified HEAD `944443f`: `runner/CMakeLists.txt` (four registered TIMEOUT values raised: `repo.rsp-no-hle` 60 to 240, `harness.state-field-use-audit` 60 to 240, `runner.cpu-diagnostic` 120 to 240, `runner.state-export` 240 to 480; FX-39), `runner/tests/t64_test_tree.h` and `runner/tests/test_pad_poll.c` (`t64_test_scope_keep()`: the retained pad-poll run directories survive process exit for the hosted collector; FX-38), and `scripts/tests/test_phase09_qualification.py` (F2 pins, final claim case). No file under `core/` and no production runner source changed. The largest registered TIMEOUT is still 2,400 s (the wrapper 3,000 s, the outer limit 4,800 s, which the qualification test pins).

| Mutant (clone only, gate flags, each reverted) | Result |
| --- | --- |
| M01 SI latch visible at T instead of T+1 | KILLED: runner.pad-poll, controls.real.pad-wrong-value, unit.core.input.latch-partition |
| M02 joybus high button byte takes the low byte | KILLED: unit.core.joybus.parser, runner.pad-poll, controls.real.pad-wrong-value, unit.core.si.device |
| M03 scheduler tie by sequence reversed | KILLED: unit.core.sched.characterize-fifo, unit.core.scheduler, unit.core.sched.matrix |
| M04 replay content hash not compared | KILLED: runner.replay-mix, unit.replay.suite |
| M05 importer drops the carried combine mode (RDP section) | KILLED: unit.core.state.import |
| M06 digest walker omits the coverage table bytes | KILLED: unit.core.digest.v25-reference, unit.core.state.export, runner.state-export |
| M07 importer triangle extent check always true | KILLED: unit.core.state.import |
| M08 EEPROM busy window one tick short | KILLED: unit.core.save.import-export, unit.core.save.device |
| M09 save write generation not saturating | KILLED: unit.core.save.import-export, unit.core.state.import |
| M10 partial store budget not aligned to its word | KILLED: unit.core.cpu.si-store-budget |
| M11 quantum not part of the profile hash | KILLED: unit.core.profile.quantum-identity, unit.core.sched.matrix |
| D1 to D3, D5, D6 claim-prose mutants | caught (see Claims) |
| D4 'all 16 rows' written into block 8 prose | SURVIVES: not mechanically pinned (advisory, committed prose is correct) |

### Prohibitions and stops (scope item 4)

I judged the 13 plan-level "no ROM, save, firmware, 64DD, hardware measurement or oracle" statements, the matching "no save, state blob or replay committed or uploaded" statements and the extra 09-11 statements against the final prose and evidence. Result: none is violated. Evidence: the inputs of every claimed row are the 15 authored `phase09-` manifest rows (assembly and linker sources, in-repo Python oracle scripts and JSON input scripts; `git ls-files` shows no ROM, save, state, replay, bin or elf extension; `fixtures/` holds only `.S`, `.ld`, `.py` and `.json`); the claims and evidence state authored and unit-tested scope only, with no hardware, other-emulator, commercial-software or oracle-agreement claim and every tie, timing, busy window, block mirror and button mapping labeled a named software-profile policy; the published material is digests, sizes, counts and public run links, and the hosted upload is the one `manifest.json` per job; rights and PII scans are clean; `docs/evidence/phase07-oracle-authority.json`, `docs/evidence/phase09-first-hosted-pair.json`, `fixtures/manifest.json`, `AGENTS.md` and `.planning/config.json` have an empty diff since `944443f` and the AUTONOMY.md edit only appends lessons, so no stop text moved. Stops: no oracle was executed or copied and Phase 7 VID-04 and Plan 13 stay blocked (`harness.rdp-oracle-authority` passes in the clone); no physical hardware, owner file or paid service is involved (GitHub Actions on the owner's public repository; the AUTONOMY section 4 owner resolution of 2026-11-09 authorizes routine pushes subject to the PII, credential, private-path and rights stops, which the scans honor); no release or release channel decision (`gh release list` empty, no tag other than `phase09-fh`, `phase09-fr`, `phase09-f2`); the working name is unchanged. Per the soft-gate rule these verdicts are non-authoritative judgment verdicts and are flagged, not acknowledgment gates.

### Required Artifacts

| Artifact | Expected | Status | Details |
| --- | --- | --- | --- |
| `docs/claims/phase09.md` | ten real `t64-claim` blocks | VERIFIED | `blocks=10 errors=0`; tiers, counts, statements, `not_claimed` as above |
| `docs/evidence/phase09-qualification.md` | records bound to X | VERIFIED | every digest, id, size and time I could re-derive matches (sections above) |
| `docs/evidence/phase09-success-criteria.md` | SC 1 to 4 to named tests and blocks | VERIFIED | criteria check ok; 23 named tests passed in both JUnits |
| `scripts/tests/test_phase09_qualification.py` | F2 pins and final-claims case | VERIFIED | `--suite f2` passes in the clone and inside `harness.phase09-qualification` |
| native and hosted records (`build/gate/09-11-phase09-private`) | exact-binding originals | VERIFIED with VL-01 | digests and manifests re-hash equal |

### Key Link Verification

| From | To | Via | Status | Details |
| --- | --- | --- | --- | --- |
| claim blocks | native originals | counted names to passed JUnit testcases; rows to LastTest case markers | WIRED | reconciliation table |
| claim blocks | `fixtures/manifest.json` | `manifest_digest` | WIRED | D5 fails check-claims |
| F2 pins | manifest, runner table, oracles, golden, inventory | `f2-identity-pins` | WIRED | passes; the executor's 13 controls were not repeated, I re-ran the case |
| hosted record | native record and artifacts | SHA-256 and API digests | WIRED | re-derived above |
| success-criteria map | claim pairs and named tests | criteria check | WIRED | 10 pairs, 23 names |

### Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
| --- | --- | --- | --- | --- |
| claim counts | `pass` and `total` | retained JUnit and LastTest originals | yes (reconciled) | FLOWING |
| hosted manifest rows | state digest, guest tick, log hash | real runner on both platforms | yes (equal across platforms and equal to the claim's oracle pin) | FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| --- | --- | --- | --- |
| full inventory | `ctest --preset gate -j4` in the clone | 224 of 224 | PASS |
| criteria map | `python3 -I -B build/gate/09-10-criteria-check.py` | ok, 23 names, 10 pairs | PASS |
| validation map | resolver over 09-VALIDATION.md | 25 rows, 36 names, 71 cases, 0 unresolved | PASS |
| claims | `sh scripts/check-claims.sh --files docs/claims/phase09.md` | blocks=10 errors=0 | PASS |
| F2 pins | `test_phase09_qualification.py --suite f2` | both cases reached, OK | PASS |
| source mutants | 11 mutants | 11 killed | PASS |
| hosted | `gh run view`, artifact digests | success on X, digests equal | PASS |

### Probe Execution

No `probe-*.sh` is declared or exists for this phase. Not applicable.

### Requirements Coverage

| Requirement | Source plans | Status | Evidence |
| --- | --- | --- | --- |
| SAVE-01 | 09-02, 09-03, 09-03.1, 09-10, 09-11 | SATISFIED (authored scope, unit-tested) | truth 1; claim block 1 |
| SAVE-02 | 09-02, 09-04, 09-10, 09-11 | SATISFIED (authored scope, unit-tested) | truth 2; blocks 2 to 4 and 6 |
| SAVE-03 | 09-02, 09-05, 09-10, 09-11 | SATISFIED | truth 2; blocks 5 and 6 |
| SAVE-04 | 09-04, 09-05, 09-10, 09-11 | SATISFIED | truth 2; blocks 2 to 6 |
| CORE-02 | 09-01, 09-03, 09-03.1, 09-06, 09-10, 09-11 | SATISFIED (unit-tested; the order is also pinned as regression behavior, stated in block 7) | truth 4; block 7 |
| CORE-04 | 09-01, 09-02, 09-08, 09-09, 09-09.1, 09-10, 09-11 | SATISFIED (unit-tested; same-build, authored machines, the four family ids and fixtures) | truth 3; block 8 |
| CORE-05 | 09-02, 09-07, 09-10, 09-11 | SATISFIED (authored scope, reliance note) | truth 3; blocks 9 (regression-pinned) and 10 |

`.planning/REQUIREMENTS.md` maps exactly these seven ids to Phase 9; none is orphaned; every id is claimed by a plan. The checkboxes and the Pending rows are still unchecked: bookkeeping left to the phase-complete step.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| --- | --- | --- | --- | --- |
| none in the changed code-bearing files (`runner/CMakeLists.txt`, `runner/tests/t64_test_tree.h`, `runner/tests/test_pad_poll.c`, `scripts/tests/test_phase09_qualification.py`) | | TBD, FIXME, XXX, stub | | none; the first-pass debt-marker scan of the unchanged files stands |

Stubs: none on a data or rendering path.

### Human Verification Required

None. The remaining decisions (the macOS cold-time lever, the final bookkeeping) are routine reversible ones under `.planning/AUTONOMY.md` section 2; no rights, authority, physical, paid, publication or destructive stop is crossed. The prohibition verdicts are reported under the autonomous soft gate and are not an acknowledgment gate. No human validation is recorded or implied by this report.

## Recorded deviations (orchestrator-owned, not scored as goal gaps)

**macOS cold projection 673 s against 600 s (extension of DEV-17).** The 09-11 plan truth "warm at most 540 s and cold projection at most 600 s on both jobs" is met on gate-linux (203 s warm, 287 s cold) and met warm on gate-macos (487 s, margin 53 s) but not cold on gate-macos (673 s, 73 s over; 619 s at the rehearsal). By the strict rubric that plan-level truth is not met as measured. I did not treat it as a gap because it is not a roadmap criterion or requirement, the cold figure is a projection (warm plus the 186 s Binutils build; never spent) that matters only on a cache miss, both cache entries were fresh and hit, and `09-REVIEW-FIX.md` DEV-17 (lines 97 and 756) records the orchestrator's decision that a hosted overrun found in 09-11 is a declared deviation and not a logic repair. The same macOS job ran 349, 420, 433, 435 and 487 s on near-identical code, so the spread is as large as the overrun (LOG F-111, F-114). The risk is operational: a cold macOS run is cancelled at the 10 minute limit and would turn `main` red until the key or the limit changes. I did not apply an override because acceptance is yours, not mine. To record it formally, add to the frontmatter:

```yaml
overrides:
  - must_have: "warm at most 540 s and cold projection at most 600 s on both hosted jobs (D-40)"
    reason: "gate-macos cold projection 673 s (warm 487 s); DEV-17 decision: a hosted overrun found in 09-11 is a declared deviation, not a logic repair; cache entries fresh and hit"
    accepted_by: "{orchestrator or owner}"
    accepted_at: "{ISO timestamp}"
```

## Verification limits

**VL-01: the literal `--verify-evidence` replay.** The task asked for `python3 -B scripts/qualify-phase09.py --verify-evidence ...` in a detached clone of `phase09-f2`. I ran it that way and it refused at once (`explicit hosted record is outside the private qualification tree`), because the verifier requires the records under the checkout's own `build/gate/09-11-phase09-private`. The records cannot simply be copied there: they bind the absolute path of the producing tree (`output_path`, `build_dir`, `collection_path`, `downloads_path`, the recorded argv and the hosted record's own paths), the verifier re-derives the suite summaries from that tree's `build/gate` and `build/asan`, and `_verify_hosted_originals` extracts the archives into a temporary directory inside the retained hosted store (`tempfile.TemporaryDirectory(dir=private_root)`). A clone therefore cannot host the literal run, and any run against the real store writes there. I tried a symlink from the clone's `build` to the shared `build`; the permission classifier denied it as a risk to the shared build artifacts, and I did not work around the denial. What I did instead is listed in the Qualification section: every part of the verifier that does not depend on that path, executed through the tag's own unmodified functions (with temporary directories in my scratchpad, one membership test pointed at the retained private root, and the clone-to-shared path mapping for the argv comparison), plus independent reconciliation of the originals. Not replayed: the regeneration of the native suite summaries from the live `build/gate` and `build/asan`, the shared native receipt and GCC13 binding (`_verify_rdp_native_receipt`), and the collector, PII and rights command-record equality; their inputs re-hash to the recorded manifests and the executor's retained final-mode output agrees. If you want the literal replay, run the reproduction of `docs/evidence/phase09-qualification.md` in the shared tree (`git switch --detach phase09-f2`, the command, `git switch main`); it needs the tree at X and writes transient directories into the retained hosted store.

## Observations (evidence-backed, none a gap)

1. **Replay-mix reliance** (carried): the replay-mix guest uses no RDP combiner or coverage state; the replay equality is not evidence for the carried RDP members, the state tests and M05 to M07 are. Add an RDP-active replay checkpoint in a later phase.
2. **N1 accept-equivalent survivor** (carried from the first pass; the importer source is unchanged): the first row-range condition of `state_check_triangle_render` is redundant for safety; its test row evaluates the field offset before the first triangle is prepared. Owner: Phase 10 test cleanup.
3. **K20 prose not pinned** (D4, advisory above).
4. **09-VALIDATION.md** (VL-02): flip `status`, `nyquist_compliant` and the 25 rows at phase complete; every row resolves.
5. **State version marker** (first-pass R1 RC-06): the v25 container carries no marker that the content changed in plan 09-10; provisional until the Phase 10 freeze, stated in the claims limits.

## Gaps Summary

No gap remains. The goal, the four roadmap success criteria and the seven requirement ids hold at the qualified commit `d3211e9`; the ten claim blocks are within K20 and the claim rules and every counted denominator reconciles with the retained originals; the qualification evidence is unchanged and consistent in everything I could re-derive; the frozen-tree facts hold; nothing regressed. Two items are carried openly and are yours: VL-01 (the literal `--verify-evidence` replay was not run) and the macOS cold-time deviation (673 s).

---

_Verified: 2026-11-20T19:55:00Z_
_Verifier: Claude (gsd-verifier)_
