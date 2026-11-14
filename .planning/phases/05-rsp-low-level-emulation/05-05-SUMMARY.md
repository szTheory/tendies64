---
phase: 05-rsp-low-level-emulation
plan: "05"
subsystem: rsp-instruction-reference
requires:
  - phase: 05-04
    provides: Authenticated early startup/refusal and separate RSP adapter.
provides:
  - Fetched scalar/control/CP0 instruction coverage.
  - COP2 moves and vector-memory transforms with complete-state matrices.
  - Primary-guide encoding ledger and explicit unsupported boundaries.
affects: [05-06, 05-07, rsp-local-evidence]
requirements-completed: []
completed: 2026-11-14
status: complete
---

# Plan05 — Scalar/control and vector transfers

The functional RSP profile now fetches the scalar integer, branch/link, byte/half/
word memory, CP0 and COP2 move paths. It implements the guide-defined normal,
packed and transpose vector memory forms with signed offsets and low12 DMEM
wrapping. DP register operations, reserved fields, branch-in-delay-slot and
unsupported memory alignments refuse explicitly before their instruction effects.
Historical profiles retain their behavior; no new dependency or optimized path
was added.

The instruction ledger maps defined and refused encodings to fetched tests.
Transfer cases cover all encoded elements and memory alignments, checking full
vector/DMEM images and preserved flags/accumulators; illegal cases are included
as transactional refusals. Raw-word and literal bit-pattern anchors complement
the matrix oracle. The guide has no defined LWC2 subopcode10 load; this slot is
refused rather than inventing the task brief's proposed LWV behavior.

## Verification and review

- The final stable-source canonical gate passes all119 tests, lint84/0, exact
  inventory controls, authored evidence and required tail stages. Saved log:
  build/gate/05-05-publish-gate.log.
- All-target normal and sanitizer builds precede final seven-check owning
  selections, both passing in build/{gate,asan}/05-05-publish-ctest.log.
  Exact inventories are119/121, with only the two planned registrations added.
- Changed C translation units pass strict GCC13 checks in
  build/gate/05-05-publish-gcc.log. Docs, diff, rights and PII checks pass.
- The executor retained genuine fetched ADDI and transfer-opcode RED/GREEN
  evidence. Task2 test/implementation share a commit; the sequencing limit is
  recorded without inventing a separate original RED commit.
- Root review reproduced and fixed J/JAL source-quadrant leakage, unsigned
  packed-load bit placement, CFC2 sign extension, stale transfer/VADD pairing,
  two-VU issue and unsupported-alignment acceptance. Earlier matrix helpers
  repeated some production mistakes; independent constants/sequences now
  expose them. Failed attempts remain saved separately.

[Review evidence](05-05-review-evidence.md) records derivations, controls,
corrections and superseded runs. Full configured phase code/security reviews
remain Plan10 work. Hosted confirmation is pending for this checkpoint.

## Next action and limits

Continue Plan06 portable vector arithmetic, flags, multiply/accumulate and
comparison. No external guest or diagnostic compiler ran during Plan05.
Original private qualification/execution artifacts retain exact identities;
current changed core/adapter bytes require deliberate requalification before
future actual guest use. Native model tests do not establish post-fix diagnostic
success, complete-suite coverage or hardware accuracy. No user action is needed.
