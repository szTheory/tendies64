---
phase: 04-complete-vr4300-cpu
plan: "14"
subsystem: runner
tags: [cpu-diagnostic, canonical-json, inventory, sanitizer]
requires: []
provides:
  - One shared closed CPU inventory/batch parser, encoder, validator and aggregator.
  - Authored exact-membership, ordering, completeness and transcript-bound controls.
affects: [phase-04-local-adapter, phase-04-private-qualification]
actuals:
  tasks: 2
  commits: 4
plan_head_before: c3d02e775a552e93776bb28ed3ff1e404b56209c
plan_head_after: 76241f8
tech-stack:
  added: []
  patterns: [Shared production protocol target, identity-bound canonical records, exact-set aggregation]
key-files:
  created: [runner/t64_cpu_diag_protocol.c, runner/t64_cpu_diag_protocol.h, runner/tests/test_cpu_diag_protocol.c, docs/spec/cpu-diagnostic.md]
  modified: [runner/CMakeLists.txt, scripts/check-test-inventory.sh]
key-decisions:
  - Require every sealed required member and all fourteen required groups; completed failing members remain distinct from incomplete execution.
  - Keep source-denominator and rights qualification in Plan16; authored protocol tests cannot establish SC4.
requirements-completed: ["VER-01", "VER-03", "CPU-09"]
coverage:
  - id: D1
    description: Shared closed parser and encoder bind authored inventory/batch records to canonical identities.
    requirement: VER-01
    verification:
      - kind: unit
        ref: runner/tests/test_cpu_diag_protocol.c#runner.cpu-diagnostic-protocol
        status: pass
    human_judgment: false
  - id: D2
    description: Exact full membership, all-group completeness, canonical order and transcript bounds reject authored omissions, duplicates and malformed data.
    requirement: VER-03
    verification:
      - kind: unit
        ref: runner/tests/test_cpu_diag_protocol.c#runner.cpu-diagnostic-inventory
        status: pass
      - kind: integration
        ref: sh scripts/gate.sh; full ASan build and two affected CTests
        status: pass
    human_judgment: false
duration: 30min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 14: Closed CPU Diagnostic Protocol Summary

The shared implementation in `runner/t64_cpu_diag_protocol.c` and authored
controls in `runner/tests/test_cpu_diag_protocol.c` establish a closed local
protocol before any external diagnostic is admitted.

## Performance

Two tasks and seven changed implementation/specification/evidence files.
The observed first-RED-to-final-implementation commit span was 30 minutes
(2026-11-12T01:12:11Z to 2026-11-12T01:41:56Z), excluding pre-commit preparation
and parent closeout. This is elapsed workflow time, not emulator throughput.

## Accomplishments

- One production CMake target exports canonical inventory parsing, identity-bound
  batch encoding/validation and aggregate status. Authored controls exercise
  exact bytes, complete pass/fail versus incomplete execution, terminators,
  empty sets, identity drift and duplicates.
- Aggregation indexes required values in sealed source/value order regardless
  of arrival order, requires all fourteen groups, and remains invalid after
  malformed or duplicate transcripts. Controls cover omitted/seam members,
  reduced/mismatched inventories, wrong selectors/ROMs/indices, aggregate-only
  and truncated records, maximum fields and the 4096-byte capture boundary.

## Task Commits

1. **Task 1: protocol tracer** - `9674468` (test), `6b9b744` (feat): compiled interface-stub RED, then production round-trip GREEN.
2. **Task 2: complete inventory** - `aa53552` (test), `76241f8` (feat): missing-group RED, then exact-set/order/boundary GREEN.

## Automated Checks

- Both RED runs configured, compiled, discovered and executed their registered
  target. Task1 observed the temporary parser interface's not-implemented
  result. Task2 passed its single-group positive assertion, then observed
  complete=1 with thirteen required groups absent. Both actual JUnit reports
  received RED_EVIDENCE_OK. The portable record is 04-14-RED.md; raw reports
  remain ignored and private.
- Task1's focused protocol test and its required post-tracer rerun passed.
  Task2's focused protocol/inventory selection passed 2/2.
- Inventory selftest passed all 188 removal controls. Exact registered sets
  contain 93 gate and 95 ASan names: the previous 91/93 plus these two names.
- Full all-target ASan/UBSan build succeeded. Both new tests passed with
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. No full 95-test sanitizer
  rerun is claimed.
- The final common gate passed all 93 CTests, fixture/evidence controls and
  core lint (19 files, zero violations). Logical-wave hooks report no blocking
  schema drift; codebase drift is skipped because no STRUCTURE map exists.
  The UI detector has no actual UI files or frontend evidence and does not
  block; its generic frontend token is the already documented false positive.

## Deviations and Issues

No implementation-scope deviation. The first RED uses a minimal compilable
new-interface stub, not a pre-existing production parser or a compile failure.
The full ASan build showed existing core frame warnings and duplicate common
library link warnings; the new protocol had no reported sanitizer diagnostic.
Parent recorded the preceding slice's successful two-host CI review separately.
After checks completed, parent identified old reparented idle test probes from
earlier completed runs and performed targeted process cleanup; no current
test/build process or resource policy was changed.

## Evidence Limits and Readiness

All public fixtures and expected results here are authored. The supplied
inventory's canonical bytes and self-consistent counts do not prove the full
pinned upstream denominator. Plan16 must derive and seal actual registration,
value, lane, zero-value, tool and asset facts before SC4 can be assessed.
External diagnostic source, ROMs, receipts, captures and results remain private.
Shared requirement IDs remain gated on unfinished sibling summaries. Ready for
Plan04-02, whose Plan01/14 prerequisites now have verified summaries.

## Self-Check: PASSED

All four commits, the shared interface, specification and sanitized RED record
exist. Verification follows the last code change. Owner configuration and
instruction edits remain unstaged; parent owns state and roadmap closeout.
