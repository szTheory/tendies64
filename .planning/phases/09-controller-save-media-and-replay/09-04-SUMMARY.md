---
phase: 09-controller-save-media-and-replay
plan: "04"
subsystem: save-media
tags: [eeprom, sram, joybus-channel-4, busy-deadline, pi-dma, digest-section-11, save-info, oracle, mutants, guard-refresh]

requires:
  - phase: 09-controller-save-media-and-replay
    provides: "09-01 composite family ids with save_kind 1 to 3, reserved section 11, the v25 digest walker; 09-02 per-row tick cap and transcript pins; 09-03 SI, joybus and the channel 4 seam that answered absent; 09-03.1 the frozen qualifier and case checker"
provides:
  - one trailing save storage region per machine, blank fill at create and at the admission reset site, sized by the profile kind alone
  - EEPROM 4 and 16 Kbit on joybus channel 4 with static exact-triple classification, the 4 Kbit mirror and a stored busy deadline E + 375,000 with no scheduler event
  - SRAM window [0x08000000, 0x08008000) on the -s32k id: aligned word access and PI DMA both ways, effect atomic at the completion event
  - t64_machine_save_info with the D-24 check prefix and statuses 20 to 22, t64_save_in_flight, reserved kinds 4 to 7
  - digest section 11 (kind, size, deadline, PI direction, storage) for the save ids only
  - five authored eeprom-rw and sram-rw ROMs, an independent transcript oracle, five fixture rows, real supervised runs and controls
  - isolated reviewed-source guard refresh with four new seeds, eight recorded out-of-tree mutants, docs/spec/save-media.md
affects: [09-05, 09-06, 09-07, 09-08, 09-09, 09-10, 09-11]

actuals:
  tokens: 57500
  tasks: 6
  commits: 13
plan_head_before: 0d109452c1b87f91cc3138ca3de8d4e250ae39a2
plan_head_after: a8498bde33d5645f77ced37fcab6407018f20df7

tech-stack:
  added: []
  patterns:
    - "classification is static (profile kind, declared counts, command byte) and outcome is dynamic (the deadline compare at the execute tick), so the planned handshake never depends on busy state"
    - "a stored deadline compared at later execute ticks instead of a scheduler event: no new event pair, closed ledger unchanged"
    - "independent literals first: ticks, transcripts, section bytes and the guard digests are derived outside the code under test"
    - "out-of-tree mutants (throwaway worktree, gate flags with -Werror) each prove a named comparator bites"

key-files:
  created:
    - core/t64_save.c
    - core/tests/test_save.c
    - runner/tests/test_save_fixtures.c
    - fixtures/save/eeprom.S
    - fixtures/save/eeprom.ld
    - fixtures/save/sram.S
    - fixtures/save/sram.ld
    - fixtures/save/save_oracle.py
    - scripts/build-save-fixtures.sh
    - docs/spec/save-media.md
  modified:
    - core/include/tendies64/t64_machine.h
    - core/t64_internal.h
    - core/t64_joybus.c
    - core/t64_si.c
    - core/t64_bus.c
    - core/t64_pi.c
    - core/t64_machine.c
    - core/t64_digest.c
    - core/tests/test_digest_v25.c
    - core/CMakeLists.txt
    - runner/CMakeLists.txt
    - runner/t64_real_fixture.c
    - fixtures/manifest.json
    - scripts/check-test-inventory.sh
    - scripts/check-rsp-no-hle.py
    - scripts/tests/test_rsp_no_hle.py
    - docs/spec/phase09-cases.json
    - docs/spec/profile-membership-sites.txt
    - docs/spec/machine-contract.md

key-decisions:
  - "Classification is static and outcome is dynamic: only the three exact triples (0x00,1,3), (0x04,2,8), (0x05,10,1) are valid on an EEPROM id (cost 2000n + 188 + 438 + 2000m + 250), everything else there is a timeout and a profile without EEPROM is absent (cost 2000n + 188 + 3907); a read while busy is valid at plan time and answers 0x40 at E"
  - "A profile with no device answers any requested kind with SAVE_DEVICE_MISMATCH (the locked set of three statuses is kept); values 20, 21, 22; plan 09-05 uses 23 upward"
  - "Section 11 exists only when save_kind is non-zero, so the stem stream keeps markers 1 to 10; the body is u32 kind, u32 size, u64 busy_until, u32 pi_save_direction, raw storage, all new-span class; pi_save_direction is the raw active_save_direction field"
  - "The widths other than 4 on the SRAM window answer BUS_UNSUPPORTED on every path, including the 64-bit read (the SI region answers BUS_DEVICE_REFUSED there) and the masked write"
  - "The openIPL3 profile ids admit only the exact asset or an authored flat ROM, so the selection control edits header bytes of a 4,160-byte authored flat ROM built from the asset in the test"

