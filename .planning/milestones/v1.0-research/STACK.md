# Technology Stack

**Project:** Tendies64 (deterministic, accuracy-first, headless-first Nintendo 64 emulator)
**Dimension:** Stack (language, build, test harness, ROM-fixture toolchain, CI, host layers)
**Researched:** 2026-11-09 (all "verified" facts below were read from live sources on this date)
**Overall confidence:** MEDIUM-HIGH. Tooling and fixture facts are verified from primary sources (HIGH). The language choice is a reasoned judgment with a named runner-up (MEDIUM). No benchmark or build of the emulator exists yet, so nothing here is "measured" for Tendies64 itself.

Evidence labels used: **[owner]** owner direction, **[primary]** primary-source fact (repo/API/docs read on 2026-11-09), **[community]** community reference, **[observed]** implementation observation (source code or file read), **[measured]** measured in this session, **[inference]**, **[recommend]**, **[unknown]**.

---

## 1. Decision Summary (prescriptive)

| Area | Decision | Confidence |
|------|----------|------------|
| Core language | **C17** (ISO/IEC 9899:2018), `CMAKE_C_EXTENSIONS OFF`, GCC + Clang as the supported compilers, MSVC as an advisory canary | MEDIUM |
| Core float policy | **Integer soft-float is canonical** for COP1. Host FP is only an optional, differentially-verified fast path. Never `libm`, never `-ffast-math`. | HIGH |
| Build system | **CMake, floor 3.25**, Ninja generator, `CMakePresets.json` with a `gate` **workflow preset** | HIGH |
| Package / install | `install(EXPORT)` + `find_package(CONFIG)` + a generated `.pc` file; static library first, hidden-visibility shared library second | HIGH |
| Unit/component tests | **CTest as runner + an in-repo single-header harness (about 200 lines, ours)**. Table-driven vectors in data files. No external test framework. | HIGH |
| Sanitizers / fuzzing | Clang ASan+UBSan lane; libFuzzer targets for ROM/media admission (ship with clang, no dependency) | HIGH |
| Fixture ROM #1 | `thelemmy/nemu64-test` pinned to a full commit, built with its own pinned Rust nightly + `nust64 0.4.1` in a **separate fixture lane**, never in the core build | HIGH (facts), MEDIUM (reproducibility, unmeasured) |
| Boot for fixtures | Libdragon's open IPL3 (r8 from `n64brew/libdragon`; note nemu64-test currently embeds r7 via nust64). No Nintendo PIF/IPL/CIC bytes anywhere. | HIGH |
| Own diagnostic ROMs | Two routes: (a) tiny hand-written MIPS III assembly assembled with LLVM (`clang --target=mips64`) + libdragon IPL3; (b) libdragon GCC 16.2 toolchain **built from pinned source** for C-level diagnostics | MEDIUM |
| PeterLemon/N64 | **Do not clone wholesale.** Cherry-pick individual `.asm` cases by commit+path, rebuild ourselves, and resolve the bundled-bootcode rights question first | HIGH (risk), MEDIUM (path) |
| Host/presentation (later) | SDL 3.4.x (zlib) in a separate `host/` target; core never links it | HIGH |
| RDP | Clean-room software reference renderer written in C inside the core tree; Vulkan compute (paraLLEl-RDP, MIT) is an optional, separate host-side backend | MEDIUM |
| CI | GitHub Actions, explicitly pinned runner labels, first-party actions pinned by SHA, one local command (`./scripts/gate.sh`) that the PR job calls verbatim | HIGH |

---

## 2. Language for the Canonical Core

### 2.1 Recommendation

**C17.** [recommend] Confidence: MEDIUM. It is the only option that satisfies all of: stable C ABI with zero shims, a flat toolchain-only dependency tree, trivial copy-with-provenance of reference code (every reference emulator core worth reading is C or C++), and LibFuturo compatibility (LibFuturo is itself a C17/CMake 3.25 project, see 2.4). Its weakness, undefined behavior and no memory safety on untrusted input, is real and is addressed by an enforced discipline (2.3) rather than waved away.

**Runner-up: Rust (stable, zero crates).** If the owner ranks memory safety on untrusted ROM input above C-ABI simplicity and portfolio coherence, Rust is the correct alternative and the cost of switching *before any code exists* is nil. After Phase 1 it is not. Treat this as a decision to confirm at roadmap time, not later.

### 2.2 Comparison

