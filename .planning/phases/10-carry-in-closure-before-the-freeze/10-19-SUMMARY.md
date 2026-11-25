---
phase: 10-carry-in-closure-before-the-freeze
plan: 19
subsystem: qualification
tags: [final-qualification, tracer-gate, cold-cache-run, gate-01, evidence, claims, owner-review, g3-limit]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: the audited head f081d39 with its green hosted pair, the Qualification base ad39b6be2f803af45af15d4ed511b72a89460339, the qualifier and its rehearsal (10-17), the Gate-growth ledger and the owner-review flags (10-18)
provides:
  - the one final native qualification record 20261125T134440416529Z-b9d957c3 (private, verified) recorded in PIN-IMPACT.md section 15.1 beside the three failed attempts
  - the measured cold-cache hosted gate-macos run 45550277510 (731 s of 900 s, margin 169 s) in LOG.md (Runs row, F-124, F-125) and PIN-IMPACT.md section 15.2 plus the cold row of the Gate-growth ledger
  - docs/evidence/phase10-qualification.md (sanitized qualification record with "Recorded exceptions for owner review")
  - docs/claims/phase10.md (eight unit-tested claim blocks that pass scripts/check-claims.sh)
affects: [phase-10-verification, phase-11]

plan_head_before: 1f862a0f7a255c5f0dd1dabb488acf44dc0a5259
plan_head_after: c4794ce9fd606824b806e51ddd9151463ef6bc4d

actuals:
  # chars/4 over the added lines of the realized diff at SUMMARY write time (this summary and the STATE edit are not in it)
  tokens: 25700
  tasks: 4
  commits: 6

tech-stack:
  added: []
  patterns:
    - "a failed final-qualification attempt stays in the evidence beside the passing record: same command, four attempts, no limit widened, no source change, host load named only generically"
    - "a claim block binds each counted CTest name to exactly one registered id; the other ids a name exercises are listed as exercised and not claimed"
    - "the verifier of a one-shot qualification is run once before the evidence commit and never after (the evidence and claim files are bound inputs newer than the base)"

key-files:
  created:
    - docs/evidence/phase10-qualification.md
    - docs/claims/phase10.md
  modified:
    - docs/ci-reviews/LOG.md
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md
    - .planning/phases/10-carry-in-closure-before-the-freeze/10-19-SUMMARY.md

key-decisions:
  - "GATE-01 is met by measurement: the cold-cache gate-macos job of run 45550277510 took 731 s against the 900 s limit (integer comparison, margin 169 s, 18.8 percent of the limit; the old 600 s limit would have been exceeded by 131 s). The G4 reversal rule (cold above 900 s or cancelled) is not triggered, so no trim plan follows."
  - "The measured cold run is higher than the older projections (F-111 673 s, plan 10-18 571 s) and inside the 10-17 ranges (696 to 735 s primary, 709 to 806 s cross-check); it is reported as measured, one sample, with the F-119 spread of 52 s beside it."
  - "The final native qualification passed on its fourth attempt (record 20261125T134440416529Z-b9d957c3, --verify exit 0); the three failed attempts (eight Phase 8 and 9 supervised-run tests at supervisor wall backstops under unrelated host load) stay in the evidence, no limit was widened and no source changed."
  - "docs/claims/phase10.md holds eight blocks, all tier unit-tested: VID-07 (3 names), CORE-06 (4 names), RUN-07 (1 ROM row) and five VER-06 blocks (4, 2, 1, 1 and 1 names) so that each of the nine unit.core.vg.* names binds to one id; no hardware, oracle, broad-title or publication claim; VID-04 stays deferred and unclaimed."
  - "The evidence document carries the owner-review exceptions as recorded items and not as approvals; the comparator change of T-10-49 is explicitly not an owner approval."

patterns-established:
  - "Evidence documents name private records by repo-relative path and id only, and describe host conditions generically"