requirements-completed: []

coverage:
  - id: E1
    description: "EEPROM 4 and 16 Kbit on channel 4: exact triples, timeout and absent answers, mirror, busy deadline compare at deadline minus 1 and at the deadline, no window extension, overflow refusal, nine handshake literals"
    requirement: "SAVE-02"
    verification:
      - kind: unit
        ref: "unit.core.save.device (eeprom-tracer, eeprom-commands, eeprom-blocks, eeprom-busy, eeprom-timing)"
        status: pass
    human_judgment: false
  - id: E2
    description: "SRAM window, aligned word access and PI DMA both directions with effect at completion, cancel, refusals and the CPU store budget rows"
    requirement: "SAVE-02"
    verification:
      - kind: unit
        ref: "unit.core.save.device (sram-window, sram-dma)"
        status: pass
    human_judgment: false
  - id: E3
    description: "Selection by profile only, SAVE_* refusal statuses in the D-24 order with bit-identical digest, storage, deadline, queue, tick and sequence"
    requirement: "SAVE-04"
    verification:
      - kind: unit
        ref: "unit.core.save.device (selection, refusals, reset-blank)"
        status: pass
    human_judgment: false
  - id: E4
    description: "Section 11 pinned by an independent encoder, field flips and the strip differential for the three save ids"
    requirement: "SAVE-02"
    verification:
      - kind: unit
        ref: "unit.core.save.device (digest-section, storage-blank), unit.core.digest.v25-strip-differential"
        status: pass
    human_judgment: false
  - id: E5
    description: "Five authored ROMs through the real supervised runner: pass with oracle-equal transcripts, same-setup wrong-byte controls, ROM and profile mismatch refusal"
    requirement: "SAVE-02"
    verification:
      - kind: integration
        ref: "runner.save-fixtures, controls.real.save-wrong-byte"
        status: pass
    human_judgment: false
  - id: E6
    description: "Isolated guard refresh with derived digests, exact numstat, four seeds and deletion controls; eight out-of-tree mutants each fail a named comparator"
    verification:
      - kind: unit
        ref: "repo.rsp-no-hle (67 seeds); build/09-04-mutants/all.out"
        status: pass
    human_judgment: false
  - id: E7
    description: "Specification wording: every figure a named policy, no reserved claim word, sources with fetch dates"
    requirement: "SAVE-02"
    verification: []
    human_judgment: true
    rationale: "Wording review against the claim boundary is a judgment; the claims, PII and rights scans and the reserved-word grep are automated"

duration: 42min
completed: 2026-11-19
status: complete
---

# Phase 9 Plan 04: Save devices, SRAM window, section 11 and the eeprom-rw and sram-rw fixtures Summary

**EEPROM 4 and 16 Kbit exact-length commands on joybus channel 4 with a stored busy deadline compared at the SI execute tick, an SRAM window with aligned word access and PI DMA, profile-only selection with three provisional refusal statuses, digest section 11, five authored ROMs whose transcripts equal an independent oracle through the real supervised runner, and the reviewed-source guard refreshed in its own isolated commit.**

## OPEN ISSUES FOR THE ORCHESTRATOR (read first)

1. **`harness.rdp-oracle-authority` fails** (the expected, pre-existing failure kind): five manifest rows were added. Not edited (owner-stop area). Computed, not applied, at HEAD `a8498bd` (the old request reproduced its own pin first, `True`):

   | Field | Pinned now | At `a8498bd` |
   | --- | --- | --- |
   | manifest `sha256` (fixture_manifest and bound_authored_inputs[1]) | `23b3442430f0f3e7d44f8010a2d5bfc7fef9cf442b2efafb630c1efeb6134972` | `abbf9c737add67c5ef07617eca9141fc797abf6c61c87db9be4b7a49e41ca2f2` |
   | manifest `git_blob_id_sha1` (same two places) | `4d48f263c61d23cce0591f3e6a70805a2264da36` | `0f65fb046c6331b4de667ac9d90508a16177ffc1` |
   | `request_sha256` and `PINNED_REQUEST_SHA256` | `4200bb02e15aa23f2ef375c3f606e4004c90c4722401386653020ea0f80f5500` | `9df6a059c9d2696074f4621e4085c7c2c743d790bd6a7f6411be533594676d36` |

   The cheap check is `ctest --preset gate -R '^harness\.rdp-oracle-authority$'` (0.2 s). Every later manifest edit moves these again.
