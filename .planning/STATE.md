---
gsd_state_version: "1.0"
milestone: v1.1
milestone_name: Observable Machine
current_phase: 9
current_phase_name: Controller, Save Media and Replay
current_plan: 3
status: executing
stopped_at: Completed 09-02-PLAN.md
last_updated: "2026-11-19T20:59:57.855Z"
last_activity: 2026-11-19
last_activity_desc: Phase 9 execution started
state_head: bf75429300bfba00fb2713114504a3404f0f00ff
progress:
  total_phases: 3
  completed_phases: 1
  total_plans: 31
  completed_plans: 19
  percent: 33
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-15)

**Core value:** Every Tendies64 support claim is backed by evidence: a named profile, a pinned build and a reproducible, rights-cleared guest result.
**Current focus:** Phase 9 — Controller, Save Media and Replay

## Current Position

Phase: 9 (Controller, Save Media and Replay) — EXECUTING
Current Plan: 3
Total Plans in Phase: 13
Status: Ready to execute
Last activity: 2026-11-19 — Phase 9 execution started

Progress: [███░░░░░░░] 33% of currently planned source plans

## Performance Metrics

- Active milestone:0 completed phases and16 completed plans;18 plans defined across Phases7–8.
- Historical v1.0: 6 phases and 73 source plans complete. This is planning completion, not a software release.
- Prior phase plan counts: 1: 13; 2: 8; 3: 13; 4: 20; 5: 10; 6: 9.
- Execution timing: not established for v1.1. Historical evidence remains in [v1.0 phase records](milestones/v1.0-phases/).

**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 09 P01 | 36 min | 6 tasks | 18 files |
| Phase 09 P02 | 32min | 7 tasks | 25 files |

## Accumulated Context

### Decisions

- v1.1 retains Phases 7 → 8 → 9: RDP/VI, AI/raw capture, then controller/save/state/replay. Eight v1.2 requirements and the v2 outline remain deferred.
- Phase 7 starts with a cleared authored DP fill → RDRAM → VI → actual runner raw-frame path, independently calculated pixels and a wrong-pixel control before the full suite. Remaining required RDP modes and VID-04 are still required.
- AI interrupt acceptance follows the cited `ai_full` 1→0 queue transition, not an interrupt per submission/start. Authored PCM proves transport without a new RSP microcode promise.
- Apply the approved [compatibility policy](../docs/spec/cpu-compatibility.md): preserve existing identities/bytes where semantics are unchanged; version concrete changes without a profile per device or version per fix.
- CORE-02 remains partial from the verified scheduler foundation; Phase 9 owns completion across all in-scope devices. API/ABI freeze and installed independent consumer stay in Phase 10.

- Phase 8 qualified 2026-11-19 on commit 3a41734 (run 44549490908): tier unit-tested, authored scope only; see docs/evidence/phase08-audio.md and docs/claims/phase08.md. The Phase 7 oracle-request re-pin (commit 15fd8d1) changed only identity hashes of authored inputs; after the automated oversight check denied it the work stopped, and the owner approved it in conversation on 2026-11-19 ("yes follow ur recs"). `local_decision` stays blocked and publication not-authorized; this note grants no oracle, rights or publication authority.
- Post-Phase-8 follow-ups: cache the pinned Binutils build in CI (docs/ci-reviews/LOG.md F-096), the duplicate rsp-no-hle run (F-097), and the supervisor's ~68 KiB per-file capture limit before larger frames can be captured (docs/spec/raw-capture.md).
- [Phase 09]: 09-01: descriptor 12 is created only under its four registered ids; the family tuple selects ring storage by registered id — an unregistered descriptor 12 would enable the AI gates with no ring storage
- [Phase 09]: 09-01: AI-device gates share t64_has_ai_device; the digest keeps exact tuple predicates; eight sealed bodies re-pinned in an isolated derived refresh — one token per site; derived digests equal working-tree digests; four seeds still reject an unauthorized change
- [Phase 09]: 09-01: v25 digest walker emits ascending section ids 1-9 over the v24 block boundaries; packet profile_id and build_id are not hashed under v25 — stripped v25 stream equals the stripped v24 stream across five states
- Phase 9 follow-up (K1): The shipped Phase 8 descriptor 11 profile has RDP depth inactive because the depth predicate is guarded by descriptor version 10; Phase 9 preserves this on descriptor 12; no Phase 9 requirement needs depth and no Phase 9 claim mentions it.
- [Phase 09]: 09-02: a runner prelaunch refusal of the artifact directory reaches the supervised result as report-error/capture-invalid with guest tick 0; the runner binary itself reports detail refusal
- [Phase 09]: 09-02: replay artifacts have no fixed binary magic; the rpl extension row is their rights-scanner control and no replay magic row is added

