---
phase: 10-carry-in-closure-before-the-freeze
verified: 2026-11-25T17:05:00Z
status: passed
score: 5/5 must-haves verified
covered_files:
  - ".github/workflows/ci.yml"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-01-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-01-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-02-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-02-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-03-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-03-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-04-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-04-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-05-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-05-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-06-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-06-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-07-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-07-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-08-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-08-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-09-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-09-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-10-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-10-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-11-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-11-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-12-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-12-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-13-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-13-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-14-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-14-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-15-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-15-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-16-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-16-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-17-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-17-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-18-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-18-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-19-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-19-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-20-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-20-SUMMARY.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-21-PLAN.md"
  - ".planning/phases/10-carry-in-closure-before-the-freeze/10-21-SUMMARY.md"
  - "core/t64_digest.c"
  - "core/t64_internal.h"
  - "core/t64_machine.c"
  - "core/t64_state.c"
  - "core/tests/test_digest_v26.c"
  - "core/tests/test_k1_refusal.c"
  - "core/tests/test_profile_v26.c"
  - "core/tests/test_rdp_depth_composite.c"
  - "core/tests/test_state_export_v26.c"
  - "core/tests/test_state_import_v26.c"
  - "core/tests/test_vg_gaps.c"
  - "docs/ci-reviews/LOG.md"
  - "docs/claims/phase10.md"
  - "docs/evidence/phase10-qualification.md"
  - "fixtures/composite/composite.S"
  - "fixtures/composite/composite.expected.json"
  - "fixtures/composite/composite_oracle.py"
  - "fixtures/manifest.json"
  - "runner/tests/test_composite_two_run.c"
  - "scripts/check-ci-yml.sh"
  - "scripts/check-rdp-oracle-authority.py"
  - "scripts/digest-sweep.py"
  - "scripts/p10-mutants.py"
  - "scripts/qualify-phase10.py"
covered_digest: "v4:sha256:df971f12192289362decd434d7979af1433a5428d09f4d943c7257e8e9fc5ab7"
behavior_unverified: 0
overrides_applied: 0
re_verification:
  previous_status: gaps_found
  previous_score: 4/5
  gaps_closed:
    - "SC2 VI half: a committed flip-every-member test sees each member of t64_vi_state (W2-03)"
  gaps_remaining: []
  regressions: []
deferred:
  - truth: "W2-01: the Z product bound state_z_products_bounded (core/t64_state.c) is pinned by a committed boundary test"
    addressed_in: "Phase 11"
    evidence: "ROADMAP Phase 11 'Carry-ins from Phase 10': 'W2-01 (the Z product bound of state_z_products_bounded is not pinned by a committed boundary test)'; PIN-IMPACT.md 13.1; evidence document exception 8"
  - truth: "W2-02: t64_has_si_v25_profile and t64_has_si_v26_profile are tokens of scripts/list-membership-sites.py (six site rows missing)"
    addressed_in: "Phase 11"
    evidence: "ROADMAP Phase 11 'Carry-ins from Phase 10': 'W2-02 (t64_has_si_v25_profile and t64_has_si_v26_profile are not tokens of scripts/list-membership-sites.py, six site rows missing)'"
  - truth: "DEF-W1-01: three sibling copies of the perspective floor are pinned behaviorally"
    addressed_in: "Phase 11"
    evidence: "ROADMAP Phase 11 'Carry-ins from Phase 10': 'DEF-W1-01 (three unpinned copies of the perspective floor)'"
  - truth: "W1-03: one sentence stating the ceil rule of the v8 clock in docs/spec/vi.md"
    addressed_in: "Phase 11"
    evidence: "ROADMAP Phase 11 'Carry-ins from Phase 10' now lists W1-03 (added by plan 10-21; the first report had flagged its absence)"
  - truth: "D-10-17-01: the 99-attempt evidence-directory cap of the two Phase 6 cold-boot tests in a long-lived build tree"
    addressed_in: "Phase 11"
    evidence: "ROADMAP Phase 11 'Carry-ins from Phase 10' now lists D-10-17-01 (added by plan 10-21); detail in deferred-items.md"
