---
phase: 09-controller-save-media-and-replay
plan: "03"
subsystem: si-pif-controller
tags: [scheduler-capacity, cpu-store-budget, si-registers, pif-ram, joybus-parser, input-latch, digest-section-10, pad-poll, oracle, guard-refresh]

requires:
  - phase: 09-controller-save-media-and-replay
    provides: "09-01 composite family ids, the v25 digest walker with reserved section 10, the closed ledger and membership list; 09-02 per-row tick cap and transcript pin, the admit fixture builder pattern, the case inventory checker"
provides:
  - one scheduler capacity function (queue room, sequence headroom, tick headroom) shared by the CPU preflight and the SI start, with the profile-gated store budget 2 for RD64B and 1 for WR64B
  - modeled SI registers and DMA with a 64-byte PIF RAM, sticky error, one token per DMA and a reset that cancels its events
  - a total, bounded joybus parser, planner and executor with the standard controller on port 0 and an absent-device answer elsewhere
  - a strictly-after per-port input latch, port field in event value bits 32 and 33, legacy handler unchanged
  - digest section 10 (SI, PIF, latch) in the v25 stream, new-span class
  - authored pad-poll ROM pair, independent oracle, builder, two fixture rows, in-process poll timing and a real supervised accepted run with four controls
  - isolated reviewed-source guard refresh with four new seeds and deletion controls
  - docs/spec/si-pif-controller.md and machine-contract updates
affects: [09-03.1, 09-04, 09-05, 09-06, 09-08, 09-09, 09-10]

actuals:
  tokens: 61000
  tasks: 5
  commits: 13
plan_head_before: 016472a392eb283a05ad5d60d4a2f97d5a4b33be
plan_head_after: 49518ec166a58786a8e24e43e144e2a8f3117299

tech-stack:
  added: []
  patterns:
    - "capacity pre-check then sequential reserve and commit, so a multi-event start never leaves a partial state (K16)"
    - "every refusal row runs an accepted baseline in the same setup and asserts its specific status"
    - "independent literals first: ticks, button bytes and transcripts are written out in tests and the oracle, never read from a production helper"
    - "out-of-tree mutants (throwaway worktree, gate flags with -Werror) that must compile and fail only their named case"

key-files:
  created:
    - core/t64_si.c
    - core/t64_joybus.c
    - core/tests/test_si.c
    - core/tests/test_joybus.c
    - runner/tests/test_pad_poll.c
    - fixtures/pad/poll.S
    - fixtures/pad/poll.ld
    - fixtures/pad/poll_oracle.py
    - scripts/build-pad-fixture.sh
    - docs/spec/si-pif-controller.md
  modified:
    - core/include/tendies64/t64_machine.h
    - core/t64_internal.h
    - core/t64_scheduler.c
    - core/t64_cpu.c
    - core/t64_bus.c
    - core/t64_observe.c
    - core/t64_machine.c
    - core/t64_digest.c
    - core/t64_warm_devices.c
    - core/tests/test_digest_v25.c
    - core/tests/test_sched_ledger.c
    - core/CMakeLists.txt
    - runner/CMakeLists.txt
    - runner/t64_real_fixture.c
    - fixtures/manifest.json
    - scripts/check-test-inventory.sh
    - scripts/check-rsp-no-hle.py
    - scripts/tests/test_rsp_no_hle.py
    - scripts/tests/test_phase09_qualification.py
    - docs/spec/profile-membership-sites.txt
    - docs/spec/phase09-cases.json
    - docs/spec/machine-contract.md

key-decisions:
  - "Section 10 also carries next_token and token_exhausted (after active_token and before execute_pending) so no state that decides later behavior is outside the digest; the plan's field list did not name them"
  - "The SI token is a per-machine counter (next token 1 after reset) like the PI token; token exhaustion refuses with RESOURCE_EXHAUSTED before busy is visible"
  - "The joybus table is authoritative for counts on every execution: the receive byte is rebuilt from the parsed count, the command byte is read from PIF RAM at plan and execute time"
  - "A refused CPU step records its attempt (last PCs, last opcode, host-error outcome) by design, so CPU-path refusal tests compare digests after pre-applying that record"
  - "The wrong-port-flag ROM runs one instruction more per poll, so its input script is derived from its own poll starts"

