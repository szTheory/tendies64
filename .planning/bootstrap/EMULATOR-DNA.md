# Emulator engineering DNA for Tendies64

Prepared 2026-11-09 as self-contained kickoff research. This combines owner
workflow preferences, current N64 hardware references, test-discovery leads,
and lessons from recent emulator and core/host projects. It is not a complete
hardware specification or an adopted architecture. Recheck mutable details
before using them as requirements.

## Evidence labels and limits

Distinguish owner direction, primary-source fact, community reference,
implementation observation, historical report, measured result, inference,
recommendation and unknown. Nintendo programming manuals and SDK material are
hosted by archival mirrors; cite the document version/section and verify
sensitive details. Emulator code and its passing tests show that implementation
behavior, not automatically console truth. A deterministic digest proves
repeatability, not accuracy. A ROM license and its generated binary/embedded
assets are separate rights questions.

## N64 hardware map

| Area | Starting evidence | Decision or common footgun |
|---|---|---|
| CPU/RCP | Nintendo's manuals describe a VR4300-family MIPS III CPU (93.75 MHz) and Reality Co-Processor (62.5 MHz), including CPU caches/TLB and separate RSP/RDP units. | Verify the exact processor/profile contract. Cache, virtual-memory, exception, alignment, DMA and device timing can be visible to games. Do not turn a clock rate into an adequate scheduler design. |
| RSP | The RSP has 4 KB instruction and 4 KB data memories and runs loaded task microcode for graphics and audio. Nintendo documents multiple standard microcodes; software may use variants or custom work. | Opcode coverage is not task/microcode compatibility. Keep HLE/LLE or hybrid choices and evidence scope explicit; do not assume one fixed N64 game pipeline. |
| RDP | The RDP consumes display lists and implements a graphics pipeline including texture, raster, depth, blending, coverage and synchronization behavior. | CPU/RSP/RDP overlap and shared memory make fixed sequential frame rendering risky. Decide what command/timing fidelity is required and measure optimized paths against a reference baseline. |
| RDRAM and scheduling | Base memory is 4 MB; an Expansion Pak adds 4 MB. CPU and RCP can execute concurrently; DMA, RSP, RDP and VI interact with shared resources. | Treat Expansion Pak as a named profile/configuration. Define bus ownership, DMA ordering, timing, interrupts and bounded guest work; a CPU-then-GPU frame loop may hide races/order. |
| Video interface | VI reads framebuffer data from memory and controls output timing/filtering. Nintendo SDK documentation includes NTSC/PAL/MPAL, different resolutions, interlace, pixel formats and VBlank-related behavior. | Do not assume a fixed 320×240 final framebuffer. Keep RDP output, VI scanout and host scaling/presentation separate. |
| Audio interface | RSP audio microcode creates samples in memory; AI DMA feeds a stereo DAC and timing/interrupt behavior. Nintendo SDK advises audio-task priority when graphics can overrun audio deadlines. | Model audio generation, buffer DMA, interrupts and sample cadence separately from host playback. “Non-silent” or a PCM hash alone proves little. |
| Boot/regions | CIC/PIF and IPL3 behavior affects startup; OS TV type and region/cart behavior are related but distinct. TV types include NTSC, PAL and MPAL. | Keep commercial PIF/boot bytes out of the repository and public CI. State the tested boot route (original hardware data, user-supplied data, or independently authored path) and its exact limitations. Do not infer the full profile from one ROM header field. |
| Cartridge and saves | PI provides cartridge access/DMA. Documentation lists optional EEPROM (4/16 Kbit) and SRAM; save media is not one interchangeable byte array. | Test recognized and unknown save devices, extents/protocols, writes, import/export and refused-operation preservation. Do not infer from a title string. |
| Controller/accessories | SI/PIF handle controller communication and port devices. Controller Pak, Rumble Pak and Transfer Pak are distinct accessories. Transfer Pak uses 32-byte transfers; the reviewed manual version describes specific GB MBC support and labels some GBC support planned. | Keep each port device/protocol as a profile choice. Transfer Pak can require another GB machine and separately cleared content. Avoid silently extending baseline support. |
| 64DD | Nintendo's separate 64DD docs describe disks with writable areas, RTC, internal ROM and an included Expansion Pak. | Treat disk boot, media, writes, RTC, persistence and expansion as a separate later system profile unless the owner selects otherwise. |

