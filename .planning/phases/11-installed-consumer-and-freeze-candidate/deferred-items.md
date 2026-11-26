# Phase 11 deferred items

Out-of-scope discoveries logged by executors (GSD scope boundary): not fixed by the plan that found them.

## F-20-A (found by plan 11-20): `t64_machine_advance` has no span check on its request and result

- **Header promise.** `core/include/tendies64/t64_machine.h` (the check order of every entry that takes a struct, before
  `t64_host_storage`) says a struct span that wraps or overlaps the machine is `T64_STATUS_INVALID_ARGUMENT` before the
  struct is read, and names `t64_machine_advance` among the entries that check their request and result this way.
- **Behavior.** With the result (or the request) placed inside the instance's own allocation (which a host owns through
  its allocator), `t64_machine_advance` answers `T64_STATUS_OK` and writes the result into the instance; every other
  entry that takes an out pointer answers `T64_STATUS_INVALID_ARGUMENT` for the same placement (output status, video and
  audio info, save info, CPU stop detail, machine and content digest, ROM size and header, video copy, save export, the
  state import report, the drain result and the poison query). `t64_machine_bus_read` also accepts a `value_out` inside
  the instance; the header makes no span promise for the bus calls.
- **Why deferred.** The files of plan 11-20 do not include `core/`, and a failing aliasing row cannot enter the gate
  generator. Review item S-06 had accepted the read of `struct_size` before the span checks as a C pointer-contract
  limit; this finding is the different, documented overlap rule.
- **Suggested plan.** A numbered defect-fix plan (as 11-26 was): add the request and result span checks to
  `t64_machine_advance` (the helper the other entries use), decide the bus-read `value_out` case in the header text,
  and add an aliasing row to `ops-advance` and to the generator (`op_advance`, a variant that points the result into
  `g->fx->ledger.slots[0]`) with the expected `INVALID_ARGUMENT` and an unchanged machine digest. The probe that found
  it is recorded in PIN-IMPACT "Install steps", plan 11-20. Class table row 1 (`INVALID_ARGUMENT`) needs no change.