2. **Full gate-preset CTest: 213 of 214 pass** (`ctest --preset gate --parallel 4`, 50 s); the single failure is item 1. No full `sh scripts/gate.sh` and no push were made (D-40 schedule; the next full gate is 09-06).
3. The membership list changed in two commits and the plan expected it unchanged (Deviations 2).
4. Candidate for the next hosted run: the Linux pre-flight (206 compile units, Ubuntu 24.04 gcc 13.3, `-Werror`) is clean; see Verification.

## Performance

- **Duration:** about 42 min (2026-11-19, from the 09-03.1 closeout at 18:15 to the last task commit at 18:53, local time) for six tasks.
- **Commits:** 13 measured from the ledger (`0d10945..a8498bd`, before this summary).
- **Files:** 29 changed (5,545 insertions, 75 deletions), 10 of them created.

## Commits

| Commit | Task | Subject |
| --- | --- | --- |
| `b846c3f` | 1 step 2 | storage region, blank reset, section 11 (direction 0 until Task 2) |
| `56c3673` | 1 step 3 | channel 4 seam, EEPROM device, additive joybus machine variants |
| `faa2a38` | 1 step 4 | sources, oracle, builder, CMake target (the manifest `source.revision`) |
| `946fd9e` | 1 step 4 | e4k fixture row, manifest row, tracer test |
| `5ad6aaa` | 2 A | SRAM region carve-out and word access |
| `5fd0812` | 2 B | PI shared start, SRAM directions, `t64_pi_complete` branch |
| `c561f9f` | 2 C | PI direction in section 11 |
| `eeec998` | 3 | isolated guard refresh (touches only the two script files) |
| `b4b752a` | 4 | EEPROM command, block, busy and timing matrices |
| `cb45a25` | 5 | save info, refusals, section encoder, strip differential, reset |
| `e354ffc` | 6 | four rows, five runner cases, two case suites |
| `8179809` | 6 | `docs/spec/save-media.md` and machine contract |
| `a8498bd` | 6 | inventory rows moved so the diff holds added lines only |

## Accomplishments

- **Tracer (Task 1).** The e4k ROM writes block 1, sees the 375,000-tick window open and close in guest time and prints 191 bytes equal to the independent pin through the real supervised runner, completion tick 1,567,331 under the 3,000,000 bound. Auto mode: the plan `<verify>` was re-run end to end on the committed content (19 of 19, legacy 25 of 25): `Tracer verified end-to-end - expanding`.
- **Save device.** `core/t64_save.c`: sizing and blank reset, `t64_save_channel4_classify` and `_execute`, SRAM word access and DMA apply, `t64_save_in_flight`, `t64_machine_save_info`.
- **Bus and PI.** One decoder branch before the domain 2 arm, six region labels, a shared `start_dma` whose ROM path keeps its checks, statuses, trace offsets and order, one direction field and one branch in `t64_pi_complete`; no new event pair.
- **Guard.** Four pinned bodies and one source name re-pinned in an isolated commit with derived digests.
- **Fixtures.** Two authored sources build five ROMs through defsyms; five rows; two same-setup wrong-byte controls; a ROM and profile mismatch control through the real runner binary.

## RED and tracer records

**Task 1 step 2.** `storage-blank` before the implementation (header declarations only): `unit.core.save.device: checks=90 failures=10`: for e4k, e16k and s32k `machine->save.bytes != NULL` failed (pointer NULL) and the marker count was 10 where 11 was required (markers 1 to 10, section 11 absent); the stem digest literal was captured at this point, before any production behavior change: `5ed31137a518103e1086ec9d84350f44306f81fef57145ca4ca72a7f0815d9b0` (no earlier literal existed in the tests; the header edit cannot change a digest). GREEN: 150 checks, 0 failures.

**Step 3.** `eeprom-tracer` against the absent cartridge channel: `si.execute_tick got 34095 want 32876` (an absent answer costs 2000 x 10 + 188 + 3907 = 24,095, not 22,876), `save.busy_until got 0 want 407876`, write image byte 5 `got 129` (0x81, receive count with the no-device flag) and the data byte (offset 16) `got 255` (fill untouched); read image byte 5 `got 136` (0x88) with fill untouched. A first version that settled at the literal completion tick cascaded (the next WR64B returned 19); the test now settles at the machine's reported completion tick after asserting the literal.

**Step 4.** Row absent, ROM built: `{"detail":"refusal","fixture_id":null,"guest_stop":{"bound_tick":"3000000","kind":"refused"},"guest_tick":"0","outcome":"harness-error"}`; GREEN with the row (completion tick 1,567,331).

