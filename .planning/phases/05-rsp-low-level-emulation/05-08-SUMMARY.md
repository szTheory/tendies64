---
phase: 05-rsp-low-level-emulation
plan: "08"
subsystem: rsp-verification
requires:
  - phase: 05-07
    provides: Fetched integer divider and complete supported opcode ledger.
provides:
  - Deterministic authored reference/invariant fuzz for every supported opcode form.
  - Separate bounded authored weekly/dispatch stress lane.
  - Executable source/object/build-route and fetched-behavior no-HLE controls.
affects: [05-09, 05-10, rsp-verification]
requirements-completed: []
completed: 2026-11-14
status: complete
---

# Plan08 — Bounded RSP coverage and fetched-route controls

Every supported opcode form receives256 deterministic valid fetched tuples,
including scalar/control, CP0/COP2 transfers, vector memory and all44 supported
vector functions. Separate refusal matrices cover reserved functions and
unavailable DP transfers. The corpus compares authored algebra/reference rules
and architectural state invariants, including untouched vector, GPR, ACC, flags,
DMEM and divider fields by owning family. No separately optimized implementation
exists; differential execution was not run and hardware equivalence is not claimed.

The long authored family exercises multiply through reciprocal/rsqrt operations
and actual32-bit high-low sequences. It uses an explicit seed and100000 iterations
per operation, a600s owned-process bound and900s scheduled-job bound. Its workflow
uses the existing checkout pin, no external input or artifact upload, and a separate
reviewed CI-policy contract without changing the required platform pair.

The no-HLE control checks the canonical source inventory, entry/fetch/scheduling
bodies, Ninja object-source/archive edges, actual archive membership and RSP
symbols. Thirteen mutation/pass controls guard known bypass and hidden-source/
object failure classes. Identical fetched LW instruction bytes with distinct
one-byte DMEM inputs produce the expected distinct loaded values; an unsupported
instruction refuses while preserving checked RSP/SP architectural state. These
are scoped architectural regression controls, not universal static proof about C.

## Verification and review

- Final stable-source canonical gate passes126 checks, exact inventory controls,
  lint86/0, workflow policy and authored evidence selftests in56s:
  build/gate/05-08-final-gate-repaired.log.
- Fresh all-target normal and sanitizer builds precede three owning checks;
  both pass in build/{gate,asan}/05-08-review-final-ctest.log. Inventories are
  126 normal and128 sanitizer tests, with only the two planned registrations.
- Fresh optimized GCC13 objects pass all owning warnings-as-errors flags:
  build/gate/05-08-review-final-gcc13.log. actionlint, shell syntax, diff, rights
  and PII checks pass. Owner instruction/config edits retain their supplied hashes.
- Reviewed authored stress passes34.179s with the owned process reaped, after
  the initial36.825s pass. Raw resources/logs remain under build/gate/05-08-*.
- The implementation's fetched VAND coverage RED is committed as a portable
  report. The verification-only Task2 marker correction and parent accepted-bypass
  RED are disclosed in [review evidence](05-08-review-evidence.md), together with
  the VMOV-preservation, GCC-mask and CI-policy corrections and prior attempts.
- Both hosted platforms pass reviewed source eac270e in
  [run43583309676](https://github.com/szTheory/tendies64/actions/runs/43583309676),
  including126 checks, lint86/0 and the evidence/rights/PII/upload tail. Linux
  gate163s (qualification91s, CTest34.43s); macOS gate314s (qualification173s,
  CTest71.25s). The actual first dispatched authored stress
  [run43583352101](https://github.com/szTheory/tendies64/actions/runs/43583352101)
  also passes, with stress25.022s and the owned process reaped inside600s.

Configured full phase code/security review remains Plan10 work. No actual external
diagnostic guest, Cargo build, dependency installation or physical test ran.

## Next action

Continue Plan09's authored CPU→SP DMA→RSP→CPU
ROM integration and meaningful negative variants. Existing private diagnostic
receipts remain bound to their original identities; final-current-core private
requalification belongs to Plan10. No user action is needed.
