---
phase: "08"
slug: "audio-interface-and-raw-capture"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-18"
---

# Phase08 validation strategy

## Test Infrastructure

Existing C17/CMake/CTest with gate and ASAN presets; no install or dependency.
Quick run: the owning newly registered AI/audio/capture CTest names, nonempty
exact inventory, and actual accepted fetched-CPU/wrong-sample consumer checks.
Full qualification: `sh scripts/gate.sh`, complete owning ASAN inventory and
existing qualified GCC13 source route for affected C units. Planner must replace
provisional test names with exact grounded registrations before plan approval.
No Phase08 test has run yet; new files/targets are pending creation.

## Sampling Rate

After each behavior task run its focused normal and instrumented checks where
applicable. After each wave run the complete affected Phase08 integration group
and relevant old-profile/device regressions. Before final qualification run cheap
actual setup, profile/device wiring, rights/schema/source/inventory and accepted
consumer controls. Review/fix first, then one cohesive full qualification; repeat
only affected checks after relevant changes, not passing suites for prose.
Tests run in background; preserve existing jobs4/twoCIjobs/ten-minute bounds.

## Per-Task Verification Map

| Scope | Requirement | Secure behavior | Test level | Automated command ownership | File/status |
| --- | --- | --- | --- | --- | --- |
| Fetched PCM tracer and wrong-sample comparator | AUD-02 | rights/identity checked before guest use; real independently expected bytes | fixture/runner | exact new owning tracer CTest, to be named in plan | Wave0 creation; pending |
| Registers, DMA, MI and sample phase | AUD-01/03 | complete span/arithmetic admission before mutation | core/scheduler | exact new AI suite plus relevant bus/scheduler/profile tests | Wave0 creation; pending |
| Raw audio queue/info/copy | AUD-01/03 | same pending event on full; short/invalid copy atomic | core/observer | exact audio-output suite, drain/partition/interleave witnesses | Wave0 creation; pending |
| Actual A/V capture bundle | RUN-06 | safe staging/no-replace finalization; existing targets unchanged on every failure | real runner | capture accepted and reached I/O/cap/no-clobber/invalid-target controls | Wave0 creation; pending |
| Cohesive qualification | all four | exact nonempty inventories, rights/privacy and actual producer identities | native/hosted | full gate, ASAN, GCC and exact selected hosted checks | existing infrastructure; pending |

Threat references and exact task/command names will be filled from the approved
plans, not invented here. All high mitigation gaps block their owning scope.

## Wave0 Requirements

Implement the new fixture/AI/audio/capture test registrations and inventory rows
inside their owning vertical slices. Existing harness/build infrastructure is
available. No new test framework or standalone stub-only plan is required.

## Manual-Only Verifications

None intended. Unresolved hardware details are explicit software-profile policy
with authored witnesses, not a fabricated manual/silicon verification. Any actual
rights, physical, publication or release stop remains outside this test strategy.

## Validation Sign-Off

- [ ] Every task has an exact owning automated command or grounded creation dependency.
- [ ] No three consecutive behavior tasks lack automated feedback.
- [ ] New references are created in their declared wave before execution.
- [ ] No watch mode, silently empty discovery, or test over rebuilt owning binaries.
- [ ] Exact original LastTest retained before CTest/discovery and immediately afterward.
- [ ] Manual batches have unique absolute JUnit; return codes and failures retained.
- [ ] Final source/artifact identities and required completed scopes verified.
- [ ] nyquist_compliant becomes true only after reached execution evidence.

Approval: delegated planning strategy; execution evidence pending, no hardware approval.
