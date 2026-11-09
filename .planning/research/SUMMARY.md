# Project Research Summary

**Project:** Tendies64 (deterministic, accuracy-first, headless-first Nintendo 64 emulator)
**Domain:** Deterministic N64 machine core with a headless runner, evidence-bound support claims and an embeddable API
**Researched:** 2026-11-09 (four parallel researchers; synthesized the same day)
**Confidence:** MEDIUM overall. HIGH for repository, license, toolchain and fixture facts read from primary sources. MEDIUM for the language choice and the feature and architecture recommendations. MEDIUM-LOW for hardware-behavior claims, because no researcher verified them against the NEC VR4300 or Nintendo manuals.

## Synthesis Notes

### Evidence labels

Each claim keeps the label its source file used. Claims marked **[synthesis]** are this summary's own cross-file judgments and are not researcher evidence.

| Meaning | STACK | FEATURES | ARCHITECTURE | PITFALLS |
|---|---|---|---|---|
| Owner direction (includes bootstrap-pack facts cited as "DNA") | [owner] | [owner] | [OWNER], [DNA] | [OD] |
| Primary source read directly | [primary] | [primary] | [PRIMARY] | [PF] |
| Community reference, not console truth | [community] | [community] | [COMMUNITY] | [CR] |
| Implementation observation (emulator or test source) | [observed] | [observation] | [IMPL] | [IO] |
| Measured in the research session | [measured] | (none) | [MEASURED] | (none) |
| Inference | [inference] | [inference] | [INFER] | [INF] |
| Recommendation | [recommend] | [recommendation] | [REC] | [REC] |
| Unknown | [unknown] | [unknown] | [UNKNOWN] | [UNK] |

Hardware constants (93.75 MHz CPU, 62.5 MHz RCP, 4 MB base RDRAM, memory map) come from the owner bootstrap pack (EMULATOR-DNA.md, cited through PROJECT.md). That pack was not re-read for this synthesis. PROJECT.md requires these facts to be rechecked before they become requirements.

### What changed after the research was written

The researchers wrote while the license, repository visibility and release channel were open. Three things now apply:

1. **License and visibility are decided.** On 2026-11-09 the owner chose Apache-2.0 and a public repository under the szTheory GitHub account. The researchers' rules written "until the owner decides" become concrete. Only ISC, MIT, BSD-3 and Unlicense code may be copied, each with a provenance record. GPL code (gopher64, Project64, Mupen64Plus-core) and Angrylion-Plus code may not be copied. ares is ISC at the root but multi-licensed, so it can be copied only file by file. Do not re-ask the owner about either decision.
2. **STACK's CI cost caveat is stale.** It assumed a private repository. Re-check Actions cost and runner assumptions for a public repository.
3. **Language, build system and API/ABI are still open.** PROJECT.md lists them as Pending. STACK recommends C17, but nothing is decided.

## Executive Summary

Tendies64 is a Nintendo 64 emulator built as one deterministic, plain-data machine core. Hosts own clocks, files, input, presentation and storage. Accurate N64 emulation is done with low-level emulation (LLE): the VR4300, RSP and RDP run real code, and a scheduler orders device activity in guest time. The closest references are ares (ISC; N64 CPU/RSP JIT rewritten in v148 on 2026-05-30; a headless test runner exists only in a third-party fork), cen64 (BSD-3, cycle-level, low recent activity), gopher64 (GPL-3, player-focused) and the Mupen64Plus and Project64 plugin ecosystems (GPL). Every slice of Tendies64's candidate value has at least a partial incumbent, so the research advises against claiming a market gap. The value to be proven is the combination: support claims bound to a named profile, pinned build and rights-cleared fixture; a written determinism contract with a richer outcome taxonomy than exit codes; versioned machine-readable results; and an embeddable API whose refusal and failure rules are tested through an installed consumer. No surveyed project was found to do all of this (not exhaustive).

The recommended approach, with its open decisions called out below: C17 for the core (MEDIUM; Rust is the runner-up, and the choice hardens after the first code); CMake 3.25 with CTest and an in-repo harness; integer soft-float for COP1; an interpreter-first VR4300 with an event scheduler and guest-time bounds; an LLE RSP with a scalar reference before the vector unit; an in-tree software RDP and VI as the canonical renderer, with Angrylion-Plus and parallel-rdp used only as out-of-tree differential oracles; a C-ABI facade with transactional admission, timed input, bounded advance and separate refused, guest-stop and defect classes; and boot through HLE-PIF seeding into an open libdragon IPL3, so no Nintendo boot bytes ship. The build order is foundations, then a first ROM observed headlessly through ISViewer, then the complete VR4300, RSP, RDRAM and timing, RDP and VI, AI, controller and saves, the API freeze with a playable rights-cleared homebrew, and finally performance and expansion profiles.

The main risks are rights and supply chain rather than CPU design. nemu64-test, the candidate first ROM suite, is MIT-licensed, but its generated ROM embeds libdragon's IPL3 and a font (Geneva 9, by Susan Kare) whose provenance the MIT grant does not cover, so it is not yet cleared for public CI. PeterLemon/N64 contains a 4,032-byte boot-code blob of undocumented origin. Angrylion-Plus carries a non-commercial MAME license. libdragon was archived on 2026-10-01 and its continuation is unconfirmed. Other risks are emulator agreement treated as hardware truth, green-on-empty test runners, determinism leaks, and hardware-behavior claims that no researcher has checked against primary documents. The mitigations are a fixture manifest with evidence tiers and rights admission, a six-outcome runner with negative controls, pinned SHAs and digests, a dependency lint plus an interleaved-instance digest test, and a primary-document check gate before the CPU and RSP phases.

## Decisions That Gate the Roadmap

