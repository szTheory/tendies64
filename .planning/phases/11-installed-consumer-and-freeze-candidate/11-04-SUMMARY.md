---
phase: 11-installed-consumer-and-freeze-candidate
plan: 04
subsystem: api
tags: [reset, unload, output-status, struct-size, retained-slots, not-ready-table, no-hle-refresh, shape-wave]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: plan 11-03 poison and in-call flags, T64_ENTRY_GUARD, the entry sweeps of test_api_poison.c; plan 11-02 status values 34 to 38; decision record DR-04, DR-06, DR-08, DR-09, DR-10 and the Wave 0 base
provides:
  - t64_machine_reset (first-admit state, digest equal to the first admission, rings and ISViewer cursor empty), t64_machine_unload (exact image release through the host allocator, instance returns to the no-image state) and t64_machine_output_status with t64_output_status (48 bytes, leading caller-filled struct_size, T64_OUTPUT_STATUS_V1_SIZE and T64_OUTPUT_STATUS_INIT in their final form)
  - core/tests/test_api_lifecycle.c (public header only) and four CTest names unit.core.api.reset, .unload, .not-ready-table, .output-status
  - the NOT_READY table over every machine-taking entry (29 test rows; PIN-IMPACT lists 30 lines with destroy) as data in the test and as a record in PIN-IMPACT
  - internal retained slot types t64_vi_packet_slot (488 bytes) and t64_ai_packet_slot (280 bytes), byte-identical to today's public info structs, so the flip can add struct_size to the public structs without moving a retained byte
  - the isolated no-HLE refresh of five sealed bodies; the digest sweep against the Wave 0 base reports 0 differences
affects: [11-05, 11-11, 11-12, 11-13, 11-14, 11-15, 11-17, 11-20, 11-23]

actuals:
  tokens: 19800
  tasks: 4
  commits: 6
plan_head_before: 456eaf21c14d553f7b727f9a90704d6f6aa32b88
plan_head_after: e3eb2a2d4e5c7b78edc56200ecb0f48ab3139b8b

tech-stack:
  added: []
  patterns:
    - "clear_guest_state is the shared half of admission, public reset and public unload; only reset and admission follow it with a boot entry"
    - "an out-struct call computes into a local record, checks span and struct_size first, then copies min(caller, known) bytes after struct_size, so a refusal leaves the caller struct byte for byte"
    - "a table test runs every entry on an unloaded instance and, for each row, on a never-admitted instance; both must give the table's status"
    - "the retained slot is an internal type; the public info call clears the out struct and copies member by member"

key-files:
  created:
    - core/tests/test_api_lifecycle.c
  modified:
    - core/include/tendies64/t64_machine.h
    - core/t64_machine.c
    - core/t64_observe.c
    - core/t64_internal.h
    - core/t64_vi.c
    - core/t64_ai.c
    - core/t64_digest.c
    - core/t64_state.c
    - core/tests/test_api_poison.c
    - core/tests/test_digest_v26.c
    - core/tests/test_state_import.c
    - core/CMakeLists.txt
    - docs/spec/machine-contract.md
    - docs/spec/state-field-rules.txt
    - scripts/check-rsp-no-hle.py
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "Public reset emits the admission trace record after reset_guest_state exactly as admission does, and sets no in-call flag because it calls no allocator callback; a failing reset_guest_state poisons (DR-11 shape)"
  - "Unload clears the guest state with the part of reset_guest_state that needs no image (clear_guest_state), clears the image members with field assignments (no memset of image_storage, which the state-field audit would read as a length use), then releases with the in-call flag raised"
  - "No entry needed a new has_image test: bus_read, bus_write, trace_set_enabled, trace_drain, profile and the poison query are image-independent by their code; the cartridge-window read keeps its BUS_UNSUPPORTED answer, the same as a never-admitted instance"
  - "t64_output_status layout: struct_size, video_queued, video_free, one explicit pad member (reserved0, written as 0), video_next_bytes, audio_queued, audio_free, audio_next_bytes, text_pending_bytes: 48 bytes with no implicit holes; the PIN-IMPACT probe estimate of 40 bytes was a planning figure and is superseded"
  - "Output status without an image is OK with zero counts and full free capacity (a host paces before the first admission too); a profile without a ring reports it empty with the full capacity free, so a host that waits for free capacity cannot stall on it"
  - "The slot copy-out is inlined in t64_video_info and t64_audio_info (no helper function) so the isolated refresh changes digests only, with no inventory edit"

