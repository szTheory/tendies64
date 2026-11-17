---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T04:34:48Z
depth: standard
source: 938eab631556378e577308cce939a2a222ac3371
baseline: 2fd44ca
files_reviewed: 6
files_reviewed_list:
  - core/t64_rdp_tmem.c
  - core/tests/test_rdp_copy.c
  - docs/spec/rdp.md
  - scripts/check-rsp-no-hle.py
  - scripts/run-cpu-diagnostic.py
  - scripts/tests/test_cpu_diagnostic_run_controls.py
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Hosted CI Repair Review

**Reviewed:** 2026-11-17T04:34:48Z  
**Depth:** standard, limited to the two hosted-CI repairs  
**Status:** clean; no remaining defect found in the scoped changes.

## Scope and assessment

Reviewed the LoadBlock validation/application changes, authored packet cases, checker body seals, and RDP contract; also reviewed `run_bounded` cleanup and the added authored process controls. The Plan03 raw-copy fixes were not re-audited.

The LoadBlock path now treats LRS as the direct final texel index in both validation and application, while retaining the existing 10.2 ULS origin conversion. Validation computes the source span and each DXT-selected 64-bit destination word before applying the command. The zero-line case maps successive words contiguously; nonzero Line adds the documented per-T skip to the monotonically advancing word index. Odd-line half swapping is performed inside each already-validated word by `tmem_store`. The literal cases cover zero-origin LRS=7, width-1/line-zero contiguous source traversal, DXT half swap, nonzero Line skip, and an end-of-TMEM refusal that preserves prior TMEM (`core/t64_rdp_tmem.c:309-350,385-423`; `core/tests/test_rdp_copy.c:387-447`). The checker refreshes digests for both changed production functions, and the retained source guard evidence reports 77 source/object edges and 32 mutation controls (`scripts/check-rsp-no-hle.py`; `build/gate/07-03-load-block-retained-artifacts/binding.json`).

The runner now has a single cleanup owner: exceptional exits reach the outer handler, which signals the owned process group, ignores only `ProcessLookupError`, waits for the leader, and re-raises. The timeout and capture-limit branches no longer kill and wait before raising, so their intended `Refusal` is not followed by a duplicate signal that could mask it (`scripts/run-cpu-diagnostic.py:299-350`). Added controls assert one signal for timeout, closed-pipe timeout, capture overflow, and unexpected exceptions; they preserve a first-signal `PermissionError`, verify cleanup after the group leader exits while a descendant holds a FIFO, retain the existing bound, and verify success does not signal (`scripts/tests/test_cpu_diagnostic_run_controls.py:90-187`).

## Evidence and limits

The retained LoadBlock artifacts record the corrected source, normal and ASAN copy consumers, RED/GREEN vectors, source/object guard, 32 mutation controls, and lint. The debugger record reports six bounded-process controls passing, the owning `runner.cpu-diagnostic` CTest passing, and baseline/mutant checks reproducing the duplicate-cleanup failure. I did not run tests or touch build outputs because the combined `scripts/gate.sh` run is active; its result and hosted CI qualification remain parent-owned.

The primary-source inconsistency around nonzero LoadBlock origin is documented accurately: zero-origin SDK producer behavior establishes direct LRS for the tested vector, while the existing 10.2 source-origin software policy remains and nonzero-origin hardware conformance is unclaimed. This review makes no broader RDP accuracy, qualification, or phase-completion claim.
