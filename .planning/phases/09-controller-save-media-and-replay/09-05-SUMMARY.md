---
phase: 09-controller-save-media-and-replay
plan: "05"
subsystem: save-media
tags: [save-import, save-export, d24-order, byte-order-selections, write-generation, verified-descriptor, no-clobber-publisher, rights-scan, oracle, mutants]

requires:
  - phase: 09-controller-save-media-and-replay
    provides: "09-04 the save storage, t64_save_in_flight, t64_machine_save_info and the SAVE_* statuses 20 to 22, section 11, the build-save-fixtures builder; 09-02 the artifact publisher, the argv bound, per-row cap and pin; 09-03.1 the frozen qualifier and case checker"
provides:
  - provisional core API t64_machine_save_import and t64_machine_save_export with the exact D-24 order R0 to R9, statuses 23 to 25 and the RAW, SWAP16 and WORDREV32 selections (authored policy)
  - observer-only write_generation (outside the digest) cleared by replacement
  - runner flags --save-import trio, --save-kind, --save-format and --save-export-leaf with supervisor validation, a verified 32 KiB descriptor and export through the no-clobber publisher after a pass
  - authored save-roundtrip ROM, independent roundtrip oracle, fixture and manifest row
  - nine closed artifact detail names accepted only for a host-error stop
  - two CTest names, five rights selftest rows, the save-transfer case suite and the Host import and export specification section
affects: [09-06, 09-07, 09-08, 09-09, 09-10, 09-11]

actuals:
  tokens: 51900
  tasks: 6
  commits: 12
plan_head_before: ebc0a1c2410ec2b6abbb4176bc8043bb946ffca9
plan_head_after: 865a60bff5dc997043cf1e569f5e53b6a49fb9ed

tech-stack:
  added: []
  patterns:
    - "every check precedes the first read of a caller byte and the first mutation, so a refusal is bit-identical by construction and no staging buffer exists"
    - "an independent ordered rule function over literal D24_ORDER rows decides every expectation; production code is never asked what to expect"
    - "whole-structure checksum plus explicit items per request, full digest per group and per first row, because a digest costs 14 ms"
    - "out-of-tree mutants (throwaway worktree, gate flags with -Werror) prove each named comparator bites"
    - "publisher opened before the guest, export only after a pass, abort otherwise"

key-files:
  created:
    - core/tests/test_save_transfer.c
    - runner/t64_save_flags.h
    - runner/tests/test_save_roundtrip.c
    - fixtures/save/roundtrip.S
    - fixtures/save/roundtrip.ld
    - fixtures/save/roundtrip_oracle.py
  modified:
    - core/include/tendies64/t64_machine.h
    - core/t64_internal.h
    - core/t64_save.c
    - core/CMakeLists.txt
    - runner/t64_supervisor.c
    - runner/t64_real.c
    - runner/t64_result_check.c
    - runner/t64_real_fixture.c
    - runner/t64_verified_input.h
    - runner/CMakeLists.txt
    - scripts/build-save-fixtures.sh
    - scripts/check-test-inventory.sh
    - scripts/rights-scan.sh
    - fixtures/manifest.json
    - docs/spec/save-media.md
    - docs/spec/machine-contract.md
    - docs/spec/result-protocol.md
    - docs/spec/phase09-cases.json

key-decisions:
  - "Import and export check admission (R1) before the pointer and span row (R2); the 09-04 info function keeps its pointer-first order because it must write its output; the difference is recorded in docs/spec/save-media.md"
  - "A zero length never constructs a span (an empty span cannot overlap or overflow), for import and for export; a NULL import pointer is still R2"
  - "write_generation lives in t64_save_state, counts successful imports, is cleared by t64_save_reset_machine_state (create and replacement) and is not hashed by the digest walker"
  - "One --save-format governs the import and the export of a run; a conversion between two selections is a file conversion outside the run"
  - "The publisher is opened before the guest runs, so an occupied target, a bad leaf grammar or an unsafe parent ends the run at guest tick 0 with the publisher's own error name"
  - "The byte-order selections are authored policy under assumption A1; no mapping to any other emulator is asserted"

requirements-completed: []

