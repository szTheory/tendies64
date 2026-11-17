---
status: resolved
trigger: "macOS hosted owned-process timeout cleanup masks its refusal"
created: "2026-11-17T04:19:55.609066+00:00"
updated: "2026-11-17T04:26:03+00:00"
---

## Current Focus

bug_class: bohrbug (deterministic duplicate cleanup; hosted permission outcome is platform-dependent)
hypothesis: timeout kills/reaps the owned process group, raises Refusal, then the generic BaseException cleanup signals the group again and receives EPERM.
test: real owned descendant/FIFO closure with an injected PermissionError only on a forbidden second group signal; mocked boundary paths preserve first-signal failures.
expecting: unfixed timeout attempts a second group signal, masking the intended Refusal; fixed code signals once and closes the descendant FIFO.
next_action: root runs its combined final gate and handles hosted CI. No user acknowledgment is needed under explicit autonomous find-and-fix authority.
candidate_causes:
  - code: branch cleanup plus generic exception cleanup repeats the same group signal.
  - environment: macOS may refuse the redundant signal after group leader reap.
  - config/data: no flags or guest contents are needed to enter this timeout path.
and_gate: yes for the observed PermissionError mask (duplicate cleanup AND a refused second signal); duplicate cleanup itself is deterministic.
reasoning_checkpoint:
  hypothesis: explicit timeout/capture cleanup is repeated because its subsequent Refusal enters the outer BaseException cleanup.
  confirming_evidence:
    - hosted trace shows timeout Refusal followed by PermissionError at outer killpg.
    - unchanged production source deterministically fails the new capture, closed-pipe timeout and leader-exited descendant timeout controls on their injected forbidden second signal; normal success, generic cleanup and first-signal permission preservation controls pass.
  falsification_test: if production source with only branch cleanup removed still signals twice or fails to close the real descendant FIFO, this ownership hypothesis is wrong.
  fix_rationale: all exceptional exits already reach the outer handler; remove duplicate branch signals/waits while retaining that handler's unconditional whole-group SIGKILL and wait. Do not catch PermissionError or skip based on leader state.
  blind_spots: hosted macOS CI is not rerun here; local macOS real descendants and deterministic injected second-signal refusal model that outcome. First-signal errors continue to propagate.
  candidate_causes:
    - code: redundant cleanup before raising the refusal.
    - environment: macOS permission behavior after cleanup exposes redundant signal.
  and_gate: yes for the observed mask; code cleanup ownership fixes the controllable contributor and requires no permission weakening.

## Symptoms

Expected: the authored bounded-router timeout raises its intended Refusal and closes the descendant FIFO within existing bounds.
Actual: runner.cpu-diagnostic fails on hosted macOS; PermissionError from os.killpg masks the already-raised timeout Refusal.
Error source: scripts/run-cpu-diagnostic.py:323 first raises Refusal after timeout cleanup; line356 in BaseException handling calls killpg again and raises EPERM.
Reproduction evidence: completed hosted run44013595697 at source2fd44ca; job133806102266.
Logs: build/gate/07-03-hosted-macos-failed.log (trace near lines785–809).
Test: scripts/tests/test_cpu_diagnostic_run_controls.py::test_bounded_guest_router_timeout_closes_descendant, called by runner.cpu-diagnostic.
History: full local preset passed; this hosted macOS run exposed the second-cleanup path. No regression cause is assumed beyond the recorded trace.

## Scope / authority

Own only scripts/run-cpu-diagnostic.py, scripts/tests/test_cpu_diagnostic_run_controls.py, and this session's substantive debug evidence. Parent owns summary/state/roadmap/CI logging/publication.
All tests use authored controls. No private ROMs, qualification refresh, budget renewal, external oracles, physical/paid actions, tags or releases. Preserve exact timeouts/caps.
Do not ignore first-signal permission errors, skip group cleanup based solely on leader exit, or weaken descendant EOF/process checks. Do not broadly signal unrelated processes.
Keep original failure and prove the fix with a targeted deterministic regression, then the affected public CTest owner.

## Eliminated

- hypothesis: a private diagnostic input or compatibility profile is required.
  evidence: the failing hosted test uses authored Python fork/FIFO children and calls run_bounded directly.

## Evidence

- checked: required instructions, current runner and complete authored controls file.
  found: three explicit cleanup branches signal and wait before fail(); the outer BaseException handler signals and waits again. The existing real-descendant control asserts FIFO EOF and a bounded return for closed pipes.
  implication: isolate cleanup ownership without weakening process-group cleanup or resource caps.
- checked: semantic recall and project skills discovery.
  found: MemPalace executable, local knowledge-base file and project skills directories are absent; configured gsd-debugger skills query returns no skills. No per-test coverage or Stryker configuration is present.
  implication: use deterministic regression and manual fix-site mutant/revert checks; SBFL and property shrinking are inapplicable to this minimal control-flow reproduction.
- checked: `PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest discover -s scripts/tests -p test_cpu_diagnostic_run_controls.py -v` against unchanged production source.
  found: RED, six tests in 2.083s, three behavioral errors. Capture-limit Refusal, closed-pipe timeout Refusal and real leader-exited descendant timeout Refusal each become the injected PermissionError on the second killpg. Three neighboring controls pass. Raw output: `build/gate/mac-owned-cleanup-red.log`.
  implication: root cause is directly reproduced without private inputs. The new direct controls run separately before the original fifteen batch controls, preserving the owning C harness's required count/message contract.