patterns-established:
  - "A new reachable struct in core/t64_internal.h widens the member set of the state-field audit: its member names need rows in docs/spec/state-field-rules.txt (rule NONE with a comment when the slot values are echoed metadata)"

requirements-completed: []

coverage:
  - id: D1
    description: "Public reset returns the admitted image to its first-admit state: the digest equals the digest right after the first admission (hello guest run to its BREAK with 30 undrained bytes, and a VI guest holding a queued packet), both rings and the ISViewer drain cursor are empty, the replay equals the first run; NOT_READY without an image and INVALID_ARGUMENT for NULL leave counters unchanged; poisoned and reentrant answers are covered by the 26-entry sweeps"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.api\\.reset$' (passed 0.17 s); RED report build/gate/11-04-task1/red.txt (link error, undefined t64_machine_reset); a mutation that reset only the execution state failed the digest, cursor, stop-detail and ring checks"
        status: pass
    human_judgment: false
  - id: D2
    description: "Public unload releases exactly the image block (allocations unchanged, releases plus one, live minus one, released size equals the image block), admission after unload equals a fresh admission by digest (same image after a run, and a different image), unload then destroy leaves zero outstanding with every allocation released once; no-image unload and a second unload are NOT_READY with counters unchanged; the release callback sees REENTRANT on all 26 entries"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.api\\.(unload|reentrant)$' (passed); RED report build/gate/11-04-task2/red.txt (link error, undefined t64_machine_unload)"
        status: pass
    human_judgment: false
  - id: D3
    description: "After unload every image-requiring entry answers NOT_READY with outputs untouched, the image-independent entries answer as on a new instance, on four profiles; 29 rows including output_status, state_import (report reason NOT_ADMITTED) and the two cartridge-window rows; each row also matches a never-admitted instance"
    requirement: "API-02"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.api\\.not-ready-table$' (passed 0.03 s); table recorded in PIN-IMPACT '11-04 NOT_READY table'"
        status: pass
    human_judgment: false
  - id: D4
    description: "t64_machine_output_status reports per ring the queued count, free capacity and next packet size and the undrained ISViewer bytes; during a run of the audio fixture on the VI and AI profile queued plus free is 2 on both rings, next bytes equal t64_video_info/t64_audio_info and the copy-size query, one copy moves a ring by one, pending text counts 30 to 0 on a drain, the call changes no digest; struct_size 0, 1, 4 and 47 answer STRUCT_TOO_SMALL with the caller struct byte-identical, NULL out and NULL machine are INVALID_ARGUMENT, a larger caller struct keeps struct_size and its tail"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.api\\.output-status$' (passed 0.87 s); RED report build/gate/11-04-task3/red.txt (link error, undefined t64_machine_output_status); python3 -B scripts/check-abi.py header --include core/include --header tendies64/t64_machine.h prints c99 ok and c++17 ok"
        status: pass
    human_judgment: false
  - id: D5
    description: "The retained VI and AI slots are internal types of 488 and 280 bytes (asserted with first and last member), t64_vi_state stays 1,208 bytes, the audio slot keeps its own struct_size value; no digest, state container byte or completeness count moved"
    requirement: "API-01"
    verification:
      - kind: integration
        ref: "python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD: 1599 shared lines, 0 differences, 0 head-only lines (head fbe5704a16b1); ctest --preset gate 268 of 268 passed; unit.core.digest.v26-completeness passed with the 791, 400 and 17 byte tallies unchanged"
        status: pass
    human_judgment: false

duration: 25min
completed: 2026-11-25
status: complete
---

# Phase 11 Plan 04: Reset, Unload, Output Status and the Retained Slot Types Summary

**Three new public entries (reset, unload, output status with a struct_size-first t64_output_status), a 29-row NOT_READY table pinned by tests, and the internal retained slot types for the VI and AI packets with a five-digest isolated no-HLE refresh; the digest sweep stays at 0 differences and the full gate passes 268 of 268.**

## Performance

- **Duration:** 25 min
- **Started:** 2026-11-25T20:37:40Z
- **Completed:** 2026-11-25T21:02Z (this summary and the state update follow)
- **Tasks:** 4 of 4
- **Files modified:** 19 (1 created, 18 modified, counting the PIN-IMPACT record)

