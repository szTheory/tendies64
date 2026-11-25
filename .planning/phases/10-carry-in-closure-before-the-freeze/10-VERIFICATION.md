---
phase: 10-carry-in-closure-before-the-freeze
verified: 2026-11-25T14:48:21Z
status: gaps_found
score: 4/5 must-haves verified
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
covered_digest: "v4:sha256:dc3220d1eb6a35d173a5aa4926587f725600291d04611b60c761d2f617766a15"
behavior_unverified: 0
overrides_applied: 0
re_verification: null
gaps:
  - truth: "SC2: a flip-every-member test sees each RDP and VI member (ROADMAP Phase 10 success criterion 2, VI half)"
    status: partial
    reason: "The committed completeness test (unit.core.digest.v26-completeness) flips every member of t64_rdp_state (100 member rows, a sizeof static assert, claimed-byte sweep for padding), the memory coverage table and the hidden depth table. It has no VI rows: no committed test flips the members of the VI state. The 10-13 review recorded this as W2-03 (info) and covered the VI half only by an uncommitted probe; the review's SC2 evidence cell says the VI state 'is covered', which overstates what a committed test proves. W2-03 is not in the ROADMAP Phase 11 carry-ins, in PIN-IMPACT 13.1 or in the evidence document's exception list, so it is carried nowhere, and adding it would be the sixth carried gap against guardrail G3 (limit five). The underlying behavior holds: this verification re-ran the probe (every byte of the VI state flipped on the state-26 stem id and on the state-25 id) and got 417 inert bytes of 1,208 on both, the same figure the review recorded (observer cursors, per-slot identity strings, padding); every other byte changes the canonical stream."
    artifacts:
      - path: "core/tests/test_digest_v26.c"
        issue: "rdp_members table and case_v26_completeness cover t64_rdp_state, rdp_memory_coverage and rdp_depth_hidden only; no VI member rows"
    missing:
      - "Add a VI member row set to a committed completeness test (a new test file or case, since core/tests/test_digest_v26.c is not oracle-bound) with named exclusions for the observer cursors packet_head and packet_count, the per-slot profile_id and build_id strings and padding, a sizeof static assert like RDP_STATE_SIZE, and one mutant row that omits a VI member from the digest walker; or record an owner-accepted override"
      - "Carry the decision in ROADMAP/PIN-IMPACT so W2-03 is not recorded only in 10-REVIEW.md"
deferred:
  - truth: "W2-01: the Z product bound state_z_products_bounded (core/t64_state.c) is pinned by a committed boundary test (importer hostile boundary)"
    addressed_in: "Phase 11"
    evidence: "ROADMAP Phase 11 'Carry-ins from Phase 10': 'W2-01 (the Z product bound of state_z_products_bounded is not pinned by a committed boundary test)'; also PIN-IMPACT.md 13.1 and docs/evidence/phase10-qualification.md exception 8"
  - truth: "W2-02: t64_has_si_v25_profile and t64_has_si_v26_profile are tokens of scripts/list-membership-sites.py (six site rows missing)"
    addressed_in: "Phase 11"
    evidence: "ROADMAP Phase 11 'Carry-ins from Phase 10': 'W2-02 (t64_has_si_v25_profile and t64_has_si_v26_profile are not tokens of scripts/list-membership-sites.py, six site rows missing)'"
  - truth: "DEF-W1-01: three sibling copies of the perspective floor are pinned behaviorally"
    addressed_in: "Phase 11"
    evidence: "ROADMAP Phase 11 'Carry-ins from Phase 10': 'DEF-W1-01 (three unpinned copies of the perspective floor)'"
---

# Phase 10: Carry-in Closure Before the Freeze - Verification Report

**Phase Goal:** Everything that changes state, descriptors or digests lands once, before the API is shaped: the composite profiles render depth under new ids, the state container carries one version marker, the accepted Phase 7 gaps have independent tests, one composite fixture runs end to end, and the gate fits its job limit
**Verified:** 2026-11-25T14:48:21Z
**Status:** gaps_found
**Re-verification:** No, initial verification
**Evidence tier of what was checked:** unit-tested, authored scope. Nothing here is a hardware, oracle or other-emulator claim.

