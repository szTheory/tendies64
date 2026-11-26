---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 15
subsystem: abi-bind-text
tags: [abi, bind-text, api-spec, relabel, stale-text-guard, evidence-for-the-bind, h-03, fn-02, fn-04]

requires:
  - phase: 12-14
    provides: the bound ABI 1.0 commit 03ccb01, the bound record block and goldens that must stay byte-identical, the handoff list for this plan
  - phase: 12-13
    provides: the re-measured Bind pin-site map (B-28 to B-32) with multi-line aware proof commands
provides:
  - the four production "provisional until the Phase 11 freeze" comments relabeled outside function bodies with line counts kept (B-31)
  - docs/spec/api.md at the bound values (2.1 to 2.3), the H-03 correction in 8.1, the FN-02 advance row, and sections 13 to 16 for the rule after the bind and the accepted limits
  - the 13 docs/spec "until the Phase 11 freeze" lines relabeled in six files (B-32)
  - docs/abi/FREEZE-CANDIDATE.md "Evidence for the bind" (FN-04), prose only
  - scripts/check-stale-freeze-text.sh extended (two deny phrases, docs/spec and docs/abi scanned, header-scoped provisional rule, header joined-text rule, 14 new seeds including seeded-header-joined)
  - PIN-IMPACT "Bind steps" text-settle record
affects: [12-16, 12-17, 12-18]

actuals:
  tokens: 5664
  tasks: 3
  commits: 4
plan_head_before: 186935e049e7678f7ca585656c120b7dc2899479
plan_head_after: 98f8e9ec1643d21a45e38076cbb8f5ff6310c065

tech-stack:
  added: []
  patterns:
    - "a guard that must catch a phrase wrapped across comment lines joins the file (newlines to spaces, then squeezes ' * ' and collapses space runs) before matching, and a seed built from the real wrapped banner proves the collapse step is load-bearing"
    - "a text settle after a one-way bind is checked by three diffs against the bind commit: core/include and the four goldens empty, and no key: value line of the record changed"

key-files:
  created:
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-15-SUMMARY.md
  modified:
    - core/t64_internal.h
    - core/t64_machine.c
    - core/t64_state.c
    - docs/spec/api.md
    - docs/spec/machine-contract.md
    - docs/spec/raw-capture.md
    - docs/spec/replay.md
    - docs/spec/result-protocol.md
    - docs/spec/save-media.md
    - docs/spec/state-container.md
    - docs/abi/FREEZE-CANDIDATE.md
    - scripts/check-stale-freeze-text.sh
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md

key-decisions:
  - "The header-scoped provisional-word rule skips t64_facade.h by exact name: the file is not installed and not in the freeze record, carries two accurate 'PROVISIONAL, in-tree only' lines, and core/include is covered by the bound-diff invariant so it cannot be edited here; the deny phrases and the joined-text rule still scan it, and a seed proves a differently named header is still caught"
  - "api.md 8.1 states the H-03 rule as: a masked handler status consumes its event (the scheduler pops it and moves the tick before the handler runs) so the next advance continues; the permanent stall is the ring-backpressure case; the class table's older sentence is said to name that case only (the golden is not edited)"
  - "Public-surface lines say 'bound by ABI 1.0'; runner and format lines say 'runner-internal' or 'format-versioned' and 'outside the ABI promise'; plan-history clauses stay"

patterns-established:
  - "Evidence for the bind cites the bind commit, its proven patch hash, the clone gate, the sweep, the consumer re-run, the hosted runs of the earlier waves (which ran at the candidate header) and the 12-11 outcome, and says plainly that the hosted pair for the bound head comes with the next push"

requirements-completed: []