**Task 2.** `sram-window`: on the -s32k id the blank word read returned `16` (BUS_UNSUPPORTED) where OK was required, then the first and last word writes likewise (14 failures of 92). `sram-dma`: `pi_start(... PI_RD_LEN, 15) got 19 want 0` (BUS_DEVICE_REFUSED where OK was required), PI status 0 where 1. The non-SRAM and legacy rows start green by design (they must not be reached by the new path) and say so in the source. Section 11 direction: flipping `active_save_direction` mid-DMA did not change the digest (`memcmp(mid, flipped) != 0` failed) until the field was wired.

**Task 3.** Guard red from Task 2 with `build/gate/09-04-t2-guard.out`: four `executable body changed` findings (`decode_region`, `t64_bus_write`, `t64_bus_write_masked`, `t64_pi_complete`) plus the unreviewed-source findings for `t64_save.c`.

**Task 4 (start green, disclosed).** All four matrix groups passed on the first run over the Task 1 device (1,686, 2,464, 440 and 258 checks). Not a manufactured failure: the tracer already implemented the whole command set, the mask and the busy edges. The named genuine-RED candidates (overflow refusal, TX flag skip, absent profiles, classification independent of busy) also passed. The evidence is current, not history: eight out-of-tree mutants (below) each fail a named comparator after the baseline passed.

**Task 5.** With stubs (`t64_machine_save_info` returning INVALID_ARGUMENT): `selection` `t64_machine_save_info ... got 1 want 22` and `got 1 want 0`; `refusals` `info.kind got 2779096485 want 1` (untouched sentinel), status 1 where 22, 21 and 20 were required; the extended strip differential: `differential idle e4k: kind=4 old_stripped=6350080 new_stripped=6350080 first_differing_offset=6350171` for all four states (kind 4 is the identity-span comparator; the stripped bodies were already equal; the helper only knew one new-class span). `digest-section` and `reset-blank` started green by design (the device already reaches those states, and the Task 1 reset hook already satisfies the replacement). The selection control also produced a finding (Deviations 3).

**Task 6.** Rows absent: the 16 Kbit and SRAM ROMs reported `harness-error` / `refusal` instead of `pass`, the wrong-byte ROMs `harness-error` / `refusal` instead of `guest-fail` / `wrong-transcript` (the e4k baseline passed in the same setup), the profile-mismatch control reached its refusal rows. GREEN with the rows on the first real run.

## Task 3: the isolated pin record (commit `eeec998`)

Cause: the SRAM region carve-out (`5ad6aaa`) and the SRAM directions in the PI completion (`5fd0812`). The numstat of `scripts/check-rsp-no-hle.py` in that commit is `5 4` (n = 4, so n + 1 insertions and n deletions). Derivation: a throwaway script outside the tree loaded the guard module, normalized `git show 0d10945:<file>` and the working tree with the guard's own functions (blank comments and literals, extract the body, drop whitespace, SHA-256); the plan-base digest equals the old pin for every row and the working-tree digest equals the new pin for the four changed rows.

| Row | Old pin (plan base) | New pin (working tree) |
| --- | --- | --- |
| `core/t64_bus.c :: decode_region` | `abee4eb6218517121a4a42c18bb969294d39fc1a8293436b095a853e147cda91` | `2d7897dd4c1b28b787b0dd627b4944bada27569294d23bb57cfc49b2be98562d` |
| `core/t64_bus.c :: t64_bus_write` | `58d73a6eee953d098a7310e5605872100642a7b0e854faef883197a0448cee71` | `c4569fc658790ea4f482b4853d55ca80f582a6274376002d0d748e50d9896baf` |
| `core/t64_bus.c :: t64_bus_write_masked` | `bb9b5b266e3c6ebe8962c73539a13f6e85847cbea4ba63167919822cf3cd5b54` | `54d8061a476f3ca3c6d43cbf1967622d2b6977f18e2f9d3c7e05efee49bf068f` |
| `core/t64_pi.c :: t64_pi_complete` | `7edd3c4b64276a9b6698cb13f16b91a183f400cee26347b350b49bccb69e36af` | `98459ddc917f56fbc902837822735a1351b7f5b6c7b63b6c4808a1a6f6240c21` |
| `core/t64_bus.c :: t64_bus_writeback_cache_line`, `core/t64_scheduler.c :: event_can_dispatch`, `t64_schedule_cancel` | unchanged | unchanged (base equals working tree) |

