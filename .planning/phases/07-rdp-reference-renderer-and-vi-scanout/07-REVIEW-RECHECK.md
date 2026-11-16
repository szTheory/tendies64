---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-16T12:40:16Z
source_commit: 6752bba
scope_files: 8
new_findings: 0
status: recheck_passed_with_validation_caveat
---

# Phase 07: VI host-drain review recheck

Rechecked the fix commit `6752bba` across the eight files listed below, focusing
on the original CR-01/CR-02 claims, VI queue backpressure, scheduler ordering,
observer-copy effects, digest coverage and the saved validation evidence. No
new code finding remains in this fix cohort.

## Original finding dispositions

### CR-01 (original BLOCKER): withdrawn

The v14 profile satisfies `t64_has_t1_policy` (`core/t64_internal.h:748-755`),
so `has_v13_state` is true and enables `has_warm_state`
(`core/t64_digest.c:206-211`). The enclosing digest block therefore reaches
the v14 RDP/VI hashing branch (`core/t64_digest.c:314,452`). The authored
regression confirms that a valid VI origin write changes the digest and that
inherited warm state is covered (`core/tests/test_vi.c:399-419`). Adding v14 to
`has_rsp_state` as CR-01 suggested is unnecessary and would conflate separate
profile predicates.

### CR-02 (original BLOCKER): root cause confirmed; proposed digest change rejected

Hashing `packet_head` and `packet_count` would include host observer state in
the canonical digest, contrary to D-06/D-07 (`07-CONTEXT.md:61-75`) and the VI
contract (`docs/spec/vi.md:56-69`). The actual defect was that a full-queue VI
event was popped before its handler reported `QUEUE_FULL`; copying then
scheduled a replacement event, changing scheduler history and equal-tick
ordering. The pre-fix RED log reproduces those failures at
`build/gate/07-vi-repair-red-LastTest.log:10-17`.

The replacement is consistent with the contract:

- `vi_event_waits_for_output_capacity` recognizes only a current, enabled VI
  field event when the observer queue is full. Both time-bound and
  counter-bound advances check this shared predicate before dispatch, CP0 time
  synchronization, pop, trace or tick mutation
  (`core/t64_scheduler.c:276-307,335-449`). The event therefore retains its
  original tick and sequence until a host copy frees capacity.
- Disabled or stale-generation VI events do not match the capacity predicate;
  the existing handler returns without producing a packet when disabled or
  when its generation is stale (`core/t64_scheduler.c:276-282`,
  `core/t64_vi.c:356-364`). Equal-tick ordering remains `(tick, sequence)` in
  the scheduler (`core/t64_scheduler.c:10-13`); the regression checks the VI
  event still precedes the later synthetic event after drain
  (`core/tests/test_vi.c:488-571`).
- A successful `t64_video_copy` copies the oldest packet and advances the
  observer head/count only (`core/t64_vi.c:463-500`). It does not schedule,
  trace or advance guest time. If called directly with a full queue, the
  handler's early capacity return remains a no-mutation safety guard
  (`core/t64_vi.c:360-368`).
- The producer chooses `(packet_head + packet_count) % capacity`
  (`core/t64_vi.c:426-431`). Each enqueue increments count; each dequeue
  advances head and decrements count. Thus the next physical slot follows the
  number of packets produced, independent of when the host drains. The empty
  queue no longer resets head, and the digest hashes packet bytes/metadata in
  stable physical slots while excluding observer cursor/count
  (`core/t64_digest.c:506-558`).

The regressions exercise the authored RDP fill fixture: the cadence test
compares raw packets and equal-boundary digests across early and delayed drains
(`core/tests/test_vi.c:422-486`); the full-queue test checks the retained event,
unchanged digest/sequence/tick across copy, resumption, and equal-tick trace
order (`core/tests/test_vi.c:488-571`). Copy refusal atomicity remains covered
at `core/tests/test_vi.c:183-225`.

## Saved evidence

- **RED:** `build/gate/07-vi-repair-red-LastTest.log` records the seven
  expected pre-fix assertion failures. Its SHA-256 matches the value in
  `07-REVIEW-FIX.md`; this RED output has no separate retained consumer
  binding, so it is corroborating reproduction evidence only.
- **Focused GREEN:** `unit.core.vi.fill` passed with 1,140 checks and no
  failures (`build/gate/07-vi-repair-green-focused-LastTest.log`). Its log hash
  matches the retained focused log. The focused `test_vi` and core archive
  hashes also match the byte-identical consumers recorded in the full normal
  binding.
- **Full normal:** `sh scripts/gate.sh` passed all 145 tests. The normal
  binding's source-delta hash matches the exact `311b9e7..6752bba` patch, and
  its recorded log and consumer file hashes/sizes verify against the archive.
- **Full ASAN:** 146/147 passed; `harness.base-run` hit its configured timeout
  at 922.45 seconds while long tests were also running. Preserve this as an
  incomplete full-suite result. The isolated BASE run at the same configured
  limit passed all 13 Python controls in 2.44 seconds. The ASAN binding's
  source-delta hash matches the exact fix patch, and its recorded artifact
  hashes/sizes verify. This evidence supports, but does not prove, concurrent
  host contention as the timeout cause.

The checked full-normal and ASAN source-delta hash is
`7ec0bd9876167ef275c6f4bd12b2db9ccfb532da6f7ec48c617d1601f2212fdf`.
All listed archive entries matched their recorded sizes and SHA-256 values.
No additional test run was needed for this bounded recheck.

## Rechecked file scope

- `core/t64_digest.c`
- `core/t64_internal.h`
- `core/t64_scheduler.c`
- `core/t64_vi.c`
- `core/tests/test_vi.c`
- `docs/spec/rdp-vi-cases.json`
- `docs/spec/vi.md`
- `scripts/check-rsp-no-hle.py`

_Rechecked: 2026-11-16T12:40:16Z_
