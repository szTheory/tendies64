# Lens: Phase 11 "Installed Consumer and Frozen API" (VER-05, API-01..04)

Read-only research. Repo facts below were observed in the working tree at commit 91c1940 unless marked "(knowledge)", which means recalled and not re-verified against a primary source this session.

## 0. Recommendations (ordered)

1. Split the freeze. Phase 11 ends with a **freeze candidate**: the API shape is final in intent, an ABI identity record is bound and enforced by checks, and every postcondition passes through the installed consumer. The **final freeze bind** moves to the end of Phase 12, or to a single closing plan there. This needs a one-line edit to API-03 and to Phase 11 criterion 5 / Phase 12 criterion 3. It changes no work, only when the lock is called final.
2. Add a "host-shaped loop" test to the Phase 11 installed consumer. It uses no SDL. It runs frame-paced bounded advances, drains the video and audio rings in the order a real host would, forces backpressure stops, and runs two instances interleaved in one process. This pulls most of what SDL3 would reveal into Phase 11 for almost no cost.
3. Land the ABI shape changes first inside Phase 11, before writing the consumer. The shape changes are `struct_size` on every in/out struct, an ABI-version handshake, a status-class table, a poison flag, and public reset/unload. Several of these are not additive later (see section 1). Evidence keyed to the runner and core build identities has to be re-derived once after this change, so do it once and early.
4. Make the installed package real before testing it. Today there are no `install()` rules at all. `t64_core` privately links the separate `t64_sha256` static library, which makes `install(EXPORT)` fail (reproduced below). The archive exports 248 global symbols against about 30 public functions, 48 of them unprefixed softfloat names. Fold sha256 into core, prefix or hide the internal symbols, and gate the public symbol list against a golden file.
5. Enforce API-04 on the shipped objects, not only the lint's own compile. Reuse `tools/lint/lint-objects.sh --objects` on members extracted from the installed archive. The archive-level undefined set today includes `___stack_chk_fail`/`___stack_chk_guard`, which the lint allowlist does not carry (lint compiles with `-fno-stack-protector`; the shipped build does not). Decide this explicitly in an ADR-0006 amendment.
6. Replace hand-written refusal tests with a "refusal is a no-op" harness. It is a deterministic call-sequence generator that snapshots machine observations before every call and asserts equality after any refused-class status. It enforces API-02 mechanically across the whole surface. The repo already has the snapshot concept (machine-contract, refusal assertions).
7. Do not re-point `t64-runner-real` at the installed package. It already uses only the public header. Add the installed consumer as a second independent client that must reproduce the same RUN-07 bytes and digest (differential evidence).

## 1. Freeze timing (question 1)

**What the repo says.** The roadmap freezes in Phase 11 and builds the SDL3 host on the frozen API in Phase 12. The in-tree header is full of "provisional until Phase 10" text. There are 12 such occurrences in `t64_machine.h`, 1 in `t64_facade.h`, 5 in `machine-contract.md` and 2 in ADR-0002, all now stale because the freeze moved to Phase 11. This is a doc-sweep item. ADR-0002 also says the language-reversal window "closes once the C API is frozen and consumed in v1.2", so a premature freeze quietly narrows a recorded reversal option.

**Why early freezing is risky here specifically.** Phase 12 is the first consumer that has a real-time clock, an audio device pulling at a fixed rate, a second thread, and a physical controller. Those are exactly the things that expose the following:
- Ring and backpressure semantics: drain cadence, what the host does when audio underruns, packet granularity.
- Pacing: how a host maps wall time to `bound_tick`.
- Input timing: timed input at a tick versus "latest value".
- Threading: instance affinity and external synchronisation.

None of these can be fixed additively after a lock. Struct layouts without a size field and changed ring semantics are non-additive. Today `t64_stop_request`, `t64_stop_result`, `t64_video_packet_info` and `t64_output_drain_result` have no size field. Only `t64_audio_packet_info` has `struct_size`. The public header also has 26 `size_t` and 6 `bool` uses (ABI-width and FFI footguns) and no `extern "C"` guard.

