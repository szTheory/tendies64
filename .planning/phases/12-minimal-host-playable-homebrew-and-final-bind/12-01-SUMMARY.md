---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 01
subsystem: infra
tags: [preflight, pin-impact, decision-record, registration-map, bind-pin-sites, libfuturo-pin, gate-growth, owner-review]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: scripts/preflight-phase11.sh, the Phase 11 PIN-IMPACT record (decision-record and ledger row forms, LibFuturo paper read), docs/abi/FREEZE-CANDIDATE.md (candidate ABI 0.1), the measured cold gate-macos 729 s of 900 s (F-136)
provides:
  - scripts/preflight-phase12.sh, the scripted local preflight cited by every Phase 12 plan (39 s with the post-gate replay, 9 s without), and scripts/preflight-phase12-regexes.txt with 14 measured rows and 9 zero rows for the Phase 12 name families
  - PIN-IMPACT.md of Phase 12: Wave 0 base, the 18-row registration map (plus 2 oracle-side rows), the 35-row bind pin-site map with measured counts and multi-line aware proof commands, the LibFuturo re-pin, the 33-row decision record, the 11-row carry-in triage, the empty host-defect and owner-question sections, the Gate-growth ledger with the Phase 12 G4 exception and the plan file-count exceptions
affects: [12-02, 12-03, 12-04, 12-05, 12-06, 12-07, 12-10, 12-11, 12-13, 12-14, 12-15, 12-16, 12-17, 12-18]

actuals:
  tokens: 22100
  tasks: 3
  commits: 3
plan_head_before: 60b9e3c34fc0efc0ff64296cfa31415565b847b2
plan_head_after: 64506afc095d0537ca7c15c931a221536346f850

tech-stack:
  added: []
  patterns:
    - "a Phase N preflight is a copy of the previous one with named deltas; a host-cheap step is skipped with a printed note while no name of its family exists"
    - "a decision record as a numbered table (DR-NN) with source and reversibility columns that every later plan cites and a later change must amend first"
    - "pin-site and registration maps measured by git grep at a recorded base, with the analogue's lines beside the new fixture's additions and one owner plan per row"

key-files:
  created:
    - scripts/preflight-phase12.sh
    - scripts/preflight-phase12-regexes.txt
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md
  modified: []

key-decisions:
  - "Wave 0 base 60b9e3c34fc0efc0ff64296cfa31415565b847b2, taken after git status --porcelain -- .planning printed nothing, so the base commit holds the plans, 12-VALIDATION.md, the research and the context"
  - "Title fixed before any guest or oracle text: profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k, 128x96 RGBA8888 (49,152-byte packets), polled MI VI pending bit as frame lock, runner scenario at most 3 fields under the 4,000,000-tick cap, consumer and pump scenarios of 10 or more fields, oracle-first order (DR-01 to DR-09)"
  - "Host: host/ standalone project t64h with host/pump and an OFF-by-default host/sdl, pump API names (t64h_pump_step, T64H_TICK_NS 16), input mapping table, strict t64.input v1 copy, SDL lane as the local script only, host-side save file omitted (fit test: does not fit one task of 12-09) (DR-10 to DR-21)"
  - "Final bind (DR-25, owner confirmed 2026-11-26): ABI 1.0 / package 1.0.0 SameMajorVersion as an ABI identity, not a release version; block keys bound_minor and candidate_amendments; executed as 12-13 (checker, no ABI byte), 12-14 (the one coupled commit), 12-15 (text), 12-16 (proofs); S-05 and I-05 kept as accepted limits (DR-24); route B (libdragon) a documented costly non-goal (DR-33)"
  - "LibFuturo pin 4177a43e67312e2db88ab21cba13aa7350063105 re-taken from committed objects: unchanged since the Phase 11 read, 1,169 commits, no remote, a local unpublished SHA; the checkout path is not recorded"
  - "Phase 12 G4 exception for owner review: the new names are required SC1 to SC4 evidence, no equal-time offset exists; reversal rule if a measured or projected cold gate-macos exceeds 900 s or the margin falls under 120 s (move non-essential names to a local lane first)"