- checked: same direct regression command after removing only the three redundant cleanup blocks.
  found: GREEN, six tests in 2.080s. Real descendant FIFO EOF, one group signal, closed-pipe bound, capture refusal, generic exception cleanup, first-signal PermissionError identity and successful no-signal path pass. Raw output: `build/gate/mac-owned-cleanup-green.log`.
  implication: single exception-handler ownership fixes the reproduced mechanism; no limits or permission exceptions changed.
- checked: exact source baseline loaded in an isolated module using the ignored `build/gate/mac-owned-cleanup-isolated-check.py baseline` driver; working-tree source is unchanged.
  found: six controls rerun, same three errors return (2.108s). Raw output: `build/gate/mac-owned-cleanup-revert-red.log`. A fix-site mutant restoring only the first timeout cleanup produces exactly one error (2.107s), caught by the real leader-exited descendant case; raw output: `build/gate/mac-owned-cleanup-mutant.log`.
  implication: both exact baseline reconfirm and a scoped mutation prove the regression observes cleanup ownership. Stryker is absent; equivalent authored in-memory mutation is recorded explicitly rather than claiming Stryker ran.
- checked: direct controls on current fixed working tree after isolated baseline/mutant checks.
  found: six controls GREEN (2.081s); `build/gate/mac-owned-cleanup-reapply-green.log`.
  implication: baseline and mutation experiments did not leave production source reverted.
- checked: `PYTHONDONTWRITEBYTECODE=1 ctest --preset gate -R '^runner.cpu-diagnostic$'`.
  found: owning CTest passes in 15.01s; its original fifteen batch controls and added six bounded-process controls execute through the owning C runner. Existing LastTest preserved before execution as `build/gate/mac-owned-cleanup-before-owner-LastTest.log`; final LastTest retained as `build/gate/mac-owned-cleanup-owner-LastTest.log`; command output: `build/gate/mac-owned-cleanup-owner.log`.
  implication: affected authored owner is green with no private diagnostic execution. Full combined gate remains parent-owned and has not been run here.
- checked: `ast.parse` for both owned Python files, exact-file `git diff --check`, source diff review.
  found: syntax accepted, whitespace clean; production diff removes only the three duplicate cleanup copies. No timeout, cap, PermissionError handling or outer process-group cleanup is weakened. Static output: `build/gate/mac-owned-cleanup-static.log`.
  implication: deletion is explicitly justified by the confirmed exception-handler ownership defect; retained unconditional group cleanup covers descendants even after leader exit.
- checked: exact staged names and normal `git commit` with configured hooks enabled, then committed file inventory and index state.
  found: source commit `55d298e` contains only `scripts/run-cpu-diagnostic.py` and `scripts/tests/test_cpu_diagnostic_run_controls.py`; no hooks were bypassed. Index window released to root immediately after commit. Protected owner changes and concurrent research notes remain uncommitted.
  implication: owned source fix is complete. Manager owns terminal debug-document archive/commit; root owns the combined final gate and hosted CI. Explicit parent authorization and project autonomous policy supersede a routine human-acknowledgment checkpoint.

## Resolution

cycles: 1 investigation + 1 fix
tdd: strict RED/GREEN; routine progression authorized by project autonomous policy and parent scope
root_cause: timeout and capture-limit paths signal/reap then raise Refusal inside an outer BaseException handler that signals/reaps again. A denied second signal masks the intended refusal.
oracle_type: specified (timeout/capture refusal, exactly one owned-group cleanup, real descendant FIFO EOF and bounded return; first-signal failures remain visible)
fix: remove three branch-local signal/reap copies; retain the single unconditional outer BaseException group termination/reap and its original error propagation.
verification:
  target_test: { result: pass, evidence: build/gate/mac-owned-cleanup-reapply-green.log }
  mutation_check: { result: pass, method: authored isolated fix-site mutation (Stryker unavailable), mutant_killed: true, evidence: build/gate/mac-owned-cleanup-mutant.log }
  no_op_deletion: { result: pass, deletion_justified_by_rca: true }
  adjacent_tests: { result: pass, suites_run: [runner.cpu-diagnostic], evidence: build/gate/mac-owned-cleanup-owner.log }
  revert_and_reconfirm: { result: pass, method: exact baseline module loaded in isolation without changing concurrent working tree, bug_returned_on_revert: true, fixed_on_reapply: true, evidence: build/gate/mac-owned-cleanup-revert-red.log }
  guardrail_verdict: accepted
  environment_limit: hosted macOS CI and combined full gate remain parent-owned; local macOS real descendants and deterministic permission injection pass.
files_changed: [scripts/run-cpu-diagnostic.py, scripts/tests/test_cpu_diagnostic_run_controls.py]
source_commit: 55d298e
prevention:
  why_not_caught: previous authored control asserted refusal and descendant EOF but did not forbid a second group signal; local group lookup could tolerate repeated cleanup while hosted macOS returned EPERM.
  recurrence_guard: scripts/tests/test_cpu_diagnostic_run_controls.py:BoundedProcessControls injects forbidden-second-signal PermissionError and checks real FIFO EOF, closed-pipe bounded return, capture overflow, generic exception cleanup, first-signal permission identity and zero-signal success.
