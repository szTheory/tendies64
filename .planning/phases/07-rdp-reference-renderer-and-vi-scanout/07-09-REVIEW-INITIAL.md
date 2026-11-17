---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T19:57:08Z
depth: deep
files_reviewed: 13
files_reviewed_list:
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_machine.c
  - core/t64_vi.c
  - core/tests/test_profile.c
  - core/tests/test_vi.c
  - docs/spec/rdp-vi-cases.json
  - docs/spec/vi.md
  - scripts/check-rsp-no-hle.py
  - scripts/check-test-inventory.sh
  - scripts/tests/test_rsp_no_hle.py
findings:
  critical: 2
  warning: 3
  info: 0
  total: 5
  necessity: 0
status: issues_found
---

# Phase 07-09: Code Review Report

**Reviewed:** 2026-11-17T19:57:08Z  
**Depth:** deep  
**Files Reviewed:** 13 source, test, specification, guard, and build files  
**Status:** issues_found

## Summary

I traced VI field scheduling and latching through the accepted event/scheduler path, and checked profile admission, serialization, digest state, packet copying, and registered test consumers. Two reachable VI behaviors can stop or reject valid scanout; three additional warnings concern field parity, packet metadata, and compatibility test coverage. The Plan09 plan, raw contract, VI field notes, and execution evidence were also reviewed.

No tests were run during this review. The execution record says the first accepted consumer run passed; the earlier setup refusal was not a semantic RED. The reviewed evidence does not establish a preimplementation semantic RED or hardware/VID-04 accuracy. The parent has retained the cited artifacts and will run the final integrated gate after fixes.

## Narrative Findings (AI reviewer)

### Critical Issues

#### CR-01: BLOCKER — A latched H_SYNC change can make the next field boundary unreachable

**File:** `core/t64_vi.c:720-744`  
**Issue:** At a field boundary, `next_boundary_units` is first set from the old `hline` at line 720. Pending registers are then activated and the new `hline` and `period` are calculated at lines 721-743, but the next boundary is not recalculated. This is reachable because an H_SYNC write while enabled is accepted and latched at lines 462-468. For example, H_SYNC 4 and V_SYNC 8 produce an old line/period of 5/40 units. Changing H_SYNC to 5 at the next-field latch produces a new line/period of 6/48, while scheduling starts at boundary 5 and then visits 11, 17, …, 47, 53. It never visits 48; the `boundary > period` guard at line 580 then disables scanout with a phase-overflow refusal. A valid timing update therefore stops subsequent fields.

**Fix:** After activating pending timing registers, set the next boundary from the new line period before scheduling. Keep CURRENT sampling and the field phase aligned with that new boundary. Add an accepted API test that changes H_SYNC (and separately V_SYNC) at the field latch and checks that consecutive packets continue.

#### CR-02: BLOCKER — The output-height limit is incorrectly applied to interlaced source rows

**File:** `core/t64_vi.c:127-148`  
**Issue:** The code maps the last output row to a source row, doubles it for serrate at lines 129-130, then rejects it when it reaches `T64_VIDEO_MAX_HEIGHT` (240) at line 133. That constant bounds the output packet allocation, not the source image. A valid 320×240 interlaced output maps its final row to source row 478 or 479; if the whole source span is within RDRAM, the documented v8 policy permits it, but this check rejects it. The same check rejects progressive source rows beyond 239 even when their addresses fit in RDRAM. Thus the implemented limit reduces interlaced output to at most 120 rows despite the 240-row packet capacity.

**Fix:** Bound output dimensions by packet capacity, then validate the computed final source address against RDRAM without using the output-height constant as a source-row limit. Add a full-height interlaced acceptance case and source-span first/last/beyond-RDRAM controls. The spec already defines serrate mapping and RDRAM span validation at `docs/spec/vi.md:79-82`.

### Warnings

#### WR-01: WARNING — CURRENT parity uses the old mode when serrate changes at a field latch

**File:** `core/t64_vi.c:716-725`  
**Issue:** `current_readback` is updated from `active[T64_VI_CONTROL]` before pending registers replace `active`. If CONTROL changes between progressive and serrate at the accepted next-field latch, CURRENT keeps the previous mode's parity until a later full-line sample. The packet for the completed field correctly uses the old mode, but CURRENT now describes the newly active field. Existing latch tests change ORIGIN only (`core/tests/test_vi.c:473-515`), so they do not exercise this transition.

**Fix:** After latching pending registers, recompute CURRENT's parity from the new CONTROL value and the incremented field, consistent with the selected sampled-line policy. Add before/on/after boundary tests for serrate transitions and assert CURRENT before the next line sample.

#### WR-02: WARNING — v8 packet start metadata mixes packed registers with decoded endpoints

**File:** `core/t64_vi.c:670-673`  
**Issue:** `info.h_start` and `info.v_start` receive the complete packed raw register words, while `info.h_end` and `info.v_end` receive decoded end coordinates. For example, V_START `0x00020008` is reported as `v_start=0x00020008, v_end=8`, although the start coordinate is 2. The v8 specification says packet metadata reports H/V endpoints (`docs/spec/vi.md:92-94`), and the geometry decoder already has the decoded starts. Consumers cannot treat these four fields as a consistent endpoint tuple.

**Fix:** Populate v8 start/end fields from decoded geometry. If raw register words are also needed, expose them as separately named raw fields and preserve the older profiles' established metadata behavior. Assert all four endpoint values in the v8 metadata tests.

#### WR-03: WARNING — The v7 predecessor's exact profile bytes are not pinned by the compatibility test

**File:** `core/tests/test_profile.c:773-835`  
**Issue:** The metadata test looks up the depth-v7 profile, but checks legacy hash and serialized-length goldens only when `index < 4` (triangle-v3 through blender-v6). Plan09 requires older profile bytes to remain exact, and `core/tests/test_vi.c:855-891` checks v7's version numbers and policy strings only. A change to the v7 public profile hash or serialized bytes would therefore pass these compatibility assertions.

**Fix:** Add v7 hash and serialized-length goldens from the retained pre-Plan09 artifact and assert them alongside the existing v3-v6 goldens. Keep the v2 legacy-profile compatibility assertion where its profile identity is exercised.

---

_Reviewed: 2026-11-17T19:57:08Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep_