| ID | Decision | Status and who decides | Research default | Blocks |
|---|---|---|---|---|
| D1 | Implementation language, build system, API/ABI (C17 vs Rust) | Open (PROJECT.md: Pending). Not an explicit AUTONOMY stop, but one-way after the first code, so batch it to the owner [synthesis] | C17 (STACK, MEDIUM); Rust is the runner-up | Any core code (Phase 1 ADR) |
| D2 | Use or redistribution of nemu64-test ROMs in public CI and public artifacts | Owner stop (AUTONOMY section 4: test-fixture use without documented rights). Rights not documented | No committed ROM binaries. No uploaded ROM artifacts [synthesis]. Build from source only once rights are documented; otherwise local-only | Phase 4 public CI |
| D3 | IPL3 pin and maintenance owner (libdragon archived 2026-10-01) | Open. PITFALLS requires a named maintenance owner for copied code | Pin the chosen binary by sha256 and mirror it if the license permits; confirm boot/ licensing first | Phase 3 (M-A) |
| D4 | Copy allowlist beyond ISC, MIT, BSD-3, Unlicense | Open. Researchers name candidates outside the list: BSD-2 (ares sljit), zlib (SDL3, sokol, GLFW, raylib), public domain or MIT-0 (miniaudio). LibFuturo, a sibling project, is Apache-2.0 and is also not on the list | Keep the allowlist as written. Link-only zlib dependencies follow the dependency policy with notices | Any copy of out-of-list code |
| D5 | Angrylion-Plus as a local oracle; redistribution of derived expected data | Unknown (ARCHITECTURE and PITFALLS both mark it UNK). Owner or legal | Run locally, out-of-tree only. Never copy. Keep derived data local-only | Phase 7 conformance claims |
| D6 | Release channel and versioning | Open (PROJECT.md: Pending) | Research continues; releases wait | Releases only |
| D7 | Name clearance for "Tendies64" | Not trademark-cleared (PROJECT.md) | No branding, logos or store listings before clearance (PITFALLS 33) | Public release and branding |

No phase requires owner-held hardware. Hardware-derived vectors are used only from documented public sources, with provenance.

Decisions the roadmapper may self-unblock under AUTONOMY section 1, each recorded as an ADR with its reversal evidence: CPU timing-tier numbering, the emux-opcode policy, the wall-clock classification, the determinism-lint mechanism, and the default IPL3 variant.

## Disagreements and Open Decisions