Changed lines inside each pinned body: `decode_region` gained the seven-line SRAM branch (`T64_SRAM_BASE` window, `t64_has_sram_save`, return `T64_BUS_REGION_SAVE_SRAM`); `t64_bus_write` gained the six-line SRAM dispatch block and one `case T64_BUS_REGION_SAVE_SRAM:` label; `t64_bus_write_masked` gained `case T64_BUS_REGION_SAVE_SRAM: return T64_STATUS_BUS_UNSUPPORTED;`; `t64_pi_complete` gained the non-ROM direction branch (apply at completion, canceled leaves the storage), the existing ROM block unchanged inside an `else`, and the trailing direction reset. The other bus hunks are outside pinned bodies (enum, `region_bytes`, probe, `t64_bus_read`, `t64_bus_read_wide`, `cache_writeback_destination`). Source set: 33 names to 34 (`t64_save.c` in sorted position). Control: four seeds (injected effect in `t64_pi_complete` and `decode_region`, the SRAM branch removed from `decode_region`, an unreviewed `core/t64_extra_save.c`) are rejected, the seed count equals the declared list (63 to 67), and deleting the `t64_save.c` name or the `t64_pi_complete` pin fails the guard with its own message.

## Out-of-tree mutants (nothing committed)

Driver: `build/09-04-mutants/run_mutant.sh LABEL CASES FILE OLD NEW` (kept in the ignored build directory, not committed): `git worktree add --detach build/09-04-mutants/wt-LABEL HEAD`, `cmake --preset gate`, `cmake --build --preset gate --target t64_core`, then `cc` with the gate flags and `-Werror` over `core/tests/test_save.c` linked with the core and SHA-256 libraries, the baseline case first, then the one-line edit and the same case; each worktree removed with `git worktree remove --force` and pruned. Every row compiled under `-Werror` (core build exit 0, test compile exit 0), passed its baseline and failed only at the named comparator.

| Mutant (one line) | Cases | First failing comparator |
| --- | --- | --- |
| M1 deadline from `si.start_tick` | `eeprom-busy`, `eeprom-tracer` | `busy_until got 385000 want 407876` (both) |
| M2 deadline from `si.complete_tick` | `eeprom-busy`, `eeprom-tracer` | `busy_until got 411972 want 407876` (both) |
| M3 compare `<` changed to `<=` | `eeprom-busy`, `eeprom-tracer` | idle-at-the-deadline rows: info status `got 128 want 0`, read byte 5 `got 72 want 8` |
| M4 write while busy recomputes the deadline | `eeprom-busy` | `busy_until got 782875 want 407876` (window extended) |
| M5 4 Kbit mask 0x3F to 0x7F | `eeprom-blocks` | alias rows: 139 failed checks from `memcmp(out, pattern, 8u) == 0` |
| M6 SRAM DMA duration without the base tick | `sram-dma` | `PI status got 0 want 1` one tick before the completion |
| X1 (extra) 64-bit overflow check disabled | `eeprom-busy` | `flags got 0 want 64`, `rx[0] got 0 want 255` |
| X2 (extra) write valid for any receive count at least 1 | `eeprom-commands`, `eeprom-timing` | `execute_tick got 266612 want 265831`, `tick_matches` |

The M5 mutant writes past a 512-byte region for index 64 and above; the alias rows run first and fail before any later write, and the process exited with a comparator failure (not a crash).

## Measurements

- **Completion ticks of the three accepted real runs** (supervised, bound 3,000,000, long cap 4,000,000): 4 Kbit **1,567,331**, 16 Kbit **1,567,338**, SRAM **539,986** (the wrong-byte ROMs complete at the same ticks as their baselines).
- **Oracle values** (`python3 -B fixtures/save/save_oracle.py --scenario S --bytes` and `--sha256`) and the independent `shasum -a 256` of the hand-written literal, all equal:

  | Scenario | Bytes | SHA-256 |
  | --- | --- | --- |
  | `eeprom-4k` | 191 | `03891120e98176bc11f8d9f6cde6278ff33ef0ebe9018d2f24ce083b3d2c6502` |
  | `eeprom-16k` | 192 | `d9a5bf956cce18ea6bc3fd563ae060d7dda3f8554a881b62214ccf89a7f4ca95` |
  | `eeprom-wrong-4k` | 191 | `87609f3fa10ce472755b3b501cfb80a457c16be2c20e536b190b8e2002598a7c` |
  | `sram-32k` | 101 | `52b0f1caee3ec470f4217236ce6f3da94e6ff1c15fd93143a68f7364b732a026` |
  | `sram-wrong-32k` | 101 | `29174dfa593b7e9b6cc9e979ff7312e1fc39b5fc41ef9b280e8833dfe049f06c` |

  The oracle reproduced the hand-derived transcripts of the plan exactly (nothing adjusted). `--check-margins`: wait margin 42,124 ticks (440,000 - 375,000 - 22,876), busy rows worst case 238,416 under a 50,000-tick gap.