coverage:
  - id: F1
    description: "Core import, export and info with the D-24 order, three transforms at 512, 2,048 and 32,768 bytes, success and refusal postconditions, in-flight refusal through the real SI and PI paths, replacement"
    requirement: "SAVE-03"
    verification:
      - kind: unit
        ref: "unit.core.save.import-export (nine cases; 15,008 checks native, mutation-bounded 80,789 checks under ASan)"
        status: pass
    human_judgment: false
  - id: F2
    description: "Runner import through a verified descriptor and export through the no-clobber publisher in every format selection, with every control reaching its own comparator after an accepted baseline"
    requirement: "SAVE-03"
    verification:
      - kind: integration
        ref: "runner.save-roundtrip (nine cases)"
        status: pass
    human_judgment: false
  - id: F3
    description: "Unknown, unsupported and mismatched device requests refused with defined postconditions on the API side"
    requirement: "SAVE-04"
    verification:
      - kind: unit
        ref: "unit.core.save.import-export (refusal-matrix, no-caller-read, export-refusals, mutation-bounded)"
        status: pass
    human_judgment: false
  - id: F4
    description: "Exported leaves carry only scanner-denied extensions, are refused by the rights scanner (staging and tree) and by the public member scan; no key or log line carries save bytes"
    requirement: "SAVE-03"
    verification:
      - kind: integration
        ref: "gov.rights.selftest (five rows), runner.save-roundtrip (upload-allowlist-save, no-save-bytes-in-result)"
        status: pass
    human_judgment: false
  - id: F5
    description: "Specification wording: transforms recorded as authored policy, no sentence about hardware, no reserved claim word"
    requirement: "SAVE-03"
    verification: []
    human_judgment: true
    rationale: "Wording review against the claim boundary is a judgment; the claims and PII scans and the reserved-word grep are automated"

duration: 45min
completed: 2026-11-20
status: complete
---

# Phase 9 Plan 05: Save import and export, byte-order selections and the roundtrip fixture Summary

**A provisional core API that imports and exports exactly 512, 2,048 or 32,768 save bytes in RAW, SWAP16 or WORDREV32 under an exact nine-row refusal order with bit-identical refusal postconditions and an observer-only import generation, an authored SRAM roundtrip ROM, and runner flags that import through a verified descriptor and export through the no-clobber publisher only after a pass, with exported leaves limited to scanner-denied extensions.**

## OPEN ISSUES FOR THE ORCHESTRATOR (read first)

1. **`harness.rdp-oracle-authority` fails again** (the expected kind): the manifest gained the roundtrip row and the five 09-04 rows had their source-content hash text refreshed. Not edited (owner-stop area). Computed, not applied, with a read-only script (the old request reproduced its own pin first, `True`). The manifest has not changed since commit `87ae11e`, so these hold at HEAD `865a60b`:

   | Field | Pinned now | At HEAD |
   | --- | --- | --- |
   | manifest `sha256` (`fixture_manifest` and `bound_authored_inputs[1]`) | `abbf9c737add67c5ef07617eca9141fc797abf6c61c87db9be4b7a49e41ca2f2` | `c1788a2b13daf3c5aa4f0fd5f28c225155fad14d519d2c4ed5b1eaa1786a3034` |
   | manifest `git_blob_id_sha1` (same two places) | `0f65fb046c6331b4de667ac9d90508a16177ffc1` | `622b4f0cca6007efb514f84b7b3e133cdec73ff0` |
   | `request_sha256` and `PINNED_REQUEST_SHA256` | `9df6a059c9d2696074f4621e4085c7c2c743d790bd6a7f6411be533594676d36` | `6d05bd4f0a5b067c5f541d82d79994335bd00dc0bef2d7036fde53c57a8caa61` |

   The cheap check is `ctest --preset gate -R '^harness\.rdp-oracle-authority$'` (0.2 s).
2. No full `sh scripts/gate.sh` and no push (the plan reserves the full gate for the end of 09-06). The plan sweep is green (below); the gate-preset base selection of 35 names stayed 100 percent after Task 1.
3. Deviations 1 to 4 below are plan-text inconsistencies resolved without a design change; none touches a frozen file.