| Topic | What the files say | Synthesis | Decide by |
|---|---|---|---|
| Language | STACK: C17 (MEDIUM), Rust runner-up, with C17's rationale partly resting on sibling-repo conventions (LibFuturo, nesturbator, muhtari, schwaystation). ARCHITECTURE, FEATURES, PITFALLS: language-neutral. ARCHITECTURE: an ADR is needed before slice 1 code | Both options need integer soft-float for COP1, so FP behavior does not separate them. The real trade is C's UB discipline versus no memory safety on untrusted input (C), against memory safety with a heavier C-ABI and copy-from-reference cost (Rust). AGENTS.md says sibling assumptions do not transfer, so the convention argument should be weighed as a preference, not evidence [synthesis] | Phase 1 ADR (D1) |
| CPU timing-tier names | PITFALLS: T0 functional-ordering, T1 instruction count with per-class costs, T2 cycle-accurate; first profile T1, timing group advisory. ARCHITECTURE: tier 0 = functional plus instruction-granular time (1 PClock per instruction), tier 1 = nemu64 `timing` expectations, tier 2 = `cycle` and `cop0hazard`; build tiers 0 and 1 first | The same idea under two numberings that do not map. Adopt one taxonomy in the Phase 1 ADR. Both files agree the first profile uses instruction-granular time with the timing group advisory [synthesis] | Phase 1 ADR (self-unblockable) |
| Boot route | All four: open IPL3 plus HLE-PIF seeding, no Nintendo bytes. STACK: nust64 builds embed IPL3 r7 [measured]; libdragon HEAD is r8 [measured]. ARCHITECTURE: the post-IPL2 state IPL3 expects is UNKNOWN, and boot/ licensing needs checking. PITFALLS: name the route in the profile and write a boot-limits doc | Agreement on the route. Open: which variant (prod, dev, compat) and revision to pin (r7 or r8). ARCHITECTURE's question about which libdragon release nust64 bundles is answered by STACK's md5 match to r7, which has not been independently re-verified | Phase 3 (D3) |
| RSP and RDP sequencing | Agreed: RSP is LLE with a scalar reference; RDP is an in-tree software reference; GPU renderers are host-side only. FEATURES places RSP at P2 and RDP at P3 in its tier table, yet lists RSP LLE in the v1 MVP and RDP in v1.x. FEATURES says RSP precedes RDP "in practice" (inference from libdragon rdpq). ARCHITECTURE says RDP has no dependency on RSP and can be driven by CPU-built lists | Follow ARCHITECTURE's slice order: RSP (slice 4), then RDP (slice 6), which may run in parallel. Use FEATURES' MVP list for scope. The libdragon homebrew path needs both [synthesis] | Phase 5 and 7 planning |
| libdragon continuation | All four: archived 2026-10-01; pin a full SHA and mirror. STACK names n64brew/libdragon (not a GitHub fork; pushed 2026-10-05; Unlicense) as the continuation, and calls the archival-cause link (issue #1012) unverified community. PITFALLS: continuation is UNK, and cites Codeberg and SourceForge copies that are not official. FEATURES: libdragon.dev does not mention the archival. PITFALLS: libdragon.dev still links to the archived repo | Treat n64brew/libdragon as a candidate successor, not a confirmed one. Do not act on the archival cause. Re-verify governance before each pin bump. Name a maintenance owner [synthesis] | Phase 1 policy; Phase 3 pin |
| nemu64-test rights | STACK: "Fixture ROM #1" with a rights-cleared section; facts HIGH. PITFALLS: embedded Geneva 9 font provenance not covered by MIT [INF]. FEATURES: generated-ROM rights UNKNOWN. PROJECT.md: "candidate". STACK's fixtures job uploads the built ROM as an Actions artifact | Candidate, not cleared. STACK's HIGH rating covers facts, not rights. In a public repository, an uploaded artifact may count as redistribution [synthesis] | D2, before Phase 4 |
| Emux opcodes (nemu64-test) | STACK: the core must execute `xioctl_fast` and `xioctl_exit` without a fatal trap; completion comes from the ISViewer stream; real-hardware behavior UNKNOWN. ARCHITECTURE: these are emulator extensions, not hardware. They may appear only in a named harness-extension profile, never in a hardware-accuracy profile; default completion is a guest-log pattern plus bounded advance | Adopt ARCHITECTURE's position. The hardware profile does not implement non-hardware encodings, and completion comes from ISViewer output. STACK notes the ROM spins after these ops on unsupported emulators, so ISViewer output is the only sound completion signal. Record as an ADR; test on day one [synthesis] | Phase 1 ADR; validate in Phase 3 |
| Timeouts and wall clock | STACK, FEATURES, ARCHITECTURE: guest-time bounds are authoritative; wall clock is a harness backstop only. PITFALLS: timeouts never use wall clock | Compatible. None says whether a wall-clock kill is `timeout` or `harness-error`. Recommend `timeout` for a harness-side kill, with `harness-error` reserved for harness faults [synthesis] | Phase 1 |
| Determinism lint | STACK: link-level purity test (`nm -u` allowlist). ARCHITECTURE: source-level lint (no time, filesystem, thread or random imports; no mutable statics) | Use both. They catch different leaks: linked symbols versus source-level statics [synthesis] | Phase 1 |
| Copy rules | STACK and PITFALLS were written while the license was open and treated GPL and Angrylion-Plus as owner hard stops. PITFALLS allows reading GPL source "for understanding" with a clean-room-style note, but no other file states that as policy | Now resolved by Apache-2.0 (see Synthesis Notes). Ratify the reading rule as project policy. The parallel-rdp suites (MIT) are not fully clean: FEATURES notes they use a modified Angrylion reference (UNK) | Phase 1 provenance policy; Phase 7 |
| CI visibility and cost | STACK's cost caveat assumed open visibility. STACK proposes a LibFuturo-style gate, `vars.T64_CI_ACTIVATED` | Visibility is now public. Re-check Actions cost assumptions and whether the gate still applies [synthesis] | Phase 1 |

## Key Findings

### Recommended Stack (STACK.md)

The stack is a C17 core built with CMake 3.25 or later, CTest and a single gate command. The toolchain and fixture facts are verified. The language choice is a judgment with a named runner-up. The test fixture lives in a separate, pinned lane and never touches the core build.

- **C17**, GCC and Clang supported, MSVC as an advisory canary [recommend, MEDIUM]. Enforce UB avoidance in CI: unsigned arithmetic with masked shifts; `memcpy` or byte-composed loads and stores; no C casts for guest float-to-int conversion; no bitfields for registers or snapshots.
- **Integer soft-float is canonical for COP1** [recommend, HIGH]. Host FP is a later fast path only behind lockstep differential tests, with `-ffp-contract=off` and no `-ffast-math`. Berkeley SoftFloat 3e is a copy candidate, described as BSD-3-Clause-style; verify the exact SPDX text before copying, or write a subset.
- **CMake 3.25 floor, Ninja, CMakePresets with a `gate` workflow** [recommend, HIGH]. `install(EXPORT)` and `find_package(CONFIG)`, a static library first and a hidden-visibility shared library second, and a generated `.pc`.
- **CTest with an in-repo single-header harness** (about 200 lines), vectors in data files [recommend, HIGH]. Copy candidates if the owner prefers: utest.h (Unlicense), acutest (MIT), greatest (ISC).
- **Sanitizers and fuzzing**: clang ASan and UBSan lane; libFuzzer targets for ROM and save admission [recommend, HIGH].
- **Fixture lane**: nemu64-test pinned to `9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c` (2026-09-08), built with `nightly-2026-07-16` and `nust64` 0.4.1 `--locked`. Facts HIGH; reproducibility across machines unmeasured.
- **Own diagnostic ROMs**: route A is LLVM (`clang --target=mips64-unknown-elf -mcpu=mips3`, measured with llvm@21) plus a small packer, preferred for early ROMs. Route B is libdragon's GCC 16.2 toolchain built from a pinned commit, for C-level and rdpq work. Do not use the Docker CLI route or the rolling prerelease binaries.
- **Host layers** (later, separate target): SDL 3.4.x (zlib). The core never includes or links SDL.
- **CI**: GitHub Actions with explicit runner labels (never `*-latest`), first-party actions pinned by SHA, a CMake 3.25 floor job, and one command (`scripts/gate.sh`) that the pull-request job calls verbatim.

Pins the fixture manifest must record:
- libdragon IPL3 prod r8, md5 `419c213307cdb855934852c67759102b` [measured]; the nust64-bundled prod r7, md5 `7f014ed41cc53bb1a5e44f211f8c3dec` [measured], is the one inside nemu64-test builds.
- libdragon at n64brew commit `98154dafd5ac50e74ee09a5de1035d24a53f54f0` (2026-10-05). Toolchain pins from `build-toolchain.sh`: binutils 2.45, GCC 16.2.0, newlib 4.4.0.20231231, GMP 6.3.0, MPC 1.3.1, MPFR 4.2.1.

Avoid: DragonMinded/libdragon as a pin; prerelease toolchain binaries; the Docker CLI in CI; cloning PeterLemon/N64 wholesale or running its BOOTCODE; copying Angrylion-Plus; host FP for COP1; `-ffast-math`, `-Ofast`, `-march=native` in shipped builds; `libm` and stdio or allocator calls in the core; C++ or Zig for the core; Rust in the core build; Meson, Bazel, Conan or vcpkg; submodules or FetchContent; GoogleTest or Catch2; convenience actions such as `mkroening/rust-toolchain-toml`; `dlopen` plugins; the Retro Regen suite.

### Expected Features (FEATURES.md)

The audience is the owner, then emulator and homebrew developers, then players. Table stakes are scoped to that order. FEATURES does not claim a market gap.

**Must have (table stakes):**
- Named first profile with explicit selection (console revision, TV type, 4 MB RDRAM, boot route, cartridge-only).
- VR4300 with CP0, TLB and exceptions, passing a pinned ROM-level suite.
- Boot without Nintendo bytes (open IPL3 or documented HLE) with its limits stated.
- Headless runner with ISViewer capture and the six-way outcome taxonomy.
- RSP LLE, scalar and vector: required by libdragon and by any LLE claim.
- RDP reference rendering and VI raw scanout (homebrew needs rdpq).
- AI raw audio with DMA and cadence.
- Save media with explicit device selection and refusal postconditions (EEPROM 4 and 16 Kbit, SRAM first).
- Standard controller as timed logical input.
- Raw frame and audio capture with hashes, treated as regression pins.
- GDB stub and ISViewer or `debugf` visibility.
- One playable, rights-cleared homebrew path and a minimal host, so the project is not tests-only.

**Should have (differentiators, each hedged):**
- Evidence-bound support claims per named profile (profile, build, fixture manifest, evidence tier), machine-checked in CI. The ares compatibility table is per title and version with an "Unverified" badge; no profile-bound scheme was found.
- A written determinism contract with guest-time limits. ares-test already has emulated time in a fork; the differentiator is making it a core invariant.
- A rich outcome taxonomy and versioned result protocol. ares-test uses exit codes 0, 1 and 2.
- A guest-time-boundary trace stream. Confidence LOW to MEDIUM.
- An embeddable API with lifetime and refusal postconditions, proven by an installed-consumer test. Mupen64Plus core and libretro already offer embedding, so the claim is about contract quality.
- RDP and VI conformance reporting against a recorded reference, with stated limits. License-gated.
- Replay artifacts bound to profile, build and content hash.
- The fixture and rights manifest as a product artifact.

**Defer (v2+ or later profiles):** Controller Pak, Rumble Pak, Transfer Pak; Expansion Pak; PAL and MPAL profiles; other CIC variants; FlashRAM edge cases; 64DD (after cartridge verification and a rights-cleared disk route); GPU renderers; save-state UI; rewind; run-ahead; netplay; RetroAchievements; cycle-timing profile; JIT with equivalence testing.

**Anti-features:** HLE graphics or audio microcode paths in the canonical core; a per-game hacks database; bundled PIF, IPL, CIC, commercial ROMs, saves or 64DD media; a plugin architecture; CPU overclock; wall-clock watchdogs in the core; cheats, widescreen or texture packs inside the machine; compatibility claims from boot screenshots, a single suite or agreement with another emulator; telemetry, accounts or a library manager; early JIT; Aleck64, iQue or Retro Regen.

**Feature-tier inconsistency to resolve:** FEATURES' Part 1 tier table (RSP P2, RDP P3) disagrees with its own MVP list (RSP LLE in v1). Use the MVP list for scope and ARCHITECTURE's slice order for sequence.

### Architecture Approach (ARCHITECTURE.md)

One `Machine` value owns all guest-visible state as plain data, with no threads, coroutines, globals or host calls. Guest time is one u64 `GuestTick` that only the core advances. A single-threaded event queue, totally ordered by (tick, class, sequence), coordinates per-component local clocks through lazy catch-up at defined observation points. The CPU drives time; RSP, RDP, VI, AI, PI and SI are caught up when observed or when their events fire. The master tick is 187.5 MHz (2 ticks per CPU PClock, 3 per RCP clock) [REC, from the owner-cited clock ratios]. The VI and AI video domains are asynchronous rational domains converted with integers, never floats.

Major components, each owning its state inside the instance:
1. **Scheduler and event queue**: `GuestTick`, per-component clocks, catch-up table.
2. **VR4300 CPU**: interpreter; CP0 and TLB; exceptions with explicit `pc`, `next_pc` and delay-slot state; I and D cache model; CP1 soft-float.
3. **Bus and memory map**: table-driven physical map with a fast-mem path; one endianness accessor layer or canonical big-endian storage.
4. **RDRAM and RI**: 4 MB base, 9th-bit plane; two-tier register model (tier A presets `RI_SELECT` to skip init; tier B models the register protocol).
5. **MI**: interrupt pending and mask semantics.
6. **PI and cartridge**: DMA as an event; domain timing; ROM normalization at admission.
7. **SI and PIF**: HLE state machine (not CPU code); joybus parsing; channel 4 save path.
8. **Joybus devices and save media**: one `JoybusDevice` interface; save images held in the instance with a guest-generation counter.
9. **RSP**: LLE scalar and VU, DMA, `SP_STATUS`, semaphore, halt and break.
10. **RDP**: in-tree software reference renderer; TMEM; SYNC_FULL as a sticky event.
11. **VI**: software scanout that replays a half-line register log per field; raw `VideoFrame` before host scaling.
12. **AI**: DMA queue, DACRATE cadence, IRQ at DMA start, 8 KiB carry quirk.
13. **Profile registry**: immutable descriptors; hash stamped into snapshots, outputs and results.
14. **Output rings**: fixed-capacity video, audio, guest-log and trace rings, drained by the host after `advance`; no callbacks.
15. **C-ABI facade**: transactional `admit`; three error classes: refused (state bit-for-bit unchanged), guest stop (resumable, not an error) and defect (instance poisoned).
16. **Host**: headless runner (first consumer; fixture manifest; outcome mapping; result JSON); later frontends and adapters.

Illustrative API flow: create, then admit (profile and content; refuse without state change), queue timed input, advance (any-of stop: tick limit, instruction limit, frame event, output full, breakpoint), drain video, audio and guest log, export or import media, reset or unload, destroy; state capture, restore and digest. Lifecycle: Created to Ready; unload returns to Created; any invariant violation moves to Poisoned.

Determinism rules (ARCHITECTURE): `GuestTick` advances only in the scheduler; total event order with the sequence counter in state; all inputs enter as timestamped events; entropy comes from a profile seed; no hash-map iteration order; no host-dependent float; `advance` is a pure function of (state, stop).

Sync quantum: the granularity of CPU-RSP interleaving is part of profile identity. The reference profile uses the finest practical quantum; any coarser quantum is a separately named profile.

Reference-keeping: interpreters, the RSP scalar and VU reference, and the software RDP and VI stay in-tree behind an equality harness. Optimizations ship only with digest equality against the reference.

Anti-patterns to avoid: frame-driven loops ("CPU N cycles, then RSP, then RDP"); HLE task handling keyed to microcode with fixed delays; coroutine or thread-per-component designs; callbacks into the host during `advance`; core-initiated file, clock or entropy access; singleton state; determinism knobs such as `randomize_interrupt`; derive-based serialization; a GPU RDP as canonical; treating the sync quantum as a performance knob.

Reference-emulator fit: ares (ISC) is the closest structural fit, but it has process globals and libco coroutines. cen64 (BSD-3) is lockstep, with a threaded barrier window in which cross-thread order is not guest-time ordered. gopher64 (GPL-3) runs RSP bursts with a spin-detection heuristic and seeds RNG and RTC from the host. Mupen64Plus-core (GPL-2 or later) runs RSP tasks atomically with fixed delay constants. Only ares (per file) and cen64 are copy candidates under the allowlist, and only with provenance.

### Critical Pitfalls (PITFALLS.md)

PITFALLS lists 34 pitfalls ordered by leverage on evidence-backed claims. The ten most important:

1. **Emulator agreement treated as hardware truth** [OD, PF]. Expected values taken from ares, Angrylion-Plus or parallel-rdp. Prevention: an expectation-provenance enum (hardware-derived, self-checking ROM, vendor document, differential against emulator@version, own derivation). Differential rows cannot promote a claim above "differential agreement". Never copy CIC seeds or RDP lookup tables from another emulator without an independent derivation test.
2. **Hash or smoke test as proof** [OD]. Prevention: name hash tests `regression-pins`; add an oracle-type column to claims; test audio with exact sample expectations, never "non-zero".
3. **Green on empty** [OD, CR]. Prevention: six mutually exclusive outcomes; pin the expected total N per fixture and feature set; four CI negative controls (a wrong expectation must fail; empty output classifies as boot-fail or timeout; a deleted fixture is missing-fixture; an injected harness crash is harness-error).
4. **Experimental groups in the required gate** [IO]. Prevention: three lanes (required, scheduled or advisory, research). The required lane runs `base` only; `timing`, `cycle` and `cop0hazard` are informational; `experimental_rdp` and `poorly_understood_quirk` never gate.
5. **Unpinned fixtures and archived upstreams** [PF]. Prevention: full SHAs, digests, toolchain pins and `Cargo.lock`; a digest mismatch is harness-error; name an IPL3 owner.
6. **Rights leakage through "open" ROMs and committed binaries** [PF, IO]. Prevention: per-asset admission (source rights, every embedded asset, build-tool blobs, whether the binary may be redistributed). Build from source in CI; commit only digests and recipes; keep uncleared ROMs local-only; author Tendies64's own diagnostics for the permanent smoke lane; never import PeterLemon's `BOOTCODE` or `IPL3Font`.
7. **Host-state leaks** [OD, IO]. Prevention: core inputs limited to profile, content, timestamped input and guest-time budgets; soft-float; a scalar RSP reference before any SIMD; required tests run twice in different environments with full-state digests compared; a sanitizer lane.
8. **License contamination** [PF]. Prevention: a PROVENANCE record for every copy (upstream path, SHA, license text, retrieval date, owner); a PR checklist item; a license scanner; the allowlist in D4.
9. **Libdragon IPL3 treated as "boot solved"** [CR, IO]. Prevention: name the boot route in the profile; write a boot-limits document listing unmodeled behavior (PIF challenge, NMI on mismatch, the roughly 5-second command timeout, CIC variants 6101 to 6106, the 6105 challenge protocol, the 64DD IPL) as deferred, never as supported; run the real IPL3 before any HLE shortcut.
10. **Region, TV type and CIC inferred from the header** [CR]. Prevention: profile-only inputs; refuse on mismatch with `refused: profile-mismatch`; a two-profile diff test.

Moderate and minor pitfalls (21 to 34) cover unaligned access and LL/SC; FPU rounding, denormals and NaN; PI DMA and open bus; MI masked versus pending; ISViewer capture bounds; the tests-only trap; dependency creep through test tooling; public CI carrying commercial content; partial state mutation on refusal; the header check code mistaken for a CRC; the clock-rate header field; VI filters folded into host scaling; trademark; and save and fixture endianness.

Hardware specifics to verify before they become requirements (all community or implementation-sourced; none verified against the manual): `sra` and `srav` upper-bit leak; `mult` and `div` as 35-bit operations; the early-board `mul` bug; RSP unsigned clamp asymmetry and the VMOV upper-lane bug; AI IRQ at DMA start and the 8 KiB carry; RDP unsynced-attribute corruption and the SYNC_FULL ordering rule; EEPROM 4 Kbit (info response 0x0080) and 16 Kbit (0x00C0) with a roughly 6 ms write busy window; Controller Pak CRC-8 (polynomial 0x85) and address checksum, with an absent pak returning zeros and CRC 0xFF.

## Implications for Roadmap

### Sequencing principle

- **Foundations before emulator code.** Governance, rights admission, the runner's six outcomes and negative controls come first (PITFALLS S0). Every later claim depends on them.
- **Boot to a headless ROM observation before CPU completeness.** The earliest end-to-end proof exercises the runner, manifest and boot route while the CPU is still small (ARCHITECTURE slice 2).
- **CPU before RSP.** RSP groups need the CPU and DMA. RSP and RDP are independent units and may proceed in parallel once RDRAM and MI exist.
- **API freeze last.** A candidate can be drafted early; the freeze waits until postconditions are observed through an installed consumer.
- **Performance last.** Benchmarks run only on a repeatable correctness baseline (AGENTS.md, PROJECT.md).

### Suggested phases

These are suggestions. The roadmapper may merge 2 with 3, 5 with 6, or 8 with 7 if scope demands. S-stage labels map to PITFALLS' provisional stages; slice numbers map to ARCHITECTURE's build order.

**Phase 1: Governance, rights and harness foundation** (PITFALLS S0; ARCHITECTURE slice 1)
- **Rationale:** Every later claim depends on provenance, rights admission and outcome classification. This is the cheapest place to enforce isolation. Gated by D1 and D4.
- **Delivers:** Provenance policy under Apache-2.0 (PROVENANCE record per copy); fixture-manifest schema (source SHA, SPDX, per-asset rights, IPL3 md5 and sha256, output digest, reproducibility flag, profile, evidence-tier enum, lane); six-outcome runner skeleton with four negative controls; CMake 3.25 and CTest skeleton with `scripts/gate.sh`; core lint (no time, filesystem, thread, random or mutable-static imports) and the `nm -u` purity check; installed-consumer scaffold against a stub API; ADRs for language, timing-tier numbering, emux-opcode policy and the wall-clock classification.
- **Addresses:** Headless runner and outcome taxonomy (table stakes); manifest and negative controls (differentiators).
- **Avoids:** PITFALLS 1, 2, 3, 5, 6, 8, 15, 25, 27, 28.
- **Exit evidence:** Runner classifies all six outcomes on synthetic cases; a wrong expectation fails; lint rejects a forbidden import; the manifest schema validates; the ADRs are accepted.
- **Research flag:** Standard patterns. Blocked by D1 and D4.

**Phase 2: Deterministic machine skeleton** (ARCHITECTURE slice 2, part; PITFALLS S1-S2)
- **Rationale:** Time, profile, outputs and digest underpin everything else. Determinism is cheapest to build in now.
- **Delivers:** `GuestTick` and total-order event queue; immutable profile descriptors with hash; output rings; state digest; physical memory map; RDRAM tier A (`RI_SELECT` preset); MI with pending and mask semantics; PI and SP DMA with copy-at-completion (flagged as a hazard); ISViewer as a device in a diagnostic profile; VI register and half-line counter stub; two-instance interleave test.
- **Addresses:** Profile identity; deterministic scheduler; ISViewer capture; the raw-output contract.
- **Avoids:** PITFALLS 7 (determinism), 13 (timing from clock ratio alone), 23 (DMA as `memcpy`), 24 (MI masked versus pending), 29 (partial mutation on refusal).
- **Exit evidence:** Interleaved instances produce digests equal to isolated runs; digests are identical on two operating systems; synthetic guest ISViewer writes parse through the runner.
- **Research flag:** Standard for the time, profile, output and bus pieces. The boot and ISViewer-semantics pieces that ARCHITECTURE rates as deeper research sit in Phase 3 [synthesis].

**Phase 3: First headless ROM observation through open IPL3** (ARCHITECTURE slice 2; PITFALLS S2; milestone M-A)
- **Rationale:** The earliest end-to-end proof. It exercises the runner, manifest, boot route and ISViewer before CPU completeness is attempted.
- **Delivers:** VR4300 integer interpreter (enough for IPL3 and a hello ROM); CP0 basics (Count, Compare, Status, Cause); basic exceptions; HLE-PIF seeding of post-IPL2 state; libdragon IPL3 (pinned binary, sha256 in `fixtures/boot`) running as guest code; one hello ROM, authored with route A or a rights-cleared libdragon build, printing through ISViewer; the boot-limits document.
- **Addresses:** Boot without Nintendo bytes; first headless ROM run.
- **Avoids:** PITFALLS 9 (boot solved), 10 (region from header), 5 (IPL3 pin and ownership), 6 (IPL3 and hello-ROM rights), 15 (wiki as spec), 25 (ISViewer grammar).
- **Exit evidence:** Hello output through the headless runner; identical digests on two operating systems; empty output classifies as boot-fail; the boot-limits document lists unmodeled PIF and CIC behavior.
- **Research flag: DEEP.** The post-IPL2 state IPL3 expects (UNKNOWN); libdragon `boot/` per-file licensing; IPL3 entropy sources (UNK); ISViewer length-register semantics at 0x14 versus libdragon's 0x16 byte count (FEATURES); the IPL3 variant and revision. First-profile assumption [synthesis]: NTSC, cartridge-only, 4 MB RDRAM, CIC 6102, consistent with libdragon's prebuilt IPL3 signing and FEATURES' note that 6102 and 7101 cover most titles.

**Phase 4: Complete VR4300 and nemu64-test CPU groups** (ARCHITECTURE slice 3; PITFALLS S1; milestone M-B)
- **Rationale:** The largest block of independent expected values, and CPU correctness gates almost everything later.
- **Delivers:** 64-bit operations; LL, SC, LLD, SCD; alignment and address errors; delay-slot-aware exceptions (`pc`, `next_pc`, `in_delay_slot`); TLB (refill, invalid, modified, XTLB); a CP0 writable-mask table; a functional cache model with a written scope decision; CP1 soft-float (single and double, rounding, FCR31, unimplemented operation); native tests with independent expected values; per-group nemu64-test status under the runner, once D2 is decided.
- **Addresses:** Accurate CPU; ROM-level CPU tier; FPU.
- **Uses:** Integer soft-float. Copy SoftFloat 3e only after an exact license check and provenance; otherwise write a subset.
- **Avoids:** PITFALLS 11 (32/64-bit semantics), 12 (delay slots), 14 (TLB, CP0, caches), 21 (unaligned access, LL/SC), 22 (FPU), 15 (bring-up shims, tagged and release-blocked), 4 (per-group lanes).
- **Exit evidence:** Native tests green with independent expectations; nemu64-test CPU groups pass or classify. Report status per group, not "base passes": full `base` also needs RSP and RDRAM tier B (ARCHITECTURE).
- **Research flag: DEEP.** No researcher read the NEC VR4300 user's manual. Every CPU, FPU, TLB, cache and delay-slot detail is INFER, COMMUNITY or UNKNOWN until checked. Read a mirrored copy, record its SHA, and complete the check before this phase starts.

**Phase 5: RSP LLE** (ARCHITECTURE slice 4; PITFALLS S3; RSP milestone M-C)
- **Delivers:** RSP scalar (SU) and then VU (COP2) scalar reference; `SP_STATUS`, semaphore, halt, break, `SP_PC`; DMA with 8-byte granularity, bank wrap, stride and double buffering; a policy for CPU access to DMEM and IMEM while the RSP runs; nemu64-test RSP groups; per-opcode VU fuzzing against the scalar oracle; a scheduled stress lane (`vmulf` through `vrsq32`).
- **Addresses:** RSP LLE (libdragon prerequisite; ROM-level RSP tests).
- **Avoids:** PITFALLS 16 (SIMD before scalar; tables pasted from elsewhere), 4 (stress in a scheduled lane); opcode coverage mistaken for task compatibility.
- **Exit evidence:** RSP groups pass or classify; zero-difference VU fuzz against the scalar reference; DMA and semaphore tests.
- **Research flag: DEEP.** DMA data-copy timing (start versus progressive; UNKNOWN in STACK and ARCHITECTURE); dual-issue and stall rules; DMEM and IMEM contention; halt corruption; VMOV and clamp details, checked against test vectors rather than wiki prose.

**Phase 6: RDRAM tier B, MI repeat mode and timing tier 1** (ARCHITECTURE slice 5)
- **Delivers:** RDRAM register protocol (delay, device ID, broadcast, MI repeat mode, calibration read-back); cold-boot IPL3 path; timing tier 1 (Count and Random per instruction; cache timing); full `base` status.
- **Addresses:** Full `base` coverage; timing tier; cold-boot path.
- **Avoids:** PITFALLS 13 (timing), 23 (open-bus behavior; RDRAM above 4 MB without Expansion Pak is UNKNOWN).
- **Exit evidence:** `rdram`, `mi` and `timing` groups classified per lane; full `base` count pinned in the manifest.
- **Research flag: MODERATE.** RDRAM calibration values; behavior between 4 and 8 MiB without the Expansion Pak.

**Phase 7: RDP software reference, software VI and raw frames** (ARCHITECTURE slice 6; PITFALLS S4)
- **Rationale:** Needs RDRAM and MI, not RSP. Can run in parallel with Phases 5 and 6.
- **Delivers:** RDP command processor (DPC registers, TMEM, rasterizer, combiner, blender, coverage and 9th bit, fill and copy); SYNC_FULL and DP interrupt; software VI replaying a register log at field end and producing raw `VideoFrame`; RDP dump record and replay in `tools/`; differential harness with labeled oracles; frame hashes recorded as regression pins.
- **Addresses:** RDP reference; VI raw scanout; raw framebuffer output.
- **Uses:** parallel-rdp-standalone suites only after a per-file license audit (its conformance tests use a modified Angrylion reference; UNK). Angrylion-Plus only as an out-of-tree local oracle, pending D5.
- **Avoids:** PITFALLS 1, 2, 17 (fixed 320x240), 32 (VI filters folded into host scaling). Undefined-behavior choices (noise seed, undefined MemoryColor) are recorded as named policy choices.
- **Exit evidence:** Conformance results labeled differential, with oracle version and modifications; hi-res interlace and PAL fixtures reviewed individually; no "accurate" claim without an evidence tier.
- **Research flag: DEEP.** RDP command timing and cost; TMEM and coverage detail; SYNC_FULL timing; unsynced-attribute behavior; VI filtering and per-line behavior; license audit.

**Phase 8: AI and raw audio** (ARCHITECTURE slice 7; PITFALLS S5)
- **Delivers:** AI DMA queue (two deep); DACRATE cadence; IRQ at DMA start; the 8 KiB boundary quirk (profile-togglable if proven revision-dependent); `AudioChunk` ring with sample index and rate metadata; an authored audio diagnostic with exact sample expectations.
- **Addresses:** AI raw audio.
- **Avoids:** PITFALLS 18 (host-clock coupling), 2 ("non-silent" is not evidence); the IRQ-at-end mistake.
- **Exit evidence:** The authored diagnostic matches exact samples across DMA boundaries; an IRQ-at-start test; an 8 KiB carry test.
- **Research flag: MODERATE.** PAL DACRATE values (UNKNOWN); cadence phase relative to CPU time; carry-bug vectors. ARCHITECTURE places audio after RSP because real content needs RSP audio microcode. Whether a CPU-driven authored diagnostic can come earlier is an inference no researcher checked [synthesis].

**Phase 9: Controller, save media and serialization** (ARCHITECTURE slice 8; PITFALLS S6)
- **Delivers:** SI and PIF HLE state machine; joybus parsing; standard controller as timed logical input; EEPROM 4 and 16 Kbit (busy window, 4K mirroring, 8-byte blocks); SRAM through PI domain 2; explicit device selection with refused-operation postconditions; import and export round trip with endianness; versioned state serialization and digest; replay artifacts bound to profile and content.
- **Addresses:** Controller input; save baseline; serialization for replay checkpoints.
- **Avoids:** PITFALLS 19 (title-keyed save type), 34 (endianness), 29 (partial mutation), 24 (interrupt acknowledgment).
- **Exit evidence:** Refusal leaves the digest unchanged; EEPROM busy-window test; restore-then-digest round trip.
- **Research flag: MODERATE.** Joybus timing (roughly 62.5 µs response window); EEPROM write timing. FlashRAM is deferred.

**Phase 10: API freeze, installed consumer, minimal host and playable homebrew** (ARCHITECTURE slice 9, part; PITFALLS S7)
- **Delivers:** C-ABI facade frozen only after lifetime, profile selection, admission and refusal, timed input, bounded advance, outputs, reset and unload, and failure postconditions are tested through an installed consumer (clean prefix, relocation check, negative controls, no source tree visible); a minimal host (SDL3 as an optional target; the core never linked to SDL); one rights-cleared playable homebrew path (authored, or a libdragon build from a pinned toolchain); a LibFuturo adapter decision, which is optional and lossy.
- **Addresses:** API contract; installed consumer; the playable path, which guards against a tests-only project.
- **Avoids:** PITFALLS 20 (frozen API that does not fit), 26 (tests-only trap), 27 (toolchain dependency creep), 33 (no branding before clearance).
- **Exit evidence:** The installed consumer runs the headless fixture from a clean prefix; the playable homebrew boots headlessly and in the minimal host.
- **Research flag: DEEP** for the playable path [synthesis]: the route B toolchain build (about one hour; GPL GCC and newlib notices; archival) and a LibFuturo contract recheck before any adapter. MODERATE for the API.

**Phase 11: Performance, accessories and expansion profiles** (ARCHITECTURE slice 9, part; PITFALLS S8-S9; deferred by default)
- **Delivers:** A named benchmark manifest (emulation throughput and latency reported separately from CI time); cached interpreter; RSP decode cache; block JIT last, with lockstep equality against the interpreter; accessories as named profiles (Controller Pak with CRC and status rules, then Rumble, then Transfer Pak); Expansion Pak, PAL and MPAL, other CIC variants; FlashRAM; 64DD as a separate system profile after cartridge verification and a rights-cleared disk route.
- **Addresses:** Performance; expansion profiles.
- **Avoids:** PITFALLS 20 (JIT too early), 19 (Pak checksum rules), 10 (TV and region as profile inputs).
- **Exit evidence:** Benchmarks report per named workload with output held constant; JIT digest equality across the corpus.
- **Research flag: DEEP** for JIT, the Transfer Pak (Game Boy content rights) and 64DD (disk rights) [synthesis]. MODERATE for the Controller Pak.

### Phase ordering rationale

- **Dependencies:** time, profile and outputs; then bus and interrupts; then CPU; then boot; then ROM observation; then CPU completeness; then RSP and RDRAM and timing; then RDP and VI (in parallel); then AI; then input, saves and serialization; then API and playable path; then performance.
- **Boot before CPU completeness:** the earliest headless ROM proof surfaces runner, manifest and boot-route problems while the CPU is still small (ARCHITECTURE).
- **Governance before code:** negative controls must exist before any emulator code (PITFALLS S0).
- **API freeze last:** a candidate can be drafted earlier, but the freeze needs observed postconditions.
- **Performance last:** a correctness baseline comes first (AGENTS.md).

### Research flags

- **Deep research before planning** (`/gsd-plan-phase --research-phase`): Phase 3 (boot state, IPL3 licensing), Phase 4 (VR4300 primary-document check), Phase 5 (RSP DMA and pipeline timing), Phase 7 (RDP and VI), Phase 10 (playable toolchain; LibFuturo recheck), Phase 11 (JIT, 64DD, Transfer Pak).
- **Moderate:** Phases 6, 8, 9.
- **Standard patterns:** Phases 1 and 2, once D1 and D4 are decided.
- **Add to the start of Phase 3** [synthesis]: a primary-document verification task. Fetch and pin mirrored copies of the NEC VR4300 user's manual and the Nintendo documents the bootstrap pack cites, with SHAs. Convert [IMPL], [CR] and [INF] claims in the CPU, RSP and RDP areas into requirements only after a primary citation or a test.

## Confidence Assessment

| Area | Confidence | Notes |
|---|---|---|
| Stack | MEDIUM-HIGH | Toolchain, repository, fixture and runner-image facts verified from primary sources on 2026-11-09 (HIGH). The language choice is a judgment (MEDIUM). CI cost assumptions assumed private visibility and are now stale. "HIGH" for fixtures covers facts, not rights. |
| Features | MEDIUM | HIGH for what surveyed projects ship (READMEs, release notes, source). MEDIUM for hardware-block needs (inference and community). LOW for any claim of absence (not exhaustive). |
| Architecture | MEDIUM-HIGH for structure; MEDIUM for scheduling; LOW for clock-domain exactness and RDRAM bus contention | Several independent sources agree on the event-queue and plain-data shape. NTSC/PAL/MPAL crystal values and inter-domain phase are UNKNOWN. |
| Pitfalls | MEDIUM | HIGH for license, repository and fixture facts (read directly). MEDIUM for hardware behavior (community references). CPU, FPU, TLB, cache and delay-slot items are not verified against the NEC manual and are flagged as such. |
| Overall | MEDIUM | Sound enough to set phase order and gates. Not sound enough to write hardware requirements before the primary-document checks in Phases 3 and 4. |

## Gaps to Address

**Before Phase 1 (decide or record):**
- D1: language, build and ABI (C17 versus Rust). Owner.
- D4: copy allowlist and the out-of-list candidates (BSD-2, zlib, public domain or MIT-0; LibFuturo's Apache-2.0). Owner.
- Timing-tier numbering, the emux-opcode policy and the wall-clock classification. ADRs.
- CI gating and cost under public visibility. Re-check.
- Ratify the clean-room reading rule for GPL and MAME-licensed material.

**Before Phase 3 (boot):**
- The post-IPL2 register, COP0, cache and RDRAM state libdragon IPL3 expects. ARCHITECTURE marks this UNKNOWN: n64brew documents what IPL3 leaves behind, not what IPL2 hands over.
- libdragon `boot/` per-file licensing. STACK reads Unlicense at the repository root; ARCHITECTURE says `boot/` states no separate license and file headers need checking. Resolve before vendoring binaries.
- IPL3 variant and revision: prod or compat; r7 or r8. r7 is embedded in nust64 builds and r8 is libdragon HEAD (both [measured] by STACK). Record md5 and sha256.
- IPL3 entropy sources (UNK in ARCHITECTURE and PITFALLS).
- libdragon continuation and maintenance owner (D3).
- ISViewer length-register semantics: nemu64-test writes at 0x14; FEATURES flags that libdragon treats 0x16 as a byte count where libultra treats it as a buffer-end pointer.

**Before Phase 4 (CPU):**
- NEC VR4300 manual check: TLB and XTLB vectors; CP0 write masks; CACHE operation semantics; cache line sizes and write policy; Count rate; FPU unimplemented-operation and FCR31 rules; LL and SC link-bit rules; delay-slot exception behavior. All currently INFER, COMMUNITY or UNKNOWN.
- Stepping and revision as a profile field (early-board `mul` bug; `sra` upper-bit leak; PRId differences).
- D2: nemu64-test rights. The Geneva 9 font provenance (PITFALLS); the generated ROM includes libdragon IPL3 (Unlicense) and compiled code from seven crates (STACK lists the crates, but none were rights-audited). Document rights, or keep the ROM local-only.
- nemu64-test reproducibility: two builds on different machines have not been compared (STACK UNKNOWN). Its output format at HEAD differs from its README ("Done! Tests: 262" in the README versus "Base: Failed N of M" in the source). The parser must be pinned to a SHA with negative controls. Single maintainer; four GitHub stars (FEATURES).

**Before Phases 5 and 7 (RSP, RDP, VI):**
- RSP DMA copy timing, dual-issue, contention and halt corruption (UNKNOWN in STACK and ARCHITECTURE).
- D5: Angrylion-Plus oracle use and redistribution of derived data. Not copyable under the allowlist.
- parallel-rdp conformance assets: a per-file audit of the modified Angrylion reference's license (FEATURES UNK).
- Video-domain clock values (NTSC, PAL and MPAL crystals; inter-domain phase). LOW confidence; record as a profile limitation (ARCHITECTURE).

**Before Phases 8 to 11:**
- PAL DACRATE values; AI cadence phase; 8 KiB carry test vectors.
- Joybus and EEPROM write timing; Controller Pak rules (community sources only).
- PeterLemon/N64: `BOOTCODE` and `IPL3Font` provenance (exclude until resolved). The LICENSE file was added 2025-09-27, so earlier history was unlicensed (STACK). ARM9/bass has no detected license, and upstream byuu bass licensing is unverified. Per-case golden-image provenance is missing. The 64drive claim is third-party and unverified per case.
- GCC, binutils and newlib notices for libdragon-built ROMs (STACK unverified).
- FlashRAM protocol (UNKNOWN; deferred).
- Routes for the 64DD and Transfer Pak content rights.

**Tracked, not research gaps:**
- LibFuturo contract recheck before any adapter. It is a sibling project with no N64 profile; do not shape the API around it.
- The "ubuntu-latest" migration: a third-party claim only (STACK UNKNOWN).
- Real CI time and cost: not yet measured. AUTONOMY.md requires measured CI-efficiency reviews.
- Name clearance (D7) and the release channel (D6).
- Core hardware constants from the owner pack (EMULATOR-DNA) have not been re-checked against Nintendo manuals.
- The libdragon archival cause: STACK's link to issue #1012 is community and unverified. Do not act on it.

## Sources

Aggregated from the four research files; each file has its full list. All accessed 2026-11-09. Licenses and repository state are mutable: re-check before any copy or pin.

**Primary (read directly; HIGH where the claim is a repository or license fact):**
- Licenses: ares-emulator/ares `LICENSE` (ISC for the core team, multi-license, per-file notices); mupen64plus-core `LICENSES` and `main.c` (GPL-2.0 or later); project64 `license.md` (GPL-2.0); ata4/angrylion-rdp-plus `MAME License.txt` (no root LICENSE; MAME terms); Themaister/parallel-rdp-standalone (MIT; README on intentional divergences and driver caveats); n64dev/cen64 (BSD-3); DragonMinded/libdragon `LICENSE.md` (Unlicense; archived 2026-10-01); thelemmy/nemu64-test (MIT); rust-n64/nust64 (MIT); PeterLemon/N64 (Unlicense, added 2025-09-27); ARM9/bass (no license detected).
- Repository and release state: nemu64-test HEAD `9a8b9f7d` (2026-09-08); n64brew/libdragon HEAD `98154daf` (2026-10-05); lemmy-64/n64-systemtest deprecation notice; ares v148 release notes (2026-05-30); gopher64 README and source; SDL 3.4.20; CMake 4.4.5; GitHub runner-image readmes and changelog; GitHub dependency-caching docs.
- Owner and project inputs: PROJECT.md, AGENTS.md, AUTONOMY.md. EMULATOR-DNA.md is cited by researchers and was not re-read for this synthesis.
- Local sibling checkouts (observed; not contracts): libfuturo (`docs/contract.md`, CMake 3.25, C17), nesturbator, muhtari, schwaystation, gabbaboy.

**Secondary (community or implementation observation; MEDIUM at best):**
- n64brew wiki: PIF-NUS, RSP, RDP, VI, AI, PI, Joybus, VR4300, MIPS III, ROM header.
- Test-suite READMEs and `Cargo.toml` feature notes (nemu64-test).
- Emulator and test source read for implementation observation: ares, cen64, gopher64, mupen64plus-core, dgb-n64.
- libdragon `boot/README` and its IPL3 md5 list.

**Tertiary (LOW; single source or search snippet; verify before use):**
- Clock-tree forum teardown (NTSC only).
- Secondhand wiki summary of cen64's stall (unconfirmed).
- The issue #1012 causal link for the libdragon archival (unverified).
- Third-party claim that `ubuntu-latest` moves to 26.04.
- gametechwiki test-results page (HTTP 403, not fetched); simple64 status (secondary).
- SaveTest-N64 and 64drive notes (search results).

---
*Research synthesized: 2026-11-09*
*Ready for roadmap: yes, with decision gates D1 to D7 and the "before Phase N" gaps above. Hardware-behavior requirements should wait for the primary-document checks in Phases 3 and 4.*