coverage:
  - id: D1
    description: "No bound hash moved: core/include and the four goldens equal the bind commit, no key: value line of the record changed, the freeze check without --write has 0 findings"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "git diff --quiet 03ccb01 HEAD -- core/include docs/abi/header.sha256 docs/abi/layout.c docs/abi/public-symbols.txt docs/abi/status-classes.txt (exit 0); git diff 03ccb01 -- docs/abi/FREEZE-CANDIDATE.md | grep -cE '^[-+][a-z_0-9]+: ' prints 0; check-abi.py freeze: abi-freeze headers=1 abi=1.0 package=1.0.0 ... findings=0"
        status: pass
    human_judgment: false
  - id: D2
    description: "Production comments and docs/spec lines relabeled with no phase promise; production comment edits keep their line counts and stay outside function bodies; no sealed digest moved"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "git show --numstat 1256a08: 2/2, 2/2, 4/4; check-rsp-no-hle.py --build build/gate exit 0 after rebuild; git grep -E 'until (the )?Phase 11' over core runner tools docs/spec docs/abi empty; joined-text scan of docs/spec 0"
        status: pass
    human_judgment: false
  - id: D3
    description: "api.md states the bound values, the H-03 correction, FN-02 and the rule after the bind; the record carries Evidence for the bind"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "grep SameMajorVersion docs/spec/api.md; grep 'Evidence for the bind' docs/abi/FREEZE-CANDIDATE.md; reserved-word grep 'stable release|released|1.0 release' on the record prints 0; ctest -R '^(repo\\.abi|repo\\.rsp-no-hle$|unit\\.core\\.api\\.)' 22 of 22"
        status: pass
    human_judgment: false
  - id: D4
    description: "The stale-text guard catches the Phase 11 promise phrases, a wrapped header phrase and a split-phrase banner, and passes the live tree"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "sh scripts/check-stale-freeze-text.sh --selftest: 24 ok lines, failures=0 (seeded-header-joined reports core/include/tendies64/t64_seed_joined.h:joined:); scan files=344 findings=0; three mutants each turn a seed red; the new guard on the 12-13 head tree reports 44 findings"
        status: pass
    human_judgment: false

duration: 14min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 15: Bind Text Settle Summary

**The text around the one-way bind is settled and no bound hash moved. The four production comments and the 13 `docs/spec` lines no longer promise a Phase 11 freeze. `docs/spec/api.md` states ABI 1.0 (constants, the major-1 create rule, `find_package(tendies64 1.0)` with `SameMajorVersion`), the H-03 correction, the FN-02 advance row and the rule after the bind. The record holds "Evidence for the bind" (FN-04). `repo.stale-freeze-text` now catches the Phase 11 phrases in `docs/spec`, `docs/abi` and the installed header, including a phrase split across comment lines.**

## Performance

- **Duration:** about 14 min
- **Started:** 2026-11-26T23:27:32Z
- **Completed:** 2026-11-26T23:42Z (before this summary)
- **Tasks:** 3 of 3 (Task 1 is the tracer; its verify passed and expansion went ahead)
- **Files modified:** 12 outside `.planning` plus PIN-IMPACT.md and this summary

## Accomplishments

- Task 1 (tracer): `core/t64_internal.h` (1), `core/t64_machine.c` (1) and `core/t64_state.c` (2) hold the relabeled comments, each outside a function body with its line count kept. State-container comments say "format-versioned, outside the ABI promise (docs/spec/api.md section 10.4)", and the profile-family comment says a registered id keeps its descriptor under ABI 1.0. `docs/spec/api.md`: the status paragraph and sections 1, 2.1, 2.2, 2.3, 8.1, 12 (advance row), 13, 14, 15 and the section 16 row label are at the bound contract. The create rule is "same major and a caller minor at most the library minor", with the exercised cases and the unexercised older-minor case named. Tracer verify: the 22 ABI, no-HLE and unit API names pass, the core grep is empty and `SameMajorVersion` is present.
- Task 2: the 13 B-32 lines of six `docs/spec` files are relabeled (public-surface lines "bound by ABI 1.0"; runner and format lines "runner-internal" or "format-versioned", "outside the ABI promise"). The wrapped `result-protocol.md` site and its neighbor were rewrapped. The line grep and the joined-text scan both print 0.
- Task 3: "Evidence for the bind" replaces the dated candidate text (bind commit and parent, patch SHA-256, clone gate 351/351, digest sweep 0 differences, the 69-name consumer re-run, the two version controls, the TITLE and HOST hosted runs 45887012740 and 45904452436, the 12-11 outcome). The guard was extended test-first: the 14 new seeds and the rewritten clean-relabel seed went in first (10 seeds red), then the rules (all green).
- PIN-IMPACT "Bind steps" holds the text-settle record (commit `98f8e9e`).

## Task Commits

1. **Task 1 (tracer): core comments and api.md bound sections** - `1256a082aac530cf7103cc61657e7f5a5e58aab0` (docs)
2. **Task 2: docs/spec relabels (13 lines, 6 files)** - `0a8c63b2e45e4aa5650a19a5ba88124741773a49` (docs)
3. **Task 3: Evidence for the bind and the extended guard** - `c5a8ff774cbd9e9717e3cd736443bb47f66ceed3` (docs)
4. **Task 3: PIN-IMPACT text-settle record (planning only)** - `98f8e9ec1643d21a45e38076cbb8f5ff6310c065` (docs, `[skip ci]`)

Plan metadata: the SUMMARY, STATE and ROADMAP commit follows this file.

## The guard (scripts/check-stale-freeze-text.sh)