- **ROMs** (source-content SHA-256 `72f1bb5c589e58b4671fdee7c2e5e55fb934d7ae47ef25d0903f8c675707090b`, the manifest `source.revision` is `faa2a381ca88301017faa39f6d5a846f0c44d872`): e4k `0f3cf3c72a0312d90c915f5d148817f9d5f201b96f1575abbc96a81e389be0e9` (5,584 bytes, completion `0x80000688`), e16k `af3267db...6812` (5,584), wrong e4k `4cab313e...9cec` (5,584), sram `9616bd3b...07db` (4,808, completion `0x800005f0`), wrong sram `783f4633...ce67` (4,808). The build is reproducible (two builds, identical ROM bytes).
- **Name budget (K13).** Gate names added: exactly three (`unit.core.save.device`, `runner.save-fixtures`, `controls.real.save-wrong-byte`). **Cumulative Phase 9 gate names: 35** (32 + 3) of the total of 45, so 10 remain for 09-05 to 09-11. Inventory: gate 214, ASan 216, no ASan-only name.
- **Verify blocks (seconds).** Task 1: block 1 9, ASan 11 (6 s build included). Task 2: ASan 3 names about 25 (`unit.core.save.device` 14.4). Task 3: block 25 (the guard test 12). Task 4: block 1 19, ASan 20. Task 5: block 1 19, ASan **45** (`unit.core.save.device` 40.5 s: the section reference states capture 6 MB digest streams). Task 6: block 1 **31**, ASan **55** (`unit.core.save.device` 48.2, `controls.real.save-wrong-byte` 36.1, `runner.save-fixtures` 16.5), gcc:13 closure over 11 units 9, Linux pre-flight **164** (emulated amd64; see efficiency notes). Full gate-preset CTest 50 (4 jobs). Fixture build 1.1.

## Deviations from Plan

**1. [Plan wording] Seven distinct frame images, not eleven.** The eleven transactions of `fixtures/save/eeprom.S` use seven distinct 64-byte frame images because the three info frames and the three block-1 read frames are byte-identical; each transaction still copies its image to RDRAM, so behavior and transcript are as specified.
- **Commit:** `faa2a38`

**2. [Plan assumption] The membership list changed in two commits.** The two new predicates `t64_has_eeprom_save` and `t64_has_sram_save` are listed as version-membership sites (regenerated in `b846c3f`); the shared PI start was renamed from `start_rom_dma` to `start_dma`, which changes the single PI reservation row's function name (regenerated in `5fd0812`; the pair `(PI, complete)` and the site count are unchanged, `unit.core.sched.ledger` passes unchanged). No SRAM bus function adds a row. The plan named `start_dma`, so the rename is the plan's own, and the difference is explained here rather than hidden.
- **Commits:** `b846c3f`, `5fd0812`

**3. [Plan assumption] An edited copy of the exact openIPL3 asset is refused.** The openIPL3 profiles admit the exact 4,096-byte asset or an authored flat ROM (z64 magic, the pinned payload hash over bytes 0x40 to 0xFFF, an entry point at 0x08 and a load length at 0x10 equal to the appended payload). Editing byte 0x3F of the asset gave `INVALID_ROM` (status 5) for the values 0x01, 0x02, 0x03, 0x10 and 0xFF, as the plan anticipated; the selection control therefore edits the header of an authored flat ROM (the asset with its entry point and load length set and a 64-byte zero payload appended, 4,160 bytes). The version byte (six values), a 20-byte title area, the game code and checksum-area bytes 0x14 to 0x1B all admit and change nothing.
- **Commit:** `cb45a25`

**4. [Design choice] Width 8 and masked writes on the SRAM window answer BUS_UNSUPPORTED.** The plan says width other than 4 is BUS_UNSUPPORTED on read and write; the SI region answers `BUS_DEVICE_REFUSED` on the 64-bit read path, so the SRAM region has its own explicit branches in `t64_bus_read_wide` and `t64_bus_write_masked` (the latter is one of the re-pinned bodies).
- **Commit:** `5ad6aaa`

**5. [Plan wording] Tasks 4 and 5 started largely green.** See the RED records: no failure was manufactured; eight mutants provide the current evidence.