## Summary verdict

The phase goal is achieved in substance and four of five roadmap success criteria are fully verified against code, tests and a read-only look at the hosted run. One literal clause of success criterion 2 is not met by any committed test: there is no flip-every-member test over the VI state (W2-03). It is narrow, cheap to close, and its underlying behavior is true (re-probed here), but it is observable as absent, is not carried into any later phase, and would breach guardrail G3 if simply carried. It is therefore reported as a gap (BLOCKER by the classification rule) with a one-test fix, or an owner override if the probe is accepted as sufficient. Everything else that the dispatch asked to be judged resolves as follows: the GATE-01 guard-removal decline meets the requirement in substance; W2-01 and W2-02 are recorded and legitimately deferred and do not falsify any stated SC2 clause; the exceptions are documented and are owner-review items, not defects.

## How this was verified (not from SUMMARY claims)

- Read the ROADMAP Phase 10 contract, REQUIREMENTS, CONTEXT, the must_haves of the plans, the review, validation, security and pin-impact records, the evidence document and claim file.
- Ran all 30 names of the `phase10` CTest label natively against the existing gate build tree with `ctest -R` in two invocations (24 unit/core names, then the 7 runner, harness and repo names, of which `harness.composite-oracle` was already in the first), all 30 passed. Also re-ran the shipped-id identity and digest tests (`unit.core.profile*`, `unit.core.digest.v25-*`, `unit.core.state.export`, `unit.core.state.import`: 10 of 10), the oracle-authority checker (`--expect-blocked` exit 0, `--require-approved` exit 2, selftest 26 controls), the CI scanner (`check-ci-yml: ok`, selftest 138 cases 0 failures), the digest-sweep selftest, the claim checker (`claims: files=1 blocks=8 errors=0`), and the composite oracle `--check` (expectation equals the oracle, 2,737 bytes). No rebuild, clean, full gate or qualifier was run; the working tree is clean after verification.
- Independently recomputed the depth literals (0x1357 times 8 = 0x9ab8, mantissa 0x26a, visible 0x09a8) and the NTSC clock vector with exact rational arithmetic (812,175 source cycles gives 1,042,709 ticks; 3,094 cycles gives 3,973 ticks; a 48,600,000 Hz clock gives 1,044,464 and 3,979). All equal the test literals.
- Built two scratch mutants and one probe program outside the repository against the existing static library and read the results: decal tolerance `<=` to `<` fails `unit.core.vg.decal-equality` (4 check failures, baseline 0); the importer Z product limit `INT64_MAX / 4` to `INT64_MAX / 2` passes both the `roundtrip` and `hostile` cases of the v26 importer test (survives, which confirms W2-01); a probe flipping every byte of the VI state reproduced the review's 417-inert-byte figure.
- Read the hosted cold run read-only with `gh run view` (run 45550277510) and the private native record `record.json` and step logs under `build/gate/10-qualification-private/` (not re-run).

