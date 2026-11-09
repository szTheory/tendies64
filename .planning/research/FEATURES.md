# Feature Research

**Domain:** Deterministic, accuracy-first Nintendo 64 emulator with evidence-bound support claims, headless-first core, developer/homebrew tooling and an embeddable integration route (Tendies64)
**Researched:** 2026-11-09 (all "accessed" dates below are 2026-11-09)
**Confidence:** MEDIUM overall. HIGH for what the surveyed projects ship today (read from their own READMEs, release notes and source trees). MEDIUM for "what a test ROM, libdragon or a commercial title needs from each hardware block" (community reference plus inference, not verified by running anything). LOW for any statement about the absence of a feature in a competitor that was not checked line by line (flagged inline).

## Evidence labels used in this file

`[owner]` owner direction in project files. `[primary]` primary-source fact (official docs, README, release notes, source code read directly). `[community]` community reference (wikis, forums, third-party guides). `[observation]` implementation observation (read from an emulator's source or CLI). `[inference]` my reasoning from the above. `[recommendation]` what I advise Tendies64 do. `[unknown]` not established. Nothing here was executed; no emulator, ROM or test was run, so no "passes" claim is made about any project.

## Headline conclusions (read this first)

1. **Do not claim a market gap.** `[observation]` A third-party ares fork (`HailToDodongo/ares-64`, last push 2026-10-26) already ships a deterministic, emulated-time, scripted headless runner (`ares-test`) with RSP/RDP stepping, `--dump-log` command-stream dumps, profiling and swappable Angrylion/parallel-RDP renderers. `[observation]` Mupen64Plus has a documented C core API and a debugger build flag; libretro cores embed it. `[observation]` ares has a GDB server and trace logger; Project64 has a large debugger. Each slice of Tendies64's candidate value has at least a partial incumbent. This is consistent with PROJECT.md's out-of-scope line on market gaps.
2. **The most credible differentiator is the combination plus the contract, not any single capability:** (a) support claims bound to a named profile + pinned build + rights-cleared fixture manifest; (b) a written determinism contract with a richer outcome taxonomy than "exit 0/1/2"; (c) versioned, machine-readable observation at guest-time boundaries; (d) an embeddable API with explicit lifetime/refusal postconditions proven by an installed-consumer test. `[inference]` I found no surveyed project that does (a) or (d) with that rigor; I did not do an exhaustive survey, so state this as "not found," never "does not exist."
3. **Table stakes depend on audience.** For the owner/developer/homebrew audience in PROJECT.md, table stakes are correctness of CPU/RSP/RDP/VI/AI/PI/SI behavior visible to test ROMs and libdragon, plus ISViewer logging and a headless runner. Player-facing table stakes (rewind, netplay, RetroAchievements, upscaling, texture packs) are real in the market but should be explicitly deferred; PROJECT.md says demand for players is not proven.
4. **libdragon is the practical homebrew target and it needs LLE.** `[primary]` libdragon's own README says its custom RSP/RDP code (rspq, rdpq, mixer, overlays) is developed against ares ("the only emulator that accurately emulates the hardware" per its README). `[inference]` Custom microcode cannot run under HLE graphics/audio plugins, so RSP LLE and a reference-accurate RDP are required for the homebrew path, not optional.
5. **Rights/availability flag found during research:** `[observation]` the GitHub repository `DragonMinded/libdragon` shows `archived: true` (GitHub API; last push 2026-09-14; the web page states "archived by the owner on Oct 1, 2026"). libdragon.dev did not mention this and I found no stated reason or new home. `[unknown]` whether development continues elsewhere. Pin by commit hash and mirror anything adopted (license is Unlicense per GitHub metadata; generated-ROM and asset rights still need per-fixture audit).

---

## Ecosystem snapshot (verified 2026-11-09)

| Project | Version / activity (observed) | What it is | Source label |
|---|---|---|---|
| ares | v148 released 2026-05-30; repo pushed 2026-11-09 | Multi-system, accuracy-focused; N64 CPU/RSP JIT rewritten in v148; no HLE; 64DD and Aleck64 present in tree | `[primary]` ares-emu.net/news/ares-v148-released, github.com/ares-emulator/ares |
| gopher64 | v1.1.42 released 2026-10-15; GPL-3.0; Rust; Windows/macOS(ARM)/Linux/Android | Player-oriented. README lists netplay, homebrew support, upscaling, CRT shader, CPU overclock, cheats, savestates, RetroAchievements. Source tree shows Transfer Pak, rumble, Controller Pak, SC64 cart, VRU modules. No debugger/GDB flag in CLI. | `[primary]` README; `[observation]` src tree and `src/lib.rs` CLI args |
| Project64 | repo pushed 2026-11-08; GPL-2.0; Windows only | Plugin-based; N64 + 64DD; built-in debugger/scripting | `[primary]` github.com/project64/project64 |
| Mupen64Plus core / RMG / Mupen64Plus-Next | core 2.6.0 (2024-07-15), repo pushed 2026-11-06; RMG v0.9.1 (2026-11-04); Next pushed 2026-10-29 | Core + plugin architecture with a documented core API; RMG is a Qt frontend; Next is a libretro core bundling GLideN64, parallel-rdp, angrylion, parallel-rsp, cxd4 | `[primary]` repos and READMEs |
| cen64 | BSD-3; repo last pushed 2025-10-26; hobby project | Aims at RTL-level accuracy; requires a user-supplied `pifdata.bin`; supports 64DD; has `debugger` and `gdb` source dirs; `-is-viewer` flag | `[primary]` README; debugger depth `[unknown]` |
| dgb-n64 (Dillonb/n64) | pushed 2026-11-05 | LLE, parallel-rdp, GDB stub, "reasonable amount of automated testing" as stated goal; "not ready for prime time" | `[primary]` README |
| simple64 | repo not found at expected URLs; secondary sources say archived 2025-02-14, superseded by gopher64 | Frontend over Mupen64Plus + parallel-rdp | `[community]` gametechwiki and news posts via search; not independently confirmed |
| BizHawk | 2.11.1 (2026-05-01) | TAS-oriented frontend; its N64 core is Mupen64Plus-derived | `[primary]` releases; accuracy and determinism commentary is `[community]` TASVideos forum posts, dated |
| Ares 64 fork | pushed 2026-10-26; ISC-style header | N64-only ares fork for developers (see developer table) | `[primary]` github.com/HailToDodongo/ares-64 README |

Cross-checks: PROJECT.md's ecosystem paragraph (ares v148, compatibility table, Project64 plugins, Mupen64Plus split) matches what I observed. `nemu64-test` pushed 2026-09-08 with 4 GitHub stars (single-maintainer risk); the older `n64-systemtest` README says it is deprecated in favor of it `[primary]`.

---

## Part 1. Hardware-capability features

"Needs" columns: (a) ROM-level test suites such as nemu64-test, (b) libdragon homebrew, (c) commercial titles. Entries are `[inference]` from the sources named unless marked. Complexity is relative implementation + verification cost for an accuracy-first, deterministic core.

Tier key (recommendation): **P1** = in the first profile or its immediate verification; **P2** = early follow-on; **P3** = later/profile-promoted; **X** = out of scope.

| Feature | Complexity | Depends on | (a) test ROMs | (b) libdragon | (c) commercial | Tier / notes |
|---|---|---|---|---|---|---|
| VR4300 integer core: MIPS III, 64-bit ops, delay slots, LL/SC, alignment/address errors | HIGH | memory map, exceptions, scheduler | Required. nemu64-test covers 64-bit MFC0/DMFC0/MTC0/DMTC0 on EntryHi/BadVAddr, LLD/LD/SC/SCD, overflow/unaligned/TRAP/BREAK/SYSCALL exceptions `[primary]` | Required. libdragon's toolchain is 64-bit capable (README) `[primary]` | Required (32-bit libultra ABI) | P1. ares exposes unaligned-address exceptions as an accuracy flag `[observation]` accuracy.hpp |
| CP0 + exceptions + interrupt routing (MI) | HIGH | CPU, MI, scheduler | Required (cop0hazard group is optional/experimental `[primary]`) | Required | Required | P1 |
| TLB / virtual memory | HIGH | CP0 | Required (nemu64-test TLB group `[primary]`) | Used by libdragon dl/dso loading? `[unknown]` | Used by some titles `[community]` | P1 for test ROM; Project64 notes TLB-exception breakpoint caveats for TLB-using games `[primary]` hack64 |
| Instruction/data caches (visible effects) | MED-HIGH | CPU, bus/DMA ordering | Partly (timing/cycle groups optional) | Likely: libdragon ships `tests/test_cache.c` `[observation]` and requires explicit cache maintenance around DMA | Some titles rely on stale-cache effects `[community]` | P2 behind explicit profile switch. ares has `dcache.cpp` `[observation]`. Decide early whether "caches as needed" is modeled or approximated; it is a determinism-visible choice |
| FPU (COP1): 32/64-bit modes, rounding, denormal/exception behavior | MED-HIGH | CPU, CP0 | Yes, FPU compare/convert corners fail often `[community]` (another emulator's README reports 28/46 on PeterLemon CP1) | Yes (libdragon `tests/test_cop1.c`) `[observation]` | Required | P1/P2 |
| Cycle/timing accuracy (instruction timing, bus contention) | VERY HIGH | everything | Optional groups (timing, cycle) `[primary]` | Rarely | A few titles | P3. ares JIT interleaves up to 4096 CPU cycles without sync `[observation]`; cen64 aims RTL-level `[primary]`. A guest-time scheduler design decision in P1 constrains this later |
| Device scheduler / guest time / DMA ordering | HIGH | all devices | Needed for deterministic results | Needed | Needed | P1. Foundation for determinism claim and boundary tracing |
| RDRAM (4 MB base, 8 MB with Expansion Pak) + RI/RDRAM regs | MED | memory map | Required | Required; libdragon detects memory size at boot `[inference]` | Required (Expansion Pak titles) | P1 base; Expansion Pak as separate named profile `[owner]` |
| PI: cartridge domain, DMA, timing | MED | scheduler, RDRAM | Required | Required | Required | P1 |
| ISViewer debug window at PI 0x13FF0000 | LOW | PI | Required for the result protocol. nemu64-test buffer 0xB3FF0020 to 0xB3FF0220, length register 0xB3FF0014 triggers print `[primary]` | Required for `debugf` in emulators `[primary]` libdragon debug.h | n/a | P1. **Protocol hazard:** ares source comment says libdragon treats offset 0x16 as a byte count while libultra treats it as a buffer-end pointer; ares forces compatibility `[observation]`. Specify exactly which semantics Tendies64 implements |
| SI + PIF + boot (no Nintendo bytes), CIC seed/entrypoint handling | MED-HIGH | PI, CPU, RDRAM | Required (libdragon open IPL3 boots nemu64-test `[primary]`) | Required | Required | P1 via open IPL3 + documented HLE boot state. CIC variants (n64brew `[community]`): 6102/7101 most titles; 6101, 7102, 6103/7103, 6105/7105, 6106/7106 differ in entrypoint mangling, ciphering, RSP use and challenge; only 6105/7105 need the full challenge algorithm. Deferring non-6102 CIC variants to later profiles is defensible. Rights stop on Nintendo PIF bytes applies. `[observation]` gopher64 source embeds a 1984-byte PIF ROM array and a 6105 challenge routine; its provenance/rights are `[unknown]`; do not copy |
| MI (interrupts), SP regs, DP regs | MED | CPU, RSP, RDP | Required | Required | Required | P1/P2 |
| RSP scalar unit + IMEM/DMEM + SP DMA + halt/break/semaphore | HIGH | MI, RDRAM | nemu64-test RSP group `[primary]` | Required (rspq/overlays) `[primary]` README | Required | P2 |
| RSP vector unit (COP2): 128-bit regs, 48-bit accumulator, all VU ops incl. quirks | VERY HIGH | RSP scalar | Required (nemu64-test has vmulf/vmulu stress tests as optional long-running groups `[primary]`) | Required | Required | P2. Opcode coverage is not microcode compatibility (EMULATOR-DNA) `[owner]` |
| RSP microcode strategy: **LLE only** | n/a | RSP | LLE | LLE (custom ucode) | LLE works for all; HLE only for stock ucodes | P2: LLE. See Anti-features for HLE |
| RDP LLE: command processor, rasterizer, TMEM, combiner, blender, coverage, 1/2-cycle, copy, fill | VERY HIGH | RDRAM, DP regs, RSP (XBUS/DMEM source) | Needed for RDP groups (optional/experimental in nemu64-test) | Needed for rdpq | Needed | P3 (after raw framebuffer contract exists). Reference approach: CPU software renderer that is bit-exact to a documented reference; GPU renderers (parallel-rdp) are host-side and must be conformance-checked against it. `[primary]` parallel-rdp-standalone "aims to be bitexact with the Angrylion-Plus reference"; its tests use a fork of the reference modified "to make certain undefined behavior deterministic," and noise is not expected to match. Implication: "matches Angrylion" is an agreement claim with stated caveats, not hardware truth |
| VI: scanout, NTSC/PAL/MPAL timing, resolution registers, interlace/serrate, AA/divot/dither/gamma filters | MED (raw scanout), HIGH (filters) | RDRAM, RDP | Optional | Needed (libdragon display, resolution modes) | Needed | P3. Keep three outputs distinct: RDP-written RDRAM framebuffer, VI post-filter scanout, host-scaled image `[owner]` |
| AI: DMA, DAC rate, buffer double-buffering, interrupt cadence | MED | scheduler, MI, RDRAM | Optional (PeterLemon sound tests) `[primary]` | Needed (mixer) | Needed | P3. ares v148 fixed audio pops at AI DMA end `[primary]`, showing this area still moves |
| Save: EEPROM 4 Kbit / 16 Kbit (Joybus protocol on SI) | MED | SI/PIF | n/a | Used (eeprom API) `[inference]` | Many titles | P2. ares v148 added EEPROM write timing and status flags `[primary]` |
| Save: SRAM (PI domain 2) | LOW-MED | PI | n/a | Possible | Many titles | P2 |
| Save: FlashRAM 1 Mbit (command state machine, chip ID) | MED-HIGH | PI | n/a | Possible | Some titles (Majora's Mask variance noted by cen64 README `[primary]`) | P2/P3 |
| Save-type selection | n/a | | | | | **Explicit device selection with refused-operation postconditions** `[owner]`. SRAM and FlashRAM share address space, so conflicts must be refused `[community]` SaveTest-N64 README via search; ares uses an internal ROM database ("mia") `[primary]` README. Do not infer from title strings |
| Controller (standard) over SI/PIF Joybus | MED | SI/PIF, scheduler | Optional | Needed | Needed | P2 |
| Controller Pak (32 KB, pak protocol, CRC) | MED | Controller | n/a | Used | Many titles | P3 profile `[owner]` |
| Rumble Pak | LOW-MED | Controller | n/a | Used | Many titles | P3 profile |
| Transfer Pak (32-byte transfers; embeds a GB cart/machine) | HIGH | Controller + GB emulation + separately cleared GB content | n/a | Rare | Few titles | P3+; gopher64 ships it `[observation]` (src/device/controller/transferpak.rs, gbcart.rs) |
| Other pak/peripherals: VRU, mouse, Bio Sensor | LOW-MED each | Controller | n/a | n/a | Niche | P3+; gopher64 has a VRU module, ares lists Bio Sensor `[observation]` |
| Expansion Pak (8 MB map) | LOW | RDRAM | optional | detected | Some titles require | Separate named profile `[owner]` |
| RTC (cartridge Joybus RTC) | LOW | SI | n/a | n/a | Very few | P3; 64DD also has a separate RTC `[owner]` |
| 64DD (disk media, writable areas, IPL, RTC, expansion) | VERY HIGH | PI, SI, RDRAM, rights-cleared disk fixture | n/a | n/a | Few | P3+ later system profile `[owner]`. ares and cen64 support it; ares models 64DD variants as named systems `[observation]` system.cpp |
| Aleck 64 (arcade), iQue | HIGH | | | | | X (out of scope). ares has `aleck64` in tree; libdragon supports iQue `[primary]` |
| Per-region profiles (NTSC/PAL/MPAL) | MED | VI timing, boot | optional | Needed for PAL homebrew | Needed | Named profile variants; OS TV type vs cartridge region are separate `[owner]` |

### Reading of the above for "what the first profile realistically needs"

- **Test-ROM tier (nemu64-test CPU/COP0/TLB/access widths):** integer CPU, CP0, TLB, PI, SI/PIF boot via libdragon IPL3, ISViewer, scheduler. No RDP/VI/AI needed (nemu64-test decides pass/fail itself and reports through ISViewer registers; empty screen means the run did not finish `[primary]`). RSP group needs the RSP.
- **libdragon tier:** adds RSP LLE (vector), RDP reference rendering, VI, AI, controller, saves, plus ISViewer `debugf`. Also needs a rights-cleared libdragon-built fixture (libdragon examples/tests are Unlicense at repository level; generated ROM and asset rights still need manifest entries `[inference]`).
- **Commercial tier:** adds CIC variants, caches/timing where games depend on them, all save types, paks, Expansion Pak, broad RDP/VI edge cases. Not claimable from a boot screenshot or one suite `[owner]`.

---

## Part 2. Host/user features

Audience reality: these are what players of ares, gopher64, Project64 and RMG expect. For Tendies64 they sit on top of the core and never change guest behavior `[owner]`.

| Feature | Market status (observed) | Complexity | Depends on | Tendies64 recommendation |
|---|---|---|---|---|
| Frontend / window / audio out / keyboard+gamepad | ares, gopher64, PJ64 (Windows), RMG all have it | MED | raw video/audio outputs, host timing | P2: one minimal reference host, enough to play the cleared homebrew title. Not a full launcher |
| Input mapping UI and profiles | gopher64 CLI has profile creation, per-port binding, `--list-controllers`; ares per-core mappings (v148) `[observation]` | MED | frontend | P3 host feature; core only sees timed logical input `[owner]` |
| Save states | gopher64 (slots 0-9, `--load-state`), ares (`--save-state slot` 1-9), PJ64, m64p (F5/F7 slots) | MED-HIGH | complete serializable state, versioned format | Core-level serialization is a **P2 core feature** (needed for replay checkpoints and tests), user-facing slots are P3. Versioned, profile- and build-bound format; refuse mismatches |
| Rewind | ares ("rewind and fast-forward" on its site) | MED | save states, deterministic stepping | P3 host feature; cheap once serialization is fast |
| Fast-forward | ares, others | LOW | core has no wall clock | Trivial for a time-free core; host paces |
| Run-ahead (latency reduction) | ares markets it | MED | fast savestates | P3+; any latency claim must be a measured, named workload `[owner]` |
| Cheats (GameShark/code lists) | gopher64 (`--cheats`), m64p (GameShark key, if enabled), ares tool | MED | CPU hooks or host memory writes at guest-time boundaries | P3. Implement only as explicit, logged host writes at defined boundaries so they cannot silently taint claims |
| Netplay | gopher64 (P2P + cloud servers), RMG (rmg-netplay-server, port forwarding), m64p (`NETPLAY=1` build flag) | HIGH | determinism, input-delay protocol, save-state sync | P3+; Tendies64's determinism contract is a prerequisite and a long-term enabler `[inference]`. Not a v1 feature |
| Upscaling / HD resolution | ares (`Quality` SD/HD/UHD via parallel-rdp), gopher64, parallel-rdp | MED | GPU RDP renderer, Vulkan | P3+ host renderer, never the canonical output path |
| Widescreen | gopher64 `--widescreen`; GLideN64 hacks | MED-HIGH | per-title hacks, projection | Anti-feature in core (see Part 5); possible later host option clearly labeled outside claims |
| Texture packs / hi-res textures | GLideN64-based stacks (Mupen64Plus-Next) `[community]` | HIGH | HLE-style texture interception | Anti-feature for the evidence core; defer indefinitely |
| RetroAchievements | gopher64 native (2026); Project64/RMG status conflicting in sources `[community]`, RALibretro doc page described as "wildly out-of-date" | HIGH | memory map exposure, rcheevos, hardcore-mode restrictions (disabled features) | P3+; large dependency and policy cost against the small-dependency rule `[owner]`. Likely arrives via libretro/adapter rather than first-party |
| libretro core | Mupen64Plus-Next, parallel-n64 | MED-HIGH | core API, serialization, frame-based run loop | A later adapter over the embeddable API, not a design driver. The libretro run-one-frame model conflicts with a guest-time-bounded API unless adapted deliberately |
| Per-game config / compatibility DB shipped with emulator | ares (internal "mia" ROM database), m64p ini | MED | hashes, rights | Anti-feature in the core; see Part 5 |

---

## Part 3. Developer features

### Survey matrix (observed 2026-11-09)

| Capability | ares (upstream) | Ares 64 fork | gopher64 | Project64 | cen64 | Mupen64Plus core | libdragon (target side) |
|---|---|---|---|---|---|---|---|
| Headless / no-window runner | No headless flag in v148 CLI listing; `--kiosk` removes UI but still a UI app `[observation]` | **Yes: `ares-test`**, JS-scripted, no window/GPU/audio, Angrylion CPU renderer or `none` `[primary]` | No (GUI/CLI app; CLI is launch + input config) `[observation]` | No (Windows GUI) | Console app `[unknown]` depth | Console frontend exists; headless not a documented goal `[unknown]` | n/a |
| Deterministic time | "Deterministic Entropy" setting exists `[observation]` system.cpp | Yes, emulated-time waits on CPU clock; entropy forced on `[primary]` | Netplay implies deterministic stepping `[inference]` | No stated guarantee | RTL-style aim `[primary]` | No stated guarantee; TAS community reports desyncs in older builds `[community]` | n/a |
| Machine-readable result protocol | No. ISViewer text is routed to the tracer sink only when tracing is enabled `[observation]` | Exit codes 0 (ran to end), 1 (uncaught JS exception = assert), 2 (usage/ROM-not-found/timeout); `ares.screenshot().sha256`; ROM ISViewer log accessible in script `[primary]` | ISViewer prints to stdout `[observation]` is_viewer.rs | No | `-is-viewer` flag prints ISViewer output (per libdragon header) `[primary]` | No | `debugf` = fprintf(stderr) mapped to ISViewer/USB `[primary]` |
| Watchdog | n/a | **Wall-clock** `--timeout` (default 120 s) `[primary]` | n/a | n/a | n/a | n/a | n/a |
| Input movies / replay | `[unknown]` not verified upstream | Script-driven timed input, not a movie format `[observation]` | Not found | Not found | Not found | m64 movie format exists in the ecosystem (BizHawk converts it); core exposes an input callback in the TAS fork `[community]` | n/a |
| Debugger: breakpoints / stepping | GDB server; CPU/RSP debugger nodes in tree `[observation]` | RDP/RSP single-step; CPU via ares GDB `[primary]` | None found | **Yes**: PC, register, memory R/W, exception, conditional via script; step/over/go; CPU log; DMA log; stack trace `[primary]` hack64 manual | `debugger` + `gdb` dirs exist `[observation]` | Debugger API built with `DEBUGGER=1`, "runs slower" `[primary]` | GDB support documented against ares (port 9123) `[primary]` libdragon wiki via mirror |
| GDB stub | Yes; multi-byte write handling was a listed to-do in a search snippet, and a v148 fix touched the RCP/PI read path `[community]/[primary]` | via ares | No | No | Yes `[community]` | No | libdragon documents VS Code and CLion setups; breakpoints implied, watchpoints and memory inspection not documented `[primary]` |
| Watchpoints | via GDB `[unknown]` | `[unknown]` | No | Yes (memory R/W breakpoints; write BPs also catch PI DMA from ROM) | `[unknown]` | `[unknown]` | n/a |
| Disassembly | CPU/RSP disassemblers in source `[observation]` | Yes, RSP command decode with libdragon ELF | n/a | Yes | `[unknown]` | Yes (dbg_decoder) `[observation]` | n/a |
| Tracing | Trace Logger: per-component tracers, each with prefix, terminal, file and instruction-mask toggles `[observation]` tracer.cpp | Adds RSP/RDP logs and a flame chart | No | CPU Log (very slow), DMA Log | `[unknown]` | `[unknown]` | n/a |
| Memory/register inspection | Memory editor tool, graphics viewer, properties `[observation]` | Memory viewer/editor, TMEM viewer, framebuffer color/coverage/depth views, overdraw heatmap | No | Memory viewer, scanner, dumper, symbols | `[unknown]` | dbg_memory | n/a |
| Frame / audio capture for tests | Screenshots (UI) | `ares.screenshot()` with sha256, audio recording `[primary]` | Not found | Not found | Not found | Screenshot only | n/a |
| ELF/symbol awareness | n/a | DWARF-based profiling; libdragon ELF used for RSP overlay names | n/a | None built in; needs script or external n64sym `[primary]` | n/a | n/a | Symbolized stack-trace error screens on crash `[primary]` |
| "Homebrew mode" strictness | Yes; libdragon README recommends enabling it for developer-specific checks `[primary]` | Yes (`setHomebrew`) | "Homebrew support" in README `[primary]` | n/a | n/a | n/a | n/a |
| ISViewer / IS64 log | Yes (PI 0x13FF0000 range) | Yes | Yes (stdout) | `[unknown]` | Yes with flag | `[unknown]` | `debugf` emulator backend: lists cen64, ares, simple64, dgb-n64 `[primary]` |

### Developer-feature recommendations for Tendies64

| Feature | Class | Complexity | Depends on | Notes |
|---|---|---|---|---|
| Headless runner with fixed reset, timed logical input, guest-time and/or instruction limit, six-way outcome taxonomy (pass, guest-fail, boot-fail, timeout, missing-fixture, harness-error) | Table stakes for this project `[owner]` | MED | scheduler, ISViewer, fixture manifest | Better than ares-test's exit 0/1/2 because a wrong or empty result must not pass. Watchdog is host-side wall clock only as a backstop; the authoritative limit is guest time `[recommendation]` |
| ISViewer result capture with a precisely specified register contract | Table stakes | LOW | PI | Specify both the nemu64-test length-register trigger at offset 0x14 and the libdragon 0x16 byte-count behavior; add negative controls |
| Machine-readable result protocol (JSON lines or equivalent, versioned, with profile id, build id, fixture digest, outcome, guest-time elapsed) | Differentiator | LOW-MED | headless runner | No surveyed project emits one that I found (`[inference]`, not exhaustive). Cheap relative to value; must be versioned |
| Deterministic replay / input movies bound to profile + build + content hash, with integrity check | Differentiator | MED | save-state-free deterministic core, timed input | m64/BizHawk movies exist but are per-emulator and carry desync folklore `[community]`. Replay as a CI artifact is the angle |
| Tracing at guest-time boundaries (retired-instruction batches, DMA start/finish, RSP task start/halt, RDP list submit/sync-full, VI scanout, AI DMA, interrupt raise/ack) as a stable, diffable event stream | Differentiator | MED-HIGH | boundaries defined in core design | ares tracers are per-component text with UI toggles; ares-64 `--dump-log` is TSV of RSP/RDP commands per frame; PJ64 logs are GUI-bound. A versioned, boundary-defined stream was not found `[inference]` |
| Raw frame and audio capture API with hashes (pre-VI RDRAM framebuffer, post-VI scanout, PCM from AI DMA) | Table stakes (for the headless story), differentiator in precision | LOW-MED | VI, AI | Keeps "output hash proves repeatability, not accuracy" explicit `[owner]` |
| GDB remote stub (CPU registers, memory, breakpoints, watchpoints, single-step) | Table stakes for developer credibility | MED | stable stepping boundary | ares and dgb-n64 have it; libdragon documents ares GDB. Place after CPU core is stable `[recommendation]`. Define behavior when stopping mid-DMA |
| Disassembly and symbol-aware memory/register inspection | Table stakes (developer) | MED | ELF reader (parser is untrusted input `[owner]`) | Reading ELF/DWARF is a parser attack surface; bound sizes and refuse malformed input |
| libdragon debug support: `debugf` via ISViewer, assertion error screens visible, symbolized backtrace-friendly state exposure | Table stakes for the homebrew path | LOW (given ISViewer) | ISViewer, saves, VI | `assertf` draws an error screen then also logs to stderr, so ISViewer capture is enough to make assertions observable `[primary]` |
| Homebrew mode (strict checks for unaligned access, bad DMA, uncached/cached aliasing misuse) | Differentiator for homebrew | MED | policy decision | ares offers it and libdragon recommends it. Define which diagnostics are guest-visible vs host-visible |
| RSP/RDP command-stream dump and RDP-dump replay | Differentiator | MED | RDP | Format of parallel-rdp-standalone dumps is "RDRAM changes plus RDP command streams" `[primary]`; reusable for conformance tests if license/rights are audited |
| Scripting (JS/Lua) for emulator control | Defer | MED | stable API | ares-64 uses QuickJS-NG; PJ64 JS; BizHawk Lua. Adds a dependency and surface. The embeddable API plus a thin external script is the cheaper route `[recommendation]` |
| Profiling / flame chart | Defer | HIGH | symbols, boundaries | Ares 64 fork already covers; revisit only if evidence of demand |

### Gaps identified (with evidence status)

1. **No upstream ares headless runner (observation, v148 CLI).** Filled partially by the Ares 64 fork, whose README says it is a WIP developer fork that must be rebased onto upstream with merge conflicts "will happen" `[primary]`. Gap is "upstream integration and stability contract," not "nobody built it."
2. **No documented machine-readable outcome taxonomy** in ares-test beyond three exit codes `[primary]`.
3. **Wall-clock watchdog inside ares-test** means a slow host can turn a correct run into exit 2 `[inference]`; a guest-time bound avoids that.
4. **ISViewer semantic divergence between libultra and libdragon** is handled by a hack in ares `[observation]`; no written specification found `[inference]`.
5. **GDB depth is uncertain:** libdragon's docs show backtrace and continue and troubleshooting for old ares builds; watchpoints/memory inspection are not documented there `[primary]`; a Ghidra discussion reported difficulty but is undated `[community, LOW]`.
6. **Input-movie determinism** across emulators is community folklore (BizHawk/Mupen desync reports, dated 2014-2016) `[community, LOW]`.

---

## Part 4. Accuracy and evidence features

| Feature | Existing pattern | Complexity | Notes for Tendies64 |
|---|---|---|---|
| Versioned compatibility table | ares-emu.net table: columns Title, Status, Version, Notes, GitHub Issues; statuses Completable / In-Game / Starts / Nothing / Unknown; "Unverified" badge; each row carries the ares version it was tested on; roughly 900 entries (46 pages observed; count not stated) `[primary]` | MED (process, not code) | Pattern is per-title and per-emulator-version. Tendies64's unit is **profile x build x evidence tier**; a title row is only supportable after a named profile exists. The table is "not an independent oracle" `[owner]` |
| Test-suite pass reporting | n64-systemtest results were reported in ares release notes (2022: 563 tests, 3 failed) `[community]` search snippet; gametechwiki has a cross-emulator test-ROM results page (could not fetch; 403) `[community, LOW]` | LOW-MED | Report pass counts per pinned fixture commit, per profile, with outcome taxonomy; never aggregate across profiles |
| Fixture manifest (source, revision, build recipe, digest, profile, rights, expected-result provenance) | Not found in surveyed emulators `[inference]` | LOW-MED | Already required by PROJECT.md; a real differentiator in rigor if it is machine-checked in CI |
| Negative controls in the harness (wrong expected output fails; missing/malformed/timeout not an empty pass) | Not found as stated practice `[inference]` | LOW | nemu64-test says an empty screen means the run did not complete `[primary]`; build the harness so that is a first-class failure |
| RDP conformance against Angrylion | parallel-rdp-standalone: ~150 fuzz tests in `rdp-conformance`, a VI conformance suite, `rdp-validate-dump` for headless dump comparison, `rdp-replayer` live comparison; MIT license; last push 2024-11-09 `[primary]` | MED-HIGH | Differential evidence, not hardware truth `[owner]`. Record reference implementation version and the deterministic-UB modifications. Listed deviations: first-cycle CombinedColor/Alpha clearing, 2-cycle alpha test using current pixel, no color-combiner chroma keying, TMEM-upload edge cases `[primary]` |
| Hardware-derived vectors | PeterLemon author reports 64drive testing; n64-systemtest/nemu64-test state "hardware quirks" `[primary]` | MED | Use only with provenance (console revision/region, flashcart, fixture hash, capture method) `[owner]`; the owner stop on physical measurement still applies |
| Evidence tiering for claims | None found as a product feature | LOW (docs + schema) | Define tiers (e.g., component test / ROM-level suite / differential vs reference / hardware-derived / game-level) so statements never blur |
| Accuracy flags / "Reference" mode | ares has a `Reference` accuracy master switch and per-component interpreter/recompiler and SISD/SIMD switches `[observation]` | MED | Tendies64 should hold the interpreter/reference path as the claim carrier and treat any JIT or SIMD as an optimization that must produce identical observable results `[recommendation]` |

---

## Feature Landscape (template view)

### Table Stakes (must have or the right audience leaves)

Scoped to Tendies64's stated audience (owner, emulator/homebrew developers, then players).

| Feature | Why Expected | Complexity | Notes |
|---|---|---|---|
| Named first machine profile and explicit profile selection (console revision, TV type, 4 MB RDRAM, boot route, cartridge-only) | Core value `[owner]` | LOW-MED | ares has named models (N64 NTSC/PAL, 64DD NTSC-U/J/DEV) plus settings (Expansion Pak, Homebrew Mode, Deterministic Entropy, Quality) `[observation]`; Tendies64 adds evidence binding |
| Accurate VR4300/CP0/TLB/exception core passing a pinned ROM-level suite | Any N64 emulator must; test ROMs are the entry credential | HIGH | nemu64-test CPU/COP0/TLB/access-width groups `[primary]` |
| Boot without Nintendo bytes (libdragon IPL3 or documented HLE boot) | Rights stop `[owner]` | MED | cen64 requires user `pifdata.bin` `[primary]` (a boundary Tendies64 deliberately avoids) |
| Headless runner with ISViewer capture and outcome taxonomy | Needed for CI and the dev audience | MED | See Part 3 |
| RSP LLE (scalar + vector) | libdragon and any LLE claim | VERY HIGH | |
| RDP reference rendering + VI raw scanout, raw framebuffer output | Homebrew needs rdpq; separated from host presentation | VERY HIGH | |
| AI raw audio with DMA/cadence | Homebrew mixer | MED | |
| Save media with explicit device selection and refusal postconditions | Games/homebrew need it; PROJECT.md requirement | MED-HIGH | EEPROM 4/16K, SRAM first |
| Standard controller input as timed logical input | Needed for play and replay | MED | |
| Raw frame/audio capture with hashes | Needed to observe results | LOW-MED | |
| GDB stub + ISViewer/`debugf` visibility | Developer credibility (ares, dgb-n64 offer GDB) | MED | |
| One playable, rights-cleared homebrew path and a minimal host | Prevents tests-only drift `[owner]` | MED | libdragon examples are a candidate; audit assets |

### Differentiators (competitive advantage, each hedged)

| Feature | Value Proposition | Complexity | Evidence and caveat |
|---|---|---|---|
| Evidence-bound support claims per named profile (profile id + pinned build + fixture manifest + evidence tier, machine-checked in CI) | Users and integrators can see exactly what is claimed and why | MED | ares table is per title + version with "Unverified" markers `[primary]`; no profile-bound, artifact-linked scheme found. `[inference]` not exhaustive |
| Written determinism contract + guest-time authoritative limits | Reproducible CI and replay; same inputs yield identical outputs | HIGH (discipline) | ares-test already demonstrates emulated-time determinism in a fork; the differentiator is making it a *core invariant with a spec*, not a mode |
| Rich outcome taxonomy and versioned result protocol | Distinguishes "failed" from "did not run" | LOW-MED | ares-test: three exit codes `[primary]` |
| Guest-time-boundary trace/event stream, versioned and diffable | Debugging and differential testing across builds | MED-HIGH | Existing tracers are UI-bound text or per-frame command logs `[observation]` |
| Embeddable core API with lifetime/refusal postconditions + installed-consumer test | Independent integration route (including a possible LibFuturo adapter) | HIGH | Mupen64Plus core API and libretro cores already offer embedding `[primary]`; differentiator is instance isolation, explicit failure postconditions, and an installed-consumer test. m64p global-state status is `[unknown]`; verify before comparing |
| RDP/VI conformance reporting against a recorded reference (dump replay, per-test digests) | Quantified RDP agreement with stated limits | MED-HIGH | Reuses parallel-rdp suites if license-audited `[primary]` |
| Replay/input-movie artifacts bound to profile, build and content hash | Regression assets that fail loudly on mismatch | MED | Exists in other forms (m64, BizHawk) `[community]` |
| Fixture/rights manifest as a product artifact | Trust for downstream users and CI | LOW-MED | Not found elsewhere `[inference]` |
| Owner-controlled implementation with small flat dependency tree | Supply-chain and maintenance clarity | n/a | Owner direction; gopher64's Rust tree and ares-64's QuickJS/SDL3/imgui show the dependency pull of dev tooling |

### Anti-Features (commonly requested, deliberately not built, or deferred)

| Feature | Why Requested | Why Problematic | Alternative |
|---|---|---|---|
| HLE graphics/audio RSP microcode paths and GLideN64-style HLE rendering in the canonical core | Speed, upscaling, widescreen, texture packs | Breaks the evidence chain: opcode/task behavior is no longer observable at RSP/RDP boundaries; custom microcode (libdragon) is unsupported; claims would not transfer `[inference]` | LLE core; if ever added, a separately named profile and host layer that cannot inherit LLE evidence |
| Per-game hacks database in the core | Compatibility for broken titles | Masks core bugs, defeats profile-bound claims, adds title-string inference (explicitly rejected for saves) `[owner]`; rights tangles around hash databases | Fix root cause; keep any external metadata optional, outside the core, with provenance |
| Bundled PIF/IPL/CIC, commercial ROMs, saves, 64DD media | Out-of-box play | Owner rights stop `[owner]` | Open IPL3 / documented HLE boot; user supplies content at the host layer |
| Plugin architecture (Project64/Mupen64Plus style) | Familiar swap-a-renderer model | Splits one deterministic machine across ABI seams and weakens evidence; explicitly out of scope `[owner]` | One canonical core with raw-output interfaces and optional host renderers |
| CPU overclocking option | gopher64 ships it `[primary]`; reduces slowdown | Changes guest time; invalidates timing evidence | If wanted, a separately named profile |
| Wall-clock watchdogs or host timers inside the core | Simple hang protection | Violates "no host clock feeds guest behavior" `[owner]` | Guest-time/instruction limits in core; wall clock only in the host runner as a CI backstop |
| Cheats, widescreen, texture packs inside the machine | Popular | Mutate guest memory/render path invisibly | Host-layer, logged, outside claim scope; defer |
| Broad compatibility claims from boot screenshots, one suite or agreement with another emulator | Marketing | Not evidence `[owner]` | Evidence-tiered per-profile claims |
| Embedded telemetry, accounts, ROM scrapers/library manager | Feature creep | Privacy, scope, rights | None; defer launcher concerns |
| JIT as an early priority | ares v148 shows large gains `[primary]` | Optimization before a repeatable correctness baseline `[owner]` | Interpreter/reference first; JIT later must match interpreter-observable behavior bit-for-bit under test |
| Aleck64, iQue, Retro Regen suite vendoring | Coverage | Out of scope or forbidden terms `[owner]` | None |

### Deferred (not anti-features, just later)

Rewind, run-ahead, netplay, upscaling renderer, RetroAchievements, libretro adapter, input-mapping UI polish, 64DD, Transfer Pak, RTC, cycle-exact timing, flame-chart profiling, scripting runtime.

---

## Feature Dependencies

```
Scheduler / guest time ──requires──> everything below
    ├──> VR4300 + CP0 + TLB + exceptions ──> MI interrupts
    │        └──> PI + RDRAM ──> ISViewer ──> headless runner + outcome taxonomy ──> CI result protocol
    │                │                                   └──> nemu64-test in CI (needs fixture manifest + rights audit)
    │                └──> SI/PIF open-IPL3 boot ──> controller input ──> timed logical input ──> replay movies
    ├──> RSP scalar ──> RSP vector ──> RSP LLE microcode tasks
    │        └──> RDP command processing ──> raw framebuffer ──> VI scanout ──> frame hashes
    │                 └──> RDP reference conformance vs recorded reference
    ├──> AI DMA ──> raw PCM ──> audio hashes
    └──> Save devices (EEPROM/SRAM/Flash) ──> refused-operation postconditions

Core serialization (versioned) ──enables──> save states, replay checkpoints, rewind, run-ahead, netplay
Boundary definitions in core ──enables──> trace stream, GDB stub, breakpoints/watchpoints
Public core API (lifetime, refusal, raw outputs) ──requires──> all of the above being observable ──> installed-consumer test ──> LibFuturo/libretro adapters
Minimal host (window/audio/input) ──requires──> raw video/audio outputs + timed input ──> playable homebrew path
HLE microcode / widescreen / texture packs ──conflicts──> evidence-bound claims and LLE observability
Overclock / wall-clock limits in core ──conflicts──> determinism contract
```

### Dependency notes

- **ISViewer before everything testable:** nemu64-test and libdragon `debugf` both depend on it, and libdragon vs libultra semantics differ at offset 0x16 `[observation]`.
- **RSP before RDP in practice:** RDP lists come from the RSP or CPU; libdragon rdpq programs RDP through RSP overlays `[inference]`.
- **Serialization is not just a user feature:** replay checkpoints, rewind and netplay need it. Design the state model early even though the UI arrives late.
- **GDB stub needs a defined stopping boundary** (what happens to in-flight DMA, RSP and RDP when the CPU halts), which falls out of the scheduler design.
- **Profile promotion order from PROJECT.md:** cartridge first; 64DD only after cartridge verification and a rights-cleared disk route `[owner]`.

---

## MVP Definition

### Launch With (v1: first verifiable profile; owner/developer audience)

- [ ] Named first profile and profile/build identity attached to every result
- [ ] Deterministic scheduler with guest-time limits; no host clock, file or input dependence in the core
- [ ] VR4300 + CP0 + TLB + exceptions + memory/PI/SI/MI with native component tests that have independent expected values
- [ ] Open-IPL3 or documented HLE boot with stated limits; no Nintendo bytes
- [ ] ISViewer capture with precise protocol, headless runner, six-way outcome taxonomy, machine-readable result protocol, negative-control tests
- [ ] Pinned, rights-audited nemu64-test CPU/COP0/TLB groups in CI with fixture manifest
- [ ] RSP scalar + vector LLE sufficient for nemu64-test RSP group
- [ ] Raw frame and audio capture interfaces (even if RDP/AI are minimal at first)

### Add After Validation (v1.x)

- [ ] RDP reference renderer + VI raw scanout; rdpq-based libdragon fixture; RDP conformance reporting
- [ ] AI path and raw PCM; small authored audio diagnostic
- [ ] Save media baseline (EEPROM 4/16K, SRAM; FlashRAM after) with refusal postconditions
- [ ] Standard controller, timed input, replay artifacts, core-level serialization
- [ ] GDB stub, disassembly, guest-time boundary trace stream
- [ ] Minimal host and the cleared playable homebrew title
- [ ] Public core API proposal and installed-consumer test (freeze only after lifetime/refusal postconditions are proven)

### Future Consideration (v2+)

- [ ] Controller Pak, Rumble Pak, Transfer Pak as separate named profiles
- [ ] Expansion Pak, PAL/MPAL profiles, other CIC variants, FlashRAM edge cases
- [ ] 64DD profile (disk fixture rights first)
- [ ] GPU renderer(s) as host-side, conformance-checked options; upscaling
- [ ] Save-state UI, rewind, run-ahead, netplay, RetroAchievements via adapter
- [ ] Cycle/timing accuracy profile and caches modeled beyond "as needed"
- [ ] JIT with equivalence testing against the reference interpreter

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---|---|---|---|
| Profile identity + evidence-bound claims | HIGH (core value) | LOW-MED | P1 |
| Deterministic scheduler + guest-time limits | HIGH | HIGH | P1 |
| CPU/CP0/TLB/exceptions + PI/SI/MI | HIGH | HIGH | P1 |
| ISViewer + headless runner + outcome taxonomy + result protocol | HIGH | MED | P1 |
| nemu64-test CI with fixture manifest | HIGH | MED | P1 |
| RSP LLE (scalar then vector) | HIGH | VERY HIGH | P1/P2 |
| RDP reference + VI raw scanout | HIGH | VERY HIGH | P2 |
| AI raw audio | MED-HIGH | MED | P2 |
| Saves (EEPROM/SRAM first) | HIGH | MED | P2 |
| Controller + timed input + replay | HIGH | MED | P2 |
| GDB stub + trace stream | HIGH (dev) | MED-HIGH | P2 |
| Core serialization | MED-HIGH | MED-HIGH | P2 |
| Public API + installed consumer | HIGH (integration) | HIGH | P2 (freeze later) |
| Minimal playable host | MED-HIGH | MED | P2 |
| Controller Pak / Rumble | MED | MED | P3 |
| Transfer Pak, 64DD, RTC, Expansion/PAL profiles | LOW-MED each | HIGH | P3 |
| Save-state UI, rewind, run-ahead, netplay, RA, upscaling | MED (players) | MED-HIGH | P3 |
| HLE microcode, per-game hacks, widescreen, texture packs | n/a | n/a | Not planned |

**Priority key:** P1 must have for the first verifiable milestone; P2 add when the P1 baseline is repeatable; P3 later or promoted on evidence.

---

## Competitor Feature Analysis

| Feature | ares (+ Ares 64 fork) | gopher64 | Project64 | Mupen64Plus / RMG / Next | Our approach |
|---|---|---|---|---|---|
| Accuracy stance | LLE only, no HLE, "accuracy and preservation" `[primary]`; Reference accuracy flags | Rust LLE-style device tree with parallel-rdp; interpreter CPU `[observation]` | Plugin choice incl. Angrylion Plus and Parallel-RDP `[primary]` | HLE and LLE options via plugins `[primary]` | LLE reference core; optimizations must match |
| Support-claim format | Per-title status + ares version, "Unverified" badge `[primary]` | Community-rated; no table found `[unknown]` | n/a | n/a | Per-profile evidence tiers with artifacts |
| Headless / CI | None upstream; `ares-test` in a fork `[primary]` | None found | None | Console frontend `[unknown]` | First-class runner, taxonomy, versioned protocol |
| Debugging | GDB, tracer, memory editor, graphics viewer | None found | Extensive GUI debugger and JS scripting, nightly-only caveats | `DEBUGGER=1` core API | GDB + boundary trace stream first; GUI later or never |
| Replay/movies | Not verified | Not found | Not found | m64 ecosystem, BizHawk | Movie artifacts bound to build+profile+content hash |
| Saves | EEPROM/SRAM/Flash; v148 EEPROM timing | Similar plus SC64 cart emulation | Yes | Yes | Explicit device selection with refusal postconditions |
| Accessories | Controller Pak, Rumble, Transfer Pak, Bio Sensor `[observation]` | Transfer Pak, rumble, VRU | Plugin-dependent | Plugin-dependent | Named profiles, later |
| 64DD | Yes (3 named variants) | `[unknown]` beyond "64DD cartridge port save support" from a search snippet | Yes | `[unknown]` | Later separate profile |
| Netplay / RA / upscaling | RA not found; upscaling via parallel-rdp | Netplay, RA, upscaling, overclock | Plugin-based | RMG netplay, m64p netplay flag | Deferred |
| Embedding | App; no stable library API found `[observation]` | `lib.rs` exposes CLI-style `run` `[observation]` | No | Core API; libretro | Embeddable API with postconditions |
| Platforms | Windows/macOS/Linux/BSD | Windows/macOS(ARM)/Linux/Android | Windows only | Windows/Linux/macOS; libretro | Decided later (language/build open) |
| Dependency posture | libco, SDL3, librashader, libchdr and more `[primary]` v148 notes | Rust crates, Slint, SDL3, tokio `[observation]` | Many | Many | Small flat tree `[owner]` |

---

## Most credible differentiators, ranked

1. **Evidence-bound claims per named profile with a machine-checked fixture/rights manifest.** Evidence for credibility: the strongest existing pattern (ares table) is per-title/per-version with explicit "unverified" status, which proves the pattern is valued but leaves profile + artifact binding open. Risk: it is a process advantage that must be kept honest; it earns nothing if claims are thin. Confidence MEDIUM.
2. **Determinism contract + outcome taxonomy + result protocol.** Evidence: ares-test shows demand and feasibility (emulated-time waits, forced deterministic entropy, exit codes) and also shows what is missing (wall-clock watchdog, three exit codes, no versioned protocol). Differentiator is rigor and being a core invariant, not novelty. Confidence MEDIUM-HIGH.
3. **Guest-time-boundary event stream (trace) and replay artifacts.** Evidence: existing tracers/logs are UI-bound or per-frame; no versioned stream found. Confidence LOW-MEDIUM because my search may have missed internal formats.
4. **Embeddable API with explicit lifetime/refusal postconditions and installed-consumer test.** Evidence: embedding already exists (Mupen64Plus core API, libretro), so the claim is only about contract quality. Confidence MEDIUM; compare against m64p after inspecting its globals.
5. **RDP conformance reporting against a recorded reference.** Evidence: parallel-rdp-standalone provides suites that can be adopted; the differentiator is publishing per-test results with reference version caveats. Confidence MEDIUM; license and rights audit pending.

Not credible as differentiators: raw speed (ares JIT, gopher64, others are ahead and optimization is deferred), breadth of titles, player features (gopher64 already ships netplay, RetroAchievements, upscaling, CRT shader, Android).

---

## Open questions and research flags for later phases

- `[unknown]` Whether ares upstream supports input-movie recording or a stable trace format; verify before claiming that gap.
- `[unknown]` Depth of cen64's debugger/GDB; check source if relevant.
- `[unknown]` Mupen64Plus core global-state/instance isolation (affects the embedding comparison).
- `[unknown]` libdragon's future after the 2026-10-01 archive notice; pin and mirror before depending on it for fixtures or IPL3.
- `[unknown]` Angrylion-Plus/Thar0 fork license and rights for any adopted code or vectors; parallel-rdp-standalone is MIT per GitHub metadata, but its test assets need per-file review.
- `[unknown]` Rights of nemu64-test's generated ROMs and embedded assets (repo MIT; 4 stars; single maintainer).
- `[unknown]` Provenance of the PIF ROM array embedded in gopher64; treat as a no-copy example.
- Run-time claims in this file about emulator behavior are from docs and source reading, not from executing those emulators.

## Sources

Accessed 2026-11-09.

Primary (READMEs, release notes, source, repository metadata):
- ares v148 release notes: https://ares-emu.net/news/ares-v148-released
- ares site: https://ares-emu.net/
- ares compatibility table: https://ares-emu.net/compatibility/nintendo-nintendo-64
- ares repository and source (accuracy.hpp, desktop-ui/tools/tracer.cpp, ares/n64/cartridge/isviewer.cpp, ares/n64/system/system.cpp): https://github.com/ares-emulator/ares
- Ares 64 fork README and README_JS.md: https://github.com/HailToDodongo/ares-64
- gopher64 README and source (src/lib.rs, src/device/is_viewer.rs, src/device/controller): https://github.com/gopher64/gopher64
- Project64: https://github.com/project64/project64
- Project64 Debugger manual (community-maintained, treated as primary for debugger features): https://hack64.net/docs/pj64d
- Mupen64Plus core: https://github.com/mupen64plus/mupen64plus-core
- RMG: https://github.com/Rosalie241/RMG
- Mupen64Plus-Next: https://github.com/libretro/mupen64plus-libretro-nx
- cen64: https://github.com/n64dev/cen64
- dgb-n64 / Dillonb: https://github.com/Dillonb/n64
- BizHawk releases: https://github.com/TASEmulators/BizHawk
- libdragon README, debug.h and libdragon.dev: https://github.com/DragonMinded/libdragon , https://github.com/DragonMinded/libdragon/blob/trunk/include/debug.h , https://libdragon.dev
- libdragon GDB wiki (via mirror): https://github-wiki-see.page/m/DragonMinded/libdragon/wiki/Debugging-via-gdb
- nemu64-test and deprecated n64-systemtest: https://github.com/thelemmy/nemu64-test , https://github.com/lemmy-64/n64-systemtest
- parallel-rdp-standalone: https://github.com/Themaister/parallel-rdp-standalone
- GitHub repository metadata (archived flags, push dates, release dates) via `gh api`, 2026-11-09.

Community reference (treat as lower confidence):
- n64brew CIC-NUS and RDP/RSP overview pages: https://n64brew.dev/wiki/CIC-NUS , https://n64brew.dev/wiki/Reality_Display_Processor , https://n64brew.dev/wiki/Reality_Signal_Processor
- SaveTest-N64 (save type coexistence notes, via search): https://www.github.com/meeq/SaveTest-N64
- TASVideos BizHawk N64 and forum threads on Mupen/BizHawk determinism (dated 2014-2016): https://tasvideos.org/Bizhawk/N64
- gametechwiki N64 test results (not fetchable, 403): https://emulation.gametechwiki.com/index.php/User:Sczther/N64_Tests
- simple64 status and gopher64 updates (secondary): https://www.gamingonlinux.com/2025/08/nintendo-64-emulator-gopher64-recently-added-save-state-slots-and-netplay-cloud-servers
- RetroAchievements support status (conflicting): https://emulation.gametechwiki.com/index.php/Achievements_support

Owner/project context: `.planning/PROJECT.md`, `AGENTS.md`, `.planning/bootstrap/EMULATOR-DNA.md`.

---
*Feature research for: deterministic accuracy-first Nintendo 64 emulator (Tendies64)*
*Researched: 2026-11-09*
