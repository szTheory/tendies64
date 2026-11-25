# Phase 11: Installed Consumer and Freeze Candidate - Research

**Researched:** 2026-11-25 (tree at commit c12c3b0; core build id `t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884` embedded in `build/gate/core/libt64_core.a`)
**Domain:** C99/C17 static-library C-ABI hardening, CMake package export, ABI-identity records, object-level determinism checks, host-shaped consumer tests
**Confidence:** HIGH for tree facts, install recipe, symbol surface and object checks (all run this session); MEDIUM for class-table proposals and plan sizing; LOW only where tagged ASSUMED or UNVERIFIED

Tags: VERIFIED = I ran it or opened the file this session (path:line or command given); CITED = primary-source URL fetched this session; ASSUMED = not verified. Every experiment ran in the session scratchpad (`.../scratchpad/`), never in the repo. No external package is installed by this phase.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
(copied from `11-CONTEXT.md` "Implementation Decisions", locked part, abbreviated only by dropping the Reversibility notes; the file is authoritative)
- Freeze is candidate-then-final: Phase 11 binds a 0.x candidate record (header hash, public symbol list, struct-layout table, status-class table); the final bind is the last plan of Phase 12; between the two only defect-driven amendments with evidence and a re-run of the installed-consumer suite.
- Every in/out public struct carries a leading caller-filled `uint32_t struct_size`; the callee reads/writes at most `min(caller, known)` and refuses an input smaller than the v1 minimum with state unchanged; no `sType`/`pNext` chains.
- `t64_machine_create` takes a create-info with `struct_size` and `abi_version`; mismatch returns a distinct status (ABI mismatch); a `t64_abi_version()` query and the build identity let a consumer assert header/library agreement (SQLite pattern).
- Three error classes with one status-class table listing every `T64_STATUS_*` and `T64_STOP_REASON_*` exactly once: refused (state unchanged, observations bit-identical), guest-stop (not an error, resumable), defect (instance poisoned: only destroy and a poison query succeed, outputs untouched, destroy always releases host allocator storage); poison is never set by a caller fault; a check fails if a status or stop reason is unclassified.
- Public `reset` and `unload` are new surface with accepted and refused cases; `bool` and `size_t` leave public structs (fixed-width types); `extern "C"` guards added; copy-out ring semantics kept; `advance` never drops output and stops resumably at backpressure; host wall time is never an input.
- Installability: fold the sha256 sources into `t64_core` (no private link to `t64_sha256`), relocatable `tendies64Config.cmake` plus version file with `SameMinorVersion` for 0.x, in-tree `tendies64::core` alias, package version is the ABI candidate number (not a release version, no tag, `project(VERSION)` untouched), config records the single built configuration and `T64_BUILD_ID`; internal symbols prefixed (`t64_` internal cross-TU helpers and the unprefixed Berkeley SoftFloat names via a small provenance-documented rename header); public API exported through one `T64_API` macro; a golden public symbol list is checked with `nm`.
- Independent consumer is a separate top-level CMake project under `tests/installed-consumer/` that includes no runner or core-internal file, runs the Phase 10 composite fixture, and agrees three ways (installed consumer, in-tree runner, independent literals); the runner is NOT re-pointed at the installed package; a grep guard fails if a public-API consumer includes anything outside `include/tendies64/`.
- Negative controls (all must fail loudly): missing package, version mismatch, visible source tree, doctored header, symbol collision (consumer defines its own `f32_add` and `t64_sha256_init`; link succeeds and results unchanged), relocated prefix including a path with spaces, seeded `clock_gettime`/`fopen`/`getentropy`/`pthread_create` object.
- Host-shaped loop in the installed consumer, no SDL: frame-paced bounded advances, drain video and audio rings in host order, forced backpressure stops, two instances interleaved in one process; the digest never depends on how or when the host drains.
- Refusal-is-a-no-op harness: deterministic call-sequence generator snapshots observations before each call and asserts bit-equality after any refused-class status; proves a guest-stop resumes and a defect poisons; runs under ASan and UBSan against the shipped library.
- API-04 enforced on the shipped objects: extract archive members, compute the undefined set at archive level, reuse `tools/lint/lint-objects.sh --objects`, scan installed headers with the L1-INCLUDE allowlist, scan disassembly for hardware clock/entropy instructions, prove failure with seeded objects.
- Order of work: shape changes once and early with a before/after proof that every existing digest and expected byte is identical; cheap-first checks before the long suite; relabel the 20 stale "Phase 10 freeze" mentions and the 19-20 deferred "until Phase 10" production comments in the same wave as the shape change (done when `git grep -n -i -E 'until (the )?Phase 10' -- core runner tools` prints nothing).
- Phase 10 carry-ins (W2-01, W2-02, DEF-W1-01, W1-03, D-10-17-01, Phase 9 items) are each triaged as done, deferred or dropped with a recorded reason; none blocks install work.
- The paper LibFuturo contract read happens in this phase before the freeze candidate; any adapter is optional and must not shape the core API.
- Plans capped at 4 tasks and about 45 minutes; no decimal plans; a defect fix is a new numbered plan; independent review per wave in scratch clones; parallelization false; `git.create_tag` false.

### Decided autonomously (re-bindable until the final bind)
Installed headers = `t64_machine.h` plus any new public header (`t64_facade.h` stays in-tree and is not in the freeze record); `__stack_chk_fail`/`__stack_chk_guard` allowlisted for the installed-archive check via an ADR-0006 amendment (lint compile stays `-fno-stack-protector`); `RESOURCE_LIMIT` resumable guest-stop (backpressure), `QUIESCENT` guest-stop, `CPU_UNSUPPORTED` terminal guest-stop that neither poisons nor claims the guest chose to stop; new statuses appended after 33 without renumbering; one poison flag inside the instance outside the digest and state container, in-call flag makes reentrant calls a refusal; one `t64_machine_output_status` style call; threading statement (distinct instances independent, one instance externally serialized, two-thread TSan run in a non-gating or local lane); `t64_profile_descriptor` gets `struct_size`/version or getters (planner picks the cheaper); optional plain-`cc` + `.pc` smoke only under one task; hosted footprint = cheap-first checks plus install-from-existing-build-tree on hosted macOS and Linux (`act` first), fuller fresh-archive variant in a manual or nightly lane, no hosted timeout change; evidence tier authored-scope, build-identity-bound, software-interface claim only; review per wave in scratch clones plus one final audit.

### Claude's Discretion
Exact new status and function names, the `T64_API` macro spelling and the internal symbol prefix; plan decomposition inside the stated order (estimate 12-16 plans); whether poison tests use an injected hook or a test-only trigger, and the call-sequence generator grammar.

### Deferred Ideas (OUT OF SCOPE)
Final ABI bind to major 1, host pump, SDL3 shell, authored playable title, written LibFuturo evaluation (all Phase 12); VID-04 oracle evidence (D5) and any release/tag/channel (D6); abi-compliance-checker/libabigail and any new dependency; a libretro-style wrapper or LibFuturo adapter.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| VER-05 | Clean installed package (relocated, no source tree visible) runs the permitted headless fixture through an independent consumer agreeing with the in-tree runner and independent literals; relocation, missing-package, version-mismatch, doctored-header, symbol-collision controls | Sections "CMake install recipe" (verified), "Composite fixture" (literals in `fixtures/composite/composite.expected.json`), "Symbol surface" (collision is a silent override today), negative-control table |
| API-01 | Instance API: lifetime, ABI handshake at create, `struct_size`, profile selection, transactional admission, timed input, bounded advance, rings with backpressure, reset, unload; installable, internal symbols prefixed | "Header inventory", "Reset/unload/poison", "Symbol surface", "Blast radius" |
| API-02 | Three error classes, status-class table, refusal-is-a-no-op harness under ASan/UBSan | "Status and stop-reason inventory" (with five flagged ambiguities), "Call-sequence generator" |
| API-03 | Freeze candidate bound record (header hash, symbol list, layout table, status-class table), amendment rule | "Freeze-candidate record", "Validation Architecture" |
| API-04 | Core owns no files/clocks/entropy/threads; mechanical check on installed archive members with seeded violation | "API-04 object check" (run, seeded, Linux/macOS) |
</phase_requirements>

## Summary

The tree matches the fan-out lens on the big points and corrects it on several numbers. The archive at HEAD has **248 defined global symbols** (not 293): 30 public functions, 170 internal `t64_*` helpers, 48 SoftFloat names, all text symbols, zero data; nine imports remain (`___stack_chk_fail`, `___stack_chk_guard`, `_bzero`, `_memcmp`, `_memcpy`, `_memset`, three `t64_sha256_*`). I built, in a scratch copy, the full package recipe and the symbol prefix, and ran the real `tools/lint/lint-objects.sh --objects` on extracted members: after the sha256 fold the **only** violations are the two stack-protector symbols; with two allowlist rows it passes; a seeded object importing `clock_gettime`, `fopen`, `getentropy` and `pthread_create` fails with exit 1. A pure macro rename header (`#define name t64i_name`, generated from `nm`) gives exactly 30 public + 222 `t64i_*` symbols on Apple clang 21 and Ubuntu gcc 13.3 and the white-box tests still link and pass, with one trap: nine SoftFloat primitives use a self-defining-macro protocol that a `#define` rename breaks, fixed by deleting three lines in each of nine vendored files.

The three hazards that decide plan sequencing are not in CONTEXT. (1) **Nine of the ten C files bound by the Phase 7 oracle-request check call the API that must change** (`t64_machine_create`, `t64_stop_request`, `t64_host_allocator`, `t64_video_packet_info`); editing them forces a non-manifest identity-only rebind that no recorded owner approval covers. (2) The shape change touches **155 `t64_machine_create` call lines in 74 files and about 145 positional `t64_stop_request` initializers**; with `-Werror` the compiler catches every miss but a codemod and per-directory plans are needed. (3) `dispatch_front` folds **every non-OK handler status into a `RESOURCE_LIMIT` stop** and four public observers (`t64_audio_info/copy`, `t64_video_info/copy`) are sealed bodies in `scripts/check-rsp-no-hle.py`, so poison placement and entry guards must avoid re-pinning sealed bodies. Gate growth is governed by G4 as written (cold hosted gate-macos 731 s of 900 s, margin 169 s).

**Primary recommendation:** Land one production-edit wave in this order inside it (types and handshake, poison/reset/unload, symbol prefix and sha fold, comment relabel), prove it with the two-tree digest sweep (printer shim) and one orchestrator rebind, then add the package, consumer, generator and object checks as CTest names inside the existing gate (no `ci.yml` edit), keep sanitizer and TSan runs local, and bind the candidate record from generated goldens.