- Deny phrases (case-insensitive, line scan): the five Phase 10 phrases plus `until (the )?Phase 11` and `the API is Phase 11`. Scan paths: `core runner tools docs/spec docs/abi docs/adr/0002-language-build-and-abi.md` (344 files on this tree; nothing was narrowed; closed `docs/claims` and `docs/evidence` stay out as before).
- Header rules under `core/include/tendies64`: any `provisional` word is "STALE-FREEZE path:line: provisional in the installed header" (one exact-name exemption, see deviation 1); each file joined with `tr '\n' ' '`, then `sed 's/ \* / /g; s/  */ /g'`, matching `until the Phase 11|the API is Phase 11|not a frozen ABI yet` is "STALE-FREEZE path:joined: phrase split across comment lines in the installed header".
- Seeds (24 selftest lines): the 9 old seeds (clean-relabel rewritten to "bound by ABI 1.0, docs/abi/FREEZE-CANDIDATE.md"), 14 new ones (`seeded-until-11`, `seeded-until-the-11`, `seeded-api-is-11`, `seeded-spec`, `seeded-abi-dir`, `seeded-header-provisional`, `seeded-header-other-name`, `seeded-header-wrapped`, `seeded-header-joined`, `seeded-header-joined-until`, `clean-facade-provisional`, `clean-history-11`, `clean-history-record`, `clean-header-bound`) and the empty-scan error. `seeded-header-joined` is the real banner shape (" * it is not a frozen" then " * ABI yet.", no `provisional`).
- Mutants of the guard run on a scratch copy (outside the repository): dropping the space-run collapse turned `seeded-header-joined` and `seeded-header-joined-until` red, removing the facade exemption turned `clean-facade-provisional` red, and dropping `not a frozen ABI yet` from the phrase list turned `seeded-header-joined` red.
- The new guard run on the exported 12-13 head tree (before the bind) reports 44 findings: 27 in the installed header and 17 outside it, so the guard would have caught every site that this plan and 12-14 settled.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The header-scoped `provisional` rule would fail on the live tree because of t64_facade.h**
- **Found during:** Task 3 (design of the rule; PIN-IMPACT M-1 lists the two lines)
- **Issue:** The plan says any `provisional` word in `core/include/tendies64` is a finding. `core/include/tendies64/t64_facade.h` holds "PROVISIONAL, in-tree only" twice (lines 2 and 18). The facade is the runner's header: not installed, not in the freeze record, and its statement is accurate. Editing it is forbidden here (the bound-diff invariant covers all of `core/include`), so the literal rule would make `repo.stale-freeze-text` red with no allowed fix.
- **Fix:** The word rule skips one file by exact name (`HEADER_RULE_EXEMPT='t64_facade.h'`, with a comment saying why). The deny phrases and the joined-text rule still scan the facade. `seeded-header-other-name` (a header with another name holding the word fails), `seeded-header-provisional` (t64_machine.h fails) and `clean-facade-provisional` (the exemption, and a mutant without it turns that seed red) pin the exemption to that one name. No scan path was narrowed.
- **Files modified:** `scripts/check-stale-freeze-text.sh`
- **Commit:** c5a8ff7

### Other deviations (recorded for transparency)

- **api.md edits beyond the named sections:** the status paragraph, the section 1 wording ("freeze candidate" became "the bind"/"the bound contract"), the section 16 row label ("freeze-candidate record" became "freeze record") and a restated section 14 (title "The bound record, the rule after the bind and amendments") went with sections 2.1 to 2.3 and 13 to 15. They said the old candidate state. The section 12 table row and the 35-row count are unchanged.
- **The class table's sentence** "a stall with no progress after a full drain is permanent" (`docs/abi/status-classes.txt`, a golden) is not edited. api.md 8.1 now says that sentence names the ring-backpressure case only.
- **Relabel wording:** `core/t64_machine.c` says "A registered id keeps its descriptor under ABI 1.0 (docs/spec/api.md section 13)" instead of the plan's example phrase, because that is the rule api.md section 13 states for the profile registry.
- **The abi_version_is_accepted comment** ("while the major is 0 ... after a major bump") was left unedited: it is not a Phase 11 promise, it still describes the coded rule, and its first branch is dead at major 1 (the unexercised older-minor case is recorded in api.md 2.2 and 15, B-35).
- **Branch:** commits are on `main` as in every earlier sequential plan (`git.allow_default_branch_commits` is true). Nothing was pushed.

## Authentication Gates

None.

## Issues Encountered

- Host load was 5 to 10 (unrelated processes); no limit was widened. The full `ctest --preset gate` passed on its first run.

