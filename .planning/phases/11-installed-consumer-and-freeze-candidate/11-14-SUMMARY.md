---
phase: 11-installed-consumer-and-freeze-candidate
plan: 14
subsystem: api
tags: [bound-commit, oracle-rebind, identity-only, shape-wave, dev-11-01, authority-gate, orchestrator]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: plans 11-12 (the flip and the scratch-proven bound patch) and 11-13 (the struct_size prologues), the DEV-11-01 window and the owner approvals of 2026-11-25
provides:
  - the bound patch re-proven at the 11-13 head in a marked scratch clone (applied unchanged, clone gate stops at ctest with exactly the two identity-block names failed, core-lint passes, sweep 0 differences)
  - the isolated, orchestrator-only bound commit e66625a (nine bound test files at API call sites only, three authority files, PINNED_REQUEST_SHA256 rebound identity-only), the last commit of the shape wave
  - closure of the DEV-11-01 window: every target in the shared tree builds again and the full gate passes with no name excluded
  - the closing PIN-IMPACT record (hashes, gate seconds, sweep line, token-preservation finding)
affects: [11-15, 11-16, 11-17, 11-19]

actuals:
  tokens: 19900
  tasks: 2
  commits: 3

plan_head_before: 7632f4417c7722dc290c23b14cdad9c7b102be43
plan_head_after: 519f8f4abfe5ca5e332d3b6a982ee673713f626c

tech-stack:
  added: []
  patterns:
    - "bound-file edit protocol: reproduce the old pinned hash, apply only the scratch-proven call-site patch, rehash exactly the edited corpus rows, change only the pin line, record old and new hash in the authority record, one isolated commit"
    - "a gate run that redirects its output must write it outside the clone tree or under an ignored path, because the PII scan reads untracked files of the tree"

key-files:
  created: []
  modified:
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md
    - core/tests/test_rdp.c
    - core/tests/test_rdp_copy.c
    - core/tests/test_rdp_triangles.c
    - core/tests/test_rdp_texture.c
    - core/tests/test_rdp_combiner.c
    - core/tests/test_rdp_blender.c
    - core/tests/test_rdp_depth.c
    - core/tests/test_vi.c
    - core/tests/test_video_output.c
    - scripts/check-rdp-oracle-authority.py
    - docs/evidence/phase07-oracle-authority.json
    - docs/evidence/phase07-oracle-authority.md

key-decisions:
  - "I-1 reading of the owner approval (DR-27): the nine API-using files are edited; runner/tests/test_rdp_video.c is listed in the approval, needs no edit and keeps its hash"
  - "the one category (c) hunk, test_rdp.c line 486 (t64_host_storage image = {NULL, 0u, 0u} to {0}), is covered by the owner's answer A of 2026-11-25 and landed in the same isolated commit"
  - "the rebind is identity-only: local_decision stays blocked, both TRUSTED_*_DECISION_SHA256 stay None, publication stays not authorized; no oracle, rights or publication authority"

patterns-established:
  - "the executor writes only PIN-IMPACT.md around an orchestrator-only authority gate (re-proof before, closing record after)"

requirements-completed: []

coverage:
  - id: D1
    description: "The nine API-using oracle-bound files changed only at API call sites in one isolated commit placed last in the shape wave, after the old pinned hash e131cbe7 was reproduced; runner/tests/test_rdp_video.c is not edited"
    requirement: "API-01"
    verification:
      - kind: integration
        ref: "git show --name-only e66625a lists exactly the nine bound test files and the three authority files; git show e66625a -- scripts/check-rdp-oracle-authority.py changes only the PINNED_REQUEST_SHA256 line; TRUSTED_LOCAL_DECISION_SHA256 and TRUSTED_PUBLICATION_DECISION_SHA256 unchanged (None); independent per-hunk token check over 110 zero-context hunks: only zero initializers (0, 0u, and NULL in the one category (c) hunk) are removed-only"
        status: pass
    human_judgment: false
  - id: D2
    description: "The oracle request is rebound identity-only: the checker stays blocked with the new pin and --require-approved exits exactly 2"
    requirement: "API-02"
    verification:
      - kind: unit
        ref: "python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked (exit 0, 'authority record valid; blocked: no oracle operation is authorized'); --require-approved (exit 2); ctest harness.rdp-oracle-authority and harness.phase10-qualification Passed in the shared-tree gate"
        status: pass
    human_judgment: false
  - id: D3
    description: "The shape change moved no existing digest or expected byte and the full gate passes in the shared tree with no name excluded"
    requirement: "API-01"
    verification:
      - kind: integration
        ref: "python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD: 'sweep: 1599 shared lines, 0 differences, 0 head-only lines' (40 s); sh scripts/gate.sh: '[gate] PASS', total 224 s, ctest 125 s, '100% tests passed out of 275' (output kept at build/11-14-final-gate.out); python3 -B scripts/check-api-init.py: files=183 findings=0"
        status: pass
    human_judgment: false

duration: 22 min
completed: 2026-11-26
status: complete
---

# Phase 11 Plan 14: The Bound Commit and Identity-Only Rebind Summary

