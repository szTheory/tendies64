---
phase: "10"
slug: carry-in-closure-before-the-freeze
status: validated
nyquist_compliant: true
wave_0_complete: true
created: "2026-11-23"
---

# Phase 10 — Validation Strategy

> Per-phase validation contract. Source: 10-RESEARCH.md "Validation Architecture" (mutant lists and the pin-impact map). Audited 2026-11-25 against the tree at the head after the final native qualification: every automated check below that can be executed without a rebuild, a clean or a full gate was executed in this audit and is marked `re-run`; checks whose subject is a hosted run, a scratch-clone gate, a push or the private native qualification record are marked `recorded` and name the record that holds the result. `nyquist_compliant: true` rests on two warning-level partial gaps (W2-01, W2-02) that are documented as accepted carry-ins; see "Accepted partial gaps". Evidence tier of every row: unit-tested, authored scope. No hardware, oracle, ABI or owner approval is implied.

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest over C test binaries (`tests/t64_test.h`) plus Python harness tests (`scripts/tests/`); out-of-tree mutant driver `scripts/p10-mutants.py`; two-tree digest sweep `scripts/digest-sweep.py` |
| **Config file** | `CMakePresets.json` (gate: 254 names; asan: 256 names), `core/CMakeLists.txt`, `runner/CMakeLists.txt` |
| **Quick run command** | `ctest --test-dir build/gate -R '<task regex>' --output-on-failure` after `cmake --build --preset gate` |
| **Phase selection** | `ctest --test-dir build/gate -L phase10 --output-on-failure` (30 names, equal to the `PHASE10_TESTS` block of `scripts/check-test-inventory.sh`) |
| **Full suite command** | `ctest --preset gate` and `sh scripts/gate.sh` (not run by this audit) |
| **Measured runtime** | phase10 label 25.6 s native (30 names); same selection under ASan and UBSan 158 s wall at `-j2`; `unit.core.state.import` 53 s; mutant driver about 100 s; digest sweep about 44 s |

## Sampling Rate

- **After every task commit:** the task's named CTest regex plus `ctest -R '^harness\.rdp-(oracle-authority|vi-qualification)$'` and `python3 -B scripts/check-rsp-no-hle.py --build build/gate`
- **After every plan wave:** `ctest --preset gate`, `sh scripts/preflight-phase10.sh` (about 40 to 82 s), act Linux compile for any C change, post-Gate CI step replay, TIMEOUT audit, `ctest -N -R` regex count check (46 rows of `scripts/preflight-phase10-regexes.txt`)
- **Before verify-work:** full gate green, ASan selection of the new names, hosted pair green on the final head, one forced cold macOS run
- **Max feedback latency:** 90 s (task level); the longest single focused check is `unit.core.state.import` at 53 s native

## Requirement-Level Map (audit result)

Classification is by confirming that each owning check exists and passes; a hosted, qualification-record or orchestrator-only subject is classed COVERED only when the record was read back and agrees.