requirements-completed: [GATE-01, GATE-02, VER-06, VID-07, CORE-06, RUN-07]

coverage:
  - id: D1
    description: "The measured cold-cache hosted gate-macos run 45550277510 on the audited head f081d39 finished inside the 900 s limit (731 s) with an actions/cache miss on the macOS Binutils key, the Binutils build inside the Gate step and the entry saved afterwards"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "gh run view 45550277510 --json conclusion,jobs (success); Task 2 and Task 3 automated verify commands (gate-macos seconds: 731, test -le 900); log lines 'Cache not found for input keys: binutils-macOS' and 'Cache saved with key: binutils-macOS' one each; gh cache list shows the macOS entry recreated"
        status: pass
    human_judgment: false
  - id: D2
    description: "The one final native qualification passed on the audited tree and its record verifies against that tree"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "python3 -B scripts/qualify-phase10.py --native (attempt 4 of 4, exit 0, 418.6 s); python3 -B scripts/qualify-phase10.py --verify build/gate/10-qualification-private/20261125T134440416529Z-b9d957c3/record.json (exit 0, 'record verified against the current tree'); rehearsal false, drift paths empty"
        status: pass
    human_judgment: false
  - id: D3
    description: "LOG.md has the Runs row and findings F-124 and F-125 for the cold run; PIN-IMPACT.md records the passing record (15.1), the cold run and the GATE-01 closeout (15.2) and the cold row of the Gate-growth ledger"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "grep -c 45550277510 docs/ci-reviews/LOG.md (3); grep -c '^### 15.2' PIN-IMPACT.md (1); Task 3 verify command printed 'gate-macos seconds: 731'"
        status: pass
    human_judgment: false
  - id: D4
    description: "docs/evidence/phase10-qualification.md and docs/claims/phase10.md are sanitized, validated and bound to the recorded build and commit; the Phase 9 records are byte unchanged"
    requirement: "VID-07, CORE-06, RUN-07, VER-06"
    verification:
      - kind: other
        ref: "sh scripts/check-claims.sh (files=71 blocks=22 errors=0, 8 blocks in phase10.md); sh scripts/check-docs.sh; sh scripts/pii-scan.sh --tree (findings 0); sh scripts/rights-scan.sh --tree (findings 0); sh scripts/preflight-phase10.sh PASS 40 s; ctest gov.claims.*, gov.rights.repo, gov.pii.repo, harness.phase09-qualification, harness.phase10-qualification, harness.rdp-oracle-authority 9 of 9; git diff --quiet -- docs/claims/phase09.md docs/evidence/phase09-qualification.md"
        status: pass
    human_judgment: false
  - id: D5
    description: "The recorded exceptions (T-10-49 comparator, G4 growth, G5 plan count, the two accepted wave 3 deviations, the guard-removal decline, the orchestrator rebind, the native retries, the deferrals at the G3 limit) are surfaced for the owner and none is presented as an approval"
    requirement: "GATE-02"
    verification:
      - kind: other
        ref: "docs/evidence/phase10-qualification.md section 'Recorded exceptions for owner review' (8 items); this summary repeats them below"
        status: pass
    human_judgment: true
    rationale: "The owner decides whether any of the eight items needs a different disposition; this plan records them and approves none"

duration: 40min
completed: 2026-11-25
status: complete
---

# Phase 10 Plan 19: Qualification, orchestrator cold-cache run, evidence and claims Summary

**The one Phase 10 qualification passed natively on its fourth attempt and verifies, the orchestrator's cold-cache hosted gate-macos run measured 731 s against the 900 s limit (margin 169 s, G4 reversal rule not triggered), and a sanitized evidence document and eight validated unit-tested claim blocks (VID-07, CORE-06, RUN-07, VER-06) now exist, with no hardware, oracle or publication claim and eight recorded exceptions listed for owner review.**

## Performance