### Pending Todos

Plans01–12 are complete within their authored scope. The concrete oracle
request/checker is independently reviewed, locally qualified and fail-closed;
production approval pins remain unset. Plan13 and VID-04 remain blocked by
competent authority, per-file/build-input rights and missing exported replay
inputs. Do not infer a grant from this preparation. Both final-batch hosted jobs and exact downloaded raw/boot, privacy and rights checks pass. Owner approved Phase8 from the qualified authored foundation on2026-11-18.
Five plans passed independent checks. Plans08-01–04 are complete; execute08-05 sequentially,
including actual-consumer preflight, tests, review and cohesive qualification. Do not repeat the sequencing
question. No oracle authority is part of this decision.

### Blockers/Concerns

- VID-04: D5 Angrylion-Plus authority is unresolved (owner/legal); parallel-rdp needs a per-file/asset rights audit. This blocks that differential acceptance, not permitted authored/native RDP/VI work. Do not execute/use/copy oracle material until applicable gates resolve; authored tests cannot substitute for it.
- D2: external diagnostic public CI/results remain blocked on documented owner rights. Existing qualified local-only routes retain their exact limits.
- D4: keep the existing copy allowlist until owner authorization changes it.
- D6: releases wait for owner-selected channel/versioning. No software release or version tag is implied by this milestone.
- D7: name remains uncleared; no branding, logos or store listings.
- Protected inputs/publication and physical-console measurement retain the explicit stops in [AUTONOMY.md](AUTONOMY.md). No new hardware qualification or broad compatibility claim.
- 09-02: harness.rdp-oracle-authority fails (fixture manifest SHA-256 changed) because Phase 9 manifest rows move the hash pinned by the Phase 7 oracle request; rebind values are in 09-02-SUMMARY.md and need an orchestrator or owner decision (precedent commit 15fd8d1)

## Deferred Items

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| Integration | Public API/ABI freeze, installed consumer, minimal host, playable homebrew, LibFuturo evaluation | Eight requirements deferred | v1.1 activation | v1.2 |
| Breadth | Performance, hardware accuracy, expansion/accessories and player features | Provisional evidence-driven outline | v1.1 activation | v2.0 |

## Session Continuity

Last session: 2026-11-19T20:59:57.726Z
Stopped at: Completed 09-02-PLAN.md
Resume file: None

Historical closeout: [v1.0 roadmap](milestones/v1.0-ROADMAP.md), [audit](milestones/v1.0-MILESTONE-AUDIT.md), [lessons](RETROSPECTIVE.md).

## Operator Next Steps

- Phase7 researched discussion and adversarial check are complete. Orchestrator executes the independently verified plans under existing autonomy. No acknowledgment is needed to preserve the oracle stop; Phase8 sequencing is approved; continue its authorized work.

## Resolved local privacy stop

The owner approved sanitation of the single unpublished evidence commit.
The document-only amendment preserved public history. Original failed gate and
exact tested artifacts remain retained locally; full privacy and the final gate
pass. This approval applies to that specific repair, not future private values.
