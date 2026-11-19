---
phase: 08-audio-interface-and-raw-capture
reviewed: 2026-11-19T00:21:45Z
depth: deep
files_reviewed: 44
files_reviewed_list:
  - .github/workflows/ci.yml
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_ai.c
  - core/t64_bus.c
  - core/t64_digest.c
  - core/t64_exception.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_scheduler.c
  - core/t64_warm_devices.c
  - core/tests/test_ai.c
  - core/tests/test_cpu_exceptions.c
  - core/tests/test_isviewer.c
  - core/tests/test_profile.c
  - docs/spec/audio-cases.json
  - docs/spec/audio-interface.md
  - docs/spec/raw-capture.md
  - fixtures/audio/pcm.S
  - fixtures/audio/pcm.ld
  - fixtures/manifest.json
  - runner/CMakeLists.txt
  - runner/t64_audio_observation.h
  - runner/t64_raw_capture.c
  - runner/t64_raw_capture.h
  - runner/t64_real.c
  - runner/t64_real_fixture.c
  - runner/t64_result_check.c
  - runner/t64_supervisor.c
  - runner/t64_video_observation.h
  - runner/tests/t64_observation_pipe.h
  - runner/tests/test_audio_capture.c
  - runner/tests/test_rdp_video.c
  - runner/tests/test_result_v2.c
  - scripts/build-audio-fixture.sh
  - scripts/check-audio-cases.py
  - scripts/check-rsp-no-hle.py
  - scripts/collect-boot-evidence.sh
  - scripts/gate.sh
  - scripts/qualify-audio.py
  - scripts/qualify-rdp-vi.py
  - scripts/tests/test_audio_qualification.py
  - scripts/tests/test_rsp_no_hle.py
findings:
  critical: 1
  warning: 1
  info: 0
  total: 2
  necessity: 0
status: issues_found
---

# Phase 08: Code Review Report

**Reviewed:** 2026-11-19T00:21:45Z  
**Depth:** deep  
**Files Reviewed:** 44  
**Status:** issues_found

## Summary

The complete scoped implementation was reviewed across guest AI scheduling, runner observation and capture, and native/hosted evidence binding. I found a raw-capture publication race and a gap that can admit a partial retained audio observation into qualification evidence. The live in-memory PCM checks remain separate from the retained-record issue.

No tests or qualification commands were run during this source review. Full local and hosted qualification remain pending as planned; that absence is not counted as an implementation finding.

## Narrative Findings (AI reviewer)

### Critical Issues

#### CR-01: Raw capture can publish a substituted staging entry

**Severity:** BLOCKER  
**File:** `runner/t64_raw_capture.c:1231-1249`

**Issue:** The code checks the staging directory’s device/inode through `stage_name`, then later calls `rename_noreplace` with that same name. The stage is a sibling in the caller-selected destination parent, and its name contains a PID and monotonic timestamp (`make_stage_name`, lines 327-337). A process with write/search access to that parent can observe and replace the entry after the check. The test-only barrier at lines 1242-1245 makes the check-to-rename gap explicit. The rename resolves the source name again; the retained `stage_fd` does not bind the rename to the checked directory. No-replace protects only the final target. A replacement directory or symlink can therefore be moved to the final target while the runner sets `committed` and reports success, leaving the real staged bundle inaccessible at the requested path.

**Fix:** Keep the stage entry in a trusted, owner-private directory on the same filesystem, retain that staging parent FD, and rename from that FD to the destination parent with the no-replace primitive. If no safe same-filesystem staging location is available, reject destination parents whose ownership/permissions allow another principal to replace the stage entry. Add a control that substitutes the source entry after the identity check; an additional check immediately before rename would still have the same race.

### Warnings

#### WR-01: Partial retained audio observations can be accepted as evidence

**Severity:** WARNING  
**File:** `runner/tests/test_audio_capture.c:892-895`; `scripts/qualify-audio.py:1648-1649`

**Issue:** `run_supervised` validates the observation in memory, then ignores the return value from `save_evidence` when writing `audio-observation.bin`. That helper can leave a nonempty partial file when a write or close fails. The qualification verifier checks only that the retained file is nonempty, then hashes and includes it in the public projection. If evidence storage fills or returns an I/O error after the bundle has been written, CTest can still pass and qualification can publish a digest for a truncated observer record.

**Fix:** Make the actual-capture test fail if saving its required observation record fails. In `_capture_record`, require the complete expected record size and validate its schema and packet fields against the accepted raw bundle before retaining its digest.

---

_Reviewed: 2026-11-19T00:21:45Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep_
