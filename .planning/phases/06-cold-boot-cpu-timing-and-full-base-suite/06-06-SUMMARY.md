---
phase: 06-cold-boot-cpu-timing-and-full-base-suite
plan: "06"
subsystem: source-complete-local-diagnostic-evidence
status: complete
completed: 2026-11-15
duration: 243min
requires:
  - phase: 06-02
    provides: Independently anchored source authority and private pre-use qualification
  - phase: 06-05
    provides: Verified authored cold boot and deterministic T1 target
provides:
  - C-owned full-source member/group/lane classification with distinct completeness and passing predicates
  - Exact context/profile/build/image and original-capture binding with terminal observations
  - Serial local required, informational and stress invocation with original-record re-verification
  - Immutable stopped attempts and exact consumer/context retention before first use
  - Measured pre-use reductions without changing execution limits
requirements-completed: [VER-02, BUS-03, CPU-08]
affects: [06-07, 06-08]
actuals:
  tokens: 73619
  tasks: 3
  commits: 12
key-files:
  created: [scripts/apply-base-diagnostic-overlay.py, scripts/base-diagnostic-lanes.py, scripts/tests/test_base_image.py, scripts/tests/test_base_lanes.py, scripts/tests/test_base_classification.py]
coverage:
  - deliverable: Independent source and bound original-record classification
    human_judgment: false
    verification:
      - {kind: ctest, ref: runner.base-diagnostic-protocol, status: pass}
      - {kind: ctest, ref: harness.base-classification, status: pass}
      - {kind: private-original-records, ref: build/private-base-diagnostic/plan06-06-execution-summary.json, status: verified}
  - deliverable: Full-entry qualification, image composition and serial lifecycle
    human_judgment: false
    verification:
      - {kind: ctest, ref: harness.base-qualification, status: pass}
      - {kind: ctest, ref: harness.base-image, status: pass}
      - {kind: ctest, ref: harness.base-run, status: pass}
  - deliverable: Local lane isolation and honest observed-versus-unexecuted receipts
    human_judgment: false
    verification:
      - {kind: ctest, ref: harness.base-lanes, status: pass}
      - {kind: private-original-records, ref: build/private-base-diagnostic/plan06-lanes-invocation-01, status: verified}
---

# Phase 6 Plan 06: Whole-source classification and local lane execution

C is the sole completeness authority. Its bound record validates the independently
qualified source/value selector and ROM identity, exact target context, canonical
guest verdict bytes, capture facts and observed entry/terminal loop. Source-order
rows and group/lane counts retain every source member. A source-complete
classification can contain blocked, unexecuted or unsuccessful members; it does
not establish authenticated complete execution or a passing suite. CPU/RSP
component formats and their completeness owners remain unchanged.

The private required sweep completed without a harness/deadline stop or
interruption, followed by successful full-entry original-record verification.
Separate informational and local stress commands completed and re-verified their
originals. Exact identities, counts, outcomes and observed-versus-blocked details
remain private in the evidence index above. Invocation or a lane definition is
never promoted into guest execution evidence. No hardware accuracy, whole-suite
passing, public external CI, new test rights or release claim follows from these
commands.

## Implementation and scope repairs

The existing image contained CPU selectors only. The approved full-source scope
therefore required the deriver, combined image builder/overlay and authored image
controls, expanding the file union from thirteen to nineteen. Source-selected
CPU/RSP and other supported members compile only under their exact accepted
component module/asset closures. Other source rows retain individual blockers;
RDP levels remain in required membership. Informational/stress rows remain
non-required. One constructor per registration uses the original global ordinal
and four-byte registration/value selector.

Version-2 contexts add independently verified RSP source inputs. Every fresh case
passes pre-use verification before dispatch, uses frozen source inputs and an
exact retained consumer, and keeps its variant, original record/capture and
lifecycle evidence. Invalid/duplicate/foreign inputs poison C completion. Native
ten-second and host twelve-second limits, ten million guest ticks, capture/event
bounds and rights stops are unchanged. One-shot context/lane markers prevent
silent retries; interrupted and partially written attempts remain immutable.

The source/host interface now consumes the verifier's checked snapshot instead
of reopening context.json after verification. Process pipes close on all paths,
interrupt cleanup reaps the bounded worker, and expired deadlines remain
failures even if cleanup exits zero. Test-only private-root/deadline hooks are
excluded from the production executable.

## Evidence-driven first-use corrections

Preserved qualification attempts exposed adjacent-comment constructor anchors,
duplicate module declarations and an incorrect absence-scan argument. The fixes
retain strict constructor matching, preserve existing Rust module visibility,
and decode only the independently audited excluded array for artifact absence
inspection. No excluded font bytes were accepted into the actual ELF/ROM.

The first newly qualified required attempt stopped before guest execution at
ROM admission. The adapter incorrectly passed byte alignment to posix_memalign.
A reached positive control using the cleared boot fixture failed before the fix,
then advanced one canonical tick after normalizing to the host alignment minimum.
Both owning presets passed before the distinct corrected context was qualified.
The earlier stopped records and exact executable/context pairs were preserved;
they were not replayed or relabeled. The original Plan02 missing-executable gap
remains disclosed and irrecoverable.

Original pre-combined history remains immutable. Changed staged/control inputs
are captured separately with a forensics-only purpose before re-derivation, so a
failed intermediate stage does not prevent a corrected qualification or overwrite
its evidence.

## Verification and efficiency

Formal behavioral RED evidence was classified for forged verdict acceptance,
context substitution after verification, and the fixed informational lane entry.
Native and complete-entry authored matrices cover target/capture/terminal changes,
missing/duplicate/foreign/resealed source data, all dispositions, ordering,
positive admission, FIFO replacement, interruptions, deadlines, CI refusal and
lane contamination. Actual retained records also passed the normal C aggregate
against its complete independent source universe.

All targets were built in both presets before owning checks. The cohesive tail
passed the BASE, shared CPU/RSP controls, admission fuzzing and sanitizer negative
control. Later host-only repairs ran their owning normal/instrumented checks,
without repeating unchanged core fuzzing solely for provenance. Final owning
registration includes both new BASE image/classification controls and the lane
control. Exact inventories and their removal controls passed. Logs are retained
under build/gate/06-06-*, with LastTest copies captured before later discovery.
Rights and PII tree scans passed; no private result material was published.

The required sweep took about 63 minutes. Baseline host medians were 0.216 seconds
per member process and 1.023 seconds per full dispatch; an independent pre-use
observation took 0.615 seconds. After the sweep, duplicate CPU validation within a
version-2 pre-use entry and redundant per-case source-document copies were removed.
The fixed CPU anchor and one full independent CPU/RSP closure verification remain.
Read-only optimized pre-use calls all passed, with a 0.497-second median on this
host. These are local observations, not controlled throughput or N64 timing claims.
See [the measured follow-up](06-PREFLIGHT-EFFICIENCY.md).

Source is committed through 453f87d2bc99c79008bb7796b4bde124668a9641. Realized diff characters per four supply
actuals, not model billing. Final original-evidence validation, support-claim
qualification, complete source/security review and the coherent phase gate remain
Plans07–08; plan completion does not replace those gates.
