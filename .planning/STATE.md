---
gsd_state_version: "1.0"
milestone: v1.2
milestone_name: Embeddable and Playable
current_phase: 10
current_phase_name: Carry-in Closure Before the Freeze
status: executing
stopped_at: Completed 10-03-PLAN.md
last_updated: "2026-11-24T01:46:45.193Z"
last_activity: 2026-11-23
last_activity_desc: Phase 10 execution started
state_head: 5f4887897fdba3737ed35eeeff5157f3b1c64d61
progress:
  total_phases: 3
  completed_phases: 0
  total_plans: 19
  completed_plans: 3
  percent: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-23)

**Core value:** Every Tendies64 support claim is backed by evidence: a named profile, a pinned build and a reproducible, rights-cleared guest result.
**Current focus:** Phase 10 — Carry-in Closure Before the Freeze

## Current Position

Phase: 10 (Carry-in Closure Before the Freeze) — EXECUTING
Plan: 4 of 19
Status: Ready to execute
Last activity: 2026-11-23 — Phase 10 execution started

Progress: [░░░░░░░░░░] 0% (0 of 3 v1.2 phases complete)


## Performance Metrics

- v1.1 (closed 2026-11-20): 3 phases (7–9) and 30 plans complete; Phase 9 had 13 plans. Plan 07-13 and VID-04 descoped to v1.2. Planning completion, not a software release.
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
| Phase 09 P11 | 85min | 6 tasks | 5 files |
| Phase 10 P01 | 23 min | 4 tasks | 6 files |
| Phase 10 P02 | 32 min | 4 tasks | 8 files |
| Phase 10 P03 | 18 min | 4 tasks | 6 files |

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
- [Phase 09]: Plan 09-11: suite-level claim blocks bind the stem id only; the other family ids the state, scheduler and save suites exercise are listed in docs/evidence/phase09-qualification.md and are not claimed
- [Phase 09]: Plan 09-11: the macOS cold projection of 673 s (warm 487 s) against the 600 s job limit is a recorded deviation extending DEV-17, not repaired (no logic edit after the freeze); the decision to act is the orchestrator's
- [Phase 09]: Plan 09-11: repo.sched-k5-anchors is a supporting name of claim block 7 and not counted because no suite of the case inventory lists it; block 3 states that no wrong-value row exists for the 16 Kbit ROM

- v1.2 roadmap (2026-11-23): three phases. Phase 10 closes the carry-ins that change state, descriptors or digests (VID-07, CORE-06, VER-06, RUN-07, GATE-01) before the freeze; Phase 11 proves the C-ABI through a clean installed package and only then freezes it (VER-05, API-01..04); Phase 12 builds the optional SDL3 host, the rights-cleared playable title and the LibFuturo evaluation on the frozen API (API-05..07). This supersedes the earlier note that placed the API freeze and installed consumer in Phase 10; they are now Phase 11. VID-04 stays unmapped until the owner decides D5.

- Phase 9 closed 2026-11-20 on source d3211e9 (tag phase09-f2; hosted run 44795894664; tiers unit-tested and regression-pinned, authored scope only). Accepted deviation DEV-17: hosted gate-macos cold projection 673 s exceeds the 600 s job limit (warm 487 s inside 540 s; cold is a projection that matters only on a Binutils cache miss). Follow-ups for the next qualification cycle (docs/ci-reviews/LOG.md F-111, F-114): start unit.core.state.import first by CTest cost (~40 s macOS), drop the duplicated audio-records step (~42 s macOS), raise gate-macos timeout-minutes to 15; ci.yml is bound source so these ride the next requalification.

- Post-close note (2026-11-20): controls.real.raw-capture-target-errors (Phase 8 control) failed once in a local full gate at host load ~15 and passed alone three times and in a full re-run (224/224); treat as a load-dependent flake to watch (build/gate/08-04-evidence accumulates about 2,400 retained run directories). Phase directories moved to .planning/milestones/v1.1-phases/; docs/evidence text paths still name the old location.