| Criterion | C17 | C++20/23 | Rust (stable 1.99.1) | Zig (0.17.1) |
|-----------|-----|----------|----------------------|--------------|
| UB-driven divergence | Real risk: signed overflow, shifts, aliasing, uninitialized reads, OOB, float-to-int out of range. Mitigated by discipline + UBSan/ASan CI | Same as C plus more footguns (templates, exceptions, ADL) | Integer overflow is defined (wrapping or panic, explicit `wrapping_*`), no UB in safe code, bounds-checked | Safety-checked in Debug/ReleaseSafe, defined wrap ops; language still changing |
| MIPS FPU semantics (rounding modes, FTZ, NaN) | Host FP gives no NaN guarantee; GCC `-ffp-contract` defaults to `fast` in GNU modes [primary]. **Use soft-float.** | Same as C | **Rust docs say arithmetic NaN sign/payload is "non-deterministically chosen" and can differ between runs** [primary]. Same conclusion: soft-float. No fenv access in stable Rust without asm | Strict float mode default, but pre-1.0 semantics churn |
| Stable C ABI for hosts / LibFuturo | Native. The header *is* the ABI | `extern "C"` wrapper layer required; C++ ABI unusable | `extern "C"` + hand-maintained header, or a code-gen dependency (cbindgen); installing a C-consumable package needs `cargo-c` or a custom install step [community] | Possible, but `@cImport` was removed in 0.17 and translate-c moved to an external package [primary] |
| Portability | Every target, every compiler; MSVC supports C17 | Broad | Tier-1 on mainstream hosts; embedded/console hosts vary | Excellent cross-compile, but pre-1.0 |
| JIT / SIMD headroom (later) | Full: intrinsics, GCC/Clang vector extensions, direct `mmap`/W^X control | Full | Intrinsics stable; **`std::simd` still nightly-only** (`portable_simd`, #86656, read 2026-11-09) [primary]; JIT needs `unsafe` | `@Vector` built in; pre-1.0 |
| Testability | Custom harness (small) + CTest + sanitizers + libFuzzer, all toolchain-provided | GoogleTest/Catch2 are heavy deps | Built-in `cargo test`; fuzzing needs nightly + libFuzzer | Built-in `zig test` |
| Tooling | gcc/clang/lldb/valgrind/sanitizers | same | cargo/clippy/miri (miri is nightly) | zig build; ZLS broken in 0.17 per release notes [primary] |
| Dependency footprint | Compiler + CMake + Ninja only | + libstdc++/libc++ ABI coupling | rustup + crates.io (zero crates is possible; build tool is an extra moving part, 6-week cadence) | Single binary, but **breaking changes every release** (0.15, 0.16 on 2026-04-13, 0.17.0 on 2026-10-01, 0.17.1 on 2026-11-02) |
| Fit with project preferences | Matches portfolio: sibling repos (libfuturo, nesturbator, muhtari, schwaystation) are all C17 + CMake 3.25 [observed] | Contradicts "small and flat" in spirit | Breaks portfolio coherence; strong on safety | Pre-1.0 contradicts "pinned, reproducible, long-lived" |

### 2.3 Required discipline when choosing C (enforce in CI, not in prose)

1. **No UB, by construction.** Arithmetic on guest registers uses unsigned types and small inline helpers (`t64_add32_overflow`, `t64_sext32`, `t64_sra64`). Shifts always masked. No pointer type-punning; load/store through `memcpy` or explicit byte composition. Fixed-width types only. `[recommend]`
2. **Float-to-int and int-to-float are never C casts for guest values.** C float-to-int conversion of an out-of-range value is UB, and CVT/TRUNC/ROUND/CEIL/FLOOR on out-of-range inputs is exactly what guest tests probe. Route through soft-float. `[recommend]`
3. **COP1 via integer soft-float (canonical).** Berkeley SoftFloat 3e is a candidate to copy with provenance (BSD-3-Clause-style text in `COPYING.txt`, project active, last push 2026-11-03) [primary], but it needs a VR4300-specific NaN/denormal "specialization" and the VR4300 unimplemented-operation rules from the processor manual. Whether to copy or hand-write a smaller subset is a Phase-2 call. Host FP may later be used as a fast path only after lockstep differential tests against soft-float, with `-ffp-contract=off`, no `-ffast-math`/`-Ofast`, SSE2/NEON only, and the rounding mode set explicitly. `[recommend]`
4. **No host leakage in the core.** The core must not call `time`, `clock`, `rand`, `getenv`, `fopen`, `printf`, `malloc` (use host-supplied allocator callbacks or caller-provided arenas), `libm`, or threading APIs. **Enforce with a link-level purity test:** link the core object set alone and fail CI if `nm -u` shows anything beyond a small allowlist (`memcpy`, `memmove`, `memset`, `memcmp`). `[recommend]`
5. **No mutable globals / instance isolation.** All state in an opaque `t64_machine` handle; a CI test creates two instances and interleaves them to prove independence (LibFuturo's contract states "context-shaped API does not prove instance isolation" as a hazard, EMULATOR-DNA). `[recommend]`
6. **Explicit serialization.** Never `fwrite(struct)`. Never bitfields for hardware registers (layout is implementation-defined); use masks/shifts. Explicit endianness for snapshots. `[recommend]`
7. **Compile flags:** `-std=c17 -Wall -Wextra -Wconversion -Wshadow -Wstrict-prototypes -Wvla -Werror`, plus `-ffp-contract=off`. Shipped artifact may add `-fwrapv -fno-strict-aliasing` as defense in depth; the **UBSan lane must run without `-fwrapv`** so overflow is still caught. `[recommend]`
8. **Sanitizer + fuzz lanes:** clang `-fsanitize=address,undefined` on the whole unit/component suite; libFuzzer harnesses for ROM header/size admission and save-media import (ROMs and saves are untrusted input per PROJECT.md Constraints). `[recommend]`

### 2.4 C17, not C23 or C11

- Ubuntu 24.04 runner image carries GCC 13.2 and Clang 18.1.3; Ubuntu 26.04 image carries GCC 15.2 and Clang 20.1.8/21.1.8/22.1.2 [primary, `actions/runner-images` READMEs]. C23 support is uneven across that range and absent in MSVC. C17 is a bug-fix revision of C11 with identical features, so there is no reason to pick C11 over it.
- LibFuturo (`~/projects/libfuturo`, Apache-2.0, last commit 2026-11-09 locally) sets `CMAKE_C_STANDARD 17` and `cmake_minimum_required(VERSION 3.25)`; nesturbator, muhtari, and schwaystation do the same [observed]. A C17 core can be consumed by a LibFuturo adapter as plain source with no language bridge. LibFuturo documents itself as an experimental C source API with no stable binary ABI promise [observed, `docs/contract.md`]; it has no N64 profile [owner/observed]. Adoption remains a later, separately decided integration.

### 2.5 What the language choice implies for the later JIT and the RSP

- **Interpreter is canonical; a JIT is an optional accelerator validated by lockstep differential against the interpreter.** ares v148 rewrote its N64 CPU/RSP JIT (2026-05-30) [owner/DNA]; ares ships `sljit` in `thirdparty/` [observed]. Do not add a JIT library until correctness baselines exist. `[recommend]`
- **RSP vector unit:** write the scalar 16-bit-lane reference first; add SSE2/NEON or GCC/Clang `vector_size` fast paths behind a compile-time switch, each proven bit-identical to the scalar reference by exhaustive/stress vectors (nemu64-test ships `vmulf`..`vmadn`, `vrcp32`, `vrsq32` stress features for this purpose; see 5.1). No `-march=native` in shipped builds. `[recommend]`

---

## 3. Build System and Package / Install Story

### 3.1 Recommendation

**CMake with a 3.25 floor**, Ninja generator, `CMakePresets.json`. Confidence: HIGH.

| Tool | Version to pin | Verified |
|------|----------------|----------|
| CMake floor | **3.25** (workflow presets arrive in 3.25) | Sibling repos use 3.25 [observed]; one sibling (gabbaboy) even keeps a `build-cmake-3.25.3` directory to keep the floor honest [observed] |
| CMake current | 4.4.x (latest release v4.4.5 on 2026-10-27; local 4.4.3; macOS-26 and Ubuntu-26.04 runner images ship 4.4.5; Ubuntu-24.04 image ships 3.31.6) | [primary] |
| Ninja | 1.13.2 (latest release 2025-11-20; on all runner images) | [primary] |
| Meson (not chosen) | 1.12.1 (2026-09-22) | [primary] |

Why CMake over the alternatives, in one line each:

- **Meson**: cleaner syntax, but adds a Python dependency to the root of the tree, has fewer downstream consumers than CMake, and gives nothing needed here that CMake presets do not. Not worth diverging from the portfolio.
- **Cargo**: only sensible if the core is Rust; C-consumable installed packages need `cargo-c` [community].
- **`zig build`**: Zig 0.17 reworked the build system again, with API breaks (`b.build_root` to `b.root`, `add*Arg` renames) [primary]. Pre-1.0 churn in the build path of a project whose selling point is pinned reproducibility is the wrong trade.
- **Plain Makefile**: fine for a prototype; no export/import of installed package metadata, no multi-config, weak Windows story. Keep a *thin* top-level `Makefile` or script only as a wrapper around `cmake --workflow`.

### 3.2 Layout implied (recommend)

```
CMakeLists.txt                  # project(tendies64 VERSION ... LANGUAGES C), C_STANDARD 17
CMakePresets.json               # configure/build/test presets + workflow preset "gate"
include/tendies64/tendies64.h   # the ONLY public header (opaque handles, no host types)
src/core/                       # canonical machine core: no host headers
src/runner/                     # headless runner (host layer #1), links the core like any consumer
host/                           # later: SDL frontends, never linked by core
tests/unit/                     # component tests + vector data
tests/protocol/                 # runner outcome/negative-control tests
tests/installed-consumer/       # independent CMake project + plain-cc project
fixtures/manifest/              # fixture manifest (source, rev, recipe, digest, profile, rights, expected-result provenance)
scripts/gate.sh                 # one command; CI calls this verbatim
```

### 3.3 Installed-consumer test (a PROJECT.md requirement)

Design [recommend]:

1. `cmake --install build --prefix "$TMP/prefix"` installs: `libtendies64.a` (and optionally a shared library with `-fvisibility=hidden` and an explicit `T64_API` export macro), `include/tendies64/tendies64.h`, `lib/cmake/tendies64/*.cmake` (via `install(EXPORT)` + `write_basic_package_version_file`), `lib/pkgconfig/tendies64.pc`, license and third-party-notices.
2. Consumer A: separate CMake project in `tests/installed-consumer/cmake/`, configured with `-DCMAKE_PREFIX_PATH=$TMP/prefix`, using only `find_package(tendies64 CONFIG REQUIRED)` + `tendies64::tendies64`. It runs the same headless fixture and result protocol as the in-tree runner (via the public API only).
3. Consumer B: no CMake at all: `cc consumer.c $(pkg-config --cflags --libs tendies64)` proves the `.pc` and header are self-sufficient and that no CMake-only coupling leaked.
4. **Relocation check:** move `$TMP/prefix` to a second path and rerun (LibFuturo runs a relocated-SDK check [observed]).
5. **Negative controls:** consumer must fail if the header is missing, if a symbol is hidden, or if a required fixture is absent; refused-content postconditions verified through the public API.
6. The installed consumer sees **no source tree and no build tree**; run it with `env -i`-style minimal environment plus a scrubbed `PATH` to catch accidental coupling.

ABI stance: **do not promise a binary ABI** until instance lifetime, profile selection, content admission/refusal, timed input, bounded advancement, raw output buffers, reset/unload, and failure postconditions are frozen (PROJECT.md Active requirement). Ship the **source-level** API first; version the header (`T64_API_VERSION`). Static library is the default consumption mode.

---

## 4. Unit and Component Test Approach

### 4.1 Recommendation

**CTest as the runner, an in-repo single-header harness as the framework.** Confidence: HIGH.

- Write `tests/harness/t64_test.h` (about 200 lines): `T64_CHECK_EQ_U64(actual, expected)` with file/line/expression and hex diff, a registration table, `--list`, `--filter`, a per-test guest-time/step bound, nonzero exit on any failure, and **a wrong-expected-value self-test that must fail** (negative control).
- **Vectors live in data files** (`tests/unit/vectors/*.txt`, one line per case: inputs, expected, provenance tag), parsed by a tiny reader. Expected values must be *independent* of the code under test: from the VR4300 manual, from nemu64-test's source semantics (it carries its own soft-float oracle in `src/math/soft_float.rs` [observed]), or hand-derived and reviewed. A test that recomputes its expected value with the same function is not a test.
- A single header copied with provenance is also acceptable if the owner prefers not to write it. Candidates verified 2026-11-09 [primary]: `sheredom/utest.h` (Unlicense, active), `mity/acutest` (MIT, single header, last push 2024-06), `silentbicycle/greatest` (ISC, last push 2023-06). My recommendation is still to own the harness, because the harness must encode the project's outcome taxonomy (pass, guest-fail, boot-fail, timeout, missing-fixture, harness-error) and negative controls; a third-party macro set will be bypassed for those anyway.
- Run tests via `add_test()` so `ctest --output-on-failure -j` is the single execution path in local and CI.

### 4.2 Test tiers (map to the verification ladder in EMULATOR-DNA)

| Tier | Where | In PR gate |
|------|-------|-----------|
| T0 pure unit: sign-extension helpers, soft-float, TLB lookup, address translation, KSEG mapping | `tests/unit` | Yes |
| T1 component: CPU exceptions/CP0, RSP scalar+vector vs reference vectors, PI/SI/MI registers, scheduler bounds | `tests/unit` | Yes |
| T2 runner protocol: outcome taxonomy, negative controls (wrong expected fails; missing/malformed/timed-out/crashed never passes as empty) | `tests/protocol` | Yes |
| T3 fixture ROM: pinned nemu64-test `base` via the headless runner | `fixtures` lane | Yes when fixture present; missing is an explicit failure in `--require-fixtures` mode |
| T4 installed consumer | `tests/installed-consumer` | Yes |
| T5 sanitizer + fuzz smoke | CI job | Yes (sanitizer), scheduled (long fuzz) |
| T6 timing/cycle/stress/RDP-experimental groups | scheduled | No, until justified |

---

## 5. Rights-Cleared Test and Diagnostic ROM Toolchain

### 5.1 `thelemmy/nemu64-test` (verified 2026-11-09, [primary] unless noted)

| Fact | Value |
|------|-------|
| Repo | https://github.com/thelemmy/nemu64-test, description "N64 test library", GitHub `created_at` 2026-07-15, `pushed_at` 2026-09-08, not archived, 2 open issues |
| HEAD at access | `9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c` (2026-09-08 17:23 UTC): "Clear the TLB after timing TLBWR/TLBWI so their duplicate entries don't shut down the TLB (#24)". **Pin a full SHA; the repository moves.** |
| Lineage | The old `lemmy-64/n64-systemtest` README says it is deprecated and points here; the Cargo package is still named `n64-systemtest` v3.0.0 [primary] |
| License | MIT. `LICENSE` file text is the standard MIT grant, "Copyright (c) 2021 lemmy-64". GitHub API reports SPDX `MIT`. (One automated README digest claimed "no license stated"; the LICENSE file read directly contradicts it.) This does **not** by itself clear the *generated ROM*, which also embeds libdragon's IPL3 (Unlicense) and compiled code from 7 crates (below) |
| Releases / packages | **None.** ROMs exist only as CI artifacts (`build-rom.yml` uploads `n64-systemtest-<variant>-z64`) |
| Build | `rust-toolchain.toml` pins **`nightly-2026-07-16`** with `rust-src`, `rustfmt`. `.cargo/config.toml`: custom target `mips-nintendo64-none.json`, `build-std = ["core","alloc"]`, `json-target-spec = true`, runner `nust64 --elf`. Prereq: `cargo +stable install nust64 --version 0.4.1 --locked`. Build: `cargo run --release` (optionally `--features cycle,timing`). Output: `target/mips-nintendo64-none/release/n64-systemtest.z64`. Release profile uses `lto = "fat"` |
| Crate dependencies | 7: `linked_list_allocator 0.10.5`, `spinning_top 0.2.5`, `arrayref 0.3.6`, `oorandom 11.1.3`, `arbitrary-int 2.1.1`, `bitbybit 2.0.0`, `num-traits 0.2.15`; `Cargo.lock` is committed |
| ROM builder | `nust64` 0.4.1: MIT, last published 2024-06-26, repo last pushed 2024-06-26 (stale but small). It **bundles three libdragon IPL3 binaries** (`ipl3_compat` 4096 B, `ipl3_dev` 11367 B, `ipl3_prod` 6336 B); default is the `release` (prod) one. Upstream CI comment: "Pinned: nust64 bundles the IPL3; an unpinned bump can break booting" |
| IPL3 revision actually embedded | **[measured]** md5 of nust64's bundled `ipl3_prod.z64` = `7f014ed41cc53bb1a5e44f211f8c3dec` = libdragon IPL3 **r7**. Libdragon trunk's `boot/bin/ipl3_prod.z64` (6344 B) = `419c213307cdb855934852c67759102b` = **r8** (matches the libdragon README changelog). So the fixture boots with an IPL3 one revision behind libdragon HEAD. Record the exact IPL3 md5/sha256 in the fixture manifest |
| Result reporting | Three-backend chain in `src/text_out.rs`: (1) **emux**: custom COP0 "CO" encodings (funct 0x20 detect, 0x25 log, 0x2c ioctl with codes `exit=1`, `fast=2`); (2) **ISViewer**: detect by writing `0x12345678` to the buffer at `0xB3FF0020` and reading it back; text is packed big-endian into the buffer in 0x200-byte chunks, then the byte length is written to `0xB3FF0014`, which flushes it; (3) **SC64 USB** (0xBFFF0000 range, key-unlock + ID read `0x53437632`); else silent. Output is also drawn on a framebuffer console |
| Summary format | **At HEAD the summary does not match the README.** README says "Done! Tests: 262. Failed: 0". The source (`src/tests/mod.rs`) prints `n64-systemtest <ver> (base=1 timing=0 cycle=0 cp0-hazards=0)` / `Finished in X.XXs. Base: Failed N of M tests (P% success rate)` per category, or `Done, but no tests were executed`. [observed] Pin the parser to the pinned commit and add a negative control (empty output, truncated output, "Failed 0 of 0") |
| Always-executed emux ops | `main()` executes `emux::xioctl_fast()` at start and `emux::xioctl_exit()` at the end **unconditionally** (no support check) [observed]. The core's CPU must execute those encodings without a fatal exception or hang. On unsupported emulators the ROM then spins in `loop {}`. The headless runner must therefore detect completion from the ISViewer stream, not from the exit op. **[unknown]**: real-hardware behavior of these encodings; test on our core on day one |
| Emulator tips (README) | Needs RI_SELECT nonzero to skip RDRAM init (normally 0x14); RSP DMA from RDRAM above 8 MiB reads zeros; RSP DMA from IMEM longer than 4 KiB wraps inside IMEM; missing LL/SC/DMFC0/DMTC0 can be stubbed temporarily to reach the end |
| Entry state | ROM sets FCSR with flush-denorm-to-zero and invalid-operation enable at entry; reads memory size, ELF offset and TV type from DMEM boot flags written by libdragon IPL3 |
| Test groups | 24 test modules under `src/tests/` (address_error_exception, arithmetic, cache, cart_memory, cop0, cop1, cop_unusable, endian_re, exception_instructions, jumps, mi, overflow_exception, pif_memory, privilege, rdp, rdram, rsp, rsp_timing, sp_memory, startup, timing, tlb, tlb64, traps) |
| Stable vs experimental (from `Cargo.toml` comments) | **Default/gated: `base`** (feature `quick`). **Opt-in:** `timing` (needs accurate time: NOP = 0.5 cycles, Random decrements per instruction), `cycle` (needs cycle-accurate emulator; "most likely wouldn't be implementable by a dynamic recompiler"), `cop0hazard`, `poorly_understood_quirk` (not enough understanding to implement; not even reported as disabled), **`experimental_rdp`** (filled-triangle RDP tests, "aren't stable on hardware yet"), `live_progress` (3x slower, "seen to wedge real hardware"), and 17 long-running stress features (`vmulf`..`vmadn`, `vrcp32`, `vrsq32`, `cop1`, plus `rcp_rsq_dump`). Upstream CI only builds `base` and `base-cycle-timing`, and does not run them in an emulator [observed] |

**Recommendation:** required CI runs `base` only. `timing`, `cycle`, `cop0hazard` become informational lanes until the core declares a timing model. `experimental_rdp` and `poorly_understood_quirk` never gate. Stress features run in a scheduled lane. [recommend]

**Build the fixture in a separate lane, not in the core build.** The fixture needs a Rust nightly, `rust-src`, and a custom JSON target; none of that should touch a C core's dependency tree. Rust nightly needs `#![feature(...)]` flags; the pinned nightly date is part of the fixture recipe. Cache the built ROM by `hash(commit, rust-toolchain.toml, nust64 version, features)`. **[unknown]: whether two clean builds on different machines are byte-identical.** The first fixture-phase task must build twice (different runners) and compare digests; if they differ, the manifest pins the artifact digest and the recipe is "informational".

### 5.2 Libdragon (verified 2026-11-09, [primary] unless noted)

| Fact | Value |
|------|-------|
| **Upstream status changed** | `DragonMinded/libdragon` was **archived by its owner on 2026-10-01** (GitHub banner "archived by the owner on Oct 1, 2026. It is now read-only"; `archived: true`; last push 2026-09-14). A search result links the archival to a contributor-policy dispute (issue #1012 "Explicit Anti-AI Contributor Stance"); treat that causal link as **[community, unverified]** |
| Continuation | `n64brew/libdragon`: not a GitHub fork, default branch `trunk`, branches `trunk`, `preview`, `unstable`, `gh-pages`, pushed 2026-10-05, 57 stars, Unlicense. HEAD at access: `98154dafd5ac50e74ee09a5de1035d24a53f54f0` (2026-10-05, "Change API reference link in README.md to libdragon.dev"); `preview` HEAD `f26e1c16145e4d6ad7e391e1306513d684083c02`. Downstream (N64FlashcartMenu issue #436, pinned 2026-11-01) is migrating paths during Oct 2026. **[unknown]: maintainers, governance, release cadence of `n64brew/libdragon`.** Treat as medium supply-chain risk |
| License | Unlicense (public-domain dedication; `LICENSE.md` read in full, no carve-outs listed in the file). Toolchain components are separate: GCC/binutils (GPL/LGPL with the GCC runtime exception for libgcc) and newlib (mixed permissive). **[unverified in this session]**; the fixture manifest must record per-component notices because a libdragon-built ROM links libdragon + newlib + libgcc |
| Toolchain | `tools/build-toolchain.sh` pins **binutils 2.45, GCC 16.2.0, newlib 4.4.0.20231231, GMP 6.3.0, MPC 1.3.1, MPFR 4.2.1**, target `mips64-elf`, install dir via `N64_INST`. Source build is 1+ hours, about 7 GB disk during, about 300 MB after (wiki: Installing libdragon) |
| Toolchain distribution | (1) Prebuilt `.deb`/`.rpm`/win64 `.zip` assets on the **rolling `toolchain-continuous-prerelease` release** (mutable tag, re-published 2026-10-06 as `V16.2.0-37360615130`; earlier 2023-01-26); (2) Docker via the third-party `libdragon` CLI (anacierdem/libdragon-docker; needs Docker + optionally Node); (3) source build. macOS Intel/Apple Silicon, Linux, Windows via WSL2/MSYS2 supported |
| Pinning consequence | The prerelease is **not an immutable pin**. For rights/reproducibility, build the toolchain from the pinned commit's `build-toolchain.sh` (versions pinned inside) and cache `$N64_INST` keyed by `hash(libdragon commit, script)`. Do **not** use the Docker CLI route in CI (an extra Docker+Node dependency chain) |
| Recent toolchain activity | 2026-09: "apply the VR4300 mulmul fix using the gcc specs", "build a more N64-relevant multilib toolchain" (relevant because miscompiled VR4300 `mul/mul` hazards are exactly the kind of thing diagnostics ROMs probe) |

#### Libdragon IPL3 (open boot code), current r8

- Source in `boot/` (`ipl3.c`, `rdram.c`, `loader.c`, `entropy.c`, `minidragon.[ch]`, linker scripts, trampolines); prebuilt `boot/bin/ipl3_{prod,dev,compat}.z64` of 6344 / 11575 / 4096 bytes; all **signed for CIC 6102** via a pre-signed trampoline; **r8 md5 for `ipl3_prod.z64` = `419c213307cdb855934852c67759102b`**, independently matching the README changelog **[measured]**. Cross-check every revision against the changelog md5.
- **What it covers:** the final boot phase: **RDRAM initialization/configuration**, clears all RDRAM, loads an appended ELF (scans ROM up to 64 MiB, 256-byte aligned), supports compressed ELF segments, sets `$sp` to end of RDRAM, clears MI_MASK, resets I/D caches, writes boot flags to DMEM (bytes 0..3 memory size; 4..7 32-bit entropy; 8 ROM type; 9 TV type PAL/NTSC/MPAL = 0/1/2; 10 reset type; 11 console type; 12..13 ELF offset in 256-byte pages), ignores the ROM header and checksum. `dev` adds logging over USB and **ISViewer** and an error screen. `compat` loads a flat 1 MiB binary from ROM `0x10001000` to the header's entrypoint and writes TV type/reset/memsize at `0x80000300/0x8000030C/0x80000318`, for legacy flat-binary homebrew.
- **What it does not cover (profile must say so):** Nintendo's IPL3 behavior (so any title or test relying on the original's side effects), other CIC variants (6101/6103/6105/6106 seeds), the **PIF handshake/CIC challenge and the PIF boot ROM itself** (that stage is a rights stop, replaced by an HLE or clean-room route per PROJECT.md), the 64DD IPL (r8 only adds a ROM-type flag "to allow in the future"), and original-hardware cold/warm RAM contents. A passing nemu64-test run through libdragon's IPL3 proves *this boot route*, not original-boot equivalence. [inference from README text]
- **Determinism hazard:** IPL3 "collects entropy" and hands a 32-bit "true random" to the application in DMEM. In our core that value must be a **function of guest-visible state/guest time** (or a profile-fixed constant), never host entropy. Record in the machine profile. [recommend]
- **Mandatory rights stance:** check the libdragon IPL3 binaries (and each nust64-bundled copy) into `fixtures/boot/` with provenance (repo, commit, md5, sha256, license). They are Unlicense binaries built from public source; do not add any Nintendo-derived boot data.

### 5.3 `PeterLemon/N64` (verified 2026-11-09)

| Fact | Value |
|------|-------|
| License | Unlicense. `LICENSE` file was **added 2025-09-27** (commit `76c012cbf5`, "Create LICENSE"); previously unlicensed. Default branch `master`; last push 2026-02-01; 325 stars; not archived |
| Build | Assembly (`.asm`/`.inc`) built with the **bass** assembler: README says "bass (v14) by byuu" and a special **ARM9/bass fork** with N64 MIPS/RSP/RDP output. One `make.bat` per demo (428 `.bat` files, Windows-oriented). `ARM9/bass`: GitHub detects **no license**, last push 2023-07-29. Upstream byuu bass licensing is **[unverified]** here. Each ROM does `insert "LIB/N64_BOOTCODE.BIN"` |
| **Rights flag (high)** | Every ROM includes `LIB/N64_BOOTCODE.BIN`, a 4032-byte (0xFC0, the size of Nintendo's IPL3) blob; the repo's `BOOTCODE/BOOTCODE.asm` header reads "N64 'Bare Metal' NUS-CIC-6102 Boot Code by krom (Peter Lemon)" (30.9 KB of assembly) plus `IPL3Font.asm`. **The repo does not document whether this is clean-room or derived from Nintendo's IPL3.** The Unlicense on the repo does not answer that. [unknown] -> treat as rights-unresolved: **do not copy or run these bootcode bytes in public CI** |
| Repo contains much more than tests | 5208 tree entries; **408 prebuilt `.N64` ROMs**, ~98 MB video blobs (`Video/BigBuckBunny/...`), `Hack/**/*.ips` patches (52 files) and `Translate/**/*.ips` aimed at commercial games, and 37 `.sfc` files under `EMU/SNES` (SNES). A whole-repo clone brings rights-bearing and non-N64 material into the workspace. |
| Result style | CPU tests render results to a framebuffer; each case directory has a `.png` next to the `.N64`, suggesting screen/golden-image verification rather than a text protocol [observed from file listing; not verified per case] |
| Real-hardware claim | README: "I only test with a real N64 using a 64drive". Self-reported **[community]**; verify per case before treating any as hardware-derived |

**Recommendation:** use PeterLemon only as a **source of individual cases**, not as a dependency: record `repo@commit:path` per case, copy the single `.asm` and its includes with provenance, assemble with LLVM/bass pinned, and replace the bundled bootcode with libdragon's `ipl3_compat` (it exists for flat-binary legacy homebrew: loads 1 MiB from ROM `0x1000`, entrypoint from header) so no unresolved boot bytes are involved. **[inference]**: compat IPL3 is designed for this layout but is not tested with PeterLemon's code in this session. Result checking needs a framebuffer/VI path, so PeterLemon cases belong **after** VI scanout exists, not in the first CPU/RSP phases.

### 5.4 Toolchain for our own diagnostic ROMs

| Route | When | Tools | Notes |
|-------|------|-------|-------|
| **A. LLVM assembler (smallest footprint)** | Tiny CPU/CP0/MI/PI/RSP-register diagnostics in hand-written MIPS III assembly | `clang --target=mips64-unknown-elf -mcpu=mips3 -c x.S`, `llvm-objcopy -O binary`, plus a small in-repo packer (header + libdragon IPL3 + payload) | **[measured]** Homebrew `llvm@21` (21.1.8) assembled `lui/ori/sw/ld/dsll32` (MIPS III 64-bit ops) correctly here with no extra linker. Linking multi-object programs needs `ld.lld` (not installed here, unverified). Avoids needing the GCC cross-toolchain for the simplest ROMs |
| **B. libdragon GCC 16.2 toolchain** | Anything written in C, audio/VI diagnostics using libdragon's rdpq/mixer, the homebrew "playable" path | `n64.mk`, `n64tool`, `mips64-elf-gcc`, built from pinned source | `N64_INST` env var; heavy (1h build, cache it); output links libdragon+newlib so the notices go in the manifest |
| **C. Rust (`nust64`)** | Only for nemu64-test | rustup nightly pinned, `nust64 0.4.1` | Kept in the fixture lane only |

Prefer A for first-phase ROMs; B arrives with the playable-homebrew requirement. Do not adopt a mandatory Docker route.

### 5.5 Fixture manifest columns (carry through every route)

`id`, `source repo@full-commit`, `source license (SPDX)`, `generated-ROM rights note`, `build recipe (toolchain pins, flags, features)`, `IPL3 revision + md5/sha256`, `output sha256 + size`, `reproducible (yes/no/unmeasured)`, `profile (machine/TV/RAM/boot/peripherals)`, `result protocol + parser version`, `expected-result provenance (hardware-derived? source, console revision, capture method)`, `status (required/informational/experimental)`. A hash proves identity, not rights.

---

## 6. Optional Host / Presentation Layers (strictly out of core)

| Layer | Pick | Version / license | Why / limit |
|-------|------|-------------------|-------------|
| Window, input, audio, GL/Vulkan surface | **SDL3** | 3.4.20 (released 2026-11-03; stable branch, 2-month cadence), Zlib | One dependency covers window + gamepad + audio + clipboard across desktop OSes. Lives in `host/sdl/` as an optional CMake target, off by default in the gate. It calls the public core API like any other consumer |
| Minimal alternative A | sokol (floooh) | Zlib, active (last push 2026-11-08) | Header-only, smaller; but audio/input story is thinner. Reasonable if SDL3's footprint becomes a problem |
| Minimal alternative B (audio only) | miniaudio | public domain or MIT-0 (README) | Single file; useful if only audio output is needed with another windowing layer |
| Also considered | GLFW (Zlib, last push 2026-08-04), raylib (Zlib) | | GLFW has no audio or gamepad-mapping story; raylib is a framework not a layer |

Rules [recommend]: the core never includes SDL headers, never owns a window or audio device, never queries wall-clock time. The headless runner is host #1 and is the only host required by the PR gate. Host scaling, filtering, audio resampling and frame pacing live in `host/`.

---

## 7. RDP Rendering Approach: Stack Implications (note only)

| Approach | License / dependency cost | Stack consequence |
|----------|---------------------------|-------------------|
| **Software reference, clean-room, C, in-core** | Ours | Fits C17 core, fully deterministic, CI-friendly, zero extra dependencies. Slow but that is the "accuracy first" reference and the oracle for any fast path. **Do not copy from Angrylion-Plus**: its repository ships a "MAME License.txt", which forbids use in commercial products or activities and requires redistributions to carry its terms [primary, read in full]; GitHub detects no SPDX license. That would constrain the project's open **license decision (an owner hard stop)** before it is made. Study it as a reference; do not vendor |
| **paraLLEl-RDP (Vulkan compute)** | MIT; README states it is a from-scratch rewrite studied against Angrylion-Plus, **not a derived codebase**; `parallel-rdp-standalone` last pushed 2024-11-09; bundles `volk`, `vulkan-headers`, a `vulkan/` layer of 55+ C++ files | Requires C++ and a Vulkan 1.x driver with 8/16-bit arithmetic support (`PARALLEL_RDP_SMALL_TYPES=0` exists to disable; a hint that GPU/driver variation is a risk). Bit-exactness is against Angrylion-Plus "where possible", and varies with drivers. **Keep it strictly host-side and optional**, never part of canonical-core outputs. Its conformance suites are differential evidence against Angrylion-Plus, not hardware truth |
| Recommended ordering | Reference first, then host GPU backend if performance demands it, with the reference as the differential oracle in CI | The C core stays dependency-free; the Vulkan backend is a separate target with its own notices |

---

## 8. CI

### 8.1 One local command that mirrors the PR gate

`./scripts/gate.sh` calls `cmake --workflow --preset gate` (configure, build with warnings as errors, CTest T0-T2, install to a temp prefix, installed consumers T4, purity check, fixtures if present). Properties [recommend]:

- Same command in CI: the workflow does `run: ./scripts/gate.sh` and nothing else of substance. LibFuturo does this today (`./scripts/check.sh`) [observed].
- Flags: `--require-fixtures` (CI) turns a missing fixture into a failure; local default reports `missing-fixture` explicitly but never as pass.
- Target: under 3 minutes for the C-only gate on a laptop. **[unknown]** until measured; record real numbers when the first tests exist (AUTONOMY.md asks for measured CI-efficiency reviews). Report emulation throughput, latency, and CI queue/setup/build/test time as separate numbers (PROJECT.md).

### 8.2 GitHub Actions layout

| Job | Runner label (pin explicitly) | Required | Content |
|-----|-------------------------------|----------|---------|
| `gate` | `ubuntu-24.04` | Yes | `./scripts/gate.sh`; matrix gcc-13 and clang-18 (both preinstalled) |
| `sanitize` | `ubuntu-24.04` | Yes | clang ASan+UBSan on T0-T2; short libFuzzer smoke on admission parsers |
| `cmake-floor` | `ubuntu-24.04` | Yes (cheap) | configure+build+test with CMake 3.25.x fetched from cmake.org with a pinned checksum (not a third-party action). The 24.04 image ships 3.31.6, so without this job the 3.25 floor silently rots |
| `portability` | `macos-26` (arm64), `windows-2025` | Advisory at first | Apple clang 21 and MSVC canary; macOS-26 image ships CMake 4.4.5. Add after the first few tests; keep off the PR path if cost matters |
| `fixtures` | `ubuntu-24.04` | When fixture manifest changes + scheduled | Install pinned Rust nightly + `nust64 0.4.1`, build nemu64-test, compare to manifest digest, upload artifact; build libdragon toolchain (cached) for route B |
| `nightly` | | No | timing/cycle/cop0hazard informational lanes, stress features, long fuzz, reproducibility double-build |

Runner-label facts [primary]: GitHub's 2026-05-14 changelog states `windows-latest`/`windows-2025` moved to Visual Studio 2026 (rollout 2026-06-08 to 06-15) and `macos-latest` moved from macOS 15 to 26 starting 2026-06-15 over 30 days; `ubuntu-24.04-arm` images are being migrated by GitHub. Third-party sources claim `ubuntu-latest` is moving from 24.04 to 26.04 between 2026-10-19 and 2026-11-19, but the official changelog I fetched does not mention it **[unverified]**; the `actions/runner-images` repo does have an `Ubuntu2604-Readme.md`. **Never use `*-latest`**: pinning explicit labels makes a runner migration a reviewed change rather than a surprise.

Action pins (latest releases on 2026-11-09 [primary]): `actions/checkout` v7.0.1 (2026-07-20), `actions/cache` v6.1.0 (2026-06-26), `actions/upload-artifact` v7.0.2 (2026-10-07), `actions/download-artifact` v8.0.2 (2026-10-07). **Pin by full commit SHA with a version comment**, as LibFuturo already does (`actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1`) [observed]. Use first-party actions only; avoid conveniences like `mkroening/rust-toolchain-toml` (nemu64-test uses it) and install the Rust nightly with `rustup` directly from the pinned `rust-toolchain.toml`.

Hardening [recommend]: `permissions: contents: read`, `persist-credentials: false`, `pull_request` (never `pull_request_target`) for untrusted code, no secrets in PR jobs, per-job `timeout-minutes`, ROM runs bounded by guest-time/instruction limits and an outer wall-clock kill (wall clock only as a harness guard, never as guest input).

### 8.3 Caching [primary: GitHub docs, read 2026-11-09]

- Limit 10 GB per repository by default; entries unused for over 7 days are evicted; oldest-accessed evicted first when over the limit.
- Scoping: a run restores from its own branch, then the default branch; PR-run caches are scoped to the merge ref and are not readable by other PRs or the base branch; sibling branches cannot read each other's caches.
- Consequences [recommend]: (1) **do not cache the C build**. A small C core compiles in seconds; add ccache only after measuring. (2) **Cache the expensive, pinned things**: the libdragon toolchain (`$N64_INST`, about 300 MB; first build about 1 h) and the fixture ROM, keyed by the pin hash. (3) Populate caches from `main`/scheduled runs so PRs get a restore hit; cold-cache builds must remain possible. (4) Because eviction is silent, the **manifest digest, not the cache, is the source of truth**.
- Cost caveat [unknown]: repository visibility and release channel are open owner decisions (PROJECT.md). Private-repo macOS minutes cost multiples of Linux minutes; I did not verify current multipliers. LibFuturo gates its workflow behind a repo variable and `workflow_dispatch` [observed]; adopt the same switch (`vars.T64_CI_ACTIVATED`) until the owner sets visibility.

---

## 9. What NOT to Use

| Avoid | Why | Use instead |
|-------|-----|-------------|
| `DragonMinded/libdragon` as a pin | Archived 2026-10-01; no more updates | `n64brew/libdragon` at a full commit SHA; re-verify governance before each bump |
| The `toolchain-continuous-prerelease` binaries as a pin | Mutable rolling tag; re-published 2026-10-06 | Build the toolchain from the pinned commit's `build-toolchain.sh` and cache it |
| libdragon's Docker CLI in CI | Adds Docker + Node dependency chain, cannot be pinned by digest as is | Source-built toolchain cache |
| Cloning all of `PeterLemon/N64` | 5k files, 408 prebuilt ROMs, game-patch `.ips` files, SNES ROMs, an IPL3-sized bootcode blob of undocumented origin | Cherry-pick individual `.asm` cases with `repo@commit:path` provenance |
| Anything built around PeterLemon's `N64_BOOTCODE.BIN` | Rights status unresolved | libdragon `ipl3_compat` |
| Copying Angrylion-Plus (MAME License) | Non-commercial, conflicts with the pending license decision | Clean-room software RDP; study only |
| Floating-point host math for COP1 guest results | NaN payload/sign non-determinism is documented even in Rust; C leaves it implementation-defined; GCC contracts FMA by default in GNU modes | Integer soft-float |
| `-ffast-math`, `-Ofast`, `-march=native` in shipped builds | Break determinism and reproducibility | `-ffp-contract=off`, explicit ISA baselines |
| `libm`, `long double`, `rand/time/clock/getenv/printf/fopen/malloc` in the core | Host leakage | Host-provided callbacks, link-level purity test |
| C bitfields or `fwrite(struct)` for registers/snapshots | Layout is implementation-defined | Masks/shifts and explicit serialization |
| Threads or global state in the canonical path | Non-reproducibility, no instance isolation | Single-threaded canonical scheduler; parallel work only as verified host accelerators |
| C++ for the core | Adds ABI, runtime, and exception policy surface for no unique benefit; the public boundary would still be `extern "C"` | C17 |
| Zig for the core (today) | Pre-1.0: 0.17.0 (2026-10-01) removed `@cImport`, reworked the build runner, and release notes say language stabilization is still "a requirement before tagging Zig 1.0" | Revisit after 1.0 |
| Rust nightly in the core build | Needed by nemu64-test only because of its bare-metal MIPS target | Keep it in the fixture lane |
| Meson, Bazel, autotools, Conan, vcpkg | Extra tooling or a package manager that defeats the flat-tree preference | CMake + vendored copies with `PROVENANCE` |
| Git submodules / FetchContent for dependencies | Network at configure time, moving refs | Vendored copies with provenance and a pinned digest |
| GoogleTest/Catch2/CMocka/Unity as framework | Heavy or non-matching outcome model | Own single-header harness |
| `*-latest` runner labels and tag-pinned third-party actions | Silent migrations, mutable tags | Explicit labels, SHA-pinned first-party actions |
| `mkroening/rust-toolchain-toml` and similar convenience actions | Third-party supply chain for one command | `rustup` directly |
| Dynamic plugin loading (`dlopen`) of graphics/RSP/audio | Out of scope per PROJECT.md (no Project64/Mupen64Plus-style plugins) | Linked core with a host API |
| Retro Regen test suite | Terms forbid modification/rebranding (PROJECT.md out of scope) | nemu64-test, own diagnostics |
| Any Nintendo PIF/IPL/CIC bytes, commercial ROMs | Owner rights stop | libdragon IPL3 + HLE/clean-room PIF route |

---

## 10. Alternatives Considered

| Recommended | Alternative | When the alternative is better |
|-------------|-------------|-------------------------------|
| C17 | **Rust (stable, no crates)** | Owner prioritizes memory safety for untrusted ROM/save/archive parsing above portfolio coherence and C-ABI simplicity. Expect: hand-written `extern "C"` header, `cargo-c` or custom install, same FP soft-float requirement |
| C17 | C++20 | Never preferred; only if a future contributor base strongly favors templates for RSP/RDP codegen |
| C17 | Zig | After Zig 1.0, if its toolchain-as-library and cross-compilation become attractive |
| CMake | Meson | If the Python build dependency is acceptable and a contributor base prefers it |
| Own harness | `utest.h` / `acutest` / `greatest` copied with provenance | If the owner prefers not to maintain 200 lines of harness code |
| LLVM assembler route A | libdragon GCC route B for everything | If diagnostic ROMs are mostly C |
| libdragon `ipl3_compat` for PeterLemon cases | Rebuilding with bass + original bootcode | Never in public CI until rights are resolved |
| SDL3 | sokol + miniaudio | If SDL3's size/behavior becomes an issue |
| Software RDP | paraLLEl-RDP host backend | When throughput demands it, behind the reference as oracle |

---

## 11. Installation / Bootstrap Commands (developer machine)

```bash
# Core build (C17). Versions are floors; CI pins exact versions.
# macOS:   brew install cmake ninja
# Ubuntu:  sudo apt-get install -y build-essential cmake ninja-build clang
./scripts/gate.sh                       # one command: configure, build, test, install, installed consumers

# Fixture lane (separate; not needed for core development)
rustup toolchain install nightly-2026-07-16 --profile minimal --component rust-src
rustup toolchain install stable --profile minimal
cargo +stable install nust64 --version 0.4.1 --locked
git clone https://github.com/thelemmy/nemu64-test && git -C nemu64-test checkout 9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c
(cd nemu64-test && cargo run --release)      # -> target/mips-nintendo64-none/release/n64-systemtest.z64

# libdragon toolchain from pinned source (route B only)
git clone https://github.com/n64brew/libdragon && git -C libdragon checkout 98154dafd5ac50e74ee09a5de1035d24a53f54f0
export N64_INST=$HOME/.local/n64   # choose a prefix
(cd libdragon && ./tools/build-toolchain.sh)   # 1h+, needs texinfo, zlib, gmp/mpfr headers as required by the script

# LLVM assembler route A (verified locally with llvm@21)
clang --target=mips64-unknown-elf -mcpu=mips3 -c diag.S -o diag.o
llvm-objcopy -O binary diag.o diag.bin
```

---

## 12. Version / Pin Table (as of 2026-11-09)

| Item | Pin / observed | Source tier |
|------|----------------|-------------|
| C standard | C17 | recommend |
| CMake | floor 3.25; latest 4.4.5 (2026-10-27); local 4.4.3 | primary |
| Ninja | 1.13.2 | primary |
| GCC on runners | 13.2 (Ubuntu 24.04), 15.2 (Ubuntu 26.04 image) | primary |
| Clang on runners | 18.1.3 (24.04); 20.1.8/21.1.8/22.1.2 (26.04); Apple clang 21 (macOS 26) | primary |
| LLVM latest | 23.1.5 (2026-11-03) | primary |
| Rust stable | 1.99.1 (2026-10-13) | primary |
| Rust nightly (fixture) | `nightly-2026-07-16` | primary (nemu64-test) |
| nust64 | 0.4.1 (2024-06-26) | primary |
| Zig | 0.17.1 (2026-11-02); 0.16.0 (2026-04-13) | primary |
| Meson | 1.12.1 | primary |
| SDL | 3.4.20 (2026-11-03) | primary |
| nemu64-test | `9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c` (2026-09-08) | primary |
| n64brew/libdragon | `98154dafd5ac50e74ee09a5de1035d24a53f54f0` (2026-10-05) | primary |
| libdragon toolchain | GCC 16.2.0, binutils 2.45, newlib 4.4.0.20231231, GMP 6.3.0, MPC 1.3.1, MPFR 4.2.1 | primary (build-toolchain.sh) |
| IPL3 | r8 `419c2133...`; nust64-bundled r7 `7f014ed4...` | measured |
| PeterLemon/N64 | master, last push 2026-02-01 | primary |
| ARM9/bass | last push 2023-07-29, no license detected | primary |
| paraLLEl-RDP standalone | last push 2024-11-09, MIT | primary |
| actions | checkout v7.0.1, cache v6.1.0, upload-artifact v7.0.2, download-artifact v8.0.2 (pin by SHA) | primary |

---

## 13. Unknowns and First Measurements to Schedule

1. **[unknown] Reproducibility of the nemu64-test ROM** across clean machines (two builds, compare sha256). Blocks promoting `fixtures` from informational to required.
2. **[unknown] Behavior of the core on nemu64-test's unconditional emux opcodes** (`xioctl_fast`, `xioctl_exit`) and the detect probe. Test on the first CPU phase; they must not trap fatally.
3. **[unknown] Provenance of PeterLemon's `N64_BOOTCODE.BIN`.** Ask the author or find a documented clean-room statement before any use; otherwise rely on libdragon IPL3.
4. **[unknown] `n64brew/libdragon` governance/release cadence** and whether it keeps a stable branch. Re-verify before each pin bump.
5. **[unknown] Licenses of ARM9/bass** (no license detected) and per-component notices for newlib/libgcc in libdragon-built ROMs. Needed only for route B and PeterLemon cases.
6. **[unknown] LibFuturo fit:** repo and docs exist publicly (`szTheory/libfuturo`, Apache-2.0, pushed 2026-11-01) with synthetic, NES (Nesturbator), and Atari 7800 (Muhtari) profiles and no N64 profile [observed]. Its contract's ownership, timing, bounded-advance and output rules need to be compared with the Tendies64 API when that API is drafted. Do not freeze Tendies64's API shape around it.
7. **[unknown] Real CI time/cost.** Measure after the first tests exist; do not pre-optimize.
8. **[unknown] `ubuntu-latest` migration** (third-party claim only).
9. **[unknown] FP policy details:** VR4300 NaN generation, denormal/unimplemented-operation exception cases, and FCSR flag/cause semantics must come from the VR4300 manual and nemu64-test `cop1` cases, not from host behavior. Belongs to the CPU/FPU phase research.
10. **Process note:** the `gsd research-plan` seam returned items for unrelated questions (ares scheduler, libretro, gopher64) when given this task's input file; I fell back to the seam's `websearch`/direct-fetch providers and did not populate the research-store cache.

---

## 14. Sources (accessed 2026-11-09)

HIGH (primary: read directly from repo files, APIs, or official docs):
- https://github.com/thelemmy/nemu64-test (LICENSE, `Cargo.toml`, `rust-toolchain.toml`, `.cargo/config.toml`, `.github/workflows/build-rom.yml`, `src/main.rs`, `src/emux.rs`, `src/isviewer.rs`, `src/sc64.rs`, `src/text_out.rs`, `src/tests/mod.rs`); GitHub API `/repos/thelemmy/nemu64-test` and `/commits`
- https://github.com/lemmy-64/n64-systemtest (deprecation notice in README)
- https://github.com/rust-n64/nust64 and https://crates.io/crates/nust64 (versions, MIT, bundled `src/ipl3/*.z64`)
- https://github.com/n64brew/libdragon (LICENSE.md, `boot/README.md`, `boot/bin/*`, `tools/build-toolchain.sh`, releases, commits), https://github.com/DragonMinded/libdragon (archived banner; wiki "Installing libdragon")
- https://github.com/n64brew/N64FlashcartMenu/issues/436 (downstream migration)
- https://github.com/PeterLemon/N64 (README, LICENSE, `BOOTCODE/`, `LIB/`, full tree), https://github.com/ARM9/bass
- https://github.com/Themaister/parallel-rdp-standalone (README, LICENSE), https://github.com/ata4/angrylion-rdp-plus (`MAME License.txt`)
- https://doc.rust-lang.org/std/primitive.f32.html (NaN bit patterns, algebraic ops), https://doc.rust-lang.org/std/simd/index.html (portable_simd nightly-only)
- https://ziglang.org/download/index.json and https://ziglang.org/download/0.17.0/release-notes.html
- https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html (`-ffp-contract` defaults)
- https://github.com/ucb-bar/berkeley-softfloat-3 (COPYING.txt)
- https://github.com/actions/runner-images (Ubuntu 24.04/26.04 and macOS 26 readmes), https://github.blog/changelog/2026-05-14-github-actions-upcoming-image-migrations, https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching
- GitHub release APIs for Kitware/CMake, mesonbuild/meson, ninja-build/ninja, libsdl-org/SDL, llvm/llvm-project, actions/*; https://static.rust-lang.org/dist/channel-rust-stable.toml
- Local repos (observed): `~/projects/libfuturo` (CMake 3.25, C17, `scripts/check.sh`, SHA-pinned workflow, `docs/contract.md`), nesturbator, muhtari, schwaystation, gabbaboy (`CMakeLists.txt`)

MEDIUM / LOW (secondary or unverified, labeled inline):
- Web search summary linking libdragon's archival to issue #1012 (secondary; causal link unverified)
- Third-party claims about `ubuntu-latest` moving to 26.04 (unverified)
- `cargo-c` as the Rust C-install route; GPL/LGPL/newlib licensing of the libdragon toolchain components; macOS runner cost multipliers; byuu bass license (community knowledge, not verified in this session)

---
*Stack research for: deterministic, accuracy-first, headless-first N64 emulator (Tendies64)*
*Researched: 2026-11-09*
