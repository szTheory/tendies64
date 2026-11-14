---
phase: 05-rsp-low-level-emulation
plan: "06"
subsystem: rsp-portable-vector-reference
requires:
  - phase: 05-05
    provides: Fetched scalar/COP2/vector transfer reference and encoding ledger.
provides:
  - Portable vector arithmetic/logical and16 multiply/round forms with VSAR.
  - Comparison/clip/merge/flag chains and closed64-function dispositions.
  - Literal complete-state matrices and explicit guide-conflict policies.
affects: [05-07, 05-08, rsp-local-evidence]
requirements-completed: []
completed: 2026-11-14
status: complete
---

# Plan06 — Portable integer vector reference

The functional profile fetches arithmetic/logical, multiply/accumulate/round,
comparison/clip/merge, VSAR and VNOP operations. Tests cover destination/source
aliases, element selectors, signed extrema, flag input extremes, all accumulator
slices, carry/wrap and instruction sequences. The complete64-slot ledger keeps
reserved encodings and Plan07's pending reciprocal functions explicitly refused.
No optimized path, HLE dispatch, dependency or historical-profile change was added.

Some pinned-guide equations conflict or leave widths unspecified. The instruction
ledger records exact integer software policies for those cases, including
partial-product slices, VMULQ/VMACQ, VSAR and flag choices. Native agreement with
those equations establishes this deterministic model; it does not establish
hardware arithmetic or independent diagnostic agreement. These choices must be
revisited when qualified independent evidence contradicts them.

## Verification and review

- Final stable-source canonical gate passes all122 native tests, lint84/0,
  exact inventory controls and its authored evidence tail. Saved log:
  build/gate/05-06-final-gate.log.
- All-target normal and sanitizer builds precede the final ten owning checks;
  both selections pass in build/{gate,asan}/05-06-review-ctest.log. Inventories
  are122/124 with exactly the three planned real registrations.
- Changed C source and tests pass strict GCC13 checks with their owning target
  flags in build/gate/05-06-review-gcc-{core,tests}.log. Docs, diff, rights and
  PII checks pass.
- Each task has genuine committed fetched-opcode RED/GREEN. Expanded per-row
  matrices followed the initial opcode GREEN; no separate original RED is
  invented for each later row. Corrected arithmetic expectations and the
  rejected GCC command-scope attempt remain recorded.
- Root review reproduces and fixes VNOP acceptance of malformed scalar COP2
  fields, VNOP/VADD sharing one VU slot, and VABS losing raw accumulator bits
  through destination saturation. Fixed raw-word and literal-state controls
  now distinguish those effects from standalone opcode retirement.

[Review evidence](05-06-review-evidence.md) records exact controls, source limits
and software-policy boundaries. Configured full phase code/security review
remains Plan10 work. Hosted confirmation is pending for this source checkpoint.

## Next action

Continue Plan07's exact selected-source/license/history audit and integer-only
reciprocal/rsqrt helper. The existing copy policy permits that bounded audited
route; it does not permit copying broad emulator VU code or assuming a software
algorithm reproduces physical ROM contents. No external guest or Cargo build
ran during Plan06. Original private receipts retain their tested identities;
changed native bytes require deliberate pre-use requalification before future
actual diagnostic execution. No user action is needed.
