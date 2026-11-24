---
phase: 10-carry-in-closure-before-the-freeze
plan: 03
subsystem: testing
tags: [gate-02, oracle-authority, per-row-pins, identity-rebind, phase09-claims, fixture-manifest, python]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: plan 10-01 preflight and PIN-IMPACT.md, plan 10-02 sweep baseline
  - phase: 07-rdp-and-video-output
    provides: the Phase 7 oracle-authority checker, request record and its blocked local_decision
  - phase: 09-controller-save-media-and-replay
    provides: the ten closed claim blocks of docs/claims/phase09.md and the F2 pins
provides:
  - per-row manifest binding cases in scripts/tests/test_rdp_oracle_authority.py (9 cases against scratch mirrors, RED report saved)
  - the per-row Phase 7 oracle request (39 bound rows by canonical hash, orchestrator edit b64098f) and its identity-only rebind, old a4bb7774... new e131cbe7...
  - BOUND_FILES aligned with EXPECTED_CORPUS_PATHS (13 paths) and the four new fixture-manifest selftest controls (26 total)
  - Phase 9 claim blocks bound to the pinned Phase 9 manifest digest plus a canonical 39-row view, so an appended manifest row leaves harness.phase09-qualification green
  - PIN-IMPACT.md section 8 (GATE-02 record) and section 9 (Phase 9 claim binding, recorded exception, consumer audit a to g)
affects: [10-05, 10-14, 10-19, phase-11]

actuals:
  tokens: 14931
  tasks: 4
  commits: 4

tech-stack:
  added: []
  patterns:
    - "bind a shared data file by per-row canonical hash looked up by id, never by position or whole-file hash, so appended rows move no pin"
    - "pin a closed record to the identity of the file it described (digest constant plus per-row view) instead of the live moving file"
    - "RED report through subTest so one failing accepted case does not hide whether the refused cases already pass"

key-files:
  created: []
  modified:
    - scripts/tests/test_rdp_oracle_authority.py
    - scripts/check-rdp-oracle-authority.py
    - docs/evidence/phase07-oracle-authority.json
    - docs/evidence/phase07-oracle-authority.md
    - scripts/tests/test_phase09_qualification.py
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md

key-decisions:
  - "GATE-02 binds each of the 39 manifest rows present at the rebind by its own _canonical_sha256 in corpus.fixture_manifest.bound_rows, looked up by id; fixtures/manifest.json left EXPECTED_CORPUS_PATHS and bound_authored_inputs; the other 13 bound files keep whole-file hashes"
  - "The Phase 7 request hash was rebound identity-only by the orchestrator (old a4bb7774b72321261e37a44bbb518161b9b564e4beb076dfebe06881aab67733 reproduced first, new e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab); both TRUSTED_*_DECISION_SHA256 stay None, local_decision stays blocked, --require-approved exits 2"
  - "The Phase 9 claim blocks now compare manifest_digest with the pinned qualification-time digest b75864fd... and the per-row view digest beb2b1c8... of the 39 rows that manifest held, instead of sha256 of the live manifest file; docs/claims/phase09.md, docs/evidence/phase09-qualification.md and every F2_PINS literal are byte-identical to the Wave 0 base"
  - "RECORDED EXCEPTION for owner review: the Task 4 comparator change rests on delegated autonomy, ROADMAP SC5, CONTEXT GATE-02 and the 2026-11-23 per-row approval; it is NOT an owner approval of this specific comparator change"

patterns-established:
  - "An appended fixture-manifest row needs no oracle rebind and moves no Phase 9 claim binding; changing, removing or duplicating any of the 39 bound rows is still an orchestrator-only identity-only rebind"
  - "A refusal control needle must be the prefix both the old and the new comparator emit (claims-final:) so RED and GREEN classify the same way; the manifest-only-lines check guards against setup errors"

requirements-completed: [GATE-02]

