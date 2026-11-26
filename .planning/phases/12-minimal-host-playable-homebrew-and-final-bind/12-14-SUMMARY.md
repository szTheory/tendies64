---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 14
subsystem: abi-final-bind
tags: [abi, final-bind, freeze-record, coupled-commit, scratch-clone, cmake-package, version-pins, header-settle, d6]

requires:
  - phase: 12-13
    provides: the bound freeze minor rule of check-abi.py (bound_minor, candidate_amendments), the re-measured Bind pin-site map and the 10-file Bind staging list
  - phase: 12-12
    provides: the closed HOST review with no amend row ("12-11 trigger: no") and the Bind base 49d41ac
provides:
  - the final ABI bind at 1.0 in one coupled commit 03ccb01 (10 files), proven by a full gate in a scratch clone before the commit
  - docs/abi/FREEZE-CANDIDATE.md bound (abi_version 1.0, package_version 1.0.0, bound_minor 0, candidate_amendments 0), with the rule after the final bind, the accepted limits S-05 and I-05 and the "Owner-review item (D6)"
  - package 1.0.0 with COMPATIBILITY SameMajorVersion; consumer and host request 1.0; the version controls request 1.1 and 1.0.5
  - the installed header with no "provisional" word and no phase promise (15 text sites settled, FN-03)
  - PIN-IMPACT "Bind steps" (bind commit, parent, patch SHA-256, clone proof, golden hashes, block, D6 restated)
affects: [12-15, 12-16, 12-17, 12-18]

actuals:
  tokens: 9900
  tasks: 4
  commits: 2
plan_head_before: 3cfcbec94d83f8e32e76e8bf76789fe7f59eafa3
plan_head_after: 327f5682d9aa2eb7c31027dbcf1cb6d23ec1edad

tech-stack:
  added: []
  patterns:
    - "prove first, commit second: stage the coupled set, export it once with git diff --cached --binary, record its SHA-256 and the index tree, gate a detached scratch clone with the patch applied, then commit the unchanged index and require the commit's binary diff to hash to the proven patch"
    - "a record whose prose is edited by later plans keeps every non-block line from starting with a lowercase key and a colon, so the block-line diff guard of 12-15 and 12-16 cannot misfire on prose"

key-files:
  created:
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-14-SUMMARY.md
  modified:
    - core/include/tendies64/t64_machine.h
    - core/CMakeLists.txt
    - core/tests/test_api_surface.c
    - docs/abi/layout.c
    - docs/abi/header.sha256
    - docs/abi/FREEZE-CANDIDATE.md
    - tests/installed-consumer/CMakeLists.txt
    - tests/installed-consumer/controls.tsv
    - host/CMakeLists.txt
    - scripts/check-install.sh
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md

key-decisions:
  - "ABI bound at 1.0 (MAJOR 1, MINOR 0), package 1.0.0 SameMajorVersion, record bound_minor 0 and candidate_amendments 0 (12-11 did not trigger); project(VERSION) untouched at 0.0.0; no tag"
  - "1.0 is an ABI identity constant and the package's ABI number, not a release version; tags, release channel and release naming stay unselected (D6), stated in the record as an owner-review item"
  - "The header create check-order comment now states the coded major-1 rule (a caller minor above T64_ABI_VERSION_MINOR is ABI_MISMATCH) instead of the major-0 exact-minor clause, in the bind commit, because any later header text fix would cost a minor"
  - "The eight implicit padding holes stay bound (inserting a reserved member is not additive) and the bus-read value_out span stays an accepted limit (judging it narrows accepted inputs, a break after the bind)"
  - "The record keeps its path and its dated 'Evidence for the candidate' section for 12-15 (FN-04); its title is now 'ABI freeze record'"

patterns-established:
  - "a coupled ABI commit is verified after the fact by hashing git diff --binary of the commit against its parent and comparing with the SHA-256 of the patch the clone gate proved"

requirements-completed: []