## Project Constraints (from CLAUDE.md and AGENTS.md)

- Read `AGENTS.md`, `.planning/AUTONOMY.md`; project-owned GSD files are authority; dated research is re-checked before becoming a requirement. (`.claude/CLAUDE.md`)
- Small flat dependency tree; a small provenance-documented copy beats a dependency (AGENTS.md "Prefer a small, flat dependency tree"). No dependency is added here; abi-compliance-checker rejected by CONTEXT.
- Evidence tiers: support claims bound to profile, build identity and evidence tier; no claim from agreement alone. Phase 11 claims are software-interface claims only.
- CI/CD local-first: reproduce locally, `act` for Linux, cheap consistency checks before long suites (AUTONOMY section 2); hosted proof is not replaced by `act`.
- Plans: 4 tasks / about 45 minutes, review per wave in `git archive` scratch clones, `parallelization` false, no tags (AUTONOMY "v1.2 execution rules").
- Rights stops: no protected ROMs; the composite fixture is authored here (PIN-IMPACT section 11).

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| ABI handshake, struct_size validation, poison/in-call guards | Core library (`core/t64_machine.c`, public entry layer) | Public header | Only the library can refuse before touching state |
| Status-class table | Public header + `docs/spec/api.md` | `abi.*` checker | One source; checker proves every constant appears once |
| Symbol prefix and public export list | Core build (`t64_prefix.h`, `T64_API`) | Golden file checked by `nm` | Static-archive collisions are link-time, not visibility |
| Package export, relocatable config, version policy | CMake (`core/CMakeLists.txt`) | `tests/installed-consumer` | Install rules live with the target |
| Independent consumer, host-shaped loop, call-sequence generator | `tests/installed-consumer/` (host side) | Runner stays in-tree reference | Independence from runner and core internals is the point |
| API-04 object and header scans | `tools/lint` + new checker script | CTest wrappers | Existing Layer 2 tool already computes archive-level undefined |
| Host wall time, files, entropy | Host (consumer) | none | The core owns none (API-04) |

## Standard Stack

No external package is added. Tools verified on this machine:

| Tool | Version | Use | Source |
|------|---------|-----|--------|
| CMake | 4.4.3 (min required 3.25) | `install(EXPORT)`, `write_basic_package_version_file`, `FILE_SET HEADERS` | [VERIFIED: `cmake --version`; `CMakeLists.txt:1`] |
| Ninja | 1.13.2 | single-config generator (core requires single-config, `core/CMakeLists.txt:3-6`) | [VERIFIED: `ninja --version`] |
| Apple clang | 21.0.0 (arm64) | shipped flags `-O3 -DNDEBUG -std=c17 -Wall ... -Werror` | [VERIFIED: `ninja -C build/gate -t commands`] |
| gcc (act image) | 13.3.0 Ubuntu x86_64 (emulated) | Linux symbol/lint probe | [VERIFIED: docker run, image `ghcr.io/catthehacker/ubuntu:act-24.04`] |
| nm, ar, size, objdump | Apple LLVM 21 | golden, members, section check, disassembly scan | [VERIFIED: ran] |
| sh, python3 stdlib | system | checkers; no new Python dependency | [VERIFIED: existing scripts] |

**Alternatives rejected:** `llvm-objcopy`/`objcopy` for symbol hiding (not installed on macOS: `which` found none [VERIFIED]; ELF `ld -r` hidden symbols still collide); abidiff/abi-compliance-checker (CONTEXT).
**Installation:** none.

## Package Legitimacy Audit

This phase installs no external package (npm, PyPI, crates). SDL3, abi-compliance-checker, libabigail are out of scope or rejected. Packages removed: none. Packages flagged: none. Registry commands (`npm view`, `pip index`, `cargo search`) are not applicable. The `package-legitimacy` seam was not run because there is nothing to check.

## Verified tree facts (item 1)

| Fact | Value | Evidence |
|------|-------|----------|
| Public headers | `t64_machine.h` 684 lines, `t64_facade.h` 57 lines (only these two under `core/include/tendies64`) | [VERIFIED: `wc -l`] |
| `struct_size` mentions | `t64_machine.h` 1, `t64_facade.h` 0 | [VERIFIED: grep] |
| Public struct types in `t64_machine.h` | 12: `t64_host_storage`, `t64_host_allocator`, `t64_profile_descriptor`, `t64_stop_request`, `t64_stop_result`, `t64_cpu_stop_detail`, `t64_trace_record`, `t64_output_drain_result`, `t64_video_packet_info`, `t64_audio_packet_info`, `t64_state_import_report`, `t64_save_info`; only `t64_audio_packet_info` has `uint32_t struct_size;` (line 328) so **11 lack it** | [VERIFIED: read lines 164-342, 518-524, 574-579] |
| `size_t` | 26 lines: 7 struct members (166, 167, 276, 277, 318, 319, 339) and 19 in the allocator typedef and prototypes | [VERIFIED: `grep -n -E '\bsize_t\b'`] |
| `bool` | 5 lines: struct members at 280-281 (`lost_output`, `total_attempted_bytes_exact`), allocator typedef 170, `trace_set_enabled` 672, `drop_count_saturated_out` 682 | [VERIFIED: grep] |
| `extern "C"` | none | [VERIFIED: grep] |
| Public functions | 30 (all defined in the archive; none declared-but-undefined) | [VERIFIED: header scan vs `nm`] |
| Archive | 86 members (36 core + 50 SoftFloat), 248 global defined (all `T`), 9 archive-level undefined; CONTEXT's "293" is not reproducible | [VERIFIED: `nm -g --defined-only`, `ar t`] |
| Global defined breakdown | 30 public, 170 internal `t64_*`, 48 SoftFloat (`f32_*`, `f64_*`, `i32_*`, `i64_*`, `softfloat_*`); internal group includes 4 `t64_test_*` hooks (`t64_internal.h:1483-1491`), `t64_core_identity` and 5 `t64_sf_*` | [VERIFIED] |
| Shipped compile line | `-DSOFTFLOAT_FAST_INT64=1 -DT64_BUILD_ID="..." -DT64_SOURCE_REVISION="..." -O3 -DNDEBUG -std=c17 -arch arm64 -Wall -Wextra -Wpedantic -Wshadow -Wconversion ... -Werror -Wdouble-promotion -Wframe-larger-than=2048` (stack protector is the Apple default, hence the `___stack_chk_*` imports) | [VERIFIED: `ninja -t commands`] |
| Lint compile line | `-std=c17 -O2 -fno-common -fno-stack-protector -fno-pic -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0` | [VERIFIED: `tools/lint/lint-objects.sh`] |
| CMake structure | `t64_core` STATIC from globbed `core/t64_*.c` + SoftFloat list + headers; PUBLIC include is an absolute path (must be wrapped in `$<BUILD_INTERFACE:>` for export); PRIVATE `tools/common`; `target_link_libraries(t64_core PRIVATE t64_sha256)` at `core/CMakeLists.txt:114`; PRIVATE definitions `T64_BUILD_ID`, `T64_SOURCE_REVISION`; single-config guard at lines 3-6; root `project(tendies64 VERSION 0.0.0 LANGUAGES C)` | [VERIFIED: read] |
| `T64_BUILD_ID` | `t64-build-v1-` + SHA-256 of a manifest: format, `PROJECT_VERSION`, cmake and compiler versions, target, platform, `CMAKE_BUILD_TYPE`, C standard, warnings, flags, plus the SHA-256 of every core `.c`/`.h`, SoftFloat file, both CMakeLists, `CMakePresets.json`, `tools/common/CMakeLists.txt`, `t64_sha256.{c,h}`; line `set(T64_BUILD_ID "t64-build-v1-${T64_BUILD_FINGERPRINT}")` is matched literally by `scripts/digest-sweep.py:60` | [VERIFIED: `core/CMakeLists.txt:20-66`] |
| Stale "until Phase 10" text | exactly 20 lines in 8 files: `t64_machine.h` 12 (30, 93, 98, 102, 120, 135, 238, 389, 428, 564, 599, 646), `t64_internal.h:1891`, `t64_machine.c:1045`, `t64_state.c:10,391`, `runner/t64_artifact_publisher.h:13`, `runner/t64_replay.h:6`, `runner/t64_save_flags.h:11`, `runner/tests/test_state_runner.c:2211`; `tools/` none | [VERIFIED: `git grep -n -i -E 'until (the )?Phase 10' -- core runner tools`; wrap-aware PCRE gives the same set] |
| Stale text the stated grep misses | `core/include/tendies64/t64_facade.h:6` ("the API freeze is Phase 10") and the banners `t64_machine.h:1-4`, `t64_facade.h:1-34`; `docs/adr/0002-language-build-and-abi.md:55,91` | [VERIFIED: `git grep -n -E 'Phase[ -]?10'`] |
| Stale text that must stay | `docs/claims/phase09.md:32`, `docs/evidence/phase09-qualification.md:276`, `fixtures/state/decode_container.py` (closed records, pinned oracle) | [VERIFIED: PIN-IMPACT section 13 T4] |
| Docs "20 stale mentions" | Already swept: Phase 10 plan 10-16 relabeled the 15 sites of 7 spec docs (gate `stale 0`); the "20" in CONTEXT/ROADMAP is the production-line count above | [VERIFIED: PIN-IMPACT section 12] |

## Pin-impact propagation (item 2)

Any production text edit moves `T64_BUILD_ID` (every core `.c`/`.h` is hashed) and, through it, the packet-bearing digests of pre-v25 ids, because retained packet metadata keeps `profile_id`/`build_id` only through v24 [VERIFIED: `docs/spec/machine-contract.md:957-962`, PIN-IMPACT section 1]. No test or script pins the *real* build id as a literal (only synthetic `aaaa...` ids appear) [VERIFIED: `git grep 't64-build-v1-[0-9a-f]{8}'`], so build-id movement itself is cheap. The things that do move or break, with the cheapest check:

