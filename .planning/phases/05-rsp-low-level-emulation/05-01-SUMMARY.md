---
phase: 05-rsp-low-level-emulation
plan: "01"
subsystem: rsp-reference
requires:
  - phase: 04
    provides: Qualified scoped CPU primitives and preserved machine profiles.
provides:
  - Native fetched scalar/VADD/DMEM/BREAK tracer and portable interpreter entry.
  - Distinct unqualified model6/state12 profile and canonical future-state domain.
affects: [05-02, 05-04, rsp-native-evidence]
plan_head_before: 16976a93628aae36470ee1ba19fa07197a48c624
plan_head_after: f8bbc96402b73d3c43dbcacbf38829308872106b
actuals: {tasks: 2, commits: 7}
requirements-completed: []
completed: 2026-11-13
status: complete
---

# Plan01 — First fetched RSP slice

An authored native IMEM program executes scalar transfers, VADD, DMEM SW and
BREAK through the production scheduler. The new model6/state12 profile is
explicitly unqualified development coverage; its exact capability predicates
reuse CPU compatibility primitives without changing historical profiles.
Future-affecting RSP fields have their own canonical state domain; diagnostic
observations are excluded. The extracted core/t64_rsp.c is in production lint
and build identity. One dedicated test adds to the preserved native inventory.

## Verification and corrections

Admission-only RED is distinct from operation RED. The first alleged VADD
word lacked the vector selector bit and is preserved as an invalid fixture
attempt. Parent inspection caught the mismatch. Corrected fetched-word RED
then established refusal before implementation; the unchanged expected
saturation, accumulator, flags, scalar transfer and DMEM bytes pass GREEN.

Source review then found global-machine halt on RSP BREAK and a missing CPU
reservation notification on accepted RSP stores. New controls failed against
those exact defects. The fixes leave queued PI work running after RSP BREAK
and invalidate only overlapping seeded native CPU link state. They do not
claim actual CPU instruction or LL/SC guest-path execution. The unpublished
new profile's duration field was aligned to the selected12-model-cycle policy.

- Task1 all-target normal build and five owning checks pass.
- Extracted Task2 all-target normal/instrumented builds and three owning
  checks pass. Exact inventory/selftest preserves111/113 plus one tracer.
- After source fixes, all-target normal and ASan/UBSan builds and six owning
  tracer/SP/profile/link/scheduler/digest regressions pass; saved raw logs
  establish the result. Core lint reports84 production files/zero violations.
- Final normal identity: `t64-build-v1-019d7175db451a975d80844d8c855f047d65b69b891f0e252875a879f34da625`.
  Instrumented: `t64-build-v1-505473c0e24df8c98ee70bca09c628f73b152246ef4e229e0dbdc3df1fb68ef3`.
  They carry prior f0ce649 source annotation with the tested fix delta; content
  hashes bind actual code. No metadata-only source annotation refresh is needed.
- The final instrumented issue function has a3168-byte frame warning under
  the existing2048-byte diagnostic threshold. It remains visible/nonfatal;
  no exemption or general stack-safety claim is made.

[RED and validation evidence](05-01-RED-evidence.md) and
[profile contract](../../../docs/spec/rsp-profile.md) retain commands, identities,
original failed attempts and current supported/refused behavior. Final full
phase native/hosted and configured independent review/security evidence remain
owned by Plan10; this summary does not claim they ran for the new RSP source.

## Scope, threat flags and next action

No external diagnostic, ROM or copied implementation was used. The native
fixture supplies owned synthetic storage and authored IMEM; it does not prove
new-profile ROM boot. No whole-RSP requirement is closed by this narrow slice.
Registered decoder/profile threats retain their planned controls; no new
unregistered threat or risk acceptance is claimed. Independent phase security
verification remains pending.

Plan02 owns SP status/PC/semaphore, issue policy and beat DMA. In that work,
check BREAK's subsequent PC and clear-HALT/resume behavior against primary
expectations instead of treating the current last-fetched-PC assertion as a
complete guest-visible SP_PC contract. No user action is needed.

## Lesson

A logged opcode and a passing RED classifier do not establish that it encodes
the intended instruction. Independently decode the major operation and selector
bits against the primary diagram before accepting its behavioral evidence.
Test cross-device progress and writer notifications at the first real path.
