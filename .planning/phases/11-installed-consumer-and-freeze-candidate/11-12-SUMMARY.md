---
phase: 11-installed-consumer-and-freeze-candidate
plan: 12
subsystem: api
tags: [struct-size, create-info, abi-handshake, record-size, codemod, shape-wave, dev-11-01]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: plans 11-05 to 11-10 (init macros and the codemod conversion of every non-bound file), 11-11 (the symbol prefix, so the flip adds no external symbol and repo.abi-prefix passes unchanged)
provides:
  - the candidate struct layouts: a leading caller-filled uint32_t struct_size on every in/out struct, fixed-width members (no size_t or bool member), the ten init macros filling struct_size with sizeof
  - t64_machine_create_info and T64_MACHINE_CREATE_INFO_INIT; t64_machine_create(const t64_machine_create_info *, t64_machine **) with the DR-05 refusal order (INVALID_ARGUMENT, STRUCT_TOO_SMALL, ABI_MISMATCH) before any allocation
  - the record_size parameter of t64_machine_trace_drain (refused below the v1 size, used as the record stride otherwise)
  - core/tests/test_api_struct_size.c and CTest unit.core.api.handshake
  - scripts/codemod-phase11.py modes --create and --trace and INIT-initialized array support (selftest 19 of 19)
  - the scratch-proven call-site patch of the nine bound files (build/11-12-bound/bound-callsites.patch, SHA-256 in PIN-IMPACT) and its per-hunk classification, ready for the orchestrator's 11-14 commit
affects: [11-13, 11-14, 11-15, 11-16, 11-17, 11-19]

actuals:
  tokens: 27700
  tasks: 3
  commits: 3

plan_head_before: d1bc22c7121afdf568b3007f0e70ff50307ac0c4
plan_head_after: 36fc5ac673ce5ef923240b2935e07a5f70b1a16f

tech-stack:
  added: []
  patterns:
    - "the create-info handshake: struct_size, then abi_version (equal major; equal minor while the major is 0), then the allocator and profile struct_size floors, all before the first allocate callback"
    - "the core is the caller of t64_host_storage: it sets struct_size to the v1 size before every allocate callback (the machine, video and audio slots in t64_machine.c and the three sites in t64_rom.c)"
    - "an entry that builds its result in a local copies the caller's struct_size back unchanged (t64_machine_cpu_stop_detail, t64_machine_output_drain, t64_machine_state_import); the callee never writes the member"
    - "t64_profile_hash and t64_profile_serialize skip struct_size, so every registered profile hash is unchanged"
    - "the flip is proven in a marked scratch clone with the bound patch applied before it is committed (W-3); the clone is the only place the nine bound files are ever converted"

key-files:
  created:
    - core/tests/test_api_struct_size.c
  modified:
    - core/include/tendies64/t64_machine.h
    - core/t64_internal.h
    - core/t64_machine.c
    - core/t64_rom.c
    - core/t64_trace.c
    - core/t64_observe.c
    - core/t64_state.c
    - core/CMakeLists.txt
    - scripts/codemod-phase11.py
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - docs/abi/status-classes.txt
    - core/tests (52 files), runner (7 files) and runner/tests (7 files): the 66 mechanical callers
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md
    - .planning/phases/11-installed-consumer-and-freeze-candidate/11-13-PLAN.md
    - .planning/phases/11-installed-consumer-and-freeze-candidate/11-14-PLAN.md

key-decisions:
  - "Option A (orchestrator, 2026-11-25) for the identity-block conflict in the plan's own gates: the scratch clone with the bound patch fails exactly harness.rdp-oracle-authority and harness.phase10-qualification until the 11-14 rebind; the flip was committed on that evidence and the 11-13 and 11-14 gates were amended instead of widening anything"
  - "create refuses host storage that overlaps the create-info object (T64_STATUS_BAD_STORAGE) as it already does for the profile and the length word, so a hostile allocator cannot alias the caller's info while the core reads it"
  - "no-HLE refresh commit skipped: check-rsp-no-hle.py --build build/gate exits 0 at the flip commit, so no sealed body was re-pinned"

patterns-established:
  - "Before a coupled flip that cannot be built in the shared tree, prove it in a marked scratch clone, record the patch SHA-256 and a per-hunk category table, and name the expected failing names in the plan wording rather than leaving the contradiction to the executor"

requirements-completed: []