- **Duration:** about 40 min for this continuation (Tasks 3 and 4 and the close-out; the plan as a whole spans one day because the final qualification needed three repeats on a loaded host, which the orchestrator ran). The earlier halted execution of the plan took 21 min.
- **Tasks:** 4 of 4 (Task 1 tracer: passed on the orchestrator's attempt 4, the executor wrote its record; Task 2 orchestrator-only, re-verified by the executor; Tasks 3 and 4 executed here)
- **Files:** 2 created (`docs/evidence/phase10-qualification.md`, `docs/claims/phase10.md`), 2 modified (`docs/ci-reviews/LOG.md`, `PIN-IMPACT.md`) plus this summary; no source, test, script or pin changed
- **New CTest names:** none

## Accomplishments

- **Task 1 (tracer), recorded.** Attempt 4 of `python3 -B scripts/qualify-phase10.py --native` passed (418.6 s; ci-scanner 2.16, inventory 0.65, oracle 0.15, gate 129.41, phase10-junit 7.16, asan 136.95, mutants 100.72, sweep 41.02 s; 25 mutants KILLED, controls as expected; sweep 1599 shared lines, 0 differences) and `--verify` exited 0 on the record `20261125T134440416529Z-b9d957c3`. The three failed attempts stay beside it in PIN-IMPACT.md section 15.1 and in the evidence document. The tracer gate (do not expand after a failed tracer) held: Tasks 3 and 4 started only after the pass.
- **Task 2 (orchestrator-only), re-verified.** Both automated verify commands pass with the planned wording against `COLD_RUN_ID=45550277510`; nothing was deleted or dispatched by the executor.
- **Task 3.** The Runs row of `docs/ci-reviews/LOG.md` for run 45550277510 and findings **F-124** (the cold measurement against the 900 s limit) and **F-125** (the failed native attempts, no limit widened); PIN-IMPACT.md section 15.2 (cold run, cache state, GATE-01 closeout) and the cold row of the Gate-growth ledger. The plan's verify command printed `gate-macos seconds: 731` and passed `test -le 900`.
- **Task 4.** `docs/evidence/phase10-qualification.md` (identities, native stage table, the four attempts, the 27-row mutant table, sweep, K1 and oracle authority, hosted records, triage reference, success criteria map, claims index, ids exercised and not claimed, "Recorded exceptions for owner review", limits, reproduction) and `docs/claims/phase10.md` (eight blocks). `check-claims.sh`, `check-docs.sh`, the PII scan, the rights scan and the preflight pass; the Phase 9 records are unchanged.

## Cold run 45550277510 (GATE-01)

| Item | Value |
|------|-------|
| run | workflow `ci`, `workflow_dispatch`, head `f081d392763ad384c3d50ae1b24b709d5af3b7d4` (the audited tree), conclusion `success` |
| gate-macos | 04:57:52Z to 05:10:03Z = **731 s** against **900 s**, margin **169 s** (18.8 percent); the 600 s limit of Phase 9 would have been exceeded by 131 s |
| steps (s) | Restore pinned Binutils 1 (miss), **Gate 653** (fixture-qualification 219 = the Binutils build, ctest 265 on 254 names, test-inventory-selftest 57, boot-evidence-selftest 31, build 31, core-lint 28, lint-selftest 12), audio qualification records 40, Phase 9 records 5, Post Restore 3 (save) |
| gate-linux | 250 s (cache hit, Gate 200 s) |
| cache after | macOS entry recreated (5.03 MiB, created 05:09:56Z); Linux entry untouched; assumption A5 (a timed-out job saves no cache) not exercised |
| against earlier numbers | warm F-111 487 s, warm F-120 346 s and F-123 385 s, F-119 spread up to 52 s; projections: F-111 cold 673 s (measured 58 s higher), plan 10-18 cold 571 s (160 s higher), 10-17 primary 696 to 735 s (top end) and cross-check 709 to 806 s (inside) |
| G4 reversal rule | not triggered (731 is not above 900, not cancelled) |

The measured run is slower than the older projections and still inside the limit; against the warm run on the same head it adds 346 s (fixture-qualification +217 s, ctest +69 s which is 17 s beyond the F-119 spread, the rest small rises). It is one sample.

## Task Commits

1. **Task 1 recording: final qualification, repeats and the passing record** - `d3d4afc` (docs, PIN-IMPACT.md section 15.1)
2. **Task 3: cold run against the 900 s limit** - `0370f25` (docs, LOG.md and PIN-IMPACT.md; carries `[skip ci]`)
3. **Task 4: sanitized evidence and claims** - `c4794ce` (docs, the two new documents)

Earlier in this plan (the halted execution): `4c27f4c` (failed attempt 1 recorded), `30a03d1` (halted summary), `b9d957c` (STATE halt). **Plan metadata:** this summary with the STATE, ROADMAP and REQUIREMENTS updates follows in one docs(10-19) commit.

## Claims

| Block | Profile id | Counted |
|------:|------------|---------|
| 1 VID-07 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2` | 3 of 3 `unit.core.rdp.depth-composite-*` |
| 2 CORE-06 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2` | 4 of 4 `unit.core.state.import-v26-roundtrip`, `-hostile`, `unit.core.digest.v26-completeness`, `-strip-differential` |
| 3 RUN-07 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` | 1 of 1 accepted ROM row `phase10-composite-s32k` (through `runner.composite-two-run`) |
| 4 VER-06 | `...-rdp-depth-v7` | 4 of 4 (decal-equality, primitive-z-saturation, delta-ceiling, z-exponent-boundaries) |
| 5 VER-06 | `...-rdp-vi-v8` | 2 of 2 (ntsc-clock, rgba5551-alpha-clear) |
| 6 VER-06 | `...-rdp-vi-filter-v9` | 1 of 1 (serrate-filter-parity) |
| 7 VER-06 | `...-rdp-texture-v4` | 1 of 1 (perspective-floor) |
| 8 VER-06 | `...-rdp-triangle-v3` | 1 of 1 (shade-floor-bayer) |

All tier `unit-tested`, `manifest_digest` `sha256:a5788a02...` (the manifest at the qualified commit, moved once by plan 10-14), `build_id` `0.0.0+t64-build-v1-b6c298e6...`. The nine VER-06 names run on six registered ids; each name is bound to one and the other ids (`rdp-vi-filter-v9` for the alpha clear, `rdp-blender-v6` for the Bayer matrix, the three save ids of the si-v2 family) are listed in the evidence document as exercised and not claimed.

## Recorded exceptions for owner review

None of these is an owner approval. They are repeated here from `docs/evidence/phase10-qualification.md` so the owner sees them at the end of the phase.

1. **T-10-49, the Phase 9 claim-binding comparator.** `claims_final_problems` in `scripts/tests/test_phase09_qualification.py` moved from the live whole-file manifest digest to the pinned Phase 9 manifest identity (`sha256:b75864fd...`) plus a canonical per-row view of the 39 rows that manifest held (plan 10-03, PIN-IMPACT.md section 9). Basis: delegated autonomy, ROADMAP SC5 and the 2026-11-23 per-row approval; **not an owner approval of this comparator change**. Reversible test code; no Phase 9 claim content, tier or authority moved (`git diff` of `docs/claims/phase09.md`, `docs/evidence/phase09-qualification.md` is empty).
2. **G4 gate-growth exception.** 224 to 254 CTest names, no offset claimed. Measured: the 30 Phase 10 names sum 23.35 s natively and 305.08 s under ASan; native ctest wall 77.30 s at 254 names against 99.7 s at 224 (different loads); hosted macOS ctest 265 s cold and 196 s warm against 251 s at 224 names (F-111); cold gate-macos 731 s of 900 s, margin 169 s; reversal rule not triggered.
3. **G5 plan-count exception.** 19 plans against a guideline of about 10 to 15 (four mandated per-wave review plans, qualifier split from the qualification).
4. **Two accepted wave 3 deviations.** One literal of `runner/tests/test_state_runner.c` (v2 to v3 string, case and inventory unchanged, no pin hashes the file); the wrong-ROM control compares its save through the guest read-back record (a failing run publishes no save; the transcript is compared whole).
5. **GATE-01 guard-removal decline.** The `rsp-no-hle` guard stays: removal needs `scripts/qualify-audio.py` and `docs/spec/audio-cases.json` edits frozen by F-106 (F-116; up to about 41 s forgone on hosted macOS).
6. **One orchestrator rebind of `PINNED_REQUEST_SHA256`** (plan 10-03, commit `b64098f`): `a4bb7774...` to `e131cbe7...`, identity only (whole-file manifest hash replaced by 39 per-row pins), unchanged since; the composite rows were appended without a rebind.
7. **Native qualification retries.** Three failed attempts (supervisor wall backstops in eight Phase 8 and 9 tests under unrelated host load), then the pass; limits not widened, no source change.
8. **Deferred carry-ins at the G3 limit of five:** W2-01, W2-02, DEF-W1-01, W1-03, D-10-17-01; plus the Phase 9 triage deferrals T1, T2, T4, T5, T6, T7, T8 (T3 done). VID-04 stays deferred and unclaimed; D5, D6 and D7 stops intact.

## Decisions Made

See `key-decisions` above.

## Deviations from Plan

**1. [Tracer gate, resolved outside this agent] The first executor attempt halted at Task 1**
- **Found during:** Task 1 (first execution, recorded in the halted summary and commit `4c27f4c`).
- **Issue:** the qualification command was refused at the gate stage by supervisor wall backstops under host load; the plan stops on a failed stage.
- **Fix:** none by the executor. The orchestrator repeated the command (attempts 2 to 4) after reducing the unrelated host load; attempt 4 passed and verifies. All four are recorded.
- **Commit:** `4c27f4c`, `d3d4afc`

**2. [Rule 2 - privacy] Generic wording of the host cause in PIN-IMPACT.md section 15**
- **Issue:** section 15, written by the earlier executor, named a specific virtual machine product; the owner rule says host causes are described generically.
- **Fix:** the sentence now reads "an unrelated virtual machine". The earlier unpushed commits (`4c27f4c`, `30a03d1`) still contain the old wording and a machine detail (a core count) in their text; no history was rewritten (see the orchestrator action items).
- **Files modified:** `PIN-IMPACT.md`. **Commit:** `d3d4afc`

**3. [Addition to the plan] A second finding, F-125**
- The plan asks for the finding "F-NNN" of the cold run; F-125 (the failed native attempts and why no limit moved) was added beside F-124 because the retries are part of the GATE-01 and qualification story and the log is the append-only place that Phase 11 reads. Verdict `declined` (nothing changed).

**4. [Plan wording] "descriptor-10 profiles" for the VER-06 names**
- The plan's Task 4 text names the VER-06 profiles as descriptor 10. By `core/t64_machine.c` only `rdp-depth-v7`, `rdp-vi-v8` and `rdp-vi-filter-v9` are descriptor 10; `rdp-triangle-v3` and `rdp-texture-v4` are descriptor 7 and `rdp-blender-v6` is descriptor 9. The claim blocks and the evidence document name the exact registered ids and state no descriptor number for them, so there is nothing false to correct; the plan text should read "their registered profile ids".

**5. [Design choice within the plan] Eight claim blocks, not four**
- The plan requires at least four. VER-06 is split into five blocks (one per id) so that each of the nine names binds to exactly one id, as the Phase 9 stem rule does; the 4-name floor (`grep -c` of at least 4) is exceeded.

**Total deviations:** 5 (1 gate stop resolved by the orchestrator, 1 Rule 2 privacy wording, 1 addition, 1 plan-wording note, 1 design choice). **Impact on plan:** none on scope; the plan's four tasks are complete and no source, test, script or pin changed.

## Issues Encountered

- The final qualification was refused three times on a loaded host before it passed (PIN-IMPACT.md section 15.1); host load as the cause is an inference, not a reproduced cause.
- The measured cold run (731 s) is above both older projections (673 s and 571 s) although inside the limit by 169 s; one sample, with a 52 s run-to-run spread. A later cold run above about 850 s would be a warning.
- One text error was caught before commit: the first draft of F-125 said "seven" tests and "five hosted runs of this tree"; it was corrected to "eight" and to the five hosted runs of Phase 10 before the LOG commit (no committed row was edited).

## Authentication Gates

None. `gh` was already authenticated; only read-only run and cache queries were made, and the executor deleted no cache and dispatched no workflow.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-46 (cold-cache timeout loop): the run finished inside 900 s and the cache entry was recreated. T-10-47 (disclosure): the PII scan and the rights scan report 0 findings, no workstation path, host name, user name or other-project name is in the new documents, and the private records stay under `build/gate`. T-10-48 (overclaiming): `check-claims.sh` passes, only the tier `unit-tested` is used, no reserved word appears, and each block has its own not-claimed text.

## Next Phase Readiness

Phase 10 is complete at the plan level (19 of 19) and ready for the orchestrator's phase verification. Phase 11 starts from a qualified carry-in state: the one identity and state bump is done, the shipped ids are byte identical, the gate fits the 15-minute limit by measurement, and the carry-ins are listed in ROADMAP Phase 11.

**Orchestrator action items:**

1. **Push is pending and was NOT done** (the plan does not state one). Local `main` is `origin/main` (`f081d39`, the audited head) plus 11 commits including this plan's close-out commit, all documents only (`.planning/`, `docs/ci-reviews/LOG.md`, `docs/evidence/phase10-qualification.md`, `docs/claims/phase10.md`). A push of them still runs the full hosted gate once (the tree-wide claims, PII and rights scans must see the new documents, F-099); the Task 3 commit carries `[skip ci]`, the last commit decides the push run. The orchestrator pushes by pinned SHA, fast-forward, after the PII scan, no force.
2. **Privacy of unpushed history.** `4c27f4c` and `30a03d1` (unpushed) contain the old section 15 wording that names a virtual machine product and a machine detail (a core count); `d3d4afc` generalizes the current text only. If the owner wants those words out of the public history, reword or squash those unpushed commits before the push (not done here: no history rewriting).
3. **`--verify` is one-shot.** The record verified at head `b9d957c`; the evidence and claim files are bound inputs newer than the Qualification base, so do not re-run `--verify` at the current head (it is expected to refuse; this was read from the qualifier, not re-run).
4. `requirements.ready-ids` should now mark GATE-01, GATE-02, VER-06, VID-07, CORE-06 and RUN-07 complete (this summary declares all six; every other declaring plan has its summary).
5. Phase verification and the owner-review items above (the eight recorded exceptions, G3 at its limit of five carried items, no further gap can be carried without closing one).

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-25*

## Self-Check: PASSED

Created files exist (`docs/evidence/phase10-qualification.md`, `docs/claims/phase10.md`); modified files exist (`docs/ci-reviews/LOG.md`, `PIN-IMPACT.md`); the commits `4c27f4c`, `d3d4afc`, `0370f25` and `c4794ce` are ancestors of HEAD; `git rev-list --count 1f862a0f7a255c5f0dd1dabb488acf44dc0a5259..c4794ce9fd606824b806e51ddd9151463ef6bc4d` is 6; `sh scripts/check-claims.sh --files docs/claims/phase10.md` reports `files=1 blocks=8 errors=0`; the tree PII scan reports 0 findings; `git diff --quiet -- docs/claims/phase09.md docs/evidence/phase09-qualification.md` exits 0. The `--verify` of the final record was run once (head `b9d957c`, exit 0) and not repeated after the evidence commit, as the plan requires.