advisory:
  - finding: "The retained video packet payload buffer (machine->video_packets, hashed per slot up to packet_lengths[slot] by the VI section of t64_digest_stream) is a machine-level allocation outside t64_vi_state and no committed flip test covers it. A scratch mutant dropping that one digest_bytes call survives unit.core.digest.v26-completeness, v26-strip-differential, v25-reference, v25-strip-differential, digest-trace and the three vi suites plus the video-output suite that I tried; I did not run the full gate against it."
    category: other
    reason: "Not a clause of SC2 as scoped by the first report, plans 10-20/10-21 and CORE-06 (those name t64_vi_state members; the payload walker is Phase 8 code that Phase 10 did not change). Reported so the owner can decide whether the freeze-candidate review should pin it; adding it to the carried list would be a sixth item against guardrail G3 (limit five)."
    evidence_status: "reproduced with a scratch mutant outside the repository; not a committed test"
---

# Phase 10: Carry-in Closure Before the Freeze - Verification Report

**Phase Goal:** Everything that changes state, descriptors or digests lands once, before the API is shaped: the composite profiles render depth under new ids, the state container carries one version marker, the accepted Phase 7 gaps have independent tests, one composite fixture runs end to end, and the gate fits its job limit
**Verified:** 2026-11-25T17:05:00Z
**Status:** passed
**Re-verification:** Yes, after gap closure (plans 10-20 and 10-21); the previous report was `gaps_found`, 4/5
**Evidence tier of what was checked:** unit-tested, authored scope. Nothing here is a hardware, oracle or other-emulator claim.

## Summary verdict

The single gap of the previous report (SC2, VI half: no committed test flipped the members of the VI state) is closed, and I re-derived the closure from code and runs rather than from the summaries. All five roadmap success criteria now hold. Production source did not move since the first verification (the only non-planning files that differ are `core/tests/test_digest_v26.c`, `scripts/p10-mutants.py`, `scripts/qualify-phase10.py` and the three evidence and claim documents), so SC1, SC3, SC4 and SC5 carry over from the first report with a native regression run on top. One non-blocking observation about a neighbouring digest input is recorded in `advisory:` and below; it is not a clause of any success criterion.

## How this was verified (not from SUMMARY claims)

