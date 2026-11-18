---
gsd_state_version: "1.0"
milestone: v1.1
milestone_name: Observable Machine
current_phase: 07
current_plan: "12"
current_phase_name: RDP Reference Renderer and VI Scanout
status: blocked
stopped_at: Plan12 authored preparation complete; oracle authority and Phase8 sequencing unresolved
last_updated: "2026-11-18T07:10:58.115357+00:00"
last_activity: 2026-11-18
last_activity_desc: Plan12 reviewed authority preparation and local gate pass; oracle stop preserved
state_head: d12125e21e69bf614344b105fa4abaa70f9ea9da
progress:
  total_phases: 3
  completed_phases: 0
  total_plans: 13
  completed_plans: 12
  percent: 92
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-15)

**Core value:** Every Tendies64 support claim is backed by evidence: a named profile, a pinned build and a reproducible, rights-cleared guest result.
**Current focus:** Plan12 authored preparation complete; preserve oracle stop and resolve Phase8 sequencing.

## Current Position

Phase: 07 (RDP Reference Renderer and VI Scanout) — BLOCKED for differential acceptance
Plan: 12 of13 in current phase
Status: Plan12 authored preparation complete; actual oracle authority unresolved; Phase8 sequencing decision pending
Last activity: 2026-11-18 — Authority preparation reviewed and locally qualified; final-batch hosted checks pending

Progress: [█████████░] 92% of currently planned source plans

## Performance Metrics

- Active milestone:0 completed phases and12 completed plans;13 Phase7 plans defined.
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
inputs. Do not infer a grant from this preparation. Hosted final-batch checks
are pending. The owner has a pending sequencing choice: start Phase8 Audio
Interface/raw capture from qualified authored output while keeping Phase7
incomplete, or hold here. No oracle permission is part of that choice.

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

Last session: 2026-11-18T03:37:04.614942+00:00
Stopped at: Plan12 authored preparation complete; Phase8 sequencing pending
Resume file: .planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-12-PLAN.md

Historical closeout: [v1.0 roadmap](milestones/v1.0-ROADMAP.md), [audit](milestones/v1.0-MILESTONE-AUDIT.md), [lessons](RETROSPECTIVE.md).

## Operator Next Steps

- Phase7 researched discussion and adversarial check are complete. Orchestrator executes the independently verified plans under existing autonomy. No acknowledgment is needed to preserve the oracle stop; the pending Phase8 dependency change is a real sequencing decision.

## Resolved local privacy stop

The owner approved sanitation of the single unpublished evidence commit.
The document-only amendment preserved public history. Original failed gate and
exact tested artifacts remain retained locally; full privacy and the final gate
pass. This approval applies to that specific repair, not future private values.
