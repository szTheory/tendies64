---
phase: 08
fixed_at: 2026-11-19T01:11:44Z
review_path: .planning/phases/08-audio-interface-and-raw-capture/08-REVIEW.md
iteration: 1
findings_in_scope: 2
fixed: 2
skipped: 0
status: all_fixed
---

# Phase 08: Code Review Fix Report

**Fixed at:** 2026-11-19T01:11:44Z  
**Source review:** `.planning/phases/08-audio-interface-and-raw-capture/08-REVIEW.md`  
**Iteration:** 1

**Summary:**
- Findings in scope: 2
- Fixed: 2
- Skipped: 0

## Fixed Issues

### CR-01: Raw capture staging source can be replaced before publication

**Files modified:** `runner/t64_raw_capture.c`, `runner/tests/test_audio_capture.c`, `scripts/qualify-audio.py`, `docs/spec/raw-capture.md`  
**Commit:** `cc18852`  
**Applied fix:** The capture bundle is now a child of a same-filesystem mode-0700 staging parent. The implementation retains and validates that parent's file descriptor, then publishes the child from that descriptor into the retained destination-parent descriptor. Cleanup only removes entries whose recorded identity still matches; a successful target rename cannot be turned into failure by later cleanup.

**Targeted proof:** The new precommit stage-container substitution control first failed on the old implementation: it replaced the visible stage entry after the prior inode check, and the published bytes differed from the accepted baseline. The preserved RED evidence is in `build/gate/08-fix-cr-stage-red-ctest.log`, `build/gate/08-fix-cr-stage-red.junit.xml`, and the before/after LastTest copies. The focused race passed after the fix in `build/gate/08-fix-cr-stage-green-ctest.log` and `build/gate/08-fix-cr-stage-green.junit.xml`. The subsequent raw-capture controls batch, including incomplete, interrupted, target-error, and race controls, passed in `build/gate/08-fix-cr-controls-ctest.log`.

### WR-01: Partial retained audio observations can be accepted as evidence

**Files modified:** `runner/tests/test_audio_capture.c`, `scripts/qualify-audio.py`, `scripts/tests/test_audio_qualification.py`  
**Commit:** `6a956aa`  
**Applied fix:** Required observer and status evidence-save results now affect the harness result. The accepted-capture path requires complete video/audio observer writes and preserves failed partial originals. The killed-process path still allows an incomplete observer record. The verifier now maps the native version-2 audio observation layout from the C declarations, requires its exact supported 64-bit little-endian Linux/macOS size (3,976 bytes), checks schema and completion fields, and compares packet metadata, bytes, profile, and build identity to the accepted raw bundle and independent literals.

**Targeted proof:** The new same-setup partial-write control first failed as intended: the guest result was `pass`, the full bundle matched its baseline, but the harness returned success with a 1,988-byte observer. The RED log and records are in `build/gate/08-fix-wr-io-red-ctest.log`, `build/gate/08-fix-wr-io-red.junit.xml`, and `build/gate/08-04-evidence/raw-transaction.nzqMp5/`. After the fix, `controls.real.raw-capture-io-errors` passed in `build/gate/08-fix-wr-io-fixed-ctest.log`.

The nine-test normal capture/error batch passed in `build/gate/08-fix-wr-normal-ctest.log` and `build/gate/08-fix-wr-normal.junit.xml`. The same nine selected tests passed under ASan/UBSan in `build/asan/08-fix-wr-asan-ctest.log` and `build/asan/08-fix-wr-asan.junit.xml`. The verifier selftest passed all 21 tests in `build/gate/08-fix-wr-evidence-selftest.log`. A retained actual capture passed `_capture_record` and its paired race checks in `build/gate/08-fix-wr-actual-record-verify.log` (2 audio packets, 29 video packets, completion tick 551001). The updated `harness.audio-qualification` CTest also passed in `build/gate/08-fix-wr-harness-ctest.log`.

## Verification Notes

Builds and targeted checks ran in the main checkout (`workflow.use_worktrees=false`). The normal and ASan source/body gate passed with 81 reviewed sources. Python AST parsing, `git diff --check`, and the targeted native builds passed. The owner `AGENTS.md` and `.planning/config.json` working blobs were preserved unchanged.

Full phase qualification remains stopped pending the parent's independent recheck. No hosted Linux runtime run or full qualification was started by this fix pass.

---

_Fixed: 2026-11-19T01:11:44Z_  
_Fixer: the agent (gsd-code-fixer)_  
_Iteration: 1_