## Performance

- **Duration:** 45 min (2026-11-20 00:01Z to 00:47Z, from the plan start to the summary commit) for six tasks.
- **Commits:** 12 measured from the ledger (`ebc0a1c..865a60b`, before this summary).
- **Files:** 24 changed (4,984 insertions, 53 deletions), 6 of them created.

## Commits

| Commit | Task | Subject |
| --- | --- | --- |
| `fded882` | 1 step 2 | header, `D24_ORDER` literals, vectors, temporary NOT_READY stubs and the formal RED |
| `dbb2d08` | 1 step 3 | core API: shared prefix, transforms, import, export, `write_generation` |
| `c9be030` | 1 step 4 | roundtrip ROM source, oracle, builder stem, CMake (the manifest `source.revision`) |
| `87ae11e` | 1 step 4 | fixture row, manifest row, runner test and the runner RED |
| `f57768d` | 1 step 5 | flags, verified descriptor, publisher export, closed detail names, GREEN tracer |
| `c90f051` | 2 | matrix, transforms at full size, postconditions, no-caller-read, in-flight, replacement |
| `4f3edad` | 3 | `mutation-bounded` sweeps and the seeded campaign |
| `607c76e` | 4 | seven runner cases |
| `58cd48a` | 5 | five rights selftest rows (this commit touches `scripts/rights-scan.sh` only) |
| `1861ade` | 5 | `upload-allowlist-save` |
| `1638692` | 3 follow-up | relative asset fallback so the plan's direct-run verify command works |
| `865a60b` | 6 | specification, replacement row, `save-transfer` suite |

## Accomplishments

- **Core API (Task 1 steps 2 and 3).** `t64_machine_save_import` and `t64_machine_save_export` in `core/t64_save.c`: one file-local prefix helper (rows R3 to R6) shared with `t64_machine_save_info`, explicit 2-byte and 4-byte group transforms straight into the storage (no large stack array, no allocation), caller bytes read only after R9. Statuses 23 to 25, formats 1 to 3, trailing `write_generation` in `t64_save_info`.
- **Fixture and oracle (step 4).** `fixtures/save/roundtrip.S` reads SRAM offsets 0x0000 and 0x7FF0, rewrites the W block at 0x0100 and the V block at 0x7FF0 and prints four 36-byte lines. `fixtures/save/roundtrip_oracle.py` (stdlib, transform as an offset mapping) reproduced both hand-derived transcripts exactly.
- **Runner flags (step 5).** Six supervisor options with usage-class validation, a verified descriptor (`T64_VERIFIED_SAVE_MAX_BYTES` 32,768), forwarding rows inside the argv bound (slot sum row `save flags 8`, sum 31 of 63), publisher opened before the guest, export after a pass, nine artifact detail names in the result checker and the protocol document.
- **Matrix (Task 2).** An independent ordered rule function over the `D24_ORDER` literals; 1,527 refusal requests across the four family ids, admitted and unadmitted, idle and busy, each proven to reach its row; forked reads against a `PROT_NONE` span for rows R3 to R9 (import and export); the real SI write path and a real PI DMA for the in-flight rows; replacement.
- **Campaign (Task 3).** 552 sweep requests plus 2,000 seeded requests (20,000 under ASan), every row reached, every success verified against the offset mapping.
- **Runner cases (Task 4).** Five format pairs, EEPROM passthrough on both admit ROMs, a mis-declared format reaching the transcript comparator, thirteen refused imports, the publication rules, the no-save-bytes scan, the maximum option set.
- **Rights and upload (Task 5).** Five selftest rows (95 to 100 cases, 27 added lines, 0 deleted) and a case that runs both scanners on four real exported leaves.

## RED and tracer records

**Task 1 step 2 (formal RED).** With the temporary stubs (`T64_STATUS_NOT_READY` from import and export), `tracer-sram-raw` on the admitted SRAM id: `t64_machine_save_import ... got 7 want 0` (NOT_READY where OK is required) and `info.write_generation got 0 want 1`; then export `got 7 want 0`, `length got 0 want 32768`, the byte comparison and the digest-differs check failed (`checks=39 failures=6`, `build/gate/09-05-t1-step2-red.out`). Setup was valid: the info check on the same machine passed with kind 3, size 32,768, no busy bit and generation 0. GREEN: 69 checks, 0 failures. `D24_ORDER` was committed (`fded882`) before the first non-stub body (`dbb2d08`).

