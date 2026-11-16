# Incremental Technology Stack: v1.1 Observable Machine

**Project:** Tendies64  
**Researched:** 2026-11-15  
**Scope:** Phases 7–9 only: RDP/VI, AI/raw capture, SI/PIF/controller/save media/state/replay. CPU, RSP and boot stack decisions remain in the [v1.0 research archive](../milestones/v1.0-research/STACK.md).  
**Confidence:** MEDIUM. Existing project decisions and current build structure are clear; D5 oracle authority and rights scope remain unresolved.

## Recommendation

Keep v1.1 dependency-flat. Implement these devices in the existing C17 core, extend the in-repository C test harness/CTest path, and let the headless runner own capture files. **No new library, service, test framework, renderer backend, codec, or serialization package is needed for these phases.** This is an incremental decision; retain the existing CMake 3.25+, Ninja, GCC/Clang, sanitizer/fuzzer and CTest setup.

Observed in the current tree: root CMake sets strict C17, a 3.25 minimum, warning checks, and separate core, runner and test targets. The project already has an in-tree harness and runner protocol. No builds, tests or guest execution were performed for this memo.

## Incremental stack

| Area | Pick for v1.1 | Reason / boundary |
|---|---|---|
| RDP and VI | Clean-room C17 software implementation in `core/` | Keeps canonical output deterministic and headless. A GPU/API backend adds no required capability to this milestone. |
| RDP/VI diagnostics | Project-authored command lists and independent literal expectations | Derive expectations from cited hardware documentation, transparent hand calculations, and reviewed vectors. Do not let the implementation generate its own expected pixels. Label emulator comparisons as differential evidence, not hardware qualification. |
| AI samples | Existing in-tree buffers and scheduler; exact integer sample assertions | Preserve sample count, values, guest timing and DMA boundaries. Do not add a playback or resampling library to the machine core. |
| Raw video/audio capture | Narrow writer in the headless host/runner using the C runtime | Store byte-exact payloads with a versioned metadata record for profile/build, pixel or sample format, dimensions/stride or sample rate, and guest frame/tick/index. Keep encoding and file I/O out of the core. No image/audio codec is needed. |
| SI/PIF and controller | In-tree protocol/device code | The standard controller path does not require a controller or Joybus library. Keep input events logical and guest-timed; physical mapping remains a future host concern. |
| EEPROM/SRAM | Explicit in-tree device models and bounded byte import/export | Distinct media types and refusal postconditions are easier to inspect and test directly than behind a persistence package. |
| Machine state/replay | Explicit, versioned, fixed-endian byte encoding; host owns files | Serialize named machine/device fields and timed logical inputs. Do not write C structs or add protobuf, SQLite or a generic serialization runtime. A format change should bump its version and preserve refusal-without-mutation behavior. |
| Test/safety tools | Existing CTest, in-repo harness, Clang ASan/UBSan and libFuzzer toolchain paths | Extend existing target patterns for new decoders, save imports and state/replay readers. No new test framework or fuzzing service. |

For captures, prefer lossless raw payloads plus metadata over PNG/WAV conversion in this milestone. Keep any viewer/converter out of the dependency graph; add one only if a concrete review workflow later needs it. This avoids turning host presentation or codec policy into machine output semantics.

## RDP oracle and rights boundary

**D5 stays unresolved. Do not fetch, build, run, copy, or publish Angrylion-Plus or data derived from it under this memo.** Its official repository currently includes `MAME License.txt`; that text says redistribution/use may not be sold or used in a commercial product or activity and sets source conditions for modified redistribution. This is the repository's stated license text, not a conclusion that it is GPL and not a determination about local black-box use. The existing owner stop controls that question. [Angrylion-Plus license](https://github.com/ata4/angrylion-rdp-plus/blob/master/MAME%20License.txt), [upstream README](https://github.com/ata4/angrylion-rdp-plus/blob/master/README.md).