**Prior art, with sources.**
- SDL3 locked its ABI at the 3.1.3 "stable ABI preview" (Oct 2024) and shipped 3.2.0 stable afterwards. Preview releases were "not guaranteed to be backwards-compatible with each other" until that lock, and 3.1.6 and 3.1.8 notes say they "maintain ABI compatibility with 3.1.3". The lock was a deliberate pre-stable step, after SDL2 and years of consumers. Sources: https://discourse.libsdl.org/t/announcing-sdl-3-1-3-stable-abi-preview/54743 and https://wiki.libsdl.org/SDL3/README-versions. This is the closest precedent for "candidate first, final later", with the lock decided by the author after real use.
- libretro defines `RETRO_API_VERSION` as 1, "only incremented for incompatible API/ABI changes", with the stated goal "to avoid having to increase this value at all costs". Growth is by new environment calls and "extended" variants, and retired numbers stay reserved. `retro_api_version()` exists "to validate ABI compatibility". Source: https://github.com/libretro/RetroArch/blob/master/libretro-common/include/libretro.h. libretro stays additive because most of its surface is an extensible callback/environment channel. Tendies64 has a flat struct-and-function surface, so it cannot extend that way unless struct_size and flags are designed in first.
- Vulkan loader/driver interface uses explicit version negotiation (`vk_icdNegotiateLoaderICDInterfaceVersion`), with `VK_ERROR_INCOMPATIBLE_DRIVER` on mismatch and a documented rule for older drivers that lack the function. Source: https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderDriverInterface.md. The lesson is that a negotiation step in the first call is what lets later growth be safe.
- SQLite documents that applications should assert header and library agree at run time: `sqlite3_libversion_number()` against `SQLITE_VERSION_NUMBER`, and `sqlite3_sourceid()` against `SQLITE_SOURCE_ID`. Source: https://www.sqlite.org/c3ref/libversion.html. This is a cheap, directly copyable check.
- mupen64plus (the N64 emulator core API) compares only the upper 16 bits of the API version word at `CoreStartup` and rejects a mismatched frontend. Plugins carry their own API version constants. Source: search result from https://github.com/mupen64plus/mupen64plus-core (`src/api/frontend.c`); I could not fetch the wiki versioning page (404), so treat the exact mask as second-hand.
- ares, higan, Dolphin (knowledge, not re-verified): none publishes a stable embeddable C ABI. They are consumed through libretro cores or as applications. This is weak negative evidence that the real-world path for an embeddable N64 core is a libretro-like contract. LibFuturo (API-07) is the place that question lives.
- libdragon is an N64 homebrew SDK, not an emulator core API. It is relevant to Phase 12 route B toolchain, not to this freeze question.

**Concrete recommendation.**
- Phase 11 exit: a versioned `0.x` ABI (`T64_ABI_VERSION_MAJOR 0`, minor increments on any change) with documented postconditions in a new `docs/spec/api.md`. Bind a freeze-candidate record containing the SHA-256 of the canonicalised installed headers, the sorted public symbol list, a struct-layout table, and the status-class table. Any change fails a check until the record is re-bound.
- Phase 12 exit (or a last Phase 12 plan): a final bind to major 1. Allowed edits between the two binds are limited to API defects found by the Phase 12 host or the installed consumer. Each carries evidence and a re-run of the Phase 11 installed-consumer suite, and the amendment is recorded.
- Why not just freeze early and rely on "additive only"? Because the things Phase 12 is likely to reveal are the non-additive ones above. Also, nobody outside the repo depends on the API yet (D6: nothing is released), so a locked ABI in Phase 11 protects no external user and costs a re-bind later.
- Counter-argument, stated fairly: freezing early forces discipline and prevents Phase 12 from reshaping the API to suit one host (API-07 says an adapter must not shape the core). Mitigation: the amendment rule above plus the "no host or adapter requirement may change the core API unless it is a defect of the postconditions" line.