requirements-completed: []

coverage:
  - id: S1
    description: "Shared capacity rule and profile-gated store budget: RD64B refused unless three events fit, WR64B unless two, legacy budgets unchanged, atomic refusals"
    requirement: "CORE-02"
    verification:
      - kind: unit
        ref: "unit.core.cpu.si-store-budget, unit.core.si.atomic-queue-full"
        status: pass
    human_judgment: false
  - id: S2
    description: "SI registers, directions, status timeline, command byte, timing literals, refusals, reset, in-flight witnesses, ledger rows and the section 10 reference encoder"
    requirement: "SAVE-01"
    verification:
      - kind: unit
        ref: "unit.core.si.device, unit.core.sched.ledger"
        status: pass
    human_judgment: false
  - id: S3
    description: "Parser totality and responders, sixteen button vectors, bounded campaign against a reference parser (20,000 gate, 250,000 ASan iterations)"
    requirement: "SAVE-01"
    verification:
      - kind: unit
        ref: "unit.core.joybus.parser"
        status: pass
    human_judgment: false
  - id: S4
    description: "Strictly-after latch: E-1, E, E+1 literals, equal-stamp rule, partition and submission-order independence, port decode and legacy pins"
    requirement: "CORE-02"
    verification:
      - kind: unit
        ref: "unit.core.input.latch-partition"
        status: pass
    human_judgment: false
  - id: S5
    description: "Authored pad-poll ROM through the real supervised runner: 68-byte transcript equal to the independent oracle, with four wrong-value and replacement controls"
    requirement: "SAVE-01"
    verification:
      - kind: integration
        ref: "runner.pad-poll, controls.real.pad-wrong-value"
        status: pass
    human_judgment: false
  - id: S6
    description: "Isolated guard refresh: two sources, three derived bus pins, four seeds and deletion controls"
    requirement: "CORE-02"
    verification:
      - kind: unit
        ref: "repo.rsp-no-hle (63 seeds)"
        status: pass
    human_judgment: false
  - id: S7
    description: "Specification wording: every figure named software-profile policy, no hardware claim, no reserved claim word, sources with fetch dates"
    requirement: "SAVE-01"
    verification: []
    human_judgment: true
    rationale: "Wording review against the claim boundary is a judgment; the claims, PII and rights scans are automated"

duration: 43min
completed: 2026-11-19
status: complete
---

# Phase 9 Plan 03: SI, PIF and the standard controller (core and tracer half) Summary

**A shared scheduler capacity rule with a profile-gated store budget, a modeled SI with a 64-byte PIF RAM, a total bounded joybus parser, a strictly-after input latch and digest section 10, proven end to end by an authored pad-poll ROM through the real supervised runner, with the reviewed-source guard refreshed in its own isolated commit. The collector, `ci.yml`, the hosted pair and the FH freeze are the decimal plan 09-03.1.**

## OPEN ISSUE FOR THE ORCHESTRATOR (read first)

The full local gate ends 209 of 210. The only failure is the expected, pre-existing `harness.rdp-oracle-authority` (fixture manifest SHA-256 changed). This plan adds **two more manifest rows** (`phase09-pad-poll-si-v1`, `phase09-pad-poll-wrong-si-v1`), so the values the rebind will need moved again. Computed (not applied; `docs/evidence/phase07-oracle-authority.json` and `scripts/check-rdp-oracle-authority.py` are unchanged) at commit `49518ec`:

