---
phase: 09-controller-save-media-and-replay
verified: 2026-11-20T16:40:00Z
status: passed
score: 13/13 must-haves verified
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
  - ".planning/phases/09-controller-save-media-and-replay/09-11-PLAN.md"
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
covered_digest: "v4:sha256:955f5003487a7aa0743cc08beef7cbe2a0f58ecde5b31950c9a5f8634db6de43"
behavior_unverified: 0
overrides_applied: 0
re_verification:
  previous_status: gaps_found
  previous_score: 6/13
  gaps_closed:
    - "VG-01 canonical state and digest carry the RDP combiner block and the memory coverage table (FX-02, FX-24); source and imported peer draw the same triangle and agree on framebuffer and digest; the audio fixture shows 0 differing coverage bytes (first pass 84)"
    - "VG-02 the importer re-establishes the rasterizer's acceptance bounds for a triangle render in progress (FX-01); row 1,500 and hostile shade slopes refused atomically; a randomized hostile import under ASan and UBSan found nothing"
    - "VG-03 native stage limits derived from measurements, pinned by value and by relations, and tied to the largest registered TIMEOUT (FX-20 to FX-23); my own ASan measurement of the dominant test is 1,224.02 s against a 2,400 s TIMEOUT and a 3,000 s wrapper"
    - "VG-04 D-44 store budget from the word the store writes, including partial stores and the reverse-endian arm (FX-04, FX-28)"
    - "VG-05 controls for active-descriptor immutability, the receive fill and the RDRAM-writer invalidations, plus guard pins (FX-08, FX-19)"
    - "VG-06 EEPROM read length and SRAM window-end rows (FX-08)"
    - "VG-07 all 36 option pairs, synthetic-mode rows and --save-import-fd pinned and defined in the spec (FX-13, FX-14, FX-33)"
    - "VG-08 full continuation matrix registered as a recorded native step; sampling named in the spec (FX-21, FX-34); exact wording constraint stated in the VG-08 row"
    - "VG-09 campaign and sweep reach reader-level rules, tracer compares the full tuple, nested accesses scanned, report span judged first (FX-03, FX-05, FX-06, FX-07)"
    - "VG-10 publisher withdraws a failed publication, a resumed run ends in an accepted closed record, supervisor outputs proved before the guest, contract stated by outcome (FX-10, FX-11, FX-29)"
    - "VG-11 the three spec passages rewritten from the code (FX-09, FX-15, FX-30, FX-35); the validation record stays draft by design (accepted limitation)"
  gaps_remaining: []
  accepted_limitations:
    - "09-VALIDATION.md keeps status draft, nyquist_compliant false and 25 pending rows until the final 09-11 verification (all 25 rows resolve against the built binaries: 36 CTest names, 71 case references, 0 unresolved)"
    - "Phase 8 qualifier limits (scripts/qualify-audio.py) are below the shared stage's new worst case: a new Phase 8 native qualification would be refused closed by its own verifier (re-check R3 RC-02, owner the next Phase 8 requalification)"
    - "a resumed run never reports pass, and a runner-published state resumes into the closed unknown-log record: the runner shows the state-resume mechanism only"
    - "the v25 container and digest carry no marker that the state content changed in plan 09-10 (re-check R1 RC-06): provisional until the Phase 10 freeze, build identity binding refuses older blobs"
  regressions: []
deferred: []
advisory: []
coincidental_reliance_items:
  - truth: "A recorded input replay reproduces the original run's digests (SC3, CORE-05)"
    reason: fixture-only
    harden: "the replay-mix guest still uses no RDP combiner or coverage state, so its equality is not evidence for the carried members (the state tests and the reproductions below are); add an RDP-active replay checkpoint in a later phase"
flagged_prohibitions:
  - id: "09-01-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; only authored inputs and the already cleared openIPL3 asset pinned in earlier phases"
    verification: judgment
    verdict: "not violated"
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
    verdict: "not violated (rights scan findings=0; the manifest, the Phase 7 authority record and the first hosted pair record have an empty diff since the first pass)"
    authoritative: "command part yes, manifest review no"
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
    authoritative: "command part yes, manifest review no"
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
    verdict: "not violated (findings=0; tie and timing text in the specs and claim skeleton is labeled named software-profile policy)"
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
    verdict: "not violated (git status clean; no tracked blob extension; every blob of my reproductions was produced in memory in an out-of-tree scratch clone)"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "09-11-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; every input is an authored fixture or the cleared openIPL3 asset (plan not yet executed; recorded for the verdict ledger only)"
    verification: judgment
    verdict: "not yet exercised; no violation possible before the plan runs"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
---

# Phase 9: Controller, Save Media and Replay Verification Report