coverage:
  - id: D1
    description: "Every public in/out struct carries a leading struct_size; struct members are fixed width; t64_machine_create takes the create-info with the DR-05 refusal order and refuses ABI skew and short structs before any allocation; record_size is the trace stride"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "ctest unit.core.api.handshake (create-info NULL, size 0 and v1-1, abi minor+1 and major+1, allocator and profile v1-1, trace record_size below v1 and above v1; profile hashes unchanged); python3 -B scripts/check-abi.py header --include core/include --header tendies64/t64_machine.h; grep -c -E '^[[:space:]]+(size_t|bool) [a-z_]+;' core/include/tendies64/t64_machine.h prints 0"
        status: pass
      - kind: integration
        ref: "ctest -E <bound regex> in the shared tree at the flip commit: 262 of 262; ctest --preset gate in the scratch clone with the flip and the bound patch: 272 of 274 (all 12 bound CTest names pass; the two failures are the identity block resolved at 11-14)"
        status: pass
    human_judgment: false
  - id: D2
    description: "The 66 non-bound callers are rewritten by one codemod invocation and the nine bound files' call-site patch is scratch-proven and classified hunk by hunk (97 hunks, categories a, b and the one owner-approved c), ready for the orchestrator commit of plan 11-14"
    requirement: "API-01"
    verification:
      - kind: integration
        ref: "python3 -B scripts/codemod-phase11.py --check core/tests runner tests (only INIT-initialized arrays listed); the scratch clone builds every target with -Werror; PIN-IMPACT 'Bound patch hunks' table; git diff --quiet HEAD on the nine bound files, the authority checker and docs/evidence"
        status: pass
      - kind: integration
        ref: "python3 -B scripts/digest-sweep.py --base f9124ae9fce1 --head 0338380171fc: sweep: 1599 shared lines, 0 differences, 0 head-only lines (the same in the scratch clone with the bound patch)"
        status: pass
    human_judgment: false

duration: 40 min
completed: 2026-11-25
status: complete
---

# Phase 11 Plan 12: The struct_size Flip Summary

**Every public in/out struct now starts with a caller-filled `struct_size` and has fixed-width members, `t64_machine_create` takes a `t64_machine_create_info` that refuses ABI skew and short structs before any allocation, `t64_machine_trace_drain` takes `record_size`, and the 66 non-bound callers moved in one codemod invocation; the bound patch for the nine oracle files was proven in a scratch clone first, and the sweep reports 0 differences.**

## Performance

- **Duration:** about 40 min wall from the plan base commit (18:47 local) to this summary, across the first executor (Tasks 1 and 2 and the scratch proof of Task 3, which stopped at the clone-gate result) and this continuation (decision, flip commit, record, plan amendments, summary); the host load was 8 to 17 from unrelated processes and no limit was widened
- **Tasks:** 3 of 3 (Task 1 tracer, Task 2 handshake cases, Task 3 scratch proof and commit); one coupled commit by design
- **Files modified:** 79 in the flip commit (1 created); 11-13-PLAN.md, 11-14-PLAN.md and PIN-IMPACT.md in two documentation commits; 110,936 characters added over the three commits

## Accomplishments

