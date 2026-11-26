---
phase: 11-installed-consumer-and-freeze-candidate
plan: 13
subsystem: api
tags: [struct-size, struct-too-small, copy-out, prologues, shape-wave, dev-11-01, no-hle-refresh]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: plan 11-12 (the leading struct_size on every in/out struct, the v1 size constants of core/t64_internal.h, the create-info handshake, the scratch-proven bound patch and the DEV-11-01 build scope)
provides:
  - the struct_size prologue of every remaining in/out entry (advance request and result, cpu_stop_detail, output_drain result, state_import report, video_info, audio_info, save_info, profile_hash and profile_serialize descriptor): a value below the v1 size is T64_STATUS_STRUCT_TOO_SMALL with the struct, every output and the machine untouched; a larger value gets exactly the v1 members, never the struct_size member and never a byte past the v1 size
  - the static inline t64_internal_copy_out in core/t64_internal.h (copy-out of a local record after struct_size, at most min(caller, known) bytes)
  - CTest unit.core.api.struct-size (nine cases, a floor and a larger-caller case each) and the group mode of core/tests/test_api_struct_size.c
  - the isolated no-HLE refresh of the two sealed observers t64_video_info and t64_audio_info
affects: [11-14, 11-15, 11-16, 11-17, 11-19]

actuals:
  tokens: 19900
  tasks: 3
  commits: 4

plan_head_before: 715c790dc4660922410d7274107daf87db42b6be
plan_head_after: e76c7e1f2a4307d7770626fc89491c161c7daae2

tech-stack:
  added: []
  patterns:
    - "a copy-out entry builds its record in a zeroed local of the known layout and copies the bytes after struct_size through t64_internal_copy_out, so the callee never writes struct_size and the padding it writes is zero"
    - "check order of an observer: NULL machine or struct, entry guard, the struct span (wrapping or aliasing the machine is INVALID_ARGUMENT before the struct is read), then struct_size below v1 (STRUCT_TOO_SMALL), then admission and state"
    - "a descriptor entry that reads named members only (t64_profile_hash, t64_profile_serialize) needs only the floor check; no local copy of the 2,304-byte descriptor"
    - "proof before commit inside the DEV-11-01 window: staged patch plus the unchanged 11-12 bound patch plus the pending no-HLE digest refresh in a marked scratch clone, then commit"

key-files:
  created: []
  modified:
    - core/t64_machine.c
    - core/t64_observe.c
    - core/t64_state.c
    - core/t64_save.c
    - core/t64_vi.c
    - core/t64_ai.c
    - core/t64_internal.h
    - core/include/tendies64/t64_machine.h
    - core/tests/test_api_struct_size.c
    - core/CMakeLists.txt
    - docs/abi/status-classes.txt
    - docs/spec/state-container.md
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - scripts/check-rsp-no-hle.py
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "the struct span is judged before the struct is read, so an observer keeps its existing INVALID_ARGUMENT answers for a wrapping or machine-aliasing pointer; struct_size is checked next and before NOT_READY"
  - "t64_machine_advance checks the request and the result struct_size before the not-admitted answer, reads the request through a zeroed local of the known layout and copies the result out only on T64_STATUS_OK"
  - "t64_machine_state_import reads the report size only when the report span is usable (not wrapping, not aliasing the machine); a NULL report is still accepted; a short report is refused before the blob is read and is never written"
  - "t64_profile_hash and t64_profile_serialize keep reading named members only; the prologue is the floor check"

patterns-established:
  - "Before an isolated no-HLE refresh commit, apply the re-pinned digests to the scratch clone as a third patch so repo.rsp-no-hle is not a second identity-block name"

requirements-completed: []

coverage:
  - id: D1
    description: "Every in/out public entry refuses a struct_size below its v1 size with STRUCT_TOO_SMALL before any state change and reads or writes at most the v1 members of a larger caller struct without writing struct_size"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "ctest unit.core.api.struct-size (cases advance, cpu-stop-detail, output-drain, state-import, video-info, audio-info, output-status, save-info, profile-descriptor; floor sizes 0, 1, 4 and v1-1 per entry with the sentinel struct and the machine digest unchanged; a larger caller compared member for member with the exact-size call and the trailing bytes and struct_size checked)"
        status: pass
      - kind: integration
        ref: "ctest -E <bound regex> in the shared tree: 263 of 263; scratch clone with the prologues, the bound patch and the digest refresh: 273 of 273 outside the two identity-block names; digest sweep 0 differences; authority checker exit 2 with 'authored corpus SHA-256 changed'"
        status: pass
    human_judgment: false
  - id: D2
    description: "The two sealed observers are re-pinned in an isolated digests-only commit and the sealed-body checker passes"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "python3 -B scripts/check-rsp-no-hle.py --build build/gate (exit 0, sources_scanned=85); ctest repo.rsp-no-hle"
        status: pass
    human_judgment: false