**Task 1 step 4 (runner RED).** The ROM with the row present and no save flags (the SRAM is blank): outcome `guest-fail`, detail `wrong-transcript`, stop `authored-completion`, 144 bytes, `log_sha256` `25206d24...f198`; the capture was `R0 ` and `R1 ` with 32 zero digits each and `W0`/`W1` equal to the rewritten blocks (so the ROM and the DMA path were already correct; only the imported content was missing). Completion tick **540,634**, below the default cap 600,000, so the row keeps the default cap. GREEN (`f57768d`): pass, `positive-match`, 144 bytes, `log_sha256` equal to the oracle, published `roundtrip.sra` of 32,768 bytes, mode 0600, digest equal to the oracle export value, one directory entry (no stage left), record validated by `t64-result-check`.

**Start-green rationales (Tasks 2 to 4).** Every group of Task 2 passed on its first run against the Task 1 implementation, because the ladder and the transforms already existed; the formal RED milestone is the Task 1 step 2 record and the wrong-value controls for the matrix are the seven mutants below. The replacement group started green because Task 1 step 3 already cleared the generation in `t64_save_reset_machine_state` (`grep -n write_generation core/t64_save.c` shows the clear and the 09-04 hook at `core/t64_machine.c` lines 2150 and 2672 runs it at create and at the replacement reset site, after the candidate image is prepared), so no `core/t64_machine.c` edit was needed. `mutation-bounded` is a widening case and started green. The Task 4 cases started green against the Task 1 flags; the only failures were two test-side expectations (below). The Task 5 rights rows started green because the three extensions were already denied; the new behavior being proven is the real-leaf shape, and the manifest-only row is the accepted baseline of the same setup.

## 09-04 reconciliation (Task 1 step 1)

Every symbol the plan lists under "Needs from 09-04" exists with the stated meaning, by grep: `t64_machine_save_info(machine, requested_kind, info_out)` (`core/t64_save.c`), `t64_save_in_flight`, `t64_save_reset_machine_state` (called at `core/t64_machine.c` 2150 and 2672), `active_save_direction` (`core/t64_pi.c`, `core/t64_internal.h`), `t64_save_storage_bytes`, the statuses 20 to 22 and the kinds 4 to 7 (`core/include/tendies64/t64_machine.h`), `core/t64_save.c` in `CANONICAL_CORE_SOURCES`, `PHASE09_TESTS=` in the inventory script, `t64_artifact_publisher_open` and `scan_public_members` in their files. Nothing was redefined. The base selection (35 names) passed 100 percent before and after Task 1 (`build/gate/09-05-t1-prebase.out`, `09-05-t1-base.LastTest.log`). `check-rsp-no-hle.py` stayed green after every core edit (no pinned body was touched).

## Out-of-tree mutants (nothing committed)

Driver `build/09-05-mutants/run_mutant.sh LABEL CASE` with the replacement rules in `build/09-05-mutants/mutants.py` (both in the ignored build directory): `git worktree add --detach build/09-05-mutants/wt-LABEL HEAD`, `cmake --preset gate`, `cmake --build --preset gate --target t64_core t64_sha256` (gate flags, `-Werror`), `cc` with the gate warning flags and `-Werror` over `core/tests/test_save_transfer.c`, the baseline case, the patch, the same build and the named case, then `git worktree remove --force`. For every row: core build exit 0, test compile exit 0, baseline exit 0 (no failure), mutant exit 1; `git worktree list` shows the main checkout only. The summary line of each mutant over all cases also reports failures (a mutant bites its named case and the wider cases that share its comparator).