- Read the previous report, the 10-20 and 10-21 summaries, the ROADMAP Phase 10 and Phase 11 text, REQUIREMENTS, the evidence document, the claim file, the review and validation records and the PIN-IMPACT carry-in tables.
- **Programmatic member cross-check (the gap itself).** I parsed the `t64_vi_state` and `t64_video_packet_info` definitions out of the headers with a script, generated a C probe that includes the committed test source and prints `offsetof` and `sizeof` for every compiler-derived member (arrays expanded per element, both packet slots expanded per member) next to the rows of the committed `vi_members` table, and compiled and ran it against the existing static library. Result: sizeof 1,208 and 488; 120 compiler members, 120 rows, 0 members without a row, 0 rows without a member, 0 offset or size mismatches; exactly six rows carry a named exclusion (`packet_head`, `packet_count`, and `profile_id` and `build_id` of each slot); byte tally 791 member, 400 excluded, 17 padding (runs of 4 at 188, 4 at 300, 4 at 788 and 5 at 1203) summing to 1,208; expected inert bytes 417. These agree with the constants and static asserts in the test.
- **Case run.** `ctest --test-dir build/gate -R '^unit\.core\.digest\.v26'` passed 2 of 2 against the existing build (no rebuild). Verbose run of the completeness case: `checks=10132 failures=0`. The built test binary is newer than the test source and contains the VI row names.
- **Independent kill reproduction.** I compiled the current test source against two scratch digest objects outside the repository: the unmutated `t64_digest.c` (case passes, 10,132 checks, 0 failures) and one with the single `hash_u64(sink, vi->phase_cycles)` call replaced by a void read (the case fails, `phase_cycles is not in the canonical stream`, 18 failures across the five ids, exit 1). I then made five more single-line omissions of other VI members (`current_readback`, `next_boundary_units`, `latch_pending`, per-slot `last_opcode_field`, per-slot `filter`); each fails the case (8, 12, 5, 16 and 16 member-not-in-stream reports). So the rows reach the comparator for more than the one member the committed mutant removes.
- `python3 -B scripts/p10-mutants.py --check-anchors` exit 0; `--list` shows 28 rows (26 KILLED expected, 1 SURVIVED control, 1 INVALID control); the new row `v26-digest-omit-vi-phase-cycles` is killed by `test_digest_v26:v26-completeness` and its edit anchor occurs exactly once in `core/t64_digest.c`. `repo.p10-mutant-anchors` and `harness.phase10-qualification` pass natively.
- **Native qualification record** `20261125T155456703622Z-1fc0b1da` read from the private build record: `rehearsal: false`, `status: complete`, `commit_sha` 1fc0b1d, empty drift list, Qualification base 76451024... bound, claims `hardware_accuracy` and `oracle_comparison` both false, eight stages inside their limits (ci-scanner 2.98 s, inventory 1.10, oracle 0.22, gate 128.13, phase10-junit 7.02, asan 125.10, mutants 105.00, sweep 40.77; total 410.78 s). The mutants log lists 28 rows with the new row `KILLED KILLED test_digest_v26:v26-completeness`. I did not re-run the qualifier or `--verify` (it refuses later heads by design).
- **Hosted pair.** `gh run view 45650890027`: event push, head 85ba477 (equal to `origin/main`), conclusion success, `gate-macos` and `gate-linux` both success; the logs show `unit.core.digest.v26-completeness` passed on both (3.13 s and 3.85 s) and "100% tests passed out of 254" on macOS. Run 45550277510 (the cold run for SC5) re-read: workflow_dispatch on f081d39, gate-macos 731 s, gate-linux 250 s, both success; f081d39 is an ancestor of HEAD.
- **No-regression diff.** `git diff --name-only` from the commit before the first verification report to HEAD, excluding `.planning/`: `core/tests/test_digest_v26.c`, `docs/ci-reviews/LOG.md`, `docs/claims/phase10.md`, `docs/evidence/phase10-qualification.md`, `scripts/p10-mutants.py`, `scripts/qualify-phase10.py`. `core/` outside `core/tests`, `runner/`, `tools/`, `fixtures/`, `.github/`, `docs/spec/`, `scripts/check-ci-yml.sh`, `scripts/check-rdp-oracle-authority.py` and `scripts/digest-sweep.py` are untouched. Since the new Qualification base the qualifier, the mutant driver and the test source have an empty diff and only the three documents differ. `docs/claims/phase09.md` and `docs/evidence/phase09-qualification.md` are byte-identical to the Wave 0 base.
- **Regression run (native, existing build, no rebuild, no full gate):** 41 of 41 passed: the 23 `unit.core.*` Phase 10 names (K1 x2, profile v26 x3, digest v26 x2, depth-composite x3, vg x9, state export-v26, export-v26-bounds, import-v26-roundtrip, import-v26-hostile), the shipped-id names (`unit.core.profile*`, `unit.core.digest.v25-*`, `unit.core.digest.sink-legacy-identity`, `unit.core.digest-trace`, `unit.core.state.export`, `unit.core.state.import`), `runner.composite-two-run`, `controls.real.composite-wrong-rom`, `runner.composite-state-v2`, `harness.composite-oracle`, `runner.state-format-v26`, `repo.p10-mutant-anchors`. Also `check-ci-yml.sh` ok; oracle authority `--expect-blocked` exit 0 and `--require-approved` exit 2; `composite_oracle.py --check` equal (2,737 bytes); `check-claims.sh` 8 blocks 0 errors; `pii-scan.sh --tree` 0 findings; `digest-sweep.py --selftest` PASS. CTest lists 254 names and 30 `phase10` names (unchanged by the gap closure). The working tree is clean after verification.

