---
phase: 09-controller-save-media-and-replay
verified: 2026-11-20T09:50:00Z
status: gaps_found
score: 6/13 must-haves verified
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
covered_digest: "v4:sha256:7b7b070bdbde269b8fc41205c0c0509bc0f458ebf56bf94790d87daa60c65d78"
behavior_unverified: 4
overrides_applied: 0
gaps:
  - id: VG-01
    class: blocking
    truth: "Exporting and re-importing machine state reproduces the canonical digest exactly, so that a restored machine continues as the original did (SC3, CORE-04, the phase goal 'machine state ... reproduce runs exactly')"
    status: failed
    reason: "The v25 canonical state, its digest and the container leave out behavior-bearing RDP state of the composite family: the combiner block (combine mode, environment, fog and blend colors, key registers and their valid flags) and the RDP memory-coverage table. The importer zeroes them. The digest therefore stays equal across an import that changes the future of the machine, and no check can see it. Reproduced twice with scratch programs against the public export/import API."
    artifacts:
      - path: "core/t64_digest.c"
        issue: "lines 669 and 677 hash the coverage table and the combiner block only under the older version predicates; core/t64_internal.h:1263 (depth predicate, descriptor 10 only) keeps them false for descriptors 11 and 12"
      - path: "core/t64_state.c"
        issue: "readers (1429-1633) carry none of these members; state_commit zeroes them (2821-2828)"
      - path: "docs/spec/state-container.md"
        issue: "lines 471-473 and 638-650 record the limit as a documented consequence, not as a decision that satisfies CORE-04"
    missing:
      - "a recorded F1 defect-fix decision: carry the members in the canonical state (new-class spans keep the strip-and-compare differential), or in a section outside the digest, or refuse export and import for such states; whichever is chosen, a control in which a source and an imported peer receive the same DP triangle after a SetCombine and agree on triangle count, framebuffer and digest, plus a one-member flip test for every RDP member"
      - "correct docs/spec/machine-contract.md where it states the members are hashed, and bound the CORE-04 claim text (the pending skeleton says continuation digests are reproduced)"
  - id: VG-02
    class: blocking
    truth: "An accepted state import cannot cause an out-of-range access, an unbounded loop or undefined arithmetic (the importer contract of D-30 and D-41, stated in core/include/tendies64/t64_machine.h above t64_machine_state_import)"
    status: failed
    reason: "A hash-valid state with a triangle render in progress is accepted without the bounds the rasterizer proved when it accepted the command: render_y and render_y_end are bounded only against the RDRAM extent, while the row executor indexes a 1,024-row coverage array, and the packet words are free 64-bit values. Acceptance of render_y = 1500 reproduced here (status OK, peer render_y 1500). The signed overflow and the out-of-array write are reported by the independent review and audit with their own harness; I did not reproduce the trap or the write. The write is blocked today only because the commit zeroes the combiner block (VG-01), so fixing VG-01 by carrying that block makes it live."
    artifacts:
      - path: "core/t64_state.c"
        issue: "1513-1542 and 1585-1617: render extent rule checks last row against RDRAM only"
      - path: "core/t64_rdp_raster.c"
        issue: "227 (acceptance clamp to 1,024 rows), 661-662 and 811 (coverage index), 385-399 (shade products) trust acceptance-time proofs"
      - path: "docs/spec/state-field-rules.txt"
        issue: "68: render_y :: ADDR_RANGE satisfies the field-use audit but is not the sufficient bound"
    missing:
      - "reader rule that re-establishes the acceptance bounds for a triangle render in progress (word count for the opcode, accepted opcodes, last row below the coverage height, product bounds) or refuses such a state"
      - "mutant rows with accepted baselines for a row at or above 1,024 and for an overflowing slope; the campaign base blobs must hold a render in progress; same change as VG-01"
  - id: VG-03
    class: blocking
    truth: "The frozen qualification route can complete with the registered test set, so the criteria can be claimed at the unit-tested tier (validation row 9-V25, D-40)"
    status: failed
    reason: "The shared native stage runs the full ASan CTest under a 600 s wrapper while one registered test needs longer than that under ASan. The stage fails closed (no false pass) and deterministically, so no native record can be sealed and the hosted collect and verify can never run."
    artifacts:
      - path: "scripts/qualify-rdp-vi.py"
        issue: "419-430: timeout_seconds=600.0 for ctest --preset asan; a timeout raises Refusal"
      - path: "core/CMakeLists.txt"
        issue: "1098-1100: unit.core.state.import TIMEOUT 1800, measured 631.8 s under ASan (09-09.1-SUMMARY.md lines 78 and 177; the retained project ASan log shows 631.80 s; my own loaded-machine run took 1,263 s wall); the last whole-suite ASan measurement 389.7 s predates it (09-03.1-SUMMARY.md line 209)"
      - path: "scripts/qualify-phase09.py"
        issue: "1641: outer shared-native-gates limit 2400 s is below the worst case 600 + 600 + 1800 + 120"
    missing:
      - "raise the wrapper above the largest registered TIMEOUT with margin, raise the outer limit and the matching bound in the receipt verifier, update the pinned test, and add a control that reads every registered TIMEOUT from ctest --show-only=json-v1 and requires the wrapper to exceed the maximum"
  - id: VG-04
    class: non-blocking
    truth: "A 32-bit store that starts an SI DMA is refused atomically when the queue or the sequence has no room (D-44)"
    status: partial
    reason: "The preflight budgets only the exact aligned register address. A full-word SWR at byte offset 3 of the RD64B register (and the mirrored forms) performs the RD64B start but is budgeted as one extra event instead of two. Reproduced with the existing test harness: with room for two events the SW control is refused atomically, the SWR form retires, queues both SI events (queue count 16 against the 14 expected) and leaves no CPU successor. Narrow reach (queue within two slots of its limit), but it is the property the decision locks."
    artifacts:
      - path: "core/t64_cpu.c"
        issue: "1483-1523 cpu_successor_extra_event_budget compares the translated physical address, while core/t64_memory.c:116-136 aligns a partial store down to its bus width"
      - path: "core/tests/test_si.c"
        issue: "case_cpu_store_budget (415-470) has only sw and sd rows"
    missing:
      - "budget from the address the store will use (aligned word, lane-xor under reverse endian), swr/swl refuse-then-accept rows for both registers and for the sequence analogue"
  - id: VG-05
    class: non-blocking
    truth: "Documented device behaviors that no test can fail on (present and wired, behavior unexercised): active-descriptor immutability, no data written on a timeout or absent answer, RDRAM-write side effects of SI RD64B and SRAM DMA"
    status: partial
    reason: "Out-of-tree mutants of each survive every related test: reading the live DRAM latch instead of the latched address at SI completion (13 related tests green); a 0xFF fill of the receive area in the timeout branch (13 green; every frame in tests, ROMs and oracles already uses a 0xFF fill, so untouched and filled are the same bytes; the absent branch is caught only by one 0xAA frame in unit.core.si.device); deleting both coverage and LL-link invalidations in the SI RD64B completion and the SRAM-to-RDRAM DMA apply (10 related tests green, and scripts/check-rsp-no-hle.py exits 0 although its route line claims invalidation by every writer)."
    artifacts:
      - path: "core/t64_si.c"
        issue: "311 (completion address) and 313-316 (invalidations)"
      - path: "core/t64_joybus.c"
        issue: "283-295 (timeout and absent branches)"
      - path: "core/t64_save.c"
        issue: "224-225 (invalidations)"
      - path: "scripts/check-rsp-no-hle.py"
        issue: "pins t64_pi_complete but not t64_save_sram_dma_apply or t64_si_complete_event"
    missing:
      - "one control per behavior with an accepted baseline in the same setup: latch rewritten between start and completion; a non-0xFF fill (0xA5) in every unanswered frame; a coverage mark and an LL link over the destination cleared after each DMA and unchanged outside it; spec sentences for the invalidation; guard pins or token checks for the two bodies"
  - id: VG-06
    class: non-blocking
    truth: "EEPROM command lengths are exact and the SRAM DMA span ends at the window end (D-18, D-21)"
    status: partial
    reason: "Two boundary rows are absent. A read with a receive count above 8 is not in the unanswered-row table, so classifying (0x04, 2, rx >= 8) as valid survives all 5 save tests. The SRAM DMA refusal table has one overshoot row of 8 bytes, so a span check that tolerates two bytes past the window survives (the apply-time check keeps it memory safe)."
    artifacts:
      - path: "core/tests/test_save.c"
        issue: "unanswered_rows (1098-1112) and the window overflow row (701)"
    missing:
      - "read with rx 9 and a DMA ending exactly at the window end (accepted) plus a DMA two bytes past it (refused)"
  - id: VG-07
    class: non-blocking
    truth: "Every option pair the specs define as refused is refused by the real supervisor and pinned by a test; every other pair is stated"
    status: partial
    reason: "The code agrees with the plans on every defined pair (my own probe: SAVE-IN with REPLAY-REC, SAVE-OUT with REPLAY-PLAY, REPLAY-PLAY with RAW, and SAVE-OUT or REPLAY-PLAY without the pair each end in a usage refusal at guest tick 0), but nine of those refusals have no test, the synthetic-mode refusal of the Phase 9 families has no test, and --save-import-fd has no direct runner test. Six pairs that combine a family with the custom schedule options are accepted by the code and stated nowhere. The refusal of replay with the save flags is what makes the replay save_init image form unreachable in a supervised run, so that stated limit rests on an unpinned refusal."
    artifacts:
      - path: "runner/t64_supervisor.c"
        issue: "546-681 validators and 995-1021 synthetic branch"
      - path: "runner/tests/test_replay.c"
        issue: "case_flag_combinations crosses no save flag with a replay flag"
    missing:
      - "tests for the nine pairs, a synthetic-mode row, a direct --save-import-fd row, and spec sentences for the INPUT combinations (R-01 to R-04 of the drift report)"
  - id: VG-08
    class: non-blocking
    truth: "The D-32 continuation set (one-shot and 1, 7, 31 and 4,096 tick partitions) runs for every boundary state in the qualified commands"
    status: partial
    reason: "Twelve of sixteen boundary rows run a single partition size in the gate, hosted and ASan presets, the instruction-count mid-tick stop row runs only in the full matrix, and no qualified native step sets T64_STATE_MATRIX_FULL. I ran the full matrix once: unit.core.state.import passes (101.8 s against about 30 s sampled), so the omitted partitions hold today; the claim must say sampled, or the full matrix must be registered."
    artifacts:
      - path: "core/tests/test_state_import.c"
        issue: "3096-3119 partition selection and 4137 mid-tick case"
    missing:
      - "register the full matrix in a recorded native step or name the sampling in the CORE-04 claim"
  - id: VG-09
    class: non-blocking
    truth: "The hostile-import evidence tooling reaches what the specs say it reaches (D-41, D-45)"
    status: partial
    reason: "The seeded campaign mostly refuses at the container level: at the gate default all 12 iterations end in reasons 8, 12, 19, 21 or 24, and at the sanitizer setting 117 of 120 refuse and only 6 reach a field or later reason (measured here). The tracer mutates the second observer value (rule INDEX_LT) while the inventory row it is named after is the first (LEN_LE) and it compares the reason only. The field-use audit matches a member after a dot only when no identifier precedes it (scripts/check-state-field-use.py:217-219), so nested accesses are invisible, and its one-row-per-name mapping cannot express two different bounds (VG-02). An argument refusal with a report span inside the machine writes the report into the machine (core/t64_state.c:2408-2445 against 2934-2936), against the stated rule."
    artifacts:
      - path: "scripts/check-state-field-use.py"
        issue: "217-219 lookbehind"
      - path: "core/tests/fuzz_state_import.c"
        issue: "structural mutants always end in TOTAL_LENGTH or TRAILER_SHA256"
    missing:
      - "allow dot-after-identifier and add a nested-access seed, map the new members and require per-site rules; header-aware campaign with a minimum count of field-level refusals; compare the rule in the tracer; check the report span before any early return"
  - id: VG-10
    class: non-blocking
    truth: "A failed publication publishes nothing, and a runner-published state can be imported by the runner into a record its own checker accepts"
    status: partial
    reason: "After the no-replace rename commits, a failing directory fsync or post-rename identity check returns failure while the leaf stays published and the stage directory stays (core/t64_artifact_publisher.c:560-578; abort is a no-op once committed). Separately a state exported by the runner at authored completion always re-imports into a record that the checker refuses, so the supervised resumed run ends in the closed report-error record and never reports pass (runner/tests/test_state_runner.c:1727-1775); the equality evidence through the runner is for a state produced by the core API."
    artifacts:
      - path: "runner/t64_artifact_publisher.c"
        issue: "560-578"
      - path: "runner/t64_real.c"
        issue: "finalize branch for a resumed run (1976-1992)"
    missing:
      - "make the failure contract true (remove the leaf if still the held inode, always remove the stage, probe directory fsync before the guest) or change the spec; give a resumed run its own accepted closed record; state in the CORE-04 claim that the runner shows the mechanism only"
  - id: VG-11
    class: non-blocking
    truth: "The specs and the validation record describe the shipped behavior"
    status: partial
    reason: "docs/spec/result-protocol.md:560-563 still says only the 30-byte hello transcript can pass; docs/spec/si-pif-controller.md:26-29 and 112-115 still say channel 4 is absent for every profile and only channel 0 has a device; docs/spec/machine-contract.md names combiner, blender and coverage members as hashed for the family (see VG-01); 09-VALIDATION.md still carries status draft, nyquist_compliant false, all 25 rows pending and an unchecked sign-off although every row resolves."
    artifacts:
      - path: "docs/spec/result-protocol.md"
        issue: "560-563"
      - path: "docs/spec/si-pif-controller.md"
        issue: "26-29, 112-115"
      - path: ".planning/phases/09-controller-save-media-and-replay/09-VALIDATION.md"
        issue: "frontmatter and status column"
    missing:
      - "rewrite the three passages from the code; update the validation record at phase completion"