- v1.2 fan-out (2026-11-23, five read-only lenses; reports in research/v1.2-fanout/): Phase 10 carries GATE-02 and GATE-01 first, then VER-06, then VID-07 and CORE-06 as one identity/state bump under new profile ids (shipped ids stay byte-identical), then RUN-07; Phase 11 ends with a bound freeze candidate and the final ABI bind moves to the end of Phase 12; API-06 is the authored route A (libdragon research only); SDL3 is link-only, off by default, non-gating; CORE-06's old premise was stale because plan 09-10 already carries the combiner and coverage members.
- Owner approval 2026-11-23 (answer "Per-row binding + standing approval"): Phase 10 changes the oracle check to per-row pins (GATE-02); until it lands, identity-only rebinds of the Phase 7 oracle request are approved for all v1.2 manifest rows (reproduce the old pinned hash first, change only manifest identities and the derived request hash and PINNED_REQUEST_SHA256, keep local_decision blocked, orchestrator task only). No oracle, rights or publication authority is granted.
- [Phase 10]: 10-01: registered ids ntsc-cart-4m-openipl3-rdram-b-t1-si-v2 (+ -e4k -e16k -s32k), tuple 13/21/26, digest domain t64.state.si.26, container word 2 / t64.state-container.v2 recorded in PIN-IMPACT.md before any digest pin (one-way once 10-09 pins a digest)
- [Phase 10]: 10-01: Wave 0 decisions: no new hosted CI step, no Phase 10 JUnit root in gate.sh, GATE-02 keeps whole-file hashes of the other 13 bound files (new tests in new files), rsp-no-hle guard removal measured last and declined if it needs a qualify-audio.py edit, make -j2 declined
- [Phase 10]: 10-01: sh scripts/preflight-phase10.sh is cited by every Phase 10 plan (23 s no C change, 57 to 70 s with one); --no-replay mid-task, clean committed tree before a push; each new CTest name needs PHASE10_TESTS, label phase10 and a regex count row
- [Phase 10]: 10-01: p10-mutants.py dirty-tree check counts only tracked changes outside .planning/ (planning records cannot affect a mutant built from HEAD)
- [Phase 10]: 10-02: K1 pins Z refusal on all five shipped composite ids at dispatch (SetZImage, SetPrimDepth) and apply (four Z triangles) with 30 build-identity-independent digest literals
- [Phase 10]: 10-02: digest-sweep.py refuses any --base that is not a 40-hex commit id and forces one T64_BUILD_ID; the printer continues resource-limit yields to the replay player bound of 64
- [Phase 10]: GATE-02: the Phase 7 oracle request pins each of the 39 fixture-manifest rows by its own canonical hash looked up by id (orchestrator rebind a4bb7774... to e131cbe7..., identity-only, trusted pins stay None); appended manifest rows need no rebind
- [Phase 10]: Phase 9 claim blocks bind the pinned Phase 9 manifest digest b75864fd... plus a canonical 39-row view (beb2b1c8...) instead of the live manifest file; RECORDED EXCEPTION for owner review: delegated autonomy and GATE-02 basis, not an owner approval of this comparator change (PIN-IMPACT.md section 9)

### Pending Todos

None for v1.1. Carry-ins for v1.2 are listed in the Deferred Items table, the v1.1 audit (milestones/v1.1-MILESTONE-AUDIT.md) and docs/ci-reviews/LOG.md F-096..F-114. VID-04 and Plan 07-13 remain blocked by competent authority, per-file/build-input rights and missing exported replay inputs; do not infer a grant.

### Standing gates (not blockers; none stops v1.2 planning)

- VID-04: D5 Angrylion-Plus authority is unresolved (owner/legal); parallel-rdp needs a per-file/asset rights audit. This blocks that differential acceptance, not permitted authored/native RDP/VI work. Do not execute/use/copy oracle material until applicable gates resolve; authored tests cannot substitute for it.
- D2: external diagnostic public CI/results remain blocked on documented owner rights. Existing qualified local-only routes retain their exact limits.
- D4: keep the existing copy allowlist until owner authorization changes it.
- D6: releases wait for owner-selected channel/versioning. No software release or version tag is implied by this milestone.
- D7: name remains uncleared; no branding, logos or store listings.
- Protected inputs/publication and physical-console measurement retain the explicit stops in [AUTONOMY.md](AUTONOMY.md). No new hardware qualification or broad compatibility claim.
- Superseded 2026-11-23 by the v1.2 oracle-pin approval below. Phase 9 oracle-request rebinds: owner approved in conversation on 2026-11-19 (answer "Yes, rebind per milestone") identity-only rebinds of the Phase 7 oracle request whenever Phase 9 manifest rows move the fixture manifest hash (same scope as 15fd8d1). Each rebind reproduces the old pinned hash first, changes only manifest identities, the derived request hash and PINNED_REQUEST_SHA256, and keeps local_decision blocked. This grants no oracle, rights or publication authority. First Phase 9 rebind: manifest sha256 23b34424...4972, request 4200bb02...5500.

## Deferred Items

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| Integration | Public API/ABI freeze, installed consumer, minimal host, playable homebrew, LibFuturo evaluation | Scheduled: Phases 11–12 (VER-05, API-01..07) | v1.1 activation | v1.2 |
| RDP oracle | VID-04 differential oracle evidence (Plan 07-13) | Deferred, unmapped; needs owner D5 decision and parallel-rdp rights audit | v1.1 close (2026-11-20) | v1.2 only by owner decision |
| Breadth | Performance, hardware accuracy, expansion/accessories and player features | Provisional evidence-driven outline | v1.1 activation | v2.0 |

## Session Continuity

Last session: 2026-11-24T01:46:45.173Z
Stopped at: Completed 10-03-PLAN.md
Resume file: None

Historical closeout: [v1.0 roadmap](milestones/v1.0-ROADMAP.md), [audit](milestones/v1.0-MILESTONE-AUDIT.md), [lessons](RETROSPECTIVE.md).

## Operator Next Steps


- Plan Phase 10 with /gsd-plan-phase 10 (discuss first per AUTONOMY.md)


## Resolved local privacy stop

The owner approved sanitation of the single unpublished evidence commit.
The document-only amendment preserved public history. Original failed gate and
exact tested artifacts remain retained locally; full privacy and the final gate
pass. This approval applies to that specific repair, not future private values.