## Observable Truths (ROADMAP success criteria, the contract)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | An authored depth fixture on the new composite ids produces Z-tested pixels and depth values equal to independent literals, a wrong-depth control fails, and shipped ids keep digests and Z refusal byte-identical | VERIFIED | Unchanged from the first report: production source has an empty diff since it. The three `unit.core.rdp.depth-composite-*` names, both K1 refusal names, `unit.core.profile.v26-predicate-matrix` (25 ids) and the shipped-id digest names pass natively again (41 of 41). The record's sweep line `1599 shared lines, 0 differences` was produced on the re-qualified tree and the production diff against the Wave 0 base is the same set of files as at the first verification. |
| 2 | State exported mid-primitive and mid-scan-out with depth imports and continues to the same frames, digest and outputs; one version marker; a flip-every-member test sees each RDP and VI member; docs state blobs are build-identity-bound and outside the ABI promise | VERIFIED | RDP half, round trip, marker and docs as in the first report (round-trip and export names pass again; `docs/spec/state-container.md:17` unchanged). **VI half, now closed:** the committed case `unit.core.digest.v26-completeness` carries 120 `vi_members` rows with `_Static_assert`s on both struct sizes; the programmatic cross-check above shows every compiler member has a row with matching offset and size, six named exclusions with reasons, 791/400/17 bytes; on the stem id all 1,208 bytes are flipped and exactly 417 are inert (the test asserts the count), the three save ids and the state 25 stem get the representative sweep; the case passes (10,132 checks, 0 failures) and an independent scratch mutant of the walker fails it for `phase_cycles` and for five other members. The exclusions are correct against the walker: `core/t64_digest.c` states that `packet_head` and `packet_count` stay out of the digest as observer state, and hashes the per-slot `profile_id` and `build_id` only under `!has_v25_framing`. |
| 3 | VG-01 to VG-05 and the standard-NTSC clock vector each have independently derived expected values and at least one failing mutant | VERIFIED | `core/tests/test_vg_gaps.c` and its nine names are unchanged and pass; the re-qualified record's mutants log lists the 14 VG and clock mutants KILLED by their named cases, the equivalent control SURVIVED and the non-compiling control INVALID. |
| 4 | One composite guest reports exact raw video and audio from a capture run and exact save bytes from a save-out run with the same final digest, each against its own independent expectation and each with a wrong-value control | VERIFIED | `runner.composite-two-run`, `controls.real.composite-wrong-rom`, `runner.composite-state-v2` and `harness.composite-oracle` pass again; `composite_oracle.py --check` proves the committed expectation equals the stdlib-only oracle; fixture files and manifest rows have an empty diff since the first report. |
| 5 | A measured cold-cache hosted gate-macos run finishes inside its limit; timings, the 15-minute limit with its scanner rule and the pinned limit relation are recorded; adding a manifest row needs no oracle-request rebind | VERIFIED | Cold run 45550277510 re-read: gate-macos 731 s against the 900 s limit, success. `ci.yml`, `check-ci-yml.sh` and `check-rdp-oracle-authority.py` have an empty diff since the first report; scanner ok, oracle authority exits 0 and 2 as required. The cold run was not repeated after the gap closure (it measured the earlier audited tree); the gap closure added test time only (existing case +1.46 s macOS, +2.83 s Linux; the warm run of the pushed head is 368 s), which the evidence states as a projection of about 733 s, not a measurement. The ROADMAP criterion asks for one measured run inside the limit, which exists. |

**Score:** 5/5 truths verified. 0 truths are present-but-behavior-unverified.

### Deferred Items

Not actionable in this phase: explicitly scheduled in the Phase 11 "Carry-ins from Phase 10" text of ROADMAP.md.

| # | Item | Addressed In | Evidence |
|---|------|--------------|----------|
| 1 | W2-01 importer Z product bound has no committed boundary pin | Phase 11 | ROADMAP carry-ins; PIN-IMPACT 13.1; evidence exception 8 |
| 2 | W2-02 two exact-tuple predicates missing from the membership-site generator | Phase 11 | ROADMAP carry-ins; PIN-IMPACT 13.1 |
| 3 | DEF-W1-01 three unpinned copies of the perspective floor | Phase 11 | ROADMAP carry-ins |
| 4 | W1-03 one sentence of the `ceil` rule in `docs/spec/vi.md` | Phase 11 | ROADMAP carry-ins (added by plan 10-21) |
| 5 | D-10-17-01 cold-boot tests' 99-attempt cap in a long-lived build tree | Phase 11 | ROADMAP carry-ins (added by plan 10-21); `deferred-items.md` |

W2-03 is not carried: it is closed (PIN-IMPACT section 16, `10-REVIEW.md` closing note and Gap closure section), so the G3 count stays at five.

### Carried warnings W2-01 and W2-02 (asked explicitly)

