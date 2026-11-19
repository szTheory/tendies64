---
phase: 09-controller-save-media-and-replay
plan: "01"
subsystem: core-identity
tags: [profile-family, descriptor-12, digest-sink, v25-walker, scheduler-ledger, guard-pin-refresh]

requires:
  - phase: 08-audio-interface-and-raw-capture
    provides: the descriptor 11 audio profile whose v24 stream is the strip-and-compare oracle
provides:
  - composite family, descriptor 12 / model 20 / state 25, four registered ids with validated labels, save_kind, peripheral_flags and quantum
  - digest sink seam (hash, buffer, size-only; span classes) with the legacy streams byte-identical
  - v25 sectioned walker equal to the v24 stream once stripped, with an independent framing encoder
  - closed (device, kind) event ledger and pairwise same-tick FIFO pins
  - committed version-membership and reservation site list with a CTest check
affects: [09-02, 09-03, 09-04, 09-05, 09-06, 09-08, 09-09]

actuals:
  tokens: 69190
  tasks: 6
  commits: 14
plan_head_before: 1831c49d423a3f87d90f2ea3a6452f4118ea3039
plan_head_after: 2c8150c99b724fb69c5dc8a50796b5b22336501d

tech-stack:
  added: []
  patterns:
    - "one walker, many sinks (hash, buffer, size query) with span classes for strip-and-compare"
    - "descriptor-version arms in validator and writer; hash-pinned canonical ids at create"
    - "golden-first characterization before any ladder or digest edit"
    - "script-generated membership list checked by a CTest name"

key-files:
  created:
    - core/tests/test_si_profile.c
    - core/tests/test_digest_v25.c
    - core/tests/test_sched_ledger.c
    - scripts/list-membership-sites.py
    - docs/spec/profile-membership-sites.txt
  modified:
    - core/include/tendies64/t64_machine.h
    - core/t64_internal.h
    - core/t64_machine.c
    - core/t64_digest.c
    - core/t64_isviewer.c
    - core/t64_ai.c
    - core/t64_bus.c
    - core/CMakeLists.txt
    - runner/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/check-rsp-no-hle.py
    - scripts/tests/test_rsp_no_hle.py
    - docs/spec/machine-contract.md

key-decisions:
  - "Descriptor 12 is created only under its four registered ids; a descriptor 12 under any other id is refused at create (the family tuple selects ring storage by registered id)"
  - "The AI-device gates in the seven AI functions and decode_region share one helper, t64_has_ai_device; digest paths keep the exact tuple predicates"
  - "The membership generator tokens include the new family names so the F0 list covers the shared AI gates and the create-time ladders"
  - "The ledger observes the queue every tick rather than the trace, because handler-emitted trace records reuse the device ids with a different kind space"

requirements-completed: [CORE-02, CORE-04]

coverage:
  - id: D1
    description: "Composite family descriptor 12 / model 20 / state 25: four ids that look up, hash, serialize, create and admit; validated labels, save_kind, peripheral_flags, quantum and ratio; older versions refuse non-zero save_kind and peripheral_flags at create"
    requirement: "CORE-02"
    verification:
      - kind: unit
        ref: "unit.core.profile.v12-identity, unit.core.profile.quantum-identity, unit.core.profile.predicate-matrix"
        status: pass
    human_judgment: false
  - id: D2
    description: "Digest sink seam with all 17 legacy idle digests and the AI mid-DMA digest unchanged, v25 sectioned stream equal to the v24 stream once stripped across five states, independent reference encoder and four surgery controls"
    requirement: "CORE-04"
    verification:
      - kind: unit
        ref: "unit.core.digest.sink-legacy-identity, unit.core.digest.v25-reference, unit.core.digest.v25-strip-differential"
        status: pass
    human_judgment: false
  - id: D3
    description: "Inherited devices (CPU, RSP, PI, ISViewer, AI window) behave under all four ids as on the Phase 8 profile; shared AI gate"
    requirement: "CORE-02"
    verification:
      - kind: unit
        ref: "unit.core.profile.family-devices"
        status: pass
    human_judgment: false
  - id: D4
    description: "Isolated derived refresh of eight no-HLE body pins with four new mutation seeds that still reject an unauthorized change"
    requirement: "CORE-02"
    verification:
      - kind: unit
        ref: "repo.rsp-no-hle (python3 -B scripts/tests/test_rsp_no_hle.py, 59 seeds)"
        status: pass
    human_judgment: false
  - id: D5
    description: "Closed event ledger and pairwise same-tick FIFO pins; membership site list committed and checked"
    requirement: "CORE-02"
    verification:
      - kind: unit
        ref: "unit.core.sched.ledger, unit.core.sched.characterize-fifo, repo.si-membership-sites"
        status: pass
    human_judgment: false
  - id: D6
    description: "Spec text: corrected digest encodings, v25 framing, composite family, closed ledger, F0 identity record, with the evidence limits stated"
    requirement: "CORE-04"
    verification: []
    human_judgment: true
    rationale: "Wording review against the claim boundary (no depth claim, no hardware claim, no API/ABI freeze) is a judgment; the sentence scan and PII/rights scans are automated"