deferred: []
behavior_unverified_items:
  - truth: "A write to SI_DRAM_ADDR while a DMA is active changes only the latch; the transfer completes at the address latched at its start (docs/spec/si-pif-controller.md register table)"
    test: "start RD64B with the latch at A, rewrite the latch to B before the completion tick, complete"
    expected: "the image is at A, RDRAM at B is untouched, the register reads B"
    why_human: "a completion that reads the live latch (core/t64_si.c:311) passes 13 related tests, so no test sees an immutability invariant by presence"
  - truth: "A joybus timeout or absent answer writes no receive data; the fill bytes stay as the guest wrote them (docs/spec/si-pif-controller.md and docs/spec/save-media.md)"
    test: "frames whose receive area is filled with 0xA5 sent to a device that times out and to an absent channel, on the controller and on channel 4"
    expected: "the receive bytes still read 0xA5 and only the count byte carries the flag"
    why_human: "every frame in the tests, ROMs and oracles uses a 0xFF fill, so a memset of 0xFF in the timeout branch is invisible"
  - truth: "The SI RD64B completion and the SRAM-to-RDRAM DMA clear the CPU LL link and the RDP coverage marks over the overwritten span (present at core/t64_si.c:313-316 and core/t64_save.c:224-225)"
    test: "LL, DMA over the linked word, SC; coverage mark written before the DMA"
    expected: "the SC fails and the mark reads clear over the span and unchanged outside it"
    why_human: "deleting both calls passes 10 related tests and the no-HLE guard; no spec sentence states the effect"
  - truth: "Every option pair the plans define as refused is refused by the real supervisor, including replay with the save flags (the reason the replay save_init image form is in-process only)"
    test: "supervisor runs with each of the nine untested pairs and with synthetic mode and the Phase 9 families"
    expected: "a usage refusal at guest tick 0 for each"
    why_human: "my probe shows the refusals exist, but dropping any of the nine validators fails no test"