| Mutant | Named case | First failing comparator |
| --- | --- | --- |
| M1 length check before the format check | `refusal-matrix` | `import ... format=99 length=513: got 24 want 23 (row R7)`, 6 failures |
| M2 busy check before the kind checks | `refusal-matrix` | `import id=...-si-v1 (no device) busy=1 kind=3: got 25 want 22 (row R3)`, 61 failures |
| M3 caller byte read before the last check | `no-caller-read` | `child ended by signal 10` (SIGBUS) on each of the eight import rows, 8 failures; the export rows are unaffected |
| M4 SWAP16 as the WORDREV32 permutation | `transform-vectors` | `memcmp(image_b, vector_raw, 8u) == 0` at the 8-byte vector, 14 failures |
| M5 import keeps the deadline and the generation | `import-export-roundtrip` | `machine->save.busy_until got 500 want 0`, `write_generation got 0 want 1`, then the stale-deadline digest comparison, 24 failures |
| M6 generation incremented before the checks | `refusal-matrix` | `state changed by 'matrix': ... generation 0`, 1,594 failures |
| M7 busy compare inclusive of the deadline tick | `in-flight-real` | `do_import ... got 25 want 0` at the deadline tick, `busy_until got 407876 want 0`, 4 failures |

## Measurements

- **Completion tick of the roundtrip run:** 540,634 (supervised, bound 600,000).
- **Oracle values** (`python3 -B fixtures/save/roundtrip_oracle.py --scenario S --bytes|--sha256`) and the independent `shasum -a 256` over the literal lines, all equal:

  | Scenario | Bytes | SHA-256 |
  | --- | --- | --- |
  | `baseline` | 144 | `649b62940ace25d9e4c31e12381f38df779a5bbef90dcf0baf1be1458e6b75c4` |
  | `swap16-as-raw` | 144 | `3717361f6b106331021025e25a738598b01a52bffb68bd33507b0ec948369128` |

  The oracle reproduced the plan's hand-derived transcripts exactly (nothing adjusted): `R0 0b30557a9fc4e90e33587da2c7ec1136`, `R1 d6fb20456a8fb4d9fe23486d92b7dc01`, `W0 c35a96e10f78b24d218ef63469d017ab`, `W1 18e57cb903d64a91ff2ea760c8358b5e`, and the scenario values `300b7a55c49f0ee95833a27decc73611` and `fbd645208f6ad9b423fe6d48b79201dc`.
- **ROM:** `phase09-save-roundtrip-s32k`, 5,000 bytes, SHA-256 `ec744b26449cafbc73fb5c0e8ebaceb58573cfd224bb8e30adae361927f977eb`, payload 904 bytes `bec4b400...8a69`, completion `0x800006a4`, source-content hash `0e3ed2db45ed139b02f51a52446c1c06fd2c08c167d45bb483da080790a2f82e`, manifest `source.revision` `c9be030374029365c469dca20d39508f3aceef61`. Two builds are byte-identical; the five 09-04 ROM identities are unchanged (the builder asserts them against their manifest rows).
- **Counts.** `unit.core.save.import-export`: 15,008 checks native; the ASan campaign ran 20,552 requests (3,674 successes; refusals by row R1 1,510, R2 4,494, R3 3,556, R4 1,960, R5 1,655, R6 847, R7 1,105, R8 1,353, R9 398). Native: 2,552 requests (sweeps 552, campaign 2,000, 435 successes, rows R1 397, R2 487, R3 399, R4 226, R5 183, R6 98, R7 144, R8 143, R9 40). Matrix: 1,527 refusal requests.
- **Name budget (K13).** Gate names added: exactly two (`unit.core.save.import-export`, `runner.save-roundtrip`). **Cumulative Phase 9 gate names: 37** (35 + 2) of the total of 45, so 8 remain for 09-06 to 09-11. Inventory: gate 216, ASan 218.
- **Timed inventory selftest:** `sh scripts/check-test-inventory.sh --selftest` 20 s (removal_controls=434, malformed_controls=5).
- **Verify blocks (seconds).** Task 1: block 1 19 (build, 2 names, 35-name base selection, guard, lint, inventory, membership, scans), ASan block 9 build plus 3. Task 2: block 1 14 (`unit.core.save.import-export` 1.5), ASan block 27 (`unit.core.save.import-export` 24). Task 3: block 1 2.4, ASan campaign alone 5.0 (20,000 requests). Task 4: block 1 5 (`runner.save-roundtrip` 2.4), ASan block 68 (`runner.save-roundtrip` 65). Task 5: 12 (`gov.rights.selftest` 11). Task 6 sweep: scans and claims 0, lint 9, gcc:13 closure over eight units 8, case checker 4, governance batch 8, ASan selection 70 (`unit.core.save.import-export` 28, `runner.save-roundtrip` 70 in parallel), inventory selftest 20. gcc:13 closure over the three core and test units alone 3. **Linux pre-flight 110** (208 compile units, Ubuntu 24.04 gcc 13.3 of the act image, no `--platform`, `LINUX-COMPILE-OK`; the image has no arm64 variant on this host, so it runs emulated).