**Phase Goal:** Timed logical input drives the standard controller, explicitly selected save devices behave with defined refusal postconditions, and machine state and input replays reproduce runs exactly
**Verified:** 2026-11-20T16:40:00Z
**Status:** passed
**Re-verification:** Yes, the one gaps-only re-verification after the single fix cycle (groups G1 to G4, FX-01 to FX-37). The initial report (gaps_found, 6 of 13, gaps VG-01 to VG-11, pre-fix digest `v4:sha256:7b7b070b...`) is preserved verbatim in the build gate directory.
**Verified source:** HEAD `944443f`, tree clean (`git status` empty before and after). The digest above covers the original 110-file list of the brief, the one file the cycle added (`runner/tests/t64_test_tree.h`), `09-REVIEW-FIX.md`, and one further file the cycle modified that is not on the original list (`core/t64_rdp_raster.c`, the shared triangle extent function of FX-01), plus the 24 PLAN and SUMMARY files the fingerprint command adds (137 files). Computed by `verification.fingerprint`, never by hand, and recomputed once to confirm it is stable. The digest over the literal list without `core/t64_rdp_raster.c` (136 files) is `v4:sha256:61f87122b7b49ed1e4c5638b285d5809e583a5582343e03dcfbbceda486f7481`; the recorded one is the stricter, because an edit to the extent function would change the importer's bounds. It goes stale on any edit to a covered file.

## Verdict

The goal is achieved for the authored scope the roadmap and claim define. Every one of the eleven previous gaps (VG-01 to VG-11) is closed, and I closed each by reproducing its original failing scenario or its mutant against the current tree, not by reading the FX records. One limitation is accepted by me (VG-11: the validation record stays draft until the final verification) and three more are recorded and accepted as stated limits (see the frontmatter). No regression: the full CTest inventory passes in an independent clone, the criteria check passes, the repository's own scheduler mutant driver gives the same verdicts as before, and 46 of 47 out-of-tree mutants of the pinned behaviors fail named tests (the one survivor is an accept-equivalent redundant check, an INFO observation).

Starting hypothesis was that the fixes closed the findings the reviewers named and no more. It did not survive contact with the evidence: I replayed the exact scenarios of the first pass (the DP-register combiner state and the audio fixture for VG-01, row 1,500 and hostile shade slopes for VG-02, the SWR budget row for VG-04, the aliasing report span, the campaign counts) and then attacked the fix itself with a randomized hostile-triangle import under ASan and UBSan (50 trials: 29 accepted and advanced, no sanitizer report; 21 refused; 0 unexpected). The fixes hold.

## What I ran (independent of the SUMMARY and FIX records)