coincidental_reliance_items:
  - truth: "A recorded input replay reproduces the original run's digests (SC3, CORE-05)"
    reason: fixture-only
    harden: "the replay-mix guest uses no RDP combiner or coverage state, so the digest blind spot of VG-01 never matters to its equality; once VG-01 is decided, add an RDP-active replay checkpoint so the equality is exercised on the state the digest used to miss"
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
    verdict: "not violated (rights scan exit 0, findings=0; manifest rows reviewed by me)"
    authoritative: "command part yes, manifest review no"
    flag: "unverified-prohibition: human review recommended for the manifest row review (not an acknowledgment gate)"
  - id: "09-03.1-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; every input is the authored pad-poll fixture and the cleared openIPL3 asset"
    verification: "command: sh scripts/rights-scan.sh --tree (exit 0, no P- or R- finding)"
    verdict: "not violated (exit 0, findings=0)"
    authoritative: true
    flag: "none"
  - id: "09-04-P1"
    statement: "No ROM, save, PIF/IPL/firmware image, 64DD media, hardware measurement or oracle is used; every EEPROM and SRAM content is an authored byte pattern"
    verification: "command: sh scripts/rights-scan.sh --tree plus the manifest row review"
    verdict: "not violated (exit 0, findings=0; no tracked save, state or replay extension)"
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
    verdict: "not violated (exit 0, findings=0; tie and timing text in the specs is labeled named software-profile policy)"
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
    verdict: "not violated (git status clean; no tracked blob extension)"
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
**Verified:** 2026-11-20T09:50:00Z
**Status:** gaps_found
**Re-verification:** No, initial verification (the one gaps-only re-verification over the final tree follows the fix cycle)
**Verified source:** HEAD `2bac6f6`, tree clean (`git status` empty before and after). The digest above is the pre-fix digest over the 110 listed files plus the 24 PLAN and SUMMARY files the fingerprint command adds; it goes stale on any edit to a covered file.

## Verdict

The goal is not achieved yet. Criteria 1, 2 and 4 and the replay half of criterion 3 hold, and each is pinned by tests that fail under my own out-of-tree mutants. The state half of criterion 3 does not: the exported canonical state omits behavior-bearing RDP state, so "export, import, digest equal" is true while the restored machine continues differently, and the importer accepts an in-flight triangle render without the bounds the rasterizer relies on. A third blocking gap stops the planned native qualification from ever completing. Eight non-blocking gaps and four present-but-behavior-unverified truths are listed; none is softened because a fix cycle follows.

Starting hypothesis was that tasks were completed and the goal missed. Criteria 1, 2 and 4 survived that hypothesis with evidence below; criterion 3 did not, and I reproduced the loss rather than adopting the review's account (two independent reproductions for VG-01, the acceptance half of VG-02, a reproduction of the D-44 defect, and confirmations of the other review and audit findings I tested, named per gap).

## What I ran (independent of the SUMMARY files)