## Verification Results

| Check | Result |
|-------|--------|
| Task 1 verify (`repo.abi*`, `repo.rsp-no-hle`, `unit.core.api.*`; core grep; SameMajorVersion) | 22 of 22; empty; present |
| `check-rsp-no-hle.py --build build/gate` after the rebuild | exit 0, same output |
| Task 1 acceptance (numstat per file; `git diff --quiet HEAD -- core/include docs/abi`) | 2/2, 2/2, 4/4; exit 0 |
| Task 2 verify (line grep and joined scan over docs/spec) | 0 and 0 |
| Task 2 acceptance (files of the commit) | six files, all under docs/spec |
| Guard selftest, RED then GREEN | 10 failures with the seeds alone; 0 failures with the rules (24 ok lines) |
| `sh scripts/check-stale-freeze-text.sh` on the tree | files=344 findings=0 |
| Task 3 verify 1 (`^repo\.(stale-freeze-text\|abi-freeze-record)` 4 of 4, seeded-header-joined line, joined pipeline count 1, git grep over `core runner tools docs/spec docs/abi`) | pass; empty |
| Task 3 verify 2 (bound files against the bind commit; no `key: ` line in the record diff) | exit 0, also re-run on the committed HEAD |
| `check-abi.py freeze` without `--write` | `abi-freeze: headers=1 abi=1.0 package=1.0.0 symbols=35 layout=14/165/190 holes=8 statuses=39 stop_reasons=8`, findings=0 |
| `sh scripts/preflight-phase12.sh --no-replay` | PASS (81 s) |
| Full `ctest --preset gate` at the Task 3 head | 351 of 351 (166 s) |
| PIN-IMPACT readers after the record edit (`consumer.literals`, `repo.api07-evaluation`, `repo.installed-case-map`, `repo.stale-freeze*`, `repo.abi-freeze*`, `repo.host*`) | 14 of 14 |
| `sh scripts/pii-scan.sh --tree`; `sh scripts/rights-scan.sh --tree` | 0 findings; 0 findings |
| Reserved-word grep `stable release\|released\|1\.0 release` on the record | 0 |
| COST of the guard names (no CTest edit) | `repo.stale-freeze-text` 0.92 s and its selftest 0.75 s against COST 1 |

## Handoff (12-16, 12-17 and the orchestrator)

- `BIND_COMMIT` stays `03ccb01c28179522d357d866c1e21aebfade7b0a`; the three bound-hash diffs against it are empty at this plan's head. 12-16 adds the abi-major-rule unit case and `installed.controls.version-mismatch-major`; api.md 2.2, 2.3 and 15 say what those cases will exercise (the major-2 request and the refusals of another major are already stated; the older-minor acceptance stays unexercisable while the library minor is 0).
- The record says "The hosted pair for the bound head comes with the next push" (12-17 pushes). The build identity string of the library differs from the bind commit's because the relabeled comments are build inputs; no code and no golden changed.
- Not owned by any plan (B-34, left by 12-14 "for 12-15 or 12-16"; 12-15 and 12-16 do not name it): the two test comments that name "the minor version one below the library's" (`tests/installed-consumer/ops.c`, `tests/installed-consumer/sequence.c`) and the comment "while the major is 0 the minor must match" in `core/tests/test_api_struct_size.c`. The expectations hold at 1.0. A relabel is a test comment edit with no bound hash; the orchestrator can add it to 12-16.
- Observation for a plan that may edit `core/include`: the comment of `core/include/tendies64/t64_facade.h` (a non-installed header) still says the public header is "a 0.x freeze candidate bound by Phase 11 (final ABI bind at the end of Phase 12)". It matches no deny phrase and sits outside the freeze record, but it is stale text.
- Optional hardening, not in the plan: the joined-text rule covers only the header directory; a phrase wrapped across lines in `docs/spec` is caught today only by the manual joined scan of this plan's Task 2 verify.
- Nothing was pushed; no tag.

## Known Stubs

None.

## Threat Flags

None. T-12-45: the new deny phrases, the header-scoped rule, the joined-text rule and 14 seeds are in `repo.stale-freeze-text` and its selftest, and the guard reports the pre-bind tree. T-12-55: comments sit outside function bodies with their line counts kept, the no-HLE check prints the same output, the freeze check without `--write` has 0 findings, and the diffs of the bound files and the block lines against the bind commit are empty. T-12-SC: no package install.

## Self-Check: PASSED

All 13 modified files and this summary exist, the four commits are ancestors of HEAD, and at HEAD the three bound-hash diffs against the bind commit are empty and no `key: value` line of the record changed.
