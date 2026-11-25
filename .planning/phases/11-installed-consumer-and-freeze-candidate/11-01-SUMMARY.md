---
phase: 11-installed-consumer-and-freeze-candidate
plan: 01
subsystem: infra
tags: [preflight, pin-impact, decision-record, abi, struct-size, digest-sweep, owner-gate]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: scripts/preflight-phase10.sh, the Phase 10 PIN-IMPACT ledger (cold gate-macos 731 s of 900 s), the carry-in list, the sweep printer and the oracle-bound corpus
provides:
  - scripts/preflight-phase11.sh, the scripted local preflight cited by every Phase 11 plan (41 s without replay), and scripts/preflight-phase11-regexes.txt with 8 measured rows and 11 zero rows for the Phase 11 name families
  - PIN-IMPACT.md: Wave 0 base, bound-file inventory measured by a scratch compile probe, public-struct call-site counts, class-table inventory (34 statuses, 8 stop reasons), 13-row pin-impact map, the resolved owner question, the 32-row decision record, the 13-row carry-in triage, the LibFuturo paper read and the Gate-growth ledger with the Phase 11 G4 exception
  - tools/digest-sweep/t64_digest_sweep.c compiling one source against both API generations (head path selected by defined(T64_MACHINE_CREATE_INFO_INIT))
affects: [11-02, 11-03, 11-04, 11-05, 11-12, 11-14, 11-15, 11-22, phase-12]

actuals:
  tokens: 25800
  tasks: 4
  commits: 5
plan_head_before: f9124ae9fce134f31b645c6f2911a6f27fa41b9a
plan_head_after: c1ef5c88351cbf9e40a927b14fb990a15d9044fc

tech-stack:
  added: []
  patterns:
    - "scratch-clone compile probe of a planned public header against the oracle-bound files, classifying every hunk (a) call expression, (b) object initializer, (c) other"
    - "one defined() selector for two API generations in a tool source, with local SWEEP_* macros so only one place branches"
    - "decision record as a numbered table (DR-NN) that every later plan cites and that a later change must amend first"

key-files:
  created:
    - scripts/preflight-phase11.sh
    - scripts/preflight-phase11-regexes.txt
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md
  modified:
    - tools/digest-sweep/t64_digest_sweep.c

key-decisions:
  - "Owner answer A (2026-11-25): the call-site-only rebind approval also covers the single value-neutral initializer update core/tests/test_rdp.c:486; t64_host_storage keeps its leading struct_size (DR-06 INFO-03: the core fills it before each allocate call); the approval is not widened (DR-27)"
  - "Statuses 34 to 38 (ABI_MISMATCH, STRUCT_TOO_SMALL, REENTRANT, POISONED, INTERNAL_DEFECT) appended after 33; ABI constants 0.1; five new public functions (35 total); create-info handshake; init macros; retained slots t64_vi_packet_slot (488) and t64_ai_packet_slot (280); prefix t64i_ (DR-01 to DR-20)"
  - "DEV-11-01 (DR-22): from the 11-12 flip to the 11-14 bound commit the nine bound test executables do not build in the shared tree; every other target builds, no push in the window, a marked scratch clone with the bound patch gates every code commit"
  - "Carry-ins: W2-01, W1-03, D-10-17-01, T8 done in 11-22; W2-02, T4, T7 done in 11-11; T3 done in Phase 10; DEF-W1-01 deferred to v1.3; T1 deferred beyond v1.2; T2, T5, T6 deferred to Phase 12"
  - "LibFuturo paper read at local commit 4177a43 (not published): no locked decision needs amending, any adapter is optional and does not shape the core API"
  - "Phase 11 G4 exception for owner review: new names are required SC1 to SC5 evidence, no offset, reversal rule if a review projects a cold gate-macos run above 900 s"

patterns-established:
  - "Every Phase 11 plan cites sh scripts/preflight-phase11.sh (--no-replay mid-plan, full run before a push) and the Wave 0 base 40-hex literal, never HEAD"
  - "A required bound-file edit outside call-site categories becomes an owner question and stops the plan before the decision record"

requirements-completed: []

coverage:
  - id: D1
    description: "Scripted Phase 11 preflight with per-step lines and a total, whose regex table fails on a seeded wrong count"
    requirement: "API-01"
    verification:
      - kind: integration
        ref: "sh scripts/preflight-phase11.sh --no-replay (PASS, total 41 s); T64_PREFLIGHT_REGEXES=<copy with one count edited> sh scripts/preflight-phase11.sh --no-replay (exit 1, count mismatch line)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Bound-file inventory proving the planned shape needs only call-site edits (17 category a, 120 category b, 1 approved category c) in the nine API-using bound files and none in runner/tests/test_rdp_video.c"
    requirement: "API-03"
    verification:
      - kind: other
        ref: "scratch compile probe of 11 units against the planned-shape header, recorded in PIN-IMPACT.md Bound-file inventory; git diff --quiet on core runner tools tests, the oracle checker and docs/evidence"
        status: pass
    human_judgment: false
  - id: D3
    description: "Decision record DR-01 to DR-32, carry-in triage of 13 items, LibFuturo paper read and Gate-growth ledger recorded before any production edit"
    requirement: "API-02"
    verification:
      - kind: other
        ref: "grep -c '^| DR-' PIN-IMPACT.md prints 32; carry-in row grep prints 13 for both the id and the disposition pattern; sh scripts/pii-scan.sh --tree findings=0"
        status: pass
    human_judgment: false
  - id: D4
    description: "Digest-sweep printer that compiles against both API generations and reports zero differences when base and head are the same production source"
    requirement: "API-03"
    verification:
      - kind: integration
        ref: "python3 -B scripts/digest-sweep.py --base <Wave 0 base> --head WORKTREE (1599 shared lines, 0 differences); cc -fsyntax-only of the printer against the planned-shape header (0 diagnostics)"
        status: pass
    human_judgment: false