- A clean out-of-tree scratch tree built from `git archive HEAD` with the gate preset flags (Release, `-Werror`), the repository's pinned fixtures copied in; its fixture ROMs are byte-identical to the repository's 26 `.z64` files. In it:
  - the 23 CTest names mapped by `docs/evidence/phase09-success-criteria.md`: all pass (the k5 anchors test needs a git repository, so it was re-run in the real tree and in a scratch repository: pass);
  - the full CTest inventory: 224 names, 219 pass; the five that fail there (`harness.base-run`, `runner.cpu-diagnostic`, `integration.rsp-rom-smoke`, `harness.rdp-vi-qualification`, `harness.phase09-qualification`) depend on repository history, tags and untracked assets that a scratch copy lacks, and all five pass in the real tree's gate build;
  - 46 out-of-tree source mutants applied by my own script (tables below and the VG-05 and VG-04 reproductions) with the same flags, each applied to the scratch copy only, built, run against the related names and reverted; and the repository's own `scripts/k5-mutants.py --run` from a scratch git repository (seven mutants KILLED, the equivalent control SURVIVED, the non-compiling control INVALID).
- Scratch C programs linked against the HEAD core (and an ASan/UBSan build of it) for VG-01 and VG-02, and a scratch D-44 row added to a copy of `core/tests/test_si.c`.
- `unit.core.state.import` with `T64_STATE_MATRIX_FULL=1` (passes, 101.8 s), the campaign counts, the supervisor option-pair probe, and `unit.core.state.import` under ASan/UBSan with the registered sanitizer environment (VG-03): passes with no sanitizer report (18,527 checks, 0 failures) but took 1,263 s wall and 1,240 s CPU on a machine that other jobs kept at a load average above 20 for much of the run, so the absolute figure is inflated; the project's own retained ASan log records the same test at 631.80 s, which is also above the 600 s wrapper.
- Repository checks, all read-only: `sh scripts/rights-scan.sh --tree` (findings=0), `sh scripts/pii-scan.sh --tree` (findings=0), `sh scripts/check-claims.sh` (errors=0), `sh scripts/check-docs.sh`, `sh scripts/check-test-inventory.sh --preset gate` (PASS, 224; the 45 `PHASE09_TESTS` names are all registered, no ASan-only Phase 9 name), `python3 -B scripts/check-phase09-cases.py --selftest`, `sh tools/lint/core-lint.sh`, `sh scripts/check-ci-yml.sh`, `python3 -B scripts/check-state-field-use.py --check` (findings=0), `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` (valid; blocked), `git ls-files` for ROM, save, state and replay extensions (empty), and a debt-marker scan of the 105 source-side files (no TBD, FIXME or XXX; the only hits are the audit script's own placeholder rule).

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
| --- | --- | --- | --- |
| 1 | SC1 (SAVE-01): a timed-input script drives the standard controller over SI/PIF joybus and an authored ROM reports the expected button states at the expected guest times | VERIFIED | D-03 chain: strictly-after latch `core/t64_si.c:46-92` (a value stamped at T is visible from T+1), sampled at the execute event's own tick `:266-289`; joybus parse, response and timing `core/t64_joybus.c:47-105,136-171,189-204`, low button byte masked `:33`; input event to latch, port 0 only `core/t64_observe.c:86-129`; authored ROM `fixtures/pad/poll.S`; independent oracle `fixtures/pad/poll_oracle.py:51-85` (standard library only); `runner.pad-poll` (`runner/tests/test_pad_poll.c:649-731`) and `controls.real.pad-wrong-value` (same-setup shifted script, wrong ROM, replacement refused). Mutants in the SAVE-01 table below all fail named tests, including a one-tick shift in three forms. Ports 1 to 3 are refused by `t64_machine_queue_input` (`:110`); channels 1 to 3 and channel 4 on a profile without EEPROM answer absent (`core/t64_joybus.c:142-151`, killed mutant M7). Limit, not a gap: the poll start ticks are measured in process and E is that start plus the literal 16,971-tick handshake (`test_pad_poll.c:449-455`), so "expected guest time" is relative to the first poll start; the ROM's Count witness only shows poll spacing. |
| 2 | SC2 (SAVE-02): EEPROM 4 and 16 Kbit (command lengths, block rules, busy window) and SRAM 32 KiB (window, DMA) work only when the profile selects them explicitly | VERIFIED (two boundary rows missing, VG-06) | Selection is `profile.save_kind` and nothing else: `core/t64_internal.h:1165-1174`, `core/t64_save.c:17-28,73-150`, create-time sizing `core/t64_machine.c:2510-2528`, four ids `:1044-1135`; no ROM header, title or game-code read exists (`git grep save_kind`). Exact triples, 4 Kbit mirror `block & 0x3F`, busy compare and deadline from the execute tick: `core/t64_save.c:73-97,121,138-149`. SRAM window carve-out `core/t64_bus.c:105-110`, word access `core/t64_save.c:158-204`, PI spans `core/t64_pi.c:57-60`, effect at the completion event `core/t64_pi.c:295-312`. Tests: `unit.core.save.device` (12 cases), `runner.save-fixtures`, `controls.real.save-wrong-byte`, oracle `fixtures/save/save_oracle.py`. Of the 15 non-equivalent save mutants, 13 failed named tests (SAVE-02 table); two survive (VG-06). |
| 3 | SC2 (SAVE-04): unknown or unsupported device requests are refused with defined postconditions | VERIFIED (the no-data rule of the wire path is truth 12) | `core/t64_save.c:259-279` (rows R3 to R6 order), `:289-313` (info, K19 order), tests `refusals` (`core/tests/test_save.c:1869-1980`) and `refusal-matrix`, `no-caller-read`, `export-refusals` (`core/tests/test_save_transfer.c`); unknown-kind mutant killed by two names. Wire level: every non-exact triple answers 0x40 and a profile without EEPROM answers 0x80 (`test_save.c:1098-1147`). |
| 4 | SC2 (SAVE-03): save bytes round-trip through import and export with endianness handling, and a refused import leaves the digest unchanged | VERIFIED | RAW, SWAP16, WORDREV32 `core/t64_save.c:323-349`; export and import in D-24 order `:359-457` (every check precedes the first caller byte read; zero length constructs no span); tests `transform-vectors`, `import-export-roundtrip`, `refusal-matrix`, `in-flight-real` of `unit.core.save.import-export`, `runner.save-roundtrip` (publisher, wrong-format control), oracle `fixtures/save/roundtrip_oracle.py`. Mutants: import copies before the busy refusal (killed), WORDREV32 lane (killed by two names), format and kind errors (killed). |
| 5 | SC3 (CORE-04): exporting and re-importing machine state reproduces the canonical digest exactly, so the restored machine continues as the original | FAILED | Digest equality holds for every state the v25 walker covers, but the walker leaves out the RDP combiner block and the memory-coverage table that the family computes with: see VG-01 and the reproductions below. The importer zeroes both. Round trip, idempotence and continuation-partition cases pass only on states that avoid them (`docs/spec/state-container.md:638-650`). |
| 6 | SC3 (CORE-05): a recorded input replay bound to profile, build and content hash reproduces the original run's digests | VERIFIED (reliance note) | Closed canonical format and bounded parser `runner/t64_replay.c` (62 mutation rows and a 400-flip sweep in `runner/tests/test_replay.c`, counts per the review; the suites pass here), bind in fixed order profile, build, content, save_init `runner/t64_replay.c:969-1021`, player `:1423-1495` (input at `event_tick`, checkpoint and end digests compared), `unit.replay.suite` and `runner.replay-mix`; the original run's digests are goldens from a reference path that does not use the recorder (`runner/tests/replay_mix_golden.h:1-40`), the input script and transcript from `fixtures/replay/mix_oracle.py`. Five replay mutants killed by both names (replay table below). Stated limit confirmed: the `save_init` image form is verified in process only (`docs/spec/replay.md:54-57,291`, `runner/tests/test_replay.c:1732-1806`). |
| 7 | SC4 (CORE-02): CPU, RSP, RDP, VI, AI, PI, SI and MI activity is ordered by the same guest-time scheduler, with coincident-event and partition/interleave checks, and the quantum stays part of profile identity | VERIFIED | One ordering function `core/t64_scheduler.c:10-13` (tick, then reservation sequence); every reservation site goes through it (`git grep t64_schedule_reserve`: AI, CPU, exception, input, PI, RDP, RSP, SI, SP, VI); MI has no events and is set synchronously by its producers' handlers, which the spec states. Closed (device, kind) ledger and 210 ordered pairs in both insertion orders `core/tests/test_sched_ledger.c:409,750`; coincident rows, partition, interleave, resource-limit resume, MI producers, Count wrap in `core/tests/test_sched_matrix.c:955-3193`. Quantum: validated to 1 and ratio 1:1 for descriptor 12 `core/t64_machine.c:1730-1738`, in the profile hash `:1827`, `unit.core.profile.quantum-identity`. The k5 driver kills all seven mutants and the scheduler mutants below (consistently wrong ranks included) fail named tests. Evidence class is stated by the project: regression pins and self-comparison, so a tie that is wrong relative to hardware would pass; every tie is labeled named policy. |
| 8 | An accepted state import cannot cause out-of-range access, unbounded loops or undefined arithmetic (D-30, D-41; header contract of `t64_machine_state_import`) | FAILED | See VG-02. Container parsing, caps, checked arithmetic, closed handler table, refusal atomicity and scratch cleanup hold (mutants on the trailer, declared digest and busy bound killed; 6,000-trial sweeps by the review found no refusal-atomicity failure), but the triangle render rule is insufficient. |
| 9 | The frozen qualification route can complete (row 9-V25, D-40) | FAILED | See VG-03. |
| 10 | Every option pair the plans define as refused is refused and pinned | PRESENT_BEHAVIOR_UNVERIFIED | Code agrees with the plans (my probe, VG-07); nine refusals have no test. |
| 11 | SI active-descriptor immutability (a latch write during a DMA does not move it) | PRESENT_BEHAVIOR_UNVERIFIED | Present at `core/t64_si.c:209,311`; mutant survives 13 related tests (VG-05). |
| 12 | Timeout and absent joybus answers write no receive data | PRESENT_BEHAVIOR_UNVERIFIED | Present at `core/t64_joybus.c:283-295` and `core/t64_save.c:113-117,129-132`; 0xFF-fill mutant survives (VG-05). |
| 13 | SI RD64B completion and SRAM-to-RDRAM DMA clear the LL link and the RDP coverage marks over their span | PRESENT_BEHAVIOR_UNVERIFIED | Present at `core/t64_si.c:313-316`, `core/t64_save.c:224-225`; deleting both passes 10 related tests and the guard (VG-05). |

**Score:** 6/13 truths verified (4 present, behavior-unverified; 3 failed). A truth that rests on a state transition or an ordering or cleanup invariant counts as verified only where a named test failed under a mutant of it.

### Mutation evidence (out-of-tree, HEAD, gate flags, each mutant reverted)

SAVE-01 (tests run: `runner.pad-poll`, `controls.real.pad-wrong-value`, `unit.core.si.device`, `unit.core.joybus.parser`, `unit.core.input.latch-partition`, `unit.core.si.atomic-queue-full`, `unit.core.cpu.si-store-budget`):

| Mutant | Result |
| --- | --- |
| latch value visible at T instead of T+1 | FAIL: runner.pad-poll, controls.real.pad-wrong-value, unit.core.input.latch-partition |
| latch visible at T+2 | FAIL: the same three and unit.core.si.device |
| execute event samples at E-1 | FAIL: the same four |
| controller high button byte takes the low byte | FAIL: five names |
| x and y swapped | FAIL: five names |
| channel 1 answers as a controller | FAIL: four names |
| joybus byte ticks 2000 to 2001 | FAIL: five names |
| read tail 4096 to 4097 | FAIL: unit.core.joybus.parser, unit.core.si.device |
| low-byte mask 0x3F to 0xFF | FAIL: unit.core.joybus.parser only (the runner scenarios carry no masked bits) |
| queue_input accepts ports above 0 | FAIL: unit.core.input.latch-partition |

SAVE-02, SAVE-03, SAVE-04 (tests run: `unit.core.save.device`, `unit.core.save.import-export`, `runner.save-fixtures`, `runner.save-roundtrip`, `controls.real.save-wrong-byte`):

| Mutant | Result |
| --- | --- |
| 4 Kbit mirror mask 0x3F to 0x7F | FAIL: three names |
| 16 Kbit also mirrored | FAIL: unit.core.save.device, runner.save-fixtures |
| busy compare `<` to `<=` | FAIL: unit.core.save.device |
| busy write not refused (data commits) | FAIL: three names |
| busy write extends the window | FAIL: unit.core.save.device, runner.save-fixtures |
| busy window 375000 to 374999 | FAIL: unit.core.save.device, unit.core.save.import-export |
| EEPROM blank fill 0xFF to 0x00 | FAIL: four names |
| 16 Kbit id byte | FAIL: unit.core.save.device, runner.save-fixtures |
| SRAM DMA directions swapped | FAIL: three names |
| SRAM word lanes swapped | FAIL: runner.save-fixtures, unit.core.save.device |
| import copies before the busy refusal | FAIL: unit.core.save.import-export |
| WORDREV32 wrong lane | FAIL: unit.core.save.import-export, runner.save-roundtrip |
| unknown device kind accepted | FAIL: unit.core.save.device, unit.core.save.import-export |
| EEPROM read classification `rx_count >= 8` | SURVIVES all five (VG-06) |
| SRAM DMA span check tolerates two bytes past the window | SURVIVES all four (VG-06) |
| SRAM word bound moved past the window in `core/t64_save.c` only | equivalent at the public boundary: the bus carve-out `core/t64_bus.c:105-110` refuses first, and the apply-time check keeps memory safe |

CORE-02 (tests run: `unit.core.sched.matrix`, `unit.core.sched.ledger`, `unit.core.sched.characterize-fifo`, `unit.core.profile.quantum-identity`, `unit.core.scheduler`, `unit.core.profile*`): the seven mutants of `scripts/k5-mutants.py` KILLED in a scratch git repository with the equivalent control SURVIVED and the compile control INVALID; tie by device id ascending, by device id descending and by kind then sequence all FAIL `unit.core.sched.characterize-fifo` and `unit.core.sched.matrix` (the kind rule also `unit.core.scheduler`); quantum omitted from the profile hash FAILS five names; descriptor 12 accepting any quantum and an unconstrained ratio denominator each FAIL `unit.core.profile.quantum-identity` and `unit.core.profile.v12-identity`.

CORE-05 (tests run: `unit.replay.suite`, `runner.replay-mix`): checkpoint digest never compared, input submitted at `at` instead of `event_tick`, build id not compared, content hash not compared, profile digest not compared: each FAILS both names.

CORE-04 (tests run: `unit.core.state.import`, `unit.core.state.export`, `runner.state-export`): declared digest never checked FAILS `unit.core.state.import`; `busy_until` upper bound removed FAILS `unit.core.state.import`; whole-blob trailer never checked FAILS `unit.core.state.import` and `runner.state-export`.

### Reproductions

VG-01, first (guest-reachable state through the DP registers, stem id, authored IPL3 asset as ROM): after SetColorImage, SetScissor, SetOtherModes, SetPrimColor, SetEnvColor and SetCombine the source holds `combine_mode_valid` and `environment_color_valid`. Export 6,351,551 bytes; import into a fresh admitted peer returns status 0, and the two digests are equal. The peer holds `combine_mode_valid` 0 and `environment_color_valid` 0. The same four-word triangle sent to both: the source `triangle_count` 1 and refusal 0; the peer `triangle_count` 0 and refusal 5; the framebuffer bytes and the digests then differ.

VG-01, second (authored fixture, no hand-poked state): `audio-pcm.z64` (4,728 bytes) on the stem id, advanced to guest tick 540,000 with packets drained, exported (6,352,351 bytes) and imported into a fresh peer: status 0, digests equal, and 84 bytes of the RDP memory-coverage table differ between source and peer (the review's figure is also 84). The digests differ at the first continuation checkpoint.

VG-02 (ASan/UBSan scratch build, state taken from a real mid-triangle render and moved to row 1,500 with valid hashes, coverage array height 1,024): `import status=0`, peer `render_y` 1500, `render_triangle` 1. The same acceptance holds for hostile shade-triangle words with slopes of 0x7FFFFFFF. No sanitizer trap occurred in my attempt (the zeroed combiner stops the render before the coverage write, and my row span was empty), so the signed overflow at `core/t64_rdp_raster.c:394` and the out-of-array write are carried from the review and audit, not confirmed here.

D-44 (VG-04): a copy of `case_cpu_store_budget` with a `swr $t1, 7($t0)` row: the accepted room passes; with room for two events the store retires (4 against 3), the queue count is 16 against 14, and the SI state and digest changed, so the refusal is not atomic.

### Confirmation items from the brief

| Item | Verdict | Evidence |
| --- | --- | --- |
| `busy_until` bound (plan cross-check M5) | CONFIRMED present and tested | `core/t64_state.c:2013-2040` bounds the deadline by the blob tick plus the profile window (zero for SRAM); `docs/spec/state-mutants.txt:49` carries the row `busy-until-future`; removing the rule fails `unit.core.state.import` (mutant above). The save-side bound is the checked sum at `core/t64_save.c:142-147`. |
| Replay `save_init` reachability (M6) | CONFIRMED as a stated limit; the refusal that creates it is unpinned | The image form is in process only: `docs/spec/replay.md:54-57,291`, CONTEXT amendment "Replay save_init", `09-07-SUMMARY.md` lines 114 and 254, claim block 10 prose planned in `09-11-PLAN.md`, in-process identity-mismatch rows `runner/tests/test_replay.c:1732-1806`. The real runner refuses replay with the save flags (my probe: usage at guest tick 0) but no test pins it (VG-07). |
| Hosted rows for the save fixtures (L7) | CONFIRMED as a stated limit | `HOSTED_ROWS` in `scripts/qualify-phase09.py:103` holds five rows, all from `runner.pad-poll` and `controls.real.pad-wrong-value`; the hosted pair covers those only. The eeprom, sram, roundtrip and replay ROMs run in the ordinary hosted `./scripts/gate.sh` (pass or fail only) and are counted from the native normal-gate JUnit, as `09-11-PLAN.md` line 140 states for claim blocks 1 to 5. No claim block exists yet to carry the sentence; it is a 09-11 obligation. |
| Option-pair matrix (R-01 to R-04, R-06) | Confirmed by my probe, with VG-07 | Defined pairs agree with the code; nine refusals, the synthetic-mode refusal and `--save-import-fd` have no test; six INPUT combinations are stated nowhere. |
| Mid-tick export and import row (D-36) | PRESENT-GREEN, sampled in the gates | `case_mid_tick_resume` (`core/tests/test_state_import.c:4137`) runs three of four stop rows by default (the instruction-count row needs the full-matrix variable); the full matrix passes here (VG-08). |
| `write_generation` decision (PD-3) | AGREE, challenged | One saturating increment as the last write of the state commit (`core/t64_state.c:2867-2870`) and in the save import (`core/t64_save.c:453-455`); outside the digest and the container; a refused import never touches it. Duplicate expression is cosmetic (R-05). Residual: the value restarts at 0 on replacement, so a host must also track an admission epoch (review IN-03); not a defect of the criteria. |

### Required Artifacts

| Artifact | Expected | Status | Details |
| --- | --- | --- | --- |
| `core/t64_si.c`, `core/t64_joybus.c`, `core/t64_observe.c` | SI registers, DMA, joybus, latch | VERIFIED | real logic; wired from `core/t64_bus.c` and the scheduler; read in full |
| `core/t64_save.c`, `core/t64_pi.c`, `core/t64_bus.c` | EEPROM, SRAM, import and export | VERIFIED | real logic; wired through the joybus channel 4 dispatch, the bus carve-out and the PI completion |
| `core/t64_scheduler.c`, `core/t64_machine.c` | one scheduler, family profile identity | VERIFIED | one ordering function; quantum validated and hashed |
| `core/t64_digest.c`, `core/t64_state.c` | v25 walker, container export and import | PARTIAL | complete for the members it carries; omits the RDP members of VG-01; importer rule gap of VG-02 |
| `runner/t64_replay.c`, `runner/t64_supervisor.c`, `runner/t64_real.c`, `runner/t64_artifact_publisher.c` | replay, option surface, runner, publisher | VERIFIED with VG-07 and VG-10 | wired supervisor to child to publisher; failure contract after the rename is incomplete |
| fixtures `pad`, `save`, `replay`, `phase09`, `state` oracles and ROM sources | authored guests and independent oracles | VERIFIED | oracles import the standard library only; 15 manifest rows, all `build-only`; no tracked ROM, save, state or replay file |
| `scripts/qualify-phase09.py`, `scripts/qualify-rdp-vi.py` | frozen qualification route | BLOCKED for the native stage | fails closed deterministically (VG-03) |
| `docs/claims/phase09.md`, `docs/evidence/phase09-success-criteria.md` | claim skeleton and draft map | VERIFIED as drafts | only `t64-claim-example` fences; the map's named tests all exist and pass; its "Limits" section lists the three limits the review turned into VG-01, the runner limit (VG-10) and the save_init limit |

### Key Link Verification

| From | To | Via | Status | Details |
| --- | --- | --- | --- | --- |
| input rows (`t64.input`) | SI sample | `t64_machine_queue_input` → INPUT event → latch → execute event sample | WIRED | killed mutants M1 to M3 and M10 show the change reaches the transcript |
| ROM frame | EEPROM bytes and back | WR64B parse → RD64B execute at event tick → `t64_save_channel4_execute` → completion copy | WIRED | save mutants change the ROM transcript through the real runner |
| profile id | save device | `profile.save_kind` → `t64_has_*_save` → bus and joybus dispatch | WIRED | no ROM-derived selection exists |
| machine | state container | digest sink sections → exporter; sections → readers → commit | WIRED except the RDP members | VG-01 |
| replay file | machine | parse → bind → `t64_machine_queue_input` at `event_tick` → digest compare | WIRED | five mutants killed |
| every device | scheduler | `t64_schedule_reserve*` | WIRED | ten source files reserve events, one ordering function |

### Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
| --- | --- | --- | --- | --- |
| pad-poll transcript | controller bytes in PIF RAM | latch sample at the execute tick | yes (button, axis and mask mutants change it) | FLOWING |
| EEPROM and SRAM transcripts | block bytes, busy flag | storage and deadline in `machine->save` | yes | FLOWING |
| state blob | RDP combiner and coverage members | not read by any section | no | DISCONNECTED (VG-01) |
| replay checkpoint digests | canonical digest | `t64_machine_digest` | yes, for the members the walker hashes | FLOWING (blind members of VG-01) |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| --- | --- | --- | --- |
| 23 mapped CTest names | `ctest -R` over the names in the criteria map, scratch gate build | 22 pass; `repo.sched-k5-anchors` passes once a git repository exists | PASS |
| Full inventory | `ctest -j4` in the scratch build | 219 of 224 pass; the 5 history-dependent names pass in the real tree | PASS |
| k5 mutants | `python3 -B scripts/k5-mutants.py --run` in a scratch git repository | 7 KILLED, equivalent control SURVIVED, compile control INVALID | PASS |
| Full state matrix | `T64_STATE_MATRIX_FULL=1`, `unit.core.state.import` | pass, 101.8 s | PASS |
| Validation map | 25 rows, 71 case references resolved against `--list-cases` of the built binaries and every CTest name against `ctest -N` | 0 unresolved | PASS |
| Phase 9 gate names | `PHASE09_TESTS` against `ctest -N` | 45 of 45 registered, no ASan-only name | PASS |
| Option pairs | six supervisor probes (VG-07) | usage refusal at tick 0 for the five refused pairs | PASS (the refusals exist; no test pins them) |

### Probe Execution

No `probe-*.sh` scripts are declared or exist for this phase. Not applicable.

### Requirements Coverage

| Requirement | Source plans | Description | Status | Evidence |
| --- | --- | --- | --- | --- |
| SAVE-01 | 09-02, 09-03, 09-03.1, 09-10, 09-11 | SI and PIF parse joybus and deliver the standard controller from timed logical input | SATISFIED (authored scope) | truth 1; the SWR budget defect VG-04 and truth 11 sit beside it |
| SAVE-02 | 09-02, 09-04, 09-10, 09-11 | EEPROM 4 and 16 Kbit with busy window and block rules, and SRAM, explicitly selected | SATISFIED (authored scope) | truth 2; VG-06 |
| SAVE-03 | 09-02, 09-05, 09-10, 09-11 | import and export bytes with endianness handling and a refusal that leaves the digest unchanged | SATISFIED | truth 4 |
| SAVE-04 | 09-04, 09-05, 09-10, 09-11 | unknown or unsupported device requests refused with defined postconditions | SATISFIED at the API; wire no-data rule unpinned | truths 3 and 12 |
| CORE-02 | 09-01, 09-03, 09-03.1, 09-06, 09-10, 09-11 | device activity ordered by the scheduler, quantum in the profile identity | SATISFIED | truth 7 |
| CORE-04 | 09-01, 09-02, 09-08, 09-09, 09-09.1, 09-10, 09-11 | versioned machine state with a canonical digest; restore-then-digest round trip exact | BLOCKED | truths 5 and 8 (VG-01, VG-02) |
| CORE-05 | 09-02, 09-07, 09-10, 09-11 | record and replay timed input bound to profile, build and content hash | SATISFIED (authored scope, reliance note) | truth 6 |

REQUIREMENTS.md maps exactly these seven ids to Phase 9 and every plan declares a subset; no orphaned requirement. Every id is claimed by a plan.

### Decision and amendment coverage

Every D-01 to D-46 and K1 to K19 has an owning plan that names it in its PLAN text (K19 only in the CONTEXT amendment; its behavior is pinned at `core/tests/test_save.c:1966-1980` and `docs/spec/save-media.md:291`) and a named test or recorded process artifact. SUMMARY files do not cite ids, as the brief says. Owner confirmed and evidence, with the decisions whose evidence is weaker than their text marked:

| Ids | Owner | Test or artifact |
| --- | --- | --- |
| D-01, D-43, K1, K2, K12 | 09-01 (09-02 for K12) | `unit.core.profile.predicate-matrix`, `.family-devices`, `.v12-identity`, `repo.si-membership-sites`, `runner.profile-table-id-length`; K1 keeps depth inactive on descriptor 12 and says nothing about the combiner block or the coverage table; D-05's strip-and-compare against v24 carries the same omission forward, which is the root of VG-01 |
| D-02, D-36, K5, K14, K17 | 09-01, 09-06 | `unit.core.sched.characterize-fifo`, `.matrix`, `repo.sched-k5-anchors`; mutants above |
| D-03, D-15, K7 | 09-03, 09-07 | `unit.core.input.latch-partition`, `runner.pad-poll`; replay port rows |
| D-04, D-37, K10 | 09-01 | `unit.core.profile.quantum-identity` |
| D-05 | 09-01 | `unit.core.digest.sink-legacy-identity`, `.v25-reference`, `.v25-strip-differential`; the strip differential inherits the v24 blind spot (VG-01) |
| D-06, D-40, D-46 | 09-03.1, 09-10 | process: tag `phase09-fh`, freeze baseline re-derived by the audit, this verification |
| D-07 | 09-01 | `unit.core.sched.ledger`, case `handler-table-ledger` |
| D-08 to D-14, K16 | 09-03 | `unit.core.si.device`, `unit.core.joybus.parser`, `unit.core.si.atomic-queue-full` |
| D-16, K3, K4 | 09-03, 09-02 | `runner.pad-poll`, `runner.fixture-transcript-pin`, `runner.phase09-admit` |
| D-44 | 09-03 | `unit.core.cpu.si-store-budget`; incomplete for partial stores (VG-04) |
| D-17 to D-23, K19 | 09-04 | `unit.core.save.device`, `runner.save-fixtures`; VG-06 |
| D-24 to D-27 | 09-05 | `unit.core.save.import-export`, `runner.save-roundtrip` |
| D-28, D-29, D-33, K8 | 09-08 | `unit.core.state.export`, `runner.state-export`, `runner.artifact-publisher` |
| D-30 to D-32, D-41, K8 | 09-09, 09-09.1 | `unit.core.state.import`; D-30 and D-41 contract incomplete (VG-02), D-32 sampled (VG-08) |
| D-45 | 09-09 | `harness.state-field-use-audit`; lexical blind spot (VG-09) |
| D-34, D-35, K6, K15 | 09-07 | `unit.replay.suite`, `runner.replay-mix`, `runner.supervisor-argv-bound`; VG-07 |
| D-38, D-39, K9, K18 | 09-02 | `runner.artifact-publisher`, `harness.fsize-artifact-real-fault`, `runner.fixture-descriptor`; manifest rows |
| D-42, K11, K13 | 09-02, 09-11 | `harness.phase09-qualification` (claim skeleton cases); inventory selftest cost recorded |

No locked decision lacks an owner and a test, so none is blocking on that rule alone.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| --- | --- | --- | --- | --- |
| none in the 105 source-side files | | TBD, FIXME, XXX, TODO, HACK, PLACEHOLDER | | the only hits are the field-use audit's own placeholder-rejection rule and one doc sentence naming it |

Stubs: none on a rendering or data path. The weakest artifact is a test that cannot fail on a documented behavior, listed under VG-05 and VG-06.

### Human Verification Required

None is required for the status. The decision VG-01 needs (carry the members, keep them outside the digest, or refuse) is a routine reversible decision under `.planning/AUTONOMY.md` section 2, not an owner stop; no rights, authority, physical, paid, publication or destructive stop is crossed. The 13 prohibition statements above are reported under the autonomous soft gate with non-authoritative judgment verdicts and are not an acknowledgment gate; the Phase 9 re-pins of the Phase 7 oracle request are identity-only and the owner approval they cite is recorded in `.planning/STATE.md` (line 129), the authority record still reads blocked and `scripts/check-rdp-oracle-authority.py --expect-blocked` passes. No human validation is recorded or implied by this report.

### Advisory (New Scope, Unevidenced)

None. This is an initial verification.

### Gaps Summary

Three blocking gaps, in the order they should be fixed together: VG-01 and VG-02 are one change (carrying the combiner block makes the coverage-index hole live), and VG-03 is a data edit to a frozen qualification file that must land before FR. Eight non-blocking gaps follow; VG-04 is a small correctness fix on a locked decision and VG-05 to VG-07 are tests that cannot currently fail. The replay half of criterion 3 and criteria 1, 2 and 4 are achieved for the authored scope the roadmap and claim define, with every timing, tie, busy window and button mapping a named software-profile policy and no hardware or other-emulator statement. The independent review and audit are consistent with everything I reproduced; I confirmed each of their reproducible findings that I tested, and I mark the two consequences of VG-02 that I could not reproduce as carried.

---

_Verified: 2026-11-20T09:50:00Z_
_Verifier: Claude (gsd-verifier)_
