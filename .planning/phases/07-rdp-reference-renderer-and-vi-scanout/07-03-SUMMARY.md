---
phase: 07-rdp-reference-renderer-and-vi-scanout
plan: "03"
subsystem: emulation
tags: [n64, rdp, tmem, texture, vi, c17, ctest]

requires:
  - phase: 07-02
    provides: [shared DPC transport, ordered command effects, and incremental raster events]
provides:
  - Per-machine 4 KiB TMEM, eight tiles, source-backed load commands, and copy-mode sampling
  - Independently authored copy pixels in RDRAM and raw VI output
  - Exact source, function-body, build-object, and archive ownership for the TMEM module
affects: [phase-07, rdp, tmem, vi, profile-serialization, qualification]

actuals:
  tokens: 35355
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - Validate complete fetched commands and source/destination bounds before mutating TMEM or output
    - Keep texture state per machine and include future-affecting fields in the canonical digest
    - Seal every new production function and its owning native object/archive route

key-files:
  created:
    - core/t64_rdp_tmem.c
    - core/tests/test_rdp_copy.c
  modified:
    - core/t64_rdp.c
    - core/t64_internal.h
    - core/t64_digest.c
    - docs/spec/rdp.md
    - scripts/check-rsp-no-hle.py
    - scripts/tests/test_rsp_no_hle.py

key-decisions:
  - "Version the expanded composite profile as descriptor 6/model 11/state 16 and hash all new TMEM, tile, TLUT, conversion, and copy state."
  - "Refuse direct RGBA32 and YUV copy tiles as documented; load YUV16 and test the named conversion helper without claiming rendered YUV pixels."
  - "Keep the full requirement set open: this plan contributes to VID-01 and VID-05 but completes neither requirement."

patterns-established:
  - "Fetched texture commands preflight their complete RDRAM and TMEM spans before applying effects."
  - "The strict RDP/TMEM ownership guard pins source functions, body digests, object edges, disk objects, and archive members."

requirements-completed: []
requirements-contributed: [VID-01, VID-05]
coverage:
  - id: D1
    description: "Fetched TMEM loads feed copy-mode pixels written to RDRAM and checked through raw VI output."
    requirement: VID-01
    verification:
      - kind: unit
        ref: "python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite copy --run --artifact-prefix 07-03-copy-final-shift"
        status: pass
    human_judgment: false
  - id: D2
    description: "TMEM, tile, TLUT, and conversion state participate in the versioned profile digest."
    requirement: VID-05
    verification:
      - kind: unit
        ref: "CTest unit.core.vi.fill and unit.core.profile; scripts/gate.sh"
        status: pass
    human_judgment: false
  - id: D3
    description: "The new TMEM source and object cannot be omitted or changed without the strict ownership controls failing."
    verification:
      - kind: other
        ref: "python3 -B scripts/tests/test_rsp_no_hle.py (31 mutation controls)"
        status: pass
    human_judgment: false

duration: 93m
completed: 2026-11-16
status: complete
commits: 3
plan_head_before: 2f00845efcc5cc398a7e8477225601ef26cb7f8f
plan_head_after: 8deb5a62f9cc8120e0bf123fae91eec425eea994
---

# Phase 07 Plan 03: TMEM Loads and Copy-Mode Pixels Summary

**Fetched TMEM loads and copy-mode texture rectangles now produce bounded, independently checked RDRAM pixels.**

## Performance

- **Duration:** About 93 minutes.
- **Started:** 2026-11-16T15:42:43Z.
- **Completed:** 2026-11-16T17:16:12Z.
- **Tasks:** 2 of 2.
- **Changed source files:** 20, measured from the plan base to the last task commit.
- **Token metric:** source-diff characters/4 estimate for GSD accounting, not measured model usage or billing.
- **Task commits:** 3, measured from the persisted plan ledger.
- **Diff size:** 141,422 characters across the plan changes (35,355 chars/4 estimate tokens).