duration: 31 min
completed: 2026-11-26
status: complete
---

# Phase 11 Plan 13: The struct_size Prologues Summary

**Every remaining in/out entry (advance request and result, cpu_stop_detail, output_drain, state_import, video_info, audio_info, save_info, profile_hash and profile_serialize) now refuses a short caller struct with STRUCT_TOO_SMALL before any state change and gives a larger caller struct exactly the v1 members through a local record and one static inline copy-out; the prologues were proven in a scratch clone with the unchanged 11-12 bound patch before the commit, and the two sealed observers were re-pinned in an isolated digest commit.**

## Performance

- **Duration:** 31 min wall (19:25 to 19:56 local), host load 6 to 10 from unrelated processes; the clone gate waited once for the load to fall to 8 and no limit was widened
- **Tasks:** 3 of 3 (Task 1 tracer, Task 2 remaining entries, Task 3 non-bound ctest, scratch proof, commits and the isolated refresh)
- **Files modified:** 16 (14 in the prologue commit, 1 checker, 1 PIN-IMPACT; 1 CMake follow-up)

## Accomplishments

- **Tracer (advance).** `t64_machine_advance` reads `request->struct_size` and `result_out->struct_size` after the NULL checks and before `NOT_READY`; a value below 16 is `STRUCT_TOO_SMALL` with the result and the machine digest unchanged. The request is read through a zeroed local, the result is computed in a local and copied out after `struct_size` only on `OK`. The RED milestone is `build/gate/11-13-task1/red.txt` (the floor cases fail with no prologue: 40 failures, the short advance ran and moved the digest).
- **Every other entry.** `t64_machine_cpu_stop_detail`, `t64_machine_output_drain`, `t64_machine_save_info`, `t64_video_info` and `t64_audio_info` judge the struct span, then refuse a short `struct_size`, then do the admission and state checks; the record is built in a zeroed local and copied out with `t64_internal_copy_out`. `t64_video_info` used to clear the caller's `struct_size` and `t64_audio_info` used to set it from the retained slot; neither writes it now. `t64_machine_state_import` refuses a short report before the blob is read and never writes it. `t64_profile_hash` and `t64_profile_serialize` refuse a descriptor below 2,304 bytes. The RED milestone is `build/gate/11-13-task2/red.txt` (104 failures over the seven entries that had no prologue; `output-status` from 11-04 already passed).
- **Test.** `unit.core.api.struct-size` runs nine cases with the group mode of `test_api_struct_size.c` (`advance`, `cpu-stop-detail`, `output-drain`, `state-import`, `video-info`, `audio-info`, `output-status`, `save-info`, `profile-descriptor`); each holds a floor case (sizes 0, 1, 4 and v1-1, the sentinel struct and the machine digest unchanged, a NULL blob still answers `STRUCT_TOO_SMALL` for a short report) and a larger-caller case (the same members as the exact-size call, `struct_size` and the 16 trailing bytes untouched, advance compared on a twin machine with an equal digest, the drain returning the 30-byte transcript through a larger result). It needs the boot fixtures and the K1 environment; `unit.core.api.handshake` stays on the default group. Native time 1.94 to 2.00 s, `COST 2`.
- **W-3 proof inside the DEV-11-01 window.** The staged prologues (`build/11-13-bound/prologues.patch`, SHA-256 `17ee4d2ebb9ab3b667b1fc2dbe0a612289eae857b6d9cd5ff055456c4978356c`), the unchanged `build/11-12-bound/bound-callsites.patch` (SHA-256 verified against PIN-IMPACT first) and the pending digest refresh were applied in the marked clone `build/11-13-bound/src` (detached at `715c790`, the parent of the prologue commit): configure and build 5 s, `ctest --preset gate` outside the two identity-block names 273 of 273 in 72 s, the authority checker exits 2 with `authored corpus SHA-256 changed: core/tests/test_rdp.c`, and the sweep reports `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (41 s).
- **Isolated refresh.** The checker names `t64_video_info` and `t64_audio_info` as changed bodies; the refresh commit changes only their two digest lines (2 old, 2 new).

## Task Commits

1. **Tasks 1 and 2 (no commit by plan) and Task 3 prologue commit** - `24d45e8f23e9a6eb692fe2fba1b9b6a05978ed3f` (feat); its parent `715c790dc4660922410d7274107daf87db42b6be` is the HEAD the scratch clone was taken from
2. **Isolated refresh of no-HLE pins for t64_video_info, t64_audio_info** - `96d33230f0da3619493d4e08825cb36473912fb6` (chore), `scripts/check-rsp-no-hle.py` only
3. **Measured cost of the new CTest name** - `205688dc2650368a6df4eb4bdecc1722f16ca3ee` (chore), `core/CMakeLists.txt` only (`COST 1` to `COST 2`)
4. **PIN-IMPACT shape step** - `e76c7e1f2a4307d7770626fc89491c161c7daae2` (docs), `PIN-IMPACT.md` only

**Plan metadata:** the summary and state commits that follow this file. `commits: 4` is `git rev-list --count 715c790..<head at write time>`.

## Decisions Made

Binding decisions are DR-06 (the struct_size contract) and DR-22 (the DEV-11-01 window). The choices inside the plan's discretion are in the frontmatter and in the PIN-IMPACT "Plan 11-13 evidence" paragraph.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] The first prologues read struct_size before the span check**
- **Found during:** Task 3 step 1 (the first full non-bound ctest: 260 of 263)
- **Issue:** `t64_video_info`, `t64_audio_info`, `t64_machine_cpu_stop_detail`, `t64_machine_output_drain` and `t64_machine_save_info` read the caller's `struct_size` before their existing span check. `unit.core.ai.output` passes a wrapping `info_out` (UINTPTR_MAX - 3) and segfaulted; `unit.core.isviewer` passes a result span inside RDRAM and got `STRUCT_TOO_SMALL` (35) instead of `INVALID_ARGUMENT`
- **Fix:** the original compound NULL and span condition is kept first and the `struct_size` check follows it (the check order of an observer is now NULL, span, `struct_size`, admission and state); `t64_machine_state_import` reads the report size only for a usable span. The full non-bound ctest was re-run on the final code (263 of 263); the first run's logs are kept as `build/gate/11-13-task3/nonbound-ctest-run1.log` and `nonbound-LastTest-run1.log`
- **Files modified:** core/t64_machine.c, core/t64_observe.c, core/t64_save.c, core/t64_vi.c, core/t64_ai.c, core/t64_state.c
- **Commit:** `24d45e8`

**2. [Rule 3 - Blocking] repo.rsp-no-hle fails in the same ctest as the prologues until the isolated refresh**
- **Found during:** Task 3 step 1 (the first non-bound ctest, third failing name)
- **Issue:** the plan wants every non-bound name and every clone name except the two identity-block names to pass before the prologue commit, but the two sealed bodies it changes make `repo.rsp-no-hle` fail until the refresh commit that follows by design (11-04 precedent)
- **Fix:** the two digests were computed with the checker's own `strip_c_comments_and_literals` and `function_body`, left uncommitted in the working tree for the second non-bound ctest and applied as a third patch (`build/11-13-bound/nohle-refresh.patch`) in the scratch clone; the commit order is the plan's (prologues, then the isolated refresh). The prologue commit alone fails `repo.rsp-no-hle`, as the refresh precedent of 11-03, 11-04 allows
- **Files modified:** scripts/check-rsp-no-hle.py (committed in the isolated refresh, 2 digest lines)
- **Commit:** `96d3323`

### Other differences from the plan text (not rule deviations)

- **Files not in the plan's list.** `core/t64_internal.h` (the static inline `t64_internal_copy_out`; the plan allows static inline helpers there), `core/include/tendies64/t64_machine.h` (a check-order comment only), `docs/spec/state-container.md` (one paragraph for the report refusal). No other file.
- **The CTest cost needed a follow-up commit.** The registration of the prologue commit carried `COST 1`; the three timing runs after it measured 1.94 to 1.95 s, so `COST 2` followed in `205688d`.
- **The sweep base is the full id.** `scripts/digest-sweep.py` refuses the 12-hex abbreviation that PIN-IMPACT prints; the first sweep call failed in 1 s on the format and the second used `f9124ae9fce134f31b645c6f2911a6f27fa41b9a` (0 differences).
- **No local copy for the profile descriptor entries.** The plan says copy-in entries zero a local and copy min bytes; `t64_profile_hash` and `t64_profile_serialize` read named members only (and `t64_profile_serialize` bounds its overlap checks by `sizeof`), so a copy of 2,304 bytes would add nothing; the floor check is the prologue. `unit.core.api.struct-size` shows a larger descriptor hashes and serializes exactly like the v1 one.
- **The test is in the existing executable.** `test_api_struct_size` gained `--group struct-size` (it needs the boot fixtures and the K1 environment); `unit.core.api.handshake` still runs the default group. `output-status` joined the matrix although the plan does not list it.

**Total deviations:** 2 auto-fixed (one Rule 1, one Rule 3). **Impact:** none on the digests (sweep 0 differences) or on the bound patch (it applies unchanged and all 12 bound CTest names pass in the clone).

## Authentication Gates

None.

## Issues Encountered

- **Unrelated host load.** Load averages of 6 to 10; the clone gate started after the one-minute average fell from 10.4 to 8.3 and no limit was widened. The non-bound ctest took 275 s, the clone ctest 72 s.
- **The DEV-11-01 window is still open.** The nine bound test executables and `test_dp_transport` still do not compile in the shared tree; every verify built the 122 recorded non-bound targets and ran every CTest outside the recorded bound regex. Nothing is pushed.

## Open items for later plans (not acted on here)

- 11-14 (orchestrator): the bound patch is unchanged and was proven to apply on top of these prologues (the clone's nine bound executables all pass); the two identity-block names pass again only after that rebind.
- 11-15 review candidates: `t64_machine_advance` now reads the request and the result `struct_size` before the `NOT_READY` answer and has no span check (it never had one), so a non-NULL garbage pointer on a machine without an image reads memory where it used to return early; the new test file duplicates the counting allocator and the fixture reader of `test_api_lifecycle.c` (the poison helpers of 11-09 are not duplicated: the sentinel is a byte fill of a wrapper).
- The `t64_machine_create` and `t64_machine_trace_drain` size contracts are covered by `unit.core.api.handshake`, not by this group.

## Verification Results

| Check | Result |
|-------|--------|
| RED of Task 1 (`build/gate/11-13-task1/red.txt`) | the advance floor cases fail before the prologue (40 failures), pass after |
| RED of Task 2 (`build/gate/11-13-task2/red.txt`) | 104 failures over the seven unprotected entries, pass after |
| `cmake --build --preset gate --target <122 recorded non-bound targets>` | exit 0 (-Werror) |
| `ctest --test-dir build/gate -R '^unit\.core\.api\.(struct-size\|handshake)$'` | 2 of 2 |
| `ctest --test-dir build/gate -E <bound regex>` at the final code (digest refresh in the working tree) | 263 of 263 (275 s); the first pass at the earlier code was 260 of 263 (see Deviation 1) |
| scratch clone: `cmake --preset gate`, `cmake --build --preset gate`, `ctest --preset gate -E '^(harness\.rdp-oracle-authority\|harness\.phase10-qualification)$'` | build exit 0; 273 of 273 in 72 s |
| scratch clone: `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` | exit 2, `authored corpus SHA-256 changed: core/tests/test_rdp.c` |
| `python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head WORKTREE` in the clone | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (41 s) |
| `python3 -B scripts/check-rsp-no-hle.py --build build/gate` after the refresh | PASS, `sources_scanned=85`, exit 0 |
| `ctest -R '^(unit\.core\.api\.(handshake\|struct-size)\|repo\.abi-prefix\|repo\.rsp-no-hle)$'` | 4 of 4 |
| `python3 -B scripts/gen-prefix-header.py --check build/gate/core/libt64_core.a` | ok (35 public functions, 220 renames) |
| `sh scripts/check-test-inventory.sh --preset gate` | PASS, count 275 |
| `ctest -N -R '^unit\.core\.api\.'` and `ctest -N -L phase11` | 13 and 21 names (the regex row 13) |
| `sh scripts/pii-scan.sh --tree` | findings=0 |
| shape: `git log --first-parent --name-only 715c790..HEAD` | no bound file, no authority checker, no authority record in any commit |

## Known Stubs

None.

## Threat Flags

None. T-11-21 (short or long caller structs on every in/out entry) is mitigated by the min(caller, known) copy, the absent struct_size write and the floor refusal before any state change; `unit.core.api.struct-size` checks the trailing bytes and the member.

## Next Phase Readiness

Plan 11-14 (the orchestrator-only bound commit and rebind) is next; it applies `build/11-12-bound/bound-callsites.patch` on a tree that now holds these prologues (proven in the clone). API-01 is not complete (the installed consumer, the installed-object lint and the freeze review are ahead), so no requirement is marked complete. Nothing was pushed.

## Self-Check: PASSED

- FOUND: core/tests/test_api_struct_size.c, build/11-13-bound/prologues.patch, build/11-13-bound/nohle-refresh.patch, build/gate/11-13-task1/red.txt, build/gate/11-13-task2/red.txt
- FOUND commits (ancestors of HEAD): 24d45e8, 96d3323, 205688d, e76c7e1