Reference leads (Nintendo-authored documents mirrored by archives): [Hardware
Architecture, Chapter 3](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro03/index.htm),
[RSP graphics](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro11/contents.html),
[RDP](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro12/index.htm),
[Video Interface](https://jrra.zone/n64/doc/pro-man/pro30/30-01.htm),
[Serial Interface devices](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro26/contents26.4.html),
[Transfer Pak](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/pro-man/pro26/26-07.htm),
[64DD Programming Manual](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/64ddman/dd01/index.htm),
and [CPU/RCP concurrency overview](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/kantan/step1/2-3.html).
These are useful archived primary-document contents, not proof every detail is
complete or applicable to all N64 revisions.

## Verification and fixture leads

- [`thelemmy/nemu64-test`](https://github.com/thelemmy/nemu64-test) is the
  current recommended lead over the deprecated `n64-systemtest` repository.
  Its README describes CPU/COP0, LL/SC, 64-bit operations, exceptions, TLB,
  memory access widths and RSP tests. The suite reports status and mirrors text
  to ISViewer-style registers, which can support a headless result protocol.
  Some timing/stress/RDP groups are optional or experimental; pin a full commit
  and audit each fixture/expected value before CI use. The repository's MIT
  license does not by itself clear every generated ROM or embedded asset.
- [`PeterLemon/N64`](https://github.com/PeterLemon/N64) offers broad CPU, RSP,
  RDP, RDRAM, VI, input, interrupt and sound homebrew tests. Its author reports
  real-console testing with a 64drive; verify individual cases and terms. The
  project identifies Unlicense terms, separate from other suites.
- [`parallel-rdp-standalone`](https://github.com/Themaister/parallel-rdp-standalone)
  documents generated RDP and VI comparison suites against Angrylion-Plus.
  This is useful differential conformance material, not independent physical
  hardware truth. Record reference implementation version and deterministic
  limitations with any adopted expected result.
- The commercial [Retro Regen N64 hardware test suite](https://retroregen.com/test-suites/retro-regen-nintendo-64-hardware-test-suite/)
  has restrictive terms against modifying or rebranding; it is not a fixture to
  vendor or adapt without permission.
- `nemu64-test` uses Libdragon's open-source IPL3 boot code and therefore offers
  a test path without bundling Nintendo PIF ROM data. Audit the generated ROM,
  boot/toolchain inputs, dependency status and exact allowed use first. Model
  the tested IPL3 route explicitly; it does not settle all original-PIF boot
  behavior.

### Staged verification ladder to evaluate

1. Fast native CPU, CP0, memory/bus, RSP and device-register tests with
   independent expected values and edge/negative cases.
2. A pinned, rights-reviewed headless `nemu64-test` run with fixed reset/input,
   ISViewer result capture and a time/instruction bound. Distinguish guest
   failure, boot failure, timeout, missing fixture, harness error and pass.
3. Focused RSP/RDP/VI tests as implementations land. Keep unstable/experimental
   fixtures out of required CI until their expected behavior is justified.
4. Add a small authored audio diagnostic with defined event/sample expectations;
   keep listening tests and physical captures supplemental where they add value.
5. Put longer timing, stress, regional, expansion, 64DD and broad game checks in
   scheduled/release lanes only when they catch a distinct failure class.
6. Test a clean installed package using the same permitted headless fixture and
   result protocol, including content refusal, save behavior and teardown where
   supported.

Use meaningful harness negative controls: wrong expected output must fail;
missing, malformed, timed-out or crashed tests must not pass as an empty result.
For hardware-derived vectors, record console revision/region, flashcart,
fixture/build hash, capture method and the exact values sourced from hardware.

## Current emulator ecosystem

The N64 emulator ecosystem is established and continues to evolve. The official
[ares v148 release](https://ares-emu.net/news/ares-v148-released) dated
2026-05-30 reports N64 CPU/RSP JIT rewrites with performance gains and other
ongoing implementation changes. Its [compatibility table](https://ares-emu.net/compatibility/nintendo-nintendo-64)
tracks title support status and emulator version, and marks some entries
unverified; this is a good pattern for version-bound support claims, not an
independent oracle. [Project64](https://github.com/project64/project64) includes
N64 and 64DD with plugin options. [Mupen64Plus core](https://github.com/mupen64plus/mupen64plus-core)
documents separate graphics, sound, input, RSP and frontend components. These
are alternatives/reference points, not a mandate to adopt the plugin
architecture or copy code.

As reviewed 2026-11-09, LibFuturo remains an experimental C source API with
synthetic, NES and Atari 7800 profiles and no N64 profile. Treat it as a
candidate only. An adapter build is not a qualified N64 route; test timing,
input sampling, bounded work, output ownership, boot/content, expansion/save
semantics and an installed consumer before considering adoption or contract
changes. Recheck current LibFuturo docs when a real decision depends on them.

## Transferable project lessons

- **Across the emulator portfolio:** explicit profile names, machine/host
  boundaries, deterministic timed input, semantic guest progress plus separate
  raw A/V, negative controls, exact fixture provenance, refusal postconditions,
  an independent installed consumer, and current project-state checks reduce
  false confidence.
- **Do not transfer stops.** A sibling's hardware facts, API, CI cap, current
  blocker, release state, license, fixtures or approval is project-specific.
  Schwaystation is an early PS1 project; its plan/tests are not N64 CPU or 3D
  evidence. Recheck it only if its implementation yields a concrete lesson.
- **One canonical machine core:** expose explicit instance/content/input/time/
  output/lifecycle/error behavior. A context-shaped API does not prove thread
  safety or instance isolation; inspect mutable global state and reset paths.
- **Measure performance:** establish correctness and named workloads first.
  Separate emulation throughput, frame/audio latency, memory/resource use and
  CI queue/setup/build/test time. Optimize the measured bottleneck; measure
  before adding caches, concurrency or wider matrices.
- **Small dependency tree:** copy a small helper only when its provenance and
  maintenance owner are clear; require concrete benefit for dependencies.
- **Milestone hygiene:** maintain main green, triage PRs, refresh a rolling
  roadmap and lessons, and release the exact tested artifact whenever the
  project-approved rights and publication path allow.

## Open decisions GSD should keep explicit

First machine/TV/RAM profile; boot/CIC/PIF/clean IPL3 path; CPU/cache/TLB and
system-time model; RSP strategy/microcodes; RDP/VI fidelity and host rendering;
audio cadence; cartridge/save profiles; controller accessories; 64DD; first
rights-cleared diagnostic and playable fixture; APIs and host; performance
workloads; language/dependencies; LibFuturo fit; threat model; license and
visibility; release/version policy. These are research topics and decisions,
not promises that every profile belongs in the first milestone.

No emulator, ROM, hardware, build, test, CI, source implementation or release
was run or changed during this bootstrap research.
