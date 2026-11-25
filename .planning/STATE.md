---
gsd_state_version: "1.0"
milestone: v1.2
milestone_name: Embeddable and Playable
current_phase: 11
current_phase_name: Installed Consumer and Freeze Candidate
status: executing
stopped_at: Completed 11-11-PLAN.md
last_updated: "2026-11-25T23:47:51.513Z"
last_activity: 2026-11-25
last_activity_desc: Phase 11 execution started
state_head: 90a1f71abdf6b9fa06ad090ac1b6387ee417c727
progress:
  total_phases: 3
  completed_phases: 1
  total_plans: 46
  completed_plans: 32
  percent: 33
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-23)

**Core value:** Every Tendies64 support claim is backed by evidence: a named profile, a pinned build and a reproducible, rights-cleared guest result.
**Current focus:** Phase 11 — Installed Consumer and Freeze Candidate

## Current Position

Phase: 11 (Installed Consumer and Freeze Candidate) — EXECUTING
Plan: 12 of 25
Status: Ready to execute
Last activity: 2026-11-25 — Phase 11 execution started

Progress: [███░░░░░░░] 33% (0 of 3 v1.2 phases complete)


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
| Phase 10 P04 | 19 min | 4 tasks | 8 files |
| Phase 10 P05 | 21 min | 3 tasks | 3 files |
| Phase 10 P06 | 22 min | 4 tasks | 5 files |
| Phase 10 P07 | 22 min | 4 tasks | 5 files |
| Phase 10 P08 | 28 min | 3 tasks | 3 files |
| Phase 10 P09 | 23 min | 3 tasks | 13 files |
| Phase 10 P10 | 27 min | 4 tasks | 11 files |
| Phase 10 P12 | 38 min | 4 tasks | 11 files |
| Phase 10 P13 | 31 min | 3 tasks | 3 files |
| Phase 10 P14 | 26 min | 4 tasks | 11 files |
| Phase 10 P15 | 41 min | 4 tasks | 12 files |
| Phase 10 P16 | 12 min | 3 tasks | 10 files |
| Phase 10 P17 | 44 min | 3 tasks | 7 files |
| Phase 10 P18 | 27 min | 4 tasks | 4 files |
| Phase 10 P19 | 40 min | 4 tasks | 4 files |
| Phase 10 P20 | 17min | 4 tasks | 5 files |
| Phase 10 P21 | 33min | 4 tasks | 6 files |
| Phase 11 P01 | 29 min | 4 tasks | 4 files |
| Phase 11 P02 | 15 min | 3 tasks | 10 files |
| Phase 11 P03 | 35min | 4 tasks | 24 files |
| Phase 11 P04 | 25min | 4 tasks | 19 files |
| Phase 11 P05 | 22min | 2 tasks | 10 files |
| Phase 11 P06 | 39 min | 2 tasks | 14 files |
| Phase 11 P07 | 10min | 2 tasks | 14 files |
| Phase 11 P08 | 12min | 2 tasks | 14 files |
| Phase 11 P09 | 19 min | 2 tasks | 13 files |
| Phase 11 P10 | 22 min | 2 tasks | 13 files |
| Phase 11 P11 | 32 min | 4 tasks | 36 files |

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
- [Phase 10]: 10-04: explicit CTest COST 210, 71, 48 and 35 (hosted macOS seconds of the F-111 run) on unit.core.state.import, harness.state-field-use-audit, repo.rsp-no-hle and harness.phase09-qualification; gate-macos timeout-minutes 15, gate-linux 10, CI-TIMEOUT bound per job; ci.yml, check-ci-yml.sh and gate.sh are final for Phase 10 (commit 4ca1054)
- [Phase 10]: 10-04: the rsp-no-hle guard removal is declined: the repeated cost is the CTest name repo.rsp-no-hle in the audio-records step (41 of 42 s hosted macOS), not the direct guard (0.19 s); removal edits docs/spec/audio-cases.json:182 and EXPECTED_INVENTORY_SHA256 of the closed Phase 8 qualifier (F-116)
- [Phase 10]: 10-04: CTest weights an explicit COST into CTestCostData.txt, so the preflight timeout audit holds a COST test to 2 times its COST (hosted seconds) and the native seconds of those four tests are read from a ctest log (PIN-IMPACT.md 10.5); GATE-01 stays open until the measured cold run of plan 10-19
- [Phase 10]: 10-05: Wave 0 reviewed in a clone-based scratch gate (227 of 227, 119 s) with nine mechanical checks and no blocking finding; the bare git archive tree of the plan cannot pass the gate (no repository), so the later review plans use a local clone detached at the head plus a copy of build/tools/install
- [Phase 10]: 10-05: head 33f6e9c pushed (origin/main 612a3ad to 33f6e9c, 48 commits) after a 67 s preflight; first hosted pair run 45311817943 green (gate-macos 346 s, ctest 184 s, gate-linux 229 s), unit.core.state.import starts 2nd of 227 on both OSes; the saving is not claimed from one sample against the F-119 band (F-120), the cold run stays plan 10-19
- [Phase 10]: 10-05: Wave 1 base f6ab8c26d008b8301b729b5aae2715e1b1079318 recorded in PIN-IMPACT.md section 5; Gate-growth ledger Wave 0 row records 0.36 native seconds for the three Wave 0 names under the G4 exception
- [Phase 10]: 10-06: all VG-01, Z exponent table and standard-NTSC clock vectors pass against current code on the first run; no defect, no fix plan; descriptor 10 may carry depth into Wave 2
- [Phase 10]: 10-06: the exponent-table decode is proved with decal probes at decoded -2..+2 (tolerance 1) because an opaque compare passes every nearer value; two mutant rows added beyond the plan so the case has a recorded kill (vg-zexp-decode-top survives the existing test_rdp_depth)
- [Phase 10]: 10-06: VI vectors must enable the VI at machine tick 0 before the guest runs, because the authored fill guest programs and enables the VI itself; the primitive-Z cap mutant is also killed by the existing test_rdp_depth, so the recorded kill is the named check of primitive-z-saturation
- [Phase 10]: 10-07: VG-02 to VG-05 all pass against current code on the first run (no defect, no fix plan); VG-04 uses a mask-3 wrap tile because a clamped tile maps both floor and truncation candidates to texel 0, and VG-05 (b) sweeps all 16 Bayer cells over c = 72..79 by a direct quantizer call
- [Phase 10]: 10-07: the perspective floor text is repeated in t64_rdp_tmem_triangle_pixel_rgba8 (combiner profiles), project_texture_q16 (LOD path) and texture_channel_q16 (from_major); the VG-04 and VG-05 mutants cover only the v4 texel path and shade_channel, so the siblings are unpinned and left to the 10-08 review or the orchestrator
- [Phase 10]: 10-08: Wave 1 (10-06, 10-07) reviewed in a clone detached at the head: gate 236 of 236, 94 literal comparisons match a document-derived calculator, no production or oracle-bound file moved, sweep 0 differences, 15 mutants KILLED; no blocking finding, VER-06 gaps VG-01 to VG-05 and the NTSC vector confirmed closed
- [Phase 10]: 10-08: three sibling copies of the perspective and major-edge floor are residual (five scratch mutants pass every behavioral test, only the rsp-no-hle body seal fails); deferred item DEF-W1-01, optional new numbered plan
- [Phase 10]: 10-08: second hosted pair green (run 45321391540, head a0c0a38, gate-macos 326 s, ctest 187 s of 236); nine new vg names show no visible cost inside the F-119 band (F-121); Wave 2 base 6dd785986f6aa78c6da5573a558b15b386a4edee recorded
- [Phase 10]: 10-09: registered ids ntsc-cart-4m-openipl3-rdram-b-t1-si-v2 (+ -e4k -e16k -s32k), tuple 13/21/26, profile hashes pinned (3bc028bd, 0a4bfc45, 34f767b0, 9dfb90d8 prefixes; PIN-IMPACT.md 2.1); exact v25 and v26 predicates drive the digest domain, the state registry and the exporter while t64_has_si_profile is their union, so no sealed body changed
- [Phase 10]: 10-09: v26 walker keeps v25 framing, streams the five depth scalars inside the RDP block as new-span members before work_policy_version and the length-prefixed hidden table after the coverage table; completeness flip test RED for exactly the depth members then GREEN; shipped ids byte-identical (sweep 1599 lines, 0 differences)
- [Phase 10]: 10-09: t64_machine_state_export and state_version_registered stay exact v25 (state 26 export refused INVALID_ARGUMENT, import reports the state-version reason) until plans 10-11 and 10-12; ISViewer widened to the eight SI-family ids
- [Phase 10]: 10-10: depth literals on the descriptor 13 ids come from a scratch calculator over the z_format[8] table and the V7 rules; the wrong-depth control compares a 5 byte window (color word, depth word, sidecar entry) of the covered pixel, not the whole Z image, because the covered pixel set is a rasterization result not independently derived
- [Phase 10]: State 26 import (plan 10-12): the canonical domain text follows the machine (t64.state.si.26 for the v26 tuple, t64.state.si.25 for v25), the five depth scalars are read before the triangle-render proof (ADDR_RANGE, BOOL_01, 16-bit INDEX_LT), the hidden table is ENUM_CLOSED per byte (0 or 0x80 to 0x83) and written by the commit for state 26 only; the Z product bound of the acceptance preflight is re-proved from the blob values by a written-out copy guarded by a parity case
- [Phase 10]: Plan 10-13 Wave 2 review passes with no blocking finding (gate 248 of 248, ASan 24 of 24, sweep 0 differences from two bases, one tuple, one domain, one container value 2); design decision D10 (written-out Z product bound) accepted; pushed f57b019 with a green hosted pair (run 45530265365); Wave 3 base 1cded8de34b0c2f5c5bea903095ffc7f1e55d6fc
- [Phase 10]: Plan 10-13 warnings W2-01 (Z product bound and its parity case are unpinned: two perturbed bounds survive the committed tests) and W2-02 (two exact-tuple predicates are not membership tokens, six site rows missing) await a decision on a follow-up numbered plan; neither blocks Wave 3
- [Phase 10]: Plan 10-13 hosted gate-macos warm sample 496 s is the highest so far (cold projection 682 s, margin 218 s, F-122); plan 10-17 re-reads the total before the cold run of plan 10-19
- [Phase 10]: 10-14: composite guest sources (419275d) and rows (f027b05) are two commits so source.revision names a commit holding the source; f027b05 is the one commit touching fixtures/manifest.json since the Wave 3 base
- [Phase 10]: 10-14: composite runner rows use the long guest-tick cap (guest completes at tick 599,357, 643 below the default cap); plan 10-15 passes --input-size and --input-sha256 for composite.input.json
- [Phase 10]: 10-15: the runner pairs the container format string with the profile in one function, state_request_admitted, called from the prelaunch guard (export (25, v1) or (26, v2), import state 25 only, every other pair the existing prelaunch refusal at guest tick 0); the supervisor validator stays profile-blind and accepts the closed set {v1, v2}
- [Phase 10]: 10-15: the wrong-byte composite ROM cannot publish a save (export only after a pass), so controls.real.composite-wrong-rom compares the record the guest read back (the SR transcript line) through the same save comparator, first difference at offset 271 equal to the oracle's wrong.save_first_difference
- [Phase 10]: 10-15: the Phase 9 wrong-format control of runner.state-export uses t64.state-container.v3 because v2 joined the closed set (one literal in test_state_runner.c, case list unchanged); a CMake helper may register only literal TIMEOUT numbers (harness.phase09-qualification scans every CMakeLists.txt)
- [Phase 10]: 10-16: only the container format strings t64.state-container.v1 and v2 and their pairing with states 25 and 26 are settled in Phase 10; every other provisional name in the 7 live spec docs (15 sites plus two settle rows) now reads provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12); the relabel gate prints stale 0 and no closed record or pin moved
- [Phase 10]: 10-16: Phase 9 triage in PIN-IMPACT.md section 13: unit.core.state.import split, replay-driver duplication and the stale production comments deferred to Phase 11; the dead t64_artifact_publisher.c link dropped from test_replay (3deee7d, case inventory unchanged); the deferred comments are 20 lines in 8 files by a wrap-aware search, not 19 (git grep -n -i -E 'until (the )?Phase 10' -- core runner tools must print nothing in Phase 11)
- [Phase 10]: 10-16: W2-01 (Z product bound unpinned), W2-02 (two exact-tuple predicates are not membership tokens) and DEF-W1-01 (three unpinned perspective-floor copies) are recorded as deferred to the Phase 11 freeze-candidate review and named in the ROADMAP Phase 11 Carry-ins line; this plan fixed none of them
- [Phase 10]: 10-17: qualify-phase10.py binds the files changed since the Wave 0 base (40-hex literal) plus 14 tools, excludes .planning/ and docs/ci-reviews/LOG.md from the bound inputs and the dirty check, and a final record requires the Qualification base and refuses non-planning drift since it
- [Phase 10]: 10-17: stage limits are data rows at least twice their measured seconds (gate 600 s, ASan selection 600 s, mutants 600 s, sweep 300 s); the rehearsal on da6af0d took 506 s at host load 12 to 13.6 and verified; the projected cold gate-macos is 696 to 735 s with a 94 s serialized-ceiling warning for 10-18 and 10-19
- [Phase 10]: 10-17: the 99-attempt cap of the Phase 6 cold-boot tests failed the first rehearsal gate; the 193 attempt directories were archived by a move, the broader prune of closed-phase evidence directories was refused by the permission system and not done (deferred-items.md D-10-17-01)
- [Phase 10]: 10-18: Wave 3 and the final audit pass with no blocking finding (scratch gate 254 of 254 in 123 s and ASan phase10 30 of 30 in 128 s in a clone whose archive hash equals the head's; sweep 0 differences over the 21 shipped ids; 25 mutants KILLED with both controls as expected; --require-approved still exits 2); the 10-15 deviations (one literal in test_state_runner.c, wrong-ROM read-back comparison) are accepted as W3-01 and W3-02
- [Phase 10]: 10-18: head f081d39 pushed (origin/main f57b019 to f081d39, 29 commits, no tag); hosted pair run 45548987145 green: gate-macos 385 s (ctest 196 s of 254 names), gate-linux 233 s; warm plus the 186 s Binutils delta projects a cold 571 s (margin 329 s), below the 10-17 projection of 696 to 735 s, F-123; the cold run stays plan 10-19
- [Phase 10]: 10-18: Qualification base ad39b6be2f803af45af15d4ed511b72a89460339 recorded in PIN-IMPACT.md Wave bases (planning-only commit 3860717) after the one source edit of this plan (qualifier derivation sentence 25 mutants and 2 controls, 4dd936a); carried items now count five against G3 with D-10-17-01, and T-10-49 stays flagged for owner review
- [Phase 10]: 10-19: GATE-01 met by a measured cold-cache run, run 45550277510, gate-macos 731 s against the 900 s limit (margin 169 s); above the F-111 (673 s) and plan 10-18 (571 s) projections, inside the 10-17 ranges; the G4 reversal rule is not triggered (F-124)
- [Phase 10]: 10-19: the final native qualification passed on attempt 4 (record 20261125T134440416529Z-b9d957c3, verified once at head b9d957c); three earlier attempts failed at supervisor wall backstops under unrelated host load and stay in the evidence, no limit widened, no source changed (PIN-IMPACT.md 15.1, F-125)
- [Phase 10]: 10-19: docs/claims/phase10.md holds eight unit-tested blocks (each unit.core.vg name bound to one id) and docs/evidence/phase10-qualification.md lists eight recorded exceptions for owner review, none an approval; do not re-run qualify-phase10.py --verify after the evidence commit (the new documents are bound inputs)
- [Phase 10]: 10-20: W2-03 closed by 120 committed VI rows in the existing case v26-completeness (791 member, 400 named-exclusion and 17 padding bytes of 1,208; every byte flipped on the stem id with exactly 417 inert; representative sweep on the three save ids and the state 25 stem); the mutant v26-digest-omit-vi-phase-cycles SURVIVED the case before the rows (RED, be683bf) and is KILLED after them (GREEN, 0ed0d75); no production source, CTest name or limit changed; the plan 10-19 qualification record is superseded by design and plan 10-21 re-qualifies
- [Phase 10]: 10-21: the plan 10-20 wave passes an independent clone review (gate 254 of 254, ASan completeness case, 26 KILLED mutants with both controls as expected, 120 compiler members against 120 VI rows, seven extra walker probes caught); findings R21-01 to R21-03 are info only
- [Phase 10]: 10-21: Qualification base 76451024e148073804e162841c3d0a3543bd2567 is the only record line of PIN-IMPACT.md section 5 (the plan 10-18 value ad39b6be... is a labelled historical line); native record 20261125T155456703622Z-1fc0b1da passed on the first attempt (410.78 s, --verify exit 0), qualified commit 1fc0b1d, no limit widened
- [Phase 10]: 10-21: the verified head 85ba477 is on origin/main (fast-forward, 26 commits, no tag) with a green hosted pair, run 45650890027: gate-macos 368 s and gate-linux 254 s; F-126 and the gap-closure ledger row recorded; the cold run of F-124 (731 s, earlier tree) was not repeated
- [Phase 10]: 10-21: evidence and claims refreshed for the re-qualified tree (28 mutant rows, VI rows in CORE-06, exceptions 3 and 8 updated, new exception 9); W2-03 closed not carried; ROADMAP Phase 11 carry-ins now list all five carried items; origin/main trails local by the document commits
- [Phase 11]: 11-01: owner answer A (2026-11-25) extends the call-site-only rebind approval to the single initializer hunk core/tests/test_rdp.c:486 and no further; t64_host_storage keeps its leading struct_size, filled by the core before each allocate call (DR-06 INFO-03, DR-27); plan 11-14 applies the hunk in its isolated orchestrator commit
- [Phase 11]: 11-01: PIN-IMPACT.md decision record DR-01 to DR-32 binds the shape wave: statuses 34 to 38 appended after 33, ABI 0.1 with SameMinorVersion, create-info handshake, the struct_size set, fixed-width members, init macros, retained slots t64_vi_packet_slot 488 and t64_ai_packet_slot 280, t64i_ prefix, DEV-11-01 window from the 11-12 flip to the 11-14 bound commit
- [Phase 11]: 11-01: bound-file probe measured the planned shape against the nine API-using bound files: 17 call expressions, 120 object initializers and one approved other hunk (110 hunks, 154 diagnostics); runner/tests/test_rdp_video.c needs no edit
- [Phase 11]: 11-01: carry-ins triaged: W2-01, W1-03, D-10-17-01 and T8 done in 11-22; W2-02, T4 and T7 done in 11-11; T3 done in Phase 10; DEF-W1-01 deferred to v1.3, T1 beyond v1.2, T2, T5 and T6 to Phase 12; G3 count projected to one carried gap
- [Phase 11]: 11-01: LibFuturo paper read at local, unpublished commit 4177a43 amends no locked decision; any adapter stays optional and the written evaluation is Phase 12
- [Phase 11]: 11-01: Phase 11 G4 exception for owner review recorded (no offset; new names are required SC1 to SC5 evidence); baseline measured cold gate-macos 731 s of 900 s (margin 169 s); reversal if a review plan projects a cold run above 900 s
- [Phase 11]: 11-01: digest-sweep printer compiles against both API generations through one defined(T64_MACHINE_CREATE_INFO_INIT) selector; sweep reports 1599 shared lines and 0 differences; sh scripts/preflight-phase11.sh passes in 41 s without replay
- [Phase 11]: 11-02: T64_ABI_VERSION is the constant expression ((MAJOR << 16) | MINOR) of two UINT32_C values, T64_API is not overridable by a host define, and docs/abi/status-classes.txt classifies all 39 statuses and 8 stop reasons (SEQUENCE_EXHAUSTED is the one refused row with RESUME terminal); repo.abi-status-classes and its ten-case selftest fail on an unclassified, duplicate, renumbered, orphan or malformed row
- [Phase 11]: 11-03: poison and in-call flags with one entry guard on all 23 machine-taking entries; scheduler handler-error whitelist by status value (INVALID_ARGUMENT and unlisted statuses poison); state export checks before it writes; bus-write carve-out written and tested
- [Phase 11]: Plan 11-04: reset emits the admission record and raises no in-call flag; unload clears guest state without a boot entry and releases the image with the flag raised; output status without an image is OK with zero counts and full free capacity; t64_output_status is 48 bytes; slot copy-out is inline in the two sealed observers; 13 slot member names mapped NONE in state-field-rules
- [Phase 11]: Plan 11-05: DR-09 macros list every member explicitly so the flip edits only the first element; codemod fails closed on EXPECTED_CORPUS_PATHS, the printer and any scratch root that is not below build/ with its own toplevel and the marker; sentinel values on a zero-only type become INIT plus named member assignments; a memset of a converted object is a hand site (it would clear struct_size after the flip)
- [Phase 11]: Plan 11-07: a byte-wise 0xa5 sentinel fill of a macro-type result becomes a file-local poison helper (INIT assignment, then every member set by name) so refusal and overwrite tests keep their detection power after the flip adds struct_size; the codemod still lists an initialized array as a hand site, so --check exits 1 by design while test_ai.c arrays remain, and the 11-10 init-form guard must accept an array whose entries all use the INIT form
- [Phase 11]: Plan 11-08: a 0xa5 fill of an import report or a save info becomes a poison helper (INIT assignment, then every member set to 0xa5.. by name; the shared helper lives in state_import_helpers.h, the save helper is file-local), zeroing memsets of reports and drain results become INIT assignments; a NULL-machine row of test_state_import.c that read a never-written report on stack residue now poisons the report and checks it untouched, matching the status-classes contract that a NULL machine leaves every output untouched; the codemod does not see a pointer-form memset of a macro-type object (report_out), so 11-09/11-10 must grep for it
- [Phase 11]: Plan 11-09: sentinel fills of public in/out records in the two API tests use file-local poison helpers (INIT form, then the 0xa5 values by name) and a scratch or observations record is poisoned member by member with a memcpy reference, so struct_size stays valid after the flip and the byte-for-byte untouched checks keep their power; the helpers are duplicated in the public-only and the white-box test because the plan writes no shared header (11-15 review candidate)
- [Phase 11]: Plan 11-10: the init-form guard (scripts/check-api-init.py) rejects an uninitialized, positional, zero, compound-literal or memset (value and pointer form) use of a public in/out struct outside the T64_*_INIT form, accepts an array whose entries all use the macro, designated descriptor lists, a lone-zero descriptor fixture and the exempt types t64_host_storage and t64_trace_record, and reports exactly the 120 category (b) sites of the 11-01 probe in the nine bound files; the memset rule found two pointer-form import-report fills the codemod does not see (fixed from the INIT form); the codemod --check still lists the 7 INIT-initialized arrays by design, so the guard is the arbiter
- [Phase 11]: Plan 11-11: internal symbols are renamed t64_X to t64i_X and every other core global (the unprefixed SoftFloat names) to t64i_<name> through the generated core/t64_prefix.h (220 entries, scripts/gen-prefix-header.py --write and --check, CTest repo.abi-prefix), reached by one #include from t64_internal.h, softfloat/platform.h, t64_rsp_divider.h and t64_facade.h (core on the t64_core, lint and three runner include paths) instead of -include; the nine vendored SoftFloat primitives lose their self-defining guard (20 deleted lines, provenance recorded); checkers that read object symbols resolve names through the header (check-rsp-no-hle.py, two checker-only commits); the 20 stale until-Phase-10 production lines and the t64_machine.h and t64_facade.h banners are relabeled (repo.stale-freeze-text guards it), runner headers are stated outside the ABI and the freeze record, ADR-0002 and ADR-0006 carry the 2026-11-25 amendments, W2-02 and T7 are done (T64_TRACE_KIND_SI_REFUSED)

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

Last session: 2026-11-25T23:47:51.325Z
Stopped at: Completed 11-11-PLAN.md
Resume file: None

Historical closeout: [v1.0 roadmap](milestones/v1.0-ROADMAP.md), [audit](milestones/v1.0-MILESTONE-AUDIT.md), [lessons](RETROSPECTIVE.md).

## Operator Next Steps


- Plan Phase 10 with /gsd-plan-phase 10 (discuss first per AUTONOMY.md)


## Resolved local privacy stop

The owner approved sanitation of the single unpublished evidence commit.
The document-only amendment preserved public history. Original failed gate and
exact tested artifacts remain retained locally; full privacy and the final gate
pass. This approval applies to that specific repair, not future private values.

### Blockers
None

- Owner approval 2026-11-25 (answer "Approve call-site-only rebind", Phase 11 plan-phase): Phase 11 may perform an identity-only, orchestrator-only rebind of the Phase 7 oracle request after API call-site-only edits to the nine bound test files (core/tests/test_rdp{,_copy,_triangles,_texture,_combiner,_blender,_depth}.c, test_vi.c, test_video_output.c, runner/tests/test_rdp_video.c): reproduce the old pinned hash first, change only those call sites and the derived hashes and PINNED_REQUEST_SHA256, keep local_decision blocked, isolate in one commit. No oracle, rights or publication authority granted.
  - Planner note 2026-11-25 (plan check item I-1; not owner text, the approval above is unchanged): the approval says "nine" and lists ten paths. Phase 11 reads it as nine API-using files edited (the seven test_rdp files, test_vi.c, test_video_output.c) and runner/tests/test_rdp_video.c listed but not edited (it needs no call-site change and keeps its hash); recorded as DR-27 in plan 11-01 and applied by plan 11-14.
- Owner approval 2026-11-25 (answer "A: Extend approval to this initializer", Phase 11 execution, plan 11-01 checkpoint): the 2026-11-25 call-site-only rebind approval also covers the single value-neutral initializer update core/tests/test_rdp.c:486 (t64_host_storage image = {NULL, 0u, 0u} becomes {0}), same identity-only, orchestrator-only, one-isolated-commit, local_decision-blocked terms; no expected value, check, case name, sizeof pin or control-flow edit in any bound file is covered. t64_host_storage keeps its leading struct_size.