## Observable Truths (ROADMAP success criteria, the contract)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | An authored depth fixture on the new composite ids produces Z-tested pixels and depth values equal to independent literals, a wrong-depth control fails, and shipped ids keep digests and Z refusal byte-identical across the 17 pre-Phase-9 profiles and the Phase 8/9 composites | VERIFIED | `core/tests/test_rdp_depth_composite.c`: on each of the four descriptor 13 ids, the accepted case checks color 0x07c1, depth 0x09a8, sidecar 0x83, then a farther primitive leaves the pixel and a nearer one replaces it (0x003f / 0x0400); literals are hand-derived in comments and no production z_compress/z_decompress is called (my recomputation matches). The wrong-depth case flips one literal byte at a time and requires the first difference at exactly that byte, plus four one-input-different setups. Predicate (`core/t64_internal.h`): `t64_has_rdp_depth_profile` adds `t64_has_si_v26_profile` outside the descriptor 10 conjunction, so descriptors 11 and 12 keep K1; `unit.core.k1.dispatch-refusal` and `unit.core.k1.apply-refusal` pass natively; `unit.core.profile.v26-predicate-matrix` covers all 25 ids. Shipped digests: `scripts/digest-sweep.py` over the 21 shipped ids (17 earlier plus four si-v1) between the Wave 0 base and the qualified head recorded `sweep: 1599 shared lines, 0 differences, 0 head-only lines` in the native record (build id normalized, disclosed in the script header); the sweep selftest passes here; v25 reference and strip-differential tests pass. Mutants `k1-predicate-widened`, `v26-sidecar-invalidation-removed` KILLED in the record. |
| 2 | A state exported mid-primitive and mid-scan-out with depth active imports and continues to the same frames, digest and outputs; one version marker; a flip-every-member test sees each RDP and VI member; docs state blobs are build-identity-bound and outside the ABI promise | PARTIAL (VI clause FAILED) | Verified parts: `unit.core.state.import-v26-roundtrip` exports an idle state, a Z triangle in flight (all four opcodes' proof path, depth scalars read before the render proof) and a scan-out with a retained video packet and two audio packets on each id, imports into a fresh machine, compares digests, depth/color bytes, the 2,097,152-byte hidden table, video and audio packet bytes and save bytes, and runs on through three further field boundaries with digest checks. One marker: `state_container_version_for` pairs state 26 with container version word 2 in exporter and reader (`core/t64_state.c`), digest domain `t64.state.si.26`; mutants `v26-export-version-word-1` and `v26-reader-pairing-dropped` KILLED. Flip test: `unit.core.digest.v26-completeness` flips the first byte of all rdp member rows, three coverage-table entries, first/middle/last hidden bytes and strided TMEM, with a `RDP_STATE_SIZE` 5144 static assert and a claimed-byte sweep that forces padding and post-count packet words to be inert; mutants `v26-digest-omit-scalar` and `v26-digest-omit-hidden` KILLED. Docs: `docs/spec/state-container.md:17` and `docs/spec/machine-contract.md:9` state blobs are build-identity-bound and outside the ABI promise. NOT met: no committed test flips VI members (see Gaps). |
| 3 | VG-01 to VG-05 and the standard-NTSC VI clock vector each have independently derived expected values and at least one mutant that fails | VERIFIED | `core/tests/test_vg_gaps.c` (nine cases, new file; the oracle-bound `test_rdp_depth.c`, `test_vi.c` and `phase09-cases.json` have an empty diff against the Wave 0 base): VG-01 runs on descriptor 10 (`rdp-depth-v7`), literals hand-derived in the file with same-setup wrong-value controls. 14 VG and clock mutants are KILLED in the private native record, each by its named case (`vg01-decal-strict`, `vg01-zcap-7ffe`, `vg01-delta-floor`, `vg-zexp-*`, `vi-clock-48600000`, `vg03-alpha255-v8/v9`, `vg02-*`, `vg04-floor-off-s/t`, `vg05-*`), with the equivalent control SURVIVED and the non-compiling control INVALID, so the driver cannot pass vacuously. The mutants map one-to-one to the eight-plus survivors that `07-VERIFICATION.md` listed. Independently reproduced here: decal-strict scratch mutant fails the named case. `repo.p10-mutant-anchors` passes in the gate. |
| 4 | One composite guest reports exact raw video and audio from a capture run and exact save bytes from a save-out run with the same final digest, each compared with its own independent expectation and each with a wrong-value control | VERIFIED | `runner.composite-two-run` and `controls.real.composite-wrong-rom` re-run and read verbosely: raw-run and save-run `state_digest` are both `97f27027...1267c`; 3 video frames of 48 bytes in end_tick order, two 16-byte audio packets in sequence order, a 32,768-byte save with sha256 `f0b7fe88...6926d`, and the 188-byte transcript equal `fixtures/composite/composite.expected.json`, which `composite_oracle.py --check` proves equal to a stdlib-only independent oracle (this run). Controls change one expected byte in video (offset 17), audio (packet 2 offset 5), save (offset 12345) and transcript (offset 100) and require the first difference there; the wrong-ROM control fails the save comparator at offset 271 (0x27 vs 0x26). A second run pair in fresh directories is identical; raw capture combined with save-out is still refused (exclusion not relaxed). |
| 5 | A measured cold-cache hosted gate-macos run finishes inside its limit; before/after timings, the 15-minute limit with its scanner rule and the pinned limit relation are recorded; adding a manifest row no longer requires an oracle-request rebind | VERIFIED | `gh run view 45550277510` (read-only): workflow_dispatch on head f081d39, `gate-macos` success in 731 s (limit 900 s), "Cache not found for input keys: binutils-macOS-ARM64-69d0bfd4..." at Restore, fixture-qualification 219 s, ctest 265 s, "100% tests passed out of 254"; f081d39 is an ancestor of the audited tree and no bound source path differs between them (only `.planning/` and the CI log). `.github/workflows/ci.yml`: gate-macos `timeout-minutes: 15`, gate-linux 10; `scripts/check-ci-yml.sh` CI-TIMEOUT rule is per job (macOS 1 to 15, others 1 to 10), selftest 138 cases 0 failures with refusal cases for 16, 11, 0 and 15.5; COST properties 210/71/48/35 present in `core/CMakeLists.txt` and `runner/CMakeLists.txt`; before/after timings and the F-106/F-111/F-114/F-117 limit relations are rows F-111 to F-125 of `docs/ci-reviews/LOG.md`. GATE-02: `scripts/check-rdp-oracle-authority.py` binds 39 manifest rows by id then per-row canonical hash; `PINNED_REQUEST_SHA256` is unchanged since commit b64098f, the manifest gained two rows afterwards (41 entries, commit f027b05) with the checker green, and the checker's selftest has 26 controls. |

**Score:** 4/5 truths verified (SC2 partial: every clause except the VI member flip is verified). 0 truths are present-but-behavior-unverified.

### Deferred Items

Not actionable in this phase: explicitly scheduled in the Phase 11 "Carry-ins from Phase 10" text of ROADMAP.md.

| # | Item | Addressed In | Evidence |
|---|------|--------------|----------|
| 1 | W2-01 importer Z product bound has no committed boundary pin | Phase 11 | ROADMAP Phase 11 carry-ins; PIN-IMPACT.md 13.1 |
| 2 | W2-02 two exact-tuple predicates missing from the membership-site generator | Phase 11 | ROADMAP Phase 11 carry-ins |
| 3 | DEF-W1-01 three unpinned copies of the perspective floor | Phase 11 | ROADMAP Phase 11 carry-ins |

### Judgment on W2-01 and W2-02 (asked explicitly)

- **Recorded:** yes, in `10-REVIEW.md` (Wave 2), `PIN-IMPACT.md` section 13.1, the ROADMAP Phase 11 carry-ins line and exception 8 of `docs/evidence/phase10-qualification.md`.
- **W2-01 (importer boundary whose mutants survive):** confirmed independently. Changing the limit to `INT64_MAX / 2` in a scratch copy passes both `roundtrip` and `hostile`. It does **not** falsify any clause of SC2 (the round trip, the one marker, the flip test over RDP members, the docs) and it is not named by CORE-06 or by claim block 2, whose wording does not claim boundary exactness. It does weaken the assurance of the hostile-import path in code the phase newly authored, and it is a test-strength gap in the very phase that closes test-strength debt, so it is a WARNING. The code equals the live preflight today per the recorded 6,000,000-packet differential, which I did not re-run. Deferral to Phase 11 has clear roadmap evidence, so it stays deferred.
- **W2-02:** confirmed (the two tokens are absent from `MEMBERSHIP_TOKENS`; the committed list names the predicates twice). It weakens a discovery aid, not v26 behavior; independent guards (exact-tuple registry, domain check, 25-id predicate matrix) exist. It does not touch SC2. WARNING, deferred.
- Neither is the gap reported above. The gap is W2-03, which the review downgraded and which nothing carries.

## GATE-01 guard-removal decline: judged in substance

The requirement text and CONTEXT both allow the decline: CONTEXT "Decided autonomously" says to decline with a recorded reason if removal needs a qualifier edit, plan 10-04's own truth records the decline in advance, and REQUIREMENTS.md carries the annotation. The reason is true in the code: `docs/spec/audio-cases.json` lists `repo.rsp-no-hle` in the Phase 8 qualification suite (line 182) and `scripts/qualify-audio.py` pins that file by `EXPECTED_INVENTORY_SHA256` (line 26), the closed Phase 8 qualifier (F-106); `git log` shows neither file touched since the Wave 0 base. Every other element of the GATE-01 requirement is met and measured: COST ordering, `timeout-minutes` 15 with the scanner rule and selftest changed together in one commit (4ca1054; ci.yml, check-ci-yml.sh changed once and gate.sh not at all), before/after timings, and the outer limits of F-106, F-111 and F-114 recorded (F-117, F-111, F-114 rows). The measured cold run is 731 s of 900 s (margin 169 s), one sample, with a recorded run-to-run spread of up to 52 s; the cold run needed the 15-minute limit (the old 600 s would have been exceeded by 131 s). Roadmap SC5 does not mention the guard at all. Verdict: GATE-01 met in substance; the forgone saving (about 41 s hosted) is honestly recorded and owned by the next Phase 8 requalification.

## Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `core/t64_machine.c` | four descriptor 13 / model 21 / state 26 ids and save variants | VERIFIED | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2` plus `-e4k`, `-e16k`, `-s32k` registered with tuple 13/21/26; unregistered id with that tuple refused at create |
| `core/t64_internal.h` | exact-tuple predicates and depth predicate | VERIFIED | `t64_has_si_v26_profile`, widened `t64_has_si_profile`, depth term outside the descriptor 10 conjunction |
| `core/t64_digest.c`, `core/t64_state.c` | v26 domain, depth scalars, hidden table, container v2 pairing, importer rules | VERIFIED | domain `t64.state.si.26`, `state_container_version_for`, five scalars read before the render proof, `state_z_products_bounded` (W2-01 caveat) |
| `core/tests/test_rdp_depth_composite.c`, `test_vg_gaps.c`, `test_k1_refusal.c`, `test_digest_v26.c`, `test_state_export_v26.c`, `test_state_import_v26.c`, `test_profile_v26.c` | independent-literal tests, new files only | VERIFIED (VI rows missing from `test_digest_v26.c`) | substantive (500 to 1,460 lines each), wired into 30 `phase10` CTest names, all passing natively |
| `fixtures/composite/*`, `runner/tests/test_composite_two_run.c` | authored guest, independent oracle, two-run test | VERIFIED | oracle `--check` equals committed expectation; test passes with controls |
| `fixtures/manifest.json` | exactly two appended composite rows | VERIFIED | 41 entries (39 plus 2) in one commit f027b05 with the oracle authority checker untouched |
| `scripts/check-rdp-oracle-authority.py` | per-row binding, rebind identity-only | VERIFIED | modified only by orchestrator commit b64098f; `local_decision` blocked; trusted pins None |
| `scripts/check-ci-yml.sh`, `.github/workflows/ci.yml` | 15-minute macOS limit with scanner rule | VERIFIED | see SC5 |
| `scripts/p10-mutants.py`, `scripts/digest-sweep.py`, `scripts/qualify-phase10.py` | out-of-tree mutant driver, two-tree sweep, thin qualifier | VERIFIED | selftests and anchors pass; qualifier run once and recorded (not re-run by design) |
| `docs/evidence/phase10-qualification.md`, `docs/claims/phase10.md` | bounded evidence and eight claim blocks | VERIFIED | `check-claims.sh` 8 blocks 0 errors; Phase 9 claim and evidence files byte-identical to the Wave 0 base; no workstation path, host or address found by a pattern scan |

## Key Link Verification

| From | To | Via | Status | Details |
|------|----|----|--------|---------|
| `t64_has_rdp_depth_profile` | descriptor 11/12 ids | K1 refusal | WIRED | v26 term sits outside the descriptor 10 conjunction; K1 tests and the 25-id matrix pass |
| exporter `write_header` | importer header check | `state_container_version_for` | WIRED | one function gives 2 for state 26, 1 otherwise; both sides use it |
| composite ROM rows | runner admission and manifest | `runner/t64_real_fixture.c` and `fixtures/manifest.json` | WIRED | two-run test admits `phase10-composite-s32k` and the wrong ROM |
| manifest rows | oracle request | per-row `row_sha256` lookup by id | WIRED | appended rows need no rebind (verified against history, not asserted) |
| `p10-mutants.py` rows | named failing cases | `OWN_CHECK_LINE` / comparator labels | WIRED | each KILLED row names test and case in the record; `repo.p10-mutant-anchors` gates the anchors |
| `ctest` COST properties | hosted start order | `set_tests_properties` | WIRED | present for the four named tests |

## Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
|----------|---------------|--------|--------------------|--------|
| depth tests | color/depth/sidecar bytes | RDP commands through DP_START/DP_END into core RDRAM | yes (the core renders; expectation is a hand literal, separate) | FLOWING |
| composite test | video/audio/save/transcript | supervised real runner on an authored ROM | yes; expectation from the stdlib oracle | FLOWING |
| round-trip test | peer machine state | blob exported from the source machine | yes, with the stated limit that a round trip compares a machine with its own export; layout pinned separately by export-side tests and the independent v2 decoder | FLOWING |

## Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Phase 10 unit/core names (24) | `ctest -R` over the `phase10` label minus runner/harness | 24 of 24 passed | PASS |
| Runner and harness names (7) | `ctest -R` exact names | 7 of 7 passed (two-run 0.55 s with controls) | PASS |
| Shipped-id identity, digest, export, import | `ctest -R` ten names | 10 of 10 passed | PASS |
| Oracle authority | `--expect-blocked`, `--require-approved`, `--selftest` | 0, 2, 26 controls pass | PASS |
| CI scanner | `check-ci-yml.sh`, `--selftest` | ok; 138 cases 0 failures | PASS |
| Composite expectation vs oracle | `composite_oracle.py --check` | equal (2,737 bytes) | PASS |
| Scratch mutant decal-strict | link mutated object against built library | named case fails (4 checks), baseline 0 | PASS (kill reproduced) |
| Scratch mutant Z product limit | same | roundtrip and hostile pass | SURVIVES (W2-01 confirmed) |
| VI state byte-flip probe | scratch program on stem id and si-v1 | 417 inert of 1,208 on both | matches review |

### Probe Execution

No `scripts/*/tests/probe-*.sh` probes are declared by this phase. Step 7c: SKIPPED (no probes declared); the equivalent runnable checks are the CTest names above.

## Requirements Coverage

| Requirement | Source Plans | Description | Status | Evidence |
|-------------|--------------|-------------|--------|----------|
| GATE-02 | 10-03, 10-05, 10-14, 10-17 to 10-19 | per-row oracle pins; adding a manifest row needs no rebind; local_decision blocked | SATISFIED | SC5 evidence; 41 rows with unchanged pin; `--require-approved` exit 2 |
| GATE-01 | 10-01, 10-04, 10-05, 10-16 to 10-19 | COST, 15 min with scanner rule, timings, measured cold run; guard removal | SATISFIED (guard removal declined, allowed by the requirement text and recorded) | SC5 evidence; F-115 to F-117, F-120 to F-125 |
| VER-06 | 10-01, 10-06 to 10-08, 10-17 to 10-19 | VG-01..05 and NTSC vector with independent values and failing mutants | SATISFIED | SC3 evidence |
| VID-07 | 10-01, 10-02, 10-09, 10-10, 10-13, 10-17 to 10-19 | depth under new ids; shipped ids unchanged; wrong-value control | SATISFIED | SC1 evidence |
| CORE-06 | 10-02, 10-09 to 10-13, 10-15 to 10-19 | one version marker; mid-primitive and mid-scan-out round trip; flip-every-member; build-identity docs | SATISFIED as worded in REQUIREMENTS (members of the RDP block, coverage table and depth members); the stricter ROADMAP SC2 wording adds VI, see gap | SC2 evidence |
| RUN-07 | 10-14, 10-15, 10-17 to 10-19 | composite guest, two supervised runs, equal digest, independent expectations, exclusion not relaxed | SATISFIED | SC4 evidence |

All six requirement IDs appear in plan frontmatter and in REQUIREMENTS.md as Phase 10; none is orphaned, and no other requirement maps to Phase 10.

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| files changed since the Wave 0 base (59 files) | n/a | TBD, FIXME, XXX markers | none found | The only TODO/placeholder hit is prose in `docs/spec/state-container.md` forbidding placeholders |
| `docs/evidence/phase10-qualification.md` exception 8 | n/a | states W1-03 and D-10-17-01 are "listed in ROADMAP.md for the Phase 11 freeze-candidate review" | WARNING (documentation accuracy) | The ROADMAP Phase 11 carry-ins line names W2-01, W2-02, DEF-W1-01 and the Phase 9 items but not W1-03 or D-10-17-01; those two live in PIN-IMPACT and `deferred-items.md` only |
| `10-REVIEW.md` SC2 row | 820 | says "the VI state and both tables are covered (W2-03)" | WARNING | W2-03 itself says the test has no VI rows; the coverage was a probe |
| `core/t64_state.c` `state_z_products_bounded` | ~1490 | boundary constants not pinned (W2-01) | WARNING | deferred, mutant survival reproduced |
| `runner/tests/test_cold_boot.c` (Phase 6) | 124 to 139 | 99-attempt cap on evidence directories in a long-lived build tree (D-10-17-01) | WARNING (hygiene, not caused by Phase 10) | recorded in `deferred-items.md`; counters at 9 and 9 per the evidence record |

## Human Verification / Owner Review Items

No item needs a human to run something, but these recorded exceptions are explicitly for owner review and none is an owner approval. They are listed in `docs/evidence/phase10-qualification.md` "Recorded exceptions for owner review" and I confirmed each is documented and bounded:

1. **T-10-49 comparator change.** `claims_final_problems` in `scripts/tests/test_phase09_qualification.py` now binds each closed Phase 9 claim block to the pinned Phase 9 manifest identity plus a per-row view of its 39 rows instead of the live whole-file hash. `harness.phase09-qualification` passes here; `docs/claims/phase09.md` and `docs/evidence/phase09-qualification.md` are byte-identical to the Wave 0 base. Reversible test code; it is a closed-phase test edit and should be reviewed by the owner.
2. **G4 gate growth** (224 to 254 names, no offset claimed; reversal rule not triggered by the 731 s measurement).
3. **G5 plan count** (19 plans against about 10 to 15, review plans mandated by the autonomy policy).
4. **Two wave 3 deviations** (one literal of `runner/tests/test_state_runner.c` moved to the v3 format string; the wrong-ROM control compares its save through the 16-byte record read back from the guest because a failing run publishes no save).
5. **GATE-01 guard decline** (above).
6. **One orchestrator rebind of `PINNED_REQUEST_SHA256`** (identity only, commit b64098f, grants no oracle, rights or publication authority).
7. **Native qualification retries.** Attempts 1 to 3 failed in the gate stage under host load (eight supervised-run tests hit their wall backstops, none a Phase 10 name), attempt 4 passed with no limit widened and no source change; the record is `rehearsal: false`, `status: complete`, 8 stages with every return code as expected, `claims.hardware_accuracy` and `claims.oracle_comparison` false. I read the record and the mutant and sweep logs and did not re-run the qualifier (its `--verify` refuses later heads by design). The load cause is an inference the evidence itself labels as such. Worth the owner's attention: a slow local host can make closed Phase 8 and 9 supervised-run tests fail the local qualification; the hosted runs of the same tree passed five times.

## Gaps Summary

One gap, narrow and cheap: **SC2's VI half.** Close it by adding VI member rows (with named exclusions for the 417 inert bytes) to a committed completeness test plus a mutant that drops a VI member from the digest walker, then re-run the qualification record, or have the owner accept the probe as sufficient through an override entry. Either way, record W2-03 in the ROADMAP/PIN-IMPACT carry-in list so it is not held only in the review file. Suggested override text if the owner accepts it:

```yaml
overrides:
  - must_have: "SC2: a flip-every-member test sees each RDP and VI member"
    reason: "VI walker unchanged by Phase 10; v26 equals v25 outside the new span (strip differential); VI member coverage shown by a probe (417 inert bytes: observer cursors, identity strings, padding), not a committed test"
    accepted_by: "owner"
    accepted_at: "ISO timestamp"
```

All other must-haves hold. W2-01, W2-02 and DEF-W1-01 are real, reproduced and correctly deferred to Phase 11. The recorded exceptions are documented and wait for owner review.

---

_Verified: 2026-11-25T14:48:21Z_
_Verifier: Claude (gsd-verifier)_
