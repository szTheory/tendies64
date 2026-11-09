# Project idea: Tendies64

## What to build

Build a high-quality, accurate, deterministic and useful Nintendo 64 emulator
under the owner-selected working name **Tendies64**. Keep support claims tied
to explicit hardware/content profiles and reproducible evidence. Aim for a
portable machine core with useful host integrations and fast, automated
engineering feedback. This seed is kickoff input, not a final hardware
specification, compatibility promise, implementation design, API/ABI, license
or fixed first milestone.

The owner chose to start this project as part of a demand-aware strategy for
broad console coverage and progressively harder emulator work. A 2026 study of
1,099 Nintendo Switch Online subscribers' logged classic-console sessions
reported the Nintendo 64 as the strongest system-specific playtime signal in
its sample. This is useful evidence of play in that sampled ecosystem, not
market-wide use or proof of unmet demand for another emulator. ([Study record](https://doi.org/10.1037/ppm0000666))

Existing N64 emulators are mature. GSD should identify Tendies64's concrete
value beside current options, such as owner-controlled implementation,
well-defined accuracy evidence, an independent integration route, developer
experience or another demonstrated gap. Do not claim a market gap or broad
compatibility from historical sales, an emulator comparison or a single test
suite.

## North Star

- Emulate explicitly named Nintendo 64 machine, boot, region and expansion
  profiles faithfully; tie each support claim to versioned evidence.
- Make CPU, RSP, RDP, video interface, audio interface, shared memory and
  peripherals deterministic and observable at meaningful guest-time boundaries.
- Keep the canonical machine independent from host clocks, filesystems,
  physical controller mapping, presentation devices, UI and durable storage.
- Aim for strong performance without changing correct results. Benchmark named
  workloads and configurations only after establishing a repeatable correctness
  baseline.
- Make content handling, debugging, testing, packaging and integration fast,
  repeatable, secure and rights-aware, with a small dependency surface.
- Keep the long-term capability goal ambitious while sequencing accurate,
  independently verifiable slices. A narrow first profile is not the ceiling.

## Product and hardware questions for GSD

Keep the base cartridge system separate from optional profiles and add-ons.
Research and explicitly scope:

- Console revision and TV/region behavior (NTSC, PAL, MPAL), frame timing,
  analog/output interpretation and boot profile.
- Base 4 MB RDRAM and Expansion Pak 8 MB configuration.
- Cartridge header/country handling, PIF/CIC boot, IPL3 or other clean-room
  diagnostics, cartridge DMA and media rejection behavior.
- CPU VR4300/MIPS III behavior, caches/TLB/virtual memory, exceptions, alignment,
  CP0, bus timing, PI/DMA and device scheduling.
- RSP instruction/task execution and microcode profile; RDP command behavior,
  synchronization, rasterization, blending, depth, texture and edge cases.
- VI scanout/timing/filtering, AI sample DMA/audio timing, interrupts and their
  interaction with CPU/RSP/RDP work.
- Save media (EEPROM/SRAM and variants), Controller Pak, Rumble Pak, Transfer
  Pak, controller/SI/PIF protocols and other cartridge/controller hardware.
- 64DD media, boot, RTC and persistence; decide explicitly whether it is a
  later separate profile.
- Language/toolchain, public API/ABI, headless runner, installed consumer,
  supported hosts, state/save ownership, dependencies and threat model.
- License, repository visibility, first public push, release channel and
  versioning. Do not infer these from the emulator name or another sibling.

Do not silently promise all regional variants, every game, 64DD, every accessory,
all microcodes or every cartridge modification in the initial release. State
what is deferred and what evidence would promote it. Use a verifiable first
profile as sequence, not as a permanent feature ceiling.

## Suggested first proof to evaluate

Consider a cartridge-first vertical slice with a clearly named boot/region/RAM
profile, an original or rights-reviewed source-built diagnostic, deterministic
headless input, machine-readable guest results, guest-time limits, raw video
and audio capture, and an independent installed consumer. Evaluate the
`nemu64-test` suite as an early ROM-level candidate after auditing its current
revision, IPL3/toolchain inputs, each fixture's rights and expected-result
source. Add focused native component tests and a real-hardware-derived oracle
only where it improves a material claim. Keep a legally cleared playable
homebrew path visible so the project does not become tests-only.

This is sequencing advice, not an accepted implementation plan. Test results
from an emulator or its own microcode are not physical hardware proof. A
matching hash proves repeatability, not accuracy. Never bundle copyrighted PIF,
IPL, commercial ROMs, game saves, or 64DD media without explicit rights.

## Core, host and integration

Keep one canonical N64 machine implementation. The core owns deterministic
machine/device state and guest behavior; the host owns file selection,
wall-clock pacing, physical controller mapping, UI, audio/video devices,
transport and durable save policy. Define instance lifetime, selected profiles,
content admission, timed logical input, bounded advancement, raw output buffers,
reset/unload, failures and refusal postconditions before freezing a public
interface.

LibFuturo is an integration candidate, not an accepted dependency or qualified
N64 route. Its reviewed experimental API has no N64 profile. A future adapter
must test real N64 guest-work/input boundaries, boot plus cartridge naming,
RSP/RDP/VI output representation, audio cadence, expansion/save-media semantics,
resource bounds and an installed consumer. A successful adapter build does not
establish N64 accuracy or a qualified product route. Recheck the current
LibFuturo contract before any milestone decision.

## Autonomous OpenGSD direction

The owner requested autonomous milestone work after project kickoff, a rolling
roadmap, fan-out discussion using relevant experts, useful automated
verification, measured CI-efficiency reviews and milestone releases when the
exact artifact is ready. Follow `.planning/AUTONOMY.md`; do not re-ask for
routine decisions it already delegates. Keep rights, project visibility and
other stated hard stops explicit. Project generation and later autonomous
execution are separate GSD commands documented in `START-HERE.md`.

## Still open

Tendies64's specific product value; initial console/TV/RAM/boot profile; CPU and
system timing model; RSP and RDP strategy; video/audio output contract; cartridge
save-media baseline; peripherals and 64DD; diagnostic and playable fixtures;
language/build system; API/ABI; host and public distribution; LibFuturo fit;
license/visibility; performance targets; and version/release plan remain to be
researched or decided. Preserve owner direction, source facts, current code
observations, measured results, inferences, recommendations and unknowns as
distinct categories.