| Requirement | Class | Owning automated checks (all exist; result of this audit) | Residual |
|-------------|-------|-----------------------------------------------------------|----------|
| GATE-02 | COVERED | `scripts/tests/test_rdp_oracle_authority.py` (9 per-row binding cases: append-unrelated-row, old-whole-file-mutation, bound-row-changed, existing-row-changed, reorder-bound-rows, duplicate-bound-id, same-bytes-other-id, empty, require-approved; plus 26 selftest controls), CTest `harness.rdp-oracle-authority`, `harness.rdp-vi-qualification`, `harness.phase09-qualification` (re-run, pass); `check-rdp-oracle-authority.py --expect-blocked` exit 0 and `--require-approved` exit 2 (re-run). Real-row proof: the manifest moved in exactly one commit after the Wave 0 base (`f027b05`, plan 10-14) and `PINNED_REQUEST_SHA256` (`e131cbe7...`) is unchanged since the orchestrator rebind `b64098f`; `git log` of the checker shows no later commit | none; the rebind is a recorded orchestrator exception, not an executor edit |
| GATE-01 | COVERED (one declined sub-item) | CTest `repo.ci-yml` and `repo.ci-yml-selftest` (138 cases, 0 failures; the real workflow with `gate-macos` 15 and `gate-linux` 10 passes the scanner, and the CI-TIMEOUT cases refuse macOS 16, Linux 11, 0 and 15.5), COST read-back from `ctest --show-only=json-v1` (210, 71, 48, 35 on the gate tree; 4 COST rows on the asan tree), 46 count rows of `scripts/preflight-phase10-regexes.txt` (0 mismatches), `.github/workflows/ci.yml` (`gate-macos` 15, `gate-linux` 10), `docs/ci-reviews/LOG.md` F-115 to F-125. Measured cold run: hosted run 45550277510 read back with `gh run view` (conclusion success, event workflow_dispatch, Binutils restore log line "Cache not found", `gate-macos` 04:57:52 to 05:10:03 = 731 s, Gate step 653 s, cache saved after the job); 731 is at most 900. Warm pair 45548987145 success on both jobs | the redundant `rsp-no-hle` guard removal is declined (F-116, REQUIREMENTS annotation): see "Accepted partial gaps" |
| VER-06 | COVERED | 9 CTest names `unit.core.vg.*` (decal-equality, primitive-z-saturation, delta-ceiling, z-exponent-boundaries, ntsc-clock, rgba5551-alpha-clear, serrate-filter-parity, perspective-floor, shade-floor-bayer; re-run, pass natively and under ASan); 14 VG and clock mutants KILLED by their named case in a fresh run of `scripts/p10-mutants.py --run`; the two controls behave (equivalent edit SURVIVED, non-compiling edit INVALID). Independent recompute of the NTSC vector with exact rational arithmetic (1,042,709 ticks per first field, 3,973 to the first line sample, 1,044,464 for the wrong clock) equals the literals in `core/tests/test_vg_gaps.c` | DEF-W1-01 (information): three sibling copies of the perspective floor are not pinned behaviorally; not part of the named gaps |
| VID-07 | COVERED (one warning) | `unit.core.rdp.depth-composite-accepted`, `-wrong-depth`, `-sidecar`; `unit.core.k1.dispatch-refusal` and `unit.core.k1.apply-refusal` (shipped ids keep refusing Z); `unit.core.profile.v26-identity`, `-isviewer`, `-predicate-matrix` (25 ids: depth active on exactly seven); `unit.core.rdp.depth` (Phase 7 depth test unchanged); `repo.si-membership-sites`; digest sweep re-run: 1599 shared lines, 0 differences, 0 head-only lines (21 shipped ids byte identical); mutants `k1-predicate-widened`, `v26-sidecar-invalidation-removed`, `v26-digest-omit-scalar`, `v26-digest-omit-hidden` KILLED | W2-02 (warning): see "Accepted partial gaps" |
| CORE-06 | COVERED (one warning on a sub-behavior) | `unit.core.digest.v26-completeness` and `-strip-differential` (RED commit `42f64db` precedes the walker change `831c947`), `unit.core.state.export-v26` and `-bounds`, `unit.core.state.import-v26-roundtrip` (mid Z-triangle and mid scan-out, RED `5e4aa1e` then GREEN `1083257`), `unit.core.state.import-v26-hostile` (16 rows plus fuzz windows), `runner.state-format-v26`, `harness.state-field-use-audit`, `unit.core.state.import` (53 s, pass), `unit.core.state.export`, `runner.state-export`; docs: `build-identity-bound` appears in `docs/spec/state-container.md` (2) and `docs/spec/machine-contract.md` (1); 9 v26 digest, export and import mutants KILLED (digest-omit-scalar, digest-omit-hidden, export-omit-hidden, export-version-word-1, reader-pairing-dropped, import-skip-hidden, import-depth-after-proof, import-addr-unchecked, import-z-products-unchecked) | W2-01 (warning), independently re-confirmed in this audit: see "Accepted partial gaps" |
| RUN-07 | COVERED | `runner.composite-two-run` (the two supervised runs print `state_digest raw-run=` and `save-run=` with the same 64-hex digest; the raw-capture plus save-out combination is refused at tick 0 as harness-error/usage, so the exclusion is unrelaxed), `controls.real.composite-wrong-rom`, `runner.composite-state-v2`, `harness.composite-oracle`, `python3 -B fixtures/composite/composite_oracle.py --check fixtures/composite/composite.expected.json` (expectation equals the twin oracle's output, 2737 bytes) | W3-02 (information, accepted): the wrong-ROM save comparison uses the guest's read-back record because a failing run publishes no save |

## Per-Task Verification Map

Status key: `re-run` = executed in this audit and green; `recorded` = green per the cited record, not re-executed here (hosted run, scratch-clone gate, push, or the private native record). Plan frontmatter `wave` equals the plan number (parallelization false); the Wave column is the phase wave of PIN-IMPACT.md (0: plans 01-05, 1: 06-08, 2: 09-13, 3: 14-18, 4: 19). Alternations in regexes are written in "Re-run commands" below; cells list exact CTest names.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior / Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|----------------------------|-----------|-------------------|-------------|--------|
| 10-01-T1 | 01 | 0 | GATE-01 (hygiene) | T-10-01 | Preflight tracer: oracle check first, one `ctest -N -R` count, Wave 0 base is a 40-hex ancestor | script | `sh scripts/preflight-phase10.sh` | ✅ | ✅ recorded (PASS 82 s in 10-18, 40 s in 10-19); base-ancestor check and oracle check re-run |
| 10-01-T2 | 01 | 0 | GATE-01 (hygiene) | T-10-01 | Full Phase 9 lesson list (act compile, post-Gate replay, TIMEOUT audit, PII) | script | `sh scripts/preflight-phase10.sh`; each row of `scripts/preflight-phase10-regexes.txt` re-counted with `ctest --test-dir build/gate -N -R` | ✅ | ✅ re-run (46 of 46 counts match) and recorded (script) |
| 10-01-T3 | 01 | 0 | VID-07 (id record) | T-10-01 | Pin-impact map, one-way id record, no PII | grep and scan | `grep -c "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2" .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` (9); `sh scripts/pii-scan.sh --tree` (findings 0) | ✅ | ✅ re-run |
| 10-01-T4 | 01 | 0 | VER-06, VID-07 | T-10-02 | Mutant driver: anchors unique, dirty tree refused, equivalent control SURVIVES, non-compiling control INVALID | mutant driver | `python3 -B scripts/p10-mutants.py --check-anchors`; `python3 -B scripts/p10-mutants.py --run`; `ctest -R '^repo\.p10-mutant-anchors$'`; `sh scripts/check-test-inventory.sh --preset gate` (count 254) | ✅ | ✅ re-run (27 rows as expected) |
| 10-02-T1 | 02 | 0 | VID-07 | T-10-19 | K1 tracer: SetZImage refused at dispatch on a shipped composite id | unit | `ctest -R '^unit\.core\.k1\.dispatch-refusal$'` | ✅ | ✅ re-run |
| 10-02-T2 | 02 | 0 | VID-07 | T-10-19 | K1 over five shipped ids and both refusal paths; predicate-widen mutant killed | unit and mutant | `ctest -R '^unit\.core\.k1\.'` (2 names); `python3 -B scripts/p10-mutants.py --run k1-predicate-widened --no-controls` | ✅ | ✅ re-run (KILLED by `apply-refusal`) |
| 10-02-T3 | 02 | 0 | VID-07, CORE-06 | T-10-03 | Two-tree digest sweep, one forced build identity, symbolic base refused | tool | `python3 -B scripts/digest-sweep.py --base <Wave 0 base> --head HEAD`; `python3 -B scripts/digest-sweep.py --base HEAD --head HEAD` (exit 2) | ✅ | ✅ re-run (1599 shared, 0 differences; exit 2 as required) |
| 10-02-T4 | 02 | 0 | VID-07, CORE-06 | T-10-04 | The sweep can fail: a seeded edit is detected | tool selftest | `python3 -B scripts/digest-sweep.py --selftest` | ✅ | ✅ re-run (64 seeded differences detected, PASS) |
| 10-03-T1 | 03 | 0 | GATE-02 | T-10-06 | Nine per-row binding cases (RED against the whole-file checker at `ff341a0`) | harness (python) | `python3 -B scripts/tests/test_rdp_oracle_authority.py` | ✅ | ✅ re-run (9 cases pass) |
| 10-03-T2 | 03 | 0 | GATE-02 | T-10-05, T-10-07 | Orchestrator-only per-row checker edit and identity-only rebind; `local_decision` stays blocked | harness (python) | `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` (exit 0); `python3 -B scripts/check-rdp-oracle-authority.py --require-approved` (exit 2); `git log --oneline -- scripts/check-rdp-oracle-authority.py` (only `b64098f` in Phase 10) | ✅ | ✅ re-run |
| 10-03-T3 | 03 | 0 | GATE-02 | T-10-06 | GREEN: both oracle harness tests pass with 13 bound files | harness (python) | `ctest -R '^harness\.rdp-oracle-authority$'`; `ctest -R '^harness\.rdp-vi-qualification$'` | ✅ | ✅ re-run |
| 10-03-T4 | 03 | 0 | GATE-02 | T-10-49 | Phase 9 claim blocks bind the pinned Phase 9 manifest identity plus a 39-row view; F2 pins untouched | harness (python) | `ctest -R '^harness\.phase09-qualification$'`; F2_PINS of `scripts/tests/test_phase09_qualification.py` equal to the Wave 0 base (ast comparison from the plan) | ✅ | ✅ re-run |
| 10-04-T1 | 04 | 0 | GATE-01 | T-10-09 | COST 210, 71, 48, 35 registered and read back on gate and asan | script | `ctest --test-dir build/gate --show-only=json-v1` with the plan's COST check (prints `COST ok`); `ctest --test-dir build/asan --show-only=json-v1` COST count 4 | ✅ | ✅ re-run |
| 10-04-T2 | 04 | 0 | GATE-01 | T-10-10 | Duplicated guard measured, removal declined; closed Phase 8 qualifier untouched | grep and diff | `grep -c "repo.rsp-no-hle" docs/spec/audio-cases.json` (1); `git diff --quiet <Wave 0 base> HEAD -- scripts/qualify-audio.py docs/spec/audio-cases.json scripts/check-audio-cases.py` (exit 0) | ✅ | ✅ re-run |
| 10-04-T3 | 04 | 0 | GATE-01 | T-10-08 | `gate-macos` 15, other gate jobs 10; CI-TIMEOUT rule and selftest changed together | scanner selftest | `sh scripts/check-ci-yml.sh && sh scripts/check-ci-yml.sh --selftest` (138 cases, 0 failures); CTest `repo.ci-yml`, `repo.ci-yml-selftest` | ✅ | ✅ re-run |
| 10-04-T4 | 04 | 0 | GATE-01 | T-10-08 | Finding rows F-115 to F-119 and the REQUIREMENTS annotation | grep | `grep -c "^| F-11[5-9] " docs/ci-reviews/LOG.md` (5); `grep -c "Recorded 10-04: the rsp-no-hle guard removal is declined" .planning/REQUIREMENTS.md` (1) | ✅ | ✅ re-run |
| 10-05-T1 | 05 | 0 | GATE-01, GATE-02 | T-10-12 | Scratch-clone gate of the Wave 0 head outside the shared tree | review (gate) | `cd build/10-review/w0/src && sh scripts/gate.sh` | n/a (scratch) | ✅ recorded (10-REVIEW.md Wave 0 Task 1) |
| 10-05-T2 | 05 | 0 | GATE-01, GATE-02 | T-10-11 | Mechanical checks: sweep, oracle check, Phase 9 records unchanged | tool and harness | `python3 -B scripts/digest-sweep.py --base <Wave 0 base> --head HEAD`; `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked`; `git diff --quiet <Wave 0 base> HEAD -- docs/claims/phase09.md docs/evidence/phase09-qualification.md`; `ctest -R '^harness\.phase09-qualification$'` | ✅ | ✅ re-run |
| 10-05-T3 | 05 | 0 | GATE-01 | T-10-11 | Push, hosted pair, warm after-timings | hosted | `gh run list --workflow ci.yml --branch main`; `grep -c "^| F-120 " docs/ci-reviews/LOG.md` (1) | ✅ | ✅ recorded (F-120, run 45311817943); F-120 row re-run |
| 10-06-T1 | 06 | 1 | VER-06 | T-10-13, T-10-14 | VG-01 decal equality, KILLED by the decal-strict mutant | unit and mutant | `ctest -R '^unit\.core\.vg\.decal-equality$'`; `python3 -B scripts/p10-mutants.py --run vg01-decal-strict --no-controls` | ✅ | ✅ re-run |
| 10-06-T2 | 06 | 1 | VER-06 | T-10-13 | Primitive-Z saturation and delta ceiling vectors | unit and mutant | `ctest -R '^unit\.core\.vg\.primitive-z-saturation$'`; `ctest -R '^unit\.core\.vg\.delta-ceiling$'`; mutants `vg01-zcap-7ffe`, `vg01-delta-floor` | ✅ | ✅ re-run |
| 10-06-T3 | 06 | 1 | VER-06 | T-10-13 | Z exponent boundary table (17 edges) and the far word | unit and mutant | `ctest -R '^unit\.core\.vg\.z-exponent-boundaries$'`; mutants `vg-zexp-decode-top`, `vg-zexp-compress-round` | ✅ | ✅ re-run |
| 10-06-T4 | 06 | 1 | VER-06 | T-10-13 | Standard-NTSC VI clock vector, literals recomputed independently in this audit | unit and mutant | `ctest -R '^unit\.core\.vg\.ntsc-clock$'`; mutant `vi-clock-48600000` | ✅ | ✅ re-run |
| 10-07-T1 | 07 | 1 | VER-06 | T-10-15 | VG-03 alpha clear through the v8 and v9 paths | unit and mutant | `ctest -R '^unit\.core\.vg\.rgba5551-alpha-clear$'`; mutants `vg03-alpha255-v9`, `vg03-alpha255-v8` | ✅ | ✅ re-run |
| 10-07-T2 | 07 | 1 | VER-06 | T-10-15 | VG-02 serrate filter parity with uniform correct rows | unit and mutant | `ctest -R '^unit\.core\.vg\.serrate-filter-parity$'`; mutants `vg02-parity-ignored`, `vg02-next-row` | ✅ | ✅ re-run |
| 10-07-T3 | 07 | 1 | VER-06 | T-10-15 | VG-04 perspective floor on the S lane and the T lane | unit and mutant | `ctest -R '^unit\.core\.vg\.perspective-floor$'`; mutants `vg04-floor-off-s`, `vg04-floor-off-t` | ✅ | ✅ re-run |
| 10-07-T4 | 07 | 1 | VER-06 | T-10-16 | VG-05 shade floor and Bayer table; full Wave 1 mutant run | unit and mutant | `ctest -R '^unit\.core\.vg\.shade-floor-bayer$'`; mutants `vg05-shade-trunc`, `vg05-bayer-shift2`; `python3 -B scripts/p10-mutants.py --run` | ✅ | ✅ re-run |
| 10-08-T1 | 08 | 1 | VER-06 | T-10-17 | Scratch-clone gate of the Wave 1 head | review (gate) | `cd build/10-review/w1/src && sh scripts/gate.sh` | n/a (scratch) | ✅ recorded (10-REVIEW.md Wave 1 Task 1) |
| 10-08-T2 | 08 | 1 | VER-06 | T-10-17 | Independent re-derivation, no production change, mutant table | tool | `python3 -B scripts/p10-mutants.py --run`; `python3 -B scripts/digest-sweep.py --base <Wave 0 base> --head HEAD` | ✅ | ✅ re-run (the re-derivation itself is recorded in 10-REVIEW.md; NTSC vector recomputed here) |
| 10-08-T3 | 08 | 1 | VER-06 | T-10-17 | Push and hosted pair | hosted | `gh run list --workflow ci.yml --branch main` | ✅ | ✅ recorded (F-121, run 45321391540) |
| 10-09-T1 | 09 | 2 | VID-07, CORE-06 | T-10-18, T-10-51 | Descriptor 13 family registered with depth still inactive; ISViewer only on the SI family; sealed bodies unchanged | unit | `ctest -R '^unit\.core\.profile\.v26-identity$'`; `ctest -R '^unit\.core\.profile\.v26-isviewer$'`; `ctest -R '^unit\.core\.k1\.'`; `ctest -R '^repo\.si-membership-sites$'`; `ctest -R '^harness\.phase09-qualification$'`; `python3 -B scripts/check-rsp-no-hle.py --build build/gate` | ✅ | ✅ re-run |
| 10-09-T2 | 09 | 2 | CORE-06 | T-10-20 | RED first: v26 completeness flip and strip differential | unit | `ctest -R '^unit\.core\.digest\.v26-completeness$'`; `ctest -R '^unit\.core\.digest\.v26-strip-differential$'` | ✅ | ✅ re-run (RED commit `42f64db` precedes `831c947`) |
| 10-09-T3 | 09 | 2 | VID-07, CORE-06 | T-10-19, T-10-20 | GREEN: depth active on the family and streamed under `t64.state.si.26`; shipped ids byte identical | unit, sweep, mutant | `ctest -R '^unit\.core\.digest\.'` (6 names); `ctest -R '^unit\.core\.profile\.'` (9 names); `ctest -R '^unit\.core\.rdp\.depth'` (4 names); `python3 -B scripts/digest-sweep.py --base <Wave 2 base> --head WORKTREE`; mutant `k1-predicate-widened` | ✅ | ✅ re-run (sweep run against the Wave 0 base: 0 differences) |
| 10-10-T1 | 10 | 2 | VID-07 | T-10-21 | Depth accepted effect on one new id: color bytes, depth word, hidden sidecar | unit | `ctest -R '^unit\.core\.rdp\.depth-composite-accepted$'` | ✅ | ✅ re-run |
| 10-10-T2 | 10 | 2 | VID-07 | T-10-21 | Wrong-depth control, four ids, all Z opcodes and modes, sidecar invalidation and reset | unit and mutant | `ctest -R '^unit\.core\.rdp\.depth-composite-wrong-depth$'`; `ctest -R '^unit\.core\.rdp\.depth-composite-sidecar$'`; mutants `v26-sidecar-invalidation-removed`, `v26-digest-omit-scalar`, `v26-digest-omit-hidden` | ✅ | ✅ re-run |
| 10-10-T3 | 10 | 2 | VID-07 | T-10-19 | Predicate matrix over 25 ids; shipped-id byte identity | unit and sweep | `ctest -R '^unit\.core\.profile\.v26-predicate-matrix$'`; `ctest -R '^unit\.core\.k1\.'`; `python3 -B scripts/digest-sweep.py --base <Wave 2 base> --head WORKTREE` | ✅ | ✅ re-run |
| 10-10-T4 | 10 | 2 | VID-07 | T-10-22 | Depth and K1 documentation in the same wave; no PII | docs | `grep -c "NU6-06-0030-001G" docs/spec/rdp.md` (1); `sh scripts/check-docs.sh`; `sh scripts/pii-scan.sh --tree` | ✅ | ✅ re-run |
| 10-11-T1 | 11 | 2 | CORE-06 | T-10-23, T-10-52 | Idle v26 export: header word 2, section 8 layout, independent decode | unit | `ctest -R '^unit\.core\.state\.export-v26$'`; `ctest -R '^unit\.core\.state\.export$'`; `ctest -R '^unit\.core\.state\.import$'`; `ctest -R '^runner\.state-export$'` | ✅ | ✅ re-run |
| 10-11-T2 | 11 | 2 | CORE-06 | T-10-25 | Mid-render and retained-packet exports, size bound, v25 byte identity | unit, sweep, mutant | `ctest -R '^unit\.core\.state\.export-v26-bounds$'`; `python3 -B scripts/digest-sweep.py --base <Wave 2 base> --head WORKTREE`; mutant `v26-export-omit-hidden` | ✅ | ✅ re-run |
| 10-11-T3 | 11 | 2 | CORE-06 | T-10-24 | Container v2 and build-identity-bound documentation | docs | `grep -c "build-identity-bound" docs/spec/state-container.md docs/spec/machine-contract.md` (2 and 1); `sh scripts/check-docs.sh` | ✅ | ✅ re-run |
| 10-12-T1 | 12 | 2 | CORE-06 | T-10-26 | Mid Z-triangle round trip, RED on the old reader (`5e4aa1e`) then GREEN (`1083257`) | unit | `ctest -R '^unit\.core\.state\.import-v26-roundtrip$'`; `ctest -R '^unit\.core\.state\.import$'` | ✅ | ✅ re-run |
| 10-12-T2 | 12 | 2 | CORE-06 | T-10-29 | Mid scan-out and idle round trips on all four ids, continuation to equal frames and outputs | unit | `ctest -R '^unit\.core\.state\.import-v26-roundtrip$'` | ✅ | ✅ re-run |
| 10-12-T3 | 12 | 2 | CORE-06 | T-10-26, T-10-27 | Hostile rows, field rules and fuzz windows for every new member; validate-then-apply leaves the target untouched | unit and audit | `ctest -R '^unit\.core\.state\.import-v26-hostile$'`; `ctest -R '^harness\.state-field-use-audit$'`; `python3 -B scripts/check-phase09-cases.py --build build/gate --suite state-import` | ✅ | ✅ re-run |
| 10-12-T4 | 12 | 2 | CORE-06 | T-10-28, T-10-26 | Importer mutants, sanitizer timing, TIMEOUT audit | mutant and ASan | `python3 -B scripts/p10-mutants.py --run`; `ctest --test-dir build/asan -R '^unit\.core\.state\.import-v26-'` (2 names, inside the 30-name ASan selection) | ✅ | ✅ re-run (4 importer mutants KILLED; ASan 30 of 30) |
| 10-13-T1 | 13 | 2 | VID-07, CORE-06 | T-10-30 | Scratch-clone gate and ASan selection of every Phase 10 name | review (gate, ASan) | `cd build/10-review/w2/src && sh scripts/gate.sh`; `ctest --preset asan -L phase10` | n/a (scratch) | ✅ recorded (10-REVIEW.md Wave 2); ASan selection re-run on the built asan tree: 30 of 30 |
| 10-13-T2 | 13 | 2 | VID-07, CORE-06 | T-10-30 | Whole-bump byte identity, completeness audit, single-bump check | tool | `python3 -B scripts/digest-sweep.py --base <Wave 0 base> --head HEAD`; `python3 -B scripts/p10-mutants.py --run` | ✅ | ✅ re-run |
| 10-13-T3 | 13 | 2 | VID-07, CORE-06 | T-10-31 | Push and hosted pair | hosted | `gh run list --workflow ci.yml --branch main` | ✅ | ✅ recorded (F-122, run 45530265365) |
| 10-14-T1 | 14 | 3 | RUN-07, GATE-02 | T-10-32 | Tracer guest: Z-tested 4x3 frame built, admitted, captured once; authority and Phase 9 pins unmoved | integration | `cmake --build --preset gate --target t64-phase10-composite-fixture && sh scripts/build-composite-fixture.sh --qualify`; `ctest -R '^harness\.rdp-oracle-authority$'`; `ctest -R '^harness\.phase09-qualification$'` | ✅ | ✅ re-run (ROM built and admitted is exercised by `runner.composite-two-run`; the fixture rebuild itself not repeated) |
| 10-14-T2 | 14 | 3 | RUN-07 | T-10-36 | Audio, input-driven save, wrong variant, independent twin oracle | harness (python) | `python3 -B fixtures/composite/composite_oracle.py --input fixtures/composite/composite.input.json` | ✅ | ✅ re-run |
| 10-14-T3 | 14 | 3 | RUN-07, GATE-02 | T-10-32, T-10-33 | Admission, rights and pin checks | integration and gov | CTest `gov.*` (13 names), `harness.rdp-oracle-authority`, `harness.phase09-qualification`, `harness.audio-qualification`, `harness.rsp-evidence-ownership`, `runner.save-fixtures`; `sh scripts/pii-scan.sh --tree` | ✅ | ✅ re-run |
| 10-14-T4 | 14 | 3 | RUN-07, GATE-02 | T-10-33, T-10-50 | The single manifest-moving commit; request pin unchanged; Phase 9 records unchanged | git and harness | `git log --oneline <Wave 0 base>..HEAD -- fixtures/manifest.json` (one commit, `f027b05`); `git diff --quiet <Wave 0 base> HEAD -- docs/claims/phase09.md docs/evidence/phase09-qualification.md` | ✅ | ✅ re-run |
| 10-15-T1 | 15 | 3 | RUN-07 | T-10-36 | Two-run tracer: raw run then save-out run, equal `state_digest` | integration | `ctest -R '^runner\.composite-two-run$'` | ✅ | ✅ re-run (both runs print the same 64-hex digest) |
| 10-15-T2 | 15 | 3 | RUN-07 | T-10-36, T-10-38 | Every output against the independent expectation with wrong-value controls | integration | `ctest -R '^controls\.real\.composite-wrong-rom$'`; `ctest -R '^harness\.composite-oracle$'`; `ctest -R '^runner\.composite-two-run$'` | ✅ | ✅ re-run |
| 10-15-T3 | 15 | 3 | CORE-06 | T-10-23 | Container v2 format string in the runner contract with pairing refusals | integration | `ctest -R '^runner\.state-format-v26$'`; `ctest -R '^runner\.state-export$'`; `ctest -R '^runner\.save-roundtrip$'`; `sh scripts/check-docs.sh` | ✅ | ✅ re-run |
| 10-15-T4 | 15 | 3 | RUN-07, CORE-06 | T-10-37 | Container v2 positive path on the composite; full RUN-07 sweep | integration | `ctest -R '^runner\.composite-state-v2$'` and the four names of T2 and T3 | ✅ | ✅ re-run |
| 10-16-T1 | 16 | 3 | CORE-06 (docs) | T-10-39 | Relabel one doc end to end; stale-label gate counts down | docs | python stale-label scan from the plan over `docs/spec/raw-capture.md` | ✅ | ✅ re-run (stale 0) |
| 10-16-T2 | 16 | 3 | CORE-06 (docs) | T-10-40 | Every site in the 7 live spec docs relabeled or settled | docs | the same scan over the 7 docs (stale 0); `sh scripts/check-docs.sh && sh scripts/check-claims.sh` | ✅ | ✅ re-run |
| 10-16-T3 | 16 | 3 | GATE-01 (hygiene) | T-10-39 | Phase 9 carry-in triage; dead publisher link removed | unit and integration | `ctest -R '^unit\.replay\.suite$'`; `ctest -R '^runner\.replay-mix$'`; `python3 -B scripts/check-phase09-cases.py --build build/gate --suite replay` | ✅ | ✅ re-run |
| 10-17-T1 | 17 | 3 | all six | T-10-41 | Qualifier tracer: snapshot, one stage, record, verify | harness (python) | `ctest -R '^harness\.phase10-qualification$'` | ✅ | ✅ re-run (22 hermetic cases) |
| 10-17-T2 | 17 | 3 | all six | T-10-41, T-10-42 | Remaining stages with measured limits; rehearsal record refused as final | harness (python) | `ctest -R '^harness\.phase10-qualification$'`; `python3 -B scripts/qualify-phase10.py --help` | ✅ | ✅ re-run |
| 10-17-T3 | 17 | 3 | all six | T-10-42 | Rehearsal on the current head | native qualification | `python3 -B scripts/qualify-phase10.py --native --rehearsal` | ✅ | ✅ recorded (PIN-IMPACT.md section 14); not re-run, it is the qualifier |
| 10-18-T1 | 18 | 3 | all six | T-10-43 | Scratch-clone gate and ASan selection of the final head | review (gate, ASan) | `cd build/10-review/final/src && sh scripts/gate.sh && ctest --preset asan -L phase10` | n/a (scratch) | ✅ recorded (10-REVIEW.md Wave 3); ASan selection re-run: 30 of 30 |
| 10-18-T2 | 18 | 3 | RUN-07, GATE-02 | T-10-43, T-10-44 | Composite evidence independence; exclusion unrelaxed; one manifest commit | harness and git | `python3 -B fixtures/composite/composite_oracle.py --check fixtures/composite/composite.expected.json`; `git log --oneline <Wave 0 base>..HEAD -- fixtures/manifest.json` (one commit) | ✅ | ✅ re-run |
| 10-18-T3 | 18 | 3 | all six | T-10-44 | Final audit matrix; sweep and mutants on the final head | tool | `python3 -B scripts/digest-sweep.py --base <Wave 0 base> --head HEAD`; `python3 -B scripts/p10-mutants.py --run` | ✅ | ✅ re-run |
| 10-18-T4 | 18 | 3 | GATE-01 | T-10-45 | Push the audited head; hosted pair green | hosted | `gh run list --workflow ci.yml --branch main` | ✅ | ✅ re-run (read-only): run 45548987145 conclusion success on both jobs, same head as the cold run |
| 10-19-T1 | 19 | 4 | all six | T-10-46 | Final native qualification on the audited head; verifier accepted it | native qualification | `python3 -B scripts/qualify-phase10.py --native`; `python3 -B scripts/qualify-phase10.py --verify <record>` | ✅ | ✅ recorded (private record, 4 attempts, last one passed; `docs/evidence/phase10-qualification.md`); not re-run by constraint, and `--verify` is documented to refuse later heads |
| 10-19-T2 | 19 | 4 | GATE-01 | T-10-46 | Orchestrator-only cold-cache hosted `gate-macos` run on the final tree | hosted (manual-only) | `gh run view 45550277510 --json conclusion,jobs` and the job log read | ✅ | ✅ re-run (read-only): success; "Cache not found" at restore; cache saved after the job |
| 10-19-T3 | 19 | 4 | GATE-01 | T-10-46 | Cold run recorded against the 900 s limit | hosted | job seconds from `startedAt` and `completedAt` of `gate-macos`, `test "$s" -le 900` | ✅ | ✅ re-run (731 s) |
| 10-19-T4 | 19 | 4 | all six | T-10-47, T-10-48 | Sanitized evidence and claims: eight claim blocks, no private path, no overclaim | docs | `sh scripts/check-claims.sh --files docs/claims/phase10.md`; `sh scripts/check-docs.sh`; `sh scripts/pii-scan.sh --tree`; `grep -c '^```t64-claim$' docs/claims/phase10.md` (8) | ✅ | ✅ re-run |

*Status: ✅ green · ❌ red · ⚠️ flaky. Every row is green; none is red or flaky in this audit.*

### Re-run commands (exact forms; run from the repository root)

```sh
# all 30 Phase 10 names, native and sanitizer lanes (built trees, no rebuild)
ctest --test-dir build/gate -L phase10 --output-on-failure
ctest --test-dir build/asan -L phase10 --output-on-failure -j2

# GATE-01, GATE-02 and supporting harness tests (13 names)
ctest --test-dir build/gate -R '^(harness\.(rdp-oracle-authority|rdp-vi-qualification|phase09-qualification|state-field-use-audit)|repo\.(ci-yml|ci-yml-selftest|si-membership-sites|rsp-no-hle)|unit\.core\.(state\.export|rdp\.depth)|runner\.(state-export|save-roundtrip|save-fixtures))$' --output-on-failure
ctest --test-dir build/gate -R '^unit\.core\.state\.import$' --output-on-failure
ctest --test-dir build/gate -R '^(gov\..*|harness\.(audio-qualification|rsp-evidence-ownership)|unit\.replay\.suite|runner\.replay-mix)$' --output-on-failure

# scripts
python3 -B scripts/tests/test_rdp_oracle_authority.py
python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked      # exit 0
python3 -B scripts/check-rdp-oracle-authority.py --require-approved    # exit 2
sh scripts/check-ci-yml.sh && sh scripts/check-ci-yml.sh --selftest
sh scripts/check-test-inventory.sh --preset gate
python3 -B scripts/check-rsp-no-hle.py --build build/gate
python3 -B scripts/p10-mutants.py --check-anchors
python3 -B scripts/p10-mutants.py --run          # 26 KILLED, 1 SURVIVED (control), 1 INVALID (control); 28 rows
python3 -B scripts/digest-sweep.py --base <Wave 0 base from PIN-IMPACT.md> --head HEAD
python3 -B scripts/digest-sweep.py --selftest
python3 -B fixtures/composite/composite_oracle.py --check fixtures/composite/composite.expected.json

# hosted measured cold run (read-only)
gh run view 45550277510 --json conclusion,jobs
```

## Wave 0 Requirements

- [x] `scripts/preflight-phase10.sh` exists with its 46 count rows in `scripts/preflight-phase10-regexes.txt`; every row was re-counted against `ctest -N -R` in this audit with 0 mismatches. The script itself was not re-executed (it can rebuild and rewrites its own replay directories under the gate tree); recorded PASS at 82 s (10-18) and 40 s (10-19)
- [x] committed two-tree digest sweep with forced build identity (`scripts/digest-sweep.py`, `tools/digest-sweep/t64_digest_sweep.c`): re-run 1599 shared lines, 0 differences; `--selftest` PASS
- [x] `core/tests/test_k1_refusal.c` characterization: first commit `093fb27` precedes the first Phase 10 edit of `core/t64_internal.h` (`286ccbb`); `unit.core.k1.dispatch-refusal` and `unit.core.k1.apply-refusal` pass
- [x] `scripts/p10-mutants.py` with anchors for the VG and VID mutants: 28 rows (plan 10-20 added v26-digest-omit-vi-phase-cycles), all as expected in a fresh run
- [x] `core/tests/test_digest_v26.c` RED completeness test before the walker change: RED `42f64db` precedes the activation commit `831c947`
- [x] pin-impact map committed (`.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md`)

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Cold-cache hosted `gate-macos` run inside the 900 s job limit | GATE-01 | Needs an empty hosted Binutils cache entry and a hosted runner; deleting the cache entry and dispatching the workflow are orchestrator actions, not CTest | Delete the macOS Binutils cache entry with `gh cache delete`, run `gh workflow run ci.yml`, then `gh run view <id> --json jobs`. Pass: the Gate step succeeds, the restore log says "Cache not found", and job seconds are at most 900. Evidence: run 45550277510, 731 s (read back in this audit) |
| Removal of the redundant `repo.rsp-no-hle` guard from the audio-records step | GATE-01 | Declined with a recorded reason: removal needs an edit of the closed Phase 8 qualifier and its case inventory pin (F-106 keeps that qualifier frozen); the 15-minute limit alone fits the measured cold run | No test. Recorded in `docs/ci-reviews/LOG.md` F-116 and F-117 and in the annotation in `.planning/REQUIREMENTS.md`; revisit at the next Phase 8 requalification |
| Final native qualification record and its verifier | all six | A private record on the machine that ran it (about 7 minutes on a quiet host, refused by a loaded host); `--verify` binds the exact qualified head and is documented to refuse any later head | Switch to the qualified head named in `docs/evidence/phase10-qualification.md` and run `python3 -B scripts/qualify-phase10.py --verify` on the record named there; expect `record verified against the current tree` |
| Scratch-clone gates and hosted pairs of the review plans (10-05, 10-08, 10-13, 10-18) | all six | They run the full gate in a detached clone or read hosted runs; excluded from this audit by instruction | `git clone --no-checkout --no-hardlinks` into `build/10-review/<wave>/src`, check out the wave head, run `sh scripts/gate.sh`; read the hosted pair with `gh run list --workflow ci.yml` |

## Accepted partial gaps (carry-ins, none blocks)

These are test-strength or discovery-aid gaps, not failing requirements. Each is recorded in `10-REVIEW.md`, `PIN-IMPACT.md` section 13.1, `docs/evidence/phase10-qualification.md` ("Recorded exceptions for owner review", item 8) and the Phase 11 carry-ins of `.planning/ROADMAP.md`. They were accepted by delegated autonomy, not by an owner approval, and sit at the guardrail limit of five carried warning-level gaps.

| Gap | Requirement | Severity | What is and is not pinned | Audit confirmation |
|-----|-------------|----------|---------------------------|--------------------|
| W2-01 | CORE-06 | WARNING | The importer's written-out Z product bound `state_z_products_bounded` (`core/t64_state.c`) is pinned only by one parity case that compares two packets and one hostile row; nothing pins its constants | Reproduced: four probe mutants, run in memory through the committed driver in a throwaway worktree (limit `INT64_MAX / 4` raised to `/ 2`, the Y divisor `y_bound` raised to `y_bound + 1`, the X term and the edge term each disabled) all compiled and all SURVIVED both cases (`roundtrip`, `hostile`) of the v26 import test. The bound is correct today per the 6,000,000-packet differential recorded in 10-13; the risk is future drift. Closing it needs a new numbered plan (boundary parity plus mutant rows); a test cannot be added by this audit because that would change the inventory and the `PHASE10_TESTS` block |
| W2-02 | VID-07 | WARNING | `scripts/list-membership-sites.py` has no token for `t64_has_si_v25_profile` or `t64_has_si_v26_profile`, so six site rows are missing from `docs/spec/profile-membership-sites.txt`; `repo.si-membership-sites` passes because it compares the generator with the committed file | Confirmed by reading the generator (0 occurrences of either predicate) and the passing test. Behavior stays guarded independently by `unit.core.profile.v26-predicate-matrix` (25 ids), `unit.core.k1.*`, the exact-tuple registry and the sweep. The script is a Phase 9 bound path, so it is not edited here |
| DEF-W1-01 | VER-06 | INFO | Three sibling copies of the perspective and major-edge floor in `core/t64_rdp_tmem.c` and `core/t64_rdp_raster.c` are not pinned behaviorally; outside the named VG-01 to VG-05 and clock gaps | Recorded by review with five scratch mutants; not re-run |
| W1-03 | VER-06 | INFO | One sentence stating the `ceil` rule is missing from `docs/spec/vi.md`; the vector itself is pinned and was recomputed here | Recomputed: ceil(1,042,708.4657...) = 1,042,709 |
| D-10-17-01 | GATE-01 (local only) | INFO | The two Phase 6 cold-boot tests stop working after 99 gate runs in one long-lived local build tree; hosted runners and fresh clones are unaffected | `.planning/phases/10-carry-in-closure-before-the-freeze/deferred-items.md` |
| Guard removal | GATE-01 | accepted deviation | The redundant `rsp-no-hle` run in the audio-records step stays (F-116, F-117) | see Manual-Only |

No requirement is MISSING. No test was added by this audit: the six requirements already own passing tests, and the two partials above can only be closed by a new numbered plan (a new CTest name or a generator change moves the inventory and a Phase 9 bound path).

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (69 of 69 tasks mapped above)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify (every task row has an automated command; the review-wave rows use gate, hosted or script checks)
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 90 s at task level (longest focused check 53 s)
- [x] `nyquist_compliant: true` set in frontmatter (every requirement COVERED; the two warning-level partials are documented accepted carry-ins)

**Approval:** validated 2026-11-25 by the Nyquist audit against the built gate and asan trees and read-only hosted records. This is not an owner approval of the carried gaps or of the recorded exceptions for owner review in `docs/evidence/phase10-qualification.md`; flip `nyquist_compliant` to false if the owner declines W2-01 or W2-02.