coverage:
  - id: D1
    description: "ABI 1.0 constants, package 1.0.0 SameMajorVersion and every version pin site in one commit of exactly the 10 Bind-staging files, with no tag and project(VERSION) untouched"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "plan Task 4 verify (10 files, no tag, git diff --binary 3cfcbec 03ccb01 hashes to 70ef0690...eed2): exit 0; git diff --quiet HEAD -- CMakeLists.txt"
        status: pass
      - kind: integration
        ref: "ctest -R '^(unit\\.core\\.api\\.|installed\\.controls\\.|host\\.pump\\.build$)' 25 of 25; version-mismatch-minor observed '\"1.1\"', -patch '\"1.0.5\"', doctored-header-abi status 34"
        status: pass
    human_judgment: false
  - id: D2
    description: "The staged bind proven by a full gate in a scratch clone of the parent before the commit; no-HLE and digest sweep show no behavior change"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "build/12-14-bind/src: sh scripts/gate.sh PASS 351/351 (248 s); check-rsp-no-hle.py exit 0; digest-sweep.py --base 60b9e3c34fc0 --head WORKTREE: 1599 shared lines, 0 differences"
        status: pass
    human_judgment: false
  - id: D3
    description: "Bound record and goldens agree at ABI 1.0; the header holds no provisional word or phase promise; D6 stated as an owner-review item"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "check-abi.py freeze findings=0 (abi=1.0 package=1.0.0); ctest -R '^(repo\\.abi|install\\.|installed\\.|api04\\.|unit\\.core\\.api\\.|repo\\.rsp-no-hle$)' 70 of 70; Task 3 joined-text verify exit 0; reserved-word grep 0"
        status: pass
    human_judgment: false

duration: 19min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 14: Final ABI Bind at 1.0 Summary

**The ABI is bound at 1.0 in commit 03ccb01. The commit holds the ABI constants MAJOR 1 / MINOR 0, package 1.0.0 with SameMajorVersion, the moved consumer, host and control requests, the 15 settled header comment sites, the regenerated layout and header goldens, and the bound record (bound_minor 0, candidate_amendments 0). The record states the D6 owner-review item: 1.0 is an ABI identity, not a release version. All of it is exactly the 10 Bind-staging files. Before the commit, the same bytes passed a full 351-name gate in a scratch clone of the parent, with 0 digest differences against the Wave 0 base.**

## Performance

- **Duration:** about 19 min
- **Started:** 2026-11-26T23:05:42Z
- **Completed:** 2026-11-26T23:24Z
- **Tasks:** 4 of 4
- **Files modified:** 10 in the bind commit plus PIN-IMPACT.md (separate commit) and this summary

## Accomplishments

- Task 1 (tracer): the header constants are now 1 and 0. `core/CMakeLists.txt` writes `VERSION 1.0.0 COMPATIBILITY SameMajorVersion`, as a literal with a reworded comment. `layout.c` was regenerated (pins 1, 0, 65536) and the consumer requests 1.0. The reinstalled moved prefix carries `PACKAGE_VERSION "1.0.0"`, and the consumer found it (3 of 3). The tracer verify passed, so expansion went ahead.
- Task 2: `test_api_surface.c` expects 1 and 0. The host requests 1.0, and the two `check-install.sh` messages name `find_package(tendies64 1.0 REQUIRED CONFIG)`. The version controls request 1.1 and 1.0.5. Their detail text is "SameMajorVersion refuses a higher minor" and "...a higher patch". The doctored-header-abi row is unchanged: it doctors the minor from 0 to 1 and gets status 34. The verify passed 25 of 25, and no 0.1-era pin is left.
- Task 3: all 15 header text sites are settled (banner, the 13 "provisional" lines, line 713), and the create-rule line was corrected (deviation 1). The record is rewritten, and the two bound lines were inserted by hand after `amendments:`. `freeze --write` then rewrote `header.sha256` and the block and kept both bound keys. `freeze` reports findings=0, and the verify passed 70 of 70. The no-HLE output is identical to the baseline.
- Task 4: the staged set was exported once (patch SHA-256 `70ef069078c3decc7f7b55beb79fa1cfba6c4da20af4f9d34c0f3f100aa4eed2`, index tree recorded). It was applied to a marked scratch clone of `3cfcbec`, where the gate passed 351/351 in 248 s, no-HLE exited 0 and the sweep found 0 differences. Only after that was it committed in the shared tree. The commit's binary diff hashes to the proven patch. After the commit the subset passed 69 of 69. PIN-IMPACT "Bind steps" was written and committed on its own.

## Task Commits

Tasks 1 to 3 staged edits that only compile green together (the coupled-commit rule). Task 4 proved them and committed them once:

1. **Tasks 1-4: final ABI bind at 1.0 (coupled commit, 10 files)** - `03ccb01c28179522d357d866c1e21aebfade7b0a` (feat); parent `3cfcbec94d83f8e32e76e8bf76789fe7f59eafa3`, the HEAD the scratch clone was taken from
2. **Task 4: Bind steps record (PIN-IMPACT.md only)** - `327f5682d9aa2eb7c31027dbcf1cb6d23ec1edad` (docs, `[skip ci]`)

Plan metadata: the SUMMARY, STATE and ROADMAP commit follows this file.

## The bound record (key lines)