## Accomplishments

- Added per-machine 4096-byte TMEM, eight tile descriptors, texture-image state, TLUT state, conversion coefficients, and bounded SetTextureImage, SetTile, SetTileSize, LoadTile, LoadBlock, LoadTLUT, and SetConvert routes. Validation rejects invalid source and destination spans before these commands change state.
- Added copy-cycle texture rectangles and flipped rectangles with source offsets, tile origins, coordinate shifts, wrap/mirror/clamp, scissor clipping, supported RGBA16/CI/IA/I sampling, and incremental RDRAM writes. The 20-case authored matrix checks expected TMEM bytes, pixels, first differences, and a raw VI frame.
- Extended the composite profile to descriptor 6/model 11/state 16, updated its identity and runner fixture pins, and added every new future-affecting texture field to the canonical digest. Earlier profiles retain their existing identity and DPC refusal policy.
- Added exact ownership for `core/t64_rdp_tmem.c`, including its source/object/archive route, complete function inventory, normalized body digests, and mutation controls for a missing object and an injected TMEM write.

## Verification

- **Copy matrix:** `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite copy --run --artifact-prefix 07-03-copy-final-shift` passed all 20 registered cases. The accepted fetched copy reaches the pixel comparator and VI byte comparison. The owning output and binary binding are retained at `build/gate/07-03-retained-artifacts/copy-v2-shift-green/`.
- **Accepted legacy fill path:** `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite fill --run --artifact-prefix 07-03-fill-final-profile` passed all five owners: core RDP fill, VI fill, the real runner fixture, wrong-pixel rejection, and replacement-fixture refusal.
- **Affected profile consumers:** `ctest --preset gate -R '^(unit\.core\.dp\.transport|unit\.core\.profile|unit\.core\.isviewer|unit\.core\.digest-trace|unit\.core\.rsp-fuzz)$'` passed 5/5.
- **Strict source/object guard:** `python3 -B scripts/check-rsp-no-hle.py --root . --build build/gate` passed with 77 reviewed production sources and exact archive membership. `python3 -B scripts/tests/test_rsp_no_hle.py` passed 31 mutation controls, including injected TMEM mutation and missing TMEM object. The full gate also passed `repo.rsp-no-hle`.
- **Full normal gate:** `sh scripts/gate.sh` passed all 147 CTests, fixture qualification, gate and ASan test-inventory self-tests, rights/provenance checks, and core-lint. The full output is `build/gate/07-03-full-normal-gate.log` (SHA-256 `6f154ed8655dd4fcf9bd5c774c0828e6fb4ac8f57b3d39dab7400af95379a804`); the owning `LastTest.log` is `build/gate/07-03-full-normal-LastTest.log` (SHA-256 `eb733fa83711a6664b0389a2a724f7c4c81f5f86e3ae30f4cd19af4479fa8055`). Exact binaries, fixture, and logs are bound at `build/gate/07-03-retained-artifacts/normal-full-gate/binding.json`.
- **ASan/UBSan:** `cmake --preset asan` and the all-target `cmake --build --preset asan` completed; `sh scripts/check-test-inventory.sh --preset asan` reported 149 tests. A focused 11-test selection passed copy/TMEM, RDP, VI, DPC transport, profile, ISViewer, digest, RSP fuzz, accepted fill, and both fill controls. Exact instrumented consumers and logs are retained at `build/asan/07-03-retained-artifacts/asan-affected-green/binding.json`. The full ASan CTest inventory was not run.
- **Sanitizer stack review:** The ASan/Debug compile reports four new TMEM frame warnings: `t64_rdp_tmem_validate_command` 3104 B, `t64_rdp_tmem_apply_command` 2976 B, `t64_rdp_tmem_validate_copy` 2512 B, and `sample_texel` 2944 B. Related RDP frames are `apply_pending_command` 3328 B, `t64_rdp_event` 3104 B, and `t64_rdp_write_register` 2960 B. `-fstack-usage` records show no large local buffer or recursion; the largest local scratch is the 3-byte RGB array in `sample_texel`. The deepest bounded load route has six nonrecursive frames, about 12.7 KiB summed; the copy-row route is five frames, about 8.9 KiB summed. These are compile-time ASan/Debug frame estimates, not runtime peak measurements. The existing CMake sanitizer rule keeps these diagnostics visible but nonfatal. Records and the reviewed call-chain sums are retained in `build/asan/07-03-retained-artifacts/asan-affected-green/stack-usage.json`.

