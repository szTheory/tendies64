# Feature Scope: v1.1 Observable Machine

**Domain:** A deterministic, headless-first N64 machine core with raw output and reproducible state/input artifacts  
**Researched:** 2026-11-15  
**Confidence:** MEDIUM overall; HIGH for owner/requirement scope, MEDIUM for hardware-visible behaviors grounded in the archived Nintendo programming manual and headers.

## Evidence labels

`[OWNER]` project direction; `[REQ]` archived v1.0 requirement; `[PRIMARY]` Nintendo-authored programming manual/header via archived mirror; `[INFER]` synthesis; `[REC]` recommendation; `[OPEN]` requires phase research before a hardware-accuracy statement. A passing regression or emulator comparison is evidence about that software run, not by itself a hardware oracle.

## Recommendation

Keep the existing 17 requirements in Phases 7–9. Deliver the smallest useful consumer path early in Phase 7: an authored command list fills a small RDRAM surface, VI scans it, and the headless runner emits one raw frame whose expected pixels are independently derived. Add a wrong-pixel control before expanding to complex primitives or long suites. This is reversible within the existing phase and demonstrates the complete output path quickly.

Use authored source-built diagnostics and independently computed expected values as the acceptance source. Pin successful outputs only as regression baselines after that independent expectation exists. A pin records repeatability; it does not establish oracle agreement or hardware accuracy. Do not upload ROMs or private diagnostic results.

## Table stakes for this milestone

| Requirement | Accepted behavior and observable criteria | Evidence / scope |
|---|---|---|
| **VID-01** | In-tree RDP consumes authored commands and writes RDRAM for fill, copy, textured/shaded triangles, combiner, blender, coverage and Z. Start with a fill path; expand by command class, each with independent expected pixels and edge cases. | `[REQ]` Exact command scope is already assigned. `[PRIMARY]` Nintendo documents RDP fill/copy and raster pipeline modes. No copied renderer. |
| **VID-02** | Full Sync completes in command order and raises the DP interrupt at the documented completion boundary; test pending/masked interrupt behavior through existing MI observables. | `[REQ] [PRIMARY]` Manual describes explicit RDP synchronization and host notification after list processing. Avoid inventing an unverified cycle count. |
| **VID-03, VID-06** | VI derives frame dimensions, field/interlace, pixel representation and VI filter state from guest registers/profile; emits the VI scanout before host scaling or host-only filters. Check a short authored frame against independent pixel expectations. | `[REQ] [PRIMARY]` VI has multiple pixel modes, filtering and scaling. Keep RDP-written RDRAM, VI scanout and host presentation as separate outputs. The selected profile remains NTSC / 4 MiB; this does not add PAL/MPAL profiles. |
| **VID-04, VID-05** | A conformance record names each oracle/version/modification and calls results differential evidence. Every chosen noise or undefined-memory policy is named and deterministic. | `[REQ] [OWNER]` D5 applies only to claims or derived expectations using Angrylion-Plus: local, out-of-tree only; no code copy; derived data stays local. It does not block authored/native work. Audit parallel-RDP fixture licensing before use. |
| **AUD-01–03** | AI consumes known authored PCM words from RDRAM across DMA boundaries; exact samples, sample index/rate, queue status and interrupt boundary are observable. Replaying the same guest-time path under different host throttling yields identical samples. | `[REQ] [PRIMARY]` Nintendo's RCP header documents double-buffered AI address/length and interrupt on `ai_full` 1→0; DACRATE is tied to video clock. Current roadmap phrase “IRQ at DMA start” must be tested at that precise queue transition, not treated as one interrupt per submit/start. Keep RSP audio-microcode coverage outside this transport criterion. |
| **RUN-06** | The headless runner writes raw VI frames and raw AI sample buffers with profile/build identity, guest/frame timing, sample index and rate metadata; output capture does not invoke a host renderer/audio device. | `[REQ] [OWNER]` Core produces buffers; runner owns files. This does not promote the installed consumer or C-ABI requirements from Phase 10. |
| **SAVE-01** | Timed logical controller input crosses SI/PIF; an authored guest diagnostic reports the expected controller query response at the expected guest boundaries. | `[REQ] [PRIMARY]` The Nintendo manual documents the standard controller query through SI/PIF. Physical mapping remains a host concern. |
| **SAVE-02–04** | Profile selects EEPROM 4/16 Kbit or SRAM explicitly. Test EEPROM block/range rules, pending/busy behavior, byte import/export round-trip and unknown-device refusal. Refused/invalid imports preserve the pre-operation digest. | `[REQ] [PRIMARY]` Manual identifies 4/16 Kbit EEPROM, 8-byte operations and range errors. Its SDK helper's 15 ms wait is not proof of an exact physical busy interval. Do not infer a device from a title. |
| **CORE-02** | CPU, RSP, RDP, VI, AI, PI, SI and MI events share the existing guest-time total order. Tests cover equal-tick order plus split/whole advances and interleaved instances; the CPU-to-RSP quantum stays in profile identity. | `[REQ] [OWNER]` Complete only after Phase 7/8 devices and SI are integrated. No host clock or presentation event enters machine time. |
| **CORE-04** | Export a versioned snapshot at a nontrivial boundary (including an in-flight/queued device operation); import it and reproduce the canonical digest and subsequent output. Reject unsupported versions transactionally. | `[REQ] [INFER]` Serialize every future-affecting machine field, including scheduler/device queues; exclude host file handles and policies. |
| **CORE-05** | Record/replay timed logical input bound to profile, build and content hash. Replay produces the same named checkpoints/final digest as the original run. | `[REQ] [OWNER]` Artifact is for reproducible input, not physical controller mapping or a player-facing movie editor. |