| Field | Pinned now | At `49518ec` |
| --- | --- | --- |
| manifest `sha256` (two places in the request) | `f7c1953d651c77b5121ce8e454f6c0953ed3495a911bcbb883dfd8ff09917931` | `23b3442430f0f3e7d44f8010a2d5bfc7fef9cf442b2efafb630c1efeb6134972` |
| manifest `git_blob_id_sha1` (two places) | `63097d1657d90315a635aa83a8d7cc7f30ffe12d` | `4d48f263c61d23cce0591f3e6a70805a2264da36` |
| `request_sha256` and `PINNED_REQUEST_SHA256` | `64b6f2608e820742d15df6e8baf40feecf2513a65eea1fef927c45ce653a9008` | `4200bb02e15aa23f2ef375c3f606e4004c90c4722401386653020ea0f80f5500` |

The old request reproduced its own pin first (`True`). Any later manifest edit moves these again; the cheap check is `ctest --preset gate -R '^harness\.rdp-oracle-authority$'` (0.2 s).

## Performance

- **Duration:** 43 min (2026-11-19T21:01:19Z to about 21:45Z, before this closeout)
- **Tasks:** 5 of 5 (Task 1 is the tracer; its steps 2 to 6 are separate commits)
- **Commits:** 13 production, test and docs commits measured from the ledger base (`016472a`)
- **Files:** 10 created, 21 modified (31 changed, 6102 insertions, 72 deletions)

## Accomplishments

- `t64_schedule_check_capacity` (queue room, width, sequence headroom, tick headroom, in the old refusal order) extracted from the CPU preflight; the pinned scheduler bodies are untouched. On the family a 32-bit store to RD64B needs three events (successor plus two SI events) and to WR64B two; every other store and every legacy profile keeps its budget.
- `core/t64_si.c`: registers `0x00`, `0x04`, `0x10`, `0x18`; ordered start validation (overlap, PIF address, alignment, span, command byte, token, capacity); write start reads RDRAM and replaces the PIF RAM at the start tick; read start reserves an execute event at `S + H` and a completion event at `E + 4096` under one token; the completion copies the whole image or nothing; sticky DMA_ERROR; reset cancels the SI events.
- `core/t64_joybus.c`: total parser over offsets 0 to 62 (four escapes, six-bit counts, two flag bits, five channels, the handshake bound), planner and executor (controller info and state, timeout for any other command, absent device, skip and reset flags), channel 4 through one file-local function that answers absent.
- Strictly-after latch under the family only; digest section 10 in the new-span class; the 17 legacy idle digests and the AI mid-DMA digest are unchanged.
- Authored pad-poll ROM pair, independent oracle (baseline 68 bytes equals the hand-derived four lines), in-process poll timing and a real supervised accepted run, plus four same-setup controls.
- Isolated guard refresh (two source names, three derived pins) with four new mutation seeds and deletion controls; the spec and the machine contract.

## Task Commits

1. **Task 1 (tracer):** `888d540` capacity rule extraction (step 2), `040a3c9` store budget and shared constants (step 3), `b322147` SI device, joybus, latch, bus region (step 4), `5282615` digest section 10 (step 5), `44755ff` pad-poll sources, oracle, builder and target (step 6), `6a6cf2d` baseline row, manifest row and tracer test (manifest `source.revision` is `44755ff0aea7b8cf299097a1fd4e886f23af51ce`)
2. **Task 2:** `bb41549` isolated guard refresh
3. **Task 3:** `ef58de9` device matrix, atomic rows, reset, section 10 encoder and ledger rows; `d5b2094` in-flight witnesses
4. **Task 4:** `d4ce676` joybus vectors, campaign and latch cases
5. **Task 5:** `8f58409` controls, wrong-ROM row and the two case suites; `d8c316e` specification; `49518ec` fix of a stale 09-02 suite-count assertion

## Tracer