patterns-established:
  - "Every Phase 12 plan cites sh scripts/preflight-phase12.sh (--no-replay mid-plan, the full run before a push) and the WAVE0_BASE 40-hex literal, never HEAD"
  - "Oracle-side registration sites are listed as O rows owned by the oracle plan, so that every R row of the registration map is owned by the fixture, runner or consumer plans"

requirements-completed: []

coverage:
  - id: D1
    description: "Scripted Phase 12 preflight with one line per step and a total, whose regex table fails on a seeded wrong count"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "sh scripts/preflight-phase12.sh (PASS, total 39 s, replay included, committed clean tree); T64_PREFLIGHT_REGEXES=<copy with ^repo\\.abi expecting 9> sh scripts/preflight-phase12.sh --no-replay (exit 1, count mismatch line)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Registration map and bind pin-site map measured at the Wave 0 base: 18 R rows, 2 O rows, 35 B rows, counts and multi-line aware proof commands"
    requirement: "API-05"
    verification:
      - kind: other
        ref: "grep -c '^| R-' prints 18, grep -c '^| B-' prints 35; a multi-line aware scan of core runner tools docs/spec tests scripts host agrees with the per-file counts; git diff --quiet on core runner tools tests fixtures docs and the oracle checker"
        status: pass
    human_judgment: false
  - id: D3
    description: "Decision record DR-01 to DR-33, carry-in triage, LibFuturo pin, Gate-growth ledger with the G4 exception and the plan file-count exceptions recorded before any production edit"
    requirement: "API-07"
    verification:
      - kind: other
        ref: "grep -c '^| DR-' prints 33; the carry-in id grep prints 9; the 729 s / 900 s grep prints 4; plan rows 8; sh scripts/pii-scan.sh --tree findings=0"
        status: pass
    human_judgment: false

duration: 13min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 01: Preflight, Pin Maps and Decision Record Summary

**Wave 0 of Phase 12: a 39 s scripted preflight, a registration map and a bind pin-site map measured at the recorded Wave 0 base, the LibFuturo pin re-taken, a 33-row decision record that fixes the title, the host and the final bind before any oracle or guest text exists, and the Phase 12 gate-growth exception for owner review; no production source changed.**

## Performance

- **Duration:** 13 min
- **Started:** 2026-11-26T16:03:57Z
- **Completed:** 2026-11-26T16:16:49Z
- **Tasks:** 3 of 3
- **Files modified:** 3 plan files (all created; no production, test, fixture or CMake file)

## Accomplishments

