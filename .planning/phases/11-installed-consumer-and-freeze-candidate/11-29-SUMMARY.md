---
phase: 11-installed-consumer-and-freeze-candidate
plan: 29
subsystem: docs
tags: [header-text, spec-text, doc-fix, review-findings, freeze-candidate]
status: complete

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the 11-21 INSTALL review findings I-02, I-03 and I-04 (11-REVIEW.md), the 11-28 green head
provides:
  - docs/spec/machine-contract.md "Raw ROM admission" states that a pinned Open IPL3 profile narrows admission to the exact pinned 4096-byte asset or an authored big-endian z64 cartridge (every other input, byte-swapped and word-reversed included, is refused with INVALID_ROM) and cites boot-limits.md; the three-order table is scoped to the skeleton profile
  - the public header and machine-contract.md give T64_STOP_FRAME_EVENT_COUNT its true meaning (a target above the counter never reports the frame-event reason through the public API; an equal target returns at once; the counter is not a video-frame count) and say the instruction counter is advanced by CPU retirement
  - the public header says t64_profile_lookup takes no machine and writes NULL to *profile_out on a refused lookup (NULL profile_out writes nothing)
affects: [11-21, 11-23]

actuals:
  tokens: 4200
  tasks: 2
  commits: 2

plan_head_before: 6506c382f3b74fb35305b7e89d64ba91da70ccf5
plan_head_after: 69122de9ae62667e2e053abdac622ea585de95d8

tech-stack:
  added: []
  patterns:
    - "a header or spec sentence is written only after the code line that makes it true was read (named in PIN-IMPACT), so the text bound by the freeze hash is checkable"

key-files:
  created: []
  modified:
    - core/include/tendies64/t64_machine.h
    - docs/spec/machine-contract.md
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "The frame-event text says the counter keeps its value from create, reset (zero) or a state import, because core/t64_state.c restores it from the blob; it does not claim the counter is always zero"
  - "The I-02 sentence scopes the narrowing by the policy text and the pinned-profile predicate (every shipped profile other than ntsc-cart-4m-skeleton) and repeats the existing status-classes.txt row that UNSUPPORTED_ROM_ORDER is never returned, instead of introducing a new claim"
  - "11-REVIEW.md is not edited: its I-02..I-04 rows keep the new-numbered-plan disposition and the 11-21 ledger row records them as resolved by 11-29 (the plan names three files)"

patterns-established: []

requirements-completed: []

coverage:
  - id: D1
    description: "machine-contract.md Raw ROM admission matches boot-limits.md, the profile rom_order_policy text and openipl3_authored_flat_rom_matches"
    requirement: API-01
    verification:
      - kind: command
        ref: "sh scripts/check-docs.sh; sh scripts/check-stale-freeze-text.sh (files=302 findings=0)"
        status: pass
      - kind: judgment
        ref: "text read against core/t64_machine.c (has_pinned_openipl3_profile_id, t64_machine_admit_rom call site, openipl3_source_asset_matches, openipl3_authored_flat_rom_matches) and core/t64_rom.c (detect_order for the skeleton)"
        status: pass
    human_judgment: false
  - id: D2
    description: "the header and the spec give T64_STOP_FRAME_EVENT_COUNT a true meaning and no longer call the instruction counter synthetic-only"
    requirement: API-01
    verification:
      - kind: command
        ref: "python3 -B scripts/check-abi.py header --include core/include (c99 ok, c++17 ok, findings 0)"
        status: pass
      - kind: judgment
        ref: "text read against core/t64_scheduler.c (test_frame_handler, advance_counter_bound order), core/t64_cpu.c retirement, core/t64_machine.c reset, core/t64_state.c section 2"
        status: pass
    human_judgment: false
  - id: D3
    description: "the header comment of t64_profile_lookup states the NULL write and the absence of a machine"
    requirement: API-01
    verification:
      - kind: judgment
        ref: "text read against core/t64_machine.c t64_profile_lookup (NULL profile_out refused first; *profile_out = NULL before the id checks; UNKNOWN_PROFILE for an unknown id)"
        status: pass
    human_judgment: false
  - id: D4
    description: "the header diff is comment lines only and the digest sweep reports 0 differences"
    requirement: API-01
    verification:
      - kind: command
        ref: "git diff -U0 of the header filtered for non-comment changed lines (empty); python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD (1599 shared lines, 0 differences, 0 head-only lines); sh scripts/preflight-phase11.sh --no-replay PASS"
        status: pass
    human_judgment: false

duration: 25min
completed: 2026-11-26
---

# Phase 11 Plan 29: Header and spec text fixes I-02, I-03, I-04 Summary

**Three texts that the freeze hash will bind are now true: raw ROM admission of a pinned Open IPL3 profile is stated as the exact asset or an authored big-endian z64 cartridge, the frame-event stop kind is stated to be unreachable through the public API, and t64_profile_lookup says it writes NULL on a refused lookup. Comment and documentation lines only.**

## Performance

