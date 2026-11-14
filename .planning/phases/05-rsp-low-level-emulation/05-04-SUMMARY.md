---
phase: 05-rsp-low-level-emulation
plan: "04"
subsystem: rsp-local-runner
requires:
  - phase: 05-03
    provides: Qualified private source closure and inspected image.
provides:
  - Shared canonical primitives with unchanged CPU reporting.
  - Separate authenticated RSP protocol and bounded local supervisor.
  - Preserved first actual refusal and tested current host binding.
affects: [05-05, rsp-local-evidence]
requirements-completed: []
completed: 2026-11-14
status: complete
---

# Plan04 — Authenticated RSP adapter and bounded startup

The CPU protocol now shares generic canonical primitives while retaining its
closed schema, membership and canonical bytes. The independent RSP adapter
binds exact component, inputs, selectors, capture and terminal retirement. Six
outcomes remain distinct; refusals, incomplete execution and harness failures
never count as passing coverage. Python delegates completeness to C.

One actual private bounded startup preceded full ISA expansion. It reached a
CPU refusal before RSP retirement. The initial launcher failure, authenticated
refusal and later validation addendum remain distinct immutable evidence. Native
reproduction found the new model/state pair missing from an existing CTC1
predicate. The narrow fix and regression pass; this establishes no post-fix
guest success. No external identity, denominator or output is published.

Root review corrected deadline handling after pipe closure, overflow/cleanup
precedence, process-group ownership, pre-invocation identity checks and repeated
read-only completeness verification. Changed source and executables are now
explicitly rebound to reviewed hashes; original qualification and image were
preserved without another Cargo build or guest replay.

## Verification

- Final canonical local gate passes all117 tests, lint84/0 and its required
  evidence/inventory tail. Log: build/gate/05-04-final-gate.log.
- Normal and sanitizer all-target builds and five owning runner checks pass
  after supervision fixes. Both exact inventories retain117/119 entries.
- Eight authored wrapper and twelve qualification controls pass. Current
  private qualification and fresh wrapper pre-use verification pass without
  executing a guest. Earlier core/profile/COP1/RSP regressions pass in both
  configurations; their evidence remains attached to the tested predicate fix.
- Docs, diff, rights and PII checks pass. Public CI uses authored fixtures only;
  hosted confirmation is pending for this source checkpoint.

[Review evidence](05-04-review-evidence.md) retains failing controls, corrections,
process limitations and private evidence locations. Full configured phase
code/security reviews remain Plan10 work. The public ancestry is preserved;
only unpublished selector-bearing commits were repaired before publication.

## Next action

Continue Plan05's full scalar/control/CP0 and vector transfer implementation
under the existing profile and limits. Actual RSP execution beyond the preserved
first refusal, complete-suite coverage and hardware accuracy remain unproven.
No user action is needed.
