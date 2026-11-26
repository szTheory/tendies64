# Roadmap: Tendies64

## Milestones

- ✅ **v1.0 Verified Headless Core** — Phases 1–6 complete; planning archive 2026-11-15. [Verified scope and history](milestones/v1.0-ROADMAP.md). This is not a software release.
- ✅ **v1.1 Observable Machine** — Phases 7–9 complete 2026-11-20 (VID-04 descoped to v1.2 by owner decision). [Archive](milestones/v1.1-ROADMAP.md), [requirements](milestones/v1.1-REQUIREMENTS.md), [audit](milestones/v1.1-MILESTONE-AUDIT.md), [phase records](milestones/v1.1-phases/). This is not a software release.
- 🚧 **v1.2 Embeddable and Playable** — Phases 10–12; planned 2026-11-23. 14 requirements mapped; scope refined by fan-out 2026-11-23; VID-04 deferred and unscheduled. This is a planning identifier, not a software release.
- 📋 **Rolling horizons (v1.3 near, v1.4 mid, v1.5+ later)** — refreshed every milestone; see [Rolling Roadmap](#rolling-roadmap-refreshed-2026-11-23).

## Decision Gates

These gates block specific phases. Already decided, so not gates: Apache-2.0 license and a public
repository under szTheory (owner, 2026-11-09).

| ID | Decision | Status / who decides | Default until decided | Blocks |
|----|----------|----------------------|-----------------------|--------|
| D1 | Implementation language, build system, API/ABI | Recorded as the Phase 1 ADR. Self-unblockable, but one-way once core code exists, so it goes in the owner decision batch. Not an owner stop | C17, CMake 3.25+, Ninja, CTest (Rust is the runner-up) | Any core code merging (Phase 1 exit) |
| D2 | `nemu64-test` ROM use in public CI and public artifacts | **Owner stop** (AUTONOMY section 4). Rights not documented (Geneva 9 font, embedded IPL3, compiled crates) | Build and run locally only. No committed ROMs, no uploaded artifacts, no public results derived from it | Public CI lanes and published results in Phases 4-6. Does **not** block local work |
| D3 | IPL3 variant/revision pin and maintenance owner | Resolved in Phase 3 | n64brew/libdragon IPL3 compat pinned to 98154dafd5ac50e74ee09a5de1035d24a53f54f0; source/binary Unlicense reviewed; Tendies64 maintainers own the pin. See Phase 3 primary/provenance and verification records | Phase 3 (BOOT-02) |
| D4 | Copy allowlist beyond ISC, MIT, BSD-3, Unlicense | Open (owner) | Keep the allowlist as written. Link-only zlib host dependencies follow the dependency policy, with notices | Any copy of out-of-list code (e.g. BSD-2, zlib, MIT-0, Apache-2.0 sibling code) |
| D5 | Angrylion-Plus as a local oracle; redistribution of derived expected data | Unknown (owner or legal) | Out-of-tree local runs only. Never copy its code. Derived data stays local | Phase 7 conformance claims that use it |
| D6 | Release channel and versioning | Open (owner) | Research and local implementation continue; nothing is released | Releases at milestone close only |
| D7 | "Tendies64" name clearance | Not trademark-cleared | No branding, logos or store listings | Public branding and store presence |

The standing hard stops in AUTONOMY section 4 apply to every phase: protected content and fixtures
without documented rights; physical-console measurement; PII, credentials and private paths; and
paid services.

## Phases

<details>
<summary>✅ v1.1 Observable Machine (Phases 7–9) — COMPLETE 2026-11-20</summary>

- [x] Phase 7: RDP Reference Renderer and VI Scanout (12/12 milestone plans) — completed 2026-11-20; Plan 07-13 and VID-04 deferred to v1.2
- [x] Phase 8: Audio Interface and Raw Capture (5/5 plans) — completed 2026-11-19
- [x] Phase 9: Controller, Save Media and Replay (13/13 plans) — completed 2026-11-20

</details>

## v1.2 Embeddable and Playable

**Milestone Goal:** A frozen C-ABI, proven through an independent installed consumer, and a
rights-cleared homebrew that plays in a minimal host, so Tendies64 does not stay tests-only.

**Ordering rule (decided at v1.2 start):** every carry-in that changes state, descriptors or
digests (VID-07, CORE-06, VER-06, RUN-07, GATE-01) closes in Phase 10, before the C-ABI shape is set in
Phase 11, so the API and state format rest on settled behavior. Phase 11 ends with a bound freeze
candidate; the final bind happens at the end of Phase 12, after a real host has used the API
(fan-out decision 2026-11-23; no external consumer exists, so an early lock protects nobody).

**Deferred, not scheduled:** VID-04 (differential oracle evidence) is unmapped. It joins v1.2 only
by an owner D5 decision plus the parallel-rdp per-file rights audit, a rights-cleared exported
corpus and a replay adapter (Plan 07-13). No oracle work happens until then.

- [x] **Phase 10: Carry-in Closure Before the Freeze** - Per-row oracle pins, the macOS gate inside its limit, the Phase 7 test-strength debt, Z depth under new profile ids with one state/container version marker, and one composite end-to-end fixture (completed 2026-11-25)
- [x] **Phase 11: Installed Consumer and Freeze Candidate** - Installable instance API, an independent installed consumer with a host-shaped loop, mechanical API-04 and refusal-is-a-no-op checks, then a bound freeze candidate (completed 2026-11-26)
- [ ] **Phase 12: Minimal Host, Playable Homebrew and Final Bind** - Host pump and optional SDL3 shell on the installed API, an authored playable title, the LibFuturo evaluation, then the final ABI bind

**Execution notes (fan-out 2026-11-23; see [synthesis](research/v1.2-fanout/SYNTHESIS.md)):** one phase per group, about 10-15 plans each, plans capped at 4 tasks and about 45 minutes with no decimal plans; every phase opens with a scripted preflight under about 90 s and closes with a review-fix-reverify wave run in scratch clones; at most 3 phases and one owner-decision batch per milestone.

### Phase 10: Carry-in Closure Before the Freeze
**Goal**: Everything that changes state, descriptors or digests lands once, before the API is shaped: the composite profiles render depth under new ids, the state container carries one version marker, the accepted Phase 7 gaps have independent tests, one composite fixture runs end to end, and the gate fits its job limit
**Depends on**: Phase 9
**Requirements**: GATE-02, GATE-01, VER-06, VID-07, CORE-06, RUN-07
**Decision gates**: D5 (no oracle material; every expected value is authored independently). Owner approval 2026-11-23: per-row oracle pins (GATE-02) plus a standing identity-only rebind for v1.2 manifest rows until it lands (reproduce the old pinned hash, change only manifest identities and the derived request hash, keep `local_decision` blocked, grant no oracle, rights or publication authority)
**Research**: LIGHT (recheck RDP depth semantics against the primary RDP documentation before deriving expected values)
**Wave order**: 0 scripted preflight, pin-impact map, GATE-02 and GATE-01 (ci.yml, check-ci-yml.sh and gate.sh are bound source, so they change first); 1 VER-06 (VG-01 on descriptor 10 before depth is enabled elsewhere); 2 VID-07 and CORE-06 as one identity and state bump (new registered ids, state v26 and container v2, v24/v25 byte-identical, completeness flip test first); 3 RUN-07 (the one manifest-moving commit); 4 independent review per wave and one qualification. Triage the Phase 9 items tagged for this phase (splitting `unit.core.state.import`, replay-driver duplication, the dead publisher link) as done, deferred or dropped
**Success Criteria** (what must be TRUE):
  1. An authored depth fixture on the new composite ids produces Z-tested pixels and depth values equal to independently calculated literals, a wrong-depth control fails, and the shipped ids keep their digests and Z refusal byte-identical across the 17 pre-Phase-9 profiles and the Phase 8/9 composites
  2. A state exported mid-primitive and mid-scan-out, with depth active, imports and continues to the same frames, digest and outputs; one version marker is present, a flip-every-member test sees each RDP and VI member, and the docs state that blobs are build-identity-bound and outside the ABI promise
  3. VG-01 to VG-05 and the standard-NTSC VI clock vector each have independently derived expected values and at least one mutant that fails
  4. One composite guest reports exact raw video and audio from a capture run and exact save bytes from a save-out run with the same final digest, each output compared with its own independent expectation and each with a wrong-value control
  5. A measured cold-cache hosted gate-macos run finishes inside its limit; before/after step timings, the 15-minute limit with its scanner rule, and the pinned limit relation are recorded; adding a manifest row no longer requires an oracle-request rebind
**Plans:** 21/21 plans complete (linear chain; phase-waves 0-4; review plans 10-05, 10-08, 10-13, 10-18; gap closure 10-20 and 10-21 for the SC2 VI half found by verification)
**G5 exception (recorded 2026-11-23, plan check iteration 1):** 19 plans exceed the about 10-15 guideline because the four per-wave review plans are mandated by AUTONOMY v1.2 ("review per wave in scratch clones") and the qualifier (10-17) is split from the one qualification (10-19); folding them would break the 4-task / about 45-minute plan cap, so the plans are not folded. Gap closure (2026-11-25): verification found SC2's VI half without a committed test (W2-03), and AUTONOMY v1.2 makes a defect fix a new numbered plan, so plans 10-20 (VI rows and mutant) and 10-21 (review, re-qualification, push, evidence) bring the count to 21.
**G4 exception (recorded 2026-11-23, plan check iterations 2 and 3; for owner review):** the required gate grows in Phase 10 without removing equal time, because the new tests (K1 Z refusal, the VG vectors, the v26 identity and state round trips, the RUN-07 composite runs and the per-row binding controls) are the required evidence of this phase's success criteria and must gate everything later phases build on. No offset removes measured gate time: GATE-01's explicit CTest COST properties change the ctest scheduling order only, and the 15-minute gate-macos limit with its scanner rule and SC5's measured cold-cache run raise the ceiling and measure the result, but shorten nothing; the redundant rsp-no-hle guard in the audio-records step (about 25-32 s, research C3) is a stop-loss that may be declined, so it is not counted as an offset unless its removal is measured. Expected growth: the gate grows by the summed native seconds of every new Phase 10 CTest name; that value is not known at planning and is the PIN-IMPACT.md "Gate-growth ledger" total row (per-wave rows from 10-05, 10-08, 10-13 and 10-18, the measured total from 10-17 before the cold run), and the binding figure is the measured cold-cache gate-macos job seconds of plan 10-19 against the 900 s limit (pre-Phase-10 cold projection 673 s). Reversal evidence: the PIN-IMPACT.md "Gate-growth ledger" records each wave's new names, measured added native seconds, hosted delta and margin to 900 s; if the measured cold-cache gate-macos run (plan 10-19) exceeds 900 s or is cancelled at the limit, a new numbered plan moves non-essential Phase 10 cases (sampled sanitizer runs, duplicate-path round trips) to a non-gating lane and the cold run is repeated. The exception covers Phase 10 only; Phases 11 and 12 follow G4 as written.

Plans:
**Wave 1**
- [x] 10-01-PLAN.md — W0: scripted preflight, pin-impact map with the one-way id record, mutant driver skeleton

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 10-02-PLAN.md — W0: K1 Z-refusal characterization on descriptors 11/12, committed two-tree digest sweep

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 10-03-PLAN.md — W0: GATE-02 per-row oracle pins of every manifest row (orchestrator-only checker edit and rebind) and the Phase 9 claim-block binding to the pinned Phase 9 manifest rows

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 10-04-PLAN.md — W0: GATE-01 COST order, guard disposition, gate-macos 15 min with scanner rule and selftest, LOG rows

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 10-05-PLAN.md — W0 review in a scratch clone, push, warm after-timings

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 10-06-PLAN.md — W1: VG-01 on descriptor 10, Z exponent boundaries, NTSC clock vector, mutants

**Wave 7** *(blocked on Wave 6 completion)*
- [x] 10-07-PLAN.md — W1: VG-02 to VG-05 with independent literals and mutants

**Wave 8** *(blocked on Wave 7 completion)*
- [x] 10-08-PLAN.md — W1 review, re-derivation, push

**Wave 9** *(blocked on Wave 8 completion)*
- [x] 10-09-PLAN.md — W2: descriptor-13 family registration, RED completeness test, depth activation and v26 digest

**Wave 10** *(blocked on Wave 9 completion)*
- [x] 10-10-PLAN.md — W2: depth behavior literals and wrong-depth control, predicate matrix, shipped-id proof, depth/K1 docs

**Wave 11** *(blocked on Wave 10 completion)*
- [x] 10-11-PLAN.md — W2: state 26 export, container v2 version marker, independent decoder, build-identity docs

**Wave 12** *(blocked on Wave 11 completion)*
- [x] 10-12-PLAN.md — W2: state 26 import, mid-primitive and mid-scan-out round trips, rules, fuzz

**Wave 13** *(blocked on Wave 12 completion)*
- [x] 10-13-PLAN.md — W2 review of the identity/state bump, push

**Wave 14** *(blocked on Wave 13 completion)*
- [x] 10-14-PLAN.md — W3: authored composite guest, builder, oracle, the one manifest-moving commit

**Wave 15** *(blocked on Wave 14 completion)*
- [x] 10-15-PLAN.md — W3: two supervised runs with equal digest, per-output expectations and controls, runner container-v2 format pairing

**Wave 16** *(blocked on Wave 15 completion)*
- [x] 10-16-PLAN.md — W4: provisional-name sweep of 7 spec docs, Phase 9 carry-in triage

**Wave 17** *(blocked on Wave 16 completion)*
- [x] 10-17-PLAN.md — W4: thin-delta Phase 10 qualifier and rehearsal

**Wave 18** *(blocked on Wave 17 completion)*
- [x] 10-18-PLAN.md — W4: Wave 3 review and final independent audit, push

**Wave 19** *(blocked on Wave 18 completion)*
- [x] 10-19-PLAN.md — W4: qualification, orchestrator cold-cache hosted run, evidence and claims

**Wave 20** *(blocked on Wave 19 completion; gap closure)*
- [x] 10-20-PLAN.md — Gap closure: VI member rows in v26-completeness (RED by mutant, GREEN), VI omission mutant, count consumers, W2-03 closed

**Wave 21** *(blocked on Wave 20 completion; gap closure)*
- [x] 10-21-PLAN.md — Gap closure: clone review of wave 20, new Qualification base, native re-qualification, push and hosted pair, refreshed evidence and claims

### Phase 11: Installed Consumer and Freeze Candidate
**Goal**: An independent consumer using only a clean installed package exercises every lifetime, admission, advance, output and failure postcondition of the C-ABI, including a host-shaped loop, and the API reaches a bound freeze candidate that the host work can still amend only by evidence
**Depends on**: Phase 10
**Requirements**: VER-05, API-01, API-02, API-03, API-04
**Decision gates**: D6 (installing and freezing are local; neither is a release, tag or packaging channel; "mismatched version" means an ABI constant, never a release version). Set `git.create_tag` false
**Research**: DEEP (facade, packaging and ABI identity are greenfield: no `install()` rules, no reset/unload/poison/status classes today; also do the paper LibFuturo contract read here)
**Order**: API shape changes land first and once (struct_size, ABI handshake, status classes, poison, reset/unload, internal symbol prefixing, `__stack_chk_*` decision, 20 stale "Phase 10 freeze" mentions relabeled, the 19 deferred "provisional until Phase 10" production-code comments in core/, runner/ and tools/ relabeled (left by Phase 10 because editing them moves the build id)); then installability (fold sha256 into core, relocatable package config, SameMinorVersion); then the independent consumer (do not re-point the runner at the installed package: three-way agreement keeps independence and runner-keyed evidence); cheap-first checks (self-contained header compile, layout asserts, `cmake --install` from the existing build tree, `nm` golden, object lint, absolute-path scan) before the long suite
**Carry-ins from Phase 10** (docs only, reasons in PIN-IMPACT.md sections 12, 13 and 13.1; each is judged by the freeze-candidate review, none blocks the install work): the deferred "until Phase 10" production comments are 20 lines in 8 files by a wrap-aware search (19 at planning; done when `git grep -n -i -E 'until (the )?Phase 10' -- core runner tools` prints nothing); W2-01 (the Z product bound of `state_z_products_bounded` is not pinned by a committed boundary test); W2-02 (`t64_has_si_v25_profile` and `t64_has_si_v26_profile` are not tokens of `scripts/list-membership-sites.py`, six site rows missing); DEF-W1-01 (three unpinned copies of the perspective floor); W1-03 (one sentence stating the `ceil` rule of the v8 clock in `docs/spec/vi.md`, whose NTSC comment cites a section that does not state the rounding); D-10-17-01 (the 99-attempt cap of the two Phase 6 cold-boot tests `integration.cold-hello` and `integration.cold-startup` in a long-lived build tree, `deferred-items.md`); the Phase 9 items split of `unit.core.state.import`, replay-driver duplication, closed recorder-failure detail names, `T64_REAL_KEEP_WORKSET`, the third SI trace kind name and the case-to-CTest mapping checker
**Success Criteria** (what must be TRUE):
  1. A consumer built against a relocated clean install runs the Phase 10 composite fixture and agrees with the in-tree runner and independent literals; missing-package, version-mismatch, visible-source-tree, doctored-header and symbol-collision controls fail
  2. Through the installed consumer every API-01 operation has an accepted case and a refusal, including output rings with forced backpressure and two instances in one process, in a host-shaped loop (frame-paced advance, drain in host order)
  3. A call-sequence generator proves every refused status leaves observations bit-identical, a guest-stop resumes, and a defect poisons the instance, under ASan and UBSan against the shipped library; every status and stop reason appears once in the class table
  4. The installed archive's members import no file, clock, entropy or thread symbol, and a seeded violating object fails the check
  5. A bound freeze-candidate record (header hash, public symbol list, struct-layout table, status-class table) exists and an incompatible change fails a check; an additive-extension rule (size fields, additive profiles, devices and error subcodes) is documented
**Plans:** 29/29 plans complete (linear chain; phase-waves W0 11-01, SHAPE 11-02 to 11-14 reviewed by 11-15, INSTALL 11-16 to 11-20 reviewed by 11-21, FREEZE 11-22 to 11-24, final audit and qualification 11-25)
**G5 note (planning 2026-11-25, revised after plan check iteration 1):** 25 plans exceed the about 10-15 guideline and the research estimate of 14-16. The one-way shape wave was first split into six plans so none passes the 4-task / about 45-minute cap; the plan check (B-4) then required at most 15 files per plan, so the init-macro codemod became six directory-group plans (11-05 runner/, 11-06 to 11-09 core/tests groups A to D, 11-10 runner/tests plus the init-form guard, each at most 15 files) and the flip became three plans (11-12 the atomic flip, 11-13 the separable struct_size prologues, 11-14 the bound re-proof and orchestrator commit); three review/audit plans are mandated by AUTONOMY v1.2. **File-count exception (11-12):** 81 files, of which 67 are the output of one codemod invocation (--create --trace over every non-bound caller), because the create and trace_drain signatures and their callers must change in one compile state (a commit with the new signatures and one unconverted caller does not compile); the 14 hand-edited files are within the cap and everything separable moved to 11-13; the three 11-12 tasks are one coupled commit and cannot be split at a task boundary. **File-count exceptions 11-03, 11-04, 11-11 (accepted by orchestrator ruling, plan check iteration 2: project rules cap tasks and time per plan, not files):** 11-03 (24 files) keeps the poison flag, in-call flag and entry guard over all 23 machine-taking entries in nine core sources in one plan, because the API-02 defect postcondition holds only when no entry is left unguarded, and the sealed-observer guards are followed by their isolated no-HLE refresh in the same plan; 11-04 (18 files) is ordered before 11-11 and 11-12 because it adds the last new public symbols (the prefix header is generated after them) and the retained-slot types the flip relies on, and its four tasks are separable; 11-11 (33 files) commits the nine SoftFloat guard removals (20 deleted lines: 3 in each of the two s_countLeadingZeros files, 2 in each of the other seven) together with the generated prefix header because the rename cannot apply to a self-guarded name, and carries the build-id-moving relabel inside the shape wave before the flip; its Tasks 3 and 4 are separable from Tasks 1-2. 11-04 and 11-11 keep their separable tasks together to avoid a further plan renumbering and one extra review/rebase hop (accepted trade-off). Reversal: if execution shows any of these plans exceeding about 45 minutes, the executor stops at a task boundary and the orchestrator splits the rest into a new numbered plan appended after 11-25 (never a decimal plan number). Each stays within the 4-task cap. **DEV-11-01 (PIN-IMPACT DR-22, corrected after the plan check):** from the 11-12 flip commit to the 11-14 bound commit the nine oracle-bound test executables do not build in the shared tree, because the 2026-11-25 owner approval requires their call-site edits as one isolated commit placed last in the shape wave; the commits in that window are not consecutive (refresh, prologue and planning commits sit between), each code commit there is made only after the full gate passes in a marked scratch clone with the proven bound patch, nothing is pushed or gated until 11-15 (every pushed head compiles), and git bisect across the window is unsupported for those nine tests. **G4:** Phase 11 records its own gate-growth exception for owner review in its PIN-IMPACT ledger (new names are required evidence and each carries a COST property; reversal: a projected cold gate-macos above 900 s moves non-essential Phase 11 names to a local lane).

Plans:

**Wave 1**
- [x] 11-01-PLAN.md — W0: scripted preflight, Wave 0 base, bound-file scratch inventory, class inventory, decision record (DR-01 to DR-32), carry-in triage, LibFuturo paper read, sweep printer shim

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 11-02-PLAN.md — SHAPE: T64_API, extern "C", ABI constants and t64_abi_version, statuses 34-38, status-class table and checker

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 11-03-PLAN.md — SHAPE: poison and in-call guards (t64_rom.c allocator sites included), poison query, AMBIG-A/B defect sites, code-derived scheduler whitelist, bus-write carve-out

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 11-04-PLAN.md — SHAPE: public reset, unload and output status; internal retained-slot types

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 11-05-PLAN.md — SHAPE: public init macros, the codemod with its marked-scratch-root exception, runner/ group

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 11-06-PLAN.md — SHAPE: codemod core/tests group A (CPU, boot, ROM admission)

**Wave 7** *(blocked on Wave 6 completion)*
- [x] 11-07-PLAN.md — SHAPE: codemod core/tests group B (RSP, DMA, RDRAM, devices)

**Wave 8** *(blocked on Wave 7 completion)*
- [x] 11-08-PLAN.md — SHAPE: codemod core/tests group C (state, save, digest, trace)

**Wave 9** *(blocked on Wave 8 completion)*
- [x] 11-09-PLAN.md — SHAPE: codemod core/tests group D (scheduler, profile, lifecycle, composite, Phase 11 API tests)

**Wave 10** *(blocked on Wave 9 completion)*
- [x] 11-10-PLAN.md — SHAPE: codemod runner/tests group and the init-form guard

**Wave 11** *(blocked on Wave 10 completion)*
- [x] 11-11-PLAN.md — SHAPE: SoftFloat guard edits and t64i_ prefix header, stale freeze-text relabel and guard, ADR-0002/0006 amendments, W2-02, T4 and T7

**Wave 12** *(blocked on Wave 11 completion)*
- [x] 11-12-PLAN.md — SHAPE: the atomic flip (struct_size layouts, create-info handshake, record_size, 67 mechanical callers; file-count exception), scratch bound-patch proof before the commit

**Wave 13** *(blocked on Wave 12 completion)*
- [x] 11-13-PLAN.md — SHAPE: struct_size prologues on every remaining in/out entry, scratch proof before the commit

**Wave 14** *(blocked on Wave 13 completion)*
- [x] 11-14-PLAN.md — SHAPE: bound-patch re-proof and the orchestrator-only bound-file commit with identity-only rebind (closes the DEV-11-01 window)

**Wave 15** *(blocked on Wave 14 completion)*
- [x] 11-15-PLAN.md — SHAPE review in a scratch clone, push, hosted pair, Install base

**Wave 16** *(blocked on Wave 15 completion)*
- [x] 11-16-PLAN.md — INSTALL tracer: sha256 fold, package export, relocated install, independent consumer smoke, independence guards, Linux undefined set, first hosted install run

**Wave 17** *(blocked on Wave 16 completion)*
- [x] 11-17-PLAN.md — INSTALL: cheap-first ABI checks (header, layout, nm golden, absolute paths) and API-04 object checks with seeded violations and the Q6 spaced-prefix check

**Wave 18** *(blocked on Wave 17 completion)*
- [x] 11-18-PLAN.md — INSTALL: composite fixture through the installed package (three-way, runner-recorded pins labeled) and accepted/refused cases for every operation

**Wave 19** *(blocked on Wave 18 completion)*
- [x] 11-19-PLAN.md — INSTALL: host-shaped loop and two instances, negative-control matrix, symbol collision, plain cc

**Wave 20** *(blocked on Wave 19 completion)*
- [x] 11-20-PLAN.md — INSTALL: call-sequence generator, local ASan/UBSan and TSan lanes

**Wave 21** *(blocked on Wave 20 completion)*
- [x] 11-21-PLAN.md — INSTALL review in a scratch clone, push, hosted pair, Freeze base

**Wave 22** *(blocked on Wave 21 completion)*
- [x] 11-22-PLAN.md — FREEZE: docs/spec/api.md and carry-ins W2-01, W1-03, D-10-17-01, T8

**Wave 23** *(blocked on Wave 22 completion)*
- [x] 11-23-PLAN.md — FREEZE: bound freeze-candidate record and the incompatible-change check

**Wave 24** *(blocked on Wave 23 completion)*
- [x] 11-24-PLAN.md — FREEZE: Phase 11 qualifier, fresh-archive lane, hermetic test, rehearsal

**Wave 25** *(blocked on Wave 24 completion)*
- [x] 11-25-PLAN.md — FREEZE: final independent audit, qualification, claims and evidence, hosted pair at a pinned SHA

**Wave 26** *(defect fix for the SHAPE review findings; executed out of wave order, right after 11-15)*
- [x] 11-26-PLAN.md — SHAPE defect fixes: output drain and status defect sites (S-02), public-header text (S-01), state-container check 1 line (S-03)

**Wave 27** *(defect fix for finding F-20-A of plan 11-20; executed out of wave order, after 11-20 and before the 11-21 review)*
- [x] 11-27-PLAN.md — advance span checks (F-20-A, review candidate g), bus value pointer decision, installed-consumer and generator aliasing rows

**Wave 28** *(defect fix for the red hosted pair of the 11-21 review, findings I-11, I-01 and F-129; executed out of wave order, before the rest of 11-21's Task 3)*
- [x] 11-28-PLAN.md — version-mismatch controls on cmake 3.31 and 4, doctored-header ABI minor exactly one higher, F-129 readiness clock in test_rsp_run.py, push and green hosted pair

**Wave 29** *(documentation fix for the INSTALL review before 11-23 binds the header hash)*
- [x] 11-29-PLAN.md — header and spec text fixes found by the INSTALL review (I-02, I-03, I-04)

**Cross-cutting constraints:** <!-- gsd:generated -->
- No oracle-bound file, no expected value, check, case name or control flow changes; only initializer forms [CTX: Owner approval 2026-11-25: bound-file edits only in the one isolated commit placed last in the shape wave]

### Phase 12: Minimal Host, Playable Homebrew and Final Bind
**Goal**: An authored, rights-cleared title runs headlessly and in a minimal host built only on the installed API, the core stays free of the host, and the ABI is bound for good once nothing found in the host work needs amending
**Depends on**: Phase 11
**Requirements**: API-05, API-06, API-07
**Decision gates**: D7 (neutral window title such as "t64 minimal host (development)", no icon, bundle or product name); D6 (no tag or binary redistribution); D4 (SDL3 is zlib: link-only against a system install, never vendored, vendoring is an owner stop); the physical-controller play-through is one owner-attested, non-blocking checklist (owner-held hardware), never a must-have
**Research**: MODERATE (route A is decided: authored assembly title with the pinned Binutils and open IPL3; the libdragon route is research only; SDL3 driver strings and headless gamepad init on both runners are still to verify)
**Success Criteria** (what must be TRUE):
  1. An authored title (16x16 square moved by the D-pad or stick, one fixed AI tone on A, one save slot; frame-locked on the VI interrupt; no Z, textures, RSP microcode or TLB) has manifest rows and produces expected checkpoint digests headlessly through the runner and through the installed consumer
  2. The host pump (ring draining, injected clock, input mapping, scripted input) is unit-tested in the required gate; scripted input gives byte-identical video, audio and save results to the headless run at two fake-clock speeds; a virtual joystick under SDL dummy drivers exercises button and axis mapping
  3. The SDL3 shell is optional, off by default and in a non-gating lane; the core builds and links with no SDL dependency and a build without the host target succeeds
  4. A written LibFuturo fit evaluation pinned to a commit SHA exists with an expected no-adapter-now conclusion, and the final ABI bind is made, or each defect-driven amendment is recorded with evidence and a re-run installed-consumer suite
**Plans:** 13/18 plans executed (linear chain; phase-waves W0 12-01 (reviewed with TITLE by 12-06), TITLE 12-02 to 12-05 reviewed by 12-06, HOST 12-07 to 12-11 reviewed by 12-12, BIND 12-13 to 12-16 (12-14 is the one coupled bind commit and the last ABI-moving plan; 12-13 before it and 12-15, 12-16 after it move no bound hash), QUALIFY 12-17 and 12-18; 12-11 is a conditional amendment slot that closes with a one-line SUMMARY and no code unless 12-10 records a host-found API defect with disposition amend; the cold gate-macos run after the 12-18 push is orchestrator-only)
**UI hint**: no (a development window only; skip UI-SPEC and UI-review steps)

Plans:
**Wave 1**
- [x] 12-01-PLAN.md — W0: scripted preflight, Wave 0 base, registration and bind pin-site maps, LibFuturo re-pin, decision record (DR-01 to DR-33), carry-in triage, Phase 12 G4 exception, plan file-count exceptions

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 12-02-PLAN.md — TITLE: title spec, independent oracle (before the guest), input scripts, checkpoint expectations, harness.title-oracle

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 12-03-PLAN.md — TITLE: authored 128x96 title guest, builder copy, fixture target, probe against the oracle, manifest rows (two commits)

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 12-04-PLAN.md — TITLE: runner rows, one supervised raw-capture run with the runner exclusions recorded, wrong-input and refused-frame controls, runner-recorded pins

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 12-05-PLAN.md — TITLE: installed-consumer title, title-persist and title-refused cases (save image and blank-frame comparisons), generated literals, consumer-recorded pins

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 12-06-PLAN.md — W0 checklist and TITLE review in a scratch clone, push, hosted pair, Host base

**Wave 7** *(blocked on Wave 6 completion)*
- [x] 12-07-PLAN.md — HOST tracer: standalone host project, pure-C pump (clock, advance loop, drain rule), fake-clock cases, include and isolation guards

**Wave 8** *(blocked on Wave 7 completion)*
- [x] 12-08-PLAN.md — HOST: input mapping table, strict scripted source, frame helpers, two-speed equality on the title, mutant driver

**Wave 9** *(blocked on Wave 8 completion)*
- [x] 12-09-PLAN.md — HOST: SDL3 dependency audit, optional SDL3 shell, local sdl lane with dummy drivers and a virtual gamepad, policy scan

**Wave 10** *(blocked on Wave 9 completion)*
- [x] 12-10-PLAN.md — API-07: written LibFuturo fit evaluation at the pinned SHA, its guard, host-found API defect triage (decides 12-11)

**Wave 11** *(blocked on Wave 10 completion)*
- [x] 12-11-PLAN.md — Reserved amendment slot (conditional on the 12-10 trigger line; otherwise a one-line SUMMARY and no code)

**Wave 12** *(blocked on Wave 11 completion)*
- [x] 12-12-PLAN.md — HOST review in a scratch clone with the full mutant run, push, hosted pair, Bind base

**Wave 13** *(blocked on Wave 12 completion)*
- [x] 12-13-PLAN.md — BIND prep (no ABI byte): generalized freeze minor rule in check-abi.py, re-measured bind pin-site map with multi-line aware proofs, Bind staging list

**Wave 14** *(blocked on Wave 13 completion)*
- [ ] 12-14-PLAN.md — BIND: the one coupled bind commit (ABI 1.0 and package 1.0.0, OWNER CONFIRMED as an ABI identity, not a release; D6 owner-review item), every version pin, header settle (FN-03), goldens and bound record, proven in a scratch clone first

**Wave 15** *(blocked on Wave 14 completion)*
- [ ] 12-15-PLAN.md — BIND text (no bound hash): production comment and spec relabels, api.md bound sections and FN-02, record "Evidence for the bind" (FN-04), stale-text guard extension

**Wave 16** *(blocked on Wave 15 completion)*
- [ ] 12-16-PLAN.md — BIND proofs (no bound hash): abi-major-rule unit case, version-mismatch-major control, sweep, oracle-authority, act Linux version controls or recorded skip

**Wave 17** *(blocked on Wave 16 completion)*
- [ ] 12-17-PLAN.md — QUALIFY: Phase 12 qualifier (thin delta), hermetic test, rehearsal, push and hosted pair of the bind head

**Wave 18** *(blocked on Wave 17 completion)*
- [ ] 12-18-PLAN.md — QUALIFY: final independent audit, Qualification base, final native qualification, claims, evidence, owner checklist template, final hosted pair

**G5:** Phase 12 has 18 plans, above the G5 guide of about 10-15 plans per phase: the plan-check of 2026-11-26 required the single bind plan (26 files, one uncommitted two-task unit) to be split into four plans with a safe resume point each (12-13 to 12-16); the scope is unchanged. Recorded for owner review, not as an approval.

**File-count exceptions:** plans above the 10-file warning band are recorded with their reasons in the Phase 12 PIN-IMPACT section "Plan file-count exceptions" (12-08 15, 12-11 15, 12-07 14, 12-15 13, 12-09 12, 12-02 11, 12-05 11, 12-14 11); the shared bookkeeping files (scripts/check-test-inventory.sh, scripts/preflight-phase12-regexes.txt, PIN-IMPACT.md) are listed there once.

**G4:** Phase 12 records its own gate-growth exception for owner review in its PIN-IMPACT ledger (new names are required evidence of SC1-SC4, each carries a COST property, no equal-time offset exists; reversal: a measured or projected cold gate-macos above 900 s moves non-essential Phase 12 names to a local lane first).

## Rolling Roadmap (refreshed 2026-11-23)

Priority order. NEXT and NEAR items name their promotion evidence and blocking decision; MID and LATER name only the evidence class. Refreshed at every new milestone from a fan-out; milestone ids are planning identifiers, not software versions. Source: [fan-out roadmap lens](research/v1.2-fanout/lens-roadmap.md).

**Guardrails.** G1 after the freeze, no ABI break to add a profile, device or error subcode. G2 compatibility appears only as per-profile evidence tiers; private-lane results are never public claims. G3 at most 5 carried warning-level gaps; carry-ins older than two milestones are closed or moved to Out of Scope. G4 the required gate may not grow without removing equal time; toolchain builds, the SDL3 host and any oracle run in cached or optional lanes. G5 at most 3 phases per milestone, about 10-15 plans per phase, one owner-decision batch per milestone, and every conditional phase names its fallback. G6 no evidence derived from the producer under test; oracle agreement is not hardware truth. G7 per-file rights rows before first use of any corpus; commercial content only under a decided private lane.

| Horizon | Milestone | Goal | Promotion evidence / blocker |
|---------|-----------|------|------------------------------|
| NEXT | v1.2 Embeddable and Playable | Frozen C-ABI proven by an installed consumer, authored playable title in a minimal host | Defined above |
| NEAR | v1.3 Real Microcode | RSP-to-RDP and RSP-to-AI paths driven by a real rights-cleared microcode corpus (libdragon rspq/rdpq/mixer, per-file license to confirm), PERF-01 measurement only, EXP-01 Expansion Pak (additive profile). Conditional phase: VID-04 oracle conformance if D5 is granted; fallback ACC-02 hardware-derived public vectors | Per-file rights rows for the corpus; D4 and D5 decisions; real microcode is the gate to any retail-shaped boot |
| MID | v1.4 Commercial-Shaped Boot and Private Lane | HLE boot per CIC profile (EXP-03, 6105 last), large ROMs, FlashRAM and Controller Pak (EXP-04/05), a local private lane for owner-held commercial ROMs with hash-keyed local results and tier labels, PERF-02 only if measurement points at the bottleneck | New decision D8 (private lane); v1.3 evidence |
| LATER | v1.5 and beyond | ACC-01 timing, PAL/MPAL (EXP-02), RTC (EXP-06), Rumble/Transfer Pak, then PERF-03 JIT last; PLAY-01 and PLAY-03 as cheap fillers | Primary-document timing sources; lockstep digest equality for any JIT |
| LONG | Open | 64DD (EXP-07, rights-blocked), optional GPU renderer (PLAY-02), a LibFuturo adapter, public external CI (D2) | Rights-cleared disk route; owner decisions |

**Owner-decision batches.** Batch 1 (any time; does not block v1.2): D4 allowlist additions (BSD-2, zlib, MIT-0, Apache-2.0 sibling code), D5 local parallel-rdp oracle after the per-file audit, D6 release channel and versioning, D7 name posture. Batch 2 (at v1.2 close): D8 private commercial-ROM lane; D2 stays deferred because the authored hello is the public smoke lane. Batch 3 (at v1.3 close, optional): whether the owner will ever run test ROMs on their own console.

## Progress

**Execution order:** 10 → 11 → 12

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 10. Carry-in Closure Before the Freeze | v1.2 | 21/21 | Complete    | 2026-11-25 |
| 11. Installed Consumer and Freeze Candidate | v1.2 | 29/29 | Complete    | 2026-11-26 |
| 12. Minimal Host, Playable Homebrew and Final Bind | v1.2 | 13/18 | In Progress | - |