The official `parallel-rdp-standalone` README describes roughly 150 RDP fuzz tests and VI conformance tests. It says the RDP tests compare bit-exactly against Angrylion-Plus, and its build instructions initialize submodules that include Angrylion-Plus and Granite. The standalone project's `LICENSE` is MIT; that does not by itself clear every test file, generated vector, submodule or expected-data artifact. **Do not make this suite a dependency.** Any later test-only use needs a per-file and asset audit plus D5 disposition, and remains emulator-to-emulator evidence. [README](https://github.com/Themaister/parallel-rdp-standalone/blob/master/README.md), [LICENSE](https://github.com/Themaister/parallel-rdp-standalone/blob/master/LICENSE).

Until D5 is resolved, Phase 7 can proceed with authored, spec-grounded vectors and a clean-room implementation. If D5 is later cleared for a narrowly scoped local oracle, pin its exact revision/build/modifications, keep it outside the product and public CI, retain oracle identity in reports, and do not elevate agreement to a hardware-accuracy claim. A documented authority path and materially useful additional coverage are the evidence that would change this recommendation.

## Carry-forward and defer

- Preserve **D2**: `nemu64-test` ROMs, builds and results remain local-only; do not commit/upload them or make public CI depend on them without documented rights. Public milestone diagnostics should be authored and rights-reviewed.
- Keep SDL3 deferred to the later optional host phase. Its official SDL3 docs describe windows/events/rendering and device audio streams/playback; that is a presentation/input boundary, not a need for raw headless file capture. If a live desktop host is built in v1.2, use the existing roadmap's separate optional SDL target and keep the core free of SDL headers. [SDL video](https://wiki.libsdl.org/SDL3/CategoryVideo), [SDL audio](https://wiki.libsdl.org/SDL3/CategoryAudio).
- Do not add Vulkan, OpenGL, FFmpeg, libsndfile, or a new JSON/database/state library for v1.1. Revisit only when a measurable consumer requirement cannot be met cleanly by the current core/runner boundary.

## Cheap actual-consumer preflight before long suites

Before a long phase suite, verify build/profile identity, manifest schema and rights, and new device/profile wiring. Then prove one **accepted authored operation through the actual runner/consumer**: e.g. a minimal RDP list yields its literal expected memory/output result; one AI DMA yields the expected sample count and values; a controller poll or explicit save device accepts the selected profile and round-trips a small authored value. Keep negative refusal controls too; a refusal-only smoke does not prove the new profile reaches the device. Use the installed consumer once that seam exists. This is readiness evidence, not hardware qualification.

## Evidence and confidence

| Claim type | Findings |
|---|---|
| **Observed project state** | Current `CMakeLists.txt` selects C17/CMake 3.25 and composes core, runner and tests. Existing v1.0 stack records the compiler, CTest, harness, sanitizer/fuzzer and boot-fixture choices; this memo does not reopen them. |
| **Primary-source facts** | Current Angrylion-Plus repository contains its MAME license text. The parallel-rdp-standalone README describes its Angrylion-based comparison and submodule setup; its repository LICENSE says MIT. SDL3's official docs describe window/presentation and playback APIs. |
| **Inference / recommendation** | These v1.1 features fit the existing C17 host/core split without new dependencies. Raw capture and versioned state bytes can be implemented with narrow in-tree code and the C runtime. |
| **Unknown** | Owner/legal disposition for any Angrylion local-oracle use, including whether any derived expected data could be retained or published; license status of individual parallel-rdp test assets not audited here; exact capture and state schemas remain phase design work. |

## Sources

- Current project structure: [`CMakeLists.txt`](../../CMakeLists.txt), [`v1.0 STACK.md`](../milestones/v1.0-research/STACK.md), [`ROADMAP.md`](../ROADMAP.md), and owner policy in [`AUTONOMY.md`](../AUTONOMY.md).
- Angrylion-Plus upstream: [README](https://github.com/ata4/angrylion-rdp-plus/blob/master/README.md), [MAME License.txt](https://github.com/ata4/angrylion-rdp-plus/blob/master/MAME%20License.txt).
- Themaister upstream: [parallel-rdp-standalone README](https://github.com/Themaister/parallel-rdp-standalone/blob/master/README.md), [LICENSE](https://github.com/Themaister/parallel-rdp-standalone/blob/master/LICENSE).
- SDL upstream docs: [CategoryVideo](https://wiki.libsdl.org/SDL3/CategoryVideo), [CategoryAudio](https://wiki.libsdl.org/SDL3/CategoryAudio).