- Status: "bound: ABI 1.0 (package 1.0.0), final bind by plan 12-14 of Phase 12"
- Block: `abi_version: 1.0`, `package_version: 1.0.0`, `amendments: 0`, `bound_minor: 0`, `candidate_amendments: 0`, `header_sha256: b4f5705c87e25f94ac56091f305cd2364acd432210914fc1093cb8e94f992dac`, `public_symbols_sha256: c45ff33c...2d39` (unchanged), `layout_sha256: 7897b5f8b19b74cf1f141ce2cfebbb75abd0c9bb058d3c5be38b78c17dbd7520`, `status_classes_sha256: 570548fc...6e5e` (unchanged), 35 functions, 39 statuses, 8 stop reasons, 14/165/190, `padding_holes: 8` with the eight unchanged rows
- Installed header hash (the line of `docs/abi/header.sha256`): `4524c0c31f37f2512b15f4bd1f7ed223c1dc4120a3a234b779a0d62d6f8020c3` (was `46d07b8f7658...`)
- New sections: "The rule after the final bind" (additive only, as api.md section 13 and G1 define it; each change takes a minor increment, a dated row and a consumer-suite re-run; any incompatible change is a new major with its own record; a pre-bind amendment folds into 1.0 through `candidate_amendments`), "Final bind (Phase 12)", "Accepted limits at the bind" (S-05 holes, I-05 bus-read span) and "Owner-review item (D6)" (1.0 is an ABI identity constant and the package version is the ABI number, not a release version; project(VERSION), tags, release channel and release naming are unselected; no tag or release; owner confirmed on 2026-11-26)

## Files Created/Modified

- `core/include/tendies64/t64_machine.h`: ABI constants 1 and 0; 15 comment sites settled plus the create-rule line; no struct, prototype, status or stop-reason change (1035 to 1024 lines, comment rewraps only)
- `core/CMakeLists.txt`: package 1.0.0 SameMajorVersion and its comment
- `core/tests/test_api_surface.c`: expectations 1 and 0
- `docs/abi/layout.c`: regenerated (three ABI-version pins)
- `docs/abi/header.sha256`, `docs/abi/FREEZE-CANDIDATE.md`: bound record and block
- `tests/installed-consumer/CMakeLists.txt`, `tests/installed-consumer/controls.tsv`: request 1.0; controls 1.1 and 1.0.5
- `host/CMakeLists.txt`: request 1.0 (and its two-line comment reworded, because it said "the final bind of Phase 12 moves it")
- `scripts/check-install.sh`: the two configure-failure messages
- `.planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md`: "Bind steps"

## Decisions Made

See the frontmatter.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Correctness of the bound contract text] Create check-order comment stated only the major-0 minor rule**
- **Found during:** Task 3 (grep of the header for 0.x-specific text)
- **Issue:** The create comment said ABI_MISMATCH is returned for "an info->abi_version whose major differs from T64_ABI_VERSION_MAJOR, or whose minor differs while the major is 0". At major 1 the coded rule (`abi_version_is_accepted`, `caller_minor <= T64_ABI_VERSION_MINOR`) also refuses a caller minor above the library's, and the bound header text would not say so. After the bind, any header text fix moves the header hash and costs a minor.
- **Fix:** The comment now reads "or whose minor is above T64_ABI_VERSION_MINOR". This is a comment-text edit only, inside the plan's prohibition ("only the two ABI constants and comment text move"), and outside the 15 measured sites.
- **Files modified:** `core/include/tendies64/t64_machine.h` (part of the bind commit)
- **Commit:** 03ccb01

### Other deviations (recorded for transparency)

- **The record title** changed from "Freeze-candidate record" to "ABI freeze record". The path is kept (DR-25). "What is not bound" gained one bullet for `project(VERSION)`, the build identity and release versioning, which points to the D6 item.
- **The "Evidence for the candidate" section was left as written.** The plan keeps it for 12-15 (FN-04). Since the bind, its sentence "the header is byte-identical" (to the 4b13a90 tree) is no longer true. 12-15 replaces the section with "Evidence for the bind".
- **A record prose line was rewrapped** so that no line outside the block starts with a lowercase key and a colon ("change: ..." would have matched the `^[-+][a-z_0-9]+: ` guard that 12-15 and 12-16 run over the record diff).
- **Clone procedure:** the clone was marked by `.git/t64-scratch-clone`. Review note T-05 was applied: the fetch URL is the public repository and the push URL is disabled. The index tree (`git write-tree`) was recorded at export and compared before the commit. These are additions to the plan's steps, not changes to them.
- **B-34 was left unedited**, as "Bind staging" decided. Its expectations hold at 1.0 and pass in the gate.
- **Branch:** the commits were made on `main`, as in every earlier sequential plan (`git.allow_default_branch_commits` is true). Nothing was pushed.