Both remain recorded as carry-ins in the ROADMAP Phase 11 "Carry-ins from Phase 10" line, with reasons in PIN-IMPACT.md section 13.1 and exception 8 of the evidence document. Neither falsifies a clause of any success criterion (W2-01 weakens pin strength of the importer's hostile boundary in new code; W2-02 weakens a discovery aid; independent guards exist). Both stay WARNING, deferred. The first report also flagged that the evidence document said W1-03 and D-10-17-01 were in the ROADMAP list when they were not; that is fixed (the ROADMAP line now names all five carried items).

## Recorded exceptions for owner review (confirmed present)

All nine items are present in `docs/evidence/phase10-qualification.md` under "Recorded exceptions for owner review", each stated as not an owner approval: (1) the Phase 9 claim-binding comparator change (T-10-49), (2) G4 gate growth 224 to 254 names with the gap closure adding none, (3) G5 plan count now 21, (4) the two wave 3 deviations (W3-01, W3-02), (5) the GATE-01 `rsp-no-hle` guard-removal decline, (6) the one orchestrator rebind of `PINNED_REQUEST_SHA256` (identity only), (7) the native qualification retries of the first pass, (8) the five carried items at the G3 limit with W2-03 closed not carried and VID-04 deferred and unclaimed, (9) the re-qualification after the gap closure. These are decisions for the owner, not defects; none is a human-run verification item and no plan carries a `<human-check>` block (the two `checkpoint:human-action` tasks were orchestrator gates, the rebind and the cold run, both completed).

## GATE-01 guard-removal decline

Unchanged from the first report: judged in substance and allowed by the requirement text and CONTEXT; every other element of GATE-01 is met and measured (cold run 731 s of 900 s, 15-minute limit with the scanner rule and selftest in one commit).

## Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `core/tests/test_digest_v26.c` | RDP rows plus VI rows, size asserts, both sweeps | VERIFIED | 1,291 lines; `vi_members` (120 rows), `VI_STATE_SIZE 1208u` and `VIDEO_PACKET_INFO_SIZE 488u` static asserts, claimed-byte ownership check (no overrun, no double claim), tallies asserted, every-byte mode on the stem id with inert count asserted equal to 417, representative mode on four more ids, digest tie |
| `scripts/p10-mutants.py` | 26 mutants and 2 controls | VERIFIED | row `v26-digest-omit-vi-phase-cycles` present, anchor unique, anchors check exit 0 |
| `scripts/qualify-phase10.py` | derivation text for 26 mutants and 28 rows, limits unchanged | VERIFIED | empty diff since the new Qualification base; record verifies against it (read, not re-run) |
| `docs/evidence/phase10-qualification.md`, `docs/claims/phase10.md` | name the new record, the VI rows and the VI mutant | VERIFIED | record id and the VI rows named; claims checker 8 blocks 0 errors; no workstation path, host or address (pattern scan 0, PII scan 0) |
| all other artifacts of the first report | as before | VERIFIED | empty production diff since the first report |

## Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `vi_members` rows | walker of `t64_digest_stream` VI section | byte flip then canonical-stream compare | WIRED | scratch omission of any of six members fails the case; unmutated passes |
| mutant row `v26-digest-omit-vi-phase-cycles` | `test_digest_v26:v26-completeness` | `own_check` kill command | WIRED | KILLED in the qualification record (3.3 s) |
| qualification record | qualified tree | `commit_sha` 1fc0b1d, base bound, empty drift | WIRED | read from the record; Qualification base is the single record line of PIN-IMPACT section 5 |
| pushed head | hosted pair | `gh run` for head 85ba477 | WIRED | run 45650890027 success on both jobs |
| all links of the first report | as before | | WIRED | no changed inputs |

## Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
|----------|---------------|--------|--------------------|--------|
| VI completeness rows | VI state of a machine holding a retained video packet | a real scan-out in the core (`build_vi_frame`) so the identity strings and a packet length are non-zero | yes; every byte of the live struct is flipped and compared against the real canonical stream | FLOWING |
| other artifacts | as in the first report | | | FLOWING |

## Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| VI completeness case | `ctest -R '^unit\.core\.digest\.v26'` (verbose) | 2 of 2 passed; 10,132 checks, 0 failures | PASS |
| VI member coverage cross-check | scratch probe over header members vs `vi_members` | 120 = 120, 0 missing, 0 extra, sizes 1,208 and 488, 791/400/17 | PASS |
| Omitted VI member is seen | scratch digest objects (`phase_cycles` plus five more members) vs the committed case | each fails the case; unmutated control passes | PASS (kill reproduced) |
| Mutant driver anchors | `python3 -B scripts/p10-mutants.py --check-anchors` | exit 0; 28 rows listed | PASS |
| Phase 10 and shipped-id names | `ctest -R` 41 names | 41 of 41 passed | PASS |
| Checkers | CI scanner, oracle authority, composite oracle, claims, PII scan, sweep selftest | ok, 0 and 2, equal, 0 errors, 0 findings, PASS | PASS |

### Probe Execution

No `scripts/*/tests/probe-*.sh` probes are declared by this phase. Step 7c: SKIPPED (no probes declared); the equivalent runnable checks are the CTest names above.

## Requirements Coverage

| Requirement | Source Plans | Description | Status | Evidence |
|-------------|--------------|-------------|--------|----------|
| GATE-02 | 10-03, 10-05, 10-14, 10-17 to 10-19 | per-row oracle pins; appended manifest rows need no rebind; `local_decision` blocked | SATISFIED | SC5; oracle authority exits 0 and 2; pin script unchanged |
| GATE-01 | 10-01, 10-04, 10-05, 10-16 to 10-19 | COST, 15 min with scanner rule, timings, measured cold run; guard removal | SATISFIED (guard removal declined as the requirement text allows, recorded) | SC5 |
| VER-06 | 10-01, 10-06 to 10-08, 10-17 to 10-19 | VG-01..05 and NTSC vector with independent values and failing mutants | SATISFIED | SC3 |
| VID-07 | 10-01, 10-02, 10-09, 10-10, 10-13, 10-17 to 10-19 | depth under new ids; shipped ids unchanged; wrong-value control | SATISFIED | SC1 |
| CORE-06 | 10-02, 10-09 to 10-13, 10-15 to 10-21 | one marker; mid-primitive and mid-scan-out round trip; flip-every-member; build-identity docs | SATISFIED, now including the VI half of the ROADMAP wording | SC2 |
| RUN-07 | 10-14, 10-15, 10-17 to 10-19 | composite guest, two supervised runs, equal digest, independent expectations | SATISFIED | SC4 |

All six requirement IDs appear in plan frontmatter and in REQUIREMENTS.md (marked Complete, mapped to Phase 10); none is orphaned and no other requirement maps to Phase 10.

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| files changed in the gap closure (3 sources, 3 documents) | n/a | TBD, FIXME, XXX markers | none found | no debt marker without a reference |
| `core/tests/test_digest_v26.c` | 850 | `VI_IDENTITY_STRINGS_REASON` says the v25 and v26 families bind identity "through the domain and the profile digest"; true for `profile_id`, not for `build_id`, which the v25 state does not carry (R21-01) | Info | wording only; the exclusion itself is correct (the walker hashes both strings only under `!has_v25_framing`, and flipping them is inert on the tested ids); recorded in the review, not a gap |
| `core/tests/test_digest_v26.c` | 921-926 | size pins are the LP64 layout (R21-02) | Info | a different layout fails the build loudly; hosted macOS and Linux compiled and ran the case |
| `docs/` and `.planning/` | n/a | stale mutant-count text (R21-03) | resolved | `10-VALIDATION.md` refreshed to 28 rows and 26 killed in the last commit |
| `core/t64_state.c` `state_z_products_bounded` | ~1490 | boundary constants not pinned (W2-01) | WARNING, deferred | Phase 11 |
| `scripts/list-membership-sites.py` | n/a | two predicate tokens missing (W2-02) | WARNING, deferred | Phase 11 |

### Observation outside the success criteria (advisory, non-blocking)

The VI section of the digest walker also hashes the retained video packet payload (`machine->video_packets`, a separate machine allocation, up to `packet_lengths[slot]` bytes per slot). It is not a member of `t64_vi_state`, so it is outside the row set that the first report, plans 10-20 and 10-21 and the claim define as the VI half, and Phase 10 did not change that code. I tested it because a flip-every-member claim invites the question: a scratch mutant that drops that one `digest_bytes` call survives the completeness case, the two v25 digest names, the strip-differential, digest-trace and the VI and video-output unit names that I relinked against it. I did not run the whole gate against it, so a pinned digest literal elsewhere may still catch it. I do not count this against SC2 (the criterion reads "each RDP and VI member", the RDP analogue for large buffers is covered because Phase 9 and 10 changed them, and the VI payload walker predates both). If the owner wants it pinned, it belongs to the Phase 11 freeze-candidate review; note that it would be a sixth carried item against guardrail G3 unless one of the five closes first.

## Human Verification / Owner Review Items

No item needs a human to run or look at anything: behavior-dependent truths have behavioral tests (round trips, completeness flips with killed mutants), no `<human-check>` blocks exist in any plan, and `behavior_unverified` is 0. The nine recorded exceptions above are owner-review decisions already documented and bounded; they are not approvals and do not change the verdict.

## Gaps Summary

None. The VI half of SC2 is met by a committed, reviewed, qualified and pushed test: a programmatic comparison against the compiler layout shows no VI member is missing, the six named exclusions and 17 padding bytes are exact (417 inert of 1,208), the walker-omission mutant is killed in the record and independently reproduced, and the hosted pair of the pushed head is green. All previously verified criteria still hold with no production change. W2-01, W2-02, DEF-W1-01, W1-03 and D-10-17-01 are real, recorded in ROADMAP Phase 11 carry-ins, and correctly deferred.

---

_Verified: 2026-11-25T17:05:00Z_
_Verifier: Claude (gsd-verifier)_