duration: 36min
completed: 2026-11-19
status: complete
---

# Phase 9 Plan 01: Core identity, digest walker and event ledger Summary

**Descriptor-12 composite profile family with four validated ids, a digest sink seam that leaves all 17 legacy digests byte-identical, a v25 sectioned walker equal to the v24 stream once stripped, a closed event ledger with same-tick FIFO pins, and an isolated, derived refresh of the eight sealed AI/bus guard pins.**

## Performance

- **Duration:** 36 min (wall clock from the plan-base capture, 2026-11-19T14:47:36-05:00, to the final full CTest run)
- **Tasks:** 6 of 6 (Task 1 is the tracer)
- **Commits:** 14 production and test commits measured from the ledger base (`1831c49`)
- **Files:** 5 created, 13 modified (18 changed, 5040 insertions, 594 deletions)

## Accomplishments

- Four registered ids share descriptor 12, model 20, state 25: `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` (38 characters, `save_kind` 0), `...-e4k` (42, 1), `...-e16k` (43, 2) and `...-s32k` (43, 3), `peripheral_flags` 1, `si_policy` `si-hle-joybus-wire-v1`, `save_policy` `save-explicit-eeprom4k16k-sram32k-busy6ms-v1` at final text. Hash, serialize, create and admit work for all four; every refusal (save_kind above 3, peripheral_flags other than bit 0, quantum other than 1, ratio other than 1:1, empty or unterminated label, every single-bit field flip) is reached at its own status with allocator counters unchanged.
- The sink seam (`t64_digest_stream`, `t64_digest_sink`, span classes body, identity, marker, new) routes the existing walker; the 17 pre-family idle digests and the AI mid-DMA digest pass with literals captured before any digest edit and never edited afterwards.
- v25 (`t64.state.si.25`): 32-bit big-endian section ids 1 to 9 ascending, ids 10, 11 and 16 reserved; the stripped stream equals the v24 stream of the Phase 8 profile for idle, AI mid-DMA with a packet in progress, AI after the first packet, a retained video packet and a pending RDP command word.
- Closed ledger of 15 rows (13 live, SI device 10 kinds 1 and 2 reserved) and 156 ordered label-only pairs plus real-device pairs, both insertion orders.
- `scripts/list-membership-sites.py` and `docs/spec/profile-membership-sites.txt`, checked by `repo.si-membership-sites`.
- Spec: corrected digest text (state-domain table v5 to v25, framing, span classes, strip rule), composite family, closed ledger and F0 identity record.

## Task Commits

1. **Task 1 (tracer):** `cfe4d34` generator (chore), `c173a5e` and `77bf968` golden characterization (test), `009c9f1` label declarations (feat), `3720963` sink seam (refactor), `80ac08a` stem wiring with the formal RED (feat), `5347b49` v25 walker GREEN (feat)
2. **Task 2:** `2fc200e` RED tests (test), `a743305` ids, validation, ISViewer arms, spec (feat)
3. **Task 3:** `fcebe14` RED tests (test), `8e3374f` gate switch (feat)
4. **Task 4:** `24be635` isolated pin refresh (chore)
5. **Task 5:** `7abc959` breadth, reference encoder, surgery controls, digest text (test)
6. **Task 6:** `2c8150c` ledger, FIFO pins, membership list, F0 record (test)