## 2. Installed package in CMake (question 2)

**Observed state.** No `install()` rules exist (grep over all CMakeLists). `t64_core` is a STATIC library with `PRIVATE` link to `t64_sha256` (tools/common). Build identity (`T64_BUILD_ID`, source revision) is computed at configure time from hashed inputs and compile definitions.

**Verified defect (reproduced in the scratchpad, not the repo).** A STATIC library with `target_link_libraries(core PRIVATE dep)` and `install(EXPORT)` fails at generate time: `install(EXPORT "xTargets" ...) includes target "core" which requires target "dep" that is not in any export set.` So the naive install fails, and the copy-paste philosophy answers it: compile the sha256 source directly into `t64_core` (or add it to the export set and ship a second archive). Merging also matches ADR-0006's include rule that core headers resolve inside `core/`.

**Verified by `nm` on `build/gate/core/libt64_core.a` (macOS arm64, built Nov 20).**
- Net undefined symbols (undefined minus defined in the archive): `___stack_chk_fail`, `___stack_chk_guard`, `_bzero`, `_memcmp`, `_memcpy`, `_memset`, `_t64_sha256_init/update/final`. Only the mem* family and bzero are in `tools/lint/allowlist.txt`.
- 248 defined global symbols, of which 200 are `t64_*` internal cross-TU helpers and 48 are unprefixed softfloat symbols (`f32_add`, `softfloat_addMagsF32`, ...). Only about 30 are public API. A consumer that links its own Berkeley SoftFloat gets duplicate-symbol failures.

**Package shape (recommended).**
- `install(TARGETS t64_core EXPORT tendies64Targets FILE_SET HEADERS ...)` and `install(EXPORT ... NAMESPACE tendies64:: ...)`, plus `add_library(tendies64::core ALIAS t64_core)` in-tree so in-tree and installed consumers write the same `target_link_libraries`.
- `tendies64Config.cmake` from `configure_package_config_file` with `@PACKAGE_INIT@` and relative paths, and `tendies64ConfigVersion.cmake` from `write_basic_package_version_file`. For the 0.x candidate use `COMPATIBILITY SameMinorVersion`; switch to `SameMajorVersion` at the final bind. Do **not** use `ARCH_INDEPENDENT`: the artifact is a static, architecture-specific library. Guidance: https://cmake.org/cmake/help/latest/manual/cmake-packages.7.html ("a relocatable package must not reference absolute paths of files on the machine where the package is built"; use `$<INSTALL_PREFIX>` or relative INSTALL_INTERFACE paths) and https://cmake.org/cmake/help/latest/command/install.html (`--prefix` needs relative destinations).
- Keep the CMake package version separate from the ABI version. D6 forbids release versions and tags; a package version inside the config file is not a tag, but make it the ABI candidate number, not `project(VERSION 0.0.0)`.
- Record `T64_BUILD_ID`, build type and compiler in the config as variables. The consumer can compare that to `t64_build_identity()` at run time (SQLite pattern). The build identity includes `CMAKE_BUILD_TYPE` and the core CMake requires a single-config generator, so the config must state the single configuration it was built for.
- Visibility: for a static archive, `-fvisibility=hidden` alone does not stop link-time collisions, because hidden symbols are still resolved across objects in a final link. Use prefixing for all non-API symbols (rename the softfloat ones with a one-line include of a rename header, a small copy-paste patch with provenance) plus a `T64_API` export macro, and assert the public list from `nm`.
- Skip abi-compliance-checker and libabigail (`abidiff` is not installed here, and it is DWARF/ELF-centric and a new dependency). For a ~30-function C API, three goldens give equal or stronger coverage: (a) public symbol list, (b) `_Static_assert` layout file with `sizeof`/`offsetof` and every public constant value, compiled against the **installed** headers, (c) header hash. All are POSIX tools plus the C compiler, which ADR-0002 already requires.