**6. [Design choice] Two extra mutants.** X1 and X2 probe the rows the plan named as RED candidates (the overflow refusal and a relaxed write classification); the plan's six are M1 to M6.

**7. [Design choice] The profile-mismatch control uses the real runner binary.** The supervisor derives the profile from the ROM bytes, so it cannot present a ROM under another profile id; the test takes the path of `t64-runner-real` as a fifth positional argument (CMake registration only) and runs it with unlinked descriptors and an explicit `--profile`, after the accepted baselines of the same ROMs through the supervisor and through the runner binary itself.
- **Commit:** `e354ffc`

**8. [Plan wording] Inventory ordering.** The first Task 6 commit appended the three names after the last row, which makes the diff contain a changed (quote-terminated) line; `a8498bd` moves them before the last row so the diff holds only added lines, which the acceptance check requires.

**9. [Plan wording] Stem digest literal.** The plan calls it the 09-03 literal, but no test held one; it was captured from the tree before the first behavior change (Task 1 step 2).

**10. [Requirements] SAVE-02 and SAVE-04 are not marked complete.** As for SAVE-01 and CORE-02 in 09-02 and 09-03: the byte import and export API half of SAVE-04 is plan 09-05, the hosted evidence and the full matrix come later, and Phase 9 qualification is 09-11; `requirements-completed` is empty.

**11. [Additional work] gcc:13 closure scope.** The required units include the changed runner files (`runner/t64_real_fixture.c`, `runner/tests/test_save_fixtures.c`) as the 09-03.1 handoff says; the plan's command lists core units only.

**Total deviations:** 11 (2 plan assumptions, 2 design choices plus 1 extra, 3 wording, 1 requirement note, 1 additional scope, 1 literal capture). **Impact:** no guard weakened; the only pins touched are in the isolated Task 3 commit; none of `scripts/qualify-phase09.py`, `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh`, `scripts/gate.sh`, `scripts/rights-scan.sh`, `tools/rights`, `runner/t64_supervisor.c`, `scripts/check-phase09-cases.py`, `scripts/build-boot-fixtures.sh` changed (empty `git log --name-only` over the paths); `scripts/check-test-inventory.sh` has only added name lines; `core/tests/test_profile.c`, `core/tests/test_ai.c`, `docs/spec/audio-cases.json`, `docs/claims/phase09.md` and `fixtures/pad` are untouched.

## Verification

