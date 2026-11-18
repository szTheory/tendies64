---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-18T01:23:38Z
depth: deep
files_reviewed: 10
files_reviewed_list:
  - .github/workflows/ci.yml
  - docs/spec/rdp-vi-cases.json
  - runner/CMakeLists.txt
  - runner/tests/test_rdp_video.c
  - scripts/check-ci-yml.sh
  - scripts/check-rdp-vi-cases.py
  - scripts/check-test-inventory.sh
  - scripts/collect-boot-evidence.sh
  - scripts/qualify-rdp-vi.py
  - scripts/tests/test_rdp_vi_qualification.py
findings:
  critical: 2
  warning: 2
  info: 0
  total: 4
  necessity: 0
status: issues_found
---

# Phase 07 Plan 11 Segment A: Code Review Report

**Reviewed:** 2026-11-18T01:23:38Z  
**Depth:** deep  
**Files Reviewed:** 10  
**Status:** issues_found

## Summary

The review traced the producer through CTest, the Python evidence verifier, CI emission, archive collection, and the hosted pair comparison. Two bounded local controls demonstrated that the public verifier accepts consistently resealed but altered raw bytes, and that the timeout wrapper can return while a child process remains alive. The real hosted collection path also bypasses the project's strict ZIP parser, and `--native` records `HEAD` without binding the working tree it builds. The source-root guard passed; the protected `AGENTS.md` and `.planning/config.json` blobs still match their supplied IDs. Full native, ASan/UBSan, GCC, and live hosted runs were not repeated; those are pending Segment B, not review findings.

## Narrative Findings (AI reviewer)

## Critical Issues

### CR-01: BLOCKER — Public evidence accepts forged raw bytes when both host records are resealed

**File:** `scripts/qualify-rdp-vi.py:790-805, 1081-1101`
**Issue:** `verify_evidence(..., public=True)` checks public bytes against hashes in the same manifest, but only the private path compares the RDRAM and packet bytes to `EXPECTED_SOURCE` and `EXPECTED_PACKET` (lines 797-798). `_verify_collected_pair` then checks that Linux and macOS bytes agree; it does not restore an independent authored-byte check. I changed one RDRAM byte on both host records, recomputed the byte hash, `private_originals` hash, and public serialization hash/length, and the complete pair verifier accepted them. Repro: `build/gate/07-11-review/public-pair-forgery.json`.

**Fix:** Compare decoded public bytes to the authored contract before accepting public evidence, or bind them to an independently authenticated original record. For this authored case, the verifier can reject after decoding:

```python
if decoded["rdram.bin"] != EXPECTED_SOURCE or decoded["vi-packet.bin"] != EXPECTED_PACKET:
    raise Refusal("public raw bytes differ from the authored output")
```

### CR-02: BLOCKER — Timeout leaves spawned descendants running

**File:** `scripts/qualify-rdp-vi.py:144-170, 226-255, 1113-1125`
**Issue:** `_run_bounded` delegates to `subprocess.run(timeout=...)`, which terminates the direct child but does not terminate its process group. The native gate and hosted collector are launched through shell scripts that start further processes. A bounded control started a child that spawned a four-second grandchild; the wrapper returned at its one-second deadline while that grandchild was still alive. The repro records that result at `build/gate/07-11-review/deadline-process-tree.json`. A timed-out gate or collector can therefore keep writing build or evidence files after the helper has reported refusal.

**Fix:** Start each command in a new process group/session and terminate and reap the whole group on timeout (use the platform equivalent where process groups are unavailable). Add a bounded parent/descendant control and verify no descendant remains after timeout.

## Warnings

### WR-01: WARNING — Actual hosted ZIPs are extracted before the strict archive checks run

**File:** `scripts/collect-boot-evidence.sh:249-264, 363-410`; `scripts/qualify-rdp-vi.py:851-916, 1454-1459`
**Issue:** The production collector calls `gh run download` directly into an extracted directory, then validates the resulting filesystem tree. The duplicate/path/mode/expanded-size checks in `_safe_extract_upload` run only through `--extract-upload`; the collector's mock `gh` uses that command, but the real `gh run download` path does not. Once an extractor has normalized or overwritten ZIP members, `verify_upload` cannot detect duplicate archive entries or independently enforce the project's pre-extraction path and size policy. The API artifact digest is also collected and printed, but is not compared with the downloaded archive bytes. This review confirms the project's ZIP validator is bypassed in the real call path; it does not assert a specific undocumented `gh` extractor exploit.

**Fix:** Download the artifact archive by its selected artifact ID, verify the downloaded archive SHA-256 against the API digest, and pass those bytes through `--extract-upload` before any extraction. Keep the current exact post-extraction member check as a second validation.

### WR-02: WARNING — Native qualification labels a potentially dirty build as `HEAD`

**File:** `scripts/qualify-rdp-vi.py:226-247, 1313-1321`
**Issue:** `_run_native` records only `git rev-parse HEAD` as `source_sha`, then runs the gate and builds from the checkout working tree. `_git_head` does not inspect or hash working-tree changes, so a tracked source, test, or harness edit can be compiled and tested while the evidence is labeled with the previous commit SHA. This weakens the exact source-to-result binding for local native qualification.

**Fix:** Bind the run to a source-tree identity that includes all relevant tracked and untracked build/test inputs, or refuse when any such input differs from the selected commit. Keep unrelated protected owner files outside that input set rather than requiring a blanket clean-worktree check.

---

_Reviewed: 2026-11-18T01:23:38Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep_