## Accomplishments

- Tracer (Task 1): `t64_machine_reset` declared with its postcondition text and defined in `core/t64_machine.c` (guard, `NOT_READY` without an image, `reset_guest_state`, the admission trace record, poison on failure). `core/tests/test_api_lifecycle.c` includes only the public header: the hello guest runs to its BREAK with 30 undrained bytes, reset restores the first-admit digest, the replay gives the same stop, digest and transcript; a VI guest holding a queued packet resets to the first-admit digest and produces the same first packet after the same authored setup. RED first (link error); a mutation resetting only the execution state failed 11 checks.
- Unload and the table (Task 2): `reset_guest_state` is split so `clear_guest_state` is shared by admission, reset and unload. `t64_machine_unload` clears the guest state and the image members and releases the image storage with the in-call flag raised. `unit.core.api.unload` asserts the exact release (counts and block size), admission after unload equal to a fresh admission (same image after a run, and a different image), refusals, and destroy after unload; `unit.core.api.not-ready-table` runs 29 rows on four profiles against an unloaded instance and a never-admitted instance. The poison, reentrant and null-machine sweeps gained both entries; the reentrancy case also covers the release callback of unload.
- Output status (Task 3): `t64_output_status` is born with its final layout and the two macros; the call is read-only, refuses a small `struct_size` before any write, never writes `struct_size` and copies at most `min(caller, 48)` bytes after it. The test tracks real ring occupancy through the audio fixture on the VI and AI profile, checks every status against `t64_video_info`, `t64_audio_info` and the empty-capacity copy size, and exercises the refusals.
- Retained slots (Task 4): `t64_vi_packet_slot` and `t64_ai_packet_slot` in `core/t64_internal.h` with size and first/last member asserts on LP64; every walker, hasher, importer and filler uses them; the public `t64_video_info` and `t64_audio_info` copy out member by member from a cleared destination. `test_digest_v26.c` keeps the 1,208/488/37-row pins on the slot type. The five changed sealed bodies were re-pinned in one isolated commit (5 old and 5 new digest lines).

## Task Commits

1. **Task 1: Tracer, public reset to digest equality** - `fb87f4d` (feat)
2. **Task 2: Public unload and the NOT_READY table** - `5b0e2a8` (feat)
3. **Task 3: t64_machine_output_status** - `58661c8` (feat)
4. **Task 4: Retained slot types** - `1aa965a` (refactor), then the isolated no-HLE refresh `fbe5704` (chore) and the PIN-IMPACT shape-steps row `e3eb2a2` (docs)

**Plan metadata:** the summary and state commits that follow this file. The measured `commits:` count of 6 is `git rev-list --count 456eaf2..e3eb2a2` at write time.

## Files Created/Modified

- `core/include/tendies64/t64_machine.h` - three declarations, `t64_output_status`, `T64_OUTPUT_STATUS_V1_SIZE`, `T64_OUTPUT_STATUS_INIT`
- `core/t64_machine.c` - reset, unload, `clear_guest_state` split; `core/t64_observe.c` - output status
- `core/t64_internal.h`, `core/t64_vi.c`, `core/t64_ai.c`, `core/t64_digest.c`, `core/t64_state.c` - slot types and their users
- `core/tests/test_api_lifecycle.c` (new), `test_api_poison.c` (26 entries, unload reentrancy phase), `test_digest_v26.c`, `test_state_import.c` (diagnostic dump type)
- `core/CMakeLists.txt`, `scripts/check-test-inventory.sh` (268), `scripts/preflight-phase11-regexes.txt` (`^unit\.core\.api\.` 11)
- `docs/spec/machine-contract.md` (reset, unload and ring rows; output status paragraph), `docs/spec/state-field-rules.txt` (13 rows), `scripts/check-rsp-no-hle.py` (five digests), `PIN-IMPACT.md` (NOT_READY table, Shape steps row and evidence)

## Decisions Made