- **Candidate layouts and macros.** `core/include/tendies64/t64_machine.h` gives the ten in/out structs a leading `uint32_t struct_size` (`t64_audio_packet_info` already had one), turns the seven `size_t` and two `bool` struct members into `uint64_t` and `uint32_t`, defines `t64_machine_create_info` and `T64_MACHINE_CREATE_INFO_INIT(allocator, profile)`, makes every DR-09 init macro fill `struct_size` with `sizeof`, and adds `T64_*_V1_SIZE` constants in `core/t64_internal.h`.
- **Handshake.** `t64_machine_create` checks in the DR-05 order: NULL info (INVALID_ARGUMENT), info `struct_size` (STRUCT_TOO_SMALL), `abi_version` (ABI_MISMATCH when the major differs, or the minor differs while the major is 0), then the allocator and profile pointers and their own `struct_size` floors, all before the first allocate callback; `*machine_out` stays NULL and the allocator is untouched on every refusal. The registered profile descriptors set `struct_size`, and `t64_profile_hash` and `t64_profile_serialize` skip it, so no profile hash moved (`unit.core.api.handshake` compares every registered profile).
- **Allocator contract (W-1).** The core sets `t64_host_storage.struct_size` before every allocate callback in `core/t64_machine.c` and at the three `core/t64_rom.c` sites; the retained copies of the allocator and profile keep the v1 size whatever the caller's larger size was.
- **record_size.** `core/t64_trace.c` refuses a `record_size` below the v1 record with outputs untouched and otherwise writes records at the caller's stride, at most v1 bytes each, trailing bytes untouched; `t64_machine_trace_drain` takes the parameter after `records_out`.
- **Codemod.** `scripts/codemod-phase11.py` gained `--create` and `--trace` (and the descriptor `struct_size` for brace-initialized non-bound descriptors), refuses the nine bound paths outside a marked scratch root, and accepts INIT-initialized arrays including two-dimensional ones (selftest 19 of 19). One shared-tree invocation converted 197 sites in 67 scanned files (141 create, 53 trace, 3 descriptors; `core/tests/test_api_surface.c` needed none) and reported 0 hand sites.
- **Bound-patch proof (W-3).** In the marked scratch clone (detached at the shared HEAD, flip patch applied) the same codemod converted the nine API-using bound files (17 call expressions, 118 initializers, three hand edits inside the clone: two `t64_video_packet_info` arrays of `test_vi.c` and the owner-approved `test_rdp.c` line 486), 97 hunks, SHA-256 `beb2cafb0a8a66a66e0cdef84a9233cef11973cbd66ed1e84f2f704c50f262cb`, every hunk classified (a), (b) or the one approved (c) in PIN-IMPACT. The clone configures and builds every target in 8 s and `ctest --preset gate` passes 272 of 274 in 85 s with all 12 bound CTest names passing.

## Task Commits

1. **Tasks 1 to 3 (one coupled commit): the flip** - `0338380171fc198c175b78e70e499c1cee554fac` (feat); its parent is `d1bc22c7121afdf568b3007f0e70ff50307ac0c4`, the head the scratch clone was taken from
2. **Shape-step record** - `3bcf1500fbcf8b588e13c3463011c3dcd7bc5a2e` (docs): the PIN-IMPACT row 11-12, the plan evidence and the identity-block resolution
3. **Plan amendments** - `36fc5ac673ce5ef923240b2935e07a5f70b1a16f` (docs): the W-3 clone-gate wording of 11-13 and 11-14
4. **Isolated no-HLE refresh:** none (skipped, see Deviations)

**Plan metadata:** the summary and state commits that follow this file. `commits: 3` is `git rev-list --count d1bc22c..<head at write time>`.

## Decisions Made