## Smallest acceptance path and ordering

1. **Phase 7 first:** one authored fill display list → RDP RDRAM writes → one VI scanout → runner raw-frame output, exact independently calculated pixels, wrong-value negative control. Then add remaining required primitives/state, SYNC_FULL/DP completion, filter/interlace cases, labeled differential reporting and named undefined-behavior policies.
2. **Phase 8:** known authored sample memory → AI DMA/queue/cadence → raw audio output. Confirm the full-to-not-full interrupt edge against primary references before calling its timing hardware-accurate. Add RUN-06 file capture only after both raw output paths exist.
3. **Phase 9:** timed standard-controller query; explicit save-device selection and refusal; media import/export; whole-machine state round-trip; timed-input replay. Close CORE-02 last with device-order, equal-tick, partition and instance checks over all in-scope devices.

Dependencies: `Phase 4 CPU command lists → RDP → RDRAM → VI → raw frame`; existing RSP baseline plus authored sample memory → AI → raw samples; both raw paths → RUN-06. SI/PIF controller input precedes controller replay. Save-media protocol precedes save import/export. All Phase 7/8/SI device state must be serializable before CORE-04/05 acceptance.

## Differentiator

`[INFER]` The milestone's product value is a single cheap consumer workflow that returns exact, inspectable raw video/audio and replayable guest-time outcomes under one named profile. Its differentiator is the bound evidence and deterministic contract, not output hashes alone or a broad game-compatibility promise.

## Deferred and out of scope

| Feature | Why defer |
|---|---|
| API-01–04, installed independent consumer, C-ABI freeze | These remain Phase 10. RUN-06 is a runner capture path, not a new public ABI. |
| SDL/native host, physical controller mapping, playable title/toolchain, LibFuturo adapter | These remain Phase 11; core output and timed logical input are prerequisites, not permission to pull the host forward. |
| RSP audio microcode compatibility | v1.1 proves AI transport from authored sample memory; it does not claim arbitrary audio tasks. |
| Angrylion-derived public expectations/claims | D5 remains a specific stop for these outputs. Authored source/native implementation and expectations can proceed. |
| PAL/MPAL, Expansion Pak, other save types/accessories, 64DD, commercial-game support, full hardware timing, GPU RDP and player features | Later profile or product work, not required for this 17-ID NTSC cartridge slice. No broader support claim follows from one suite or matching emulator output. |

## Sources

- `[OWNER/REQ]` [Current roadmap](../ROADMAP.md), [archived v1.0 requirements](../milestones/v1.0-REQUIREMENTS.md), and [Emulator DNA](../bootstrap/EMULATOR-DNA.md).
- `[PRIMARY]` Nintendo Programming Manual, [Chapter 12: RDP Programming](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro12/index.htm) and [§12.2: RDP synchronization](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro12/12-02.htm).
- `[PRIMARY]` Nintendo-authored [RCP register header](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm) (AI double buffering, DACRATE and interrupt transition); [Chapter 8 I/O functions](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro08/08-08.html) (AI/VI buffer operations).
- `[PRIMARY]` Nintendo Programming Manual [§26.2 Standard Controller](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro26/26-02.htm), [§26.6 EEPROM](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro26/26-06.htm), and [§30.1 VI](https://jrra.zone/n64/doc/pro-man/pro30/30-01.htm). These archived mirrors are source references, not a license to redistribute documents or protected content.