Binding decisions are DR-04, DR-06, DR-08, DR-09 and DR-10; this plan implemented them. Choices inside the plan's discretion are in the frontmatter (no in-call bracket for reset, field assignment for the image storage, layout and no-image answer of the status, inlined copy-out).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] A non-bound white-box test named the public type of a retained slot**
- **Found during:** Task 4 (first full build)
- **Issue:** `core/tests/test_state_import.c` (a diagnostic dump in the state import test) declared `const t64_video_packet_info *x = &a->vi.packet_info[slot]`, which no longer type-checks once the slot is an internal type. It is not in the plan's file list and is not oracle-bound.
- **Fix:** the two declarations use `t64_vi_packet_slot`; nothing else in the file changed.
- **Files modified:** `core/tests/test_state_import.c`
- **Commit:** `1aa965a`

**2. [Rule 3 - Blocking] The state-field audit gained 13 unmapped member names**
- **Found during:** Task 4 (`harness.state-field-use-audit` failed after the slot types moved into `core/t64_internal.h`)
- **Issue:** the audit walks the types reachable from `struct t64_machine` in the internal header only; the slot members (previously in the public header, outside the walk) joined the member set, and 13 names with lexical index, length or loop uses had no mapping row. The plan expected a type-name update only; the audit maps by member name.
- **Fix:** 13 rows in `docs/spec/state-field-rules.txt` (rule NONE, reader `state_read_video_slot` or `state_read_audio_slot`, with a comment): the slot values are echoed metadata and the lexical hits are same-named geometry members, loops under `sizeof` of constant-size text arrays and `sizeof` expressions. `docs/spec/state-field-rules.txt` is in the plan's file list.
- **Files modified:** `docs/spec/state-field-rules.txt`
- **Commit:** `1aa965a`

**3. [Rule 1 - Bug] A memset of the image storage tripped the same audit**
- **Found during:** Task 2 (`harness.state-field-use-audit`: unmapped member `image_storage`, class M)
- **Issue:** clearing the storage descriptor with `memset(&machine->image_storage, 0, sizeof machine->image_storage)` is a member in a length argument.
- **Fix:** three field assignments; no mapping row needed.
- **Commit:** `5b0e2a8`

### Plan text applied with additions or adjustments (recorded for transparency)