Task 1 proved the whole path before expansion. The baseline ROM on the stem id, run through the real supervisor with input stamped at E1-1, E2 and E3+1, reported `pass`, `positive-match`, 68 transcript bytes, SHA-256 `715aa3854b455dc9fd8e038e1aa27e5d200ea311acbfd032930cdc760498d3f6` (the oracle `--sha256` value and the independent `shasum` of the hand-derived literal agree), completion tick 626,225 (below the 1,000,000 bound). The Task 1 verify blocks (13 of 13 names, 25 of 25 legacy, scans, region gates, guard expectation, ASan 4 of 4) ran on the committed content: `Tracer verified end-to-end - expanding`.

## RED records

**Task 1 step 2 (start green by design, pure extraction).** Characterized by the 19-row table (27 checks, 27 of 27; a mutant with `>=` in the headroom comparison fails 3 checks) and the legacy group: `LEGACY_CORE` 25 passed before and 25 passed after.

**Step 3 (observed before the budget edit).** `budget row 'family RD64B': status 14 (want 11), retired 3, queue 14` and `budget row 'family RD64B sequence': status 14 (want 12), retired 3, queue 8`; 4 failures of 290 checks (the two statuses and their digest comparisons). Status 14 is RESOURCE_EXHAUSTED, not OK as the plan assumed: the store passed the legacy preflight (extra budget 1) and the bus then refused the unmodeled register, so the CPU stopped. The WR64B pair and the legacy rows started green because the WR64B budget (1) equals the legacy budget; only RD64B drives the change.

**Step 4 (reconstructed, disclosed).** The SI device was drafted before the test run, so this RED is reconstructed by disabling the SI branch of `decode_region` for one run: `tracer-poll` failed 23 of 130 checks: DRAM_ADDR, WR64B and RD64B writes returned 16 (BUS_UNSUPPORTED), STATUS read 0 where 1 was required, `execute_tick` 0 where 19,971, and the first compared destination byte (RDRAM 0x3903) read 0 where 0x80. The test then failed once for its own defect (a 62-byte frame literal, byte 63 was zero) and was fixed.

**Step 5.** Before the walker emitted section 10: `v25-strip-differential` kind=2 (marker order) in all five states (for example idle `old_stripped=6350080 new_stripped=6350080 first_differing_offset=6350167`), `v25-reference` `find_marker(... 10 ...)` failed (1 of 72), `sink-legacy-identity` 124 of 124.

**Step 6.** Fixture row absent, ROM built: `{"detail":"refusal","fixture_id":null,"guest_stop":{"bound_tick":"1000000","kind":"refused"},"guest_tick":"0","outcome":"harness-error"}`; GREEN with the row.

**Tasks 3 and 4 (started green, disclosed).** The full register, status, command byte, sticky error, token and reset logic was written in one coherent `start_dma` in Task 1 step 4 instead of the thinnest path, so the Task 3 and Task 4 groups started green; no failure was manufactured. The evidence is current, not history: out-of-tree mutants (next section).

**Task 5 (wrong-ROM row absent).** `same-setup-wrong-rom`: the accepted baseline passed in the same setup, then the wrong ROM run reported `harness-error`, `refusal`, kind `refused`, guest tick 0 where `guest-fail` / `wrong-transcript` was required (7 failed checks). The shifted-script and neutral-script controls and the replacement control passed (the ROM already existed).

## Out-of-tree mutants (nothing committed)

Commands: `git worktree add --detach build/09-03-mutants/<name> HEAD`, `cmake --preset gate`, `cmake --build --preset gate --target t64_core`, then `cc` with the gate warning flags and `-Werror` over `core/tests/test_si.c` (or `test_joybus.c`) linked with the core and SHA-256 libraries, one case run. Every row compiled under `-Werror` and failed only its named case; each worktree was removed with `git worktree remove --force` and pruned.

