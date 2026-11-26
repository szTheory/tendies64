# Phase 11 deferred items

Out-of-scope discoveries logged by executors (GSD scope boundary): not fixed by the plan that found them.

## F-20-A (found by plan 11-20): `t64_machine_advance` has no span check on its request and result

**RESOLVED by plan 11-27** (fix commit d3d3570; consumer rows, the bus decision and the header text 6e6bf36; the two
hand-built white-box tests 18c5219; records in PIN-IMPACT "Install steps", 11-27). `t64_machine_advance` judges the spans of
its request and its result first (before either `struct_size`, before the admission answer) and answers
`INVALID_ARGUMENT` with nothing read or written; review candidate (g) of 11-13 is closed by the same change. The
bus-read `value_out` case is decided: it is not span-checked and the header says so ("The value_out span of
t64_machine_bus_read is the caller's responsibility"), with the reason and the way to reverse it recorded in PIN-IMPACT.
The finding text below is kept as found.

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

## Preflight `head-ci` cannot pass while the plan under way is the one that repairs the red head (found by plan 11-28)

- **Observed.** `scripts/preflight-phase11.sh` ends its `head-ci` step in `FAILED` when the newest hosted `ci.yml` run of
  `origin/main` completed with a failure, and the script exits there, so `timeout-audit`, `pii`, `linux-compile` and the
  post-gate replay never run. That is the designed guard against long native work on a red head (F-107 lesson). A
  numbered plan whose purpose is to push the repair of a red head (11-28) cannot reach a green preflight by the script as
  written.
- **What plan 11-28 did.** The script was not edited (it is not in the plan's file list). The real script was run once to
  show the failure, the remaining steps ran from a scratch copy without the single `step head-ci` line (ignored build
  tree, `ROOT` fixed), and the real script passed `head-ci` again on the green head after the push. Recorded in
  PIN-IMPACT "11-28".
- **Why deferred.** An escape (for example a `--allow-red-head` flag that prints the red head and continues, restricted to
  the case where `HEAD` is a descendant of the red head) changes a Phase 11 guard script and needs a plan that names it.
- **Suggested plan.** Low priority: only if another red-head repair plan is expected. Otherwise a future plan of this kind
  states "preflight green except `head-ci`, run from a copy" in its Task 3 wording.