**Cheap checks that run before the long suite (in order).**
1. Each installed header compiles alone with `-std=c99 -pedantic -Werror` and as C++17 (needs `extern "C"` guards, which are missing today).
2. Layout and constant `_Static_assert` file against the source header, then against the installed header.
3. `cmake --install <existing gate build tree> --prefix <tmp>` for the Development component only. No recompile, so hosted macOS (10-minute limit; GATE-01 exists because it is tight) pays near nothing.
4. `nm` public-symbol golden; extract members with `ar x` and run `lint-objects.sh --objects`.
5. Absolute-path scan: `grep -r` the install tree and `strings` of the archive for the source and build directories.
6. Relocate the prefix (`mv`), configure the separate consumer project with only `CMAKE_PREFIX_PATH`, build, run the tiny authored fixture, compare outputs.
7. Negative controls (below).
8. Then the long CTest.
Run a fuller variant (fresh `git archive` checkout, install, build consumer) in a nightly or manual lane only.

**Negative controls for VER-05 (listed so the plan can pick).**
- Missing package: `find_package` fails with a clear error.
- Mismatched version: config requests a different minor and is rejected.
- Visible source tree: assert the consumer's `compile_commands.json` and link line contain no path under the source tree.
- Relocation: install, move prefix, rebuild consumer; a variant with spaces in the path.
- Header/library mismatch: consumer built against a doctored header copy with a wrong `T64_ABI_VERSION` or wrong `struct_size` refuses at `create` with an ABI status.
- Symbol collision: consumer defines its own `f32_add` and `t64_sha256_init`; link must succeed and results must be unchanged.
- Compiler-flag independence: consumer built with different warnings and `-O0`/`-O2` than core.
- Seeded leak: an object referencing `clock_gettime`, `fopen`, `getentropy`, `pthread_create` added to an archive copy must fail the installed-library check.

An optional second consumer without CMake (a plain `cc` line plus a 10-line `.pc` file) proves the package is not CMake-coupled. It is cheap but optional; not needed for the criterion as written.

## 3. API design: handles, versioning, rings, errors, poison (question 3)

**Already good (keep, freeze as is).** Opaque `t64_machine*`; host-supplied allocator callbacks with no core file/clock/entropy use; status is `uint32_t` with numeric constants, not an enum (ABI-width safe); size-then-copy-out convention with `BUFFER_TOO_SMALL`; documented exact check orders for refusals (save import/export R0 to R9); "refusals leave the machine unchanged" language; stop reasons are in the result struct and not in the status (so guest-stop is already structurally separate from error); two-packet lossless video and audio queues with peek-then-dequeue copy; 256-record trace ring with drop counters.

**Gaps against API-01/02 (observed).**
- No public reset or unload. The reset logic is internal (`t64_machine_reset_execution_state`, static `reset_guest_state`) and runs only on ROM replacement via `t64_machine_admit_rom`. API-01 lists reset and unload, so these are new surface, and a new surface needs its own refused/accepted tests.
- No poison state, no defect class. `T64_STATUS_BAD_STORAGE` appears 9 times in core; nothing makes it sticky. The status list is flat (0 to 33) with no class.
- No ABI/version handshake (only build-identity strings), no `struct_size` on most structs, no `extern "C"`, no thread-safety statement beyond per-function "calls must be serialized".
- Ambiguous classification: `T64_STOP_REASON_CPU_UNSUPPORTED`, `RESOURCE_LIMIT` and `QUIESCENT`. Decide which are guest-stops (resumable or terminal) and which are defects. `CPU_UNSUPPORTED` is an emulator limitation, so it should neither poison the instance nor claim the guest stopped by its own choice.
- Profile descriptor (`t64_profile_descriptor`) is a large public struct of fixed char arrays returned as a pointer to a static table. Freezing it freezes every field and its capacity. Prefer size+version on it or getter functions.