## Mapping of the validation names (9-V14 to 9-V16)

| Validation name | Case |
| --- | --- |
| `unit.core.save.refusal-matrix` | `refusal-matrix`, with `no-caller-read` and `export-refusals` |
| `unit.save.mutation` | `mutation-bounded` (and the seven out-of-tree mutants above) |
| `controls.real.save-import-refused` | `import-refused` in `runner.save-roundtrip` |
| `harness.upload-allowlist-save` | `upload-allowlist-save` in `runner.save-roundtrip` |

## Rights selftest rows (Task 5, commit `58cd48a`)

Pin: the existing `selftest` case list of `scripts/rights-scan.sh`. Old count 95, new count 100 (`selftest: cases=100 failures=0`). Added data rows only: `staging-manifest-only-clean` (the accepted baseline), `staging-save-export-eep`, `staging-save-export-sra`, `staging-save-export-sav` and `tree-save-export-sra`; diff 27 added lines, 0 deleted, no scanner logic, rule or message changed, `tools/rights` untouched. Causing need: exported saves carry the denied extensions by construction, so the scanner must be proven to refuse real-shaped leaves in staging and tree. Controls: the manifest-only row accepts, each save-extension row exits 1 with `R-DENIED-EXT <leaf>`.

## Deviations from Plan

**1. [Plan wording] "four save-capable ids" is three plus the no-device id.** The family has four ids and three of them carry a device (`-e4k`, `-e16k`, `-s32k`). Success rows run on the three; refusal rows run on all four (the stem refuses at R3). No coverage was dropped.

**2. [Plan inconsistency] One `--save-format` for import and export.** The plan lists a single `--save-format TOKEN` (and the slot sum counts one format pair) but its format-matrix and same-setup rows name distinct import and export formats. No second flag was invented (the flag names freeze at F1 and 09-07 to 09-09 reuse them). The same-format rows run exactly as planned (import encoded in the format, export in the same format, digest equal to the oracle export value). The cross rows (swap16 to raw, wordrev32 to swap16, and swap16 imported while the wordrev32 reading is compared) run in the import format and the published file is converted by the independent offset mapping before its digest is compared with the oracle value of the target format; every cross pair through the API is exercised in the core cases (`transform-vectors`, `import-export-roundtrip`). If distinct selections per direction are wanted, a second flag is a Phase 10 change.
- **Commit:** `607c76e`

**3. [Behavior] An occupied target is refused at publisher open, at guest tick 0.** The plan expects a host-error stop with detail `artifact-exists`; that holds, but the no-replace check happens when the publisher is opened (before the guest), so the record has guest tick 0, 0 log bytes and no milestones, not the completion tick. The original file bytes are unchanged and no stage is left.
- **Commit:** `607c76e`

**4. [Plan wording] `argv-max-save` baseline.** The observation validators belong to the audio and VI fixtures, so on a family id every run with the video or audio descriptor ends as `harness-error`, stop `host-error`, detail `video-observation-invalid`. That is the accepted baseline of this setup (the twin run with the descriptors and the artifact pair but no save flag); the full option set ends with the same outcome, its transcript digest equals the oracle value (the flags were parsed, forwarded and used) and nothing is published (a non-pass run publishes nothing); the same set plus a raw capture directory is usage. The test-side `MAX_ARGS` of 40 truncated the full list silently once (a usage record); it is 64 now.
- **Commit:** `607c76e`