Golden-capture precedes the first edit of `core/t64_internal.h`, `core/t64_machine.c` and `core/t64_digest.c` (`c173a5e` and `77bf968` before `3720963` and `80ac08a`).

## Tracer

Task 1 proved one id end to end before expansion. After its commit the full Task 1 verify block was re-run on the committed content and passed, so `Tracer verified end-to-end` and Tasks 2 to 6 expanded from it.

## RED records

**Task 1 formal RED (commit `80ac08a`, differential before the v25 branch existed).** Result kind length mismatch; stripped lengths old 6350080 (Phase 8 profile, v24) and new 6349744 (stem id, still the v23 domain with no audio block); first differing offset 6349744, so the new stripped stream is a prefix of the old and the 336-byte difference is exactly the audio block.

**Task 2 RED (commit `2fc200e`, actual statuses against the Task 1 stem id).** Hash and serialize of a descriptor-12 copy with `save_kind` 4, 5 and 0xffffffff: OK (want INVALID_ARGUMENT, status 0 want 1). Quantum 2 and 3: OK. Ratio 2:1, 1:2, 3:1, 1:3: OK. `peripheral_flags` 2, 3 and 0x80000001: OK. Create of a non-pinned legacy-version copy (skeleton copy, unique id) with `peripheral_flags` 1 and with `save_kind` 1, 2, 3 and 7: OK. Create of a descriptor 12 under an unregistered id: OK. Stem ISViewer window: refused (status 19). The three other ids did not look up yet; those failures are setup, not RED.

**Task 3 RED (commit `fcebe14`).** Under all four ids the AI control write returned status 19 (device refused, want OK), the AI length write returned status 16 (unsupported, want OK) and `t64_audio_info` produced no packet. The CPU, RSP and PI operations already passed because they follow the predicate chain, not the AI gates.

**Task 5.** Started green by design: verification-only breadth over the Task 1 walker, so no failure was manufactured. The controls are the discriminating evidence, plus two throwaway production mutants observed and reverted: retaining the packet identity under v25 (differential kind identity span mismatch) and omitting the machine tick under v25 (length mismatch and the reference encoder section 2 mismatch). These are current evidence, not original RED history.

**Task 6.** Started green by design (a characterization). A throwaway mutant that reversed the equal-tick comparison in `event_precedes` failed 478 checks across the label-only, PI/SP, VI/AI and CPU pair cases, then was reverted.

## Task 4 old and new pins (isolated commit `24be635`)

Cause: commit `8e3374f` replaced the exact-tuple predicate `t64_has_ai_profile` by `t64_has_ai_device` at one site in each of eight sealed bodies. New values were derived: the pre-edit source from the parent of the cause (`git show fcebe14:path`) with the one textual substitution, normalized by the guard's own comparison (blank comments and literals, extract the body, drop whitespace, SHA-256). The pre-edit digest equals the old pin for all eight rows, the substitution count is 1 per body, and the derived digest equals the working-tree digest for all eight rows.

| Row | Old pin | New pin |
| --- | --- | --- |
| `core/t64_bus.c :: decode_region` | `e72b14de8821c660fa08a293d3ee1479f8af8aaad3f83c442f2835938e939fa6` | `69b17e64915f063c8605fd3f28dab8cf1b76beae62378df2633806a548c6a97c` |
| `core/t64_ai.c :: t64_ai_event` | `e7585ee80dd16f5de02f45c6acaf06f2adcbfe58378ea0a6f22df398baeae8cf` | `5efa395e84b7c25dba3edf76f62ae3ff0818cb46f8413bd43ac6968261ed18d0` |
| `core/t64_ai.c :: t64_ai_event_can_dispatch` | `05cd6d94688e436a41bcf3e0b89d2064ac417a8df75762cbf4b6d50b03ca18e0` | `10ac9923f09a41c9d67737981dcc9c68c524484b88ee5aaa8a0b98295f16bd53` |
| `core/t64_ai.c :: t64_ai_event_waits_for_output_capacity` | `7dae7a9d89a0e47b14b2c2517f9b05a4b249bae1e4a4f1d598a0ba30983c8d56` | `89df8b2478ddaa8d42667cec2ea208a00c8d56b6355182341a7714a123a82970` |
| `core/t64_ai.c :: t64_ai_read_register` | `e2bd08a3af82759c4ba065ece2e11aa60848b0044a10fdf4e01f66af9cc02284` | `712bae79d7a46f36cc17b0ee3180ca12e13180cffded853238b2b16ec8893ec9` |
| `core/t64_ai.c :: t64_ai_write_register` | `b9e6112bb4bbe6f1e9a5f3da20e6a099e19547f59c68551ddf1fbc68a2d2c110` | `69511e698a08242dc74befa9e6c11b84a43b22673583f297ec13b470e12ee26a` |
| `core/t64_ai.c :: t64_audio_copy` | `a655c179545c078317e5b95bde272153b6ff34b6c480aae7972a46d0223cc5f1` | `4638a1db12bdb801ff8881108ddfa78771ddb53f03c50f7decca36537b50f2c5` |
| `core/t64_ai.c :: t64_audio_info` | `8f30f9fb3670f82d509a67e499300cb7092f8f06f22df60ea8fdec0a1a977031` | `f1c0ff86b4cff5f5b4900c9183bc4c18ab093554dc898075ff824a397c62a701` |