- Tracer (Task 1): `scripts/preflight-phase12.sh` is the Phase 11 preflight with the Phase 12 deltas: the `phase12` label step reads a `PHASE12_TESTS` block of `scripts/check-test-inventory.sh` (printing "no block yet" and passing while it is absent), the guards step also runs `scripts/check-host-includes.sh` when it exists ("not yet present" otherwise) and a `host-cheap` step runs `ctest -R '^(repo\.host|repo\.api07|harness\.title-oracle)'` when that regex matches a name ("skipped: no names yet" otherwise). The regex table has 14 measured rows (verified with `ctest -N -R` before writing) and 9 zero rows with a comment naming the owning plan and the fixture closure each count will include. The full preflight passed (39 s, replay of 11 `ci.yml` steps included); a scratch regex file with one wrong count exits 1 with `count mismatch: ^repo\.abi expected 9 got 8`.
- Registration map and bind pin-site map (Task 2): 18 `R-` rows (every site of a new authored fixture, with the composite analogue's measured lines and one owning plan between 12-03 and 12-05 or every plan), 2 `O-` rows for the oracle plan 12-02, and 35 `B-` rows (2 ABI constants, the 15 header text sites with the banner and line 713, the version pins, the goldens, the checker and seed, the doc and live-text sites, the host request and three observation rows). Measured counts: 13 header lines hold "provisional" (11 one-line phrases, the wrapped site at 914-915, and "(provisional)" at 179), 4 `core` and 13 `docs/spec` lines hold "until the Phase 11" outside the header, and the multi-line aware proofs give 1 (joined line) and 14 (occurrences). The LibFuturo section names the SHA, the commit date and "local, unpublished", with no filesystem path.
- Decision record, triage, ledger (Task 3): DR-01 to DR-33 (costly: DR-01, DR-02, DR-04, DR-10 to DR-13, DR-33; one-way: DR-25), the carry-in triage (T2, T5, T6 deferred to v1.3; FN-02, FN-03, FN-04 done by 12-14 and 12-15; FN-01 and S-05 accepted; OIPL3 no action; G3 count 5, at the limit), the empty "Host-found API defects" and "Owner questions" sections, the Gate-growth ledger (baseline cold 729 s of 900 s, margin 171 s, warm 487 to 592 s; projection about 23 names and 15 to 25 s hosted) with the G4 exception and its reversal rule, and the plan file-count exceptions (8 plan rows, 3 shared bookkeeping files listed once).

## Task Commits

Each task was committed atomically:

1. **Task 1: Preflight tracer, Wave 0 base and regex count table** - `6291262` (chore); its parent is the recorded Wave 0 base
2. **Task 2: Registration map, bind pin-site map and the LibFuturo re-pin** - `a928a77` (docs)
3. **Task 3: Decision record DR-01 to DR-33, carry-in triage, gate-growth ledger and file-count exceptions** - `64506af` (docs)

**Plan metadata:** the summary and state commit that follows this file (docs: complete plan).

The measured `commits:` count of 3 is `git rev-list --count 60b9e3c..64506af` at write time (ledger read from the persisted base file).

## Files Created/Modified

- `scripts/preflight-phase12.sh` - the scripted local preflight (created in Task 1)
- `scripts/preflight-phase12-regexes.txt` - expected `ctest -N -R` counts for every Phase 12 verify regex (created in Task 1)
- `.planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md` - Wave bases, Registration map, Bind pin-site map, API-07 pin, Decision record, Carry-in triage, Host-found API defects, Owner questions, Gate-growth ledger, Plan file-count exceptions (created in Task 1, filled in Tasks 2 and 3)

## Decisions Made

The binding ones are rows DR-01 to DR-33 of PIN-IMPACT.md; the key choices are in the frontmatter. Two details worth stating here: DR-12 writes the deadzone as "an absolute value below 8" and DR-05 as "an absolute axis value of at least 16" instead of the plan's bar notation, because a vertical bar breaks a Markdown table cell; and DR-24 is rated "reversible (until 12-14)" because keeping the eight padding holes can only be reversed before the bind binds them.

## Deviations from Plan

None to the work itself; the plan executed as written. Recorded for transparency (no Rule 1 to 4 deviation arose):

- **Measurement correction M-1.** The plan counts the 13 header lines holding "provisional" as the result of `git grep -n -i provisional -- core/include`. That command prints 15 lines: 13 in `core/include/tendies64/t64_machine.h` and 2 in `core/include/tendies64/t64_facade.h` ("PROVISIONAL, in-tree only"; the facade is not installed and sits outside the freeze record). The 13 and the 15 header text sites are in the installed header only, as the plan states; the pin-site map records the facade lines as not bind sites rather than narrowing the scan.
- **O rows beside the R rows.** The acceptance text says every `R-` row names an owner between 12-03 and 12-05 or "every plan". The two oracle-side sites (the oracle files and the `harness.title-oracle` registration in `runner/CMakeLists.txt`) belong to plan 12-02 (its frontmatter and DR-27 say so), so they are listed in a second table with `O-` rows instead of being dropped or mis-owned; the 18 `R-` rows all satisfy the acceptance text.
- **Plan 12-14 file count.** The plan text of DR-25 says "10 files"; the frontmatter of 12-14 lists 11 files and the file-count exceptions say 11, so the record states 11.
- **Plan 12-09 sits on the band, not above it.** It has 10 files; the plan lists it as the eighth exception row, so it is recorded with the note "on the band".
- **Extra rows beyond the plan's lists.** B-33 (the host project's request, added by 12-07), B-34 (test expectations and two labels that name a minor below that no longer exists at 1.0) and B-35 (the live branch of `abi_version_is_accepted`) were measured and added, as were the carry-in rows DEF-W1-01 and T1 (carried unchanged, which the G3 count of 5 needs). The verify regex counts (9 carry-in ids) are unaffected.
- `requirements-completed` is empty on purpose. The plan frontmatter lists API-05, API-06 and API-07, but this plan delivers records and a preflight, not the host, the title or the evaluation, so marking them complete would be false; the delivering plans (12-02 to 12-10) own that.