- **In-call flag for reset (plan: set in_call).** Reset calls no allocator callback, so no host code can re-enter during it, and the flag would guard nothing; it is not raised. The entry guard still answers REENTRANT for a reset issued from another call's callback (covered by the 26-entry sweep). Unload raises the flag around its one callback.
- **White-box include only for the poison case (plan).** `test_api_lifecycle.c` includes only the public header. The poisoned, reentrant and NULL answers of the three new entries are proved in `test_api_poison.c` by extending `ENTRY_COUNT` to 26 (the running note of 11-03), with sentinel comparisons and the digest comparison.
- **`t64_output_status` is 48 bytes.** The PIN-IMPACT probe estimated 40; the plan's member list with fixed-width members and one explicit pad member is 48 bytes (`T64_OUTPUT_STATUS_V1_SIZE` UINT32_C(48), asserted equal to `sizeof` by the output-status case). The probe figure was a planning estimate.
- **Reset digest scenario.** The plan text names a 20,000 tick advance; the tests use the hello guest run to its BREAK (700,000 tick bound, 30 undrained ISViewer bytes) and a VI guest with a queued packet, which exercise more state than 20,000 ticks. 
- **Copy-out by inline code (plan: copy member by member).** The slot copy is inline in the two sealed bodies instead of a helper, so the refresh commit changes only digests (a new reviewed function would have changed the checker's function inventory). The destination is cleared first so the bytes (padding included) equal the old whole-struct copy while the layouts are equal.
- **`docs/spec/profile-membership-sites.txt`** is in the plan's file list; `repo.si-membership-sites` passed without regeneration, so it was not touched.
- **Four-task plan with an eighteen-file list (ROADMAP G5 exception).** The plan finished in about 25 minutes of wall time, so no split was needed.
- **Optional ledger.** The cross-phase `.planning/WINDOWS.md` ledger was not appended (best-effort; no stub, skipped test or unrun verify exists).

**Total deviations:** 3 auto-fixed (2 Rule 3, 1 Rule 1), 7 recorded adjustments. **Impact:** none on scope; every plan truth and acceptance criterion holds.

## Authentication Gates

None.

## Issues Encountered

- The host load stayed between 7 and 12 during the native runs (the full gate took 83 s); every timing here is a pass/fail or a count, and no limit was widened. The reset, unload, table and output-status names measure 0.18, 0.23, 0.03 and 0.87 s, all `COST 1`, `TIMEOUT` 60.
- `repo.rsp-no-hle` was red between `1aa965a` and `fbe5704` by design (exactly five bodies), as in plan 11-03.
- A mistyped path in one root-pin invocation ran nothing; the next invocation passed. All commits followed a passing pin.

## Open items for later plans (not acted on here)

- **Output status on a defect:** the ISViewer cursor-beyond-capture inconsistency reports 0 pending text instead of the drain's `RESOURCE_EXHAUSTED` (the call has no refusal for state that no input reaches). The 11-15 SHAPE review may want it to poison like the other post-write defects.
- The 11-03 open items (state-export post-write branch, `event_queue_capacity` 0 accepted at create, zero-poison case set white-box) are unchanged.
- The codemod plans (11-05 to 11-10) and the flip (11-12) must include `core/tests/test_api_lifecycle.c`, which declares `t64_state_import_report`, `t64_output_drain_result`, `t64_video_packet_info`, `t64_audio_packet_info`, `t64_cpu_stop_detail`, `t64_stop_request` and `t64_stop_result` objects without initializers (`T x;` plus `memset`), and the `T64_OUTPUT_STATUS_INIT` uses, by grep at their start.

## Verification Results

| Check | Result |
|-------|--------|
| `cmake --build --preset gate` | builds clean |
| `ctest -R '^unit\.core\.api\.'` | 11 of 11 passed (`ctest -N` lists 11) |
| `ctest --preset gate` at `fbe5704` | 268 of 268 passed (83 s, load 8) |
| `unit.core.digest.v26-completeness` | passed (791 member, 400 named-exclusion and 17 padding bytes of 1,208 unchanged) |
| `python3 -B scripts/check-rsp-no-hle.py --build build/gate` | exit 0 after `fbe5704`; findings=5 at `1aa965a` (exactly the five bodies) |
| refresh commit | one file; 5 old and 5 new digest lines (`git diff --numstat` 5 5) |
| `python3 -B scripts/digest-sweep.py --base f9124ae... --head fbe5704` | `1599 shared lines, 0 differences, 0 head-only lines` |
| `python3 -B scripts/check-abi.py header --include core/include --header tendies64/t64_machine.h` | c99 ok, c++17 ok, findings=0 |
| `T64_OUTPUT_STATUS_INIT` in C11 and C++17 with `-Wconversion -Werror`; `sizeof(t64_output_status)` 48 | compiles, equals 48 |
| `harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `repo.abi-*` | passed at the head |
| `sh scripts/check-test-inventory.sh --preset gate` | PASS count 268 (264 + 4) |
| `sh scripts/preflight-phase11.sh` (full, with replay) at `e3eb2a2` | PASS, 65 s (linux-compile 231 units gcc 13.3.0, post-gate-replay 27 s) |
| `sh scripts/pii-scan.sh --tree` | findings=0 |

## Known Stubs

None.

## Threat Flags

None. T-11-11 (use after unload) is mitigated by the 29-row table test on four profiles and the never-admitted comparison; T-11-12 (storage release) by the exact counting-allocator assertions of `unit.core.api.unload`; T-11-13 (output status disclosure) by the read-only, struct_size-floor, `min(caller, known)` copy and the refusal sentinels of `unit.core.api.output-status`.

## Next Phase Readiness

The last new public symbols exist (35 public functions, DR-04), so plan 11-05 can land the public init macros and the codemod can start, and the prefix header (11-11) is generated after them. Plans that edit one of the five re-pinned bodies, or the four sealed observers, re-pin them in an isolated commit. API-01 and API-02 are not complete: the flip, the struct_size prologues, the installed consumer and the refusal-is-a-no-op harness are still ahead. Nothing in this plan is orchestrator-only and no owner decision is pending.

## Self-Check: PASSED

- FOUND: core/tests/test_api_lifecycle.c, core/include/tendies64/t64_machine.h, core/t64_internal.h, docs/spec/state-field-rules.txt, PIN-IMPACT.md "11-04 NOT_READY table" and "Shape steps"
- FOUND commits: fb87f4d, 5b0e2a8, 58661c8, 1aa965a, fbe5704, e3eb2a2 (all ancestors of HEAD)