Between `8e3374f` and `24be635` the guard failed with exactly eight `executable body changed` findings (verified: eight lines, exit non-zero, then nothing pushed). Control: four new seeds (injected effects in `t64_audio_copy` and `decode_region`; the family gate reverted to the exact-tuple predicate inside `t64_audio_info` and inside `decode_region`) are rejected; the seed count equals the declared list (59). Deleting any single new digest row fails the guard with `reviewed normalized body digest is missing` (8 of 8, one finding each). The commit changes `scripts/check-rsp-no-hle.py` by exactly 8 insertions and 8 deletions inside `REVIEWED_BODY_DIGESTS`, and only the two script files.

## K1 STATE follow-up sentence (exact)

The shipped Phase 8 descriptor 11 profile has RDP depth inactive because the depth predicate is guarded by descriptor version 10; Phase 9 preserves this on descriptor 12; no Phase 9 requirement needs depth and no Phase 9 claim mentions it.

The golden predicate matrix pins depth inactive on the AI row and the four family rows and active on the descriptor 10 rows depth-v7, vi-v8 and vi-filter-v9. The spec states no depth coverage for descriptor 11 or 12.

## F0 identity record

- Ids: `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` and the suffixes `-e4k`, `-e16k`, `-s32k`; tuple 12/20/25; `peripheral_flags` 1; `save_kind` 0, 1, 2, 3.
- Labels: `si_policy` = `si-hle-joybus-wire-v1`; `save_policy` = `save-explicit-eeprom4k16k-sram32k-busy6ms-v1`.
- Stream: domain `t64.state.si.25`; section ids 1 identity, 2 machine core, 3 rdram, 4 sp memory, 5 interrupt/PI/SP, 6 scheduler, 7 warm devices, 8 rdp/vi, 9 audio; 10 SI/PIF and 11 save media reserved; 16 observer continuity reserved and outside the digest; span classes body, identity, marker, new.
- Frozen at F0: membership pattern (the listed sites), section ids and framing, id scheme, numeric-field validation. Provisional until Phase 10; no API/ABI freeze.

## Version-membership sites left unchanged (one-line reasons)

- `t64_has_rdp_depth_profile`: stays on descriptor 10 only (K1, no depth claim).
- `t64_has_vi_scanout_v8_profile`: a descriptor 10 tuple; the family gets scanout through `t64_has_vi_scanout_profile` and the v9 chain.
- `t64_has_ai_profile`: the exact Phase 8 tuple, kept for the v24 digest branch and as one half of `t64_has_ai_device`.
- `t64_has_rdp_vi/triangle/texture/combiner/blender/cpu/t1/rdram/rsp_functional/compat` predicates: bodies unchanged; the family propagates through `t64_has_vi_filter_v9_profile`.
- `core/t64_scheduler.c :: vi_event_waits_for_output_capacity` and `core/t64_vi.c :: render_packet`: use `t64_has_vi_filter_v9_profile`, so they include the family by the chain.
- `is_openipl3_profile`, `is_rdram_a_t1_profile`, `is_rsp_functional_profile`, `is_vr4300_openipl3_profile`, `is_vr4300_compat_profile`: legacy descriptors 2 to 4, unaffected.
- `core/t64_digest.c :: t64_digest_stream`: keeps the exact tuple predicates for domain selection (v24 and v25 are separate branches).