## Authentication Gates

None.

## Issues Encountered

- Host load reached 10 to 17 during the clone gate (unrelated processes). No limit was widened and the gate passed on its first run.

## Verification Results

| Check | Result |
|-------|--------|
| Baseline `check-rsp-no-hle.py --build build/gate` (before Task 1) | exit 0 |
| Task 1 verify (`install.setup`, `installed.consumer.build`, `repo.abi-layout`, constant grep); acceptance: SameMajorVersion 1, SameMinorVersion 0, top-level CMakeLists.txt untouched | 3 of 3; 1 / 0 / untouched |
| Task 2 verify (`unit.core.api.*`, `installed.controls.*`, `host.pump.build`, no 0.1 pins) | 25 of 25; no pin left |
| Task 2 acceptance (controls -V) | minor: request 1.1, observed `compatible with requested version "1.1"`; patch: 1.0.5, observed `"1.0.5"`; doctored-header-abi: expected and observed 34 |
| Task 3 verify 1 (provisional count, joined-text phase promises) | 0 and 0 |
| Task 3 verify 2 (`repo.abi*`, `install.*`, `installed.*`, `api04.*`, `unit.core.api.*`, `repo.rsp-no-hle`; bound keys) | 70 of 70; keys present |
| Task 3 acceptance ("Owner-review item (D6)", "not a release version", "padding_holes: 8"; reserved-word grep) | present; 0 |
| `check-abi.py freeze` without `--write` | `abi-freeze: headers=1 abi=1.0 package=1.0.0 ...`, findings=0 |
| Task 4 clone: `sh scripts/gate.sh` | `[gate] PASS`, 351 of 351, ctest 163 s, total 248 s |
| Task 4 clone: no-HLE; digest sweep vs Wave 0 base | exit 0, identical output; 1599 shared lines, 0 differences |
| Task 4 commit verify (10 files, no tag, commit diff SHA-256 equals patch SHA-256) | exit 0 |
| Shared tree after commit (`repo.abi`, `install`, `installed`, `api04`, `unit.core.api`) | 69 of 69 |
| PIN-IMPACT readers (`consumer.literals`, `repo.api07-evaluation`, `repo.installed-case-map`, `repo.stale-freeze*`) | 7 of 7 |
| `sh scripts/preflight-phase12.sh --no-replay` | PASS (80 s) |
| `sh scripts/pii-scan.sh --tree`; `sh scripts/rights-scan.sh --tree` | 0 findings; 0 findings |

## Handoff (12-15, 12-16 and the orchestrator)

- `BIND_COMMIT` = `03ccb01c28179522d357d866c1e21aebfade7b0a` ("feat(12-14): final ABI bind at 1.0"). From here on, `core/include`, the four goldens and the block lines of the record must stay byte-identical (the 12-15 and 12-16 guards).
- 12-15: replace "Evidence for the candidate" (FN-04; its "byte-identical header" sentence is stale now). Bring api.md sections 2.1-2.3 and 13-15 to the bound values. Section 2.2 should match the header's create-rule wording: same major, and a caller minor at most the library minor. The 17 outside-header "until the Phase 11" lines (B-31, B-32) remain, as do H-03 and FN-02.
- 12-16: the abi-major-rule unit case and `installed.controls.version-mismatch-major`. The header now promises that a caller minor above the library's is ABI_MISMATCH, which is what the major-rule case should pin.
- Still open from 12-13: the COST of `repo.abi-freeze-selftest` (it measures about 1.1 s against COST 0.7).
- Nothing was pushed. The next push is the one a plan names (12-17).

## Known Stubs

None.

## Threat Flags

None. T-12-42: the header diff is the two constants and comment text only, `repo.abi-freeze-record` recomputes every golden, and the clone sweep found 0 differences. T-12-43: the record carries the D6 paragraph and the owner-review item, the reserved-word grep returns 0, there is no tag and `project(VERSION)` is unchanged. T-12-44: each control requests 1.1 or 1.0.5 and was observed failing with its own headline. T-12-54: the set was staged, exported, proven in the clone and then committed, and the commit equals exactly the 10 files and the proven patch bytes.

## Self-Check: PASSED

- FOUND: the 10 bind files at HEAD; `docs/abi/FREEZE-CANDIDATE.md` contains `bound_minor: 0`, `T64_ABI_VERSION_MAJOR UINT32_C(1)` is in the header and `SameMajorVersion` is in `core/CMakeLists.txt`
- FOUND commits: 03ccb01, 327f568 (ancestors of HEAD); `git tag --points-at 03ccb01` is empty