**Design recommendations.**
- Every in/out struct: `uint32_t struct_size` first (caller fills), reserved/flags after. The callee writes at most `min(caller size, known size)` for outputs and refuses an input smaller than the v1 minimum with state unchanged. Win32/Vulkan-style; do not adopt `sType`/`pNext` chains for a surface this small. Vulkan's `sType`+`pNext` and OpenXR negotiation structs (knowledge; I could not fetch the OpenXR loader chapter, 404) are the pattern to avoid over-copying.
- `t64_machine_create` takes a create-info with `struct_size` and `abi_version`. Mismatch returns a distinct `T64_STATUS_ABI_MISMATCH`. Also expose `t64_abi_version()` and the build identity for the SQLite-style assert.
- Replace public `bool` and `size_t` in structs with `uint8_t`/`uint32_t`/`uint64_t` (or document and test LP64 only). FFI hosts (Rust, Python, C#) otherwise differ on `bool` and `size_t`.
- Status classes: add a macro or function `t64_status_class(status)` returning REFUSED, DEFECT, or (for completeness) OK. Put the 33 codes plus the stop reasons in one table in `docs/spec/api.md` and add a check that every `T64_STATUS_*` and `T64_STOP_REASON_*` appears exactly once. Adding a status without classifying it then fails a check.
- Poison: one flag inside the instance, excluded from the digest and from the state container. Set only by a defect (an internal invariant violation that cannot be a caller fault). After poison, every call except `destroy` and a `t64_machine_is_poisoned()`-style query returns a `T64_STATUS_POISONED` refusal, with output arguments untouched. `destroy` must always work because it releases host allocator storage. Never poison on a caller error (invalid argument, overlap, wrong size): otherwise a bad call can brick the instance, and the same rule limits hostile-caller damage.
- Reentrancy and misuse detection that is cheap: an in-call flag turns an allocator callback or other reentrant call into a refusal. Contract violations the core cannot detect (use after destroy, concurrent calls on one instance, overlapping spans it cannot see) are caller undefined behaviour and must say so in the spec.
- Rings: keep copy-out semantics (the core never hands the host a pointer into core memory). It avoids the lifetime footgun libretro cores have with buffers valid only inside a callback. Add a single `t64_machine_output_status()` returning for each ring: queued count, free capacity, and the next packet size, so a real-time host can pace without trial and error. Backpressure is: `advance` returns a resumable stop (`RESOURCE_LIMIT`) with machine state consistent, never drops silently, and the digest does not depend on how or when the host drains (the existing ordering matrix covers "output-backpressure stop" and split-invariance; keep that guarantee and re-assert it in the host-shaped loop test).
- Timed input: input is stamped with a guest tick and refused if in the past or if the queue or sequence is exhausted (already so). Document explicitly that host wall time is never an input; the host chooses ticks.

**Footguns other emulator core APIs hit (libretro header plus knowledge).**
- Variable-size serialisation: `retro_serialize_size` is only promised "never larger than a previous returned value" while a game is loaded. Tendies64's state export size-query is fine; make the invariant explicit.
- Singleton cores: many libretro cores hold static globals and so allow one instance per process. Tendies64's lint forbids writable sections, so two instances in one process is a cheap, strong control (see section 4).
- Callback ordering and "only one of the audio callbacks may be used" style rules: Tendies64 has no callbacks during advance, which is an advantage to preserve (state it as a postcondition).
- Dropped or duplicated frame semantics and extensible-but-ambiguous behaviour when a frontend is slow: spell out what the core does when the host drains late (it stops with RESOURCE_LIMIT; it does not run ahead).
- Enum widths and struct padding in the public header, covered above.
- `retro_deinit` "must release all allocated resources before return": Tendies64's allocator-callback model makes the equivalent testable (allocator counts equal at destroy, including after poison).

**Security lens (hostile callers).** The existing exact check orders, overlap checks and zero-capacity NULL handling are the right base. Add: (a) every public function rejects NULL and zero-length-with-pointer combinations by documented rule; (b) a deterministic call-sequence fuzzer against the installed library under ASan and UBSan, asserting that a refused-class status leaves all observations equal (this reuses the repo's refusal snapshot idea); (c) state import already claims memory safety plus liveness for any blob (ADR-0002 evidence limits, `core/tests/fuzz_state_import.c`), so keep that fuzz target running against the shipped objects.

## 4. Enforcing API-04 mechanically (question 4)

**What already exists (ADR-0006, `tools/lint/`).** Layer 1 source scan (banned headers, time/entropy/file/stdio/alloc/thread/env names, cycle-counter intrinsics, inline asm, TLS, atomics, ctor/section attributes, writable file-scope heuristics) plus Layer 2 object check (non-empty writable sections, commons, constructor sections, undefined symbols outside the core and `allowlist.txt`). It compiles with a fixed flag set and has a seeded-violation corpus (`tools/lint/selftest`). `scripts/gate.sh` runs it as a late step. This already satisfies the spirit of API-04 for the source tree.

**What Phase 11 criterion 4 adds, and the gaps.**
- Run Layer 2 on the **shipped** objects (`ar x` from the installed archive; `lint-objects.sh --objects` exists for this). The lint's own compile uses `-fno-stack-protector -fno-pic -O2`, so it cannot see what the shipped build imports. Evidence from the existing archive: `___stack_chk_*` is imported. Either build core with `-fno-stack-protector` (a hardening trade-off against the input parsers) or add `__stack_chk_fail`/`__stack_chk_guard` to the allowlist for the installed check with an ADR-0006 amendment saying why (compiler runtime, no host service, process-abort only).
- Compute the undefined set at archive level (undefined minus defined in other members); per-object undefined symbols include `f32_*` and sha256 that are defined elsewhere in the archive.
- Add an installed-header include scan using the existing L1-INCLUDE allowlist (stdint/stddef/stdbool/limits/string and project headers). Public headers must not pull `<time.h>`, `<stdio.h>`, `<stdlib.h>`, `<pthread.h>`.
- Add a disassembly scan on shipped objects for hardware clock/entropy instructions (`rdtsc`, `rdrand`, `mrs ... cntvct`/`cntpct`). Layer 1 bans them by name, but compilers can emit them from builtins that evade a token scan (ADR-0002 evidence limits already note Layer 1-only classes are weaker).
- Positive control with seeded violations on the installed archive (reuse selftest seeds): an injected object referencing `clock_gettime`, `fopen`, `getentropy`, `arc4random`, `pthread_create`, `mmap` must fail.
- "No threads" also means thread-compatible. Run two instances on two threads in a TSan consumer, each with its own instance; any shared state is a failure. Combined with the no-writable-sections rule this proves instance independence. State the guarantee as: distinct instances are independent; one instance needs external serialisation.
- "No entropy": the profile carries `hle_seed_reset_version`, so check that the seed is a deterministic profile constant and not host input; the spec should say so.

None of this is review work. It is all `nm`, `ar`, `grep` and the existing lint, so it fits the "cheap checks first" rule in AUTONOMY.

## 5. Fit with the existing runner (question 5)

**Observed.** `runner/t64_real.c` (2619 lines) includes only `tendies64/t64_machine.h` and `t64_sha256.h` from the core side, and calls `t64_machine_create`, `admit_rom`, `queue_input`, `advance`, `video_copy`, `audio_copy`, `destroy`. It already is a public-API client, linked against the in-tree static `t64_core`. Three white-box probes (`t64_cold_boot.c`, `t64_rsp_diagnostic.c`, `t64_rsp_smoke.c`) include `t64_internal.h` and are not API consumers. `runner/tests/test_cpu_diagnostic.c` references internal headers too. The runner has its own build-identity manifest (`t64-runner-v1`, hashed from its listed inputs), and the supervisor and result-check tools pin to those identities.

**Recommendation.**
- Do not re-point the runner at the installed package. That would churn the runner build fingerprint, invalidate recorded qualification receipts and pins keyed to it, and make the installed consumer not independent (VER-05 wants independence).
- Keep the runner as the reference client. Add `tests/installed-consumer/` as a separate top-level CMake project, written without including any runner file, that runs the RUN-07 composite fixture and compares to the same independent expectations the runner is checked against. The criterion "same result as the in-tree runner" then means three-way agreement: installed consumer, in-tree runner, independent expected literals. That is stronger than either alone and keeps existing evidence intact.
- The ABI shape changes (struct_size, create-info, ABI version) will touch every call site in `t64_real.c` and the other runner files. Do this as one mechanical, early change with a before/after proof that all existing digests and expected bytes are byte-identical, mirroring Phase 10 criterion 1. Then re-qualify once. Putting it after the consumer exists would force two re-qualifications.
- Keep the white-box probes in-tree on `t64_internal.h`. A guard that fails if a public-API consumer (installed consumer, SDL host) includes anything outside `include/tendies64/` is a one-line `grep` check.

## 6. Lens summary

- **API designer:** candidate then final bind; struct_size, handshake, status-class table, poison, reset/unload.
- **Embedded/host integrator:** host-shaped loop test, `output_status`, LP64-clean types, `extern "C"`, documented threading and pacing.
- **Security:** no poison on caller faults, reentrancy refusal, call-sequence fuzz under ASan/UBSan against shipped objects, header/library mismatch refusal.
- **Devops/packaging:** fold sha256 into core, prefix and golden the symbol surface, install from the existing build tree for the gate, relocation and negative controls, no abidiff.
- **QA:** refusal-is-no-op harness, three-way differential, status table completeness check, seeded-leak control on the installed archive, two-instance interleave, TSan two-thread run.

## 7. Open points needing an owner or plan-time decision

- Whether to amend API-03 and the Phase 11/12 criteria to the candidate-then-final-bind form (recommended). It is a roadmap wording change, not a stop under AUTONOMY section 4.
- `__stack_chk_*`: allowlist versus `-fno-stack-protector` (ADR-0006 amendment).
- Classification of `CPU_UNSUPPORTED`, `RESOURCE_LIMIT`, `QUIESCENT` stop reasons.
- Whether the installed consumer also runs on Linux in `act` before hosted CI (AUTONOMY section 2 local-first rule); macOS hosted budget is the constraint.
- Unverified in this session: libretro serialisation and unload semantics are from the header text fetched (partial, truncated pages); mupen64plus mask detail is second-hand; OpenXR struct negotiation is from memory (404 on the loader chapter).

## Sources

- https://discourse.libsdl.org/t/announcing-sdl-3-1-3-stable-abi-preview/54743
- https://wiki.libsdl.org/SDL3/README-versions
- https://github.com/libretro/RetroArch/blob/master/libretro-common/include/libretro.h
- https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderDriverInterface.md
- https://www.sqlite.org/c3ref/libversion.html
- https://github.com/mupen64plus/mupen64plus-core (`src/api/frontend.c`, second-hand)
- https://cmake.org/cmake/help/latest/manual/cmake-packages.7.html
- https://cmake.org/cmake/help/latest/command/install.html
- Repo: `core/include/tendies64/t64_machine.h`, `t64_facade.h`; `core/CMakeLists.txt`; `runner/t64_real.c`; `docs/adr/0002-language-build-and-abi.md`; `docs/adr/0006-determinism-lint-mechanism.md`; `tools/lint/{rules.txt,allowlist.txt,lint-objects.sh}`; `docs/spec/machine-contract.md`; `.planning/ROADMAP.md`; `.planning/REQUIREMENTS.md`; `.planning/AUTONOMY.md`
