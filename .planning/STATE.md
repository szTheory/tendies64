---
gsd_state_version: "1.0"
milestone: v1.1
milestone_name: Observable Machine
current_phase: 07
current_plan: "03"
current_phase_name: RDP Reference Renderer and VI Scanout
status: executing
stopped_at: Phase07 Plan02 qualified; execute plan03
last_updated: "2026-11-16T15:40:43.539006+00:00"
last_activity: 2026-11-16
last_activity_desc: DPC transport and fetched RSP qualified on both hosts; TMEM/copy next
state_head: 31a0b6e5cfe83d268ae25ed5d88117df9b1712b8
progress:
  total_phases: 3
  completed_phases: 0
  total_plans: 13
  completed_plans: 2
  percent: 15
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-15)

**Core value:** Every Tendies64 support claim is backed by evidence: a named profile, a pinned build and a reproducible, rights-cleared guest result.
**Current focus:** Phase7 reviewed DPC transport and fetched RSP access; TMEM/copy next.

## Current Position

Phase: 07 (RDP Reference Renderer and VI Scanout) — READY TO EXECUTE
Plan: 03 of13 in current phase
Status: DPC transport and fetched RSP integration verified/reviewed; TMEM/copy next
Last activity: 2026-11-16 — transport and fetched RSP verified; append coverage warning fixed

Progress: [██░░░░░░░░] 15% of currently planned source plans

## Performance Metrics

- Active milestone:0 completed phases and2 completed plans;13 Phase7 plans defined.
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

The reviewed VI queue repair passed both hosted jobs, and exact authored artifact
comparison passed. Reviewed Plan07-02 passed both hosted jobs and exact artifact comparison.
Execute Plan07-03 next.
Carry its informational redundant DPC flag cleanup into the owning state/digest slice.
Local normal checks pass; the instrumented suite retains one BASE timeout with
an unchanged-limit isolated pass. See the current [review](phases/07-rdp-reference-renderer-and-vi-scanout/07-REVIEW.md).

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

Last session: 2026-11-16T05:13:26.082Z
Stopped at: Phase07 Plan02 qualified; execute plan03
Resume file: .planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-03-PLAN.md

Historical closeout: [v1.0 roadmap](milestones/v1.0-ROADMAP.md), [audit](milestones/v1.0-MILESTONE-AUDIT.md), [lessons](RETROSPECTIVE.md).

## Operator Next Steps

- Phase7 researched discussion and adversarial check are complete. Orchestrator executes the independently verified plans under existing autonomy. No routine owner acknowledgment is needed; VID-04 and release stops remain.
