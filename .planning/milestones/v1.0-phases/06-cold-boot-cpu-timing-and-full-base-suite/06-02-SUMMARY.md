---
phase: 06-cold-boot-cpu-timing-and-full-base-suite
plan: "02"
subsystem: base-diagnostic-qualification
status: complete
completed: 2026-11-15
requires:
  - phase: 06-09
    provides: Cold/T1 profile and authored startup seam
provides:
  - Phase-owned C inventory, record encoder and aggregate
  - Independent pre-use, complete CLI controls and qualification-owned artifact retention
  - One retained failed bounded external invocation with explicit evidence limits
affects: [06-03, 06-06, 06-07, 06-08]
requirements-completed: [BUS-03, CPU-08, VER-02]
actuals:
  tokens: 51023
  tasks: 3
  commits: 11
key-files:
  created: [runner/t64_base_diag_protocol.c, runner/t64_base_diag_protocol.h, runner/t64_base_diagnostic.c, runner/tests/test_base_diag_protocol.c, scripts/derive-base-diagnostic-source.py, scripts/verify-base-diagnostic-qualification.py, scripts/build-qualified-base-diagnostic.py, scripts/run-base-diagnostic.py, scripts/tests/test_base_qualification.py, scripts/tests/test_base_run.py, docs/spec/base-diagnostic.md]
  modified: [runner/CMakeLists.txt, scripts/check-test-inventory.sh]
---

# Plan 02 — Whole-BASE trust path and first failed invocation

The phase-owned C protocol authenticates canonical membership and original
member bytes, poisons duplicates and owns aggregate completeness. Python
derives membership from the independently pinned source projection and prepares
qualified commands. Production pre-use calls the existing reviewed source
qualifier rather than trusting two resealed snapshots. Existing component
contracts remain unchanged. The new adapter directly advances the canonical
Tier B/T1 core; the legacy CPU guest adapter cannot select that target.

The global executor's BASE selection includes both RDP levels as well as the
ordinary CPU levels. Parent spot-check found that the initial derivation kept
all non-CPU skipped rows, including out-of-BASE levels. The final deriver applies
the verified global selection to every lane, retaining source ordinals, blocked
BASE rows and empty-once semantics. The earlier overinclusive denominator claim
is withdrawn. Private outputs and historical contexts were deliberately not
regenerated after this fix; they are historical/stale, not current qualified
BASE evidence. Future qualification must derive the corrected inventory after
preserving its prior input records. Non-CPU initial placeholder blockers are
not final rights/device dispositions; Plan06 must reconcile individual members.

## Actual attempt and limits

Two preliminary invocations refused before creating a run directory or
launching a guest because of host role/entry-order defects. Complete wrapper
CLI controls now cover those paths. One subsequently qualified bounded external
invocation failed to produce an authenticated member record: a manually counted
JSON key length truncated `inventory_sha256`, so the C aggregate rejected it.
There is **no valid member result or guest-progress claim** from
that invocation. It was not retried, and no later qualification or guest run
was performed for the local corrections.

The attempted executable bytes were not archived before its repair rebuild.
Its context, binding and failed-run logs remain, but that is not retained
executable evidence. This historical gap cannot be repaired or used to justify
a replay. A later unused qualified executable/context pair was retained before
the shared-encoder rebuild and is explicitly stale; it is a different artifact.
All external identities, counts, raw results and artifact records stay private.

Forward protection now captures exact executable, context and binding bytes in
an immutable context-digest-keyed snapshot before activation. Pre-use verifies
that pair before dispatch or directory creation. Controls prove overwrite
survival, stale-live refusal, immutability, and missing/tampered pair rejection.
Status/history metadata is separate from immutable context bytes.

## Verification and corrections

- Meaningful REDs preceded the new C authority, verifier and bounded-run behavior.
  The shared-encoder RED concerns its new API, not a retroactive reproduction of
  the original serializer failure.
- Full CLI authored acceptance reaches pre-use, role/runtime binding, normal
  dispatch and the real C aggregate. Synthetic records are labeled and provide
  no external guest evidence. The production adapter and regression test now use
  the same C encoder, with independent literal-key and original-byte assertions.
- All-target normal and ASan/UBSan builds and final protocol/run controls pass
  after final C changes. Later retention and membership corrections changed
  Python/specification files only; their direct and normal/instrumented harness
  controls pass. Scope analysis calls for those owning host checks, which ran.
  Inventories pass at132/134, inherited129/131 plus three registrations.
- Source/tool/image closure reuse is explicitly separate from old consumer
  identity. Rebuilt and archived legacy CPU consumer seal mismatches were
  preserved; neither was relabeled as the new target or used for its guest run.

Exact commands and original log paths are in ignored
`build/gate/06-02-execution-checkpoint.md`. Final native logs are the private
`task3-final-*` records; retention controls are `retention-*-focused.log`, and
final level-selection controls are `build/gate/06-02-d08-*`. The eleven commits
run from e2aece4 through086f3ee. Actuals use realized diff characters divided by
four, not model billing. Protected owner instruction/config edits stayed intact.

## Next action

Proceed with Plan03's independent native RDRAM protocol sequences. Preserve the
Plan02 guest stop and all failed/stale evidence; no acknowledgment or retry is
needed. Whole-BASE classifications, valid member results, final artifact review
and phase requirements remain pending. No user action is needed.