**5. [Metadata] Source-content hash refreshed in the five 09-04 manifest rows.** The builder's source hash covers the builder script and the new sources, so every stem's metadata hash changed from `72f1bb5c...` to `0e3ed2db...`. The ROM SHA-256 and size of the five 09-04 ROMs are unchanged (asserted by the builder against their rows); only the recipe text and `expected.revision` were refreshed so they match what the script prints.
- **Commit:** `87ae11e`

**6. [Design choice] Comparison state of the matrix.** A full digest costs 14 ms (10.6 MB of machine storage), so per request the test compares a checksum over the whole machine structure (79 KB, every device field outside RDRAM) plus the explicit D-24 items (device bytes, deadline, queue head and count, next sequence, tick, trace count, allocator counters, generation), and the full digest per group and on the first call of each row. A refusal side effect that persists is caught at the call that made it; one that restores itself would need a self-reverting mutation, which none of the seven mutants is.
- **Commit:** `c90f051`

**7. [Design choice] A zero export capacity constructs no span.** Writing the independent rule function showed that the shared overlap helper treats an empty span strictly inside the machine storage as overlapping; export now skips span construction for a zero capacity, like import for a zero length (a row of `export-refusals` pins it).
- **Commit:** `c90f051`

**8. [Plan wording] Tasks 2 to 4 started largely green.** See the rationales above; no failure was manufactured and the evidence is the seven mutants.

**9. [Test infrastructure] The plan's direct-run verify command needs the asset path.** `./build/gate/core/test_save_transfer --case mutation-bounded` without `T64_TEST_IPL3_PATH` failed at the asset read; the test now falls back to `build/fixtures/ipl3_compat.z64` relative to the repository root (CTest still sets the variable).
- **Commit:** `1638692`

**10. [Plan wording] `t64_sha256` is linked to `test_save_transfer` though unused** (the plan lists it); no effect.

**11. [Requirements] SAVE-03 and SAVE-04 are not marked complete.** As in 09-02 to 09-04: the claim blocks and the hosted evidence are 09-11, so `requirements-completed` is empty.

**Total deviations:** 11 (4 plan-text, 2 design choices, 1 behavior note, 1 metadata, 1 test infrastructure, 1 wording, 1 requirement note). **Impact:** no guard weakened and no frozen file logic-edited: `git log --name-only --format= ebc0a1c..HEAD -- scripts/qualify-phase09.py .github/workflows/ci.yml scripts/check-ci-yml.sh scripts/gate.sh tools/rights scripts/check-rsp-no-hle.py scripts/check-phase09-cases.py runner/t64_raw_capture.c runner/t64_real_result.c scripts/build-boot-fixtures.sh` prints nothing; `scripts/check-test-inventory.sh` has two added name lines and no deleted line; `scripts/rights-scan.sh` has added selftest cases only, in its own commit.

## Verification

- Cheap checks first, all clean: `ast.parse` of the oracle, `sh scripts/pii-scan.sh --tree` (1,191 files, 0 findings), `sh scripts/rights-scan.sh --tree` (0 findings), `sh scripts/check-claims.sh --files docs/claims/phase09.md` (ok, 0 blocks), `grep -niw 'accurate\|compatible\|supports' docs/spec/save-media.md` (no line), `sh tools/lint/core-lint.sh` (97 files, 0 violations), `python3 -B scripts/list-membership-sites.py --check` (ok).
- `python3 -B scripts/check-phase09-cases.py --build build/gate --suite save-transfer --run` validated and ran the suite; both names pass natively and under ASan (no sanitizer report); the guard is green on `build/gate` and `build/asan` (84 sources); the inventory passes for both presets; the governance batch (`gov.manifest.*`, `gov.provenance.repo`, `gov.claims.repo`, `gov.rights.*`) passes.
- gcc:13 closure over `core/t64_save.c`, `core/t64_machine.c`, `core/tests/test_save_transfer.c`, `runner/t64_supervisor.c`, `runner/t64_real.c`, `runner/t64_result_check.c`, `runner/t64_real_fixture.c` and `runner/tests/test_save_roundtrip.c`: every unit compiled, no warning.
- The result record keeps its closed 22-key set (`no-save-bytes-in-result`), no hex string of the imported image, the W block or the V block appears in the record, the meta file or the supervisor output (they do appear in the guest transcript, which proves the scan strings are the right ones), `git log` over `runner/t64_real_result.c` is empty, and no `__pycache__` remains.