| Mutant (one line in production) | Case | Result |
| --- | --- | --- |
| WR64B starts the read handler (direction swap) | `registers` | exit 1 at the direction comparator after the accepted register baseline (2 failures: the following RD64B start returned 19, and `direction_matches`) |
| 24-bit mask removed | `registers` | 1 failure (read-back 0xFF123456 where 0x00123456) |
| sticky error not set on misalignment | `status` | 5 failures (bit 3 reads 0) |
| command byte check removed | `pif-command` | 12 failures (a refused byte accepted) |
| token exhaustion check removed | `refusals` | 5 failures |
| RD64B destination written at the start | `dma-timing` | 2 failures (old bytes expected right after the start); an earlier witness version missed it because the host overwrote the destination first, so the witness reads before overwriting |
| handler promotes after storing | `latch-strictly-after` | exit 1 (literal example reads neutral); `tracer-poll` passes in the same build |
| `next_visible_from` set to the stamp tick | `latch-strictly-after` | exit 1 (B visible at E); `tracer-poll` passes in the same build |
| parser reads the flag bits as the count | `frame-vectors` | 145 failures |

## Task 2 pin record (isolated commit `bb41549`)

Pins: `CANONICAL_CORE_SOURCES` and the `REVIEWED_BODY_DIGESTS` rows of `core/t64_bus.c`. Cause: the SI region edit of commit `b322147`. Between `b322147` and `bb41549` the guard failed with exactly three `executable body changed` findings (`build/gate/09-03-t1-guard.out`) and the unreviewed-source finding for `t64_joybus.c` and `t64_si.c`. Derivation: a throwaway script outside the tree loaded the guard module, normalized `git show 016472a:core/t64_bus.c` and the working tree with the guard's own functions (blank comments and literals, extract the body, drop whitespace, SHA-256); the plan-base digest equals the old pin for all four rows, and the working-tree digest equals the new pin for the three changed rows.

| Row | Old pin | New pin |
| --- | --- | --- |
| `core/t64_bus.c :: decode_region` | `69b17e64915f063c8605fd3f28dab8cf1b76beae62378df2633806a548c6a97c` | `abee4eb6218517121a4a42c18bb969294d39fc1a8293436b095a853e147cda91` |
| `core/t64_bus.c :: t64_bus_write` | `efca62b4fabbbca012e5ca5d6946092c87bf98b2b2fd06227ef61b14c61004d6` | `58d73a6eee953d098a7310e5605872100642a7b0e854faef883197a0448cee71` |
| `core/t64_bus.c :: t64_bus_write_masked` | `963205739dd3cc6d4b10f56fc4e31ed041a5a864f7ef90cdfc736fa842809506` | `bb9b5b266e3c6ebe8962c73539a13f6e85847cbea4ba63167919822cf3cd5b54` |
| `core/t64_bus.c :: t64_bus_writeback_cache_line` | `00b19891...e0621e80` | unchanged (not edited) |

Changed lines inside each body: `decode_region` gained the five-line SI branch (`t64_has_si_profile` window to `T64_BUS_REGION_SI`); `t64_bus_write` gained the six-line SI dispatch block and one `case T64_BUS_REGION_SI:` label; `t64_bus_write_masked` gained one `case T64_BUS_REGION_SI:` label. The other hunks of the bus diff are outside pinned bodies (the moved `T64_SI_BASE` define, the enum value, `region_bytes`, `t64_bus_probe_access`, `t64_bus_read`, `t64_bus_read_wide`, `cache_writeback_destination`). Source set: old 31 names, new 33 (`t64_joybus.c` and `t64_si.c` in sorted position). `git show --numstat` of the guard in that commit is `5 3`. Control: four new seeds (injected effects in `t64_bus_write` and `decode_region`, the SI branch removed from `decode_region`, an unreviewed `core/t64_extra.c`) are rejected and the seed count equals the declared list (63); deleting any one of the three pins fails the guard with `reviewed normalized body digest is missing` (exactly that finding for the row) and removing either source name from the reviewed set reports the unreviewed-source finding.

## Measurements

