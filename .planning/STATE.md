---
gsd_state_version: "1.0"
milestone: v1.1
milestone_name: Observable Machine
current_phase: 08
current_phase_name: Audio Interface and Raw Capture
status: executing
stopped_at: Phase8 Plan01 committed and verified; Plan02 register and cadence matrix next
last_updated: "2026-11-18T17:48:38.770Z"
last_activity: 2026-11-18
last_activity_desc: PCM tracer and composite profile passed normal and instrumented suites; continue register and cadence cases
state_head: 79e8ab5a9e020c5964f046c603501a63768f319b
progress:
  total_phases: 3
  completed_phases: 0
  total_plans: 18
  completed_plans: 13
  percent: 72
current_plan: "02"
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-15)

**Core value:** Every Tendies64 support claim is backed by evidence: a named profile, a pinned build and a reproducible, rights-cleared guest result.
**Current focus:** Phase8 Audio Interface/raw capture executable planning and implementation; oracle stop preserved.

## Current Position

Phase: 08 (Audio Interface and Raw Capture) — EXECUTING
Current Plan: 2
Total Plans in Phase: 5
Status: Plan01 complete; Plan02 ready. Phase7 remains12 of13 with an authority halt
Last activity: 2026-11-18 — Fetched PCM, reached wrong-sample and historical profile controls pass in normal and sanitizer builds

Progress: [███████░░░] 72% of currently planned source plans

## Performance Metrics

- Active milestone:0 completed phases and13 completed plans;18 plans defined across Phases7–8.
- Historical v1.0: 6 phases and 73 source plans complete. This is planning completion, not a software release.
- Prior phase plan counts: 1: 13; 2: 8; 3: 13; 4: 20; 5: 10; 6: 9.
- Execution timing: not established for v1.1. Historical evidence remains in [v1.0 phase records](milestones/v1.0-phases/).

## Accumulated Context

### Decisions

- v1.1 retains Phases 7 → 8 → 9: RDP/VI, AI/raw capture, then controller/save/state/replay. Eight v1.2 requirements and the v2 outline remain deferred.
- Phase 7 starts with a cleared authored DP fill → RDRAM → VI → actual runner raw-frame path, independently calculated pixels and a wrong-pixel control before the full suite. Remaining required RDP modes and VID-04 are still required.
- AI interrupt acceptance follows the cited `ai_full` 1→0 queue transition, not an interrupt per submission/start. Authored PCM proves transport without a new RSP microcode promise.
- Apply the approved [compatibility policy](../docs/spec/cpu-compatibility.md): preserve existing identities/bytes where semantics are unchanged; version concrete changes without a profile per device or version per fix.
- CORE-02 remains partial from the verified scheduler foundation; Phase 9 owns completion across all in-scope devices. API/ABI freeze and installed independent consumer stay in Phase 10.

### Pending Todos

Plans01–12 are complete within their authored scope. The concrete oracle
request/checker is independently reviewed, locally qualified and fail-closed;
production approval pins remain unset. Plan13 and VID-04 remain blocked by
competent authority, per-file/build-input rights and missing exported replay
inputs. Do not infer a grant from this preparation. Both final-batch hosted jobs and exact downloaded raw/boot, privacy and rights checks pass. Owner approved Phase8 from the qualified authored foundation on2026-11-18.
Five plans passed independent checks. Plan08-01 is complete; execute08-02 through08-05 sequentially,
including actual-consumer preflight, tests, review and cohesive qualification. Do not repeat the sequencing
question. No oracle authority is part of this decision.

### Blockers/Concerns

- VID-04: D5 Angrylion-Plus authority is unresolved (owner/legal); parallel-rdp needs a per-file/asset rights audit. This blocks that differential acceptance, not permitted authored/native RDP/VI work. Do not execute/use/copy oracle material until applicable gates resolve; authored tests cannot substitute for it.
- D2: external diagnostic public CI/results remain blocked on documented owner rights. Existing qualified local-only routes retain their exact limits.
- D4: keep the existing copy allowlist until owner authorization changes it.
- D6: releases wait for owner-selected channel/versioning. No software release or version tag is implied by this milestone.
- D7: name remains uncleared; no branding, logos or store listings.
- Protected inputs/publication and physical-console measurement retain the explicit stops in [AUTONOMY.md](AUTONOMY.md). No new hardware qualification or broad compatibility claim.

## Deferred Items

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| Integration | Public API/ABI freeze, installed consumer, minimal host, playable homebrew, LibFuturo evaluation | Eight requirements deferred | v1.1 activation | v1.2 |
| Breadth | Performance, hardware accuracy, expansion/accessories and player features | Provisional evidence-driven outline | v1.1 activation | v2.0 |

## Session Continuity

Last session: 2026-11-18T07:22:46.107364+00:00
Stopped at: Phase8 Plan01 complete; Plan02 ready
Resume file: .planning/phases/08-audio-interface-and-raw-capture/08-02-PLAN.md

Historical closeout: [v1.0 roadmap](milestones/v1.0-ROADMAP.md), [audit](milestones/v1.0-MILESTONE-AUDIT.md), [lessons](RETROSPECTIVE.md).

## Operator Next Steps

- Phase7 researched discussion and adversarial check are complete. Orchestrator executes the independently verified plans under existing autonomy. No acknowledgment is needed to preserve the oracle stop; Phase8 sequencing is approved; continue its authorized work.

## Resolved local privacy stop

The owner approved sanitation of the single unpublished evidence commit.
The document-only amendment preserved public history. Original failed gate and
exact tested artifacts remain retained locally; full privacy and the final gate
pass. This approval applies to that specific repair, not future private values.
