# Phase 11 independent review record

One section per phase-wave, written by the review plans (11-15 for W0 and the SHAPE wave, 11-21 for
the INSTALL wave, 11-25 for the final audit). Each review reads the wave's diff fresh in a scratch
clone of the committed head under `build/11-review/` (never in the shared build tree: no
`ninja -t clean`, no fixture rebuild, no mutant run there) and re-runs the cited commands instead of
trusting the plan summaries. Evidence tier: code observation and command results at the commit named
in each section; a software-interface claim only, nothing here is a compatibility, accuracy or hardware
claim. No oracle material (D5) was used. All paths are repo-relative.

## SHAPE (11-01 to 11-14)

W0 (plan 11-01, records only) and the SHAPE wave (plans 11-02 to 11-14). Reviewed by plan 11-15.

| Item | Value |
|------|-------|
| Wave 0 base | `f9124ae9fce134f31b645c6f2911a6f27fa41b9a` (extracted from PIN-IMPACT.md "Wave bases" with the base idiom: exactly one 40-hex line, an existing commit, an ancestor of the head) |
| Reviewed head | `1d451b9b250f3707d8d055c590cc5e87bcff045d` (74 commits after the base; 158 files changed, 138 of them outside `.planning/`) |
| Host | Darwin arm64, Apple clang 21.0.0, cmake 4.4.3, ninja 1.13.2; 1-minute load 5 to 12 during the scratch gate (unrelated processes on the same machine, so seconds are upper bounds for a quiet host) |

### Task 1: scratch gate of the SHAPE head

Method (the Phase 10 W0-01 form): `git clone --no-checkout --no-hardlinks` of the main repository into
`build/11-review/shape/src`, `git checkout --detach 1d451b9b250f3707d8d055c590cc5e87bcff045d`, and a copy
(not a symlink) of `build/tools/install` into the clone's `build/tools/install`. `git archive HEAD` of the
clone and of the main repository have the same SHA-256 (`94296d41599a91b2d1c5b9d9a01b4b09d7deb98ea6dc1c7e88a62d92a7eee727`),
so the gated tree is exactly the committed head; `git status --short` of the clone is empty. The gate
output was written outside the clone tree (`build/11-review/shape/gate.out`, ignored, not committed), so
`gov.pii.repo` sees no untracked output file (the 11-14 lesson).

Command: `sh scripts/gate.sh` inside `build/11-review/shape/src`, default environment (no `CI` variable).

Result: `[gate] PASS`, `[gate] total 162s`, exit 0. Steps in seconds: sh-n 0 (35 scripts), lint-selftest 5,
fixture-qualification 1, configure 1, build 5, boot-evidence-selftest 14, test-inventory-selftest 34,
test-inventory 1, ctest 92 (`100% tests passed out of 275`, `Total Test time (real) = 91.39 sec`), core-lint 9.
No name was excluded; `harness.rdp-oracle-authority` and `harness.phase10-qualification` pass (the
identity block of 11-12 to 11-14 is cleared by the bound commit). No limit was widened.

### Task 2: mechanical checks (a) to (n)

