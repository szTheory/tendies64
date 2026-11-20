---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-18T02:11:36Z
depth: deep
files_reviewed: 1
files_reviewed_list:
  - scripts/collect-boot-evidence.sh
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 07 Plan 11 Segment A: Final Focused Recheck

**Reviewed:** 2026-11-18T02:11:36Z
**Depth:** deep
**Files Reviewed:** 1
**Status:** clean

## Summary

The remaining archive download bound warning is resolved. The collector streams through `head -c 1048577`, so it retains at most one byte beyond the one-megabyte limit before refusing, and it checks the API producer's exit status separately from the sink. The shell waits for the pipeline to complete before those checks; the overflow control retained the exact 1,048,577-byte prefix and the producer's exit code `141`, while the valid-archive/producer-failure control retained the full 7,254-byte archive with producer exit code `1` and was refused. Both errors occur before the archive extraction call.

The saved actual-producer selftest exited 0 in 7.47 seconds with 34 rejection controls. Its collector SHA-256 matches both the source file and committed HEAD `2c8e9ef487792970b588b14ea253f4a706d0a861`; the saved command's `source_sha` is the precommit base `2973555ab6439e5e69acf6cc26a0881942656c27`. The overflow and producer-failure runs used mocked transport. They establish no hosted proof. Full native, ASan/UBSan, GCC, and live hosted runs remain pending the later segment.

## Resolved Finding

The warning from the repair recheck, “Artifact response is size-checked only after it is written,” is resolved at `scripts/collect-boot-evidence.sh:264-284`. The bounded sink stops after 1,048,577 bytes; the collector rejects that length before digest verification or extraction. It also refuses a valid-length archive if the API producer exits nonzero. The retained overflow records at `build/gate/07-qualification/hosted-download.2ZZR02/` show the exact byte count and exit code; the producer-failure records at `build/gate/07-qualification/hosted-download.uDTdrm/` show the full archive bytes and exit code `1`. The focused collector control asserts the overflow prefix size and existence of its producer exit record before the test can pass.

No source defect remains in this narrowly scoped download-bound path. The earlier four Segment A findings remain resolved as recorded in the repair recheck.

## Bounded Verification

- Verified the current and committed collector SHA-256 against the saved selftest command record.
- Inspected the retained final selftest command and outcome: return code 0, 7.47 seconds.
- Inspected the 2 MiB overflow control's retained 1,048,577-byte response prefix and producer status `141`.
- Inspected the valid archive plus failed producer control: 7,254 response bytes, producer status `1`, and refusal before extraction.
- The source-root guard passed before report creation. Protected owner-file Git blob IDs still match their supplied values.

---

_Reviewed: 2026-11-18T02:11:36Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_
