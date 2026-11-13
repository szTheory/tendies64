# Plan02 source-review corrections

The restart preserved the original semantic RED reports and partial source.
The replacement executor resumed that source, fixed its compile/link seams,
and committed the SP control, beat DMA, timing model and authored control
matrix. No earlier phase research or hosted verification was repeated.

## Parent review and regression controls

Primary review against the SGI/Nintendo RSP Programmer's Guide, printed p43,
found that MTC2's VU destination was marked ready at issue+3 instead of
issue+4. MTC2 and MFC2 also omitted their store-side role in the documented
load/store hazard. These are source observations and guide-backed corrections,
not measured hardware results.

Before production correction, an added native regression observed:

- MTC2 at tick1 had ready=4, with the dependent pair retired at tick4;
  expected ready=5 and no pair retirement before tick5.
- Each independent MTC2/MFC2 two opportunities after LW retired at tick3
  with zero stalls; expected one load/store bubble and retirement at tick4.

`test_rsp_control --review-cop-hazards` exited1 against that source, then
exited0 after correction. Raw before/after output is retained under the
ignored build tree. This is a later source-review regression control, not a
replacement for either original task RED. The tracer's expected ticks were
updated to include the corrected destination delay; output bytes and exact
fetched instruction assertions remain unchanged. The permanent control suite
includes these regressions.

Review also corrected a bank-conflict diagnostic that incorrectly reported
vector readiness. Refusal observations remain outside canonical guest state.
The spec now explicitly labels the current BREAK PC behavior as software
policy and distinguishes MTC2's VU timing from MFC2's SU timing.

## Retained limits

Native authored fixtures establish selected functional behavior. No external
ROM, diagnostic run, hardware test, general RSP compatibility or full ISA
qualification is claimed. The profile remains development coverage.
Before the parent correction, the executor measured the issue callback's normal
stack frame at592 bytes and its instrumented frame at8064 bytes, on a bounded
nonrecursive callback path. The
warning is retained without suppression or a general stack-safety claim.

Repeated baseline digest optimization was deferred; no cache, dependency,
new concurrency policy or weakened assertion was introduced. Final owning
verification is recorded in the accompanying SUMMARY from saved logs.