## Handoff for 09-06 to 09-11

- **API surface (provisional until Phase 10, F1 freeze at the end of 09-06):** `t64_machine_save_import`, `t64_machine_save_export`, `t64_save_info.write_generation`, the statuses 23 to 25, `T64_SAVE_FORMAT_*`, the flag names, the token lists and the nine artifact detail names (`artifact-cap`, `artifact-dir-unsafe`, `artifact-exists`, `artifact-leaf-name`, `artifact-no-replace-unsupported`, `artifact-rename`, `artifact-stage`, `artifact-substitution`, `artifact-write`), which 09-07 and 09-08 reuse for their own artifacts.
- **For 09-08:** whether `write_generation` travels in the observer-continuity section of the state container is open; it is outside the digest and reset by replacement.
- **For Phase 10:** guest writes do not increment the generation; per-direction format selection would need a second flag; the SWAP16 and WORDREV32 definitions are policy (A1).
- **Publisher timing:** the publisher is opened before the guest, so any export problem is found at tick 0; a later artifact (replay, state) can follow the same shape.
- **`harness.rdp-oracle-authority`:** rebind per Open Issue 1 before any push.

## CI/CD efficiency notes (AUTONOMY section 2)

- **Cheap checks that paid off:** the inventory check (0.1 s) after each name row; running the plan's own verify command text (not a paraphrase) found the missing asset path of a direct run before it could fail a reviewer; the builder's new identity assertion (the five 09-04 ROM hashes against their manifest rows) turns a silent ROM change into a one-line failure.
- **Digest cost drove the test design:** 14 ms per digest meant a per-request digest over the 1,527-request matrix would cost 21 s native and a 20,000-request ASan campaign minutes; a structure checksum plus explicit items ran the campaign in 5 s under ASan with the same bite (the seven mutants).
- **ASan cost of the runner test:** `runner.save-roundtrip` takes 65 to 70 s under ASan (about 35 supervised runs at roughly 1.8 s each) against 2.4 s native; it fits the 300 s timeout. If hosted ASan time matters later, share one baseline run across the control cases.
- **Linux pre-flight:** 110 s for 208 units under amd64 emulation (the act image is amd64 only here); it found nothing. Keep it for the push, run once.
- **Mutant driver:** eight to ten seconds per mutant (core build in a worktree, test compile, two runs); seven ran in about four minutes in one background script. A `for m in "A b"` loop with word splitting fails under zsh; the script uses explicit calls.

## Authentication gates

None.

## Known Stubs

None. The temporary NOT_READY bodies of `fded882` were replaced in `dbb2d08`; no placeholder text exists in the created or modified files.

## Threat Flags

None beyond the plan's register (T-09-36 to T-09-45): the new surface is the core import and export entry points, six supervisor options, the verified import descriptor and the publisher export, each with its mitigation test; `test_save_transfer` forks children and `test_save_roundtrip` runs the two scanners as subprocesses (test-only).

## Self-Check: PASSED

- The six created files are tracked (`git ls-files` lists `core/tests/test_save_transfer.c`, `runner/t64_save_flags.h`, `runner/tests/test_save_roundtrip.c`, `fixtures/save/roundtrip.S`, `fixtures/save/roundtrip.ld`, `fixtures/save/roundtrip_oracle.py`); all twelve commit hashes above are ancestors of HEAD; `git worktree list` shows the main checkout only.
- `scripts/check-rsp-no-hle.py`, `scripts/qualify-phase09.py`, `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh`, `scripts/gate.sh` and `tools/rights` were not modified; the reviewed-source guard is green on both builds.
- No ROM, save, PIF/IPL image, 64DD media, hardware measurement or oracle was used; the only external input is the already cleared openIPL3 asset embedded by the authored ROMs, and every save image is an index-derived authored pattern generated at test time. No push was made.
