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

### Task 2: INSTALL checklist (a) to (l) with a log-level control review

`BASE` is the Install base `06e31ecab5b8` and `HEAD` the reviewed head `862920da140e` for every check, run inside
the clone. Where a check needs a log that the gate deletes on success (a passing control removes its directory),
the reviewer ran a scratch copy of `scripts/check-install.sh` whose only two differences are a fixed `ROOT` (the
clone) and a kept control directory, with `--work` outside the clone; that copy and the reviewer's independent
literal reader are scratch files outside the repository and are not committed. Logs are under the ignored
`build/11-review/install/review/`.

| Check | Command or method | Result |
|-------|-------------------|--------|
| (a) consumer independence | `sh scripts/check-consumer-includes.sh`; a fresh grep of `tests/installed-consumer` for every `#include`, for any `core/`, `runner/` or `tools/` path and for `add_subdirectory` | `consumer-includes: files=17 findings=0`. The includes are `<tendies64/t64_machine.h>` (10 files), the consumer's own `t64c_*.h` and the C system headers `stdbool.h`, `stddef.h`, `stdint.h`, `stdio.h`, `stdlib.h`, `string.h`, `time.h` (the host delay of `hostloop.c`) and `pthread.h` (`threads.c`, built only with `T64C_THREADS`). The four `core/`, `runner/` or `tools/` hits and the one `add_subdirectory` hit are comments; the consumer `CMakeLists.txt` uses only `find_package(tendies64 ${T64C_REQUIRED_VERSION} REQUIRED CONFIG)`, its own sources and `tendies64_BUILD_ID` of the package. No expected value is read from the core or the runner at run time: the literals are generated constants, the hashes are the consumer's own `t64c_sha256.c`, the class table is the hand-written `t64c_classes.h`; digests the core returns are only compared with each other (before and after a call, cadence against cadence) or with the runner-recorded pin |
| (b) literals and B-1 | `python3 -B scripts/check-consumer-literals.py --check` and `--selftest`; the reviewer's own reader (`python3 -I`, no import of the checker) parses `composite.expected.json`, `composite.input.json` and `composite.runner-pins.json` and the text of `t64c_composite.h`, split at the INDEPENDENT and RUNNER-RECORDED PINS banners | `findings=0`; selftest `failures=0`. Own reader: `checks=53 failures=0`: every input, video, audio, transcript (text, 188 bytes, SHA-256 recomputed), polled word and save value (offset, bytes, SHA-256 of the rebuilt 32,768-byte image) equals the INDEPENDENT section; the pins section equals the pins file, whose label is exactly `runner-recorded pin, not independent`; `composite.expected.json` has no completion tick, stop reason, final digest or video packet count, and the INDEPENDENT section names none of them; the cited Phase 10 PIN-IMPACT section 11.3 contains `599,357`, the digest `97f27027...1267c`, `3 video frames` and the save SHA-256. Every case and three-way line that reports one of the four pinned values labels it `runner-recorded pin, not independent` (composite case output, `three-way` lines below); nothing calls them independent |
| (c) three-way | `ctest --test-dir build/gate -R '^installed\.consumer\.three-way$' -V` | passes (3 names with the fixture closure, 1.40 s). Both digests read from the log: runner raw-capture run = runner save-out run = consumer = `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c`, tick 599357 on both sides, stop reason GUEST_COMPLETION, 3 video packets (each line labeled as a runner-recorded pin); the three video packets, two audio packets, the 32,768-byte save and the 188-byte transcript equal byte for byte (labeled independent literals checked by the consumer); `the installed consumer matched every independent literal (12 comparisons)` |
| (d) negative controls, log by log | `ctest --test-dir build/gate -R '^installed\.controls\.' -V` (11 names with `install.setup`, all pass, 4.42 s), then each row once more through the kept-directory copy, reading `controls/<id>/log.txt` | `missing-package`: `CMakeLists.txt:18 (find_package)` "Could not find a package configuration file provided by "tendies64" (requested version 0.1)" with an empty prefix path. `version-mismatch-minor` and `-patch`: the package was found and refused, "considered but not accepted: .../moved prefix/lib/cmake/tendies64/tendies64Config.cmake, version: 0.1.0 / The version found is not compatible with the version requested" for 0.2 and for 0.1.5. `visible-source-tree`: "no repository path" after the scan of `compile_commands.json` and `build.ninja`; `-seed`: "seed detected: 22 line(s) ... name the source include directory". `doctored-header-abi`: the diff of the copied header and `smoke: create status=34 machine=NULL` (ABI_MISMATCH), but the doctored minor is `UINT32_C(322)`, not one higher (finding I-01). `doctored-header-size`: the diff removes `const t64_profile_descriptor *profile;` and its initializer argument, `smoke: create status=35 machine=NULL` (STRUCT_TOO_SMALL). `relocated-spaces`: `smoke: checks=13 failures=0` against `another moved prefix` and the no-earlier-prefix line. `symbol-collision`: link line, `t64i_f32_add` and `t64i_sha256_init` in the archive and no unprefixed name, composite `checks=25 failures=0` at the pinned digest, `f32_add calls: 0`, `t64_sha256_init calls: 0`. `plain-cc`: `cc -std=c99 ... <8 consumer sources> .../libt64_core.a`, `smoke: checks=13 failures=0`. Every row fails (or, for the four positive rows `visible-source-tree`, `relocated-spaces`, `symbol-collision` and `plain-cc`, passes) for its own stated reason; none passes for an unrelated reason |
| (e) API-04 seeds | `ctest --test-dir build/gate -R '^api04\.' -V` | 5 names with the fixture, all pass (7.96 s). Seeded: four `L2-UNDEF-SYMBOL seed_host_services.o` lines naming `clock_gettime`, `fopen`, `getentropy`, `pthread_create`; `unrecorded import: memmove`; `mrs x8, CNTVCT_EL0` (1 hit); `L1-INCLUDE-BANNED seed_banned_include.h:9: banned header <stdio.h>`; `seeded ok (4 of 4)`. Members: 86 extracted in place from the spaced path, archive-level undefined set (Darwin) `__stack_chk_fail __stack_chk_guard bzero memcmp memcpy memset`, the control without the installed allowlist names exactly the stack-protector pair (26 lines); disassembly 96,008 instruction lines; headers 1 file. The set comparison fails in both directions (`unrecorded import`, `recorded import gone`), so a pass on a host equals its recorded rows |
| (f) sanitizer and TSan records | PIN-IMPACT "Install steps" 11-20 and 11-27; re-run `sh scripts/check-install.sh sanitize shipped` once in the clone, plus the 5-second `tsan` and `tsan --seed-race` lanes | rows exist with build ids (shipped `t64-build-v1-02b2ce10...`, instrumented `...2d353331...`, TSan `...c26a7ea4...` of the 11-20 library; 11-27 re-ran shipped only) and are dated by their commits (`949dcbe` and `50b277f`, 2026-11-26), not inside the rows (finding I-08). Re-run at the reviewed head: shipped archive build id `t64-build-v1-6bf5a855b9f166b154e9db35223b7818c28acfef57d3461e8c11b410332349de`, 0 AddressSanitizer references in the archive and 34 in the consumer, all 24 cases pass, the 20,000-step sequence at stride 1 `calls 21186`, `refusal bursts 1275, machine digests verified 1285, refusals without a digest check 0`, `checks=98461 failures=0`, no sanitizer report, 81 s at load 8 to 12. TSan (library build id `t64-build-v1-d4ab69b5...`): two threads, one instance each, both at tick 599357 and the pinned digest, `checks=18 failures=0`, no report; `--seed-race` reports the race (4 reports, exit 134), so the lane can fail. The instrumented lane (about 5 min) was not re-run |
| (g) CI files unchanged | `git diff --name-only BASE..HEAD -- .github/workflows/ci.yml scripts/check-ci-yml.sh scripts/gate.sh` | empty |
| (h) authority still blocked | `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked`; `--require-approved`; `git diff --name-only BASE..HEAD` over the checker, its record and the ten bound test files | exit 0 `authority record valid; blocked: no oracle operation is authorized`; exit 2; the diff is empty (no bound file moved in the INSTALL wave) |
| (i) digest sweep | `python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD` (Wave 0 base) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines`, exit 0, 43 s |
| (j) PII over the range | `sh scripts/pii-scan.sh --range BASE..HEAD` | `pii-scan: tree_files=skipped commits=56 findings=0` |
| (k) COST on every phase11 name | `ctest --test-dir build/gate -L phase11 --show-only=json-v1` | 70 names (13 `unit.core.api.`, 11 `repo.`, 5 `install.`, 27 `installed.consumer.`, 10 `installed.controls.`, 4 `api04.`), every one with `COST` (sum 55.85) and `TIMEOUT` 30, 60, 120 or 180; no `install.`, `installed.`, `api04.` or `repo.consumer` name lacks the `phase11` label |
| (l) Linux records | PIN-IMPACT "Install steps": the act-measured Linux undefined set of 11-16 (`_GLOBAL_OFFSET_TABLE_`, `__stack_chk_fail`, `memcmp`, `memcpy`, `memset`, act image gcc 13.3.0) and the Q6 spaced-prefix `ar x` row of 11-17 (in place, BSD and GNU `ar`) | both present; `docs/abi/installed-undefined.txt` carries the same five Linux rows, labeled act-measured. (l) hosted: gate-linux of run 45766004142 (head `463bd09`, Ubuntu gcc 13.3.0) `api04.members ... Passed 7.41 sec`; its set comparison fails in both directions, so the pass confirms the five Linux rows by a hosted measurement (the earlier push run 45750585142 at `c6f28f6` also passed it, 6.88 s); the Linux install path passed too (`install.setup` and the four `install.*` checks). The same gate-linux job failed two other names (Task 3, I-11) |

Task 2 verify (in the clone): `ctest --test-dir build/gate -R '^(installed\.|install\.|api04\.)' --output-on-failure`
`100% tests passed out of 46` (20.54 s); `consumer-includes: files=17 findings=0`; `check-consumer-literals: findings=0`.

#### 11-27 verified (the defect-fix plan executed before this review)

`t64_machine_advance` (`core/t64_machine.c`): the entry guard, then NULL machine, request and result, then the
span of the request and of the result at their v1 sizes against the machine storage, the admitted image and both
packet storages (`t64_internal_observation_range_is_disjoint`), all before either `struct_size` is read, before
`STRUCT_TOO_SMALL` and before `NOT_READY`; the request is copied at most `min(caller, known)` bytes and the result
copied out at most the known size, so the judged spans cover every byte read or written. Rows: the in-tree spans
inside `unit.core.api.not-ready-table` (`test_api_lifecycle.c` `advance_span*`, five refused calls per live block and
the outside control), `ops-advance` `advance_aliased` and the generator's 15th advance variant (`{"advance", 14u,
op_advance, 15u, ...}`, `default: seq_advance_aliased`; the 20,000-step lane drew `advance 15/15`). COSTs
`T64_CONSUMER_COST_ops-advance 0.65` and `T64_CONSUMER_COST_sequence 1.20`. The header sentence "The value_out span
of t64_machine_bus_read is the caller's responsibility" is present, and `core/t64_rsp.c` calls
`t64_machine_bus_read` at lines 2198, 2213 and 2231, as the 11-27 reason states.

#### Decision-by-decision read

| Decision (CONTEXT) | Where implemented | Test names | Verdict |
|--------------------|-------------------|------------|---------|
| Installability: sha256 folded, relocatable config and version file (SameMinorVersion, 0.1.0), `tendies64::core` alias, single configuration and build id recorded, `project(VERSION)` untouched | `core/CMakeLists.txt` (folded `tools/common/t64_sha256.c` with the four `t64_sha256_*=t64i_sha256_*` definitions, `BUILD_INTERFACE`/`INSTALL_INTERFACE`, `FILE_SET HEADERS` of the one header, `install(EXPORT)`, `write_basic_package_version_file(VERSION 0.1.0 COMPATIBILITY SameMinorVersion)`), `core/tendies64Config.cmake.in` (`tendies64_BUILD_ID`, `tendies64_BUILD_TYPE`) | `install.setup` (six files), `installed.consumer.build`, `.smoke`, `repo.abi-prefix`, `install.abs-paths` | implemented |
| Independent consumer, three-way, runner not re-pointed, include guard | `tests/installed-consumer/` (separate top-level project), `scripts/check-consumer-includes.sh`, `scripts/check-consumer-literals.py`, `scripts/check-install.sh three-way` | `repo.consumer-include-guard`(`-selftest`), `installed.consumer.literals`, `.composite`, `.three-way`, the 17 `ops-*` cases | implemented; checks (a) to (c) |
| Negative controls (all must fail loudly) | `tests/installed-consumer/controls.tsv` (10 rows), `cmd_control` | `installed.controls.*` (10) | implemented; every row observed failing for its own reason (check (d)); the ABI row doctors a different minor than it states (I-01) |
| Host-shaped loop, no SDL; digest independent of drain cadence and host delay; two instances | `tests/installed-consumer/hostloop.c` | `installed.consumer.hostloop`, `.hostloop-cadence`, `.hostloop-delay`, `.two-instance` | implemented (in the gate run and the sanitizer lane) |
| Refusal-is-a-no-op harness: generator, guest-stop resume, defect poisons, ASan and UBSan against the shipped library | `tests/installed-consumer/sequence.c`, `scripts/check-install.sh sanitize` | `installed.consumer.sequence` (400 steps, stride 2), local lane (20,000 steps, stride 1) | implemented; the gate samples the machine digest (I-06) |
| API-04 on the shipped objects (members, undefined set both directions, disassembly, installed headers, seeds) | `scripts/check-api04.sh`, `tools/lint/lint-objects.sh --allow`, `docs/abi/installed-undefined.txt`, `tools/lint/installed-seeds/` | `api04.members`, `.disasm`, `.headers`, `.seeded` | implemented on Darwin; the Linux rows wait for the hosted run (check (l)) |
| Stack protector: shipped build hardened, exactly three allowlist rows with platform and reason, ADR-0006 amendment, lint compile `-fno-stack-protector` | `tools/lint/allowlist-installed.txt` (3 rows), `docs/adr/0006-determinism-lint-mechanism.md` amendment, `tools/lint/allowlist.txt` byte-identical to the Wave 0 base | `api04.members` (and its control naming exactly the pair) | implemented |
| Hosted footprint: cheap-first plus install-from-build-tree on both hosted OS inside the existing gate step, no `ci.yml` or timeout change; sanitizer, TSan and fresh-archive runs local | CTest names in `tools/abi/CMakeLists.txt`; lanes in `scripts/check-install.sh` | check (g); `ctest -N -R '^installed\.consumer\.threads'` 0 | implemented; hosted half in Task 3 |
| Package smoke: plain `cc` line; `.pc` dropped (DR-29) | `ctl_plain_cc` | `installed.controls.plain-cc` | implemented |
| Thread statement: distinct instances independent, one instance needs external serialization, two-thread TSan run in a local lane | header text before `t64_machine_is_poisoned`; `tests/installed-consumer/threads.c`; `check-install.sh tsan` | local lane only (re-run here, check (f)) | implemented |

#### Findings

Severity scale as for SHAPE: blocking (stops the plan before the push), warning (a real gap that does not block the
push), info (a recorded observation or accepted limit). The reviewer fixes nothing.

| id | severity | file:line | finding | disposition |
|----|----------|-----------|---------|-------------|
| I-01 | warning | `scripts/check-install.sh:469-472`, `tests/installed-consumer/controls.tsv` row `doctored-header-abi`, PIN-IMPACT "Install steps" 11-19 | The ABI doctoring takes the field `UINT32_C(1)` and deletes every non-digit, which keeps the `32` of `UINT32`: the copy says `T64_ABI_VERSION_MINOR UINT32_C(322)`, not 2. The control still fails for its own reason (create answers 34 ABI_MISMATCH, machine NULL; a minor other than the library's is a mismatch while the major is 0), and minor plus one and minus one are asserted through the create-info by `unit.core.api.handshake` and `installed.consumer.ops-handshake`, so no ABI coverage is missing; the row's detail and the 11-19 record ("one higher") describe a different edit | new numbered plan (strip `UINT32_C(` and `)` before the arithmetic, correct the record); not blocking |
| I-02 | warning | `docs/spec/machine-contract.md:23-31`; `core/t64_machine.c` `openipl3_authored_flat_rom_matches` (`z64_magic`), `T64_OPENIPL3_FLAT_ENTRY_POLICY` | Review item 1 (observation of 11-18): the "Raw ROM admission" section lists the byte-swapped and word-reversed signatures as accepted without saying that a pinned openipl3 profile (every profile of the installed-consumer suite) narrows admission to the exact pinned 4096-byte asset or a big-endian z64 cartridge. The code matches `docs/spec/boot-limits.md` ("an authored big-endian z64 cartridge") and the profile's own `rom_order_policy` text `exact-source-or-flat-payload-z64-v2` (part of the hashed descriptor); the generic order detection (`core/t64_rom.c` `detect_order`) serves the skeleton profile (`all-known-orders-to-big-endian-v1`). So it is a documentation gap, not a code defect, and `UNSUPPORTED_ROM_ORDER` stays never-returned. The public header says nothing about orders and none of the four goldens 11-23 binds carries this text | documentation correction in a new numbered plan (a sentence in machine-contract.md naming the profile narrowing and pointing to boot-limits.md); not required for the 11-23 goldens, recommended before 11-23 so its `docs/spec/api.md` cites a consistent contract |
| I-03 | warning | `core/include/tendies64/t64_machine.h:201,204`; `core/t64_scheduler.c:41-45`; `docs/spec/machine-contract.md:279-283` | Observation (b) of 11-18 confirmed: the frame-event counter is advanced only by the synthetic scheduler test handler (`test_frame_handler`); no public entry schedules such an event, so through the public API a `T64_STOP_FRAME_EVENT_COUNT` target above the counter never fires (only an equal target, which returns at once). The header defines the kind and the stop reason with no meaning, and a host would read it as video frames. The spec sentence "These counters are driven only by explicitly labeled synthetic test events" is right for the frame-event counter and stale for the instruction counter, which CPU retirement drives (`core/t64_cpu.c:2579`) | header comment (and the spec line) in the same new numbered plan, placed before 11-23 binds the header hash (a later header edit is a candidate amendment); not blocking |
| I-04 | info | `core/include/tendies64/t64_machine.h:529-531`; `core/t64_machine.c` `t64_profile_lookup` | Observation (e) of 11-18: on a refused lookup (NULL or unknown id) the call writes NULL to `*profile_out`; the header says only that a returned descriptor is immutable and valid for program life. The entry takes no machine, so no refusal-is-a-no-op rule is broken | optional header sentence in the same plan; otherwise accepted |
| I-05 | info | `core/include/tendies64/t64_machine.h:968-973`; `core/t64_bus.c:1004`; `core/t64_rsp.c:2198-2231` | Review item 2: the bus-read `value_out` is not span-checked (11-27 decision). Acceptable for the candidate: the header states the exception explicitly (so the bound text is honest), `value_out` is a scalar pointer outside the struct-span rule, a caller that aliases it is writing storage it already owns through its allocator, and the uniform rule needs an edit of the sealed `t64_rsp_issue_event` body plus a digests-only no-HLE refresh. Adding the check later narrows accepted inputs (a header edit, so a candidate amendment after 11-23 or a Phase 12 final-bind item) | accepted limit for the candidate; no plan before 11-23 |
| I-06 | info | `tests/installed-consumer/sequence.c:69,447` (`SEQ_DEFAULT_STRIDE 2u`) | Review item 4: in the gate the machine digest is compared over every second refusal burst (400 steps: 37 bursts, 18 verified, 109 of 282 refusals without a digest comparison), while the cheap snapshot (content digest, output status, packet, save and stop records, ROM size and header, poison query, allocator counters) is compared after every refused call. The same 400-step stream at stride 1 passes (37 of 37 verified, `checks=5488 failures=0`) and costs 2.0 s against 1.1 s native; the local shipped lane verifies every burst over 20,000 steps (1275 bursts, 0 refusals unverified). The plan property "bit-equality after any refused-class status" therefore holds in full in the local lane and in sampled form in the gate; the gate's sampling buys 0.9 s native (about 3 s hosted) under a tight 900 s margin | accepted (documented sampling); re-judge at 11-25 with the cold measurement |
| I-07 | info | `scripts/tests/test_rsp_run.py:110-124` (`harness.rsp-run`) | Review item 3: push #2 run 45750585142 attempt 1, gate-macos failed `test_closed_pipes_do_not_end_the_outer_deadline` at `assertIsNotNone(command["rejected_batch_path"])`: with `outer_seconds=0.1` the outer deadline can expire before the Python guest has started and written its batch file, so no rejected batch exists; the failed-job rerun (attempt 2) passed. Not an INSTALL-wave change (the test predates Phase 11). Recorded as finding F-129 in `docs/ci-reviews/LOG.md` (Task 3) | known flake; AUTONOMY forbids hiding a scheduling race with repeated CI attempts, so a new numbered low-priority plan naming `scripts/tests/test_rsp_run.py` makes the deadline expire only after the guest has published its batch (a readiness barrier, not a longer sleep); until it lands, one rerun of the failed job with F-129 cited |
| I-08 | info | PIN-IMPACT "Install steps" 11-20 lane rows | The sanitizer and TSan rows carry build ids but no date in the row (dated by commit `949dcbe`, 2026-11-26); the instrumented and TSan rows are of the pre-11-27 library. This review re-ran shipped and TSan at the reviewed head (check (f)) | accepted; 11-25 records lane dates and build ids of the qualified head |
| I-09 | info | `tests/installed-consumer/sequence.c` op table | The 20,000-step stream draws `state 12/13` variants (one of the two state-import variants never fires in that stream); `installed.consumer.ops-state` and the in-tree state tests cover both import forms | accepted; a later generator edit may raise the import weight |
| I-10 | info | PIN-IMPACT "Gate-growth ledger" | Review item 5: the cold gate-macos measurement is deferred to 11-25 (orchestrator-only, with the final test set); Task 3 records warm numbers and projections only | accepted (orchestrator decision) |

Result before the push: zero blocking findings; three warnings (I-01 to I-03), all text or test-driver fixes for one
new numbered plan whose placement (before 11-23 for I-03, which edits the header) is the orchestrator's decision,
and seven info rows. The installed-consumer suite is independently confirmed control by control, and the wave moved
no digest (check (i)).

### Task 3: push and hosted pair (RED: the plan stops before the ledger row and the Freeze base)

Before the push, in the shared tree with a clean committed tree at `463bd09d472c1f4fc097a6941118f6f903230869` (the
reviewed head plus the two 11-REVIEW.md commits of Tasks 1 and 2): `sh scripts/gate.sh` `[gate] PASS`, 324 of 324,
ctest 94 s, total 157 s (load 8 to 14); `sh scripts/preflight-phase11.sh` (full) `[preflight] PASS` in 65 s
(ctest-counts 21 rows and 70 `phase11` names equal to the block, abi-cheap 14 names, guards, host-load WARN at 13.5,
head-ci `origin/main` `c6f28f6` completed success, timeout-audit 177 TIMEOUTs with lowest ratio 2.9, pii,
linux-compile 233 units with the act image cc 13.3.0, post-gate-replay 11 `ci.yml` steps); `sh scripts/pii-scan.sh
--tree` (1421 files) and `--range origin/main..HEAD` (25 commits) 0 findings; `sh scripts/rights-scan.sh --tree --range
origin/main..HEAD` `tree=1421 history=83 findings=0`.

Push: `git push origin 463bd09d472c1f4fc097a6941118f6f903230869:refs/heads/main`, a fast-forward from `c6f28f6` (no
force, no tag, no other branch). Hosted run 45766004142 (`ci.yml`, event push, Binutils cache hit on both jobs, so a
warm sample): conclusion failure.

- gate-macos success, job 487 s (Gate 429 s, ctest 291 s, `100% tests passed out of 324`, test-inventory-selftest
  59 s; cmake 4.5.0). The 49 INSTALL-wave names sum 76.58 s and the 21 SHAPE names 23.94 s there.
- gate-linux failure, job 194 s, ctest 130 s, `99% tests passed, 2 tests failed out of 324`:
  `installed.controls.version-mismatch-minor` and `installed.controls.version-mismatch-patch` (finding I-11). Every
  other name passes, among them all `install.`, `api04.` and the other 35 `installed.` names (the INSTALL-wave names sum
  38.93 s there), so the Linux install path and the Linux undefined rows are confirmed (check (l) above).
- `docs/ci-reviews/LOG.md`: Runs rows for 45750585142 (the orchestrator's push after 11-18, which had none) and
  45766004142, finding F-129 (the `harness.rsp-run` flake, I-07) and finding F-130 (this run: the failure, the hosted
  seconds and the cold projections).

| id | severity | file:line | finding | disposition |
|----|----------|-----------|---------|-------------|
| I-11 | blocking | `tests/installed-consumer/controls.tsv` rows `version-mismatch-minor` and `version-mismatch-patch` (expected text column); `scripts/check-install.sh` `ctl_judge` (`grep -qF`) | The expected text "The version found is not compatible with the version requested" is a reason line that CMake 4 prints under "considered but not accepted" (local 4.4.3, hosted macOS 4.5.0). The hosted Linux runner has cmake 3.31.6, which prints "Could not find a configuration file for package "tendies64" that is compatible with requested version "0.2"" (and "0.1.5") and lists the moved-prefix config as "considered but not accepted ... version: 0.1.0" without that line, so the judge reports "the expected text did not occur" although the package was found and refused for the right reason. Reproduced locally without a rerun: the judge's `grep -F` answers not found on the hosted 3.31.6 output (copied from the job log) and found on the local 4.4.3 control log; both outputs contain "considered but not accepted" and "version: 0.1.0". Deterministic (CMake version), not a flake, so the failed job was not rerun. The 11-19 record's "observed with the CMake of the build tree" names the dependency but no Linux run existed before this push (act has no cmake) | blocking: the plan stops here, before the Gate-growth ledger row and the Freeze base; `origin/main` `463bd09` is red on gate-linux. New numbered plan: expected text that both CMake versions print (for example "compatible with requested version", or both "considered but not accepted" and "version: 0.1.0" for the found-and-refused case), with the I-01 doctoring fix in the same driver, then a push with a green hosted pair; the Freeze base and the INSTALL ledger row follow that pair |

Cold projections from this warm sample (the cold measurement is the orchestrator's at 11-25; no cold figure is
claimed): warm plus the 186 s Binutils build 673 s (margin 227 s; the plan's G4 rule is not triggered); F-126
projection 733 s plus the summed hosted seconds of the 70 Phase 11 names (100.5 s) and the inventory selftest growth
(11 s) 845 s (margin 55 s); 733 s plus the whole warm delta since F-126 (119 s) 852 s (margin 48 s); this warm run plus
the F-123 to F-124 cold delta of 346 s 833 s (margin 67 s). WARNING: three of four projections are under the 120 s
warning margin; none is above 900 s. These numbers go into the INSTALL ledger row once a green pair exists.

Result after the push: one blocking finding (I-11). No Freeze base is recorded and the INSTALL ledger row is not
written; the plan returns to the orchestrator.

### Resolutions of the INSTALL findings (recorded when plan 11-21 resumed)

The tables above are the record of the review as it stood at the push and are not edited. Plans 11-28 and 11-29
were numbered defect-fix plans run after it; the reviewer did not fix anything (the review rule). Their
dispositions, in finding order:

| id | final disposition | resolved by | evidence |
|----|-------------------|-------------|----------|
| I-01 | resolved | plan 11-28, fix commit `9e5acee` | the doctoring awk strips `UINT32_C(` and `)` before the arithmetic and `ctl_doctored` asserts original plus one before the build; the control log reads `doctored ABI minor: 1 -> 2` and still ends in status 34; green on both hosted jobs in run 45770631197 |
| I-02 | resolved | plan 11-29, commit `21ca1d9` | `docs/spec/machine-contract.md` "Raw ROM admission" says a pinned Open IPL3 profile narrows admission to the exact pinned 4096-byte asset or an authored big-endian z64 cartridge and cites `docs/spec/boot-limits.md`; code unchanged; recorded in PIN-IMPACT "11-29" |
| I-03 | resolved | plan 11-29, commit `21ca1d9` | the header comment before the stop kinds and the spec say the frame-event counter is advanced by no public entry (a larger target never reports that reason, an equal target returns at once, the counter is not a video-frame count) and that CPU retirement drives the instruction counter; the header diff is comment lines only and the digest sweep reports 0 differences (11-29 SUMMARY) |
| I-04 | resolved | plan 11-29, commit `21ca1d9` | the `t64_profile_lookup` header comment says the call takes no machine, that a NULL `profile_out` writes nothing and that any other refusal writes NULL to `*profile_out` |
| I-05 | accepted | review decision | the bus-read `value_out` span stays the caller's responsibility, stated in the header; a later check is a candidate amendment or a Phase 12 final-bind item |
| I-06 | accepted | review decision | documented sampling of the machine digest in the gate lane (every second burst; the local shipped lane verifies every burst); re-judged at 11-25 with the final measurement |
| I-07 | resolved | plan 11-28, fix commit `0e6b96d` | `test_closed_pipes_do_not_end_the_outer_deadline` measures its outer deadline from guest readiness with a readiness clock (not a longer sleep); 20 of 20 runs at natural load and 20 of 20 under a 0.3 s interpreter start-up delay; `harness.rsp-run` passes on both hosted jobs in run 45770631197 (F-132 closes F-129) |
| I-08 | accepted | review decision | lane dates are the commit dates; 11-25 records lane dates and build ids of the qualified head |
| I-09 | accepted | review decision | the 20,000-step stream rarely draws the second state-import variant; `installed.consumer.ops-state` and the in-tree state tests cover both forms |
| I-10 | accepted | orchestrator decision | the review accepted a deferred cold measurement; the orchestrator took one anyway after the 11-28 push (run 45772586364, gate-macos 731 s, margin 169 s, F-133); 11-25 repeats it with the final test set |
| I-11 | resolved | plan 11-28, fix commit `9e5acee` | the two version-mismatch rows expect `compatible with requested version "0.2"` and `"0.1.5"`, which cmake 3.31.6 and 4 both print on one line and the missing-package log does not contain; `installed.controls.version-mismatch-minor` and `-patch` pass on gate-linux (cmake 3.31.6) and gate-macos (cmake 4.5.0) in run 45770631197; F-131 closes F-130 |

Open after these resolutions: no blocking finding and no open warning. The reviewer's remaining notes are the accepted
info rows I-05, I-06, I-08, I-09 and I-10.

### Task 3 completed: second push, green hosted pair, check (l) hosted

Plan 11-21 resumed after the 11-28 repair (origin/main `0e6b96d`, hosted run 45770631197 green) and the 11-29 text fixes
(local). Head of this push: `4b13a9032dabfe1e00a8b86808cb7501504b100d`, the 11-28 head plus nine docs-only commits
(the 11-28 and 11-29 records, the comment-only header edit and the machine-contract.md text of 11-29, F-133 and the
resolutions above). Before the push, in the shared tree with a clean committed tree at that head:

- `sh scripts/preflight-phase11.sh` (full, with replay): `[preflight] PASS` in 88 s (oracle-authority, ctest-counts 21
  rows 0 mismatches, abi-cheap 14 names, guards, head-ci ok on `0e6b96d`, timeout-audit 177 TIMEOUTs 0 too low with
  lowest ratio 2.9, pii, linux-compile 233 units with the act image cc 13.3.0, post-gate-replay 11 `ci.yml` steps).
- `sh scripts/gate.sh`: `[gate] PASS`, 324 of 324, ctest 86 s, total 145 s (load 6 to 12; no limit widened).
- `sh scripts/pii-scan.sh --tree` (1425 files) and `--range origin/main..HEAD` (9 commits): 0 findings;
  `sh scripts/rights-scan.sh --tree --range origin/main..HEAD`: `tree=1425 history=15 findings=0`.

Push: `git push origin 4b13a9032dabfe1e00a8b86808cb7501504b100d:refs/heads/main`, a fast-forward from `0e6b96d` (no
force, no tag, no other branch). Hosted run 45775107983 (`ci.yml`, event push; Binutils cache hit on both jobs, so a
warm sample; no rerun): conclusion success.

- gate-macos success, job 592 s (Gate 525 s, ctest 317 s, `100% tests passed out of 324`, test-inventory-selftest
  74 s; cmake 4.5.0). gate-linux success, job 240 s (Gate 197 s, ctest 128 s, `100% tests passed, 0 tests failed out
  of 324`; cmake 3.31.6, gcc 13.3.0).
- (l) hosted, result: gate-linux of run 45775107983 reports `api04.members` Passed 7.84 s (the earlier runs 45750585142
  and 45766004142 also passed it, 6.88 s and 7.41 s). The check compares the archive-level undefined set with
  `docs/abi/installed-undefined.txt` in both directions (an `unrecorded import` or a `recorded import gone` fails), so a
  pass on the hosted Ubuntu runner confirms the five Linux rows (`_GLOBAL_OFFSET_TABLE_`, `__stack_chk_fail`, `memcmp`,
  `memcpy`, `memset`) by a hosted measurement. No mismatch, so no finding for a new numbered plan. The Linux install
  path passed: `install.setup` 0.04 s, the three `install.abi-*` names and `install.abs-paths`, `api04.disasm`,
  `.headers` and `.seeded`, every `installed.consumer` name and the 10 controls, among them the two version rows that failed in
  F-130 (0.19 s and 0.18 s) and `installed.controls.doctored-header-abi` (0.33 s). Check (l) no longer reads pending.
- `docs/ci-reviews/LOG.md`: Runs rows for 45772586364 (the cold dispatch) and 45775107983, finding F-133 (the measured
  cold gate-macos run, 731 s, margin 169 s, supersedes every cold projection) and finding F-134 (this pair: seconds,
  per-wave hosted sums, the 105 s warm spread, the sensitivity note). PIN-IMPACT "Gate-growth ledger" has the INSTALL
  row; the Freeze base is recorded in PIN-IMPACT "Wave bases" by its own commit after the records.

Result of the review: zero blocking findings and no open warning; the INSTALL wave is reviewed in a scratch clone,
repaired where the review found defects (11-28, 11-29), pushed, and green on both hosted operating systems.