- Poll start ticks (the RD64B register writes of the neutral and accepted runs; identical): **541,467, 562,654, 583,846, 605,038**; E = S + 16,971 = 558,438, 579,625, 600,817, 622,009; inputs stamped 558,437 (E1-1), 579,625 (E2), 600,818 (E3+1). Poll periods 21,187, 21,192 and 21,192 ticks (the policy minimum is 21,067). Completion tick of the baseline ROM **626,225** (neutral and accepted runs are equal, so the timeline does not depend on input); the wrong-ROM variant completes at 626,229 (one instruction more per poll).
- Oracle transcripts (`fixtures/pad/poll_oracle.py --sha256`, all 68 bytes): baseline `715aa385...d3f6` (independent `shasum` of the printf literal equal), shifted `4c788991...7910`, neutral `caf55c81...f137`, wrong-rom `07de0176...3315`.
- ROMs: baseline `29a005be19b2f86e866d00cfb3ab2f8aac39e376d85a5e5d24e78c113a80dab6` (4688 bytes, completion `0x800005a8`), wrong-port-flag `84bacc101384d52b109a4c41e4b4fac340143a89ad79a4fc4460c5890923a8f6` (4688 bytes, `0x800005ac`); source-content SHA-256 `0e923ac3ca72a4d5d6f6ffb485c3b4803dba0bf12ad37776cde76b421aef3f46`; stem profile hash `661b161dfee9a10857efe3f98d840c09f05cbf0492dfde59e8ec44190dabb6d5`.
- Inventory selftest: **20 s** (210 gate names, 212 ASan). **Cumulative Phase 9 gate names: 31** (24 + 7: `unit.core.cpu.si-store-budget`, `unit.core.si.atomic-queue-full`, `unit.core.si.device`, `unit.core.input.latch-partition`, `unit.core.joybus.parser`, `runner.pad-poll`, `controls.real.pad-wrong-value`; no ASan-only name; `harness.phase09-collect-real` moved to 09-03.1).
- Verify blocks (seconds, warm host): Task 1 legacy baseline 5; Task 1 block 1 6 (13 names, 25 legacy, scans); Task 1 ASan 13 (4 names, build included); guard 8; Task 2 guard test 10 and `repo.rsp-no-hle` 10.6; Task 3 block 1 13 (10 names) and ASan 28 (4 names: `unit.core.si.device` 24.5 under ASan against 1.3 native); Task 4 block 1 17 (9 names, guard 13) and ASan 19 (joybus campaign at 250,000 iterations 0.3, latch 15.7); Task 5 ASan 39 (7 names, build included), core lint 9, gcc:13 closure over eleven units 8, case checker both suites 3, inventory selftest 20; **full local `sh scripts/gate.sh` 85 s** (209 of 210, the single failure is the expected oracle pin above; its first run, 85 s as well, also failed the two 09-02 qualification tests fixed in `49518ec`).
- Joybus campaign: 20,000 iterations in the gate preset (under 1 s) and 250,000 under ASan through `T64_JOYBUS_FUZZ_ITERATIONS` in the sanitizer block of `core/CMakeLists.txt` (appended to the test environment; timeout raised to 600); zero differences from the independent reference parser, entry count at most 5.

## Deviations from Plan

**1. [Plan order] The SI device was implemented in full in Task 1 step 4.** One coherent start function holds every validation, the sticky error, the token and the reset, so the Task 3 and Task 4 groups started green (disclosed above); evidence is the out-of-tree mutants. The RED of step 4 is a reconstruction.

**2. [Rule 2 - Missing critical] Section 10 carries `next_token` and `token_exhausted`.** The plan's field list named only `active_token`, but the token counter decides later behavior and the F1 audit (09-06) would flag it as hidden state. Added after `active_token` and before `execute_pending`; the independent encoder and the spec table include them.
- **Commit:** `5282615`

**3. [Plan assumption] CPU-path refusals are not digest-identical.** A refused CPU step records its attempt (`last_pc`, `last_pc_full`, `last_opcode`, outcome host error) before the preflight returns. The budget case pre-applies that record to the expected pre-state and then asserts equality; the 8-byte store row asserts the analog's status, retired count, queue and SI state (the CPU stops on the bus refusal exactly as it does on the AI window, so its digest is not compared). The step 3 RED status was 14, not OK (see RED records).
- **Commit:** `040a3c9`