## Task Commits

| Commit | Change |
| --- | --- |
| `727c4843ce54718d2e4eb2995ae3e6b389a37484` | Registered and ran the first failing TMEM tile-dispatch case (RED). |
| `ea96f77bde05360f10e6e978c6b467b17929e0bf` | Implemented TMEM loads, state/digest integration, and copy pixels. |
| `8deb5a62f9cc8120e0bf123fae91eec425eea994` | Sealed the TMEM source/object route and added mutation controls. |

## Decisions Made

- Updated the profile identity and serialization versions together because TMEM and tile state change future machine behavior; kept older profile behavior unchanged.
- Followed the Nintendo texture restrictions for copy mode: direct RGBA32 and YUV tiles are refused. YUV16 load bytes and the named conversion helper are tested, but no rendered YUV pixel claim is made.
- Kept copy sampling point-based. Filtering, combiner, blender, alpha compare, coverage, and depth remain outside this plan.

## Deviations from Plan

- **Scoped compatibility wiring omitted from the file list:** Updating the approved profile/state policy also required `core/t64_machine.c`, `core/t64_isviewer.c`, `runner/t64_real.c`, `runner/t64_real_fixture.c`, `runner/tests/test_rdp_video.c`, `core/tests/test_rdp.c`, `core/tests/test_vi.c`, `docs/spec/vi.md`, and `fixtures/manifest.json`. These direct consumers were updated so the version triplet, profile digest, fixtures, and retained older profiles remain consistent. No unrelated profile or peripheral behavior changed.
- **TDD commit chronology:** The first accepted RED was observed before the implementation was complete, but production edits had begun before the test-only RED commit `727c484` landed. The summary records the actual order and does not claim the RED commit preceded all implementation edits. A second RED for inverted tile bounds was also retained before its correction.

**Total deviations:** 2 documented, both within the parent-approved compatibility and executor scope.

## Issues Encountered

- The first accepted fill-suite attempt used an old composite-profile digest pin in the runner fixture. Updating the runner fixture and test pins to the versioned profile digest resolved it; the five-owner fill suite then passed. The initial failure and successful retry remain in separate `07-03-fill-v2*` and `07-03-fill-final-profile*` artifacts.
- The ASan/Debug build emitted the frame-size warnings listed above. Focused ASan/UBSan tests passed, and source/stack-usage review found bounded, nonrecursive paths without large local buffers; no broad redesign was needed.

## User Setup Required

None.

## Next Phase Readiness

The source-backed TMEM load and copy-mode pixel slice is ready for later texture/filter work. Direct YUV and RGBA32 copy remain intentionally refused, and no hardware-accuracy or broad compatibility claim follows from these authored tests. This plan contributes to VID-01 and VID-05 but completes neither; `requirements-completed` is intentionally empty. VID-04 remains blocked pending the authority and actual-comparison plans. External-rights and release stops remain unchanged.

## Self-Check: PASSED

The summary exists, the three task commits are ancestors of the plan head, the persisted ledger matches the declared three commits, and `git diff --check` passed for the plan changes.

---
*Phase: 07-rdp-reference-renderer-and-vi-scanout*
*Completed: 2026-11-16*

## Orchestrator closeout