| Edit class | Pin it moves | Cheap check |
|------------|-------------|-------------|
| Any edit of the 10 C files bound by `scripts/check-rdp-oracle-authority.py` (`EXPECTED_CORPUS_PATHS`: `core/tests/test_rdp{,_copy,_triangles,_texture,_combiner,_blender,_depth}.c`, `test_vi.c`, `test_video_output.c`, `runner/tests/test_rdp_video.c`, plus `fill.S`, `check-rdp-vi-cases.py`, `rdp-vi-cases.json`) | whole-file SHA-256 and the derived `PINNED_REQUEST_SHA256` (`:23`); orchestrator-only rebind | `ctest -R '^harness\.rdp-oracle-authority$'` (0.2 s) |
| API-shape edit of those same files | 9 of the 10 C files use the changing API (counts: test_rdp.c create=3 stop_request=1; test_vi.c create=4 stop_request=12 video_info=46; test_video_output.c create=1 stop_request=3; the others create 1-2, allocator 1-2; `test_rdp_video.c` uses none) | `grep -c` per file [VERIFIED] |
| Public functions that are sealed bodies | `core/t64_ai.c :: t64_audio_copy`, `:: t64_audio_info`, `core/t64_vi.c :: t64_video_copy`, `:: t64_video_info` are among 231 `REVIEWED_BODY_DIGESTS` keys; `t64_bus_write` and `t64_bus_write_masked` (callees of the public bus wrappers) are sealed too | `python3 -B scripts/check-rsp-no-hle.py --build build/gate` [VERIFIED: parsed keys] |
| Digest sweep printer | `tools/digest-sweep/t64_digest_sweep.c` (330 lines, a few API sites) compiles against base and head trees with one source; a changed `t64_machine_create` signature breaks the base build unless the printer carries a compat shim keyed on a macro such as `T64_ABI_VERSION_MAJOR`; the driver also requires the literal `set(T64_BUILD_ID ...)` line to occur exactly once in `core/CMakeLists.txt` and builds targets `t64_core t64_sha256` | `python3 -B scripts/digest-sweep.py --selftest` (91 s) [VERIFIED: `scripts/digest-sweep.py:60,181`] |
| New public/internal struct member (`poisoned`, `in_call`) | doc rows in `docs/spec/machine-contract.md:957` "State member audit" (68 members; no test parses it); `state-field-rules.txt` only covers members used as index/length/shift/loop bound, so a plain flag needs no row | `ctest -R '^harness\.state-field-use-audit$'` [VERIFIED] |
| Any function whose body mentions a profile version field or predicate | `docs/spec/profile-membership-sites.txt` (generated) | `ctest -R '^repo\.si-membership-sites$'` |
| New CTest name | exact lists in `scripts/check-test-inventory.sh` (`PHASE10_TESTS` block at line 245 is the template; add a `PHASE11_TESTS` block and an `ALL` concatenation at line 377), preflight label counts, about 0.2 s selftest cost per name; baseline 254 names | `sh scripts/check-test-inventory.sh --preset gate` |
| Edits of `ci.yml`, `check-ci-yml.sh`, `gate.sh` | bound source; scanner rules `CI-RUN-LINE`, `CI-STRESS-WORKFLOW`, `CI-WORKFLOW-FILE` reject unreviewed lines and extra workflow files | `ctest -R '^repo\.ci-yml'` |
| Phase-root literals | `scripts/qualify-phase10.py:64` (`PIN_IMPACT_SUBPATH`), `scripts/preflight-phase10.sh` (`PF_ACT_IMAGE`, regex table), `scripts/check-test-inventory.sh` | copy to `qualify-phase11.py` / `preflight-phase11.sh` |
| Closed records | `docs/claims/phase10.md` and `docs/evidence/phase10-qualification.md` cite `b6c298e6...`; they stay as history; Phase 11 writes `docs/claims/phase11.md` | `sh scripts/check-claims.sh` (`errors=0`) |

**Re-pin once:** the Phase 10 pattern (Wave 0 base recorded as a 40-hex line in a PIN-IMPACT-style file, two-tree sweep against that base, forced build id `t64-build-v1-sweep`) is the proof that "shape change moves nothing observable". Reuse it unchanged except for the printer shim.

### Composite fixture (what the consumer reproduces)

- ROM `build/fixtures/phase10-composite-s32k.z64` (5,496 bytes, ROM SHA-256 `7a0426b4...4945`), wrong-byte control `phase10-composite-wrong-s32k.z64`, built by `scripts/build-composite-fixture.sh` from `fixtures/composite/composite.S` with the pinned Binutils (not installed; consumer takes a path argument) [VERIFIED: `ls build/fixtures`, PIN-IMPACT 11.1].
- Profile `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k`; guest completes at tick 599,357 with stop reason `T64_STOP_REASON_GUEST_COMPLETION`; state digest `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c` (PIN-IMPACT 11.3).
- Input script `fixtures/composite/composite.input.json`, verbatim: `{"events":[{"buttons":2049,"port":0,"tick":"564000","x":20,"y":-20},{"buttons":17424,"port":0,"tick":"582000","x":-100,"y":100}],"schema":"t64.input","schema_version":1}` (the consumer hard-codes these two events and a test compares them to the file).
- Independent literals: `fixtures/composite/composite.expected.json` (generated by `composite_oracle.py --write`, `harness.composite-oracle` proves it equals the oracle): video 4x3 RGBA8 48 bytes `00ff00ff...`, two 16-byte S16BE audio packets, save SHA-256 `f0b7fe88...6d`, 188-byte ISViewer transcript SHA-256 `da07d916f0314db05e8d609326242d7f944098e96ce19a96339ef1cd6a7aad39`, polled `["00000000","080114ec","44109c64"]`.
- The runner invocation is `runner.composite-two-run` (`runner/CMakeLists.txt:725-752`): raw-capture run and save-out run, same final digest. The installed consumer does one in-process run: admit, queue the two inputs, advance, drain video (3 packets), audio (2 packets), `output_drain` transcript, `save_export` SRAM raw 32,768 bytes, digest; compare each to the literals. The runner's raw capture holds 3 video frames and the video ring holds two packets, so an undrained run probably hits a backpressure stop at the third field boundary without a second guest (09-06 decision: "a third field boundary stalls the VI"); that is ASSUMED until the consumer is run once without draining.

## CMake install recipe (item 3, run in scratch)

Scratch copy of HEAD (`git archive`), patched `core/CMakeLists.txt`, Ninja Release, CMake 4.4.3, Apple clang 21, macOS arm64. All results [VERIFIED: ran].

```cmake
# core/CMakeLists.txt changes (scratch-tested)
add_library(t64_core STATIC ${T64_CORE_SOURCES} ${T64_SOFTFLOAT_SOURCES}
  ${T64_SOFTFLOAT_HEADERS} "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.c")   # fold sha256
target_include_directories(t64_core
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>"
  PRIVATE "${CMAKE_SOURCE_DIR}/tools/common" "${CMAKE_CURRENT_SOURCE_DIR}/softfloat")
# remove: target_link_libraries(t64_core PRIVATE t64_sha256)
target_compile_definitions(t64_core PRIVATE   # core's private copy; host tools keep t64_sha256_*
  t64_sha256_init=t64i_sha256_init t64_sha256_update=t64i_sha256_update
  t64_sha256_final=t64i_sha256_final t64_sha256_hex=t64i_sha256_hex)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(tendies64::core ALIAS t64_core)
set_target_properties(t64_core PROPERTIES EXPORT_NAME core)
target_sources(t64_core PUBLIC FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/tendies64/t64_machine.h")
install(TARGETS t64_core EXPORT tendies64Targets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT Development FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT Development)
install(EXPORT tendies64Targets NAMESPACE tendies64:: FILE tendies64Targets.cmake
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tendies64" COMPONENT Development)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/tendies64ConfigVersion.cmake"
  VERSION 0.1.0 COMPATIBILITY SameMinorVersion)     # ABI candidate number, not project(VERSION)
configure_package_config_file(tendies64Config.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/tendies64Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tendies64")  # .in sets tendies64_BUILD_ID, _BUILD_TYPE
```

