# Architecture Research

**Domain:** Deterministic, accuracy-first Nintendo 64 emulator (Tendies64): one canonical machine core, thin hosts
**Researched:** 2026-11-09
**Confidence:** MEDIUM-HIGH for structure and boundaries (several independent source observations agree); MEDIUM for the scheduling recommendation (partly inference); LOW for clock-domain exactness and RDP/RDRAM bus contention (open, flagged below)

## Evidence labels

Every non-obvious claim carries one of: **[OWNER]** owner direction in project files; **[PRIMARY]** Nintendo/NEC document fact; **[COMMUNITY]** n64brew wiki or similar reference (not console truth); **[IMPL]** implementation observation (what an emulator or test source does, not what hardware does); **[MEASURED]** measured by this research (none this session); **[INFER]** inference from the above; **[REC]** recommendation; **[UNKNOWN]** unresolved. Emulator code and tests passing in it are never console truth. Access date for all URLs and local clones is 2026-11-09. Nintendo manuals were not re-fetched this session; they are cited only through the bootstrap pack (`EMULATOR-DNA.md`) and must be rechecked before becoming requirements.

## Executive recommendations (the 12 decisions this document asks the roadmap to adopt)

1. **[REC] One `Machine` value owns all guest-visible state as plain data.** No coroutine stacks, no threads inside the core, no globals, no host calls. Any instant at which `advance()` returns is a consistent, serializable state.
2. **[REC] Guest time is a single integer axis (`GuestTick`, u64) that only the core advances.** The host asks for work in guest-time terms; it never supplies a clock. Wall clock, filesystem and input-poll callbacks do not exist inside the core.
3. **[REC] Scheduler = event queue (total order) + per-component local clocks with lazy catch-up, single-threaded.** This is the ares/gopher64 consensus shape. The CPU is the time driver; RSP/RDP/VI/AI/PI/SI are caught up at interaction points and at event ticks.
4. **[REC] The CPU-to-RSP synchronization quantum is guest-visible behavior and is part of the profile identity,** not a tuning knob. The reference profile uses the finest practical quantum; a coarser quantum is a different, separately evidenced profile.
5. **[REC] RSP: LLE interpreter (scalar plus VU, scalar-portable reference) is the baseline and permanent reference.** HLE is rejected as a canonical path. SIMD/JIT RSP are later optimizations, verified by lockstep differential against the reference.
6. **[REC] RDP/VI: write an in-tree software reference renderer from documents and tests; use Angrylion-Plus and parallel-rdp only as out-of-tree differential oracles.** A GPU RDP is never part of the canonical core (async completion races against guest-visible RDRAM).
7. **[REC] Core API is an instance-based, pull-style, bounded-advance API:** `create -> admit(profile, content) -> queue_input(timed) -> advance(stop spec) -> drain outputs -> export media -> reset/unload -> destroy`, with transactional refusal and three distinct error classes (refused, guest-stop, defect). Borrow mupen64plus's backend-interface seam idea and LibFuturo's buffer/postcondition discipline; avoid libretro/ares/mupen64plus callbacks-inside-run, global state, and core-initiated file or clock access.
8. **[REC] Observability is a compile-time-selected sink fed guest-tick-stamped events, drained by the host after `advance()` returns.** ISViewer is a guest-visible device in an explicit diagnostic profile feeding a guest-log channel, not a trace hook.
9. **[REC] Boot route: HLE-PIF that seeds post-IPL2 state and runs an open IPL3 (libdragon's) as guest code.** Never ship PIF/IPL bytes. A user-supplied PIF ROM is a rights-gated host input for a later, separately named profile.
10. **[REC] Keep a reference implementation of every optimizable component compiling and runnable in-tree** (interpreter, RSP scalar/VU, software RDP/VI) with a differential harness; optimizations ship only with an equality gate against the reference.
11. **[REC] Treat the CPU time model as pluggable accuracy tiers** that map to the nemu64-test feature tiers (base, timing, cycle/cop0hazard); build tier 0 (functional plus instruction-granular time) and tier 1 (timing) first.
12. **[REC] Enforce core isolation mechanically** (dependency lint that forbids time/fs/thread/random imports in the core; an interleaved-two-instances digest-equality test in CI).

---

## Standard Architecture

### System overview

```
                         HOST LAYER (owns clocks, files, input mapping, display, audio device, durability)
 +---------------------------------------------------------------------------------------------+
 | headless runner (first) | SDL/native frontend (later) | LibFuturo adapter (maybe) | test harness|
 +---------------------------------------+-----------------------------------------------------+
                                         |  C-ABI facade (opaque handle, struct_size-versioned, no callbacks in advance)
                                         v
 +---------------------------------------------------------------------------------------------+
 |                               CORE (deterministic; no I/O, no clock, no threads, no globals)   |
 |                                                                                               |
 |  API facade: profile registry | admit/refuse | input queue | advance | output ring | media | state
 |                                                                                               |
 |  Machine { profile, scheduler, event queue, outputs, trace sink }                              |
 |                                                                                               |
 |   +-----------+     +---------------------------- system bus / memory map ------------------+  |
 |   | VR4300    |<--->| RDRAM(+9th-bit plane) | RI | MI | PI | SI/PIF | RSP regs+DMEM/IMEM | |  |
 |   | CPU       |     | RDP regs (DPC/DPS) | VI | AI | cart ROM/saves | ISViewer(dbg profile) |  |
 |   | CP0 TLB   |     +--------+-----------+-----------+-----------+-----------+-------------+  |
 |   | I/D cache |              |           |           |           |           |                |
 |   | CP1 FPU   |        +-----v----+ +----v-----+ +---v----+ +----v----+ +----v-----+          |
 |   +-----------+        | RSP      | | RDP      | | VI     | | AI      | | SI+PIF   |          |
 |                        | scalar+VU| | cmd proc | | scanout| | DMA+DAC | | joybus   |          |
 |                        | DMA      | | raster   | | timing | | cadence | | CIC (opt)|          |
 |                        +----------+ +----------+ +--------+ +---------+ +----+-----+          |
 |                                                                              |                |
 |                                            controllers/paks/EEPROM/SRAM/Flash (device objects) |
 +---------------------------------------------------------------------------------------------+
   outputs (guest-tick-stamped, core-owned ring): VideoFrame | AudioChunk | GuestLog(ISViewer) | TraceEvents | MediaDirty
```

### Component responsibilities

| Component | Owns (state) | Talks to | First-implementation fidelity | Key hardware facts / evidence |
|---|---|---|---|---|
| **VR4300 CPU core** | GPRs, HI/LO, PC, pipeline-visible state (delay slot, LL bit), exception state | Bus (all loads/stores/fetch), CP0, CP1, scheduler (retire cost) | Interpreter, exact MIPS III incl. 64-bit ops, LL/SC, alignment/address errors, delay slots | VR4300 MIPS III, 93.75 MHz [OWNER/PRIMARY via DNA]. nemu64-test categories: CPU/COP0, LL/SC/LLD/SCD, 64-bit ops, exceptions, TLB, access widths [IMPL: nemu64-test README + `src/tests/` tree at 9a8b9f7] |
| **CP0 / TLB / exceptions** | Count/Compare/Status/Cause/EPC/Config/Wired/Index/EntryHi/Lo/Context/XContext/PErr/TagLo-Hi, 32 dual TLB entries, interrupt pending lines | CPU, MI (IP2), scheduler (Count/Compare timer event) | Exact. Count advances at half PClock rate [IMPL: ares `stepCount((clocks-counted)>>1)`; verify against VR4300 manual, UNKNOWN until checked] | Startup tests expect Config 0x7006E463, Status 0x34000000 at boot (with soft-reset allowances) [IMPL: nemu64-test `tests/startup`]. TLB 32 entries [COMMUNITY secondhand quote of the user manual; recheck] |
| **I-cache / D-cache** | Tags, valid/dirty, line data for 16 KB I / 8 KB D | CPU, bus (fills/writebacks) | Exact functional model (line data kept in cache, write-back). RCP DMA does not snoop, so stale-data behavior is guest-visible [DNA: cache visibility; IMPL: nemu64-test has `tests/cache/{icache,dcache}.rs` and `tests/timing/cache.rs`] | Sizes: NEC VR4300 datasheet (16 KB instr, 8 KB data) [PRIMARY-ish, via search snippet of bitsavers PDF, recheck]; write policy and line sizes [UNKNOWN until read from the manual] |
| **CP1 FPU** | 32 FP regs (FR modes), FCR31, condition bit | CPU | Software-implemented arithmetic (no host FPU) so rounding/denormal/unimplemented-op behavior is host-independent [INFER+REC]. Needs a dedicated research pass on VR4300 FPU edge semantics | nemu64-test has `tests/cop1/{compares,full_vs_half_mode,randomized}` and a `cop1_stress_test` feature [IMPL]. Early-stepping multiply bug and `sra` leak exist on some CPU steppings [COMMUNITY: n64brew VR4300] so stepping/revision belongs in the profile |
| **System bus / memory map** | Address decode tables, open-bus behavior, access-size rules | Everything | Table-driven physical map (one 64 KB-granule dispatch like gopher64/mupen64plus) plus a direct "fast-mem" pointer path for RDRAM/ROM; **canonical big-endian storage or one accessor layer** | Physical map constants (RDRAM 0x0, RDRAM regs 0x03F00000, SP 0x04040000.., DPC 0x04100000, MI 0x04300000, VI 0x04400000, AI 0x04500000, PI 0x04600000, RI 0x04700000, SI 0x04800000, ROM 0x10000000, ISViewer 0x13FF0000, PIF 0x1FC00000) [IMPL: mupen64plus-core `src/device/device.h`]. Unmapped PI-bus reads return low 16 address bits repeated [COMMUNITY: n64brew PI] |
| **RDRAM + RI** | Data array (4 MB base; 8 MB with Expansion Pak = profile), **9th-bit plane**, per-device RDRAM register file, RI registers | Bus, RDP/VI (9th bit), MI (init/repeat mode) | Reads above installed size return zero, no mirroring (IPL3 and nemu64-test depend on it) [COMMUNITY: nemu64-test README; IMPL: libdragon `boot/ipl3.c` DMAs from 8 MiB+0x2000 to clear IMEM]. Register protocol modeled in two tiers (see Boot) | 9th bit usable only by RDP/VI [COMMUNITY: n64brew RDRAM]; RDRAM init needs delay register, device-id assignment, broadcast writes, MI repeat mode for the reset write-delay quirk [COMMUNITY]; registers mirror every 0x40 bytes, regs 10-15 read 0 [COMMUNITY] |
| **MI** | MI_MODE (repeat/init/ebus/upper), MI_VERSION, MI_INTERRUPT, MI_MASK | All RCP units (raise), CPU (IP2 line), RDRAM (repeat/upper writes) | Exact interrupt semantics: masked source still sets flag; unmasking an already-set flag signals immediately [COMMUNITY: n64brew MI]. Sources: SP, SI, AI, VI, PI, DP | `tests/mi/repeat.rs` exists in nemu64-test [IMPL] |
| **PI + cartridge** | DMA registers, domain-1/2 LAT/PWD/PGS/RLS, status, ROM image, cart bus decode | Bus, RDRAM, MI, scheduler | DMA as event: duration from domain registers (word cost = LAT + PWD + RLS-class terms, per n64brew), data copied at completion or progressively (see Timing, [UNKNOWN] choice); aligned path first, buggy unaligned path behind hardware-dump test (`n64_pi_dma_test`) | PI facts and `n64_pi_dma_test` hardware dumps [COMMUNITY: n64brew PI]. ROM byte order (z64/v64/n64) normalized at admission, never in the core hot path |
| **SI + PIF** | SI regs, 64-byte PIF RAM, PIF command byte, channel state machine, boot-state machine | MI, RDRAM (SI DMA), joybus devices, save devices on channel 4, CIC model | PIF is a small state machine, not CPU code (HLE-PIF): command byte at offset 0x3F, joybus frame parsed on command write, executed (channels 4 down to 0) on SI DMA read, results written to PIF RAM before the DMA copies out; per-transaction timing from RX/TX delay rules | [COMMUNITY: n64brew PIF-NUS]. CIC challenge/response (6105 area) and the ~5 s boot-complete timeout are modeled only when a CIC profile requires them |
| **Joybus devices** | Controller state, Controller Pak (banked), Rumble Pak, Transfer Pak, EEPROM (4/16 Kbit), RTC | PIF channels | One `JoybusDevice` interface (command bytes in, response bytes out, advance(tick)); accessories are separately named profile choices, never default-on | [OWNER/DNA]: Transfer Pak needs a GB machine and separately cleared content. ares exposes 1/4/16/62-bank Controller Pak sizes [IMPL: ares `system.cpp`] |
| **Save media** | Media image bytes and `guest_generation` counter for SRAM, FlashRAM, EEPROM, Controller Pak | PI (SRAM/Flash on cart bus), PIF channel 4 (EEPROM), host (export/import only) | In-instance bytes; core never touches files. Explicit device selection from the profile, not inferred from title | [OWNER] explicit device selection and refused-operation postconditions |
| **RSP** | PC, scalar GPRs, 32x128-bit VU regs, accumulators, VCC/VCO/VCE, 4 KB DMEM + 4 KB IMEM, SP_STATUS, DMA queue (double-buffered), semaphore | Bus (via DMA only), MI, RDP regs (via COP0 c8-c15), CPU (via SP regs and DMEM/IMEM window) | LLE interpreter: scalar + VU, microcode-agnostic; DMA with the documented 8-byte alignment, bank-wrap, stride, double-buffer, FULL/BUSY, and "halt does not stop DMA" rules | [COMMUNITY: n64brew RSP interface]. RSP never loads or stores RDRAM directly: all RDRAM traffic is DMA or RDP XBUS command fetch [INFER from the hardware description; gives a clean interaction-point list] |
| **RDP** | DPC regs (START/END/CURRENT/STATUS/CLOCK/BUSY counters), TMEM (4 KB), render state, command FIFO position | RDRAM (cmd fetch, TMEM loads, color/Z), RSP DMEM (XBUS), MI (DP interrupt on SYNC_FULL) | Software reference renderer, commands executed in order; timing via cost model + completion event; documented crash modes (SYNC_FULL overlap) modeled as a sticky `crashed` state | [COMMUNITY: n64brew RDP interface]. ares sets `command.crashed` and reports busy forever [IMPL: ares `rdp.cpp`] |
| **VI** | VI registers, half-line counter, interrupt line, per-frame register-change log | RDRAM (scanout read incl. 9th bit), MI (VI interrupt), output ring | Counter/interrupt exact; scanout produced per field by a software VI that replays the register log by half-line, producing raw RGBA frames before host scaling | NTSC/PAL/MPAL standard values; V_CURRENT write clears interrupt; interlace/serrate rules; ORIGIN/WIDTH/Y_SCALE may change during hblank without resetting the accumulator [COMMUNITY: n64brew VI] |
| **AI** | Two-entry DMA queue, DACRATE/BITRATE, sample counter, output ring | RDRAM (reads), MI (IRQ at DMA *start*), output ring | Sample cadence from DACRATE; 16-bit stereo only; the 8 KiB-boundary delayed-carry quirk modeled | AI IRQ fires when a transfer starts, not ends [COMMUNITY: n64brew AI]. Emulating it at end breaks double-buffered pacing |
| **Scheduler / event queue** | `now`, per-component local clocks, event heap (total order) | All components | See Timing section | [IMPL: ares, gopher64, cen64, mupen64plus] |
| **Output rings** | Video frames, audio chunks, guest log, trace events | Host drains after `advance()` | Fixed capacity; backpressure by stopping advance with `OUTPUT_FULL`, never by dropping or blocking | [REC] |
| **Profile registry** | Immutable profile descriptors (console revision, TV, RAM, boot route, CIC, ports/accessories, save devices, debug devices, timing tier, sync quantum) | API facade | Profile id is hashed into every state snapshot and output record | [OWNER] support claims tie to named profiles |

---

## Recommended Project Structure

Language is undecided [OWNER: pending research]. The structure below is language-neutral; the constraints that matter are (a) plain-data state, (b) a build-time wall around `core/`, (c) monomorphizable trace hooks (generics/templates/macros), (d) a C-ABI facade regardless of implementation language.

```
core/                         # deterministic machine; may not import time/fs/thread/random/log/net
  time/                       # GuestTick, ClockDomain converters, event queue, scheduler
  cpu/                        # vr4300 decode+exec, cp0, tlb, cache, cp1 (soft-float), cost model
  bus/                        # physical map, fast-mem, open-bus, endianness accessors
  rcp/{mi,ri,rdram,pi,si,pif,sp,rsp,rdp,vi,ai}/
  devices/{cart,joybus,controller,pak,eeprom,sram,flash,isviewer}/
  boot/                       # hle-pif, post-IPL2 state seeds, boot-flag contract with open IPL3
  profile/                    # profile descriptors, validation, refusal reasons
  output/                     # rings + record layouts
  state/                      # explicit versioned serializer + canonical digest
  trace/                      # sink trait, event records (no-op default)
  reference/                  # slow reference impls kept in-tree: rsp_scalar, rdp_sw, vi_sw, cpu_interp
api/                          # C-ABI facade, struct_size versioning, status enums, postcondition docs
host/
  runner/                     # headless: fixture manifest, outcome taxonomy, ISViewer parser, result JSON
  frontend/                   # later: window, audio device, input mapping, save files (all host-side)
  adapters/                   # later: LibFuturo adapter (lossy by design)
tools/                        # trace decoders, RDP dump record/replay, differential harness
tests/{component,rom,differential,installed-consumer}/
```

### Structure rationale

- **`core/` wall:** the single most valuable architectural control. Enforce by dependency lint, not review. Isolation bugs found in reference emulators are all "a host thing leaked in" (see Global-state section).
- **`reference/` beside optimized code:** every optimization needs an in-tree oracle that does not rot. Angrylion-style external oracles are for conformance, not for daily differential runs (GPU/driver variance, licensing).
- **`api/` separate from `core/`:** the facade owns ABI shape and postconditions; the core can change internal types freely. The facade is where "refused vs guest-stop vs defect" is enforced.
- **`host/runner` is a first-class consumer, not a test hack:** [OWNER] requires an independent installed consumer test; building the runner against the public API from day one keeps the API honest.

---

## Scheduling and timing model

### How the references do it (all [IMPL] unless noted; sources in the Sources section)

| Project | Time axis | CPU/RSP coupling | RDP | DMA/IRQ | Observations relevant to Tendies64 |
|---|---|---|---|---|---|
| **ares** (ISC) | Per-component `Thread::clock` (s64); CPU is the primary scheduler thread; Count advances at half the thread clock | `CPU::synchronize()` subtracts elapsed clocks from vi/ai/rsp/rdp/pif, calls each `main()`, steps a global `Queue` of timed events. **Interpreter returns `true` from `instruction()` every instruction, so it synchronizes after every instruction.** JIT runs blocks bounded by `min(JitInterleaving, timer delta, queue delta)`. RSP is a cooperative loop (`while(clock<0)`): halted state burns 128-cycle chunks; running state issues instructions with dual-issue modeling, DMA stepped by the clock delta | `RDP::main` steps by `system.frequency()`; software command loop runs synchronously on the calling thread from `current` to `end`; optional Vulkan path; crash modeled as sticky busy | Event IDs: PI/SI DMA, bus writes, RTC, EEPROM write, flash completion, 64DD, GDB poll. `RBusDevice` enum names every RDRAM requester (CPU caches, SP/PI/SI/VI/AI/DP DMA, DP draw) | Closest existing fit to the recommended model. Costs: global singletons (`system`, `queue`, `random`, per-component globals, `platform`), libco coroutines (needs a `Synchronize` pass to save state), and an option for deterministic entropy that is opt-in rather than mandatory |
| **cen64** (BSD-3) | Lockstep cycles | Single-thread `device_spin`: per RCP cycle pair, `vr4300_cycle` + `rsp_cycle` + `ai_cycle` + `pi_cycle` + `vi_cycle`, with an extra CPU cycle per 2 RCP (3 CPU : 2 RCP, matching 93.75 : 62.5). Threaded mode: CPU and RCP threads synchronize at a mutex/condvar barrier every 6250 RCP cycles | RDP is a separate unit | Per-cycle devices | Cycle-level reference value, but the threaded mode's window means cross-thread RDRAM races inside a window are not ordered by guest time. Last upstream commit 2025-10-25 (e0641c8), so a reading reference, not a base |
| **gopher64** (GPLv3, Rust) | Event queue keyed to CP0 Count (`create_event` = Count + delay); CPU loop checks `Count > next_event_count` each instruction | RSP runs as a **burst** when HALT is cleared: instructions execute until a `sync_point`, returning elapsed CPU cycles; an `SP` event then fires. `sync_point` is set heuristically when the CPU polls DPC status/current repeatedly (spin-wait detection) | `DPC_END` write calls `process_rdp_list(now)` synchronously (via parallel-rdp), sets PIPE_BUSY, schedules a DP interrupt event after the returned timer | Single `Device` struct, serde-serializable; host UI embedded in `Device` (`#[serde(skip)]`) | Elegant and fast but: the burst plus spin-detection is a heuristic outside the hardware model; RNG seeded from OS entropy and RTC from `SystemTime::now()` leak host into guest (netplay then has to ship the seed); `parallel-rdp/interface.cpp` holds `static` processor/WSI/window state |
| **mupen64plus-core / simple64** (GPLv2+) | Interrupt queue keyed by CP0 Count (`add_interrupt_event`) | RSP task executed **synchronously and atomically** via `rsp.doRspCycles(0xffffffff)` when status clears HALT. Task type read from `DMEM[0xfc0/4]` (1 = gfx, 2 = audio, else other) with fixed fudge delays (1000/4000/0 cycles) for the SP interrupt, and a 4000-cycle DP interrupt | Plugin `ProcessRdpList` | RSP DMA completion as an event | The plugin-table design and fudge constants are exactly the unevidenced shortcuts to avoid. **simple64 was not inspected separately; assumed to share core scheduling [UNKNOWN]** |
| **n64-systemtest / nemu64-test** authors | No scheduler prescribed. Self-checking ROM (prints `Done! Tests: N. Failed: M`), results mirrored to ISViewer | Tests are tiered by feature: `base` (functional), `timing` ("a NOP is 0.5 cycles, Random decrements by 1 per instruction"), `cycle` and `cop0hazard` (need cycle accuracy; authors say probably not implementable by a dynamic recompiler), `poorly_understood_quirk`, `experimental_rdp`, per-VU-op stress tests | RDP triangle tests off by default (unstable on hardware) | n/a | [IMPL: `Cargo.toml` feature comments at 9a8b9f7; package is still named `n64-systemtest` v3.0.0]. The default `base` suite can be passed with instruction-granular time; this is the tiering the roadmap should copy |

### Recommended model ([REC], supported by [INFER] where noted)

**A single-threaded, event-queue scheduler with per-component local clocks and lazy catch-up, measured in `GuestTick`.**

**Time base.** `GuestTick` is a u64 count of a master tick fast enough to represent every clock domain exactly. CPU 93.75 MHz and RCP 62.5 MHz [OWNER/PRIMARY via DNA] are 3:2, so a 187.5 MHz master tick gives 2 ticks per CPU PClock and 3 per RCP clock (this is also the ratio cen64 hard-codes and the constant ares uses for its GDB poll interval, `93750000*2`). The VI/AI domain is different: ares sets NTSC video frequency to 48,681,818 Hz and PAL to 49,656,530 Hz [IMPL], and one forum teardown reports a separate 14.318 MHz video crystal and a 62.5 MHz system crystal [LOW, single source, NTSC only]. Treat the video domain as an **asynchronous rational domain**: events are computed by an integer converter (`num/den` plus carried remainder, u128 intermediate) from video-domain counts to ticks, never by float. u64 at 187.5 MHz wraps after about 3,000 years of guest time, so no rebasing is needed. **[UNKNOWN]:** exact NTSC/PAL/MPAL crystal values, and the phase relationship between domains (synchronizer latency); model none initially and record it as a profile limitation.

**Event queue.** A binary heap keyed `(tick, class_priority, sequence)`. The tuple gives a total order so two events at the same tick are never ambiguous (ares solves the same problem with per-thread `uniqueID`; gopher64 does not and relies on enum order). Event classes: `SP_DMA_DONE`, `SP_STATUS_PENDING`, `SP_RUN_RESUME`, `DP_SYNC_DONE`, `PI_DMA_DONE`, `SI_DMA_DONE`, `SI_JOYBUS_RESPONSE`, `AI_SAMPLE_FETCH`, `AI_DMA_START`, `VI_HALFLINE`, `COUNT_COMPARE`, `RDRAM_REFRESH_OR_INIT_STEP` (if modeled), `INPUT_APPLY`, `MEDIA_COMMIT_MARK`, `STOP_TICK`.

**Run loop (pseudo-code, language-neutral).**

```
advance(stop):                              // stop = { tick_limit, instr_limit, frame_event, output_full, break_mask }
  loop:
    horizon = min(queue.peek().tick, stop.tick_limit)
    cpu.run_until(horizon)                  // executes whole instructions; returns exact end tick (may overshoot by < 1 instruction)
                                            //   inside: any access to an RCP-visible register/memory first calls catch_up(target, cpu.now)
    now = cpu.now
    while queue.peek().tick <= now: dispatch(queue.pop())   // deliver in total order; may raise MI interrupts, which cpu samples at the next instruction boundary
    if stop satisfied: return report(reason, now, overshoot)
```

**Catch-up rule.** A component with local clock `t_c < now` that something is about to observe is `run_until(now)` first. Observation points (the only places components can influence each other):

| Observer | Component caught up first | Why |
|---|---|---|
| CPU read/write of SP regs, SP DMEM/IMEM window, SP_SEMAPHORE, SP_PC | RSP, SP DMA | Halt/broke/signal/DMA-busy are timing-visible; DMEM/IMEM single-ported contention rules [COMMUNITY] |
| CPU read/write of DPC/DPS regs | RDP | BUSY counters and CLOCK are time-derived |
| CPU read of MI_INTERRUPT, VI_V_CURRENT, AI_STATUS/LENGTH, PI_STATUS, SI_STATUS | the owning unit | derived from time |
| CPU or any DMA read of RDRAM | any agent that may have **pending** writes to the same region (RDP framebuffer/Z, RSP DMA writes, PI/SI DMA) | order of memory effects by guest time |
| RSP COP0 access to DPC regs, SP_STATUS, semaphore | CPU not needed (CPU is the driver) but the event queue must have been drained to RSP's local time | RSP interaction points are its only external effects |
| Any DMA completion | the consumer (e.g. RSP waiting on `DMA_BUSY`) | status bits |

**[INFER]** from the hardware description: the RSP reaches RDRAM only through DMA and reaches the RDP only through COP0/DPC registers or XBUS command fetch. So the RSP can legitimately run ahead of the CPU *between its own interaction points* without breaking memory ordering, **if** the drift is bounded. That bound is the **sync quantum**.

**The sync quantum is profile identity ([REC], [INFER]).** If the RSP lags the CPU by up to Q ticks, an RSP-initiated DMA reads RDRAM as of the CPU's later time, so Q changes guest-visible results for racy software. A determinism guarantee (same profile + content + input + bounds gives the same result) holds for any fixed Q; an accuracy claim does not transfer between Qs. Therefore:

- Profile field `sync_quantum` (ticks) is hashed into the profile id, state snapshots and result records.
- **Reference profile:** minimal quantum (RSP and CPU interleave at instruction granularity, like ares's interpreter; cost is paid only while the RSP is running, which is when it matters). RSP halted and RDP idle cost nothing beyond event-queue checks.
- Larger Q (burst-until-interaction, like gopher64) is allowed only as a separately named profile with its own evidence, never as a hidden performance option. Spin-wait detection ("CPU repeatedly polls the same status") is a legitimate *deterministic* optimization only if it is a pure function of guest state and time and the skipped work is equivalent; keep it in the optimization tier with a differential gate.

**Per-device timing treatments.**

- **CPU time model (tier 0):** cost = 1 PClock per instruction retired (2 master ticks) plus cache-miss, uncached-bus and TLB penalties from tables; Count += cost/2 with fractional carry; Random decrement per instruction. **Tier 1** reproduces the `timing` feature expectations. **Tier 2** (`cycle`, `cop0hazard`) adds pipeline-stage modeling and is interpreter-only [IMPL: authors' own caveat]. All tiers share one `cost(instr, ctx) -> ticks` seam at retirement; the interpreter calls it, a future JIT bakes static costs per block and adjusts for dynamic terms.
- **RSP:** local clock in RCP ticks; coarse cost first (1 clock per instruction with dual-issue approximations), refined against `rsp_timing` tests; halted state advances by event, not by stepping (ares steps 128-cycle chunks; an event-based skip is cheaper and equivalent).
- **SP DMA:** double-buffered registers; `DMA_FULL` clears a few cycles before completion; duration approx. 3.7 B per PClock plus fixed overhead [COMMUNITY]. Data-copy timing (at start, progressive, or at completion) is **[UNKNOWN]**: start with copy at completion plus a hazard note, then let hardware-derived tests decide.
- **RDP:** commands execute functionally and in order when the command range is submitted (`DPC_END` write, unfrozen); busy counters and the `SYNC_FULL` interrupt follow a cost model. Known limitation, stated in the profile: a CPU read of the framebuffer between submission and the modeled completion sees the final pixels early. A later refinement makes RDP a lazily caught-up component with per-command costs and the RDRAM-read catch-up rule above. RDRAM bus contention between agents is **[UNKNOWN]** and not modeled initially.
- **PI/SI DMA:** completion events from domain/transfer timing; PI IRQ at completion, SI IRQ when the PIF RAM transfer completes [COMMUNITY: MI].
- **VI:** a half-line event stream; `VI_V_CURRENT` is computed from time, not stored; interrupt at the `V_INTR` match (progressive ignores bit 0; interlace has the one-line-early case); a **frame event** at field end drives output and `advance(frame_event)`.
- **AI:** per-sample fetch cadence from DACRATE; the IRQ fires at DMA **start** (when a queued buffer becomes active); output ring receives 16-bit stereo samples with their sample index and the rate in effect.

**Why not cycle-lockstep everywhere (cen64)?** It is the strongest accuracy baseline but pays full cost always and bakes pipeline detail the project cannot yet evidence. **Why not run-RSP-as-atomic-task (mupen64plus)?** It is microcode-specific heuristics with invented delays and cannot be an evidence-backed hardware claim. **Why not threads?** Host thread scheduling cannot be a source of guest ordering; cen64's barrier window is exactly where order stops being a function of guest time. Parallelism, when needed, goes inside a single deterministic step (e.g. RDP tile-parallel software rasterization with a deterministic merge) or into host-side post-processing, never across guest-visible interactions.

**Determinism rules (hard requirements for the core).**

1. `GuestTick` is advanced only by the scheduler; nothing reads host time.
2. Total event order is `(tick, class, sequence)`; the sequence counter is part of state.
3. All inputs enter as timestamped events in the queue (`queue_input` validates tick >= now and monotone per port).
4. All entropy is derived from an explicit per-profile seed (default constant). Power-on RAM/cache/RDRAM contents follow the profile (zeroed or seeded-PRNG), never uninitialized memory; libdragon's IPL3 itself accumulates entropy during boot and clears RDRAM, which is only reproducible if guest time and device timing are exact [IMPL: `boot/entropy.c`, `ipl3.c` comments; which sources it samples is [UNKNOWN] until read].
5. No iteration order from hash maps, no pointer-keyed ordering, no host-dependent float behavior (CP1 and RSP reciprocal tables via integer/soft arithmetic).
6. The result of `advance(stop)` is a pure function of (state, stop). Outputs are a pure function of guest history.

---

## RSP and RDP strategy and the reference path

### RSP: LLE interpreter baseline ([REC])

- **Why LLE first.** The RSP runs whatever microcode the game loads (graphics, audio, custom), so opcode coverage is not task compatibility [DNA]. LLE is microcode-agnostic, testable by ROM-level tests (nemu64-test has per-opcode groups, VU stress tests, `registers`, `wrap_around`, `rsp_timing` [IMPL]) and by hardware-derived vectors, and it makes every RSP-visible interaction (SP_STATUS signals, halt/broke, DMA queue, semaphore, COP0 DPC access) real instead of simulated.
- **Why not HLE.** mupen64plus's HLE-style path reads the task type from `DMEM[0xfc0/4]` and applies fixed delays [IMPL]; any such shortcut ties results to known microcodes and invented timing, which cannot support a profile-bound evidence claim. If an HLE path is ever added for speed or low-power targets, it is a separate named non-reference profile with explicit scope (microcode list, build hashes) and is never used to produce expected values.
- **VU implementation.** Portable scalar reference (lane loops, 48-bit accumulators, explicit saturation/clamp rules, integer reciprocal tables per n64brew/`nemu64-test` `vrcp32`/`vrsq32` stress tests) as the permanent oracle. gopher64 uses host SIMD intrinsics with a separate aarch64 shim [IMPL: `src/compat/aarch64.*`]; if SIMD is added, it ships only behind per-opcode exhaustive/random differential tests against the scalar reference (VU opcodes are small enough to fuzz heavily) and must not change results across hosts.
- **Later tiers:** decoded-instruction cache for IMEM (invalidate on DMA into IMEM and CPU writes), then JIT. ares v148 rewrote its RSP/CPU JIT [DNA/IMPL]; its `jitClockTarget` budget idea (run a compiled block no further than the next event or timer) is the right pattern: JIT blocks must stop at or before the next scheduler horizon and quantum.
- **Known hardware oddities to be handled as tests, not folklore:** SP_PC readable only while halted; single-step mode is reported broken (branches wrong, MFC0/MTC0 lose a 2-cycle writeback) and likely impractical to emulate exactly [COMMUNITY: n64brew RSP interface]; halt during certain opcode sequences can corrupt state [COMMUNITY, no reproducible case]. Declare unsupported behaviors explicitly in the profile instead of guessing.

### RDP and VI: software reference first ([REC])

- **Own the reference.** Write the software rasterizer, TMEM, color combiner, blender, coverage/9th-bit handling, and the software VI in-tree from n64brew/Nintendo documents and conformance tests. Rationale: [OWNER] prefers copy-with-provenance and a small tree, but the obvious copy candidate has an **unresolved license**: `ata4/angrylion-rdp-plus` has no root `LICENSE` file (raw URL returned 404 on master, observed 2026-11-09) and descends from angrylion's plugin and MAME-era code; ares's RDP is a separate implementation in ISC-licensed ares. Copy only after a per-file provenance audit; do not assume.
- **Oracles, out-of-tree.** Run Angrylion-Plus (as a reference build, never vendored) and `parallel-rdp-standalone` suites (`rdp-conformance` about 150 fuzz tests from a custom command generator, `vi-conformance`, `rdp-replayer`, `rdp-validate-dump`) as **differential evidence only**; their README says the reference is a fork that makes some undefined behavior deterministic, that noise is not reproduced exactly, and lists intentional divergences [IMPL: parallel-rdp-standalone README]. Record the oracle version and these limits with every adopted expected value [DNA].
- **Why not a GPU RDP in the canonical core.** parallel-rdp imports RDRAM directly as host-visible memory (`VK_EXT_external_memory_host`), processes asynchronously, and "sync points sit on a timeline... the policy is left to the integration code" [IMPL: README]. Guest-visible RDRAM writes would complete in host-driver-dependent order, which defeats the determinism requirement. It is MIT-licensed and attractive for a later *host-side* accelerated presenter fed by the core's raw outputs, as an explicitly non-canonical profile that must pass the conformance suites and either write back deterministically at sync points or declare framebuffer-readback unsupported.
- **Raw outputs.** RDP writes color/Z into RDRAM (with the 9th bit). VI scanout is a separate software stage producing `VideoFrame` records (pre-scaling). Host scaling, filtering and aspect decisions live entirely in the host. Do not assume 320x240; width/height/field come from VI registers [DNA].
- **VI register log.** Because ORIGIN/WIDTH/Y_SCALE can change during hblank and V_VIDEO changes mid-frame are ignored [COMMUNITY: n64brew VI], the VI keeps a guest-tick/half-line stamped log of register writes per field and renders at field end by replaying it. This keeps scanout a pure function of guest history without per-line rendering cost.

### Keeping a reference path while optimizing ([REC])

| Layer | Reference | Optimized variants (later) | Equality gate |
|---|---|---|---|
| CPU | Interpreter (tier-specific cost seam) | Cached/decoded interpreter, block JIT | Lockstep: run both for the same `stop` spec, compare `state_digest()` at every `advance` return and at randomized intermediate tick limits; plus ROM-level suites |
| RSP | Scalar+VU interpreter | IMEM decode cache, SIMD, JIT | Per-opcode fuzz over register/accumulator/flag state; whole-task replays; digest equality of DMEM/IMEM/VU regs at task end |
| RDP | In-tree software rasterizer | Tile-parallel software, SIMD, optional GPU presenter | Byte equality of RDRAM color/Z/9th-bit planes after each `SYNC_FULL` over (a) synthetic fuzz command streams (generator may be written against the parallel-rdp approach, synthetic so rights-clean), (b) recorded RDP command dumps from rights-cleared fixtures |
| VI | Software VI | Fast paths | Pixel equality per `VideoFrame` |
| Whole machine | Reference configuration of all of the above | Any optimized configuration | Output-stream digest over fixed (profile, content, input, stop) tuples |

Provide `tools/` support for **RDP command dump record/replay** (command list + the RDRAM/TMEM regions it touches), because it isolates RDP differences from CPU/RSP differences (parallel-rdp's `rdp-replayer` is the precedent [IMPL]).

---

## Core/host API shape

### Comparison of existing APIs (what to borrow, what to avoid)

| API | Borrow | Avoid |
|---|---|---|
| **libretro** (`libretro.h`, API version 1) | Small verb set: `retro_init/deinit`, `retro_load_game/unload_game`, `retro_reset`, `retro_run`, and the triple `retro_serialize_size/serialize/unserialize` with the rule that size never grows between load and unload so the host allocates once. Memory-region export (`RETRO_MEMORY_SAVE_RAM`, `RTC`, `SYSTEM_RAM`) for saves. Capability negotiation as a concept | `retro_run` = one video frame (frame-granular only; no "until guest tick T"); video/audio/input delivered through **callbacks during run** (core decides when to poll input; host time leaks in); string-keyed environment grab-bag (`GET_SAVE_DIRECTORY`, `GET_SYSTEM_DIRECTORY` mean the core knows host paths); the header itself warns that cores must not rely on global initialization and must reset globals in `deinit`, i.e. the model assumes one core per process; `RETRO_SERIALIZATION_QUIRK_*` flags (`INCOMPLETE`, `MUST_INITIALIZE`, `SINGLE_SESSION`, `ENDIAN_DEPENDENT`, `PLATFORM_DEPENDENT`) catalog exactly the savestate non-determinism we must not have |
| **ares** (`ares::Platform`, `Event`) | Typed event enum (`Frame`, `Power`, `Synchronize`, `Shutdown`) returned from the scheduler to the host; an explicit **synchronize-to-safe-points** pass before serialization; deterministic-entropy option (seed zero) | Process-global `platform`, `scheduler`, `system`, `queue`, `random`; libco coroutine threads (state lives on stacks, hence the Synchronize pass); **pull-style** `platform->input(node)` and `platform->pak(node)` (core requests host files and polls host input at its own moments); `System::unload()` calls `save()`, i.e. the core triggers persistence; options written straight into globals |
| **mupen64plus core API** | `device` struct as a container for *all* state; **backend interface vtables** for `audio_out`, `clock`, `storage`, `joybus_device` passed to `init_device(...)`: the right seam shape for devices; separate command vocabulary (`ROM_OPEN/CLOSE`, `RESET`, `ADVANCE_FRAME`, `STATE_SAVE/LOAD`, `CORE_STATE_QUERY`); debugger API kept apart | Plugin ABI (gfx/audio/input/RSP function tables receiving raw pointers into RDRAM/DMEM, `ProcessDList/ProcessAList/ProcessRdpList` called at core-chosen times); blocking `run_device()`/`M64CMD_EXECUTE` until stop (host cannot drive); globals (`g_dev`, `g_CoreConfig`, `g_FrameCallback`, `g_media_loader`); `clock` backend returns host time to the guest; `init_device` takes `randomize_interrupt`, a determinism knob that must not exist; storage backend lets the core call the host to persist |
| **LibFuturo** (local checkout, commit a28cccd, 2026-11-09; [IMPL] of a sibling project, not an N64 contract) | `create/destroy`, `load`, `set_input`, `advance(work_units)`, `copy_media/import_media` with `guest_generation`, snapshot size/capture/restore, caller-provided video/audio destinations, `struct_size` versioning, `lf_stop_reason` (`BUDGET_REACHED`, `REJECTED`, `CORE_HALTED`), profile and capability query, documented postconditions per call | Treat only as an adapter target. Gaps for N64: single `uint32 buttons` (no analog stick or accessory state); input sampled only at advance start (`LF_INPUT_AT_ADVANCE_START`), with no intra-advance timestamps; video formats GRAY8/RGB24/INDEX8 only (N64 produces 16/32-bit, variable size, interlaced fields); one fixed audio rate (N64 DACRATE varies); time unit is frames or source ticks. Do not shape the canonical API by it |

### Recommended API (C-ABI facade; names illustrative)

```c
// Pure queries, no instance needed
t64_status t64_profile_describe(const char *profile_id, t64_profile *out);   // exact machine/TV/RAM/boot/CIC/ports/saves/debug/timing-tier/sync-quantum

// Lifetime. No I/O, no clock, no threads. Allocator injected.
t64_status t64_create(const t64_create_options *opt, t64_instance **out);
t64_status t64_destroy(t64_instance **owned);

// Content admission: transactional. On any non-OK status the instance is exactly as before the call.
t64_status t64_admit(t64_instance *i, const t64_admit_request *req, t64_admit_report *rep);
   // req: profile_id, rom bytes (borrowed for the call), optional boot-data slot (e.g. user PIF; rights-gated by host),
   //      media images (EEPROM/SRAM/Flash/Pak), accessory selection, debug devices, power-on seed.
   // rep: effective profile id + hash, content digest, per-slot accept/refuse reasons.

// Timed logical input: validated (tick >= now, monotone per port); applied by the event queue at that tick.
t64_status t64_queue_input(t64_instance *i, const t64_input_event *ev, size_t n);

// Bounded advancement: any-of stop conditions.
t64_status t64_advance(t64_instance *i, const t64_stop_spec *stop, t64_advance_report *rep);
   // stop: tick_limit | cpu_instr_limit | frame_event(n) | output_full | breakpoint_set_id
   // rep : reason, end_tick, overshoot_ticks, instr_retired, outputs_pending, guest_condition flags

// Outputs: core-owned records valid until the next advance/destroy, or copy-out into caller buffers.
t64_status t64_drain_video(t64_instance *i, t64_video_frame *out, size_t cap, size_t *n);
t64_status t64_drain_audio(t64_instance *i, int16_t *dst, size_t cap_frames, t64_audio_info *info);
t64_status t64_drain_guestlog(t64_instance *i, uint8_t *dst, size_t cap, t64_guestlog_info *info);

// Media: bytes + generation. Core never writes files.
t64_status t64_media_describe(t64_instance *i, t64_media_info *out, size_t cap, size_t *n);
t64_status t64_media_export(t64_instance *i, const char *media_id, uint8_t *dst, size_t cap, size_t *len);
t64_status t64_media_import(t64_instance *i, const char *media_id, const uint8_t *src, size_t len); // only before first advance, or after reset

// Lifecycle
t64_status t64_reset(t64_instance *i, t64_reset_kind kind);     // SOFT (reset button, NMI path) | HARD (power cycle); media policy explicit
t64_status t64_unload(t64_instance *i);                         // back to Created; media exported by host beforehand if wanted

// State
t64_status t64_state_size(t64_instance *i, size_t *n);
t64_status t64_state_capture(t64_instance *i, uint8_t *dst, size_t cap);   // pure; allowed after any advance
t64_status t64_state_restore(t64_instance *i, const uint8_t *src, size_t n); // validates envelope first; failure leaves state unchanged
t64_status t64_state_digest(t64_instance *i, uint8_t out[32]);              // canonical hash over guest-visible state; separate from the serializer
```

**Instance lifecycle (state machine).** `Created --admit OK--> Ready --advance--> Ready` (guest conditions such as quiescent are reports, not states) `--unload--> Created`; `reset` stays in `Ready`; any internal invariant violation moves to `Poisoned`, after which only `state_digest`, `drain_*` and `destroy` are valid.

**Three error classes ([REC]; mirrors the runner outcome taxonomy in [OWNER] requirements).**

| Class | Meaning | Postcondition |
|---|---|---|
| **Refused** (`T64_REFUSED_*`, `INVALID_ARGUMENT`, `BUFFER_TOO_SMALL`, `UNSUPPORTED_PROFILE`, `MALFORMED_CONTENT`, `LIMIT_EXCEEDED`) | Caller/content problem detected before any state change | **Instance state bit-for-bit unchanged**; no partial admission, no half-applied input; detail code plus a machine-readable reason (which slot/field) |
| **Guest stop** (not an error: `rep.reason`/flags) | Guest reached tick/instruction/frame/breakpoint bound, or a guest-level halt/quiescent/RDP-crash/unsupported-hardware-behavior condition | Instance valid and resumable; deterministic |
| **Defect** (`T64_INTERNAL_DEFECT`) | Core invariant violated (a bug) | Instance `Poisoned`; never continues silently |

Pass/fail/boot-fail/timeout/missing-fixture/harness-error are **runner-level** outcomes derived from the core's stop reasons plus the guest log; the core must not know about test protocols.

**Content admission and parsing.** ROMs and media are untrusted [OWNER Security]: bound sizes before allocation, normalize byte order at admission (z64/v64/n64 detection is by header magic), reject unknown or oversized inputs, never infer save type from a title string, and refuse (not guess) when the profile cannot support the declared save device. Archive formats (zip, 7z) are host concerns; gopher64's `get_rom_contents` shows how much archive code can creep toward the core if the boundary is not held.

**Timed logical input.** Events are N64-logical (16-bit button mask, signed stick x/y per port, accessory insert/remove), not host key codes. The PIF samples device state when the joybus transaction runs (on the SI DMA read), so the sampled value is a function of guest time. A host that wants "input once per frame" queues one event per frame at the frame's start tick; a script or replay queues arbitrary ticks. This replaces libretro's poll callback and ares's `platform->input` pull.

**Bounded advancement.** `advance` stops at the first instruction boundary at or after the limit, reporting exact `end_tick` and overshoot (LibFuturo reports overshoot similarly). Stop conditions are any-of and combinable. Breakpoints and watchpoints are additional stop conditions, evaluated only by the debug-instrumented executor variant (see Observability).

**Outputs.** Pull after `advance` returns, from fixed-capacity core-owned rings. `VideoFrame { start_tick, end_tick, field, width, height, format (RGBA5551 | RGBA8888 | blank), stride, pixels }`; `AudioChunk { first_sample_index, rate_num, rate_den, frames[] }` (rate changes arrive as chunk metadata, not as host re-init); `GuestLog { tick, bytes }`. If the ring fills, `advance` returns `OUTPUT_FULL`; the core never drops, blocks, or calls out. RGBA expansion/gamma/aspect/scaling is host work.

**Save media.** Core keeps images in-instance with a `guest_generation` that changes deterministically with guest writes (LibFuturo's "guest change token, not a durability acknowledgment" semantics). The host exports when it chooses and owns staging, fsync, atomic rename and acknowledgment. Import is legal only at defined points (before first advance, or immediately after reset) so it cannot corrupt a running guest; a refused import leaves the existing image unchanged. RTC base time is an explicit admission input (profile default constant), never `SystemTime::now()`.

**State serialization.** A hand-written, versioned, endian-fixed envelope (profile id + hash, core build id, behavior revision, content digest, extent, payload schema version, digest of header) around a schema-explicit payload, not a derive-on-struct dump (gopher64's serde-derived `Device` is convenient but binds the format to struct layout [IMPL]). Because the core is a single-threaded plain-data machine, capture is legal after any `advance` return; no "synchronize" pass is needed. `restore` validates the envelope before touching state. `state_digest` is separate so differential tests and replay checks never depend on serializer details.

**Reset and unload.** Soft reset follows the hardware path (pre-NMI then NMI, CIC/PIF state implications) and differs in guest-visible CP0/cache/RDRAM state from a hard reset (nemu64-test's startup test allows soft-reset variance for Wired, Index, etc. [IMPL]); both are exact, profile-defined transitions that never touch host resources.

**Threading contract.** One instance is single-threaded and externally serialized (LibFuturo states the same: the host owns and serializes the handle [IMPL]); multiple instances are fully independent and may run on separate threads concurrently. This is only true if the isolation checklist below holds.

---

## Observability without perturbation

**Design ([REC]).**

1. **Trace sink is a compile-time parameter of the executors** (generic/template/macro), default a no-op that the compiler removes. The executor variants are: `fast` (no hooks), `trace` (sink hooks), `debug` (sink + breakpoint/watchpoint checks). Switching variant between `advance` calls must not change guest results; test this by digest equality.
2. **Events carry `GuestTick` and a global sequence number**, never host time. Categories: instruction retire (pc, word, tick), exception/interrupt taken, TLB write/miss, MMIO read/write (physical address, size, value, initiator, tick), DMA start/complete (kind, src, dst, len), SP halt/break/signal, DPC submit/SYNC_FULL, VI frame/field/line-interrupt, AI buffer start/sample underrun, PIF command, joybus transaction (channel, bytes), media write, scheduler dispatch.
3. **Sinks cannot call back into the core** (no reentrancy); they write to a fixed ring (or a user-supplied sink struct) and the host drains after `advance`. Sink cost is host-time only; it can never alter scheduling or guest time.
4. **ISViewer is a guest-visible device**, not a trace hook. nemu64-test writes up to 0x200 bytes at 0xB3FF0020 with word stores of packed bytes (`pack`), then writes the length to 0xB3FF0014; it first *detects* the device by writing a magic word and reading it back, and polls `PI_STATUS.io_busy` between writes [IMPL: `src/isviewer.rs`]. mupen64plus maps `MM_IS_VIEWER` at 0x13FF0000; gopher64 prints the buffer as UTF-8 on the length write [IMPL]. Model it as a device present only in an explicit **diagnostic/dev-cart profile** (real consoles with retail carts do not have it), appending `(tick, bytes)` to the `GuestLog` ring. The runner parses it (e.g. the `Done! Tests: N. Failed: M` line and per-test failure lines).
5. **Emulator-extension opcodes are not hardware.** nemu64-test also probes custom COP0 CO-function encodings 0x20 (detect), 0x25 (log) and 0x2c (ioctl exit/fast) and uses `xioctl_exit()` to end the run [IMPL: `src/emux.rs`, `main.rs`]. The ROM otherwise loops forever after printing. **Recommendation:** default completion detection is "guest-log pattern + bounded advance", which is hardware-faithful; the EMUX opcodes, if supported at all, are an explicitly named **harness-extension profile** that changes instruction semantics for those encodings and therefore must never be part of a hardware-accuracy profile. Decide this in the headless-runner phase and record it as an ADR.
6. **Debugger seam:** pause is simply `advance` returning; step is `instr_limit = 1`; breakpoints/watchpoints are stop conditions in the debug executor; register/memory inspection is a separate **side-effect-free `peek` API** distinct from bus reads (RCP register reads have side effects: `SP_SEMAPHORE` read sets the lock, `VI_V_CURRENT` write clears an interrupt). ares's GDB hooks must special-case 64-bit RCP access "to avoid freezing" [IMPL], which is the symptom of not having a peek path. Any `poke` into guest state is recorded as a stamped input event so replay stays exact; pokes are unavailable in profiles that claim reproducibility without them.
7. **Replay is free:** (profile, content, input log, stop specs) fully determines a run; periodic `state_digest` checkpoints make divergence localization a bisection on guest ticks.

---

## Data flow

### Content to outputs

```
host bytes (ROM, optional boot data, media images, accessory selection, profile id)
    |
    v  t64_admit  (transactional)
[ profile validation ] -- refuse (state unchanged) --> host   (unknown profile, bad size/magic/byte order, unsupported save/accessory combo,
    |                                                           boot-data slot rights not asserted by host, RAM config vs Expansion Pak mismatch)
    v
[ normalize ] ROM -> canonical big-endian image; header parse (entry, CRCs, region) as *data*, not as profile inference
    v
[ boot route ] profile.boot =
    A. hle-pif + open IPL3 (default): seed post-IPL2 CPU/COP0/RSP/MI/RI state and IPL2-era registers; copy ROM[0x40..0x1000) to DMEM;
       PC = IPL3 entry; IPL3 runs as GUEST CODE (RDRAM init skipped via preset RI_SELECT, or exercised in tier B)
    B. user-supplied PIF ROM (host-provided, rights-asserted): run IPL1/IPL2 as guest code with the PIF state machine + CIC model   [later profile]
    v
[ CPU/RCP execution under scheduler ]   events: input apply, DMA complete, VI half-line, AI fetch, Count/Compare, DP sync, SP resume
    v
[ raw outputs into rings, guest-tick stamped ]  VideoFrame | AudioChunk | GuestLog | TraceEvents | media dirty marks
    v  t64_drain_* / t64_media_export after advance returns
host: scale/filter/present, resample/play, parse guest log -> outcome, persist saves
```

**Boot contract details found this session ([IMPL]/[COMMUNITY]; each needs a boot-phase check):**

- Real boot: CPU starts at 0xBFC00000 (PIF ROM); CIC sends region ID, two seeds and a checksum; IPL1 copies IPL2 to IMEM; IPL2 DMAs IPL3 (ROM 0x40..0x1000) to DMEM, locks PIF ROM with command 0x10, sends checksum with 0x20/0x40; IPL3 inits RDRAM, copies its second half to RDRAM 0x80000040, DMAs the first 1 MiB of ROM, verifies the header checksum, resets RSP, clears interrupts, clears IMEM/DMEM, jumps to the header entry; the game must send PIF command 0x08 within about 5 s or the PIF halts the CPU via NMI [COMMUNITY: n64brew PIF-NUS]. Documented post-boot state: PIF-ROM locked, boot info at 0xBFC007E4, RSP reset, interrupts cleared; full CPU register/RDRAM/cache state is **not** documented there [UNKNOWN: needs additional sources].
- Libdragon's open IPL3 (repo is Unlicense at root; the `boot/` directory README states no separate license, so file-level headers must be checked before any redistribution [UNKNOWN]) loads an appended ELF (or flat binary in the `compat` build), clears RDRAM using RSP DMA from a zeroed IMEM buffer (including a DMA from RDRAM above 8 MiB that must read zeros), passes **boot flags in DMEM** (non-compat), and **skips RDRAM init when `RI_SELECT != 0`** (`if (!bbplayer && *RI_SELECT == 0) memsize = rdram_init(...)`) [IMPL: libdragon `boot/ipl3.c`, `boot/rdram.c`, README]. nemu64-test reads memory size from `DMEM[0]` and TV type from `DMEM` byte 9 and the ELF offset from `DMEM[12]>>16<<8` [IMPL: `main.rs`]. The prebuilt IPL3 binaries are signed for CIC 6102 [IMPL: README], which only matters when a real IPL2 verifies the checksum, not for HLE-PIF.
- **Two-tier RDRAM/RI model:** Tier A presets `RI_SELECT` and the post-init RDRAM register file so IPL3 skips initialization (cheapest path to the first ROM-level test; nemu64-test README suggests the same). Tier B models enough of the RDRAM register protocol (delay register, device-id assignment, broadcast writes, MI repeat mode, CC calibration read-back, refresh) for a cold-boot IPL3 and for nemu64-test's `tests/rdram` and `tests/mi/repeat` groups. Tier B is required for the full `base` suite [IMPL: those directories exist in the base set].

### State management

```
Machine (single owner; plain data)
   |-- scheduler { now, event heap, sequence }
   |-- cpu/cp0/tlb/caches/cp1, rsp, rdp, vi, ai, pi, si/pif, mi, ri, rdram(+9th bit)
   |-- devices { cart rom (immutable, shared by ref count of digest only), media images, controller state, isviewer buffer }
   |-- outputs rings, trace sink handle
   '-- profile (immutable)
```

Only `advance`, `queue_input`, `admit`, `reset`, `media_import`, `state_restore` mutate it. Everything else is read-only observation.

### Key flows

1. **Timed input to guest:** `queue_input(tick,...)` -> event queue -> `INPUT_APPLY` updates controller device state -> SI DMA read executes joybus channel transactions -> PIF RAM -> RDRAM -> game reads at its own time.
2. **Graphics:** CPU or RSP writes DPC START/END (or XBUS from DMEM) -> RDP executes in order -> RDRAM color/Z/9th bit -> `SYNC_FULL` -> DP interrupt event -> CPU handler; separately VI half-line stream -> frame event -> software VI reads RDRAM (+9th bit) using the register log -> `VideoFrame` to ring.
3. **Audio:** RSP audio microcode writes samples to RDRAM -> CPU programs AI address/length -> AI queue (2 deep) -> IRQ at start of transfer -> sample-cadence fetch -> `AudioChunk` ring with rate metadata -> host resamples.
4. **Save:** guest writes via PI cart bus (SRAM/Flash) or PIF channel 4 (EEPROM) -> media image bytes + generation bump -> host polls `media_describe`, exports when it wants.

---

## Architectural patterns to follow

### Pattern 1: Event queue with total order plus lazy catch-up
**What:** Components own local clocks; the scheduler runs the CPU to the next horizon, and `catch_up(component, now)` is called at observation points.
**When:** Always; this is the base.
**Trade-offs:** Less brute-force than cycle lockstep; correctness depends on the observation-point table being complete. Make that table a reviewed artifact and test each row with a targeted ROM or component test.
**Sketch:**
```rust
// language-neutral intent
fn mmio_read_sp_status(&mut self) -> u32 {
    self.rsp.run_until(self.now());      // catch-up first
    self.rsp.status.bits()               // then observe
}
```

### Pattern 2: Backend interfaces at device seams, state inside the core
**What:** Joybus devices, cartridge media and the ISViewer are objects behind narrow interfaces (command bytes in, response out, `advance(tick)`), with their state *inside* the instance and serialized with it.
**When:** All port/pak/save devices.
**Trade-offs:** Borrowed from mupen64plus's backend structs but with the host callouts (clock, storage write-through, audio out) removed; the host only exchanges bytes at API boundaries.

### Pattern 3: Reference-plus-optimized behind one trait with an equality harness
**What:** `trait Executor { run_until(tick) -> tick }` implemented by interpreter (reference) and later cached/JIT; same for RSP and RDP; harness drives both with identical stop specs and compares `state_digest()`.
**When:** From the first optimized variant on; the reference is never deleted.
**Trade-offs:** Maintenance cost of two paths; paid back by catching every optimization bug at its source. Also forces canonical state to live in plain structs, not in JIT registers, at executor boundaries.

### Pattern 4: Outputs as event streams with guest stamps
**What:** Video, audio and logs are records in rings; the host drains after `advance`.
**When:** Always.
**Trade-offs:** Slightly more copying than callbacks; gains: no reentrancy, no FFI callbacks in the hot loop, trivially testable, backpressure by stopping.

### Pattern 5: Profile as data, hashed into everything
**What:** The profile (console revision, TV, RAM, boot route, CIC, ports, accessories, save devices, debug devices, timing tier, sync quantum, RDP/VI fidelity tier, behavior revision) is an immutable descriptor; its hash is stamped on state snapshots, output records and test results.
**When:** From the first line of code. It makes [OWNER]'s "support claims bound to profiles" mechanically checkable.

### Pattern 6: CPU time driver with budget capped by the next event
**What:** The executor is handed `horizon` and must not run past it by more than one instruction (interpreter) or one block (JIT, where blocks are cut at `min(quantum, next event)` like ares's `jitClockTarget`).
**When:** Interpreter now; mandatory contract for cached interpreter/JIT later.

## Anti-patterns to avoid

| Anti-pattern | Why it fails | Instead |
|---|---|---|
| Frame-driven loop "CPU for N cycles, then RSP, then RDP, then VI" | Hides races, order, and DMA timing [DNA warns the same]; makes the frame the unit of correctness | Event-ordered scheduler with explicit observation points |
| HLE RSP tasks keyed on task type with fixed delays (mupen64plus) | Microcode-specific, invented timing, un-evidenceable | LLE reference; any HLE is a separately named non-reference profile |
| Coroutine/thread-per-component (ares libco, cen64 threads) | State on stacks forces synchronize passes; thread windows break guest-time ordering | Run-to-time functions over plain-data state |
| Callbacks from core into host during `advance` (libretro, ares, mupen64plus) | Reentrancy, host-time coupling, hard FFI | Pull outputs after advance; push timed inputs before |
| Core-initiated file/clock/entropy access (ares `pak()`, m64p clock backend, gopher64 `SystemTime::now`/OS RNG) | Host state becomes guest-visible | Explicit admission inputs; seed in profile |
| Singleton `g_dev`/`system`/`scheduler` | One machine per process; no isolation test possible | Instance handle; lint against mutable statics |
| Determinism knobs (`randomize_interrupt`) in the core | A knob that makes results non-reproducible is a defect by design | Remove; if randomization is wanted for fuzzing, it is seeded by the profile |
| Serializing by dumping the live struct (derive-based) with host pointers/JIT state | Format drift, platform dependence, restores that silently differ | Schema-explicit versioned envelope plus separate digest |
| Treating a GPU RDP as canonical | Async completion order, driver variance, required host-memory import extension | Software reference canonical; GPU only as non-canonical presenter |
| Equating "output hash matches another emulator" with accuracy | Agreement is not hardware evidence [OWNER] | Differential tests label oracle and version; hardware-derived vectors carry provenance |
| One giant PIF/IPL "HLE" that guesses state | Boot-state errors masquerade as game bugs | Document which state is seeded and why; test with startup/boot tests; keep a path to run real boot code |
| Treating the sync quantum as a performance flag | Changes guest-visible results | Quantum is in the profile hash |

---

## Suggested build order (dependency graph)

### Dependencies

```
guest time + event queue + profile + output rings + state digest
        |
        +--> bus/memory map + RDRAM + cart ROM + PIF-RAM stub + MI + RI(tier A) + SP regs/DMA (no RSP exec) + PI DMA + ISViewer + VI regs/line counter
        |         |
        |         +--> VR4300 integer interpreter + CP0 (Count/Compare/Status/Cause) + exceptions + cache ops accepted
        |                    |
        |                    +--> [M-A] open IPL3 boots (HLE-PIF seed) and a libdragon hello prints via ISViewer     <-- earliest headless ROM-level run
        |                    |
        |                    +--> TLB + 64-bit + LL/SC + alignment + cache model (functional) + CP1 soft-float
        |                    |         |
        |                    |         +--> [M-B] nemu64-test `base` CPU/COP0/TLB/cache/FPU groups under the runner (expected-results manifest)
        |                    |
        |                    +--> RSP LLE (scalar then VU) + SP_STATUS/semaphore/halt/break + RSP<->DMA ordering   (independent of RDP)
        |                    |         |
        |                    |         +--> [M-C] nemu64-test RSP groups; RSP timing tier
        |                    |
        |                    +--> RDRAM tier B + MI repeat mode  (needed for `base` rdram/mi groups and cold-boot IPL3)
        |                    |
        |                    +--> timing tier 1 (Count/Random per instruction, cache timing)  --> [M-D] nemu64-test `timing` feature
        |
        +--> RDP (CPU-driven display lists) --> software VI --> raw VideoFrame --> RDP/VI conformance (parallel-rdp suites, PeterLemon cases)
        |         \__ RDP does not depend on RSP: libdragon/other CPU-built lists can drive it
        +--> AI + audio ring  (audio tests; real use needs RSP audio ucode)
        +--> SI/PIF joybus + controller + timed input --> save media (EEPROM, SRAM, then Flash, Controller Pak) --> accessories
        |
        +--> API freeze candidate (after M-B + stable output records) --> installed-consumer test
        '--> playable libdragon homebrew = RSP (rspq/mixer) + RDP + VI + AI + SI input + saves + a frontend host + performance work
                (cached interpreter, RSP decode cache; JIT last)
```

### Phase-shaped recommendation for the roadmap

| Order | Slice | Exit evidence | Why here |
|---|---|---|---|
| 1 | Foundations: `GuestTick`, event queue (total order), profile descriptors and hash, output rings, state digest, dependency lint wall, headless runner skeleton with the 6-way outcome taxonomy and fixture manifest, installed-consumer scaffold against a stub API | Runner distinguishes pass/guest-fail/boot-fail/timeout/missing-fixture/harness-error on synthetic cases; wrong expected output fails; lint rejects a forbidden import | Everything else depends on time, profile and outputs; cheapest place to enforce isolation |
| 2 | Bus + RDRAM + cart + PIF stub + MI + RI tier A + PI/SP DMA + VR4300 integer interpreter + CP0 basics + ISViewer (diagnostic profile) + HLE-PIF boot of open IPL3 | **M-A:** a rights-cleared libdragon hello prints through ISViewer, headless, same digest on two OSes | Minimal device set that reaches the first ROM-level observation |
| 3 | Complete VR4300: 64-bit, LL/SC, exceptions, TLB, alignment, functional caches, CP1 soft-float | **M-B:** `nemu64-test` pinned fixture, expected-results manifest per test, negative controls | Largest block of independent expected values; drives CPU completeness |
| 4 | RSP LLE scalar+VU, SP semaphore/status/DMA/halt/break, RSP timing tier | `nemu64-test` RSP groups; VU per-opcode fuzz vs scalar oracle | Independent of RDP; unblocks microcode |
| 5 | RDRAM/RI tier B, MI repeat mode, timing tier 1 | `nemu64-test` rdram/mi/timing groups; cold-boot IPL3 | Closes `base`/`timing` coverage |
| 6 | RDP software reference + software VI + raw frames + RDP dump record/replay | parallel-rdp conformance (as differential), selected PeterLemon cases individually audited | Needs RDRAM/MI only; can proceed in parallel with 4-5 |
| 7 | AI + audio ring; cadence/IRQ-at-start tests; small authored audio diagnostic | defined sample/event expectations | Needs RSP audio ucode for real content |
| 8 | SI/PIF joybus, controller, timed input, EEPROM/SRAM media + export/import/refusal | save tests including refused-operation preservation | Needed for playable |
| 9 | API freeze candidate, installed consumer on the real core, then frontend host, playable libdragon homebrew, cached interpreter/RSP decode cache, benchmarks | named workloads on a repeatable baseline | [OWNER]: performance only after correctness baseline |

Parallelism for planning: slices 4, 6 and the AI part of 7 are mutually independent after slice 2/3 and can be separate workstreams. Slices 3 and 5 touch the CPU/RI and should not be run concurrently with large CPU refactors.

---

## Global mutable state and instance isolation

### What reference emulators actually do (observed 2026-11-09)

| Source | Global / shared state observed |
|---|---|
| ares | `system`, `queue`, `random`, per-component objects (`cpu`, `rsp`, `vi`, ...), `GDB::server`, `Scheduler scheduler` (single global instance), `Platform* platform`; options write directly into globals; Vulkan load flag; fixed JIT allocator released on power |
| mupen64plus-core | `struct device g_dev`, `g_CoreConfig`, `g_FrameCallback`, `g_media_loader`, `g_mem_base`, `g_EmulatorRunning`, `g_rom_pause`, `g_cheat_ctx` (about 21 file-level globals in `main/main.c` by a rough grep) |
| gopher64 | Core is a single `Device` struct (good), but `parallel-rdp/interface.cpp` has `static` `processor`, `wsi`, `window`, `rdp_device`, `gfx_info`, `rdram_tracker`, `g_tmem`, `g_hidden_rdram`; `ui::video::process_rdp_list` is a free function over that state; RNG/RTC seeded from host |
| cen64 | Instance-style `cen64_device`, but `setjmp/longjmp` unwind state in `bus.unwind_data`, host FPU state save/restore around the run, OpenAL/GL host coupling |
| libretro | The header explicitly cautions that cores may be statically linked or reloaded and must not rely on global initialization; the API shape assumes one active core per process |

### Checklist for Tendies64 ([REC]; each item gets a mechanical check)

| # | Hazard | Rule | Check |
|---|---|---|---|
| 1 | Component singletons / file-level mutable statics | None in `core/` | Dependency/static lint (no mutable statics, no thread-locals, no lazy-init globals) |
| 2 | Host callbacks stored globally | Core stores no host pointers; only values in the instance | Code review gate + lint on function-pointer fields in `core/` |
| 3 | Lookup tables built at power (RSP reciprocal tables in ares) | Compile-time constants or per-instance const data derived by pure code | Table-hash test |
| 4 | JIT code buffers / allocators | Per-instance; any shared immutable code cache is a later opt-in with an explicit key | Interleaving test with JIT on |
| 5 | RNG and time | Seed and RTC base are admission inputs; no OS entropy; no `now()` | Lint forbids time/random imports in `core/` |
| 6 | Host FP environment (rounding mode, flush-to-zero) | CP1 and RSP math do not use host FP; or wrap `advance` with a scoped FP-env guard and restore | Run with altered host FP modes; digests must match |
| 7 | Endianness | Canonical big-endian representation or one accessor layer (gopher64's `byte_swap` XOR is an example of the latter) | Cross-endian CI job or a big-endian-simulation unit test of accessors |
| 8 | Logging | Through trace/log sinks, never stdout/stderr singletons in core | Lint |
| 9 | Error unwinding | No `longjmp`/exceptions across the facade; invariant violations set `Poisoned` | Fault-injection tests |
| 10 | Allocation | Allocator injected; no allocation in the `advance` hot loop after admission (bounded memory, deterministic behavior) | Counting-allocator test |
| 11 | GPU/window/audio device state | Host-only | Dependency lint |
| 12 | Cross-instance interference | Two instances interleaved on one thread (advance A by x, B by y, repeat) must produce digests equal to each run alone; run the same on two threads under TSan | CI test, mandatory from slice 2 |
| 13 | Process-level side effects (signal handlers, env vars) | None | Lint/review |

A context-shaped API does not by itself prove isolation [DNA]; the interleaving-invariance test is the proof obligation.

---

## Scaling considerations (throughput, not users)

| Stage | Approach | What breaks first |
|---|---|---|
| ROM-level tests and boot (slices 1-5) | Plain interpreter, per-instruction event-horizon check, functional caches | Nothing; correctness dominates. Cost center: exact caches/TLB per access |
| Playable homebrew (slice 9) | Decoded-instruction cache for CPU and RSP IMEM; fast-mem pointer path for RDRAM/ROM; skip-ahead when RSP halted and RDP idle; tile-parallel software RDP with deterministic merge | Software RDP rasterization and per-instruction scheduling overhead while RSP is running |
| Commercial breadth, performance tier | Block JIT for CPU (horizon-capped blocks) and RSP (ares v148 precedent), accelerated *non-canonical* presenter | JIT invalidation (code pages written by DMA/RSP/CPU, TLB changes, cache ops) and keeping dynamic cost terms equivalent to the interpreter |
| Many instances (CI, fuzz farms) | Share-nothing instances, one per thread; immutable ROM image shared by digest only | Memory per instance (RDRAM + hidden plane + caches); keep allocation explicit |

Benchmarks are [OWNER]-gated: only after a repeatable correctness baseline; report throughput, frame/audio latency, memory and CI time separately.

---

## Integration points

### External

| Consumer/Service | Integration pattern | Notes |
|---|---|---|
| Headless runner | Links the public facade; owns fixture manifest, stop specs, outcome mapping, JSON result | First consumer; also the installed-package consumer |
| LibFuturo | Adapter in `host/adapters/` mapping its 1-`u32` input/frames-or-ticks time/fixed video+audio formats onto the richer N64 API | Lossy by construction; no N64 profile exists upstream; recheck its contract before any decision [DNA] |
| Frontends (SDL etc.) | Map keyboard/gamepad to timed logical input, scale RGBA frames, resample audio, persist saves | All wall-clock pacing lives here: the host decides how many guest ticks to request per wall interval |
| Differential oracles (Angrylion-Plus, parallel-rdp, other emulators) | Out-of-tree builds invoked by `tools/`; results labeled with oracle version | Never feed oracle output back into expected values without provenance |

### Internal boundaries

| Boundary | Communication | Notes |
|---|---|---|
| CPU <-> bus | Direct calls with typed accesses (size, cached/uncached, initiator) | Initiator tag feeds trace and the RDRAM-requester model (ares `RBusDevice`) |
| Scheduler <-> components | `run_until(tick) -> tick`, `catch_up`, event dispatch | The only place guest time moves |
| RSP <-> RDP | DPC register file; XBUS command fetch from DMEM | RSP issues commands via COP0 regs c8-c15 [COMMUNITY] |
| RCP units <-> MI | `mi.raise(source)` / `mi.clear(source)` | CPU samples IP2 at instruction boundary |
| Core <-> facade | Plain structs, no pointers retained across calls except core-owned rings | `struct_size` versioning |
| Facade <-> host | Bytes and typed records only | No callbacks during `advance` |

---

## Unknowns and research flags for roadmap phases

| Phase/slice | Flag | Item |
|---|---|---|
| 1 | Standard | Time/event/profile plumbing is conventional; research ADR for language, build and ABI (open [OWNER] decision) is needed before slice 1 code |
| 2 | **Deeper research** | Post-IPL2 register/COP0/cache/RDRAM state needed by the open IPL3 (UNKNOWN: n64brew documents what IPL3 leaves, not what IPL2 hands IPL3); libdragon `boot/` per-file licenses; which entropy sources IPL3 samples; ISViewer exact register semantics beyond the nemu64-test usage |
| 3 | **Deeper research** | VR4300 cache line sizes/write policy and Count rate against the NEC manual; FPU denormal/unimplemented-operation/FCR31 rules; stepping/revision selection (multiply bug, `sra` leak) as profile fields; COP0 hazards |
| 4 | **Deeper research** | RSP DMA data-copy timing (start vs progressive), dual-issue and stall rules for `rsp_timing`, DMEM/IMEM contention, halt-corruption cases |
| 5 | Moderate | RDRAM register protocol fidelity needed for `tests/rdram`; refresh/calibration read-back values |
| 6 | **Deeper research** | RDP command timing/cost model, TMEM and coverage details, license audit for any copied Angrylion-derived code, RDRAM bus contention between agents, VI exact filtering and per-line behavior |
| 7 | Moderate | AI sample-cadence phase relative to CPU time; 8 KiB carry bug test vectors |
| 8 | Moderate | Joybus transaction timing; Controller Pak semantics; EEPROM write time; Flash/SRAM cart-bus domains |
| any | Unknown | Exact NTSC/PAL/MPAL clock values and inter-domain phase (video vs system crystals); simple64 scheduling specifics (not inspected) |

---

## Sources

Hardware references (n64brew wiki is community documentation, not console truth; all accessed 2026-11-09):
- RSP interface: https://n64brew.dev/wiki/Reality_Signal_Processor/Interface
- RDRAM: https://n64brew.dev/wiki/RDRAM
- PIF-NUS (boot, PIF RAM, joybus): https://n64brew.dev/wiki/PIF-NUS
- RDP interface (DPC): https://n64brew.dev/wiki/Reality_Display_Processor/Interface and overview https://n64brew.dev/wiki/Reality_Display_Processor
- Video Interface: https://n64brew.dev/wiki/Video_Interface
- Audio Interface: https://n64brew.dev/wiki/Audio_Interface
- Peripheral Interface (incl. `n64_pi_dma_test` mention): https://n64brew.dev/wiki/Peripheral_Interface
- MIPS Interface: https://n64brew.dev/wiki/MIPS_Interface
- VR4300 (steppings, load interlock, bugs): https://n64brew.dev/wiki/VR4300
- NEC VR4300 datasheet (cache sizes, via search snippet; recheck the PDF): https://bitsavers.mirrorservice.org/components/nec/mips/Vr4300-ds_200011.pdf ; user manual: https://ftp.mirrorservice.org/sites/www.bitsavers.org/components/nec/mips/1995_NEC_VR4300_MIPS_RISC_Microprocessor_Users_Manual.pdf (not read in this session)
- Nintendo manuals via `EMULATOR-DNA.md` links (ultra64.ca / jrra.zone mirrors); not re-fetched this session
- Clock-tree forum teardown (LOW, single source, NTSC only): found via search, mirrored at https://dogcxf037x385.cloudfront.net/assemblergames.com/threads/mapping-n64-overclockability-achieved-3-0x-multiplier-but-not-3-0x-speed.51656/index.html

Emulator and test sources (implementation observation; clones shallow, 2026-11-09):
- ares (ISC): `ares/n64/{n64.hpp, system/system.cpp, cpu/cpu.cpp, rsp/rsp.cpp, rdp/rdp.cpp, rdp/render.cpp}`, `ares/ares/{platform.hpp, scheduler/scheduler.hpp, scheduler/scheduler.cpp}` on master via https://github.com/ares-emulator/ares ; release notes https://ares-emu.net/news/ares-v148-released (per DNA)
- cen64 (BSD-3): https://github.com/n64dev/cen64 at e0641c8452a3ae8edcd2bf4e46794bb4eaafc076 (`device/device.c`)
- gopher64 (GPLv3): https://github.com/gopher64/gopher64 at 19b03c96a72729a66fc3993a6c1f5d9a11258e3b (`src/device.rs`, `device/{events,cpu,rsp_interface,rsp_cpu,rdp,is_viewer}.rs`, `parallel-rdp/interface.cpp`)
- mupen64plus-core (GPLv2+): https://github.com/mupen64plus/mupen64plus-core at a9ca8ec0b7decd17390c76c6341cf423d5d79321 (`src/device/device.h`, `src/device/rcp/rsp/rsp_core.c`, `src/api/{m64p_types.h, m64p_plugin.h}`, `src/main/main.h`)
- libretro API header: https://github.com/libretro/libretro-common/blob/master/include/libretro.h
- parallel-rdp-standalone (MIT): https://github.com/Themaister/parallel-rdp-standalone (README)
- Angrylion RDP Plus (no root LICENSE found): https://github.com/ata4/angrylion-rdp-plus
- libdragon (repo Unlicense) IPL3 sources: https://github.com/DragonMinded/libdragon/tree/trunk/boot (`ipl3.c`, `rdram.c`, `entropy.c`, README)
- nemu64-test (MIT; successor to deprecated n64-systemtest): https://github.com/thelemmy/nemu64-test at 9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c (README, `Cargo.toml` feature comments, `src/{isviewer,emux,main}.rs`, `src/tests/` tree); deprecated predecessor https://github.com/lemmy-64/n64-systemtest
- LibFuturo local checkout a local libfuturo checkout at a28cccd03bc47e0b066aca668c99e00a6b45d85a, `include/libfuturo/libfuturo.h` (sibling project; not an N64 contract)

Project inputs: `.planning/PROJECT.md`, `AGENTS.md`, `.planning/bootstrap/EMULATOR-DNA.md`, `.planning/AUTONOMY.md`.

Not verified this session: simple64 internals, Project64 internals, Nintendo manuals, VR4300 user-manual cache/Count details, n64brew CP1/FPU and CIC pages, libdragon IPL3 register contract.

---
*Architecture research for: deterministic accuracy-first N64 emulator (Tendies64)*
*Researched: 2026-11-09*
