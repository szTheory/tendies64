---
phase: "05"
slug: rsp-low-level-emulation
status: planned
nyquist_compliant: false
wave_0_complete: true
created: "2026-11-13"
---

# Phase5 validation contract

Planning contract only; no RSP implementation or test has passed. Existing
C17/CMake/CTest infrastructure covers the phase, so no setup-only wave is
needed. New tests are registered with their first production behavioral slice.
The task/test/threat ownership and real additions below are the planned
execution contract. No check or implementation has passed from this document.

## Infrastructure and inherited limits

- Preserve all111 normal and113 instrumented CTest names and their exact-set,
  removal, disabled, malformed and empty-selection controls. Add no padding.
- Build all normal targets before owning tests; configure/build ALL sanitizer
  targets before instrumented owning tests. Keep ASAN_OPTIONS=detect_leaks=0:
  halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1.
- Owning commands use --no-tests=error and explicit fails_when. Retain admission
  fuzz120-second bound, its resource limits and sanitizer negative control.
- Use `sh scripts/gate.sh` at meaningful combined source boundaries, final
  source and hosted closeout; immediate owning tests after changes. Inspect
  verification-scope before broad reruns and reuse unchanged evidence.
- Actual RSP component runs are local-only, headless/background, fresh-state
  and bound to audited source/tool/assets/profile/core/selector/image. Preserve
  existing per-case guest/step/wall/capture bounds and immutable adapter checks.
- No external inputs/results enter required hosted jobs or sanitized upload.
  Long stress belongs in its separate scheduled lane; rights must permit its
  inputs before activation. No new service, speculative cache or concurrency.

## Required behavioral owners

| Requirement | Immediate assertions to plan | Current state |
|---|---|---|
| RSP-01 | Real fetched IMEM scalar/VU/DMEM/BREAK tracer, full scalar/control/vector transforms, flags/accumulators/latches, status/semaphore/PC and transactional refusal | pending |
| RSP-02 | Independent expected vectors and bounded per-opcode edge/invariant fuzz; complete-state differential only for an actually separate optimized path | pending |
| RSP-03 | Alignment, length/count/stride, bank wrap, active/pending queue, HALT-independent DMA, notifications/order and unsupported conflict atomicity | pending |
| RSP-04 | Early qualified actual build/composition/startup/refusal and final exact component membership with individually reasoned classifications | pending |
| RSP-05 | No HLE dispatch in canonical source/object inventory, executable negative control for hidden/task-specific shortcuts | pending |

A guide-backed result is distinct from selected undefined/timing policy.
Legacy profile/hash/digest/refusals, advance partitioning/interleaving,
observer exclusion, source/object lint and CPU protocol controls are regression
owners. A copied divider helper must pass its actual pre-copy scope/license
and provenance controls plus independent integer/rational expectations;
reference-generated goldens alone cannot verify correctness.

## Manual-only and evidence flags

No manual action is invented for executable native/software contracts.
Undocumented exact silicon arbitration/corruption/divider edge results stay
explicit evidence limits. Physical testing and external publication remain
owner stops. Plan prohibitions and unclassified probes stay visible under the
AUTONOMY soft-gate rule; no human validation or risk acceptance is recorded.

## Planning sign-off

- [x] Every task has automated verification, fails_when and measurable acceptance.
- [x] Exact test additions and complete task/requirement/threat map are filled.
- [x] All named entry-gate areas have primary/vector evidence or explicit
  supported/refused/profile-policy contracts without a hardware claim.
- [x] First real component attempt precedes long native expansion in plan order.
- [x] Final native, sanitizer, review, exact hosted and private evidence owners
  are explicit; no source/result artifact is relabeled after changes.
- [ ] Actual implementation and checks establish nyquist_compliant.

## Exact new registrations

Planning observation: `ctest --test-dir build/gate --show-only=json-v1` lists111
tests and the same command for build/asan lists113. No test execution was run.
Preserve every existing name, label behavior and malformed/disabled/empty
inventory control. Add exactly these16 registrations to both inventories for
planned totals127 normal/129 ASan; no padding or rename of inherited tests.

| New registration | First behavior/registration owner | Requirement |
|---|---|---|
|unit.core.rsp-tracer|01.1 behavior,01.2 registration|RSP-01/02/05|
|unit.core.rsp-control|02.2|RSP-01/03|
|harness.rsp-qualification|03.1 behavior,03.2 registration|RSP-04|
|harness.rsp-build|03.2|RSP-04|
|unit.rsp-diag-protocol|04.2 behavior,04.3 inventory|RSP-04|
|harness.rsp-run|04.3|RSP-04|
|unit.core.rsp-scalar|05.1|RSP-01|
|unit.core.rsp-transfers|05.2|RSP-01/02|
|unit.core.rsp-vector|06.1|RSP-01/02|
|unit.core.rsp-multiply|06.2|RSP-01/02|
|unit.core.rsp-compare|06.3|RSP-01/02|
|unit.core.rsp-divider|07.2 behavior,07.3 inventory|RSP-01/02|
|harness.rsp-divider-audit|07.1 authored controls,07.3 registration|RSP-01/02|
|unit.core.rsp-fuzz|08.1|RSP-02|
|repo.rsp-no-hle|08.2|RSP-05|
|integration.rsp-rom-smoke|09.2|RSP-01/03/05|

Tests introduced before inventory registration have explicit direct executable
or script verification, so no task depends on a not-yet-registered CTest name.
The leading tracer extends existing unit.core.sp-dma before extraction; no
setup-only wave. Newly registered tests exercise authored behavior/controls;
external qualification/run modes are separate local-only invocations.

## Task/verification/threat ownership