Results:
- Configure + generate succeeded with sha256 folded (the lens's reproduced failure was the private link). `ninja t64_core` alone then `cmake --install bld --component Development --prefix "<dir with spaces>"` installed 6 files (`lib/libt64_core.a`, `include/tendies64/t64_machine.h`, `lib/cmake/tendies64/{tendies64Config,tendies64ConfigVersion,tendies64Targets,tendies64Targets-release}.cmake`). `cmake --install` needs no other target built, so "install from the existing gate build tree" holds once the gate build has run; configure after the CMake edit moves the build id and rebuilds objects once.
- Relocation: consumer `find_package(tendies64 0.1 REQUIRED CONFIG)` + `target_link_libraries(consumer PRIVATE tendies64::core)` built and ran against `inst dir with space`, then after `mv` to `moved prefix`; the second build tree contains no occurrence of the old path. `_IMPORT_PREFIX` is derived from the config file location; `IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libt64_core.a"`.
- Absolute-path scan: `grep -rl` of the installed tree and `strings -a` of the archive for the scratch path, `/Users/`, `/private/`, `/var/folders` found nothing (archive is Release, no `-g`).
- Version policy: request `0.1` accepted; `0.2` and `0.1.5` rejected with "The version found is not compatible with the version requested" (SameMinorVersion rejects a higher patch than installed); missing prefix gives "Could not find a package configuration file provided by "tendies64"".
- Install tree is 6 files; the config records `tendies64_BUILD_ID` and `tendies64_BUILD_TYPE` for the consumer to compare with `t64_build_identity()`.
- macOS vs Linux: `act-24.04` image has **no cmake and no ninja** [VERIFIED], so `act` can run only compile/`nm`/lint-objects probes (done: 17.6 s for 86 units under amd64 emulation), not `cmake --install`; hosted ubuntu-24.04 has CMake, so the Linux install path is **UNVERIFIED until a hosted run**. `GNUInstallDirs` uses `lib/<multiarch>` only for prefix `/usr`; `_IMPORT_PREFIX` depth is generated, so relocatability is unaffected [CITED: cmake docs, lens-api.md section 2; not re-fetched].
- A prefix with spaces works for `cmake --install --prefix`, `CMAKE_PREFIX_PATH`, `find_package` and the build; `tools/lint/lint-objects.sh` documents "Include paths must not contain spaces" for its own `-I` flags only (`--objects` is path-safe: it ran on members under the long scratch path; members with spaces were not tried) [VERIFIED/ASSUMED boundary: extract members to a space-free temp dir].

Negative-control inventory with one-line implementation each: missing package (`CMAKE_PREFIX_PATH` empty); version mismatch (`find_package(tendies64 0.2)`); visible source tree (assert consumer `compile_commands.json` and the ninja link line contain no path under the repo; `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` worked); doctored header (copy installed header, edit `T64_ABI_VERSION_*` or shrink a struct, expect the new ABI-mismatch status or a v1-minimum refusal at `create`); relocation with spaces (above); symbol collision (see next section: must compare *results*); seeded violating object (below).

## Symbol surface (item 4)

**Today's collision behavior is a silent override, not a link error.** A consumer defining `f32_add` and `t64_sha256_init` linked successfully against the scratch archive; the linker map showed `libt64_core.a(t64_softfloat.c.o)` pulled and `f32_add.c.o` *not* pulled (0 matches), so the core's own COP1 arithmetic would call the consumer's `f32_add` [VERIFIED: `-Wl,-map`]. When a member that defines the name is pulled in by another symbol the result is `duplicate symbol '_f32_add'` (macOS) or `multiple definition of 'f32_add'` (Linux) [VERIFIED]. `-fvisibility=hidden` changes neither outcome on either OS [VERIFIED]. The "symbol collision" control must therefore assert *unchanged results* (guest COP1 add through the composite or a COP1 fixture, digest equal), and the Phase 11 plan should show the control failing against an unprefixed build once.

**Mechanism compared**

| Mechanism | macOS | Linux | Cost | Verdict |
|-----------|-------|-------|------|---------|
| Generated rename header, `#define name t64i_name` | works; built 86 TUs `-Werror` clean | works (gcc 13.3, `-include core/t64_prefix.h`) | one generated header (218 `#define` lines plus guards), one `-include` on `t64_core`, one `#include` at the top of `t64_internal.h` so white-box tests and probes follow; text of every sealed body unchanged | **recommended** [VERIFIED] |
| `#pragma redefine_extname` | works but emits a symbol without the Mach-O underscore | **ignored by gcc 13.3 for definitions** (symbol stayed `softfloat_x`) | n/a | rejected [VERIFIED] |
| `ld -r -exported_symbols_list` | gives 30 `T` symbols, colliding consumer linked | needs `objcopy` (binutils) and a different recipe; hidden symbols still collide on ELF | two code paths; no `objcopy`/`llvm-objcopy` on macOS | rejected [VERIFIED on macOS; ELF part from the visibility test] |
| `-fvisibility=hidden` | does not prevent collision | does not prevent collision | trivial | keep only as defense in depth with `T64_API` |

**SoftFloat trap (VERIFIED):** nine vendored primitives (`softfloat_approxRecip32_1`, `_approxRecipSqrt32_1`, `_countLeadingZeros32`, `_countLeadingZeros64`, `_mul64To128`, `_shiftRightJam32`, `_shiftRightJam64`, `_shiftRightJam64Extra`, `_shortShiftRightJam64`) are defined in `core/softfloat/s_*.c` under `#ifndef softfloat_X` with a self `#define softfloat_X softfloat_X`; a `#define` rename makes the guard skip the definition (archive dropped to 243 symbols, link failures in tests). Deleting the guard line, the self-define line and the trailing `#endif` in those nine files (27 lines) gave exactly 252 = 30 public + 222 `t64i_*` (170 internal + 48 SoftFloat + 4 sha256) on both compilers. These nine are `mode: adapted` rows in `provenance/copies.json` (only the upstream SHA is recorded), so the edit is a documented modification (update each `modifications` text) and `sh scripts/check-provenance.sh`; no hash row moves.
Verified after the edit: `unit.core.softfloat32` checks=10466, `unit.core.softfloat64` checks=10478, `unit.core.softfloat-isolation` 84, `unit.core.cop1-state` 829, `unit.core.lifecycle` 317, all failures=0 (macOS scratch, gate environment variables exported).

**Residual unprefixed symbols to decide:** `t64_core_identity` is defined by `core/t64_core_stub.c` and used by `runner/t64_supervisor.c:322` through `t64_facade.h` (not installed). The generated header renames it; the facade header would then need to include the rename header, or the symbol stays unprefixed and appears in the golden as a documented non-API symbol. Cheapest: keep it unprefixed, list it in the golden under "in-tree facade symbol (not API)". Four `t64_test_*` hooks ship in the archive (`t64_internal.h:1483-1491`); they become `t64i_test_*`.

**Golden `nm` check (portable, run on both):**
```sh
nm -P -g --defined-only "$LIB" | awk -v os="$(uname -s)" 'NF>=3 && $2 ~ /^[A-Z]$/ {n=$1; if (os=="Darwin") sub(/^_/,"",n); print n}' | LC_ALL=C sort -u
```
Compare to `docs/abi/public-symbols.txt` (public list) and assert every other name starts with `t64i_`. Same flags work on Apple nm and GNU nm [VERIFIED both: macOS 252 lines; Linux 252 `T`]. The check is a few milliseconds.

**Internal-name completeness:** the rename header is generated from the set (defined globals minus public list); a gate test regenerates it and diffs, and the golden check proves nothing escaped. 164 of the 170 internal names are declared in `t64_internal.h`/`t64_rsp_divider.h`; the other six are `t64_core_identity` and five `t64_sf_*` declared elsewhere [VERIFIED].

## Status and stop-reason inventory (item 5)

Status constants are `UINT32_C` 0 to 33 (`t64_machine.h:73-112`), e.g. `#define T64_STATUS_STATE_INVARIANT UINT32_C(33)`. Stop reasons are `T64_STOP_REASON_BOUND UINT32_C(1)` through `T64_STOP_REASON_GUEST_COMPLETION UINT32_C(8)` (`:146-154`). Counts are lines in `core/*.c` [VERIFIED: grep]. **Proposed class** column is my recommendation; rows marked AMBIG need a decision recorded in the class table.

| Value(s) | Name(s) | Where returned | Proposed class |
|----------|---------|----------------|----------------|
| 0 | OK | everywhere (542 sites) | success (not an error class; table row `ok`) |
| 1 | INVALID_ARGUMENT (144 sites) | NULL/overlap/range checks in all public entries | refused. AMBIG-A: `t64_cp0_synchronize_guest_time` returns it for a corrupt internal phase (`t64_cp0.c:558-575`) and `t64_machine_admit_rom` returns it when the stored profile digest no longer matches the profile bytes (`t64_machine.c:2937-2944`): internal invariant violations, i.e. defect candidates |
| 2 | UNKNOWN_PROFILE | `t64_profile_lookup` (`t64_machine.c:2566`) | refused (no instance) |
| 3 | ALLOCATION_FAILED | host allocator refusal at create/admit (`t64_machine.c:2758,2791,2818`, `t64_rom.c:172`) | refused |
| 4 | BAD_STORAGE (10 sites) | host allocator returned inconsistent storage at create/admit (`t64_machine.c:2766,2804,2836`, `t64_rom.c:184`) vs internal inconsistency while hashing/writing (`t64_digest.c:345,350`, `t64_state.c:169,214,221,335`) | AMBIG-B: one numeric value, two meanings. Recommend: keep BAD_STORAGE refused (allocator contract) and append a new `T64_STATUS_DEFECT`-style status (planner names it, value 34 or above) for the six internal sites, which poison; note `t64_machine_state_export` currently clears the first `total` buffer bytes on that status (header comment `:405-409`) |
| 5 | INVALID_ROM | `t64_rom.c:133,138,165,168`; `t64_machine_admit_rom(NULL, ...)` returns it for a NULL machine (`t64_machine.c:2935`) | refused (NULL-machine case should be INVALID_ARGUMENT: candidate fix, moves no digest) |
| 6 | UNSUPPORTED_ROM_ORDER | **never returned**; the only occurrence in the repo is the `#define` (`git grep`) | reserved/refused; the checker must allow a "reserved, unreachable" row |
| 7 | NOT_READY (31 sites) | no image / terminal CPU stop | refused |
| 8 | TIME_REVERSED | `advance_time_bound` (`t64_scheduler.c:419`), `t64_schedule_reserve` | refused. AMBIG-C: inside a dispatch it can follow earlier state changes (`t64_cp0_synchronize_guest_time` after events), see RESOURCE_LIMIT |
| 9 | BUFFER_TOO_SMALL | size-then-copy-out queries | refused (carries required length) |
| 10 | LIMIT_REVERSED | `advance_counter_bound` (`:473`) | refused |
| 11 | QUEUE_FULL | `t64_schedule_reserve` via `t64_machine_queue_input` | refused (host advances then retries) |
| 12 | SEQUENCE_EXHAUSTED | reserve (`:170-172`), flag set at commit (`:216`) | refused, permanent until reset (`t64_machine_reset_execution_state` clears `sequence_exhausted`, `t64_machine.c:2165`) |
| 13 | TICK_OVERFLOW | ai, pi, scheduler, sp | refused |
| 14 | RESOURCE_EXHAUSTED (56 sites) | caller-facing: container size ceiling; handler-internal: backpressure signal | refused at the API boundary; inside `dispatch_front` it is the backpressure signal, never returned by `advance` |
| 15-19 | BUS_UNMAPPED, BUS_UNSUPPORTED, BUS_ALIGNMENT, BUS_RANGE, BUS_DEVICE_REFUSED | `t64_machine_bus_read/write` are thin wrappers of `t64_bus_read/write` (`t64_bus.c:1004-1013`) | refused for address/width/alignment; **AMBIG-D**: BUS_DEVICE_REFUSED is documented to leave device state (PI latched length and sticky error, SI sticky DMA error, a trace record; `t64_machine.h:655-665`), so "refusal leaves observations bit-identical" is false for debug bus writes. Decide: exclude `t64_machine_bus_write` from the refusal-no-op domain with a written carve-out, or class BUS_DEVICE_REFUSED as "completed with device error" |
| 20-25 | SAVE_UNKNOWN_DEVICE, SAVE_UNSUPPORTED_DEVICE, SAVE_DEVICE_MISMATCH, SAVE_BAD_FORMAT, SAVE_SIZE_MISMATCH, SAVE_BUSY | `t64_save.c`; documented exact check order R0-R9 (`t64_machine.h:597-626`) | refused |
| 26-33 | STATE_FORMAT, STATE_INTEGRITY, STATE_VERSION, STATE_PROFILE, STATE_CONTENT, STATE_BUILD, STATE_RANGE, STATE_INVARIANT | `t64_state.c`; header states a refusal leaves the machine bit-identical (`:437-443`) | refused (STATE_INVARIANT is a blob fault, not an instance defect) |
| stop 1 | BOUND | `advance_time_bound` end (`t64_scheduler.c:463`) | guest-stop, resumable |
| stop 2, 3 | INSTRUCTION_COUNT, FRAME_EVENT_COUNT | `reached_reason` (`:407-411`) | guest-stop, resumable |
| stop 4 | HALT | `machine->halted` is set only by the synthetic halt event (`t64_scheduler.c:50`, scheduled through `t64_test_*` hooks) | guest-stop, terminal until reset; **unreachable through the installed public API** (exercise in-tree) |
| stop 5 | QUIESCENT | counter bound with empty event queue (`:493-496`) | guest-stop, resumable only by queued input |
| stop 6 | RESOURCE_LIMIT | per-advance event budget (`:432-436,498-501`), `event_can_dispatch` false for VI/AI output backpressure or counter overflow (`:361-367,437-441,503-507`), **and any non-OK handler status** (`:444-451,510-517`: `machine->cpu.stopped ? cpu_stop_result_reason : RESOURCE_LIMIT`) | **AMBIG-E** (contradicts the locked "resumable backpressure" wording): three causes plus masked handler errors. A counter-overflow stall never resumes ("stalled resource limit" is one of the sweep's stop kinds, 18 lines). Recommend the table row say "resumable after drain or yield; a stall with no progress after drain is permanent", and the poison hook intercept the handler-error path only for statuses outside a whitelist, proven by zero new poisons across the 1,599-line sweep |
| stop 7 | CPU_UNSUPPORTED | `t64_cpu.c:1333`, `t64_scheduler.c:394-398` | terminal guest-stop (per CONTEXT) |
| stop 8 | GUEST_COMPLETION | `t64_cpu.c:1348` | terminal guest-stop |

**Reset/unload/poison today.** Public functions with no reset/unload/poison equivalent: all 30. Replacement is the only reset: `t64_machine_admit_rom` installs a candidate image, then `reset_guest_state` (`t64_machine.c:2292-2349`) clears RAM, RDP coverage and depth tables, SP memories, `cpu`, `rsp` and its observation fields, `boot`, `cache`, `guest_devices`, `output` (ISViewer), `output_drain_cursor`, `mi_*`, RDP/VI/AI/SI/save device state, the trace, then `prepare_openipl3_warm_entry` or `prepare_openipl3_vr4300_entry`. `t64_machine_reset_execution_state` (`:2154-2166`) zeroes tick, counters, sticky error, `halted`, the scheduler queue and sequence state. A public `reset` is `reset_guest_state` on the already-admitted image (digest after reset must equal digest after first admit; assert that). Not cleared (preserved by design): allocator, machine storage, profile, profile digest, content digest, header, `write_generation` restarts at 0 on replacement. Public `unload` must additionally release `image_storage` through the allocator, clear `has_image`, and every public entry already tests `has_image` (`t64_machine_advance`, content_digest, rom_size, rom_header, cpu_stop_detail, queue_input) so NOT_READY after unload falls out; audit the rest (`video_info`, `audio_info`, `save_info`, `output_drain`, `bus_*`, `trace_*`) in one table test. `admit_rom` is transactional until the image swap, but `reset_guest_state` runs after the swap and returns `t64_cpu_schedule`'s status (`:2978-2985`): a failure there leaves a replaced image with partial state, which is a defect-class path.
Poison without touching digest/state bytes: yes. Digest walkers and the container writer emit named members, never `struct` bytes (`tools/common/t64_sha256.h` comment, `machine-contract.md` member audit), so a new `poisoned`/`in_call` member is outside both; document it as an excluded member in the audit table. Entry guard: one check at the top of each of the 24 non-destroy entries that take a machine (30 public functions minus `t64_build_identity`, `t64_source_revision`, `t64_profile_lookup/hash/serialize`, `t64_machine_create` and `t64_machine_destroy`); `t64_machine_destroy` (`:2902-2925`) reads only `machine->allocator` and storage descriptors, so it works on a poisoned instance.

## Public API shape plan (item 12: blast radius)

| Surface | Count | Consequence |
|---------|-------|-------------|
| `t64_machine_create(` | 155 lines, 74 files (e.g. `test_lifecycle.c` 13, `test_si_profile.c` 10) | signature change to create-info is a mechanical edit across tests, runner, probes, sweep printer |
| `t64_machine_advance(` | 273 lines, 61 files; 145 `t64_stop_request x = {...}` initializer lines (mostly positional, e.g. `t64_stop_request request = {T64_STOP_GUEST_TICK, tick};`) | a leading `struct_size` turns each positional initializer into a missing-field error under `-Wextra -Werror` (`-Wmissing-field-initializers`); runner uses a compound literal at `runner/t64_real.c:1711` |
| `t64_host_allocator` | 206 lines, 73 files | `{test_allocate, test_release, &ctx}` positional initializers become pointer-to-int errors |
| `t64_profile_descriptor` | 303 lines, 79 files (tests copy a looked-up descriptor and edit fields) | getters would break every such test; adding a leading `struct_size` is the cheaper freeze (recommend; ASSUMED) |
| `t64_video_packet_info` / `t64_audio_packet_info` | 85 / 29 lines | fixed `char build_id[128]` strings stay |

Recommendation (ASSUMED benefit): ship initializer macros in the public header (e.g. `T64_STOP_REQUEST_INIT(kind, value)`, `T64_HOST_ALLOCATOR_INIT(alloc, release, ctx)`) so the codemod is a regex rewrite and future callers do not hand-write `struct_size`. Call it out in the freeze record.
Struct-by-struct judgement calls the planner must settle: `t64_trace_record` is an array element (a leading size per element does not work; use a size parameter or a request struct), `t64_host_storage` is passed by value to the release callback (exempt with a layout assert), the 7 `size_t` struct members and 19 `size_t` parameters move to `uint32_t`/`uint64_t` (a ROM or state container size can exceed 32 bits on paper; the profile caps are small, so use `uint64_t` for lengths and counts and `uint32_t` only for fixed-capacity fields).

## API-04 object check (item 6, run)

- **Commands run (read-only against repo, scratch members):** `ar x libt64_core.a` into a scratch dir (87 files), then `sh tools/lint/lint-objects.sh --objects members/*.o` (3.1 s). Because all members go to one invocation the tool already computes "undefined minus defined across the set" (`lint-objects.sh` `def.all` merge), i.e. the archive-level computation; no new set arithmetic is needed [VERIFIED: read script and ran].
- **Result after the sha256 fold:** 26 `L2-UNDEF-SYMBOL` lines, only `__stack_chk_fail` and `__stack_chk_guard` (13 members each); appending `__stack_chk_fail|compiler runtime stack protector abort only|ADR-0006 amendment|any` and `__stack_chk_guard|...|any` to a *scratch copy* of `allowlist.txt` gave exit 0. `bzero` (Darwin) and `mem*` are already allowlisted; before the fold the three `t64_sha256_*` imports would also fail [VERIFIED].
- **Linux (act image, gcc 13.3, default Ubuntu hardening, `-O3 -DNDEBUG`, x86_64 emulated):** archive-level undefined = `_GLOBAL_OFFSET_TABLE_`, `__stack_chk_fail`, `memcmp`, `memcpy`, `memset`; `__stack_chk_guard` is not imported on x86_64 (TLS canary), no `__memcpy_chk`. The ELF allowlist needs `_GLOBAL_OFFSET_TABLE_` (linker pseudo-symbol, PIC objects) and, for aarch64 Linux, `__stack_chk_guard` [VERIFIED for x86_64; aarch64 ASSUMED]. Use OS-keyed rows (the allowlist's fourth column).
- **Seeded violation:** an object importing `clock_gettime`, `fopen`, `getentropy`, `pthread_create` (with the allowlist amended) exits 1 with four `L2-UNDEF-SYMBOL` lines [VERIFIED]. Reuse `tools/lint/selftest` seeds for the permanent control and add the 4-symbol object as `tools/lint/selftest/bad_installed_*.c` plus a script that compiles it (`-fno-stack-protector`) and appends it to the extracted members.
- **Disassembly scan:** `objdump -d --no-show-raw-insn` over the 87 members took 0.9 s, no hits for `mrs xN, CNTVCT/CNTPCT/RNDR`, `rdtsc[p]`, `rdrand`, `rdseed`, `rdpmc`; a seeded `mrs x0, cntvct_el0` object printed `mrs x0, CNTVCT_EL0` [VERIFIED]. Linux GNU objdump accepts the same flags [ASSUMED].
- **Header include scan:** reuse the L1-INCLUDE allowlist in `tools/lint/rules.txt` over `$PREFIX/include/**` (currently `stdbool`, `stddef`, `stdint`); expect the allowlist to stay that small (no `<string.h>`; `memcpy` lives in `.c`).
- **Flags:** shipped build is `-O3` with the platform default stack protector and PIC; lint compile is `-O2 -fno-stack-protector -fno-pic`, so only the extracted-member run sees what ships. Recommended handling per CONTEXT: keep the shipped build hardened, allowlist exactly those names via an ADR-0006 amendment, keep the lint's own compile unchanged. One installed-archive allowlist row set per OS; `abi.installed-objects` runs lint on members, then asserts the post-allowlist undefined set equals the recorded expected set (so a new import fails even if allowlisted names change).

## Call-sequence generator and host-shaped loop (item 7)

- **Observations that exist (read-only):** `t64_machine_digest`, `t64_machine_content_digest`, `t64_machine_state_export` (composite family only; contains observer cursors and ring occupancy, about 10.5 MB for an idle v26 machine per PIN-IMPACT 2.3), `t64_video_info`, `t64_audio_info` (oldest packet; no dequeue), `t64_machine_save_info`, `t64_machine_cpu_stop_detail`, `t64_machine_rom_size`, `t64_machine_rom_header`, the new `t64_machine_output_status` (per-ring queued count, free capacity, next packet size) and the consumer's own allocator counters. Mutating "observers" (`output_drain`, `trace_drain`, `video_copy`, `audio_copy`, bus access) are operations, not snapshot members.
- **Cost:** a state hash is 6 to 10 MB; PIN-IMPACT records about 22x under ASan, so budget snapshots (digest + export hash + output_status + info structs), not per-call export, in sanitizer runs; native generator of a few hundred calls fits seconds, sanitizer runs are local.
- **Minimal deterministic grammar:** LCG with a constant seed in the consumer (a test host may own a PRNG; the core owns none). Operations: `admit(valid|NULL|zero-length|wrong-size|candidate-overlap)`, `advance(kind in {tick, instructions, frames}, value in {reverse, equal, small, large}, NULL result)`, `queue_input(past, future, bad port, bad axis, fill-to-QUEUE_FULL)`, `video_copy/audio_copy(NULL, zero, small, exact)`, `output_drain(small, exact)`, `save_export/import(wrong kind/format/size, busy window)`, `state_import(truncated, flipped byte, valid)`, `reset`, `unload` then `admit`, `output_status`, and one terminal defect trigger. Per step: snapshot, call, classify the returned status with the public class function/table, then: refused => snapshot bit-equal; ok => no assertion; stop reason guest-stop => the next advance either makes progress or the table says terminal; defect => every later call except `destroy` and the poison query returns the poisoned status and outputs stay untouched; finally `destroy` and equal allocator counts.
- **Defect trigger without extra ABI:** the consumer owns the allocator storage, and the instance keeps `profile_digest[32]` while `admit_rom` re-hashes the profile against it (`t64_machine.c:2937-2944`, today answered INVALID_ARGUMENT). The consumer can compute the same digest with the public `t64_profile_hash`, search its instance storage for those 32 bytes, flip one bit, and call `admit_rom`; the planned classification (AMBIG-A) turns that into a defect and poison. This exercises a real invariant check through the shipped library with no test-only symbol (ASSUMED feasible; a `t64i_test_*` hook in `t64_internal.h` for the in-tree white-box case follows the existing four-hook precedent).
- **Host-shaped loop:** admit the composite ROM, queue the two input events, advance in bounded slices (the runner's replay slice is 16 ticks; use a few thousand ticks for speed), draining video then audio then ISViewer text after each advance in host order; deliberately skip draining until `T64_STOP_REASON_RESOURCE_LIMIT` appears (the composite emits three video packets into a two-packet ring), then drain and resume; run a second instance interleaved on the same ROM and compare both final digests with the single-instance run and `97f270...`; vary drain cadence (every slice, every 3rd, only on stop) and assert equal digests. `GUEST_COMPLETION` ends the run.
- **Sanitizers (VERIFIED on this machine):** Apple clang accepts `-fsanitize=address,undefined` and `-fsanitize=thread` and both ran a test program. `build/asan` exists from `CMakePresets.json` (`asan` preset: Debug, `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`). Cheapest sanitized install: `cmake --preset asan && cmake --build --preset asan --target t64_core && cmake --install build/asan --component Development --prefix <tmp>`, then configure the consumer with `-DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all"`; library and consumer are both instrumented. Hosted CI has no sanitizer lane today; keep these local and in the qualifier. TSan: two instances on two threads, one thread each, no shared state, local only; Apple's TSan ran a two-thread test here [VERIFIED]; the core's lint forbids writable globals so the result is expected clean [ASSUMED].

## Freeze-candidate record (API-03)

Bind four generated, checked-in goldens under `docs/abi/` (path is a planner choice): header hash (SHA-256 of installed headers canonicalized with LF endings), public symbol list (the `nm` output above), struct-layout table (`_Static_assert` file with `sizeof`/`offsetof`/constants compiled against source headers then installed headers, C99 pedantic and C++17), and the status-class table. A single `abi.freeze-record` CTest recomputes all four and fails on any difference with a message naming the re-bind command; a re-bind is a commit that edits goldens and a one-line amendment record. Additive rule text goes in a new `docs/spec/api.md`: new trailing struct fields guarded by `struct_size`, new statuses appended after the last value, new profiles/devices/error subcodes additive; stop-reason and status numbers never reused. Cheap self-contained-header check (each installed header compiles alone with `-std=c99 -pedantic -Werror` and as C++17) requires the `extern "C"` guards and `<stdbool.h>` removal of `bool` from the public surface.

## Prior art re-check (item 9)

| Claim | Result |
|-------|--------|
| mupen64plus compares only the upper 16 bits of the API version | `if ((APIVersion & 0xffff0000) != (FRONTEND_API_VERSION & 0xffff0000))` in `CoreStartup`, returns `M64ERR_INCOMPATIBLE` [CITED: raw.githubusercontent.com/mupen64plus/mupen64plus-core/master/src/api/frontend.c] (resolves the lens's second-hand item) |
| libretro `RETRO_API_VERSION` | `#define RETRO_API_VERSION 1`; "only incremented for incompatible API/ABI changes ... A design goal of libretro is to avoid having to increase this value at all costs" [CITED: raw.githubusercontent.com/libretro/RetroArch/master/libretro-common/include/libretro.h, first 100,000 characters; `retro_api_version()`, `retro_serialize_size` and unload/reset comments were not reached] |
| SDL3 stable-ABI preview, Vulkan loader negotiation, SQLite `sqlite3_libversion_number` | [CITED: lens-api.md section 1 (2026-11-23), URLs there; not re-fetched this session] |
| OpenXR negotiation | unverified in the lens and not decision-bearing; dropped |

Lesson usable now: a 0.x minor-equals-compat policy matches mupen64plus (major mask) and is what `SameMinorVersion` plus `T64_ABI_VERSION_MAJOR 0`/minor increments encodes; do not copy libretro's "never bump" rule for a struct-and-function surface.

## LibFuturo paper read (item 8, API-07 input)

**Source and pin.** Web search "LibFuturo emulator core API contract" returned no page for it (results: retro-rs, rust-libretro, other cores) [VERIFIED: WebSearch]. I did not try repository-host URLs guessed from the owner's handle (privacy). The primary source is the sibling checkout the sibling libfuturo checkout (path omitted), read at **commit `4177a43e67312e2db88ab21cba13aa7350063105`** (2026-11-23, "docs: finalize browser-host v2.4 state"; the working tree is dirty with 12 uncommitted lines in `include/libfuturo/libfuturo.h`, so I read the committed objects with `git show 4177a43:include/libfuturo/libfuturo.h` (347 lines) and `git show 4177a43:docs/contract.md` (430 lines)). The earlier planning reference `a28cccd` (2026-11-09) is 550 commits older; the pin for API-07 should be taken again in Phase 12. No public URL was found, so the evaluation pin is a local SHA (UNVERIFIED as published).

| Aspect | LibFuturo at 4177a43 | Relation to CONTEXT decisions |
|--------|----------------------|-------------------------------|
| Instance model | `lf_create(profile_id, options, &out)` / `lf_destroy(&owned)` (consumes and clears the handle); host owns and serializes the handle (LC-LIFE-01); allocator callbacks `void *allocate(ctx, bytes)` | Compatible; ours adds alignment and storage descriptor. No conflict |
| Admission | `lf_load(content, configuration_id)`; borrowed span, rejected load keeps the previous session, unloaded stays unloaded (LC-LOAD-01) | Matches our transactional admit; LibFuturo has no unload/reset call |
| Errors | `enum lf_status` 6 values (`LF_OK`..`LF_RESOURCE_EXHAUSTED`), no classes; LC-ERROR-01: callers "MUST use the operation's documented postcondition rather than infer rollback from the status name" | Our three-class table is stricter; adapter maps refused to the nearest value; **no equivalent of defect/poison** (adapter must report poison as a distinct failure) |
| ABI hygiene | `size_t struct_size` first in each struct; enums for statuses and stop reasons; `extern "C"` present; explicitly no stable ABI ("rebuild against the matching header") | Our locked `uint32_t struct_size`, `uint32_t` status and stop constants differ from `size_t`/enums (adapter converts; no change to our decision) |
| Timing | `work_units` per profile (frame, source tick, synthetic step); input sampled only at advance start (`LF_INPUT_AT_ADVANCE_START`); stop reasons `BUDGET_REACHED`, `REJECTED`, `CORE_HALTED`; cadence `0/0` allowed | Our timed input stamped to guest ticks cannot be expressed; adapter is lossy by design (CONTEXT: may not shape the core) |
| Output | caller-provided destination spans inside `lf_run_request`; formats GRAY8/RGB24/INDEX8 only, one audio rate | Our copy-out rings convert at the adapter; N64 RGBA8 not representable (as ARCHITECTURE.md recorded) |
| Media/snapshot | `lf_copy_media`/`lf_import_media` with `guest_generation`; snapshot size/capture/restore with identity limits | Ours: `save_export/import`, `write_generation`, state export/import bound to build identity |
| Threading | external serialization, no concurrency promise | Same as our decided statement |

Conclusion for API-07: nothing in the LibFuturo contract requires amending a locked decision; the expected no-adapter-now conclusion holds. Two facts to carry into the Phase 12 text: LibFuturo promises no stable ABI itself, and it has no defect class, so an adapter has no failure shape for poison.

## Architecture Patterns

### System Architecture Diagram
```
 composite.input.json literals ---------------+
 composite.expected.json (independent) -------+--> installed consumer (tests/installed-consumer)
                                               |        |  find_package(tendies64 0.1)
 cmake --install (build tree, Development) --> prefix/{include,lib,lib/cmake}   [relocatable, spaces ok]
        ^                                              |
 t64_core (sha256 folded, t64i_ prefixed) --------------+--> t64_abi_version()/build id assert
        |                                                      create(info) -> admit -> queue_input
        +--> nm golden, header hash, layout asserts             -> advance(bound) -> {BOUND|RESOURCE_LIMIT|COMPLETION}
        +--> ar x -> lint-objects --objects (+allowlist)        -> drain video/audio/isviewer -> save_export -> digest
        +--> objdump clock/entropy scan, seeded objects         refused? snapshot equal : guest-stop? resume : defect? poisoned
 in-tree runner (t64_real) ----> same ROM, same literals (reference, not re-pointed)   => three-way agreement
```

### Recommended structure
```
core/include/tendies64/t64_machine.h  (+ optional t64_abi.h)   public, installed
core/t64_prefix.h                      generated rename header (nm-derived, regenerate-and-diff test)
docs/abi/{public-symbols.txt,layout.c,status-classes.txt,header.sha256}   freeze-candidate goldens
docs/spec/api.md                       postconditions, class table, additive rule, threading
tests/installed-consumer/{CMakeLists.txt,main.c,hostloop.c,sequence.c}    separate top-level project, not add_subdirectory'd
scripts/{preflight-phase11.sh,qualify-phase11.py,check-abi.sh}            thin delta of the Phase 10 templates
```
The root `CMakeLists.txt` auto-adds `tools/*/CMakeLists.txt` and `tests/controls`; do **not** place the consumer under `tools/` and do not `add_subdirectory` it; CTest drives it with `cmake -S tests/installed-consumer -B <tmp>` [VERIFIED: root CMakeLists glob].

### Pattern: struct_size prologue
**What:** every entry validates `struct_size >= V1_MIN`, copies `min(caller, known)`, refuses otherwise before any state change. **Example (own design, ASSUMED):**
```c
static t64_status take_stop_request(const t64_stop_request *in, t64_stop_request *local) {
  if (in == NULL || in->struct_size < T64_STOP_REQUEST_V1_SIZE) return T64_STATUS_INVALID_ARGUMENT;
  memset(local, 0, sizeof *local);
  memcpy(local, in, in->struct_size < sizeof *local ? in->struct_size : sizeof *local);
  return T64_STATUS_OK;
}
```

### Anti-patterns
- Re-pointing the runner at the installed package (loses independence and runner-keyed evidence).
- Poisoning on a caller fault, or inside the existing handler-error path without a whitelist (would turn today's stalled-resource stops into poisoned instances and break the sweep).
- Hand-writing a symbol list: generate it from `nm`.
- A nested `ctest` inside a CTest (F-113 precedent); drive `cmake`/`ninja` and the consumer binary directly.

## Don't Hand-Roll

| Problem | Don't build | Use instead | Why |
|---------|-------------|-------------|-----|
| Relocatable package | custom find script | `install(EXPORT)` + `configure_package_config_file` + `write_basic_package_version_file` | verified above |
| Archive-level undefined set | new nm arithmetic | `lint-objects.sh --objects` over all members | already does set subtraction and section/COMMON/ctor checks |
| Digest/printer comparison | new differ | `scripts/digest-sweep.py` with a printer shim | 1,599-line baseline exists |
| Qualifier | new framework | copy `scripts/qualify-phase10.py` pattern (base records, tool paths, stage table) | Phase 8-10 precedent |
| ABI diff tools | abidiff, abi-compliance-checker | three goldens | rejected by CONTEXT; ~30-function C API |
| Symbol hiding | `objcopy`/`ld -r` | rename header | no `objcopy` on macOS |

## Common Pitfalls

1. **Oracle-bound corpus edit.** 9 of the 10 C files in `EXPECTED_CORPUS_PATHS` use the changing API; an unplanned edit trips `harness.rdp-oracle-authority`. The only recorded approvals are for manifest rows (STATE.md lines 143, 150; superseded Phase 9 grant). **How to avoid:** make the batched identity-only rebind an explicit orchestrator-only task and put the approval question first (see Open Questions). Warning sign: `authored corpus SHA-256 changed`.
2. **Silent collision.** Control that only checks "link succeeds" passes today while the core calls the consumer's `f32_add`. Assert results, show it failing pre-prefix once.
3. **`#define` rename vs SoftFloat guards** (nine symbols vanish; link errors in tests).
4. **Positional initializers.** `{T64_STOP_GUEST_TICK, tick}` and allocator initializers compile or half-compile after `struct_size` is prepended; `-Werror` catches most, designated or compound-literal forms hide mistakes. Use init macros plus a grep guard.
5. **Handler errors folded into RESOURCE_LIMIT** (`t64_scheduler.c:444-451`): poison placement must not change sweep output.
6. **Sealed public observers** (`t64_audio_*`, `t64_video_*`): entry guards inside them re-pin sealed bodies; prefer one entry layer or an isolated derived refresh (Phase 9 plan 09-01 precedent). Verify with `check-rsp-no-hle.py` first.
7. **Linux allowlist gap:** `_GLOBAL_OFFSET_TABLE_` on ELF PIC objects; `__stack_chk_guard` on aarch64.
8. **`act` has no cmake/ninja:** Linux install path cannot be reproduced locally; budget one hosted cycle.
9. **G4 growth** (below) and `check-test-inventory.sh` exact lists.
10. **Stale-text grep is too narrow:** passing the stated grep leaves `t64_facade.h:6`, the banners and ADR-0002 lines 55 and 91 stale; add a broader check (`git grep -n -E 'Phase 10' -- core runner tools` with an allowlist for the Phase 10 composite names).
11. **Docs count error:** the "20 stale docs mentions" were already swept; do not plan a docs relabel wave beyond ADR-0002, `docs/spec` additions and the new `api.md`.
12. **`t64_core_identity`** is an unprefixed symbol used by the runner supervisor; decide before the golden is generated.

## State of the Art

| Old | Current | Impact |
|-----|---------|--------|
| Private link of a helper static lib | fold or export every dependency in the export set | `install(EXPORT)` fails otherwise (lens, reproduced there; scratch generate passes after fold) |
| Visibility for archive hygiene | prefix/rename | verified above |
| Hand-written ABI checks | generated goldens | CONTEXT decision |

Deprecated: unprefixed SoftFloat names and unprefixed `t64_*` internals in the archive; the "PROVISIONAL" banners of both headers.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | A batched identity-only oracle-request rebind for edits of the nine bound test files is within the existing owner approvals (precedent 15fd8d1, STATE.md line 209 "same scope"), or the owner will approve it | Pin-impact, Pitfall 1 | Owner stop (AUTONOMY section 4 "any new action outside delegated scope that materially changes authority") blocks the shape wave |
| A2 | Entry guards can be added without re-pinning sealed bodies by a thin public entry layer or one isolated derived refresh | Pitfall 6 | Extra re-pin plan, more risk to Phase 8/9 pins |
| A3 | Linux `cmake --install`/consumer flow equals macOS | CMake recipe | One hosted failure cycle |
| A4 | aarch64 Linux needs `__stack_chk_guard`; `objdump --no-show-raw-insn` exists in GNU binutils | API-04 | Allowlist or scan fails on that OS |
| A5 | Corruption-based defect trigger (flip the stored profile digest) works black-box | Generator | Fall back to an in-tree `t64i_test_*` hook plus an in-tree-only poison test |
| A6 | `struct_size` on the profile descriptor is cheaper than getters | API plan | Re-plan if getters are required by Phase 12 host |
| A7 | Plan count 14-16 and per-plan sizing (below) | Plan sizing | Plan cap overruns |
| A8 | Initializer macros are acceptable public API | API plan | Drop macros, codemod stays regex |
| A9 | TSan result is clean | Sanitizers | A real shared-state defect surfaces (a finding, not a plan break) |
| A10 | SDL3/Vulkan/SQLite prior-art statements from the lens still hold | Prior art | Wording only |

## Open Questions

1. **Is the oracle-request rebind for edited bound test files pre-approved?** Known: approvals cover manifest rows (STATE.md 143, 150, 209). Unclear: whole-file hashes of nine test sources. Recommendation: the planner's first plan records the question and the orchestrator resolves it before the shape wave; fallback is a compat shim only for those nine files (not recommended: positional initializers cannot be shimmed).
2. **AMBIG-A to E** (section "Status and stop-reason inventory"): BUS_DEVICE_REFUSED carve-out, BAD_STORAGE split with a new defect status, RESOURCE_LIMIT stall wording, NULL-machine statuses, dead status 6. Recommend deciding in the class-table plan with code evidence from the sweep.
3. **`t64_core_identity`:** keep unprefixed (golden annotation) or move the stub to the runner.
4. **Package version number:** `0.1.0` as the first ABI candidate (scratch used it); the planner picks the number and the bump rule (minor on any golden change).
5. **Hosted lane for the fuller fresh-archive variant:** a new workflow needs scanner rules (`CI-WORKFLOW-FILE`, `CI-STRESS-WORKFLOW`-style exact contract). Recommend a local script plus the qualifier only, no new workflow.
6. **Space-containing prefix and `lint-objects`:** run lint on members extracted into a space-free temp path; whether `ar x` from a spaced prefix works with the script's `$@` quoting was not tried.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|-------------|-----------|---------|----------|
| CMake, Ninja | package, consumer | yes | 4.4.3, 1.13.2 | none needed |
| Apple clang, `nm`, `ar`, `size`, `objdump` | build, golden, lint, scan | yes | clang 21, Apple LLVM | none |
| Docker + `act` image `ghcr.io/catthehacker/ubuntu:act-24.04` | Linux compile/nm/lint probe | yes (cc 13.3.0, GNU binutils) | image on disk | compile-only; no cmake/ninja inside |
| Hosted ubuntu-24.04 / macos-26 | install-from-build-tree on Linux | hosted only | n/a | one hosted cycle |
| `objcopy` / `llvm-objcopy` | alternative symbol hiding | no (macOS) | n/a | rename header |
| Pinned Binutils (fixtures) | composite ROM | yes, `build/tools/install` (`bin`, `lib`, `mips64-elf`) present locally [VERIFIED] | cached in CI | cold hosted build adds about 186 s |
| `abidiff` | rejected | no | n/a | goldens |

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | CTest on Ninja (CMake 3.25+), C tests with `tests/t64_test.h` (`T64_CHECK`, summary `name: checks=N failures=M`), Python/sh harness tests |
| Config file | `CMakePresets.json` (`gate`, `asan`), `core/CMakeLists.txt`, `runner/CMakeLists.txt`, `scripts/check-test-inventory.sh` |
| Quick run | `cmake --build --preset gate --target t64_core && ctest --preset gate -R '^(repo\.abi|install|installed|api04)\.'` (new names, about 10 s native) |
| Full suite | `./scripts/gate.sh` (254 names today; cold hosted gate-macos 731 s of 900 s) |
| Local-only | `cmake --preset asan` build + consumer with `-fsanitize=address,undefined`; TSan two-thread run; `python3 -B scripts/digest-sweep.py --base <40-hex> --head WORKTREE` (60 s) |

### Phase Requirements to Test Map (proposed names)
| Req | Behavior | Type | Command | Exists |
|-----|----------|------|---------|--------|
| API-01 | header self-contained C99 pedantic and C++17; layout `_Static_assert` vs source then installed headers | cheap | `ctest -R '^repo\.abi-header$'`, `'^repo\.abi-layout$'` | Wave 0 |
| API-01 | every operation accepted and refused case incl. reset/unload, struct_size, ABI mismatch | C unit (in-tree, white-box allowed) and installed | `ctest -R '^unit\.core\.api\.'`, `'^installed\.consumer\.ops$'` | Wave 0 |
| API-02 | status/stop table covers every constant once, unclassified fails; seeded unclassified constant fails | script | `ctest -R '^repo\.abi-status-classes$'` | Wave 0 |
| API-02 | refusal-no-op generator (native small N; sanitizer local large N), guest-stop resumes, defect poisons | consumer | `ctest -R '^installed\.consumer\.sequence$'`; local `asan` variant | Wave 0 |
| API-03 | freeze record recomputed (header hash, symbols, layout, classes); incompatible change fails | script | `ctest -R '^repo\.abi-freeze-record$'` + mutant row | Wave 0 |
| API-04 | installed-member lint with amended allowlist, expected undefined set, disassembly scan, header include scan, seeded objects (4 symbols + `mrs`) fail | script | `ctest -R '^api04\.'` | Wave 0 |
| VER-05 | install, relocate (spaces), consumer composite three-way, negative controls (missing, version, source-tree, doctored header, collision with results, seeded) | script + consumer | `ctest -R '^install\.'`, `'^installed\.consumer\.composite$'`, `'^installed\.controls\.'` | Wave 0 |
| VER-05 | host-shaped loop, two instances, drain-cadence independence | consumer | `ctest -R '^installed\.consumer\.hostloop$'` | Wave 0 |
| API-01 | `repo.consumer-include-guard`: grep that `tests/installed-consumer/**` includes only `tendies64/` headers | grep | `ctest -R '^repo\.consumer-include-guard$'` | Wave 0 |

### Sampling Rate
- Per task commit: `ctest --preset gate -R '<names of the touched area>'` plus `harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `check-rsp-no-hle.py`.
- Per wave merge: `./scripts/gate.sh` once after the shape wave; sweep against the recorded wave base; ASan run of the consumer locally.
- Phase gate: full gate green, hosted pair green, qualifier record.

### Wave 0 Gaps
- [ ] `scripts/preflight-phase11.sh` (copy of the Phase 10 script; act-image compile-only Linux probe, oracle-authority, inventory counts, TIMEOUT audit, PII, post-Gate replay; target under 90 s)
- [ ] printer shim in `tools/digest-sweep/t64_digest_sweep.c` and a recorded Phase 11 wave base file with 40-hex lines
- [ ] `PHASE11_TESTS` block and `ALL` list entry in `scripts/check-test-inventory.sh`
- [ ] `tests/installed-consumer/` project, `docs/abi/` goldens and generator, `docs/spec/api.md`
- [ ] `scripts/qualify-phase11.py` thin delta and `docs/claims/phase11.md`

### CI and time budget
- Cheap-first set native estimate: header compile (C99 and C++17) about 0.5 s, layout asserts 0.3 s, `nm` golden 0.1 s, lint-objects 3.1 s, objdump 0.9 s, install under 1 s, consumer configure+build+run about 3 s each, so about 10 to 15 s summed. Hosted multiplier 2.8 to 4.3 (PIN-IMPACT 6.1) gives 30 to 65 s of summed test time, about 8 to 16 s of 4-job wall.
- **G4 applies as written to Phase 11** (ROADMAP: "The exception covers Phase 10 only"): either offset equal time or record a Phase 11 G4 exception for owner review as Phase 10 did. Margin to the 900 s limit is 169 s (cold measured 731 s, PIN-IMPACT section 6 final rows). Everything heavy (sanitizers, TSan, fresh-archive variant, 3-way runner re-run with the long cap) stays in local or qualifier stages.
- No `ci.yml`, `check-ci-yml.sh` or `gate.sh` edit is needed if every new check is a CTest name; this avoids a second round of bound-source edits. `act` can pre-flight compile/`nm`/lint-objects on Linux but not cmake.

## Security Domain

### Applicable ASVS Categories
| Category | Applies | Control |
|----------|---------|---------|
| V2/V3/V4 auth, session, access control | no | library API, no identities |
| V5 input validation | yes | `struct_size` floor, NULL/overlap/span checks (existing), version handshake, hostile blob import (existing fuzz, keep running on shipped objects) |
| V6 cryptography | no new use | SHA-256 is an identity digest only (`t64_sha256.h`: "never a key, MAC or signature") |
| V14 build/supply chain | yes | no new dependency; doctored-header and symbol-collision controls |

### Known Threat Patterns
| Pattern | STRIDE | Mitigation |
|---------|--------|------------|
| Header/library skew (doctored header, wrong `struct_size`) | Tampering | `abi_version` + `struct_size` handshake at create, build-id compare, controls |
| Symbol interposition of SoftFloat/sha256 by a host | Tampering | `t64i_` prefix, results-asserting collision control |
| Reentrant call from allocator callback | Elevation/DoS | in-call flag, refusal |
| Use after destroy, concurrent calls on one instance | DoS | documented undefined behavior, TSan control for distinct instances |
| Hostile state blob | DoS | existing check order plus fuzz (`core/tests/fuzz_state_import.c`) |
| Host-visible clock/entropy leak into core | Information/Tampering | API-04 member scan, disassembly scan, seeded objects |

## Phase 10 carry-ins and Phase 9 items (item 11)

| Item | Location (verified) | Effort | Recommendation |
|------|---------------------|--------|----------------|
| W2-01 Z product bound | `core/t64_state.c:1490` `state_z_products_bounded`; parity case `core/tests/test_state_import_v26.c:1295`; two surviving mutants (limit `INT64_MAX / 2`, divisor `y_bound + 1`) | 1 plan (test + 2 mutant rows), no core edit | do: it is pin strength of a state path the candidate exposes; or defer with reason |
| W2-02 membership tokens | `scripts/list-membership-sites.py` (Phase 9 bound path) + `docs/spec/profile-membership-sites.txt` (6 rows, 3 details) | 20 min, one commit; run `harness.phase09-qualification` | do inside the shape wave (the list moves with edited bodies anyway) |
| DEF-W1-01 perspective floor | `core/t64_rdp_tmem.c:1218-1221`, `core/t64_rdp_raster.c:495,556-557` (sealed bodies; tests and mutants only, no core edit) | 1 plan | drop or defer (G3 count; no API effect) with the PIN-IMPACT reason |
| W1-03 `ceil` sentence | `docs/spec/vi.md:215` area | 5 min docs | do in the docs plan |
| D-10-17-01 99-attempt cap | `runner/tests/test_cold_boot.c:124-139` `make_evidence_dir` | 15 min plus preflight WARN | do (moves runner test identity only) |
| T1 split `unit.core.state.import` | pinned by `docs/spec/phase09-cases.json` digest | high | defer beyond Phase 11 (recorded cost fits the 15-minute limit) |
| T2 replay-driver duplication | `runner/t64_replay.c` host loop | 1 plan, reshaped by API change anyway | do only if the shape wave already touches the loop; else defer |
| T5 recorder failure names, T6 `T64_REAL_KEEP_WORKSET` (`runner/t64_supervisor.c:1140`), T7 third SI trace kind name (`T64_TRACE_DEVICE_SI`, `T64_TRACE_KIND_SI_*`, `core/t64_si.c:15-19`), T8 case-to-CTest checker | docs/spec/result-protocol.md, supervisor, header | each small, T7 is a public-name decision in the same wave | triage T7 in the shape wave (public constants); T5/T6 defer with the existing reason; T8 add with the Phase 11 case list |

## Risks and ordering hazards (item 12)

Plan sizing (estimate 14 to 16 plans; ASSUMED): 11-01 preflight, pin map, decisions, class inventory (no production edit); 11-02/03 shape types and handshake in header plus core (two plans); 11-04 poison/in-call/reset/unload with white-box tests; 11-05/06 mechanical call-site codemod split by directory (`core/tests`, then `runner`+`tools`+`tests`), isolated bound-file commit last; 11-07 prefix header, sha fold, nine SoftFloat edits, comment relabel and ADR amendments; 11-08 sweep, rebind, review in scratch clone; 11-09 install/export and cheap-first checks; 11-10 consumer composite and negative controls; 11-11 host loop, two instances; 11-12 generator and local sanitizer/TSan runs; 11-13 API-04 object checks; 11-14 freeze record, `api.md`, carry-ins; 11-15 LibFuturo doc and review; 11-16 qualifier, claims, hosted pair. The shape wave (02 to 08) is the ballooning risk; the codemod plans stay mechanical only if initializer macros land first.

Ordering constraints: class table and decisions (AMBIG-A to E, oracle rebind answer) before any production edit; one wave base recorded before the first edit; header shape and the call-site codemod must land together (a half-edited tree does not compile under `-Werror`, so commit only compiling states, header and codemod in one plan or behind a staged compat); prefix header generated only after the last production symbol is added (poison query, reset, unload, output_status, abi_version are new public functions and must be in the public list first); golden generation last; the sweep printer shim before the sweep.

Other hazards: an `abi_version`/`T64_API` header edit moves the runner build id as well; the cold hosted margin is 169 s; `scripts/check-docs.sh` and `scripts/check-provenance.sh` must pass after the nine vendored edits; do not run `ninja -t clean` or fixture rebuilds in the shared tree (AUTONOMY Phase 9 lessons); reviewers use `git archive` clones; prune/hide `build/gate` run directories (`06-05-cold-*-attempt-*` near 99).

### Contradictions between CONTEXT and the tree (reported, not overridden)
1. "20 stale 'Phase 10 freeze' mentions" in docs: docs were relabeled in Phase 10 (15 sites, gate `stale 0`); the 20 is the production-line count. The stated done-grep misses `t64_facade.h:6` and ADR-0002 lines 55 and 91.
2. "293 global symbols": measured 248 (30 + 170 + 48), 9 imports.
3. RESOURCE_LIMIT as always-resumable backpressure: it also covers permanent stalls and masked handler errors (AMBIG-E).
4. Refused means observations bit-identical: false for `BUS_DEVICE_REFUSED` debug writes and for BAD_STORAGE during export (AMBIG-B, D).
5. "Shape change before the consumer, with a before/after proof" does not mention that nine oracle-bound test files must be edited (Open Question 1) and that the sweep printer must compile against both API generations.
6. `t64_facade.h` "not installed" but its `t64_core_identity` is defined in the installed archive and used by the runner.
7. "No hosted timeout change" and G4 as written leave no recorded growth allowance for Phase 11; the Phase 10 exception does not extend.

## Sources

### Primary (HIGH)
- Repo files read this session: `core/include/tendies64/t64_machine.h`, `t64_facade.h`, `core/CMakeLists.txt`, `CMakeLists.txt`, `CMakePresets.json`, `tools/common/CMakeLists.txt`, `tools/lint/{lint-objects.sh,allowlist.txt}`, `core/t64_machine.c`, `core/t64_scheduler.c`, `core/t64_observe.c`, `scripts/{gate.sh,digest-sweep.py,check-rdp-oracle-authority.py,qualify-phase10.py,check-test-inventory.sh}`, `.github/workflows/ci.yml`, `.planning/phases/10-*/PIN-IMPACT.md`, `fixtures/composite/*`, `provenance/copies.json`
- Commands run: `nm`, `ar`, `size`, `objdump`, `cmake`, `ninja`, `docker run` on the act image (all listed above)
- LibFuturo checkout `4177a43e67312e2db88ab21cba13aa7350063105` header and `docs/contract.md` (local)

### Secondary (MEDIUM)
- https://raw.githubusercontent.com/mupen64plus/mupen64plus-core/master/src/api/frontend.c (version mask)
- https://raw.githubusercontent.com/libretro/RetroArch/master/libretro-common/include/libretro.h (RETRO_API_VERSION text)
- `.planning/research/v1.2-fanout/lens-api.md`, `SYNTHESIS.md` (dated 2026-11-23, facts re-checked above)

### Tertiary (LOW)
- Prior-art URLs in lens-api.md (SDL3, Vulkan loader, SQLite, CMake packaging) not re-fetched; CMake `GNUInstallDirs` multiarch note from the lens.

## Metadata

**Confidence breakdown:**
- Standard stack and install recipe: HIGH (run on macOS; Linux install unverified)
- Symbol surface and API-04: HIGH (run on both OSes for symbols/lint; aarch64 Linux assumed)
- Status classes: MEDIUM (code read, five ambiguities need decisions)
- Plan sizing and effort: MEDIUM-LOW (estimates)
- LibFuturo: MEDIUM (local primary source, no public URL)

**Research date:** 2026-11-25
**Valid until:** until any production edit lands (build id moves), otherwise 30 days