Binding decisions are DR-05 to DR-09 (layouts, handshake order, fixed-width members, macros), DR-22 (the DEV-11-01 window) and DR-27 (the bound-file reading). The choices inside the plan's discretion are in the frontmatter.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Entries that build a result in a local overwrote the caller's struct_size**
- **Found during:** Task 1 (the in/out structs now carry `struct_size`, and the entries build their result in a local)
- **Issue:** `t64_machine_cpu_stop_detail`, `t64_machine_output_drain` and `t64_machine_state_import` assemble their result in a zeroed local and copy it out, which would write 0 into the caller's `struct_size` (the contract of 11-13 is that the callee never writes the member)
- **Fix:** each writes the caller's `struct_size` back unchanged (`detail.struct_size = detail_out->struct_size`, and the same for the drain result and the import report); two lines each in `core/t64_observe.c` and `core/t64_state.c`, neither a sealed source
- **Files modified:** core/t64_observe.c, core/t64_state.c (not in the plan's list)
- **Commit:** `0338380`

**2. [Rule 2 - Missing critical functionality] create did not refuse host storage aliasing the create-info**
- **Found during:** Task 1 (create now reads the caller's info after the allocate callbacks)
- **Issue:** a hostile or buggy allocator could return storage overlapping the info object, so the core would read an info that its own initialization overwrote; create already refused overlap with the profile and the length word
- **Fix:** `range_disjoint_from_object(allocation_range, info, sizeof *info)` joins the storage validation and refuses with `T64_STATUS_BAD_STORAGE`
- **Files modified:** core/t64_machine.c
- **Commit:** `0338380`

**3. [Rule 3 - Blocking] Nine positional `t64_host_storage` initializers did not compile**
- **Found during:** Task 1 (the build with -Werror)
- **Issue:** `{NULL, 0u, 0u}` puts a pointer into the new leading `struct_size` member
- **Fix:** the nine non-bound sites became `{0}` (the core's own sites use `T64_HOST_STORAGE_EMPTY`); the bound file `test_rdp.c` line 486 has the same edit in the scratch patch, category (c), approved by owner answer A
- **Files modified:** core/tests and runner/tests files listed in the codemod diff
- **Commit:** `0338380`

**4. [Rule 3 - Blocking] The codemod check rejected INIT-initialized arrays**
- **Found during:** Task 1 (`python3 -B scripts/codemod-phase11.py --check core/tests runner tests` must list no site outside the bound files)
- **Issue:** the tool listed every array declaration of a macro type as a hand site even when all entries use the INIT form (the 11-07 and 11-10 notes); the verify's `fails_when` ("a remaining site outside the nine bound files") could not pass
- **Fix:** the tool accepts arrays whose entries all use the INIT form, including two-dimensional arrays; `--selftest` grew to 19 cases, all pass; `scripts/codemod-phase11.py` is in the plan's file list
- **Files modified:** scripts/codemod-phase11.py
- **Commit:** `0338380`

### Other differences from the plan text (not rule deviations)

- **Create call sites use the compound-literal form everywhere.** The plan asks for a named local `info` where the caller is a statement; the codemod emits `&(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(allocator, profile)` for statement and expression callers alike (one rewrite rule, no declaration insertion into 141 sites). No behavior differs.
- **The plan's clone-gate wording contradicts its own expected identity block.** Task 3 step 5 and the verify require a fully green `ctest --preset gate` in a clone whose nine bound files differ from the pinned corpus, while the same step expects `check-rdp-oracle-authority.py --expect-blocked` to fail with "authored corpus SHA-256 changed". The two harness tests that run that checker (`harness.rdp-oracle-authority`, and `harness.phase10-qualification` through `test_real_stage_commands_start`) therefore cannot pass in that clone before the 11-14 rebind. The previous executor stopped at this; the orchestrator decided option A (2026-11-25): the two names are the expected identity block (restoring only the nine bound files to the pinned bytes in the clone made both pass; the other 272 pass; the sweep is 0 differences). The flip was committed on that evidence and the 11-13 and 11-14 plan files were amended (commit `36fc5ac`): their pre-rebind clone gates exclude exactly those two names (11-13) or require `gate.sh` to stop at ctest with exactly those names and then run core-lint (11-14), each also requires the checker's exit 2 with "authored corpus SHA-256 changed", and the 11-14 post-rebind gate must be fully green with every name.
- **No isolated no-HLE refresh commit.** `python3 -B scripts/check-rsp-no-hle.py --build build/gate` exits 0 at the flip commit (the change to `core/t64_observe.c` and `core/t64_state.c` touches no sealed body), so no `REVIEWED_BODY_DIGESTS` value was re-pinned and the plan's "skip if nothing is re-pinned" applies.
- **`scripts/tests/test_rsp_qualification.py` is unchanged (I-3).** Its profile-policy seed uses designated initializers and the parser reads named fields only, so `struct_size` needs no seed row; `python3 -B scripts/tests/test_rsp_qualification.py` passes (30 tests).
- **The non-bound ctest was last run at the flip commit, not the staged state.** The committed tree is byte-identical to the staged flip (`git diff --cached --binary` hashed equal to `build/11-12-bound/flip.patch` before the commit); an earlier run during the task flagged two init-guard findings in the then-uncommitted `core/tests/test_api_struct_size.c` and a stray `scripts/__pycache__` file, both fixed or removed before the commit.

**Total deviations:** 4 auto-fixed (one Rule 1, one Rule 2, two Rule 3). **Impact:** none on the digests (sweep 0 differences) or on the bound patch; the three code deviations are part of making the flip compile and keep the "callee never writes struct_size" contract.

## Authentication Gates

None.

## Issues Encountered

- **Unrelated host load.** The load average was 8 to 17 from other processes (VM and container work); the 278 s non-bound ctest and the 85 s clone gate ran at that load with no limit widened and no run repeated for load.
- **The DEV-11-01 window is open.** From the flip commit `0338380` the nine bound test executables and `test_dp_transport` do not compile in the shared tree; every verify of 11-13 builds only the recorded 122-target list (PIN-IMPACT line `- Non-bound targets:`) and runs every CTest outside the recorded bound regex. Nothing is pushed.
- **The shell does not word-split an unquoted variable.** `cmake --build ... --target $TL` needs `bash -c` or `${=TL}` in this zsh environment.

## Open items for later plans (not acted on here)

- 11-13 adds the `struct_size` prologues of the other entries and `unit.core.api.struct-size`; its clone gate follows the amended wording.
- 11-14 (orchestrator): apply `build/11-12-bound/bound-callsites.patch` (SHA-256 above), confirm every category (b) hunk against the PIN-IMPACT table (each lists the public call it belongs to, I-2), and rebind identity-only; the two identity-block harness tests pass again only after that commit.
- The nine bound files must not be edited in the shared tree before then; `scripts/check-api-init.py` without `--exclude-oracle-bound` still reports the 120 category (b) findings by design.

## Verification Results

| Check | Result |
|-------|--------|
| `python3 -B scripts/check-abi.py header --include core/include --header tendies64/t64_machine.h` | pass |
| `python3 -B scripts/codemod-phase11.py --check core/tests runner tests`, `--selftest` | only INIT-initialized arrays listed; selftest 19 of 19 |
| `grep -c -E '^[[:space:]]+(size_t\|bool) [a-z_]+;' core/include/tendies64/t64_machine.h` | 0 (9 before) |
| `grep -c 't64_machine_create(const t64_machine_create_info' core/include/tendies64/t64_machine.h` | 1 |
| RED of Task 2 (`build/gate/11-12-task2/red.txt`) | the handshake cases fail with the checks removed (create returns 0 for size and ABI skew, allocations counted) and pass after |
| `cmake --build --preset gate --target <122 recorded non-bound targets>` at the flip commit | exit 0 (-Werror) |
| `ctest --test-dir build/gate -E <bound regex>` at the flip commit | 262 of 262 passed (278 s) |
| scratch clone: `cmake --preset gate`, `cmake --build --preset gate`, `ctest --preset gate` (flip plus bound patch) | configure and build 8 s; 272 of 274 in 85 s; the 2 failures are the identity block (option A); all 12 bound CTest names pass |
| scratch clone: nine bound files restored to the pinned bytes | `harness.rdp-oracle-authority` and `harness.phase10-qualification` pass (7.5 s and 2.6 s); checker prints "authority record valid; blocked: no oracle operation is authorized" |
| `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` in the clone | exit 2, "authored corpus SHA-256 changed: core/tests/test_rdp.c" |
| `python3 -B scripts/digest-sweep.py --base f9124ae9fce1 --head 0338380171fc` and the clone worktree | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (49 s and 39 s) |
| `python3 -B scripts/check-rsp-no-hle.py --build build/gate` | PASS, `sources_scanned=85`, exit 0 (no re-pin, refresh commit skipped) |
| `python3 -B scripts/gen-prefix-header.py --check build/gate/core/libt64_core.a` | ok (35 public functions, 220 renames) |
| `ctest -R '^(unit\.core\.api\.handshake\|repo\.abi-prefix\|repo\.abi-status-classes)$'` | 3 of 3 |
| `python3 -B scripts/tests/test_rsp_qualification.py` | 30 tests OK |
| `sh scripts/check-test-inventory.sh --preset gate` | PASS, count 274 |
| `git diff --quiet HEAD` over the nine bound files, `scripts/check-rdp-oracle-authority.py`, `docs/evidence` | exit 0 |
| `sh scripts/pii-scan.sh --tree` | findings=0 |
| shape: `git show --name-only --format= 0338380` | 79 files; none of the nine bound files, the checker or the authority record |

## Known Stubs

None.

## Threat Flags

None. T-11-20 (header and library skew) is mitigated at create (`unit.core.api.handshake`). T-11-63 (the DEV-11-01 window) is mitigated by the scratch gate with the classified bound patch before the flip commit; the amended wording of 11-13 and 11-14 keeps that proof before each later code commit in the window, and nothing is pushed or hosted-gated until 11-15.

## Next Phase Readiness

Plan 11-13 is next (the `struct_size` prologues of the other entries and `unit.core.api.struct-size`); it builds only the recorded non-bound target list and runs the amended clone gate. Plan 11-14 is orchestrator-only for its Task 2. API-01 is not complete (the prologues, the installed consumer and the installed-object lint are ahead) and API-02 is partly covered by the later plans, so no requirement is marked complete. Nothing was pushed.

## Self-Check: PASSED

- FOUND: core/tests/test_api_struct_size.c, build/11-12-bound/bound-callsites.patch, build/11-12-bound/flip.patch
- FOUND commits (ancestors of HEAD): 0338380, 3bcf150, 36fc5ac