The executor completed implementation and verification, then encountered an
oversized routine-summary write. The orchestrator stopped only that bookkeeping
turn, reconciled the three existing task commits and draft, and finalized this
summary under AUTONOMY.md's ownership rule. Implementation was not redispatched.
The parent copy consumer passed1497checks with zero failures. Both retained
full-normal and affected-ASan artifact sets match their recorded sizes/SHA-256
values; their recorded source hashes match the committed tree. Captures remain
labeled post-verification retention, not retroactive pre-use qualification.
Source review and hosted qualification follow; this summary does not complete
Phase07 or mark any full VID requirement achieved.

## Review qualification hold

The independent [initial review](07-03-REVIEW-INITIAL.md) found five raw command
contract defects. Tests shared incorrect mode/field encodings with the decoder.
The parent independently reproduced the raw-word failures; the earlier green
cohort therefore does not establish valid raw copy support. Its exact artifacts
remain intact. Source publication and Plan04 stay on hold while the
[raw contract](07-03-RAW-CONTRACT.md) is corrected and requalified.

## Corrected raw-command qualification

The initial qualification hold is resolved by source fix `4eeb32b` and independent
[recheck](07-03-REVIEW-RECHECK.md). COPY mode, S/T packing, TLUT enable/type, signed
stepping and inclusive endpoints now match the pinned primary input contract.
Left/top clipping preserves texture origin. The corrected suite has28named cases
including independent literal packets and expected pixels; parent execution
passed2123checks/0failures. The final fractional case uses a distinct source
pattern with a decoy at the old address.

The full corrected normal preset CTest cohort passed147tests and affected
ASAN/UBSan coverage passed11owners. A final test-data-only strengthening passed
both copy owners afterward. The preset log was initially mislabeled a full
`scripts/gate.sh` run; the parent separately executed the required core-lint tail
(90files/0violations), reusing unchanged wrapper self-test and fixture evidence.
The initial governance failure from generated Python bytecode remains preserved.
The full149ASAN inventory remains unrun. See [fix evidence](07-03-REVIEW-FIX.md).
Exact corrected consumers/logs are retained under
`build/gate/07-03-raw-fix-retained-artifacts/{gate,asan}/binding.json`. No prior
artifact or source identity is relabeled. Hosted qualification follows.

The initial stack-usage analysis is tied to the initial source; the arithmetic
correction adds no large buffers or recursion, but that analysis is not presented
as a new runtime peak measurement. Hardware/scissor quad-lane behavior, rendered
YUV and VID-04 remain unqualified.

## Hosted failure and combined repairs

Hosted run44013595697 failed on two independent paths. Linux GCC diagnosed
possibly uninitialized LoadBlock validation state. macOS reached the native
cohort but its timeout control exposed duplicate process-group cleanup masking
the intended refusal. Initial logs remain in ignored build evidence and F-084.

Root's literal LoadBlock regression also exposed an incorrectly scaled LRS and
refusal of the standard zero-line contiguous producer. Commit9247fdb initializes
validation state and corrects integer LRS, linear word progress, per-T skips and
within-word odd swapping. Normal and instrumented copy pass2142checks; strict
guards,32mutations and lint pass. Standard zero-origin producer words are pinned;
nonzero-origin hardware semantics remain explicitly unqualified.

The scoped debugger reproduced the macOS failure and committed55d298e, removing
three duplicate cleanup blocks while preserving unconditional outer group cleanup
and first-signal error propagation. Six controls and runner.cpu-diagnostic pass.
See [debug evidence](../../debug/resolved/mac-owned-group-double-cleanup.md).
Independent [CI repair review](07-03-CI-REVIEW.md) is clean.

The parent executed the actual combined `scripts/gate.sh` wrapper after both
repairs:147tests plus every wrapper stage pass,56seconds total. Exact final
consumers/logs are retained at
`build/gate/07-03-final-combined-retained-artifacts/binding.json`. Previous source
and artifact records remain unchanged. A fresh hosted pair follows.
