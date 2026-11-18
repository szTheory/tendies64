---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-18T03:05:06Z
depth: deep
files_reviewed: 2
files_reviewed_list:
  - scripts/qualify-rdp-vi.py
  - scripts/tests/test_rdp_vi_qualification.py
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 07 Plan 11: Hosted Emitter Repair Review

**Reviewed:** 2026-11-18T03:05:06Z
**Depth:** deep
**Files Reviewed:** 2
**Status:** clean

## Summary

The fresh-directory repair closes the hosted emitter setup failure. `_emit_hosted` now allocates a private temporary root and passes its uncreated `original-run` child to `_build_actual_bundle`, whose `directory.mkdir(...)` requires a fresh leaf. The added test exercises the real parser and `main()` dispatch, actual producer, and original-record verifier while isolating only `PUBLIC_ROOT` in a temporary directory. It checks the four emitted files and their source, run, attempt, and job identities. This focused review found no remaining defect in the changed path.

## Recheck

- In `scripts/qualify-rdp-vi.py:1779-1807`, `_emit_hosted` retains its validation of job, source SHA, run ID, run attempt, and checked-out `HEAD`. The new private path is an uncreated child beneath `_private_dir`'s mode-700 directory. `_build_actual_bundle` creates that child at line 760, copies the actual built producer executables, runs the accepted producer and verifies its original result before publication.
- The public output goes through the module's `PUBLIC_ROOT` at lines 1799-1807. The regression imports the real module in a subprocess and overrides only that variable with a `TemporaryDirectory` beneath `build/gate`; it leaves the source root, fixtures, binaries, private evidence root, and producer behavior real. It runs `module.main()` with `--emit-hosted`, and checks exact output membership and manifest identities at `scripts/tests/test_rdp_vi_qualification.py:17-50`.
- The `--selftest` preflight invokes just this named unittest method at `scripts/qualify-rdp-vi.py:1598-1604`. The selected method does not invoke `--selftest`, so it cannot recurse. The separate existing test still invokes the full helper selftest, which now includes the single selected emitter test.
- The retained `entry-regression-before` record reaches the original `File exists` failure. The retained focused logs show the emitter test and full two-test module passing. The initial control that stopped on incomplete job/SHA environment is recorded as setup failure, not as the reproduced emitter failure.
- The actual hosted run recorded in the repair note had both normal gates pass, then both emitters fail before raw production. Local identity values in the regression are test controls, and its public output is temporary. The tests establish the repaired call path locally; they are not a hosted evidence result.

The inspected source matches committed HEAD `bdf4948108792055848e6c8ac201b1def0adfedc`. The root guard and protected owner-file blob checks passed before this report was written. No source files were changed, no tests were rerun, and no commit was created.

---

_Reviewed: 2026-11-18T03:05:06Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_