coverage:
  - id: D1
    description: "Per-row oracle binding contract is executable: appended row, whitespace-only reformat and reordered rows leave the record valid and blocked; a changed bound row, a changed or removed unconsumed row, a duplicated bound id, a row cloned under another id, a missing manifest and an empty or absent row list refuse with the checker's own exit 2"
    requirement: "GATE-02"
    verification:
      - kind: unit
        ref: "python3 -B scripts/tests/test_rdp_oracle_authority.py (9 per-row cases pass); ctest --test-dir build/gate -R '^harness\\.rdp-(oracle-authority|vi-qualification)$' (2 of 2 passed)"
        status: pass
    human_judgment: false
  - id: D2
    description: "The per-row checker and identity-only rebind grant no authority: --expect-blocked exits 0 with 'blocked: no oracle operation is authorized', --require-approved exits 2, both trusted decision pins stay None, selftest passes 26 controls (orchestrator commit b64098f)"
    requirement: "GATE-02"
    verification:
      - kind: integration
        ref: "python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked (exit 0); --require-approved (exit 2); --selftest (26 controls)"
        status: pass
    human_judgment: false
  - id: D3
    description: "BOUND_FILES equals the 13-path EXPECTED_CORPUS_PATHS, the four fixture-manifest selftest controls are in the exact control inventory, and PIN-IMPACT.md records the old and new request hashes and the 39 bound rows"
    requirement: "GATE-02"
    verification:
      - kind: other
        ref: "sh scripts/preflight-phase10.sh (PASS 64 s with the post-gate replay on the clean committed tree); grep of the new PINNED_REQUEST_SHA256 in PIN-IMPACT.md"
        status: pass
    human_judgment: false
  - id: D4
    description: "Appended or moved manifest rows no longer move the Phase 9 claim binding while any change, removal or duplication of a Phase-9-era row, a schema change or a malformed manifest is still refused; closed Phase 9 records and the F2_PINS literal are unchanged"
    requirement: "GATE-02"
    verification:
      - kind: unit
        ref: "ctest --test-dir build/gate -R '^harness\\.phase09-qualification$' (passed; test_claims_final_blocks with 4 accepted and 15 refused manifest controls); Task 4 verify command (git diff --quiet against the Wave 0 base on the three closed files, F2_PINS ast equal)"
        status: pass
    human_judgment: true
    rationale: "The authority basis of the comparator change is delegated autonomy plus ROADMAP SC5, CONTEXT GATE-02 and the 2026-11-23 per-row approval; it is not an owner approval of this specific change, so the owner must review the recorded exception in PIN-IMPACT.md section 9"

duration: 18min
completed: 2026-11-24
status: complete
commits: 4
plan_head_before: 00c5dc186a887b121e562c06977b44c73fa56016
plan_head_after: 9790b7a60c88821de841b654a07b1743331699e1
---

# Phase 10 Plan 03: Per-row oracle manifest pins and the Phase 9 claim binding Summary

**Appending an authored row to fixtures/manifest.json no longer needs an oracle-request rebind or moves any Phase 9 claim: the Phase 7 request pins each of the 39 manifest rows by its own canonical hash (identity-only rebind a4bb7774... to e131cbe7..., authority unchanged), and the ten closed claim blocks are bound to the pinned qualification-time manifest digest plus a 39-row canonical view.**

## Performance

- **Duration:** 18 min (wall clock from the base commit `00c5dc1` at 2026-11-24T01:28:32Z to 2026-11-24T01:46Z; the continuation that did Tasks 3 and 4 took about 9 min)
- **Started:** 2026-11-24T01:28:32Z (commit time of `plan_head_before`; the start of the first segment was not recorded)
- **Completed:** 2026-11-24T01:46:00Z
- **Tasks:** 4 (Tasks 1, 3 and 4 by the executor, Task 2 by the orchestrator)
- **Files modified:** 6 (0 created, 6 modified)

## Accomplishments

- **Task 1 (RED).** Nine per-row binding cases in `scripts/tests/test_rdp_oracle_authority.py`, each on a private scratch mirror under `build/gate/07-12-task1`: append, whitespace-only reformat, bound-row change (sha256, size, source_rights, binary_rights, distribution of `rdp-fill`, sha256 of `rdp-fill-payload`), existing-row change and removal (`hello-openipl3`), reorder, duplicate bound id (identical copy and altered copy first), same bytes under another id, missing manifest, empty and absent row list, `--require-approved`. Against the whole-file checker exactly `append-unrelated-row`, `old-whole-file-mutation` and `reorder-bound-rows` failed (saved in `build/gate/10-03-task1/red.txt`).
- **Task 2 (orchestrator, `b64098f`).** The checker pins each of the 39 manifest rows by `row_sha256` in `corpus.fixture_manifest.bound_rows`, looked up by id; a missing, duplicated or changed bound row and an empty bound set refuse; `fixtures/manifest.json` left `EXPECTED_CORPUS_PATHS` and `bound_authored_inputs`. Old request hash reproduced first, `PINNED_REQUEST_SHA256` rebound to `e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab`, four selftest controls added (26). Not performed by the executor.
- **Task 3 (GREEN).** `BOUND_FILES` now has 13 entries (manifest kept in `MIRROR_EXTRA` so mirrors stay complete) and `expected_controls` lists the four `fixture-manifest-*` controls; all nine cases pass, `harness.rdp-oracle-authority` and `harness.rdp-vi-qualification` pass, `--expect-blocked` exits 0, `--require-approved` exits 2. PIN-IMPACT.md map rows and a new section 8 record the contract, both hashes and the 39 bound rows.
- **Task 4.** `claims_final_problems` compares each block's `manifest_digest` with `sha256:` plus `PHASE09_CLAIM_MANIFEST_SHA256` (`b75864fd...`) and calls the new `claim_manifest_problems`, which looks the 39 pinned ids up by id and compares the canonical view digest `PHASE09_CLAIM_MANIFEST_VIEW_SHA256` (`beb2b1c8...`). Reproduced first on the unchanged tree: file digest, 39 entries, view digest. Appended row, `rdp-fill` moved last and a full reversal are accepted; changed, removed, duplicated or re-id'd rows, schema changes and malformed manifests are refused with lines beginning `claims-final: manifest`. PIN-IMPACT.md section 9 records the exception and the consumer audit (a) to (g).

