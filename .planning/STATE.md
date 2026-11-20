---
gsd_state_version: "1.0"
milestone: v1.1
milestone_name: Observable Machine
current_phase: 9
current_phase_name: Controller, Save Media and Replay
current_plan: 13
status: executing
stopped_at: Completed 09-10-PLAN.md
last_updated: "2026-11-20T18:27:27.455Z"
last_activity: 2026-11-19
last_activity_desc: Phase 9 execution started
state_head: b57ca55803c1df342f637535a8dc1734e5c04565
progress:
  total_phases: 3
  completed_phases: 1
  total_plans: 31
  completed_plans: 29
  percent: 33
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-15)

**Core value:** Every Tendies64 support claim is backed by evidence: a named profile, a pinned build and a reproducible, rights-cleared guest result.
**Current focus:** Phase 9 — Controller, Save Media and Replay

## Current Position

Phase: 9 (Controller, Save Media and Replay) — EXECUTING
Current Plan: 13
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
| Phase 09 P03 | 43min | 5 tasks | 31 files |
| Phase 09 P03.1 | 70min | 5 tasks | 15 files |
| Phase 09 P04 | 42min | 6 tasks | 29 files |
| Phase 09 P05 | 45min | 6 tasks | 24 files |
| Phase 09 P06 | 36min | 6 tasks | 7 files |
| Phase 09 P07 | 42min | 4 tasks | 18 files |
| Phase 09 P08 | 40min | 5 tasks | 20 files |
| Phase 09 P09 | 65min | 5 tasks | 15 files |
| Phase 9 P09.1 | 95min | 5 tasks | 13 files |
| Phase 09 P10 | Task 6 1h45m (plan span one day) | 6 tasks | 61 files |

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
- [Phase 09]: 09-03: section 10 of the v25 digest also hashes next_token and token_exhausted so no behavior-deciding SI state is outside the digest; the plan's field list named only active_token
- [Phase 09]: 09-03: a refused CPU step records its attempt (last PCs, opcode, host-error outcome); CPU-path refusal tests pre-apply that record before asserting a bit-identical digest
- [Phase 09]: 09-03: the SI device was implemented in one coherent start function in Task 1 step 4, so Task 3 and Task 4 groups started green; evidence is out-of-tree mutants, and the step 4 RED is a disclosed reconstruction
- [Phase 09]: 09-03.1: row matching keys on the SHA-256 of the fixture file named in the retained command plus the input script SHA-256, so the altered-copy refusal row matches its own run; a record made before pending source files exist carries a rehearsal label that must equal the requested verification
- [Phase 09]: 09-03.1: the combined upload contract admits exactly three layouts per job (boot and raw, plus audio, plus audio and phase09/JOB/manifest.json); phase09 requires audio; the native JUnit private root is a predicate over two named roots
- [Phase 09]: 09-03.1: hosted Linux compiles are the evidence the local gcc:13 closure cannot give; three Linux-only build defects from 09-02 reached hosted CI, so run the ninja compile commands under the act image's Ubuntu cc before pushing C changes (203 units in about 50 s)
- [Phase 09]: 09-03.1: FH tag phase09-fh on 2065efc; frozen logic changes since the pair candidate are the inventory data row only; 09-04 to 09-11 add data rows, suites and test names only
- [Phase 09]: 09-04: classification is static and outcome is dynamic: only the three exact joybus triples are valid on an EEPROM id, everything else there is a timeout and a profile without EEPROM is absent; a read while busy answers the timeout flag at the execute tick (named policy)
- [Phase 09]: 09-04: a profile with no save device answers any requested kind with SAVE_DEVICE_MISMATCH (statuses 20 to 22 provisional; 09-05 uses 23 upward); section 11 exists only for the three save ids so the stem stream keeps markers 1 to 10
- [Phase 09]: 09-04: the busy window is a stored deadline E + 375,000 compared at later execute ticks with no scheduler event (label eeprom-busy-ceiling-6ms-v1, a named ceiling policy, never exact or measured); SRAM DMA reuses the PI completion pair
- [Phase 09]: 09-04: openIPL3 profile ids admit only the exact asset or an authored flat ROM, so the selection control edits header bytes of an authored flat ROM; the reviewed-source guard was refreshed in an isolated commit for t64_save.c and four bus and PI bodies with derived digests
- [Phase 09]: 09-05: import and export check admission before the pointer and span row while the 09-04 info function keeps pointer-first; a zero length constructs no span; one --save-format governs import and export of a run; the artifact publisher opens before the guest so an occupied target ends the run at guest tick 0; byte-order selections are authored policy (A1)
- [Phase 09]: 09-06: SP DMA raises no MI bit (the RSP break raises MI SP only with interrupt-on-break set, the AI only when its descriptor queue falls from two to one), so MI is ordered through its producers' events and mi.sp-dma records an absence
- [Phase 09]: 09-06: the all-devices workload runs the VI with v_sync 16 because the two-packet video queue is never drained and a third field boundary stalls the VI with no progress; tie.pi-sp runs on the skeleton profile where the legacy SP completion event exists
- [Phase 09]: 09-06: budget runs are compared at checkpoints inside the SI read tail, not only at the horizon, so the completion-from-budget mutant (equivalent at 4,096) is killed; the K5 driver reuses the main tree asset and builds only the core library in its worktree
- [Phase 09]: 09-06: F1 freezes state content with 58 section goldens and a 67-member audit of struct t64_machine; lengths of sections 2 to 7, 10 and 11 were derived by hand and equal the walker, hashes are regression pins
- [Phase 09]: 09-07: replay is runner-side; record is the ordinary result plus a private rpl artifact published only at guest-completion or guest-bound, play re-runs the row with an empty schedule and publishes an rpr report in every ending; a pre-guest mismatch is the existing prelaunch refusal and a divergence is the existing harness-error host-error record (no new t64.result key)
- [Phase 09]: 09-07: the checkpoint interval is validated against the runner slice of 16 ticks and the loop target is the minimum of the slice and the next replay op; identity binding compares profile (five fields), build, content (two fields) then save_init with a status per family and the field name before the first advance; goldens hold ops and checkpoint digests only and the header is composed at test time
- [Phase 09]: 09-08: the state container observer body is exactly five u64 values (drain cursor, video head and count, audio head and count); write_generation never travels and 09-09 owns its rule; export is defined for state version 25 only and every refusal leaves bytes, length and machine unchanged
- [Phase 09]: 09-08: the runner opens the .sta publisher before the guest and at authored completion replaces only the outcome, stop kind and detail with the closed state-export-* names (no t64.result key); the result detail field grows to 48 bytes because state-export-incomplete-terminal is 32 characters; the artifact preflight stays first
- [Phase 09]: 09-08: the real exported state is 6,351,511 bytes for the authored admit ROM (6,351,551 idle stem); the plan's 8.4 to 11.5 MiB arithmetic assumed two coverage arrays, so the bounds are 6,303,756 (above) and 11,534,336 (below)
- [Phase 09]: 09-09: validate-then-apply importer with one reader per section in both modes, a 600-byte machine-owned scratch of per-pair queue aggregates, a closed ten-pair handler table and a commit that zeroes then overlays the structures the v25 state describes
- [Phase 09]: 09-09: write_generation adds exactly one per successful state import into a profile with a save device (none on refusal); an imported EEPROM deadline is at most the blob tick plus 375,000 (stale values accepted); the report carries closed (status, reason, section, rule, position)
- [Phase 09]: 09-09: a resumed runner run never reports pass (its verdict needs milestones observed through its own trace); it reaches the same completion tick, final state digest and log bytes as the uninterrupted run, and a state above the bound is the existing prelaunch refusal
- [Phase 9]: 09-09.1: the importer requires the RDP continuation event while an operation is in flight unless a freeze or flush holds the device, and a render in progress must lie inside RDRAM by the acceptance extents; the drop mutants of the (SP, RSP issue) and (INPUT, SUBMITTED) events are accepted by design (a wedged RSP and a host-submitted input are legitimate states)
- [Phase 9]: 09-09.1: the v25 state does not carry the RDP combiner block or the RDP memory-coverage table that the filter-v9 family behaves on; a pending triangle and a VI scan-out over coverage marks cannot continue equal after an import (open finding for 09-10 or Phase 10, CORE-04 left unmarked); the field-use scan sees 72 of 472 reachable members
- [Phase 9]: 09-09.1: the sanitizer preset raises the sweep and campaign counts and the test timeout (1800 s, measured 631.8 s) but not the full boundary matrix; gate cost of unit.core.state.import is 33.2 s native against a 25 s target
- [Phase 09]: Plan 09-10: the hosted macOS cold projection 619 s over the 600 s gate is accepted as DEV-17 (no third candidate cycle, no ci.yml edit); 09-11 checks Binutils cache freshness with gh cache list before its first push
- [Phase 09]: Plan 09-10: FR freeze recorded at 8dd6af1 with the local tag phase09-fr (not pushed); six declared frozen-file deviations FD-01 to FD-06; 09-11 edits data only, no CTest name may be added (45 of 45)

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
- Phase 9 oracle-request rebinds: owner approved in conversation on 2026-11-19 (answer "Yes, rebind per milestone") identity-only rebinds of the Phase 7 oracle request whenever Phase 9 manifest rows move the fixture manifest hash (same scope as 15fd8d1). Each rebind reproduces the old pinned hash first, changes only manifest identities, the derived request hash and PINNED_REQUEST_SHA256, and keeps local_decision blocked. This grants no oracle, rights or publication authority. First Phase 9 rebind: manifest sha256 23b34424...4972, request 4200bb02...5500.

## Deferred Items

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| Integration | Public API/ABI freeze, installed consumer, minimal host, playable homebrew, LibFuturo evaluation | Eight requirements deferred | v1.1 activation | v1.2 |
| Breadth | Performance, hardware accuracy, expansion/accessories and player features | Provisional evidence-driven outline | v1.1 activation | v2.0 |

## Session Continuity

Last session: 2026-11-20T18:27:27.296Z
Stopped at: Completed 09-10-PLAN.md
Resume file: None

Historical closeout: [v1.0 roadmap](milestones/v1.0-ROADMAP.md), [audit](milestones/v1.0-MILESTONE-AUDIT.md), [lessons](RETROSPECTIVE.md).

## Operator Next Steps

- Phase7 researched discussion and adversarial check are complete. Orchestrator executes the independently verified plans under existing autonomy. No acknowledgment is needed to preserve the oracle stop; Phase8 sequencing is approved; continue its authorized work.

## Resolved local privacy stop

The owner approved sanitation of the single unpublished evidence commit.
The document-only amendment preserved public history. Original failed gate and
exact tested artifacts remain retained locally; full privacy and the final gate
pass. This approval applies to that specific repair, not future private values.