Every row's command is literal in its PLAN `<automated>`; each has adjacent
`fails_when`. Normal configure/build ALL precedes focused CTest or script checks.
Instrumented owning checks require configure/build ALL sanitizer targets and
the inherited ASAN/UBSAN environment before execution, not a build of one test.

| Task | Immediate automated evidence | Threat refs | Status |
|---|---|---|---|
|01.1|Existing sp-dma/profile/digest/determinism/scheduler selection plus actual fetched behavioral RED→GREEN|T-05-01/02|Planned|
|01.2|New tracer+old SP/profile normal and instrumented selection; exact inventory|T-05-01/02|Planned|
|02.1|SP/status/DMA/issue route plus old profile/event/digest selection|T-05-03/04|Planned|
|02.2|Control edge/refusal matrix, link notifications and partition/interleave selection|T-05-03/04|Planned|
|03.1|Authored qualification/closed lock+asset/source tests; no private roots required|T-05-05/06|Planned|
|03.2|Authored composition controls then actual qualified local build/composition attempt|T-05-05/06|Planned|
|04.1|Existing runner.cpu-diagnostic-protocol/inventory/adapter canonical byte regression|T-05-09|Planned|
|04.2|Direct RSP protocol executable: component/seal/completion/malformed exact partition controls|T-05-07|Planned|
|04.3|Authored runner limits/negative controls then first actual qualified local startup/refusal|T-05-07/08|Planned|
|05.1|Fetched full scalar/control/CP0 and legacy profile selection|T-05-10|Planned|
|05.2|All transfer element/alignment/wrap/alias and VMOV matrix|T-05-11|Planned|
|06.1|Add/sub/logical flags and selectors complete-state assertions|T-05-12|Planned|
|06.2|Multiply/round/clamp/masked48 slices independent integer assertions|T-05-12|Planned|
|06.3|Compare/clip/merge flags/chains/full64-function-slot ledger|T-05-13|Planned|
|07.1|Exact pre-copy source/history/license/digest audit and authored contradiction controls|T-05-14|Planned|
|07.2|Direct fetched divider integer/latch executable with independent math|T-05-15|Planned|
|07.3|Registered divider/audit+provenance controls normal/ASan|T-05-14/15|Planned|
|08.1|Nonempty per-opcode deterministic fuzz independent vectors/invariants; stress syntax/config controls|T-05-16/18|Planned|
|08.2|Source/object route mutations and unsupported fetched-opcode refusal|T-05-17|Planned|
|09.1|Authored build/rights/boot/source/composition selftest and actual fixture build|T-05-20|Planned|
|09.2|Actual CPU→SP→RSP ROM and meaningful variants normal/ASan|T-05-19/21|Planned|
|10.1|Exact private --run-all/--verify-complete partition; every outcome/reason retained|T-05-22/24|Planned|
|10.2|Configured code/security review reports plus evidence mutation selftests and focused fix retests|T-05-23/24/25|Planned|
|10.3|Final canonical native gate; ALL ASan build and owning selection including admission fuzz/negative control; final evidence/artifact checker; exact required hosted pair|T-05-23/24|Planned|

Reserved T-05-SC appears in every plan: no package installation is planned;
any proposed package install first needs its legitimacy audit. ASVS1 high and
critical issues block. Unresolved lower judgment flags remain visible; no
fabricated human acceptance or automatic clearance of explicit stops.

## Execution cadence and non-vacuity

1. Meaningful RED for every tdd=true task captures intended fetched opcode,
   PC and failed expected observation. A setup/addressing/registration failure
   is not RED. Extraction/qualification/review tasks explicitly marked without
   TDD start GREEN honestly; they must not manufacture a failure.
2. After each changed production slice run immediate owning normal checks.
   At coherent combined boundaries after02,04,07 and final10 run the canonical
   native gate with verification-scope first. No broad gate repeat solely for
   report/summary/annotation updates. The first required gate tail that was
   aborted by an earlier failure still must run after the fix.
3. Before instrumented owning execution, build ALL targets with asan preset.
   Environment: ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and
   UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. Preserve admission fuzz120,
   its address-space/CPU/process limits and sanitizer negative control. New
   short per-opcode fuzz is bounded60s with nonzero per-opcode cases; it does
   not replace admission fuzz or the separate authored scheduled stress.
4. Refusal matrices first prove an accepted authored baseline; every setup/write
   return is checked and every intended refusal reason reached. Compare memory,
   registers, flags/latches, queue/events/interrupt and future-state identity.
5. Local RSP external use retains guest10M, step16, child10s, outer12s,
   capture4096 and exact original deadline/no-retry rules. Private qualification
   precedes build; actual output composition precedes execution. Observe one
   actual early attempt in03/04 before05–07 expansion. A classified refusal is
   evidence of refusal, never a passing compatibility member. External runs
   are serialized after sanitizer/fuzz to avoid unmeasured host contention.
6. Required hosted gate-linux/gate-macos use authored clean inputs only. Separate
   long stress uses authored loops, preserves independent caps and remains
   outside required jobs. External hosted stress stays disabled until D2.
7. Final review includes verification-plan threats and actual artifact/privacy
   checks, exact CPU membership preservation, copy provenance, unsupported
   boundaries and no false differential/hardware labels. Save actual review
   status, including incomplete/interrupted outcomes; never infer a pass.
8. Final artifact check follows review fixes and tested source. Commit-safe
   evidence contains portable authored assertions/identities, not private
   external names/counts/hashes/paths/results. Parent performs routine summary,
   state, roadmap and authorized hosted tracking from the saved evidence.

The five generic edge probes remain unresolved in raw saved JSON; explicit
predicates/owners are in05-PROBE-LEDGER.md. Planned coverage is complete across
the four source types in05-SOURCE-AUDIT.md. Execution must establish results
before nyquist_compliant or hardware/software qualification claims change.