## Task Commits

1. **Task 1: per-row binding tests, RED against the whole-file checker** - `ff341a0` (test)
2. **Task 2: per-row checker edit and identity-only rebind (ORCHESTRATOR ONLY, not an executor task)** - `b64098f` (fix)
3. **Task 3: align the test with per-row binding, record the contract** - `2c3b5aa` (test)
4. **Task 4: Phase 9 claim-block manifest binding** - `9790b7a` (test; controls, comparator and PIN-IMPACT section 9 in one commit, RED evidence is the saved report)

**Plan metadata:** the SUMMARY commit and the STATE, ROADMAP and REQUIREMENTS updates follow in docs(10-03) commits.

## Files Created/Modified

- `scripts/tests/test_rdp_oracle_authority.py` - nine per-row binding cases, `MIRROR_EXTRA`, 13-path `BOUND_FILES`, 26-control inventory
- `scripts/check-rdp-oracle-authority.py` - per-row binding and rebound `PINNED_REQUEST_SHA256` (orchestrator, `b64098f`)
- `docs/evidence/phase07-oracle-authority.json` and `.md` - `corpus.fixture_manifest.bound_rows`, rebind record (orchestrator, `b64098f`)
- `scripts/tests/test_phase09_qualification.py` - `PHASE09_CLAIM_MANIFEST_SHA256`, `PHASE09_CLAIM_MANIFEST_VIEW_SHA256`, `PHASE09_CLAIM_MANIFEST_IDS`, `claim_manifest_problems`, the new comparison in `claims_final_problems`, 19 manifest controls in `test_claims_final_blocks`
- `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` - map rows 15 and 17, sections 8 and 9

## Decisions Made

See key-decisions. In short: per-row pins by id for the Phase 7 request (orchestrator), the same idea for the Phase 9 claim blocks (pinned identity plus a per-row view), no closed record re-pinned, and the Phase 9 comparator change surfaced as a recorded exception rather than presented as an owner approval.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical] Task 4 controls beyond the plan's list, and two comparator guards**
- **Found during:** Task 4 (writing the controls)
- **Issue:** the plan lists changed, removed, duplicated rows, schema_version and the block digest. A comparator that accepts garbage or a boolean `schema_version`, or that crashes on a missing manifest, would be a correctness hole in a refusal gate.
- **Fix:** added controls for `schema` changed, `schema_version` as `true`, a profile field change, a duplicated or altered-copy-first `rdp-fill`, a row cloned under another id, no rows, `entries` not a list, invalid JSON and a non-object manifest, plus an all-lines-mention-manifest check so a setup error cannot pass as a refusal. The comparator rejects a boolean `schema_version` and reports an unreadable manifest as a `claims-final: manifest` line instead of raising.
- **Files modified:** `scripts/tests/test_phase09_qualification.py`
- **Verification:** `harness.phase09-qualification` passes; mutation check in a scratch copy: removing the view-digest comparison fails the three changed-row controls, a count test that allows duplicates fails the duplicated-row control, restoring the live-file digest fails the three accepted controls (the schema check is also covered by the view digest, so removing it alone is not detectable, which is why it is kept only for a clear message)
- **Committed in:** `9790b7a`

**2. [Rule 3 - Blocking] RED report made through `subTest`**
- **Found during:** Task 4 (first RED run design)
- **Issue:** one unittest method stops at its first failing assertion, so the first failing accepted case would hide whether the refused controls already pass, which the plan requires the RED report to show.
- **Fix:** each manifest control runs in its own `subTest`; the RED report lists three failing accepted subtests (appended row and both reorder variants) and every refused control passing.
- **Files modified:** `scripts/tests/test_phase09_qualification.py`
- **Committed in:** `9790b7a`