Changed sites: `t64_isviewer_active`, `profile_fields_are_valid`, `write_profile`, `has_pinned_openipl3_profile_id`, `is_rdram_b_t1_profile`, `t64_machine_create` (eight ladders plus two refusals), `t64_profile_lookup`, the seven AI gates and `decode_region`, the digest walker.

## Verify-block timings (measured on this host, warm)

| Block | Seconds |
| --- | --- |
| Task 1 build, three CTest names, guard, inventory, core lint | 9.5 |
| Task 2 ten CTest names, guard, inventory (after build; 3.5 with the build) | 1.4 |
| Task 3 ten CTest names (with build) / guard-must-fail check | 2.7 / 0.3 |
| Task 4 guard, guard self-test, `repo.rsp-no-hle`, numstat | 19.1 |
| Task 5 four digest names (three matched), guard, inventory | 3.2 |
| Task 6 block 1 (eleven names, inventory, PII, rights) | 11.2 |
| Task 6 block 2 ASan (build 4.5, nine names, guard, inventory) | 4.5 + 6.5 |
| Task 6 block 3 gcc:13 closure over eight units | 9.7 |
| core lint | 11.0 |
| Inventory selftest (189 gate names, 191 asan) | 26.6 |
| Full CTest gate preset, 185 names after Task 2 / 189 final | 35.6 / 37.9 |

Hosted-budget input (K13): the inventory selftest takes 26.6 seconds on this host with ten new names; the K13 figure of about 15 seconds for 78 names on macOS is consistent with roughly 0.2 seconds per name.

## Pairs covered only by the label-only synthetic case

Real-device pairs in both insertion orders: PI completion with legacy SP completion (skeleton profile), VI field with AI sample, and the CPU step with PI completion, RSP issue, timed input, RDP work, VI field and AI sample. Every other pair of live rows (including all pairs of RDP, input, PI, SP/RSP, VI and AI with each other except the two named, and every pair with the synthetic device) has no in-repo builder and is covered by the 156 label-only ordered pairs. SP completion (device 4 kind 1) is reachable only on the legacy skeleton profile; the functional-RSP profiles issue SP DMA through RSP issue events (device 4 kind 2), so no CPU-with-SP-completion pair exists. The claim wording later should say the order is regression-pinned, not hardware evidence.

## Deviations from Plan

**1. [Rule 1 - Bug] AI mid-DMA golden was build-dependent**
- **Found during:** Task 1 step 4 (declaring the header fields)
- **Issue:** the first literal was taken after the first AI sample event dispatched; that event writes the packet metadata including the build identity string, so the digest changed whenever any production input changed (the public header edit moved it). The plan asserted it was build-independent because no packet exists yet.
- **Fix:** the state is taken one tick before the first sample dispatches (descriptor active, CPU has run, no packet). The new literal was captured from an archive of the pre-edit tree compiled with two different synthetic build identities and was identical. The 17 idle literals were never affected. Reached assertions added.
- **Commit:** `77bf968`

**2. [Rule 2 - Missing critical] Descriptor 12 refuses create under any unregistered id**
- **Found during:** Task 2 (mutation loops)
- **Issue:** a descriptor 12 copy under an unregistered id passed hash-pinning (unregistered ids are not pinned) and created a machine whose family tuple enabled the AI gates with no ring storage allocated.
- **Fix:** `t64_machine_create` refuses descriptor 12 unless the id is one of the four registered ids, before any allocation; covered by `si-profile-identity` (RED observed).
- **Commit:** `a743305`

**3. [Rule 3 - Blocking] Ledger observation by queue sampling; SP completion on the skeleton profile**
- **Found during:** Task 6
- **Issue:** the trace cannot serve as the dispatch ledger because handler-emitted records reuse the device ids with a different kind space, and SP completion (4/1) is never scheduled on RSP-functional profiles.
- **Fix:** the ledger samples the queue every tick across authored scenarios; the SP completion scenario and the PI/SP real pair use the skeleton profile; the CPU-with-SP-completion probe was replaced by CPU with RSP issue.
- **Commit:** `2c8150c`