- A clean out-of-tree clone of HEAD (`git clone --no-local`, full history, the private fixture directory copied in; every `.z64` byte-identical to the repository's) built with the gate preset flags (Release, `-Werror`, 338 steps) and, for the sanitizer work, the asan preset (342 steps). Nothing was built, cleaned or rebuilt in the shared tree.
  - the full CTest inventory with the gate-script environment: **224 of 224 passed**, 68.97 s at `-j4` (the first pass ran 224 names in a history-less copy where 5 history-dependent names failed; the clone keeps history, so all pass);
  - `sh scripts/check-test-inventory.sh --preset gate`: PASS, 224; the guard `scripts/check-rsp-no-hle.py --build build/gate` exit 0; `scripts/qualify-phase09.py --selftest` exit 0; `scripts/tests/test_phase09_qualification.py --selftest` pass; `scripts/check-phase09-cases.py --suite state-audit --run` pass.
- `python3 -I -B build/gate/09-10-criteria-check.py` in the real tree (read only): `criteria map ok: rows=4 named_ctest=23 claim_pairs=10 cases_listed_by=15 binaries`.
- My own validation-map resolver: 25 rows of `09-VALIDATION.md`, 36 CTest names against `ctest -N`, 71 case references against `--list-cases` of the built binaries: **0 unresolved**.
- Scratch C programs linked against the HEAD core (release and ASan/UBSan builds) for VG-01, VG-02 and VG-09, and a randomized hostile-triangle importer attack under ASan and UBSan.
- 47 out-of-tree source mutants applied by my own driver to the clone only (each built, run against the related names, reverted, the clone left clean), plus `scripts/k5-mutants.py --run` in the clone: seven KILLED, the equivalent control SURVIVED, the non-compiling control INVALID, as in the first pass.
- `unit.core.state.import` under the registered sanitizer environment (VG-03): **passed in 1,224.02 s wall (1,171 s user)** with `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` and the registered sanitizer iteration counts, on a host at load average 10 to 16 (so the figure is upper-biased), no sanitizer report.
- Read-only repository checks in the real tree, all clean: `sh scripts/rights-scan.sh --tree` (findings=0), `sh scripts/pii-scan.sh --tree` (findings=0), `sh scripts/check-claims.sh` (errors=0), `sh scripts/check-docs.sh` ok, `python3 -B scripts/check-state-field-use.py --check` (findings=0) and `--selftest` (seven cases), `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` (valid; blocked), `python3 -B scripts/check-phase09-cases.py --selftest`, `sh tools/lint/core-lint.sh` (98 files, 0 violations), `sh scripts/check-ci-yml.sh` ok, `git ls-files` for ROM, save, state, replay, `.bin` and `.elf` extensions (empty), the freeze check against the tag `phase09-fh` (17 files: 9 identical, 2 data, 6 logic, each logic verdict carrying a recorded FD id), and a debt-marker scan of the 112 source-side covered files (no TBD, FIXME or HACK; every `XXX` hit is a `mkstemp` template such as `.tmp.XXXXXX`; the only TODO hits are the audit script's own placeholder-rejection rule and one sentence naming it).

## Re-verification of the eleven gaps

| Gap | Previous class | Verdict | How I closed it (evidence) |
| --- | --- | --- | --- |
| VG-01 canonical state omits the RDP combiner block and coverage table | blocking | CLOSED | **Reproduction 1** (stem id, IPL3 asset, DP registers: color image, scissor, fill, one-cycle mode, primitive, environment, fog, blend, combine): the source holds `combine_mode_valid`, `environment_color_valid`, `fog_color_valid` and 72 coverage marks; export 8,448,795 bytes (6,351,551 plus 2,097,244, as `docs/spec/state-container.md` says); the imported peer holds the same flags, `fog_color` 0xa1b2c3d4, 72 marks and an equal digest. The same four-word triangle then sent to both: **source and peer `triangle_count` 1, refusal 0, framebuffers equal, digests equal** (first pass: peer 0 and refusal 5, framebuffers and digests differ). **Reproduction 2** (authored `audio-pcm.z64` on the stem id, tick 540,000, packets drained, export 8,449,595 bytes): **0 coverage bytes differ** (first pass: 84), digests equal, and 20 of 20 continuation checkpoints (1,000-tick steps) equal. **Digest completeness**: flipping each of blend color, combine mode, the combine-valid flag, key width, fog color, environment color, primitive LOD fraction and the first and last coverage byte moves the digest (9 of 9; first pass: unchanged). **Mutants** killed: importer drops the combine mode (N3), digest walker omits the coverage table (N4, five names), the walker's fail-closed guard removed (N18). Code: carried tail read at `core/t64_state.c:1872-1910` (closed entry set 0 or 0x80 to 0x88, which matches every write at `core/t64_rdp_pipeline.c:235-250`), hashed at `core/t64_digest.c:877-900` in the new class, fail-closed at `:328`. Docs: `docs/spec/machine-contract.md` states the members and the 2,097,244-byte growth. The 17 pre-Phase-9 ids are unchanged (the diff of the legacy digest literals removes no hex line; only the stem family literal of `core/tests/test_save.c` moved). |
| VG-02 importer accepts a triangle render in progress without the rasterizer's bounds | blocking | CLOSED | `core/t64_state.c:1449-1483` re-establishes the acceptance proof through the shared `t64_rdp_triangle_extent` (`core/t64_rdp_raster.c:192-286`). **Reproduction** (render in progress from the DP registers, edited on the source and exported with valid hashes): baseline accepted and continued; `render_y` 1500 refused (status 32, FIELD_RANGE, section 8, INDEX_LT, peer digest unchanged); `render_y_end` 1500 and both 1500 refused the same; a wide shaded triangle (rows 0 to 1022, `render_y` 500) with benign slopes accepted and **advanced 20,000 ticks under ASan and UBSan with no report**; the same with every slope word 0x7FFFFFFF7FFFFFFF refused (FIELD_RANGE, LEN_LE); opcode 0x0B refused (ENUM_CLOSED). **Randomized attack on the fix** (flat and shade triangles, random edge and slope words, random scissor and image width, the rows taken from the extent function, 50 trials under ASan and UBSan): 29 consistent states accepted and each advanced 30,000 ticks with no sanitizer report, 21 inconsistent or unbounded states refused, **0 unexpected** (no consistent state refused, none inconsistent accepted). **Mutants**: the extent and products check always true (N2) killed by `unit.core.state.import`; the first row-range condition removed (N1) survives and is accept-equivalent (Observation 1). Eight `render-triangle-*` rows of `docs/spec/state-mutants.txt` run with accepted baselines in `case_mutant_refusals`. |
| VG-03 qualification limits | blocking | CLOSED (mechanism and measurement) | Limits are module constants pinned by value and by relations in `scripts/tests/test_phase09_qualification.py`: ASan full CTest wrapper 3,000 s (`scripts/qualify-rdp-vi.py:80`), configure 120, build 600, gate 600, gcc13 300; outer limit 4,800 (`scripts/qualify-phase09.py:65`) at least the stage sum 4,620 plus 120; record bound 3,000 at least the largest stage; the wrapper must exceed the largest registered TIMEOUT (now 2,400, `core/CMakeLists.txt:1106`) by 600. The control reads every `TIMEOUT` literal of the **tracked** `CMakeLists.txt` files (`git ls-files`, at least 40 values, a non-literal TIMEOUT fails the parse), not `ctest --show-only=json-v1` as I first asked (DEV-09): I accept this, because that verb rewrites `LastTest.log` in the build it runs in and a gate test must not do that, and an untracked copy of the repository cannot move the result. **Measurements**: the retained run of the fix tree (226 of 226 passed, no sanitizer report, JUnit and LastTest files present) is 1,380.7 s for the whole ASan CTest and 1,339.5 s for the import test; my own run of the registered command for the import test alone is **1,224.02 s**, a 1.96 times margin to its 2,400 s TIMEOUT and 2.45 times to the 3,000 s wrapper, so the first-pass failure (a 600 s wrapper killing a test that needs about 1,200 s) cannot recur. **Mutants**: the ASan wrapper back to 600 (N11), the outer limit back to 2,400 (N12) and the matrix step without its environment (N15) each fail `harness.phase09-qualification`. Fail-closed behavior is unchanged (a timeout raises `Refusal`, `_run_bounded`). Not run here: the whole native flow end to end; its first run is the Task 6 rehearsal that follows this verification, not an obligation of this report. |
| VG-04 SWR store budget not atomic | non-blocking | CLOSED | `core/t64_cpu.c:1485-1540` budgets from the word the store writes. The refuse-then-accept rows for SWL and SWR at RD64B, WR64B and the sequence analogue and the TLB-mapped reverse-endian rows bite: partial store not aligned (N5) and the reverse-endian arm deleted (N5b) are both killed by `unit.core.cpu.si-store-budget`. |
| VG-05 behaviors no test can fail | non-blocking | CLOSED | Active-descriptor immutability: the completion reading the live latch (N6) is killed by `unit.core.si.device` and by the guard `repo.rsp-no-hle`. Invalidations: deleting both calls in the RD64B completion (N7) is killed by `unit.core.si.device` and the guard; deleting both in the SRAM-to-RDRAM apply (N7b) by `unit.core.save.device` and the guard. Timeout-branch fill: a 0xFF fill (N8, the mutant that was invisible in the first pass) is killed by `unit.core.joybus.parser` and `unit.core.save.device`. Spec sentences at `docs/spec/si-pif-controller.md:71-80` and `docs/spec/save-media.md:126-128`; the guard pins the bodies of `t64_save_sram_dma_apply` and `t64_si_complete_event` and their invalidation tokens (FD-04). |
| VG-06 EEPROM read length and SRAM span rows | non-blocking | CLOSED | A read classified valid for rx at least 8 (V3) and an SRAM DMA span that tolerates two bytes past the window (V4) are each killed by `unit.core.save.device`. |
| VG-07 option pairs | non-blocking | CLOSED | `runner/tests/test_phase09_runner.c:904-950` holds all 36 unordered pairs behind a `_Static_assert` and a uniqueness check and compares them with the 9 by 9 table of `docs/spec/state-container.md`; synthetic-mode rows for the 19 flags and `--save-import-fd` rows exist. Replay accepting the save flags (N10) is killed by `runner.supervisor-argv-bound`. The six INPUT combinations are defined in the spec. |
| VG-08 continuation matrix sampling | non-blocking | CLOSED, with a wording constraint | The full matrix is now a recorded native qualification step, `normal-state-import-full-matrix` (`scripts/qualify-phase09.py:73-76,1691-1730`), with its own marker check (the recorded environment, one passed JUnit case, more checks than the sampled run of the same record). **Exact wording constraint for the 09-11 claim**: the full continuation matrix is **17 rows, B01 to B17**; **B01, B07, B10 and B14 run every partition size (one-shot, 1, 7, 31 and 4,096 ticks) in every preset; the other 13 rows are sampled** (the imported peer in 7-tick partitions against the one-shot source) in the gate, hosted and sanitizer runs, and the instruction-count mid-tick stop row runs only in the full matrix; **the full set (all 17 rows, every partition, and that row) runs only in the native qualification step**, not on the hosted platforms and not under the sanitizer; the step records are unsealed originals. I confirmed the table (`core/tests/test_state_import.c:3222-3280`: exactly four rows with `full` true) and the spec (`docs/spec/state-container.md:631-633,670-673,741-743`), and killed a drift of the sampling sentence (N14) and a step without its environment (N15). |
| VG-09 hostile-import evidence tooling | non-blocking | CLOSED | Campaign reach, measured by running the case: gate default 12 iterations give **9 refusals, 5 at reader level (reasons 26 to 29), 3 accepted** (first pass: 12 of 12 at container level); at the sanitizer count 120 the stem campaign gives 85 refusals, **36 at reader level or later**, 35 accepted, and the triangle-blob campaign 43 refusals and 17 accepted, violations 0 (first pass: 6 of 117). The runner tracer `runner.phase09-tracer` passes and its retained output prints expected and observed tuples, both `STATE_RANGE/OBSERVER_RANGE/16/LEN_LE`. Nested accesses: restoring the pre-fix lookbehind (N13) fails `harness.state-field-use-audit`. **Report span**: with the report pointer inside the machine's RDRAM a NULL blob, a zero length, an oversize length, a bad flag word and a garbage blob all end with status INVALID_ARGUMENT and **no aliased byte changed**, while a disjoint report receives reasons 1, 2, 2, 3 and 8; removing the check (N23) is killed by `unit.core.state.import`. |
| VG-10 publisher and supervisor contract | non-blocking | CLOSED | Publisher: `runner/t64_artifact_publisher.c:572-660` sets `committed` only after both post-rename checks and withdraws the leaf (only while it is still the held file) and the stage on any later failure; a withdrawal that does not withdraw (N9) is killed by `controls.artifact-substitution` and `runner.state-export`, a withdrawal without the identity test (N20) by `controls.artifact-substitution`. Resumed run: `runner/t64_real.c` ends a resumed run of a runner-published state in the closed unknown-log record; `case_import_accepted_continues` asserts the checker accepts it, the outcome is harness-error, and the guest tick and final state digest equal the uninterrupted run's (`runner/tests/test_state_runner.c:1845-1935`); reverting it (N16) is killed. Supervisor: both post-publication outputs are probed before the guest runs (`runner/t64_supervisor.c:182-203` and the calls in `real_run_mode`), N17 killed. The contract is stated by outcome in `docs/spec/save-media.md:405-452`, which I read: it changes the spec for what a supervisor failure after the commit leaves (DEV-12), a design choice I accept because the record, not the leaf, says whether a run passed and the spec says so. Stated limit: a resumed run never reports pass and a runner-published state resumes into the unknown-log form, so the claim must say the runner shows the mechanism only. |
| VG-11 spec and validation drift | non-blocking | CLOSED (one accepted limitation) | The three passages are rewritten from the code: `docs/spec/result-protocol.md:580-592` (15 rows with their own pins, the legacy 30-byte pin for the rest, a resumed run never passes), `docs/spec/si-pif-controller.md:27-29,124-128` (channel 4 is answered by the save device on the two EEPROM ids), `docs/spec/machine-contract.md:321-324,349-357,389-405,949-965` (the combiner block and coverage table are hashed in the new-class tail for the family). **Accepted limitation**: `09-VALIDATION.md` keeps frontmatter `status: draft`, `nyquist_compliant: false` and all 25 rows pending until the final 09-11 verification, as the orchestrator stated; everything it can be checked against resolves (25 rows, 36 CTest names, 71 case references, 0 unresolved). |

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
| --- | --- | --- | --- |
| 1 | SC1 (SAVE-01): a timed-input script drives the standard controller over SI/PIF joybus and an authored ROM reports the expected button states at the expected guest times | VERIFIED | Unchanged chain (strictly-after latch `core/t64_si.c:46-92`, joybus `core/t64_joybus.c`, `fixtures/pad/poll.S`, independent oracle `fixtures/pad/poll_oracle.py`, `runner.pad-poll`, `controls.real.pad-wrong-value`); none of these sources changed in the cycle. The first-pass mutants re-run: latch visible at T (S1), joybus byte ticks (S2), high button byte (S3), low-byte mask (S4), x and y swapped (S5), port above 0 accepted (S6), channel 1 as a controller (S7), a sample one tick early (S8): each fails 1 to 5 named tests. Limit kept: "expected guest time" is relative to the first poll start. |
| 2 | SC2 (SAVE-02): EEPROM 4 and 16 Kbit and SRAM 32 KiB work only when the profile selects them explicitly | VERIFIED | Selection is `profile.save_kind` only (unchanged). Busy window 375000 to 374999 (V1), busy compare (V2), read length (V3), SRAM span (V4) and a busy write that commits (V5) each fail `unit.core.save.device` or more; the two first-pass survivors are now killed. |
| 3 | SC2 (SAVE-04): unknown or unsupported device requests are refused with defined postconditions | VERIFIED | `core/t64_save.c:259-313` unchanged; `unit.core.save.device` and `unit.core.save.import-export` pass in the full run and fail under the mutants above. |
| 4 | SC2 (SAVE-03): save bytes round-trip with endianness handling and a refused import leaves the digest unchanged | VERIFIED | Unchanged transforms; the generation helper is shared and saturating (`core/t64_save.c:73-78`), killed when made a plain increment (N19: `unit.core.save.import-export`, `unit.core.state.import`). |
| 5 | SC3 (CORE-04): exporting and re-importing machine state reproduces the canonical digest exactly, so the restored machine continues as the original | VERIFIED | VG-01 reproductions; row B17 added to the boundary matrix; N3, N4 and N18 killed; the runner path asserts equal guest tick and final state digest for a mid-run native state. |
| 6 | SC3 (CORE-05): a recorded input replay bound to profile, build and content hash reproduces the original run's digests | VERIFIED (reliance note) | Unchanged format and binding; the content hash (R1), profile digest (R2) and build id (R3) not compared and the input submitted at `at` instead of `event_tick` (R4) each fail `unit.replay.suite` and `runner.replay-mix`. The goldens were re-captured for the FX-02 digest change; the record says the canonical values were re-derived by a procedure that first reproduced the old pins, and the suites pass. |
| 7 | SC4 (CORE-02): CPU, RSP, RDP, VI, AI, PI, SI and MI activity is ordered by the same guest-time scheduler, with coincident-event and partition/interleave checks, and the quantum stays part of profile identity | VERIFIED | Tie reversed (C1), tie by device id (C3), quantum omitted from the profile hash (C2) and descriptor 12 accepting any quantum (C4) each fail named tests; `scripts/k5-mutants.py --run`: seven KILLED, the equivalent control SURVIVED, the compile control INVALID. Evidence class unchanged: regression pins and self-comparison, every tie labeled named policy. |
| 8 | An accepted state import cannot cause out-of-range access, unbounded loops or undefined arithmetic (D-30, D-41) | VERIFIED | VG-02 row: reader rule, reproduction, randomized attack under ASan and UBSan, N2 killed, atomic refusal (peer digest unchanged on every refused row). |
| 9 | The frozen qualification route can complete (row 9-V25, D-40) | VERIFIED (mechanism and measurement; the end-to-end native flow is the Task 6 rehearsal) | VG-03 row. |
| 10 | Every option pair the plans define as refused is refused and pinned | VERIFIED | VG-07 row (36 pairs, static assert, spec table compare, N10 killed). |
| 11 | SI active-descriptor immutability | VERIFIED | N6 killed by `unit.core.si.device` and the guard. |
| 12 | Timeout and absent joybus answers write no receive data | VERIFIED | N8 killed (frames with a 0xA5 fill). |
| 13 | SI RD64B completion and SRAM-to-RDRAM DMA clear the LL link and coverage marks over their span | VERIFIED | N7 and N7b killed by behavior tests and by the guard. |

**Score:** 13/13 truths verified (0 present and behavior-unverified, 0 failed). A truth that rests on a state transition or an ordering or cleanup invariant counts as verified only where a named test failed under a mutant of it; each of truths 5, 6, 7, 8 and 10 to 13 does.

### Mutation evidence (out-of-tree, HEAD, gate flags, each mutant reverted, the clone left clean)

47 mutants: 46 killed, 1 survived (N1, accept-equivalent).

| Mutant | Result |
| --- | --- |
| S1 latch visible at T instead of T+1 | FAIL: runner.pad-poll, controls.real.pad-wrong-value, unit.core.input.latch-partition |
| S2 joybus byte ticks 2000 to 2001 | FAIL: runner.pad-poll, unit.core.joybus.parser, controls.real.pad-wrong-value, unit.core.input.latch-partition, unit.core.si.device |
| S3 high button byte takes the low byte | FAIL: the same five names |
| S4 low-byte mask 0x3F to 0xFF | FAIL: unit.core.joybus.parser |
| S5 x and y swapped | FAIL: the same five names |
| S6 queue_input accepts ports above 0 | FAIL: unit.core.input.latch-partition |
| S7 channel 1 answers as a controller | FAIL: runner.pad-poll, unit.core.joybus.parser, controls.real.pad-wrong-value, unit.core.si.device |
| S8 latch sampled one tick early | FAIL: runner.pad-poll, controls.real.pad-wrong-value, unit.core.input.latch-partition |
| V1 EEPROM busy window 375000 to 374999 | FAIL: unit.core.save.import-export, unit.core.save.device |
| V2 busy compare `<` to `<=` | FAIL: unit.core.save.device |
| V3 read classified valid for rx at least 8 | FAIL: unit.core.save.device |
| V4 SRAM DMA span tolerates two bytes past the window | FAIL: unit.core.save.device |
| V5 busy write commits its data | FAIL: controls.real.save-wrong-byte, runner.save-fixtures, unit.core.save.device |
| C1 tie by sequence reversed | FAIL: unit.core.sched.characterize-fifo, unit.core.scheduler, unit.core.sched.matrix |
| C2 quantum omitted from the profile hash | FAIL: unit.core.profile.v12-identity, unit.core.profile.quantum-identity, unit.core.sched.matrix |
| C3 tie by device id ascending | FAIL: unit.core.sched.characterize-fifo, unit.core.sched.matrix |
| C4 descriptor 12 accepts any quantum | FAIL: unit.core.profile.v12-identity, unit.core.profile.quantum-identity |
| R1 to R4 content hash, profile digest and build id not compared; input at `at` | each FAIL: unit.replay.suite and runner.replay-mix |
| I1 declared digest never checked | FAIL: unit.core.state.import |
| I2 busy_until upper bound removed | FAIL: unit.core.state.import |
| I3 trailer never checked | FAIL: runner.state-export, unit.core.state.import |
| N1 importer: first row-range condition removed | SURVIVES (accept-equivalent, Observation 1) |
| N2 importer: extent and products check always true | FAIL: unit.core.state.import |
| N3 importer drops the combine mode | FAIL: unit.core.state.import |
| N4 digest walker omits the coverage table | FAIL: unit.core.state.export, unit.core.digest.v25-reference, unit.core.sched.matrix, runner.state-export, unit.core.state.import |
| N5, N5b D-44 partial-store alignment removed; reverse-endian arm deleted | each FAIL: unit.core.cpu.si-store-budget |
| N6 SI completion reads the live DRAM latch | FAIL: repo.rsp-no-hle, unit.core.si.device |
| N7 SI RD64B completion drops both invalidations | FAIL: repo.rsp-no-hle, unit.core.si.device |
| N7b SRAM-to-RDRAM apply drops both invalidations | FAIL: repo.rsp-no-hle, unit.core.save.device |
| N8 joybus timeout branch fills the receive area with 0xFF | FAIL: unit.core.joybus.parser, unit.core.save.device |
| N9 publisher: post-rename sync failure does not withdraw | FAIL: controls.artifact-substitution, runner.state-export |
| N10 supervisor: replay accepts the save flags | FAIL: runner.supervisor-argv-bound |
| N11, N12 shared ASan wrapper back to 600 s; outer limit back to 2,400 s | each FAIL: harness.phase09-qualification |
| N13 field-use audit: the pre-fix lookbehind restored | FAIL: harness.state-field-use-audit (my first version of this mutant was a wrong regular expression and survived; it is discarded and the exact pre-fix pattern is what is counted) |
| N14 sampling sentence of the matrix drifts | FAIL: harness.state-field-use-audit |
| N15 full-matrix step without its environment | FAIL: harness.phase09-qualification |
| N16 resumed-run record reverted | FAIL: runner.state-export |
| N17 supervisor: capture path not probed | FAIL: runner.state-export |
| N18 digest walker not failing closed | FAIL: unit.core.digest.* (the control aborts on the missing table) |
| N19 save generation not saturating | FAIL: unit.core.save.import-export, unit.core.state.import |
| N20 publisher withdrawal without the identity test | FAIL: controls.artifact-substitution |
| N23 report-span aliasing not judged | FAIL: unit.core.state.import |

### Required Artifacts

| Artifact | Expected | Status | Details |
| --- | --- | --- | --- |
| `core/t64_si.c`, `core/t64_joybus.c`, `core/t64_observe.c` | SI registers, DMA, joybus, latch | VERIFIED | logic unchanged; what FX-08 added is test coverage |
| `core/t64_save.c`, `core/t64_pi.c`, `core/t64_bus.c` | EEPROM, SRAM, import and export | VERIFIED | one shared saturating generation helper (`core/t64_save.c:73-78`) |
| `core/t64_scheduler.c`, `core/t64_machine.c` | one scheduler, family profile identity | VERIFIED | |
| `core/t64_digest.c`, `core/t64_state.c`, `core/t64_rdp_raster.c` | v25 walker, container export and import, shared extent | VERIFIED | carried tail, triangle render check, fail-closed walker |
| `runner/t64_replay.c`, `runner/t64_supervisor.c`, `runner/t64_real.c`, `runner/t64_artifact_publisher.c` | replay, option surface, runner, publisher | VERIFIED | withdrawal, probes, closed unknown-log record |
| fixtures, oracles and ROM sources | authored guests and independent oracles | VERIFIED | `fixtures/manifest.json`, `docs/evidence/phase07-oracle-authority.json` and `docs/evidence/phase09-first-hosted-pair.json` have an empty diff since the first pass; no tracked ROM, save, state or replay file |
| `scripts/qualify-phase09.py`, `scripts/qualify-rdp-vi.py` | frozen qualification route | VERIFIED (limits) | VG-03 row |
| `docs/claims/phase09.md`, `docs/evidence/phase09-success-criteria.md` | claim skeleton and draft map | VERIFIED as drafts | the skeleton's CORE-04 sentence ("re-imports and reproduces the canonical digest and the continuation digests for authored machines on the same build") is now true; the criteria check passes |

### Key Link Verification

| From | To | Via | Status | Details |
| --- | --- | --- | --- | --- |
| machine | state container | digest sink sections to exporter; sections to readers to commit | WIRED including the RDP carried tail | VG-01 reproductions; N3 and N4 killed |
| importer | rasterizer acceptance proof | `state_check_triangle_render` calls `t64_rdp_triangle_extent` | WIRED | N2 killed; randomized attack |
| publisher | callers | `finalize` withdraws on a post-rename failure; `abort` is a no-op once committed | WIRED | N9, N20 killed |
| supervisor | result and capture paths | `atomic_write_probe` before the guest | WIRED | N17 killed |
| qualifier | native stage limits and the matrix step | module constants, `_native_command_plan` | WIRED | N11, N12, N15 killed |
| input rows, ROM frame, profile id, replay file, every device to the scheduler | unchanged first-pass links | | WIRED | mutants S1 to S8, R1 to R4, C1 to C4 |

### Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
| --- | --- | --- | --- | --- |
| state blob | RDP combiner and coverage members | section 8 carried tail, written by the exporter, read by `state_read_rdp_carried` | yes (reproductions 1 and 2) | FLOWING (first pass: DISCONNECTED) |
| replay checkpoint digests | canonical digest | `t64_machine_digest` including the carried tail | yes | FLOWING (the replay-mix guest uses no RDP state, see the reliance note) |
| pad-poll, EEPROM and SRAM transcripts | PIF RAM bytes, block bytes, busy flag | latch sample, `machine->save` | yes | FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| --- | --- | --- | --- |
| full inventory | `ctest -j4` in the clean clone | 224 of 224 | PASS |
| criteria map | `python3 -I -B build/gate/09-10-criteria-check.py` | ok, 23 named CTest names, 10 claim pairs | PASS |
| validation map | resolver over `09-VALIDATION.md` | 25 rows, 36 names, 71 cases, 0 unresolved | PASS |
| scheduler mutants | `scripts/k5-mutants.py --run` in the clone | 7 KILLED, control SURVIVED, control INVALID | PASS |
| state campaign | `test_state_import --case fuzz-campaign` (default and with 120 iterations) | 9 of 12 and 85 of 120 refused, reader level 5 and 36, violations 0 | PASS |
| importer under sanitizers | scratch programs and the randomized triangle attack | no report | PASS |
| ASan state import | `ctest --test-dir build/asan -R '^unit\.core\.state\.import$'` | passed, 1,224.02 s | PASS |
| inventory and guard | `check-test-inventory.sh --preset gate`, `check-rsp-no-hle.py --build` | PASS 224, exit 0 | PASS |

### Probe Execution

No `probe-*.sh` scripts are declared or exist for this phase. Not applicable.

### Requirements Coverage

| Requirement | Source plans | Status | Evidence |
| --- | --- | --- | --- |
| SAVE-01 | 09-02, 09-03, 09-03.1, 09-10, 09-11 | SATISFIED (authored scope) | truths 1, 11, 12; VG-04 closed |
| SAVE-02 | 09-02, 09-04, 09-10, 09-11 | SATISFIED (authored scope) | truth 2; VG-06 closed |
| SAVE-03 | 09-02, 09-05, 09-10, 09-11 | SATISFIED | truth 4 |
| SAVE-04 | 09-04, 09-05, 09-10, 09-11 | SATISFIED | truths 3 and 12 |
| CORE-02 | 09-01, 09-03, 09-03.1, 09-06, 09-10, 09-11 | SATISFIED | truth 7 |
| CORE-04 | 09-01, 09-02, 09-08, 09-09, 09-09.1, 09-10, 09-11 | SATISFIED (unit-tested tier; claim wording constraints below) | truths 5 and 8 |
| CORE-05 | 09-02, 09-07, 09-10, 09-11 | SATISFIED (authored scope, reliance note) | truth 6 |

REQUIREMENTS.md maps exactly these seven ids to Phase 9; no orphaned requirement; every id is claimed by a plan.

### Decision and amendment coverage (regression)

No decision lost its owner or its test: 45 of 45 Phase 9 CTest names are still registered and no CTest name was added or renamed (the one case rename, `mutant-count-drift` to `doc-count-drift`, is inside `harness.state-field-use-audit`); `docs/spec/phase09-cases.json` and the criteria map resolve. The evidence of these decisions changed: D-30 and D-41 (the importer contract now includes the triangle render bounds), D-32 (full matrix registered as a native step, sampling named), D-44 (complete for partial stores and the reverse-endian arm), D-45 (nested accesses scanned), D-34 and D-35 (all 36 option pairs pinned), D-05 and K1 (the strip-and-compare against v24 and the inactive depth members stand; the carried tail is new-class), D-06 (six frozen files changed, each with an FD record, as the freeze check shows).

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| --- | --- | --- | --- | --- |
| none in the 112 source-side covered files | | TBD, FIXME, HACK | | `XXX` matches only `mkstemp` templates |

Stubs: none on a rendering or data path.

### Human Verification Required

None. The remaining decisions are routine reversible ones under `.planning/AUTONOMY.md` section 2; no rights, authority, physical, paid, publication or destructive stop is crossed. The 12 prohibition statements above are reported under the autonomous soft gate with non-authoritative judgment verdicts and are not an acknowledgment gate. No human validation is recorded or implied by this report.

### Observations (evidence-backed, none a gap)

1. **N1 survives and is accept-equivalent.** The first condition of `state_check_triangle_render` (`y_now < 1024 && y_end < 1024`) is redundant for safety: the extent function clamps the last row to 1,023 and the later derived-equality and range checks refuse every blob the first check refuses, so the set of accepted blobs is identical with or without it; only the reason tuple differs for a `render_y_end` of 1,024 or more. The row `render-triangle-row-beyond-coverage` (`core/tests/test_state_import.c:4933-4947`) cannot see the difference because its first edit, `tri_edit(TRI_FLAT, tri_blobs[TRI_FLAT].field + 12u, ...)`, reads `field` before `tri_prepare` runs (the argument is evaluated first and `field` is still 0 on the first triangle row), so the `render_y_end` edit lands at section offset 12 and only `render_y` is edited. Harmless today (the other arm refuses with the same tuple); the fix is to call `tri_prepare(TRI_FLAT)` before the edit. Owner: Phase 10 test cleanup.
2. **Phase 8 requalification limits** (re-check R3 RC-02): `scripts/qualify-audio.py` keeps its own outer limit and record bound of 2,400 s, below the shared stage's new worst case, so a new Phase 8 native qualification on this tree would be refused closed by its own verifier (a refusal, not a false pass); the Phase 8 record already sealed still verifies.
3. **First hosted pair digests** (FX-27): the four non-null `state_digest` values of `docs/evidence/phase09-first-hosted-pair.json` belong to the digest function of source revision 2db1be4d and are superseded by the 09-11 pair; the record is retained unedited and no test compares them with current output.
4. **State version marker** (re-check R1 RC-06): the v25 container and digest carry no marker that the state content changed in plan 09-10. Accepted: the state and container versions are provisional until the Phase 10 freeze and a blob of another build is refused by the build identity binding.
5. **Hosted and native evidence still to come**: six logic verdicts of frozen files (FD-01 and FD-03 to FD-06) and the runner identity change (FX-23) need the Task 6 native and hosted rehearsal; this verification does not run qualification.
6. **Claim wording obligations for 09-11** (recorded in `09-REVIEW-FIX.md`; `09-11-PLAN.md` was not updated by the cycle and does not yet carry them): the matrix wording of the VG-08 row; the runner shows the state-resume mechanism only (a resumed run never reports pass, a runner-published state resumes into the unknown-log record); the supervisor failures after the commit that leave a leaf next to a failure record, and the rule that a caller accepts exit status 0 plus a `pass` record; the hosted pair digests are valid for 2db1be4d only; the step records of the native qualification are unsealed originals; no raw check totals as strength.

### Advisory (New Scope, Unevidenced)

None.

### Gaps Summary

No gap remains and no regression was found. VG-01 and VG-02 (blocking) are closed by one change and verified together, including an attack on the fix; VG-03 (blocking) is closed at the mechanism and measurement level. The eight non-blocking gaps are closed by tests that fail under my own mutants of the behaviors they pin. `09-VALIDATION.md` stays draft by design until the final verification and is the one accepted limitation of the eleven. The replay half of criterion 3 keeps its fixture-only reliance note (the replay-mix guest uses no RDP state, so its equality is not evidence for the carried members; the state half is evidenced by the state tests and the reproductions above).

---

_Verified: 2026-11-20T16:40:00Z_
_Verifier: Claude (gsd-verifier)_