- Cheap checks first, all clean: `ast.parse` of the oracle, `sh scripts/pii-scan.sh --tree` (1,184 files, 0 findings), `sh scripts/rights-scan.sh --tree` (0 findings), `sh scripts/check-claims.sh --files docs/claims/phase09.md` (ok, 0 blocks), `grep -niw 'accurate\|compatible\|supports' docs/spec/save-media.md` (no line), `sh tools/lint/core-lint.sh` (97 files, 0 violations).
- Task 6 verify block 1: 21 of 21 (the plan's expected count), both case suites validated and run by `scripts/check-phase09-cases.py`; ASan block 5 of 5 with no sanitizer report; guard green on `build/gate` and `build/asan` (84 sources, 67 seeds); inventory passes for both presets; membership check passes.
- gcc:13 closure over the 11 changed units: every unit compiled, no warning. **Linux pre-flight (F-102 procedure):** the 206 compile commands of the gate build rerun with the Ubuntu 24.04 `cc` 13.3 of the act image (`/Users` path to `/src`, `-arch`, `-MD` and `_DARWIN_C_SOURCE` removed, output to `/dev/null`): `LINUX-COMPILE-OK`, no hosted run was made.
- The frozen-file log `git log --name-only --format= 0d10945..HEAD -- <the nine paths>` prints nothing; no `__pycache__` remains.

## Handoff for 09-05 to 09-11

- **Frozen for the rest of Phase 9:** save kinds `T64_SAVE_NONE` 0, `T64_SAVE_EEPROM_4K` 1, `T64_SAVE_EEPROM_16K` 2, `T64_SAVE_SRAM_32K` 3 (requestable but unsupported 4 to 7: FlashRAM, Controller Pak, Rumble Pak, Transfer Pak), the `save_policy` label `save-explicit-eeprom4k16k-sram32k-busy6ms-v1`, statuses `T64_STATUS_SAVE_UNKNOWN_DEVICE` 20, `T64_STATUS_SAVE_UNSUPPORTED_DEVICE` 21, `T64_STATUS_SAVE_DEVICE_MISMATCH` 22 (provisional until Phase 10; **09-05 uses values from 23 upward**).
- **Section 11 order:** marker 11 after section 10, then u32 `kind`, u32 `size_bytes`, u64 `busy_until`, u32 `pi_save_direction` (0 none or ROM, 1 SRAM to RDRAM, 2 RDRAM to SRAM), then `size_bytes` raw storage bytes, all in the new-span class; present only when `profile.save_kind != 0`. Contents freeze at F1 (09-06).
- **Classification rule:** valid only for (0x00,1,3), (0x04,2,8), (0x05,10,1) on an EEPROM profile; any other transaction there is a timeout (cost `2000n + 188 + 3907`); a profile without an EEPROM is absent (same cost); the busy outcome is decided at the execute tick E, never at plan time.
- **Entry points for the import and export half:** `machine->save.bytes` and `t64_save_storage_bytes(kind)` (the storage and its size), `machine->save.busy_until`, `t64_save_in_flight(machine)` (EEPROM deadline in the future at `machine->tick`, or an active non-ROM PI direction; this is the in-flight refusal condition), `t64_save_info` (`kind`, `size_bytes`, `flags`) and `T64_SAVE_INFO_BUSY`, and `t64_save_reset_machine_state` at create and at the admission reset site (a successful replacement restores a blank device; the replacement matrix rows and the table text beyond one row are 09-05). `t64_machine_save_info` is the D-24 check prefix only.
- **Fixtures:** ids `phase09-eeprom-rw-e4k-v1`, `phase09-eeprom-rw-e16k-v1`, `phase09-eeprom-rw-wrong-e4k-v1`, `phase09-sram-rw-s32k-v1`, `phase09-sram-rw-wrong-s32k-v1`, all with the long cap; `scripts/build-save-fixtures.sh` now exists (move it from `PENDING_SOURCE_PATHS` by the file existing; it is a pending source path of the qualifier). A hosted fixture of a later plan appends `HOSTED_ROWS` data; this plan adds no hosted row (the retained-run marker format `private phase09 runner evidence retained at` is not used by `test_save_fixtures`).
- **`harness.rdp-oracle-authority`:** rebind per Open Issue 1 before any push.
- **Do not forget:** `t64_bus_write_masked` and `t64_bus_read_wide` answer BUS_UNSUPPORTED on the SRAM region; the PI read-length register latches and starts only on the SRAM id; the CPU store budget needed no change (a store outside RDRAM already reserves one extra event).

## CI/CD efficiency notes (AUTONOMY section 2)

- **Cheap checks that caught defects before the long ones:** the membership list check (0.2 s) caught the two new predicates and the PI rename in the first run after each edit; the inventory check (0.1 s) caught the name rows; `ctest -R '^harness\.rdp-oracle-authority$'` (0.2 s) shows the rebind need after any manifest edit.
- **Linux pre-flight cost:** 164 s because the container ran under `--platform linux/amd64` emulation on an arm host; the 09-03.1 measurement was about 51 s. Run it without forcing the platform (the act image has an arm64 variant on this host) or keep it for the push only; it found nothing here.
- **ASan time:** `unit.core.save.device` takes 48 s under ASan against 1.5 s native, almost all of it the section 11 reference states (6 MB digest streams captured per state) and the field flips. If hosted ASan time matters later, capture fewer states or flip fewer fields; the budget is met today.
- **Out-of-tree mutants** need only `t64_core`, `libt64_sha256.a` and the test source (about 3 to 10 s each); eight of them ran in one background script.

## Authentication gates

None.

## Known Stubs

None. No placeholder text in the created or modified files (the temporary `t64_machine_save_info` stubs used for the Task 5 RED were replaced in the same task).

## Threat Flags

None beyond the plan's register (T-09-28 to T-09-35): the new surface is the channel 4 EEPROM executor, the SRAM bus window and PI directions and `t64_machine_save_info`, each with its mitigation test; `test_save_fixtures` spawns the real runner binary with unlinked descriptors (test-only).

## Self-Check: PASSED

- The ten created files are tracked (`git ls-files` lists `core/t64_save.c`, `core/tests/test_save.c`, `runner/tests/test_save_fixtures.c`, `fixtures/save/*` five files, `scripts/build-save-fixtures.sh`, `docs/spec/save-media.md`); all thirteen commit hashes above are ancestors of HEAD; `build/09-04-mutants` holds no worktree (`git worktree list` shows the main checkout only).
- `scripts/check-rsp-no-hle.py` changed only in `eeec998` (`5 4`); the reviewed-source guard and its test are green on both builds.
- No ROM, save, PIF/IPL image, 64DD media, hardware measurement or oracle was used; the only external input is the already cleared openIPL3 asset embedded by the authored ROMs. No push was made.
