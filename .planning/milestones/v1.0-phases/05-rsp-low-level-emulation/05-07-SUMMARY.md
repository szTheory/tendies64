---
phase: 05-rsp-low-level-emulation
plan: "07"
subsystem: rsp-integer-divider
requires:
  - phase: 05-06
    provides: Portable integer VU reference and function dispositions.
provides:
  - Exact permitted ISC divider-source audit and complete copy provenance.
  - Integer-only reciprocal/rsqrt words and fetched high/low latch execution.
  - Independent rational/square bounds and clean authored audit controls.
affects: [05-08, 05-09, rsp-local-evidence]
requirements-completed: []
completed: 2026-11-14
status: complete
---

# Plan07 — Audited integer divider

The saved selected ares source, ISC notice and pinned path-history receipt pass
the actual local audit. No license/history contradiction was found. Only the
approved integer generation, normalization/result and high/low latch equations
were adapted into the small attributed helper. The copy record and full notice
cover that file; original interpreter code fetches, validates and commits its
effects. No broad VU code, literal table dump, host float, dependency or upstream
emulator execution was added.

All six reciprocal/rsqrt operations execute through IMEM fetch. Tables are
computed on demand with bounded integer arithmetic, and pending input/result
latches remain instance-owned. The active development profile names the divider
integer policy while historical profiles retain their contracts. Selected
software formulas and guide-derived ACC broadcast remain qualification limits;
these are not physical-ROM or hardware-equivalence claims.

## Verification and review

- Final stable-source canonical gate passes all124 tests, lint86/0, exact
  inventory controls and authored evidence tail. Log:
  build/gate/05-07-final-gate.log.
- All-target normal and sanitizer builds precede12 owning checks, both passing
  in build/{gate,asan}/05-07-review-ctest.log. Inventories are124/126, with only
  the two planned real registrations added.
- Optimized GCC13 objects pass with owning core/test flags in
  build/gate/05-07-review-gcc-{core,tests}.log. The actual saved-byte audit,
  docs/diff, provenance and rights/PII scans pass.
- A genuine fetched VRCPH RED precedes implementation. Tests check every512
  table index using rational/squared-integer bounds, literal sign/zero/minimum
  anchors, mixed high/low and high-high sequences, VMOV interaction and distinct
  machine latches. Observed retirement is required after absolute tick advance.
- Parent review replaced the reciprocal oracle's repeated division with
  cross-multiplied bounds and moved all adapted latch equations inside the
  attributed helper. This is a verified refactor/oracle strengthening starting
  from GREEN, with no invented behavior-failure claim.

[Review evidence](05-07-review-evidence.md) preserves original attempts, audit/
copy-record ordering, ACC interpretation and absolute-tick harness corrections.
[Provenance report](../../../docs/evidence/rsp-divider-provenance.md) records
exact selected source, license, history, modifications and accuracy boundaries.
Configured full phase code/security review remains Plan10 work. Both hosted gates pass repaired checkpoint5e89cb4 in run43575884934,
including all124 checks, lint86/0 and authored evidence/publication tail.
The first Linux run stopped at upstream tool-source transport; the bounded,
hash-preserving official alternate and updated fixture-source pins are
recorded separately in the review evidence and F-071/F-072.

## Next action

Continue Plan08 bounded per-opcode reference/invariant fuzz, separate authored
stress lane and executable no-HLE controls. No external diagnostic guest or
Cargo build ran during Plan07. Private receipts retain their exact tested
identities; changed profile/core bytes require deliberate requalification before
future actual diagnostic use. No user action is needed.