**4. [Rule 3 - Blocking] Membership generator tokens extended before F0**
- **Found during:** Task 6 (review of the F0 list)
- **Issue:** with the plan's eight tokens the list no longer named the seven AI gates (they now call `t64_has_ai_device`) or the eight create-time ladders, so the F0 review could not confirm coverage.
- **Fix:** five family names added to the token list (`t64_has_ai_device`, `t64_has_si_profile`, `is_ai_family_profile`, `is_si_profile`, `has_si_family_profile_id`) in the Task 6 commit, before the list was committed and before the script logic freezes. `scripts/list-membership-sites.py` is therefore touched by Task 6 although only Task 1 lists it.
- **Commit:** `2c8150c`

**5. [Plan arithmetic] Two verify-block counts do not match their own regexes**
- Task 2 expects "out of 11"; the regex matches 10 names (`unit.core.profile` once, plus four, isviewer, two AI, two digest). Task 5 expects at least 4 matches for `^unit\.core\.digest\.`; the regex matches 3 because `unit.core.digest-trace` has a dash. No test is missing: all 10 and all 4 pass (digest-trace was run explicitly). Recorded, not worked around.

**6. [Test literal] Old-profile contrast status**
- The family-devices old-profile contrast first asserted the AI length write returned status 19; the filter v9 profile returns 16 (unsupported) for that write and 19 for the control write. The literal in the test was corrected to the observed pre-existing behavior; no production change.
- **Commit:** `8e3374f`

**Total deviations:** 6 (1 bug in a plan assumption, 1 missing-critical hardening, 2 blocking, 1 plan arithmetic, 1 test literal). **Impact:** no guard weakened, no pin changed outside Task 4, no scope beyond core identity, digest, ledger and docs.

## Authentication gates

None.

## Known Stubs

None. Section ids 10, 11 and 16 and the SI device constants are declared reserved and emit nothing; they are documented reservations for 09-03, 09-04 and 09-08, not stubs that block this plan's goal.

## Threat Flags

| Flag | File | Description |
| --- | --- | --- |
| threat_flag: internal-state-copy | core/t64_digest.c | `t64_digest_stream` can copy the full machine state, including RDRAM, into a caller buffer. It is declared only in the internal header (not the public header) and is bounded by the sink capacity with an overflow flag; no new public surface. |

## Notes for 09-02 and later plans

- `scripts/list-membership-sites.py`, `docs/spec/profile-membership-sites.txt` and the `PHASE09_TESTS` rows in `scripts/check-test-inventory.sh` are data and tooling; 09-02 decides whether the qualifier stub binds them. The generator logic is frozen at F0; later plans refresh the list file in the commit that adds their site.
- `scripts/check-test-inventory.sh` now carries ten Phase 9 names (K13 budget about 45 for the phase).
- CI/CD efficiency (AUTONOMY section 2): the build-dependent literal was found only after an unrelated header edit; the cheap check that would have caught it is to capture a digest literal from the pre-edit tree under two synthetic build identities (seconds, no CI). The two plan regex counts that disagree with their names are checkable in seconds with `ctest --preset gate -N -R <regex>` before any verify block. Task 6 block 2 shows a warm ASan configure and build of the whole tree in 4.5 seconds, so the asan selection is cheap to run before a hosted job.
- No ROM, save, PIF/IPL image, 64DD media, hardware measurement or oracle was used; the only external input is the already cleared openIPL3 asset through `T64_TEST_IPL3_PATH`. No push was made.

## Self-Check: PASSED

- Created files exist: `core/tests/test_si_profile.c`, `core/tests/test_digest_v25.c`, `core/tests/test_sched_ledger.c`, `scripts/list-membership-sites.py`, `docs/spec/profile-membership-sites.txt` (all tracked by `git ls-files`).
- All 14 commit hashes above are ancestors of HEAD.
- `git log --oneline 1831c49..HEAD -- core/tests/test_profile.c core/tests/test_ai.c docs/spec/audio-cases.json` prints nothing; `scripts/check-rsp-no-hle.py` changed only in `24be635`.
- Final full CTest gate preset: 189 of 189 passed; ASan selection of the nine new unit names passed with no sanitizer report; guard green on `build/gate` and `build/asan`; inventory passes for gate (189) and asan (191); gcc:13 closure, core lint, PII scan and rights scan clean; no `__pycache__` remains.