`BASE` is `f9124ae9fce134f31b645c6f2911a6f27fa41b9a` and `HEAD` is the reviewed head `1d451b9b250f`
for every check, run inside the clone unless the row says "shared tree" (the remote-ref check needs
the shared repository's `origin`). The reviewer's own scripts (a hunk classifier, a padding probe and
a public-API probe) are scratch files outside the repository and are not committed; their results are
stated here.

| Check | Command or method | Result |
|-------|-------------------|--------|
| (a) digest sweep | `python3 -B scripts/digest-sweep.py --base BASE --head HEAD` | `sweep: 1599 shared lines, 0 differences, 0 head-only lines`, exit 0, 54 s (build 5 s per tree, sweep 46 s per tree) |
| (b) bound files moved once | `git log --format=%H BASE..HEAD --` the nine bound files; `git diff --name-only BASE..HEAD -- docs/spec/rdp-vi-cases.json fixtures/rdp/fill.S scripts/check-rdp-vi-cases.py runner/tests/test_rdp_video.c` | exactly one commit, `e66625a6494863ba6921da575fdb86e379a1c76d`; the second command prints nothing. `git show --name-status e66625a` lists the nine bound files and the three authority files (12 paths, all `M`) |
| (c) hunk classes, re-derived | own classifier over `git show -U0 e66625a -- core/tests` (110 hunks): for each hunk the added text is reverse-transformed (`&(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(A, B)` to `A, B`, the trace `(uint32_t)sizeof(t64_trace_record)` argument removed, each `T64_*_INIT(args)` to `{args}`, each bare `T64_*_INIT` to `{0}`), all-zero brace lists canonicalized, whitespace removed, then compared with the removed text; hunks with no API object form are read by hand | 17 category (a) (16 create calls, 1 trace drain), 92 category (b) (macro uses across those hunks: 16 `T64_HOST_ALLOCATOR_INIT`, 16 `T64_STOP_REQUEST_INIT`, 35 stop-result forms, 54 `T64_VIDEO_PACKET_INFO_INIT` at 52 sites incl. the two uninitialized declarations `test_vi.c` 715 and 1894 and the two arrays of two, 1 `T64_CPU_STOP_DETAIL_INIT`), 1 category (c): `test_rdp.c` 486 `t64_host_storage image = {NULL, 0u, 0u};` to `{0}`, value-neutral, the owner's answer A of 2026-11-25 (DR-27). Equal to the 11-01 inventory (17 / 92 / 1, 110 hunks) without reading it first. No other (c) hunk: not blocking |
| (d) authority still blocked | `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked`; `--require-approved`; `git diff BASE..HEAD -- scripts/check-rdp-oracle-authority.py` | exit 0 `authority record valid; blocked: no oracle operation is authorized`; exit 2 `no pinned competent owner/legal decision; oracle operations remain blocked`; the checker diff is the `PINNED_REQUEST_SHA256` line only (`e131cbe7...` to `75f2b651...`). The JSON record diff is the nine `bound_authored_inputs` rows (sha256 and blob id) and `request_sha256`; the Markdown record gains one two-line paragraph; all three in `e66625a` only |
| (e) isolated no-HLE refreshes | `git log BASE..HEAD -- scripts/check-rsp-no-hle.py` and each commit's `-U0` diff | five commits, each touching only the checker: `7592675` (4 digest lines: `t64_audio_copy`, `t64_audio_info`, `t64_video_copy`, `t64_video_info`), `fbe5704` (5: `t64_ai_event`, `t64_audio_info`, `t64_vi_event`, `vi_v8_event`, `t64_video_info`), `96d3323` (2: `t64_audio_info`, `t64_video_info`), and `270dc51` plus `0b3d911` (symbol lookups through the prefix map, no `REVIEWED_BODY_DIGESTS` value). Each changes exactly the entries its subject names. `python3 -B scripts/check-rsp-no-hle.py --build build/gate` passes (`sources_scanned=85`) |
| (f) nm golden | `nm -g build/gate/core/libt64_core.a`, defined globals; `python3 -B scripts/gen-prefix-header.py --check build/gate/core/libt64_core.a` | 255 defined globals: 35 without the `t64i_` prefix, all the DR-04 public functions (`t64_abi_version` ... `t64_video_info`), and 220 `t64i_` names; `ok (35 public functions, 220 renames)` |
| (g) stale freeze text | `git grep -n -i -E 'until (the )?Phase 10' -- core runner tools`; `sh scripts/check-stale-freeze-text.sh` | grep prints nothing (exit 1); `stale-freeze-text: files=296 findings=0` |
| (h) init guard, every file | `python3 -B scripts/check-api-init.py` (no flag) | `check-api-init: files=183 findings=0 excluded=1` (the one exclusion is the guard's own built-in list, not the oracle-bound flag), exit 0 |
| (i) ABI header and classes | `python3 -B scripts/check-abi.py header --include core/include`; `classes --header core/include/tendies64/t64_machine.h --table docs/abi/status-classes.txt`; `--selftest` | `c99 ok`, `c++17 ok`, `header findings=0`; `classes constants=47 rows=47 findings=0`; selftest 10 cases, 0 failed |
| (j) CI files unchanged | `git diff --name-only BASE..HEAD -- .github/workflows/ci.yml scripts/check-ci-yml.sh scripts/gate.sh` | empty |
| (k) PII over the range | `sh scripts/pii-scan.sh --range BASE..HEAD` | `pii-scan: tree_files=skipped commits=74 findings=0` |
| (l) DEV-11-01 window | shared tree: `git branch -r --contains C` for every commit of `0338380^..e66625a` (13 commits), after `git fetch origin`; PIN-IMPACT "Shape steps" rows 11-12 to 11-14 | no window commit is on any remote ref (`origin/main` is `167a084`, the Phase 10 close). Code commits in the window: `0338380` (flip), `24d45e8` (prologues), `96d3323` (no-HLE refresh), `205688d` (COST 1 to 2), `e66625a` (bound commit). Pre-commit scratch-clone evidence is recorded for `0338380` (clone `build/11-12-bound`, flip plus bound patch, build of every target, ctest 272 of 274 with exactly the two identity-block names failing), for `24d45e8` and `96d3323` together (clone `build/11-13-bound`, ctest 273 of 273 with `-E` of the two identity-block names) and for `e66625a` (clone `build/11-14-bound`, `sh scripts/gate.sh` stops at ctest with exactly the two identity-block names, then the shared-tree gate PASS 275 of 275 after the rebind). The evidence form is the identity-block wording of the orchestrator amendment `36fc5ac`, not a literal `[gate] PASS`; `205688d` changes one CTest `COST` property and has no run of its own (finding S-10) |
| (m) COST on every phase11 name | `ctest --test-dir build/gate -L phase11 --show-only=json-v1` | 21 names, every one with `COST` (1, except `unit.core.api.bus-carve-out` 3 and `unit.core.api.struct-size` 2) and `TIMEOUT` 30 or 60 |
| (n) codemod and flip callers | own classifier (with all twelve init macros) over `git show -U0` of the codemod commits `2be1265`, `7f50c69`, `8201262`, `a435fa1`, `2ff6931`, `fbeb9ab`, `ad1dd6c`, `e22030d` (511 hunks) and of the 66 caller files of the flip `0338380` (`core/tests/test_api_surface.c` needed none); every residual hunk read by hand | codemod: 385 hunks reverse-transform exactly; the 126 residual hunks are the hand-conversion kinds the 11-05 to 11-10 records name (`memset(&x, 0, sizeof x)` to `x = (T)T64_X_INIT`; `memset(&x, 0xA5, ...)` to a poison helper that starts from the INIT form and sets every other member non-zero by name, plus the new helper bodies and comments; uninitialized `T x;` to the INIT form; member-by-member allocator fills to one INIT assignment; non-zero `t64_cpu_stop_detail` sentinels to INIT plus member assignments; a removed `memset` after an INIT-initialized declaration or array) and one changed check: `core/tests/test_state_import.c` `case_container_refusals`, the NULL-machine row now poisons the report and checks it stays untouched instead of reading `report.reason == T64_STATE_REASON_ARG_NULL` (commit `2ff6931`, recorded as the 11-08 Rule 1 deviation; finding S-04). Flip callers: 250 hunks, all create-info or trace call forms, the three descriptor `struct_size` initializers (`core/tests/test_profile.c` 143 and 340, `runner/t64_skeleton_probe.c` 42) and nine value-neutral `t64_host_storage` `{NULL, 0u, 0u}` to `{0}` (design point 4 of 11-12). No other change |

#### Decision-by-decision read

Each row reads the code at the reviewed head against a locked or autonomous CONTEXT decision that the
SHAPE wave implements (DR numbers of PIN-IMPACT "Decision record").

| Decision | Where implemented | Test names | Verdict |
|----------|-------------------|------------|---------|
| Leading caller-filled `struct_size` on every in/out struct, `min(caller, known)`, refuse below v1 with state unchanged, no `sType`/`pNext` (DR-06) | header structs `t64_host_storage` ... `t64_save_info` (12 structs carry it, `t64_trace_record` exempt with `record_size`); v1 floors `core/t64_internal.h` (`T64_*_V1_SIZE`, `_Static_assert` sizes 32/32/24 on 64-bit, 2304/16/16/24/24/72/488/280/32/24 everywhere); `t64_internal_copy_out` (copies bytes after `struct_size`, at most `min(caller, known)`); prologues in `t64_machine_advance`, `t64_machine_cpu_stop_detail`, `t64_machine_output_drain`, `t64_machine_output_status`, `t64_machine_state_import`, `t64_video_info`, `t64_audio_info`, `t64_machine_save_info`, `t64_profile_hash`, `t64_profile_serialize`, `t64_machine_trace_drain` (stride `record_size`), `t64_machine_create` (create-info, allocator, profile) | `unit.core.api.struct-size` (nine entry cases, floor and larger caller), `unit.core.api.handshake`, `repo.api-init-guard` | implemented; the core reads the 4-byte `struct_size` of a non-NULL caller pointer before the not-admitted answer (advance) or the span check (output status), see S-06 |
| Create-info with `struct_size` and `abi_version`, distinct ABI mismatch status, `t64_abi_version()` (DR-02, DR-05) | `core/t64_machine.c` `t64_machine_create` (order: `machine_out`, info, info size, `abi_version_is_accepted`, allocator and profile, then the older checks, all before the first allocation; host storage overlapping the create-info is `BAD_STORAGE`), `abi_version_is_accepted` (major equal; minor equal while major is 0), `t64_abi_version` | `unit.core.api.handshake` (minor +1, -1, major +1, 0 all `ABI_MISMATCH`), `unit.core.api.abi-version` | implemented |
| Three classes, one status-class table, every status and stop reason exactly once, poison never from a caller fault (DR-01, DR-11, DR-14 to DR-16) | `docs/abi/status-classes.txt` (47 rows for 47 constants), `scripts/check-abi.py classes` | `repo.abi-status-classes`, `repo.abi-selftest`, `unit.core.api.refusals-never-poison`, `unit.core.api.defect-sites` | implemented; two defensive branches answer a refused status or a zero instead of the defect class for an internal inconsistency that no caller input reaches (S-02) |
| Poison scope: one flag in the instance outside digest and state, set only by `t64_internal_defect`; entry order NULL, poisoned, in-call; destroy and the query unguarded; destroy always releases (DR-10) | `core/t64_internal.h` `t64_internal_entry_guard` / `T64_ENTRY_GUARD` (26 entries: the 23 older machine-taking entries plus reset, unload, output status), `core/t64_machine.c` `t64_internal_defect`, `t64_machine_is_poisoned`, `t64_machine_destroy` (releases image, both packet storages and the machine storage, no guard) | `unit.core.api.poison`, `.reentrant`, `.null-machine`, `.defect-sites`, `.refusals-never-poison`, `.not-ready-table` | implemented; destroy called from inside one of the same instance's allocator callbacks is not refused and the header text does not exclude it (S-01) |
| Public reset and unload with accepted and refused cases (DR-04) | `core/t64_machine.c` `t64_machine_reset` (NOT_READY without an image; a failing `reset_guest_state` is a defect), `t64_machine_unload` (NOT_READY without an image; clears guest and image members, releases the image with the in-call flag raised) | `unit.core.api.reset`, `unit.core.api.unload`, `unit.core.api.not-ready-table` | implemented |
| `bool` and `size_t` leave public structs; fixed-width members (DR-07) | header struct bodies (scanned: no `size_t`, `bool`, `int`, `long`, `unsigned` or `enum` member; `char` arrays and pointers only otherwise) | `repo.abi-header`, the `_Static_assert` sizes | implemented; six structs keep implicit alignment padding (S-05) |
| `extern "C"` guards and one `T64_API` macro on every public function (DR-03) | header lines 206 to 208 and the closing guard; 35 `T64_API` declarations, every public function | `repo.abi-header` (self-contained C99 pedantic and C++17 compile) | implemented as text; no target sets hidden visibility, so `T64_API` is a marker and the header's claim that a host "sees exactly the marked functions" overstates it (S-01) |
| Internal prefix `t64i_` through a generated rename header (DR-17) | `core/t64_prefix.h` (220 renames), `scripts/gen-prefix-header.py`, the includes of DR-17 | `repo.abi-prefix` | implemented (macOS); the ELF verification is the first hosted gate-linux run (Task 3) |
| Public init macros, mechanical codemod, compiling commits only outside DEV-11-01 (DR-09, DR-22) | twelve `T64_*_INIT` macros in the header; codemod commits of 11-05 to 11-10 and the flip callers | `repo.api-init-guard`, `repo.api-init-guard-selftest`, check (n) | implemented; one recorded check change (S-04) |
| Retained slots byte-identical to the old public layouts (DR-08) | `core/t64_internal.h` `t64_vi_packet_slot` (488) and `t64_ai_packet_slot` (280) with `_Static_assert` | `unit.core.digest.v26-completeness`, check (a) | implemented |
| Output status call (DR-04, CTX autonomous "Output status") | `core/t64_observe.c` `t64_machine_output_status` | `unit.core.api.output-status` | implemented; see S-02 for the masked inconsistency branches |
| Bus carve-out (DR-13) | header statement for `t64_machine_bus_write` | `unit.core.api.bus-carve-out` | implemented |
| Copy-out ring semantics (CONTEXT locked; lens-api "Rings: the core never hands the host a pointer into core memory") | `t64_video_copy`, `t64_audio_copy`, `t64_machine_output_drain` copy bytes out | the ring tests | implemented for the rings; `t64_machine_profile` (pre-Phase-11) still returns a borrowed pointer into the instance (`&machine->profile`, header "borrowed and valid until machine destruction"); outside the ring scope of the decision, but a struct-growth hazard (S-01) |

#### Findings

Severity scale: blocking (stops the plan before the push and goes to a new numbered plan), warning
(a real gap that does not block the push), info (a recorded observation or accepted limit). The
reviewer fixes nothing.

| id | severity | file:line | finding | disposition |
|----|----------|-----------|---------|-------------|
| S-01 | warning | `core/include/tendies64/t64_machine.h:19-21`, `:567`, `:577-579`, `:641-643` | Public-header text gaps that would be bound by the 11-23 header hash: (1) "A host that links the installed archive sees exactly the marked functions" is not true: no target sets `-fvisibility=hidden` and a static archive exposes all 255 defined globals to the linker (35 public, 220 `t64i_`); the enforced property is the `nm` golden plus the `t64i_` prefix. (2) The reentrancy sentence says a call from inside one of the instance's own allocator callbacks is refused, but `t64_machine_destroy` (void, unguarded) called from such a callback frees the instance under the running call; nothing documents that it must not be. (3) `t64_machine_profile` returns a borrowed pointer to the instance's v1-sized copy (`struct_size` set to the v1 size at create); a consumer compiled against a later, larger descriptor must check `struct_size` before reading any later member, and the header does not say so | new numbered plan (header comment edits; optionally `C_VISIBILITY_PRESET hidden` on `t64_core`, which changes nothing for the static link); best placed before 11-23 binds the header hash, otherwise a defect-driven candidate amendment. Orchestrator placement decision |
| S-02 | warning | `core/t64_observe.c:56-59`, `:140-163` | Internal inconsistencies that state import already rejects (`core/t64_state.c:2441-2453`: drain cursor beyond the capture length, ring head or count out of range), so no caller input reaches them, are answered in the refused class: `t64_machine_output_drain` returns `T64_STATUS_RESOURCE_EXHAUSTED` (the table's `RESOURCE_EXHAUSTED` row does not list this site) and `t64_machine_output_status` reports 0 pending text or 0 queued packets. By AMBIG-A/B these are defect sites (INTERNAL_DEFECT and poison). Review candidate (d) | new numbered plan (two branches to `t64_internal_defect`, one class-table note, a white-box case beside `unit.core.api.defect-sites`); class rows are re-bindable until the final bind, so not blocking |
| S-03 | warning | `docs/spec/state-container.md:448` | Check 1 still reads "machine and bytes present (`ARG_NULL`)", but since 11-03 a NULL machine answers `INVALID_ARGUMENT` at the entry guard with the report untouched (status-classes row 1); only a NULL blob gets `ARG_NULL` now | documentation fix in the same new numbered plan as S-01 |
| S-04 | info | `core/tests/test_state_import.c:1290-1293` (commit `2ff6931`) | The one non-mechanical change in the codemod range: the NULL-machine row of `case_container_refusals` now checks that the report stays untouched. At the base the import wrote `ARG_NULL` for a NULL machine; the 11-03 entry guard stopped that, and from 11-03 to 11-07 the row read an uninitialized report and passed on stack residue (the 11-03 gate PASS of this row was by chance). The new check matches the NULL-machine decision and the class table; reverting it would restore an indeterminate read. Review candidate (e): the init guard now makes every public-struct local initialized (183 files, 0 findings), and a scan of NULL-machine calls followed by a member read finds no other case | accepted (recorded 11-08 deviation); lesson for later plans: a change that makes an output untouched must grep the tests that read it |
| S-05 | info | header `t64_host_storage`, `t64_host_allocator`, `t64_cpu_stop_detail`, `t64_output_drain_result`, `t64_audio_packet_info` (three holes), `t64_state_import_report` | `clang -Wpadded` reports eight implicit 4-byte alignment holes, while `t64_output_status` has an explicit `reserved0`. Copy-out builds each record in a zeroed local, so no stack byte leaks; the layout is fixed by each platform ABI, and on a target with 4-byte `uint64_t` struct alignment (i386 System V) the offsets would differ | accepted for the candidate; 11-17's layout table should pin offsets per target; explicit reserved members are a final-bind (Phase 12) question |
| S-06 | info | `core/t64_machine.c:3229-3230`, `core/t64_observe.c:123` | Review candidate (g): `t64_machine_advance` reads `request->struct_size` and `result_out->struct_size` before `NOT_READY` and checks no span (the base had no span check either), and `t64_machine_output_status` reads `status_out->struct_size` before its span check (it needs the size to form the span). A non-NULL invalid pointer is undefined behavior in either order | accepted limit (C pointer contract); the 11-20 call-sequence generator may add an aliasing-span row for advance |
| S-07 | info | `core/t64_machine.c:2731` | Review candidate (b) not reproduced through the public API: a probe over registry descriptors (skeleton, warm, vr4300, rsp-functional) copied under a custom id found `event_queue_capacity` 0 and `sequence_width_bits` 0 refused at create (`INVALID_ARGUMENT`), and capacity 1 or widths 1, 2 and 64 admitted with no poison; the vr4300 descriptor kept under its pinned id with capacity 1 or 2 is refused at create (`INVALID_ARGUMENT`, the pinned-profile check). The 11-03 reproduction edits `machine->profile` after create (white-box) | accepted; the 11-20 generator should include descriptor mutations |
| S-08 | info | `core/t64_state.c` (export post-write branches), `core/tests/test_api_poison.c` | Review candidates (a) and (c): defensive state-export defect branches with no triggering input, and the guest-driven zero-poison case set up white-box (no guest loader in core tests) | accepted (defensive branches; the poison path itself is covered through `t64i_test_force_defect`) |
| S-09 | info | `core/tests/test_api_poison.c:78-101,258-340`, `core/tests/test_api_lifecycle.c:52-76,110,223-305`, `core/tests/test_api_struct_size.c:56-82,492,1196` | Review candidate (f): the counting allocator and release (three copies), the fixture reader (two) and nine `poison_*` helpers (two, plus `poison_import_report` in `core/tests/state_import_helpers.h`, `poison_save_info` in the save tests and `poison_drain_result` in `core/tests/test_isviewer.c`). A public struct that gains a member must be poisoned in every copy, or an "untouched" check becomes vacuous for the new member | new numbered plan or fold into 11-20 (one shared test header); orchestrator choice |
| S-10 | info | PIN-IMPACT "Shape steps" 11-12 to 11-14 | Check (l): the window's pre-commit evidence is a clone `ctest` run with the identity-block exception (amendment `36fc5ac`), not `sh scripts/gate.sh`; `205688d` (one `COST` property) has no run of its own. The head passes the full gate (Task 1) and no window commit was ever published | accepted |
| S-11 | info | `core/CMakeLists.txt` `unit.core.api.struct-size` | `COST 2` against 2.54 s native in the clone gate (load 10 to 12; 11-13 measured 1.94 to 2.00 s at load 8); `TIMEOUT 60` is above 3.5 times either | accepted (host load); re-measure in the 11-21 ledger row |
| S-12 | info | `scripts/check-rsp-no-hle.py` (`0b3d911`) | The prefix-map fallback reads the checker's own repository header when a control fixture carries none; the controls link the real build objects compiled from that header, so the map is the right one | accepted |

Review candidates (h) and (i) are measured by Task 3 (the first hosted gate-linux run of `repo.abi-prefix`
and the cold hosted gate-macos time against the 900 s limit). Accepted exceptions carried: the G5 plan-size
note (four plans over 15 files) and the Phase 11 G4 exception of PIN-IMPACT "Gate-growth ledger".

Result: zero blocking findings; three warnings (S-01 to S-03) and nine info rows. The SHAPE wave's bound-file
edits are independently confirmed call-site-only (check (c)), and the wave moved no digest (check (a)).

### Task 3: push, hosted pair and the Install base

Before the push, in the shared tree with a clean committed tree at `43b31e2c0511155622dd8428dcd2f2368fe40501`
(the reviewed head plus the two 11-REVIEW.md commits; `git diff --quiet 1d451b9 HEAD -- core runner tools tests
scripts fixtures docs` exits 0): `sh scripts/preflight-phase11.sh` (full, no flag) reports `[preflight] PASS` in
102 s (oracle-authority 1, ctest-counts 0, abi-cheap 1, guards 2, host-load 0 with a WARN at load 14.6, head-ci 1
with `origin/main` `167a084` completed success, timeout-audit 0 with 128 TIMEOUTs and lowest ratio 2.9, pii 2,
linux-compile 64 with 232 units and the act image cc 13.3.0, post-gate-replay 31 with 11 `ci.yml` steps).
`sh scripts/pii-scan.sh --tree` (1377 files) and `--range origin/main..HEAD` (79 commits, the whole unpublished
range including the commits of the earlier unpublished PII repair) report 0 findings, and
`sh scripts/rights-scan.sh --tree --range origin/main..HEAD` reports `tree=1377 history=423 findings=0` (exit 0).

Push: `git push origin 43b31e2c0511155622dd8428dcd2f2368fe40501:refs/heads/main`, a fast-forward from `167a084`
(no force, no tag, no other branch). Hosted run 45736981784 (`ci.yml`, event push, head `43b31e2`): both jobs
success; gate-macos 445 s (Gate 367 s, ctest 219 s, `100% tests passed out of 275`), gate-linux 251 s (Gate 204 s,
ctest 132 s, 275 of 275); Binutils cache hit on both, so the sample is warm. `docs/ci-reviews/LOG.md` has its Runs row
and finding F-127; PIN-IMPACT "Gate-growth ledger" has the SHAPE row.

- Review candidate (h): `repo.abi-prefix` passes on gate-linux (0.08 s) and gate-macos (0.11 s); the gate-linux run is
  the first ELF check of the prefix golden and of the generator's `nm` parsing. An empty or wrong parse cannot pass:
  `--check` fails on a header entry absent from the archive (`stale`, the 11-11 control).
- Review candidate (i): the cold gate-macos time was not measured (warm cache; deleting the cache entry and
  dispatching a cold run is an orchestrator step that this plan does not name). Projections from this warm sample:
  757 s (attributable growth), 791 s (cold delta of F-123 to F-124) and 810 s (whole warm delta), margins 143 s,
  109 s and 90 s. Two are under the 120 s warning margin; none is above 900 s, so the G4 reversal rule is not
  triggered. The 21 SHAPE names sum 19.62 s on hosted macOS and 14.77 s on hosted Linux.

The Install base is recorded in PIN-IMPACT "Wave bases" by its own commit after this record.

## INSTALL (11-16 to 11-20)

The INSTALL wave (plans 11-16 to 11-20) and the numbered defect-fix plans executed inside it (11-26 for
the SHAPE warnings S-01 to S-03, run after 11-15; 11-27 for finding F-20-A, run after 11-20). Reviewed by
plan 11-21.

| Item | Value |
|------|-------|
| Install base | `06e31ecab5b84c08996aba79cb3ab6e29bb021cc` (extracted from PIN-IMPACT.md "Wave bases" with the base idiom: exactly one 40-hex line, an existing commit, an ancestor of the head) |
| Reviewed head | `862920da140e4a9a3936162abae7e660e0b99205` (56 commits after the base; 67 files changed, 53 of them outside `.planning/`) |
| Host | Darwin arm64, Apple clang 21.0.0, cmake 4.4.3, ninja 1.13.2; 1-minute load 8 to 11 during the scratch gate (unrelated processes on the same machine, so seconds are upper bounds for a quiet host) |

### Task 1: scratch gate of the INSTALL head

Method (the 11-15 form): `git clone --no-checkout --no-hardlinks` of the main repository into
`build/11-review/install/src`, `git checkout --detach 862920da140e4a9a3936162abae7e660e0b99205`, and a copy (not a
symlink) of `build/tools/install` into the clone's `build/tools/install`. `git archive` of the head in the clone
and in the main repository have the same SHA-256 (`1050cef7b6e604426bf43588716aa386ba3cc600b5ac2c28c9a47533ada4b864`),
so the gated tree is exactly the committed head; `git status --short` of the clone is empty before and after the
gate. The install fixtures (`install.setup`), the consumer builds and the control scratch directories ran inside
the clone's own `build/gate`; the gate output was written outside the clone tree (`build/11-review/install/gate.out`,
ignored, not committed).

Command: `sh scripts/gate.sh` inside `build/11-review/install/src`, default environment (no `CI` variable).

Result: `[gate] PASS`, `[gate] total 160s`, exit 0. Steps in seconds: sh-n 0 (38 scripts), lint-selftest 5,
fixture-qualification 2, configure 0, build 6, boot-evidence-selftest 14, test-inventory-selftest 33,
test-inventory 1, ctest 90 (`100% tests passed out of 324`, `Total Test time (real) = 89.49 sec`), core-lint 9.
No name was excluded and no limit was widened. Phase 11 names: `ctest -N -L phase11` lists 70 (21 at the SHAPE
review); the whole preset has 324 names, 49 more than the 275 of the SHAPE head.