**4. [Rule 3 - Blocking] Supervised runs with a custom schedule need `--input-size` and `--input-sha256`.** Without them the supervisor judges only the built-in empty schedule and refused the run. The test helper computes both from the script file.
- **Commit:** `6a6cf2d`

**5. [Rule 3 - Blocking] Manifest `expected.source` is limited to 256 characters.** The first text (334) failed `gov.manifest.repo`; shortened.
- **Commit:** `6a6cf2d`

**6. [Rule 1 - Bug] Stale 09-02 assertion.** `test_zero_case_inventory_refused` asserted the inventory has exactly two suites; the two data suites this plan adds made it fail in the full gate (the plan's file list omitted the test). It now compares with the committed suite count and keeps the two original suites.
- **Commit:** `49518ec`

**7. [Test finding] The wrong-ROM variant shifts the poll timeline.** `andi` adds one instruction per poll, so the baseline script crossed an execute tick (P2 printed state B, observed `e91682e9...`). The control derives its stamps from the variant's own poll starts, which is the oracle's wrong-rom scenario. The sources were not edited.

**8. [Plan wording] Membership list updated in two commits.** The first family site (`cpu_successor_extra_event_budget`) landed in step 3 and the SI sites in step 4; the list was regenerated in each.

**9. [Plan wording] `case_digest_section` fields.** The reference encoder reads the SI state fields; it calls no production digest code.

**10. [Test literal] Frame literal.** The first tracer literal was 62 bytes (byte 63 zero); found by the GREEN run, fixed with a designated initializer.

**11. [Requirements] SAVE-01 and CORE-02 are not marked complete.** Hosted evidence is 09-03.1 and the full ordering matrix is 09-06, as in 09-02; `requirements-completed` is empty.

**Total deviations:** 11 (1 order, 1 missing-critical, 1 plan assumption, 2 blocking, 1 bug, 1 test finding, 4 notes). **Impact:** no guard weakened; the only pins touched are in the isolated Task 2 commit; no file of the FH list (`scripts/qualify-phase09.py`, `ci.yml`, `check-ci-yml.sh`, `gate.sh`, `rights-scan.sh`, `tools/rights`, `runner/t64_supervisor.c`, `build-boot-fixtures.sh`) changed; `core/tests/test_profile.c`, `core/tests/test_ai.c` and `docs/spec/audio-cases.json` are untouched.

## Authentication gates

None.

## Known Stubs

- Channel 4 (cartridge) answers "absent" for every id (`cartridge_channel_response` in `core/t64_joybus.c`): intentional, 09-04 replaces its body behind the same interface.
- No other stub; no placeholder text in created or modified files.

## Threat Flags

None beyond the plan's register (T-09-16 to T-09-23): the new surface is the four SI registers and the joybus parser, each with its mitigation test; `build/09-03-mutants` held throwaway worktrees only and none remains.

## Inputs for 09-03.1

- **Final `SOURCE_PATHS` additions** to the 09-02 tuple: `scripts/build-phase09-admit-fixture.sh`, `scripts/build-pad-fixture.sh`, `scripts/collect-boot-evidence.sh`, `scripts/qualify-rdp-vi.py`, `scripts/qualify-audio.py`, `scripts/check-audio-cases.py`, `docs/spec/profile-membership-sites.txt`, `scripts/build-boot-fixtures.sh`. **`PENDING_SOURCE_PATHS` (six, replacing the earlier three):** `scripts/build-save-fixtures.sh` (09-04), `scripts/build-replay-fixture.sh` (09-07), `scripts/check-state-field-use.py` (09-09), `scripts/k5-mutants.py` (09-06, run by `repo.sched-k5-anchors`), `docs/spec/state-field-rules.txt` (09-09.1, input of `harness.state-field-use-audit`) and `docs/spec/state-mutants.txt` (09-09.1, read through `T64_STATE_MUTANT_INVENTORY`). `core`, `runner`, `fixtures` and `tools/rights` are bound wholesale, so `fixtures/pad/poll_oracle.py` and the new core and runner files are covered.
- **Collector inputs:** suites `si-core` and `controller` of `docs/spec/phase09-cases.json`; the pad-poll transcript (68 bytes, SHA-256 `715aa385...d3f6`); fixture and profile hashes of the two pad-poll rows (above); the platform-invariant observation list: transcript bytes, fixture identity, profile hash, measured poll start ticks and the guest completion tick range (build ids differ per platform by design). The baseline and wrong rows both carry `guest_tick_cap` `T64_REAL_GUEST_TICKS_LONG_CAP`; the supervised run uses `--bound 1000000` plus `--input-size` and `--input-sha256`.
- **Supervisor argv slot sum:** unchanged (this plan added no option).
- **Name budget (K13):** 31 Phase 9 gate names now; the total is exactly 45 gate names and zero ASan-only names, so 09-03.1 to 09-11 must fit in 14 more; 09-03.1 adds 1 (32).
- **State the collector can rely on:** the pad-poll timeline is input independent (the neutral and accepted runs complete at the same tick), the wrong-ROM variant completes 4 ticks later, and the poll start tick is the bus trace tick of the RD64B write.
- **Oracle rebind** decision (above) before any push.

## CI/CD efficiency notes (AUTONOMY section 2)

- The stale 09-02 suite-count assertion and the oracle pin were found only by the full gate (85 s). Seconds-long checks that would have caught them: `ctest --preset gate -R '^harness\.phase09'` (1.5 s) after editing `docs/spec/phase09-cases.json`, and `ctest --preset gate -R '^harness\.rdp-oracle-authority$'` (0.2 s) after any `fixtures/manifest.json` edit.
- `ctest -N -R <regex>` before relying on a verify block avoided the plan-arithmetic traps of 09-01 and 09-02 (the 13, 10, 9 and 7 name counts all matched).
- Under ASan the SI device suite takes 24.5 s (against 1.3 s native) because its section 10 cases capture a 6 MB digest stream per state and the field flips hash 44 times; the latch partition suite takes 15.7 s (step size 1 over six submission orders). If hosted ASan time matters, fewer streams or a larger smallest step are the cheapest cuts; none was taken because the budget is met.
- The pinned gcc:13 image was already on disk (`gcc:13`, id `ad13696c6b24`), so the closure ran in 8 s with no pull.
- Out-of-tree mutants need only `t64_core`, `libt64_sha256.a` and the test source (about 5 s each); building the test targets in the worktree would drag in the fixture build.

## Self-Check: PASSED

- The ten created files are tracked (`git ls-files`); all thirteen commit hashes above are ancestors of HEAD.
- `scripts/check-rsp-no-hle.py` changed only in `bb41549` (`5 3`), `scripts/tests/test_rsp_no_hle.py` only there; `git log --name-only` over the FH paths since the plan base prints nothing; `scripts/check-test-inventory.sh` changed in the task commits only by added names.
- Final content checks: gate-preset names of this plan 7 of 7, Task 1 block 13 of 13, Task 3 block 10 of 10, Task 4 block 9 of 9, legacy 25 of 25, ASan 7 of 7 with no sanitizer report, guard green on `build/gate` and `build/asan` (83 sources, 63 seeds), inventory passes for gate (210) and ASan (212), PII scan, rights scan, claims check and core lint clean, gcc:13 closure clean, no `__pycache__` remains. Full local gate 209 of 210 (expected oracle failure only).
- No ROM, save, PIF/IPL image, 64DD media, hardware measurement or oracle was used; the only external input is the already cleared openIPL3 asset embedded by the authored ROMs. No push was made.
