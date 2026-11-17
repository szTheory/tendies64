---
gsd_state_version: "1.0"
milestone: v1.1
milestone_name: Observable Machine
current_phase: 07
current_plan: "10"
current_phase_name: RDP Reference Renderer and VI Scanout
status: verifying
stopped_at: Plan10 local verification complete; hosted qualification pending
last_updated: "2026-11-17T23:18:18.245553+00:00"
last_activity: 2026-11-17
last_activity_desc: Filters/output reviewed and locally qualified; preparing pinned push
state_head: ef6c8ae8a0df270b1d981aef5654f7d2005c9e1b
progress:
  total_phases: 3
  completed_phases: 0
  total_plans: 13
  completed_plans: 10
  percent: 77
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-15)

**Core value:** Every Tendies64 support claim is backed by evidence: a named profile, a pinned build and a reproducible, rights-cleared guest result.
**Current focus:** Execute Plan10 VI filters and lossless raw-output contracts.

## Current Position

Phase: 07 (RDP Reference Renderer and VI Scanout) — EXECUTING
Plan: 10 of13 in current phase
Status: Filters/output reviewed and local gate green; hosted qualification pending
Last activity: 2026-11-17 — Filter bound and test repairs pass full local gate

Progress: [████████░░] 77% of currently planned source plans

## Performance Metrics

- Active milestone:0 completed phases and10 completed plans;13 Phase7 plans defined.
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

Plan10 implementation, independent review repairs, normal/instrumented/compiler
owners and complete local gate pass. Push the exact checkpoint, qualify both
hosted jobs and selected authored startup records, then execute Plan11's
qualification harness/source-review segment before its coherent long matrix.
No user action is needed. Rights/oracle/release and expired-window stops persist.

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

Last session: 2026-11-17T23:18:18.245553+00:00
Stopped at: Plan10 local complete; hosted qualification pending
Resume file: .planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-10-PLAN.md

Historical closeout: [v1.0 roadmap](milestones/v1.0-ROADMAP.md), [audit](milestones/v1.0-MILESTONE-AUDIT.md), [lessons](RETROSPECTIVE.md).

## Operator Next Steps

- Phase7 researched discussion and adversarial check are complete. Orchestrator executes the independently verified plans under existing autonomy. No routine owner acknowledgment is needed; VID-04 and release stops remain.

## Resolved local privacy stop

The owner approved sanitation of the single unpublished evidence commit.
The document-only amendment preserved public history. Original failed gate and
exact tested artifacts remain retained locally; full privacy and the final gate
pass. This approval applies to that specific repair, not future private values.