**The nine oracle-bound test files now use the new API shape in one isolated, last-of-wave orchestrator commit (e66625a) with PINNED_REQUEST_SHA256 rebound identity-only (e131cbe7... to 75f2b651...); the DEV-11-01 window is closed, the full gate passes with no name excluded (224 s), the digest sweep reports 0 differences over 1599 shared lines, and local_decision stays blocked with both trusted pins None.**

## Performance

- **Duration:** about 22 min wall (19:57 to 20:19 local): Task 1 re-proof 13 min, orchestrator Task 2 about 5 min, closing checks and records about 4 min
- **Tasks:** 2 of 2 (Task 1 tracer re-proof by an executor; Task 2 authority gate performed by the orchestrator)
- **Files modified:** 13 (nine bound test files, three authority files, PIN-IMPACT.md)

## Accomplishments

- **Task 1 (re-proof, commit `2abe207`).** `build/11-12-bound/bound-callsites.patch` (SHA-256 `beb2cafb0a8a66a66e0cdef84a9233cef11973cbd66ed1e84f2f704c50f262cb`, equal to the 11-12 and 11-13 records; 9 files, 97 hunks, not `runner/tests/test_rdp_video.c`) applied unchanged at the 11-13 head in the marked clone `build/11-14-bound/src`. The clone gate stopped at its ctest step with 273 of 275 passed and exactly the two identity-block names failed (`harness.phase10-qualification`, `harness.rdp-oracle-authority`, message `authored corpus SHA-256 changed: core/tests/test_rdp.c`); `core-lint` passed (files=99, violations=0); the sweep reported 0 differences. The old-hash reproduction command and the I-1 reading went into PIN-IMPACT.
- **Task 2 (orchestrator, commit `e66625a6494863ba6921da575fdb86e379a1c76d`).** The old request hash `e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab` was reproduced from the committed record first; the commit changes the nine bound test files at API call sites only (create-info, init macros, record size, and the one value-neutral `t64_host_storage` initializer of test_rdp.c line 486, owner answer A), rehashes exactly those nine rows, and sets `PINNED_REQUEST_SHA256` to `75f2b651394825ade3a2661950b889522a59b03e2fbc21c65c70f271adda2779`. 12 files, 249 insertions, 152 deletions, no deletion of a file. The orchestrator's hunk-token comparison showed every numeric and identifier token preserved.
- **Closing checks (this continuation, all run in the shared tree at `e66625a`).**
  - `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` exits 0 with `authority record valid; blocked: no oracle operation is authorized`; `--require-approved` exits exactly 2.
  - `sh scripts/gate.sh` post-rebind (measured by the orchestrator, output `build/11-14-final-gate.out`, not re-run because nothing was inconsistent): `[gate] PASS`, total 224 s, ctest 125 s (`100% tests passed out of 275`, both identity-block names Passed again), core-lint 16 s.
  - `python3 -B scripts/check-api-init.py`: `files=183 findings=0 excluded=1` (the one exclusion is `tools/digest-sweep/t64_digest_sweep.c`, excluded by design in the checker; the nine bound files are now scanned and clean).
  - `python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD`: `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (40 s).
- **Acceptance checks.** `git show --name-only e66625a` lists exactly the nine bound test files and the three authority files; `git show e66625a -- scripts/check-rdp-oracle-authority.py` changes only the `PINNED_REQUEST_SHA256` line; the `TRUSTED_LOCAL_DECISION_SHA256` and `TRUSTED_PUBLICATION_DECISION_SHA256` lines are unchanged and `None`.
- **Independent token-preservation re-check.** Over `git show -U0 e66625a` for the nine files (110 zero-context hunks) the token multiset of removed lines minus added lines per hunk contains only the zero literals `0` and `0u` and, in the one category (c) hunk, `NULL`: zero initializers now supplied by an `*_INIT` macro or `{0}`. No non-zero numeric literal, expected value, case name or non-API identifier is removed in any hunk.

## Task Commits

1. **Task 1: re-prove the bound patch at the 11-13 head** - `2abe20724bd53a2723fe4a4e927b320e5e51e8e4` (docs), `PIN-IMPACT.md` only
2. **Task 2: isolated bound-file commit and identity-only rebind (orchestrator, owner approvals 2026-11-25)** - `e66625a6494863ba6921da575fdb86e379a1c76d` (fix), nine bound test files and three authority files; this closes the DEV-11-01 window
3. **Closing record** - `519f8f4abfe5ca5e332d3b6a982ee673713f626c` (docs), `PIN-IMPACT.md` only (bound commit, hashes, gate and sweep lines, DEV-11-01 closure, token-preservation finding)

**Plan metadata:** the summary and state commits that follow this file. `commits: 3` is `git rev-list --count 7632f44..<head at write time>`; `plan_head_before` is the parent of the Task 1 commit (no per-plan ledger file exists for 11-14: the Task 1 executor did not write one).

## Decisions Made

Binding decisions are DR-22 (the DEV-11-01 window), DR-27 (the I-1 reading and the category (c) hunk) and the owner approvals of 2026-11-25 recorded in `.planning/STATE.md`. This continuation made no new decision.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] The Task 1 verify redirect made `gov.pii.repo` a third failing name in the clone (found by the Task 1 executor)**
- **Found during:** Task 1, first clone gate run
- **Issue:** the plan's verify redirects `sh scripts/gate.sh > gate.out` inside the clone. `scripts/pii-scan.sh` scans the untracked, non-ignored files of the tree and `gate.out` carries the clone path in its cmake and ctest banner lines, so `gov.pii.repo` failed with two `P-PRIVATE-PATH` hits, a failure outside the expected two names
- **Fix:** the second run wrote the gate output outside the clone tree and `gov.pii.repo` passed; recorded in PIN-IMPACT "Plan 11-14 evidence" with the rule that any redirect must go to an ignored or external location. The orchestrator's post-rebind gate ran in the shared tree and wrote its output under the ignored `build/` directory, so it hit no such failure
- **Files modified:** none (the plan's verify text is the defect; PIN-IMPACT records it)
- **Commit:** `2abe207`

### Other differences from the plan text (not rule deviations)

- **Task 2 was done by the orchestrator, not the executor**, as the plan requires (authority gate, owner approvals of 2026-11-25); this continuation only verified and recorded it.
- **The gate was not re-run by the continuation.** The post-rebind gate result (PASS, 224 s) was measured by the orchestrator at the bound commit with the output kept; nothing in the tree changed afterwards except PIN-IMPACT.md, so the continuation cites it. The authority checker, the init guard and the digest sweep were run by the continuation.
- **The sweep base is the full 40-hex id** (the checker refuses the 12-hex abbreviation that PIN-IMPACT prints).

**Total deviations:** 1 auto-fixed (Rule 1, a plan verify defect found in Task 1). **Impact:** none on the digests, the bound patch or the rebind.

## Authentication Gates

None. Task 2 was an orchestrator-only authority gate (owner approvals of 2026-11-25), not an authentication gate.

## Issues Encountered

- **Unrelated host load.** The closeout checks ran at load averages 15.5, 18.1 and 14.2 from an unrelated project on the same machine; the authority checker, the init guard and the sweep are not wall-limited gate stages and passed; no limit was widened.

## Open items for later plans (not acted on here)

- 11-15 (SHAPE review, first push): the first push is allowed now that every head compiles; run `sh scripts/preflight-phase11.sh` on a clean committed tree first. Carry the open review candidates recorded in 11-03, 11-04, 11-08, 11-09 and 11-13 (see the running notes and the 11-13 summary); none was touched here.
- The authority record and the checker now carry the second identity-only rebind in `docs/evidence/phase07-oracle-authority.md`; a future bound-file API edit needs its own owner-approved orchestrator commit.

## Verification Results

| Check | Result |
|-------|--------|
| Task 1 clone gate (`2abe207`) | stops at ctest, exactly the two identity-block names failed, 273 of 275; core-lint 0 violations; sweep 0 differences |
| `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` | exit 0, `authority record valid; blocked: no oracle operation is authorized` |
| `python3 -B scripts/check-rdp-oracle-authority.py --require-approved` | exit 2 (`oracle-authority: no pinned competent owner/legal decision; oracle operations remain blocked`) |
| `sh scripts/gate.sh` at `e66625a` (orchestrator, `build/11-14-final-gate.out`) | `[gate] PASS`, 224 s total, ctest 125 s (275 of 275), core-lint 16 s, no name excluded |
| `python3 -B scripts/check-api-init.py` | `files=183 findings=0 excluded=1` (exit 0) |
| `python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD` | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (40 s) |
| `git show --name-only --format= e66625a` | exactly nine bound test files and three authority files |
| `git show e66625a -- scripts/check-rdp-oracle-authority.py` | one line changed: `PINNED_REQUEST_SHA256` e131cbe7... to 75f2b651...; both `TRUSTED_*_DECISION_SHA256` lines unchanged (`None`) |
| per-hunk token check (`-U0`, 110 hunks) | only zero initializers removed-only; no non-zero literal or non-API identifier lost |
| `sh scripts/pii-scan.sh --tree` | `findings=0` |

## Known Stubs

None.

## Threat Flags

None. T-11-22 (elevation of privilege on the authority checker and record) is mitigated: orchestrator-only Task 2, old hash reproduced, only the pin line and the nine corpus rows changed, `--require-approved` exits 2. T-11-23 (tampering with bound-file test semantics) is mitigated by the scratch-proven patch re-proven at this head, the per-hunk category review and the token-preservation checks.

## Next Phase Readiness

The shape wave is complete (11-02 to 11-14): the bound files changed only at call sites in one isolated commit, the request is rebound identity-only, the full gate passes, no digest moved and the DEV-11-01 window is closed. API-01 and API-02 are not complete (the review, the installed consumer, the installed-object lint, the sanitizer harness and the freeze record are ahead), so no requirement is marked complete. Nothing was pushed; plan 11-15 holds the first push.

## Self-Check: PASSED

- FOUND: .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md, build/11-14-final-gate.out, build/11-12-bound/bound-callsites.patch
- FOUND commits (ancestors of HEAD): 2abe207, e66625a, 519f8f4