---

**Total deviations:** 2 (1 Rule 2, 1 Rule 3)
**Impact on plan:** both inside the two files the task names; no closed record, authority pin or F2 literal touched.

## Issues Encountered

- The plan expects two failing accepted cases at RED; the reorder case has two variants (`rdp-fill` moved last, every row reversed), so three accepted subtests failed, all with "manifest_digest ... differs". No refused control failed at RED, so there was no needle-setup error.
- Audit row (f): `check-base-evidence.py --check-current` exits 0 in this checkout (it reads the local `build/private-base-diagnostic` batches) but exits 2 in a clean clone of the base, where those private artifacts do not exist; `--check-artifacts` exits 2 in both. Row (e): `check-rsp-evidence.py --check` exits 1 at the Wave 0 base and now (recorded `verification_sha256` `be5b9776...`, computed `97d76933...` at the base), a pre-existing failure. Neither was caused or fixed here.
- `docs/evidence/phase07-oracle-authority.md` dates the rebind section 2026-11-24 (the UTC date of the commit) while the owner approval is dated 2026-11-23 (local date); no action taken, the orchestrator-owned record was not edited.

## Authentication Gates

None. Task 2 was an authority gate by design (orchestrator-only checker edit and rebind), completed by the orchestrator before this continuation.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None. T-10-05 mitigated (the executor made no edit to the checker, the request record or any pin; `git log` shows only `b64098f` touching them; both trusted decision pins unchanged and `None`; `--require-approved` exits 2). T-10-06 mitigated (bound rows checked by id and canonical hash; duplicate, missing, same-bytes-other-id and empty cases refuse). T-10-07 mitigated (the `.md` record carries old and new hash, date and "grants no oracle, rights or publication authority"). T-10-49 mitigated (pinned Phase 9 digest plus a 39-row view; closed records and `F2_PINS` proven unchanged against the Wave 0 base; recorded exception in PIN-IMPACT.md).

## Recorded exception for owner review

Task 4 changed how the ten closed Phase 9 claim blocks bind the manifest (test code only). Its basis is delegated autonomy for routine reversible engineering, ROADMAP Phase 10 SC5, CONTEXT GATE-02 and the 2026-11-23 owner approval of per-row binding. It is **not** an owner approval of this specific comparator change. It is reversible (reverting restores whole-file binding), edits no closed record, claim, tier, count, oracle revision or F2 literal, and is described in PIN-IMPACT.md section 9 with the rejected alternative (re-pinning the closed blocks, which would be an orchestrator-only identity-only task). The plan 10-19 summary and the claim documents must surface it to the owner.

## Next Phase Readiness

- Plan 10-14 may append authored rows to `fixtures/manifest.json` with no oracle rebind and without moving `harness.phase09-qualification`; it must append (or reorder) only, since changing, removing or duplicating any of the 39 bound rows refuses in both places.
- The 13 other oracle-bound files still carry whole-file hashes: an unavoidable edit of one is one batched orchestrator identity-only rebind recorded in PIN-IMPACT.md.
- Plan 10-05 should re-check the Task 4 comparator in its review (T-10-49) and surface the recorded exception.
- No blockers; no defect found in earlier plans.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-24*

## Self-Check: PASSED

All six modified files and the three saved reports (`build/gate/10-03-task1/red.txt`, `build/gate/10-03-task4/red.txt`, `build/gate/10-03-task4/green.txt`) exist; the four task commits (`ff341a0`, `b64098f`, `2c3b5aa`, `9790b7a`) are ancestors of HEAD; plan-level checks passed on the committed tree: `ctest -R '^harness\.rdp-(oracle-authority|vi-qualification)$'` 2 of 2, `ctest -R '^harness\.phase09-qualification$'` passed, `--expect-blocked` exit 0, `--require-approved` exit 2, `--selftest` 26 controls, the Task 4 verify (closed Phase 9 records and manifest `git diff --quiet` against the Wave 0 base `4ac633e`, `F2_PINS` ast equal), `grep -c PHASE09_CLAIM_MANIFEST_VIEW_SHA256` 3, the `_f2_digest(root, "fixtures/manifest.json")` count 0, the `EXPECTED_CORPUS_PATHS` manifest count 0, `BOUND_FILES` 13 entries, and `sh scripts/preflight-phase10.sh` PASS (64 s, replay on).