duration: 29min
completed: 2026-11-25
status: complete
---

# Phase 11 Plan 01: Preflight, Pin Inventory and Decision Record Summary

**Wave 0 of Phase 11: a 41 s scripted preflight, a bound-file probe proving the planned struct_size shape needs only call-site edits (one extra initializer hunk, owner-approved), a 32-row decision record, and a sweep printer that spans both API generations; no production source changed.**

## Performance

- **Duration:** 29 min (Tasks 1 and 2 by the first executor, then a pause at the owner question; Tasks 3 and 4 and this summary by the continuation executor)
- **Started:** 2026-11-25T19:04:18Z
- **Completed:** 2026-11-25T19:33:26Z
- **Tasks:** 4 of 4
- **Files modified:** 4 plan files (the orchestrator's owner-approval record, commit `238583e`, also touched STATE.md and 11-CONTEXT.md)

## Accomplishments

- Tracer (Task 1): `scripts/preflight-phase11.sh` is the Phase 10 preflight with the Phase 11 deltas; it prints one `[preflight]` line per step and a total (oracle-authority, ctest-counts, abi-cheap, guards, host-load, head-ci, timeout-audit, pii, linux-compile in the act image, post-gate-replay). The regex table has 8 measured rows and 11 zero rows that prove the Phase 11 families are absent until their plans register them. The Wave 0 base `f9124ae9fce134f31b645c6f2911a6f27fa41b9a` is recorded in PIN-IMPACT.md and is the parent of the Task 1 commit.
- Inventory (Task 2): a scratch clone with the planned public header compiled the nine API-using bound files plus the second unit of `test_rdp.c` and `runner/tests/test_rdp_video.c`. 154 diagnostics unchanged; the edit set is 17 call expressions (a), 120 object initializers or declarations (b) and 1 other (c), in 110 hunks; `test_rdp_video.c` compiled unchanged and needs no edit. The (c) hunk (`core/tests/test_rdp.c:486`) raised the owner question; the owner answered A on 2026-11-25 and it is now an approved hunk for the isolated commit of plan 11-14. The same task recorded the public-struct call-site counts (155 `t64_machine_create` call sites in 74 files), the class-table inventory of 34 statuses and 8 stop reasons, and a 13-row pin-impact map (the sha256 archive-member hazard for 11-16, the `nm` literal-symbol hazard of `check-rsp-no-hle.py` for 11-11, four sealed public observers).
- Decision record (Task 3): DR-01 to DR-32 fix the status names and values 34 to 38, the ABI constants, `T64_API`, the five new functions, the create-info and its check order, the `struct_size` set with the INFO-03 exception for `t64_host_storage`, fixed-width members, the retained slots, the init macros, the poison scope and defect sites, the class table, the `t64i_` prefix, the stack-protector allowlist, the package recipe, the CTest names by plan, DEV-11-01, the hosted footprint, the G4 position, the call-site rule, the threading statement and the composite three-way scope. The carry-in triage has 13 rows, each with one disposition, a reason and an owning plan. The LibFuturo read is at local commit `4177a43` (not published) and amends no locked decision. The Gate-growth ledger starts from the measured cold gate-macos run of 731 s of 900 s (margin 169 s) and records the Phase 11 G4 exception with a reversal rule.
- Sweep printer (Task 4): `tools/digest-sweep/t64_digest_sweep.c` selects its head path with `#if defined(T64_MACHINE_CREATE_INFO_INIT)` and branches in one place (`SWEEP_ALLOCATOR_INIT`, `SWEEP_STOP_REQUEST_INIT`, `SWEEP_STOP_RESULT_INIT`, `sweep_create`). The base path reports 1599 shared lines and 0 differences; the head path compiles with 0 diagnostics against the planned-shape header.

## Task Commits

Each task was committed atomically:

1. **Task 1: Preflight tracer, Wave 0 base and regex count table** - `01e389e` (chore)
2. **Task 2: Bound-file inventory, class-table inventory, pin-impact map and owner question** - `dc8dc18` (docs); the plan stopped here for the owner question
   - Owner approval record (orchestrator, not a plan task) - `238583e` (docs)
3. **Task 3: Decision record, carry-in triage, LibFuturo paper read and G4 ledger** - `fa01f8c` (docs); also records the resolved owner question
4. **Task 4: Digest-sweep printer shim for two API generations** - `c1ef5c8` (chore)

**Plan metadata:** the summary and state commit that follows this file (docs: complete plan).

The measured `commits:` count of 5 is `git rev-list --count f9124ae..c1ef5c8` at write time; it includes the orchestrator's `238583e`.

## Files Created/Modified

- `scripts/preflight-phase11.sh` - the scripted local preflight (created in Task 1)
- `scripts/preflight-phase11-regexes.txt` - expected `ctest -N -R` counts for every Phase 11 verify regex (created in Task 1)
- `.planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md` - Wave bases, inventories, owner question and resolution, decision record, carry-in triage, LibFuturo read, Gate-growth ledger (created in Task 1, filled in Tasks 2 and 3)
- `tools/digest-sweep/t64_digest_sweep.c` - two-generation printer shim (Task 4)

## Decisions Made

The binding ones are rows DR-01 to DR-32 of PIN-IMPACT.md; the key choices are in the frontmatter. Two details worth stating here: DR-02 defines `T64_ABI_VERSION` as a `uint32_t` constant equal to the major shifted left 16 or the minor (0x00000001 for 0.1) rather than wrapping the expression in `UINT32_C`, because `UINT32_C` pastes a suffix and cannot wrap an expression; and DR-12 leaves the exact handler-status whitelist to be measured in 11-03 as the plan says.

## Deviations from Plan

None to the work itself; the plan executed as written. Recorded for transparency:

- Task 2 stopped at its own owner-question gate (the plan prescribes this) and Tasks 3 and 4 ran after the owner answered A. The Task 3 text was applied with `t64_host_storage` keeping its leading `struct_size`, as the answer requires.
- `requirements-completed` is empty on purpose. The plan frontmatter lists API-01, API-02 and API-03, but this plan delivers inventories, records and tooling, not the API, so marking them complete would be false; the delivering plans (11-02 to 11-24) own that.
- The plan text for Task 4 mentions initializing video info, audio info and output-drain-result with their macros; the printer passes none of those structs (only the allocator, the create-info, the stop request and the stop result), so nothing else needed converting.
- The Phase 10 qualifier lists `tools/digest-sweep/t64_digest_sweep.c` in its `TOOL_PATHS`, so the closed plan 10-21 qualification record no longer matches the live tree. This is expected (any Phase 11 production edit does the same); `harness.phase10-qualification` (the hermetic test) still passes, and the record must not be re-verified at a Phase 11 head.

### Auto-fixed Issues

None.

## Authentication Gates

None.

## Issues Encountered

- The shell used by the tool is zsh, which does not split an unquoted variable of flags; the two-generation compile check was run from a bash script in the scratchpad instead.
- The probe artifacts and the scratch header for the head-path syntax check live in the scratchpad outside the repository; the temporary scratch directory of the check was removed.

## Verification Results

| Check | Result |
|-------|--------|
| `sh scripts/preflight-phase11.sh --no-replay` | PASS, total 41 s (ctest-counts 1 s, linux-compile 39 s over 228 units, guards and abi-cheap skip lines for absent names, host load 6.9) |
| mismatch control (`T64_PREFLIGHT_REGEXES` with one wrong count) | exit 1, `count mismatch: ^harness\.rdp-oracle-authority$ expected 2 got 1` |
| `python3 -B scripts/digest-sweep.py --base <Wave 0 base> --head WORKTREE` | 1599 shared lines, 0 differences, 0 head-only lines |
| printer, base path (current header) and head path (planned-shape header), gate flags, `-fsyntax-only` | 0 diagnostics each; the head branch emits the create-info call and the base branch the three-argument call |
| `grep -c '^\| DR-'` and the carry-in greps | 32, 13 and 13 |
| `harness.phase10-qualification`, `harness.rdp-oracle-authority`, `repo.rsp-no-hle` | 3 of 3 passed |
| `sh scripts/pii-scan.sh --tree` | findings=0 |
| `git diff --quiet` on core, runner, tools (outside the printer), tests, the oracle checker and docs/evidence at the Task 2 commit | exit 0 (no production or oracle-bound change) |

## Known Stubs

None.

## Threat Flags

None. The LibFuturo path is not recorded (T-11-01), the probe ran in a scratch clone (T-11-02) and the one non-call-site hunk was taken to the owner before the decision record (T-11-03).

## Next Phase Readiness

Plan 11-02 (additive public surface, first SHAPE plan) can start: it cites the preflight, the Wave 0 base, DR-01 to DR-03, DR-15, DR-16 and DR-21. It adds the `PHASE11_TESTS` block (the preflight label step passes while the block is absent) and raises the `^repo\.abi` and `^unit\.core\.api\.` rows. Plan 11-14 applies the approved `test_rdp.c:486` hunk with the other bound edits in its isolated orchestrator commit. No owner decision is pending.

## Self-Check: PASSED

- FOUND: scripts/preflight-phase11.sh, scripts/preflight-phase11-regexes.txt, PIN-IMPACT.md, tools/digest-sweep/t64_digest_sweep.c
- FOUND commits: 01e389e, dc8dc18, 238583e, fa01f8c, c1ef5c8 (all ancestors of HEAD)