- **Duration:** about 25 min wall (the preflight and the digest sweep took 60 s and about 100 s at host load 6 to 8)
- **Tasks:** 2 (both `type="auto"`)
- **Files modified:** 3 (header comments, one spec file, PIN-IMPACT)

## Accomplishments

- **I-02.** "Raw ROM admission" in `docs/spec/machine-contract.md` now scopes the three-order table to a profile whose `rom_order_policy` is `all-known-orders-to-big-endian-v1` (the skeleton profile) and states that every pinned Open IPL3 profile accepts, after the length and interval checks, only the exact pinned 4096-byte source asset or an authored big-endian z64 cartridge that passes the `boot-limits.md` checks, refusing everything else (byte-swapped and word-reversed orders included) with `T64_STATUS_INVALID_ROM`. All 24 non-skeleton profile ids sit in `has_pinned_openipl3_profile_id`, so the scope sentence holds for the whole shipped set. No code change.
- **I-03.** A comment before `T64_STOP_GUEST_TICK` in the header and the "Deterministic scheduler and state digest" paragraph of the spec now say: the instruction counter is advanced by CPU retirement; the frame-event counter is advanced only by a synthetic test event that no public entry can schedule, so it keeps its value from create, reset (zero) or a state import and is not a video-frame count; a larger `T64_STOP_FRAME_EVENT_COUNT` target never ends an advance with `T64_STOP_REASON_FRAME_EVENT_COUNT`; an equal target returns at once with that reason unless a halt or CPU stop is already present (the order of `advance_counter_bound`). The stale sentence that called both counters synthetic-only is replaced.
- **I-04.** The comment of `t64_profile_lookup` says the call takes no machine, a NULL `profile_out` is refused with `T64_STATUS_INVALID_ARGUMENT` and nothing is written, and any other refusal (NULL or empty id, unknown id) writes NULL to `*profile_out`.
- **Records.** PIN-IMPACT has a "11-29" section after "11-28" with the code lines each sentence was checked against, the check results and the sweep.

## Task Commits

1. **Task 1: spec and header text (I-02, I-03, I-04)** - `21ca1d9` (docs)
2. **Task 2: records** - `69122de` (docs: PIN-IMPACT "11-29" section; preflight, digest sweep and PII scan run before it)

## Verification

| Check | Result |
|-------|--------|
| Header diff comment lines only (`git diff -U0`, non-comment changed lines) | empty |
| `python3 -B scripts/check-abi.py header --include core/include` | c99 ok, c++17 ok, findings 0 |
| `sh scripts/check-docs.sh` | ok |
| `sh scripts/check-stale-freeze-text.sh` | files=302 findings=0 |
| `sh scripts/pii-scan.sh --tree` (before each docs commit) | findings=0 (1424 files) |
| `sh scripts/preflight-phase11.sh --no-replay` | PASS, 60 s (head-ci passed: origin/main is green again after 11-28) |
| `python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD` | 1599 shared lines, 0 differences, 0 head-only lines |
| CTest names | unchanged (324); none added; no native gate run was needed (no code, macro, layout or status change) |

## Files Created/Modified

- `core/include/tendies64/t64_machine.h` - two comment blocks (stop kinds, `t64_profile_lookup`); 13 added lines, 1 changed comment line, no declaration touched
- `docs/spec/machine-contract.md` - one paragraph after the ROM-order table (I-02), the counter sentences of the scheduler paragraph (I-03)
- `.planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md` - section "11-29"

## Decisions Made

See `key-decisions` in the frontmatter.

## Deviations from Plan

### Auto-fixed Issues

None - plan executed exactly as written. One scoping choice, recorded under key-decisions: the frame-event sentences name the state-import path (the counter is restored from a blob) so the text does not over-claim "always zero".

## Auth Gates

None.

## Known Stubs

None.

## Threat Flags

None. No file added or changed introduces a network endpoint, auth path, file access or schema change. T-11-69 (contract text bound by the freeze record) is mitigated: each sentence was read against the code lines named in PIN-IMPACT before the commit.

## Notes for the orchestrator

- Plan ran out of wave order (after 11-28, before the remainder of 11-21), as instructed. STATE's next plan stays at the 11-21 remainder.
- Nothing pushed. Local unpushed commits: the two code/docs commits of this plan plus the SUMMARY and state commit.
- 11-21's ledger row should record I-02, I-03 and I-04 as resolved by 11-29 (`21ca1d9`); `11-REVIEW.md` was not edited. 11-23 must bind the header at a head that contains `21ca1d9`, and its `docs/spec/api.md` can cite the corrected machine-contract text.
- The `requirements: [API-01]` of this plan is not marked complete: a text fix does not deliver the requirement (the freeze candidate does).

## Self-Check: PASSED

- FOUND: core/include/tendies64/t64_machine.h, docs/spec/machine-contract.md, PIN-IMPACT.md (modified)
- FOUND commits: 21ca1d9, 69122de (both ancestors of HEAD)