### Auto-fixed Issues

None.

## Authentication Gates

None.

## Issues Encountered

- The shell used by the tool is zsh: `${PIPESTATUS[0]}` printed nothing, so the mismatch control's exit status was read from a second run without a pipe (exit 1).
- Findings for the later plans, none a defect of the bound API: `docs/abi/layout.c` holds three ABI pins at lines 245-247 (the research named 245-246), so the bind regenerates `T64_ABI_VERSION == 65536ll` as well; at 1.0 the create tests still refuse every skewed value (`- 1u` becomes a lower major), but `core/tests/test_api_struct_size.c` line 112 and `tests/installed-consumer/ops.c` line 405 name a minor "one below" that no longer exists (B-34, a text-only relabel for 12-14 to judge); the `freeze-amendment-missing` seed anchor `UINT32_C(1)` disappears at 1.0, which 12-13 already generalizes (B-27).

## Verification Results

| Check | Result |
|-------|--------|
| `sh scripts/preflight-phase12.sh` on the committed clean tree at `64506af` | PASS, total 39 s (ctest-counts 23 rows 0 mismatches, abi-cheap 6 s, guards 2 s, post-gate-replay 28 s, linux-compile skipped: no C change since the base), host load 5.6 |
| mismatch control (`T64_PREFLIGHT_REGEXES` with `^repo\.abi` expecting 9) | exit 1, `count mismatch: ^repo\.abi expected 9 got 8` |
| Wave 0 base record | one 40-hex line, `git cat-file -e` true, ancestor of HEAD, `12-01-PLAN.md` and `12-VALIDATION.md` present in the base commit, and `git rev-parse HEAD~1` of the Task 1 commit equals the base |
| `grep -c '^| R-'`, `'^| B-'`, `'^| DR-'` | 18, 35, 33 |
| carry-in id grep (T2, T5, T6, FN-01 to FN-04, S-05, OIPL3) | 9 |
| `grep -cE '729 s\|900 s'`, `grep -cE '[0-9a-f]{40}'` | 4 and 2 (the Wave 0 base and the LibFuturo SHA are the 40-hex lines) |
| zero-row acceptance grep | 3 |
| `sh scripts/pii-scan.sh --tree` before each commit | findings=0 |
| `git diff --quiet HEAD` on core, runner, tools, tests, fixtures, docs and the oracle checker after Tasks 2 and 3 | exit 0 (no production or oracle-bound change) |

## Known Stubs

None.

## Threat Flags

None. The LibFuturo path is not recorded (T-12-01: read through `LIBFUTURO_DIR` with `git show` of committed objects only; the record names the SHA, the date and the status), DR-04 names `fixtures/title/title-spec.md` as the single behavior authority (T-12-02), and the G4 exception text says "for owner review" with its reversal rule (T-12-03).

## Next Phase Readiness

Plan 12-02 (the oracle, written before the guest) can start: it cites the preflight, WAVE0_BASE `60b9e3c34fc0efc0ff64296cfa31415565b847b2`, DR-02 to DR-07 and DR-27, and it adds the `PHASE12_TESTS` block and raises the `^harness\.title-oracle$` row to 1 in the same commit. Nothing in this plan is orchestrator-only or owner-gated; no push is named (the first push a plan names is 12-06). Local commits since `origin/main` (`dc247fa`): the Phase 11 closeout and Phase 12 planning documents, the three task commits above and this summary.

## Self-Check: PASSED

- FOUND: scripts/preflight-phase12.sh, scripts/preflight-phase12-regexes.txt, .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md
- FOUND commits: 6291262, a928a77, 64506af (all ancestors of HEAD)
