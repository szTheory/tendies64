# Plan08 review evidence

The executor commits add authored reference/invariant fuzz and a separate bounded
stress lane, followed by a source/object/build-route no-HLE control. No optimized
backend exists; no differential or hardware-accuracy result is claimed.

## Saved implementation checks

- The compiled fetched VAND RED retired one tuple and failed its required256
  coverage assertion. The committed portable report contains no machine paths.
- The complete default corpus retires256 tuples for each of130 supported forms;
  reserved vector functions and unavailable DP transfers have separate refusal
  counters. All expected vector/GPR/ACC/flag/DMEM/divider fields are compared by
  owning families; this is architectural reference/invariant evidence, not an
  arbitrary whole-machine equivalence proof.
- Fresh all-target normal and sanitizer builds precede the owning fuzz/no-HLE/
  tracer selections. Saved raw captures are build/{gate,asan}/05-08-*.
- Authored stress uses seed0x05f8c0de and100000 iterations per operation,
  including actual32-bit reciprocal/rsqrt high-low sequences. The first run
  passed36.825s; the reviewed harness run passed34.179s, approximately5.9MB RSS,
  with the600s timeout unexpired and the owned process reaped. The scheduled job
  is900s. The native binary has fixed bounded allocations and creates no child
  processes; existing host policy is inherited. No external input is enabled.
- The time-wrapper probe failed in macOS resource reporting; it is not counted
  as a guest failure. The replacement uses standard-library monotonic timing,
  child resource accounting, explicit timeout and checked exit status.
- Initial BEQ expectation and divider high-low readiness mistakes were corrected
  before the saved passing corpus. A low opcode must actually retire after
  advancing to current_tick+delta; readiness stalls do not count as coverage.

## Parent review and corrections

1. VMOV preservation checked divider_input against a sentinel and did not check
   its other two divider fields. It now snapshots and compares input, result and
   pending-high validity. The production implementation did not change.
2. Token/route-presence scanning accepted an unnamed DMEM-signature early-return
   mutation. build/gate/05-08-review-no-hle-red.log captures that actual accepted
   bypass. Reviewed normalized digests now guard the seven canonical entry,
   scheduling, fetch and SP-dispatch bodies in addition to inventories, route
   checks, archive edges/members and RSP symbols. The same seed is rejected and
   thirteen controls pass, including a prose/literal-only pass. This is a bounded
   architectural regression control, not a universal proof about arbitrary C.
3. The original differing-content control changed instruction immediates. The
   replacement executes identical fetched LW bytes and identical initial RSP
   registers against two authored DMEM inputs differing by one byte. Only its
   loaded GPR and that input byte differ; unsupported fetch preserves the checked
   architectural RSP/SP fields before any effect.
4. GCC13 rejected two implicit signed masks that Clang accepted. Explicit32-bit
   unsigned masks preserve the intended arithmetic. Fresh optimized GCC objects
   pass with the owning warnings-as-errors flags; no compiler was installed.
5. The first combined gate passed125 tests and failed the repository workflow
   contract because it assumes only the two required jobs. Its log is retained
   as build/gate/05-08-final-gate.log. The approved separate authored stress YAML
   now has an exact reviewed-byte contract, without relaxing any required-job
   rule. Six mutations change time, trigger, command, permission, action or upload;
   all are rejected. Policy selftests pass67/0. actionlint and shell syntax pass.
   The initial gate build reported no work, so the concurrent authored stress
   binary was unchanged; the final combined attempt is serialized.

Task2 originally carried tdd=true although it adds verification to an existing
fetched path. No original formal RED was saved for that task. Its inapplicable
marker is corrected with this disclosed rationale. The parent accepted-bypass
RED is current review evidence, not reconstructed original implementation history.

Fresh parent normal and ASan all-target builds and three owning checks pass in
build/{gate,asan}/05-08-review-final-{configure,build,ctest}.log. GCC evidence is
build/gate/05-08-review-final-gcc13.log. The final combined gate and hosted pair
are recorded in the plan summary after they finish. Full configured phase code
and security review remain Plan10 work. No actual external diagnostic, Cargo
build, tool installation, physical test or private-result publication occurred.

The weekly/dispatch workflow syntax was checked against GitHub's live
[workflow reference](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax)
and [event reference](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#schedule).
The advertised ci_monitor helper is absent from the installed skill and project;
existing connector job/log tools and the CLI run lookup are used instead.
