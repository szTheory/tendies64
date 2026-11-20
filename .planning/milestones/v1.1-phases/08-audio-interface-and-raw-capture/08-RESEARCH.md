<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

[VERIFIED: .planning/phases/08-audio-interface-and-raw-capture/08-CONTEXT.md:18-25]
### Real consumer and timing
- **D-01:** Lead with a build-only rights-cleared authored diagnostic making
  fetched CPU writes to ordinary AI registers. Transport exact literal signed
  stereo samples across a DMA boundary into the actual raw observer and real
  runner. Derive expected samples independently; a same-setup wrong-sample
  control reaches its comparator. Run cheap accepted setup/identity/rights and
  representative consumer checks before expensive suites. Core-only output,
  non-silence, hashes alone or a fixed observation record do not prove RUN-06.
- **D-02:** Sample progress comes only from canonical guest time. Use exact
  bounded rational cadence in the existing62.5MHz timeline; NTSC audio clock
  is48,681,812Hz with divisor DACRATE+1. Batching may materialize output but
  cannot hide partial-DMA semantic sample boundaries or defer all RAM reads
  until completion. Resolve primary register masks, enable/reset, source reads,
  latching and same-tick order in phase research; explicitly name deterministic
  software-profile rules where hardware evidence does not resolve a choice.
- **D-03:** Two-entry DMA queue and ai_full1→0 MI AI interrupt are locked
  primary-derived requirements, not an IRQ per submission/start. Verify the
  one-buffer and two-to-one queue cases. The documented boundary failure
  ((address+length)&0x3fff)==0x2000 is a specific condition; do not generalize
  it into an8KiB carry model. Unresolved hardware status effects stay unknown
  or qualified software policy. No emulator implementation or physical oracle.

[VERIFIED: .planning/phases/08-audio-interface-and-raw-capture/08-CONTEXT.md:40-53]
### Ownership and observation
- **D-04:** Use a separate bounded lossless audio packet queue and info/copy
  seam analogous to VI. Decide complete packet granularity before coding.
  Queue-full leaves the same producing event pending: no sample phase,
  source/DMA cursor or AI/MI effect commits until capacity permits resume.
  No drop/overwrite/reorder; host drain advances no guest time. Short/invalid
  destinations refuse atomically with required-size reporting as applicable.
  Verify partial DMA, source-mutation witness for selected read policy,
  split/whole advance, drain cadence, interleaved machines and same-tick ties.
- **D-05:** Keep all future-affecting AI state in the machine and canonical
  state/digest; caller memory, file state and host observer cursors stay out.
  Apply the approved compatibility policy through a changed composite profile
  and descriptor/state versions where required. Preserve all old identities,
  bytes and refused AI behavior. No profile-per-device or public ABI promise.

[VERIFIED: .planning/phases/08-audio-interface-and-raw-capture/08-CONTEXT.md:55-79]
### Actual capture and evidence
- **D-06:** The runner owns raw files: numbered actual RGBA frames and explicit
  signed PCM bytes plus a JSON manifest. Specify byte order/channels/packing,
  geometry/stride, sample index/rational rate/tick bounds, sequence and exact
  profile/build/content/input identities. Capture both real observer paths,
  not fixed-size test observations or reconstructed expected bytes. Use raw
  packet bundles without codec/container dependencies; PAM/WAVE can wait for
  an actual interoperability need. A/V metadata relates separate streams.
- **D-07:** Finalize a requested capture all-or-nothing. Existing/unsafe targets
  stay untouched; incomplete files cannot appear as a successful final bundle.
  Define staging/finalization and bounded disk/output/error effects first.
  Missing/zero/truncated data, output-full/resume and I/O/capacity failures
  are explicit, never success over a partial bundle. Output errors remain
  host-side and do not invent guest progress or mutate canonical core policy.
- **D-08:** Keep C17/CMake/CTest and current dependencies/jobs/limits. No new
  codec, serializer, cache, package install or production thread. Preserve
  owning LastTest before CTest/discovery and after success/failure; manual
  batches get unique absolute JUnit paths. Qualify cohesive source changes,
  review before final combined suites, and reuse unchanged exact evidence.
- **D-09:** Existing two hosted jobs, ten-minute limits and CTestjobs4 remain.
  Generated ROMs and raw capture files remain local/build-only. Publish only
  a bounded cleared projection after rights/privacy checks, retaining original
  command/outcome/producer/sample evidence privately. Authored software tests
  are not silicon, general compatibility, oracle permission or VID-04 evidence.
  D2/D4/D5/D6/D7, private windows and physical/paid/destructive stops persist.

[VERIFIED: .planning/phases/08-audio-interface-and-raw-capture/08-CONTEXT.md:81-87]
### Agent discretion

Choose concrete internal representations, queue sizing, packet cap, provisional
schemas and profile version numbers under these contracts and primary evidence.
Research unresolved fields once; begin the smallest verifying implementation
promptly. Reopen a choice only for a concrete contradiction or failing witness.

### Deferred Ideas (OUT OF SCOPE)

[VERIFIED: .planning/phases/08-audio-interface-and-raw-capture/08-CONTEXT.md:114-119]
Merged A/V envelope, PAM/WAVE/codec formats, host playback/resampling, DSP/RSP
microcode breadth, Phase9 replay/state export, installed ABI consumer, public
raw capture distribution, physical testing and release remain outside Phase8.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| AUD-01 | “AI DMA, the two-deep queue, DACRATE cadence and interrupt behavior produce raw audio buffers with sample index and rate metadata” [VERIFIED: .planning/REQUIREMENTS.md:24] | Register, cadence, queue and MI behavior below; packet info/copy API and output tests. |
| AUD-02 | “An authored audio diagnostic checks exact sample values across DMA boundaries; ‘non-silent’ is never accepted as evidence” [VERIFIED: .planning/REQUIREMENTS.md:25] | Fetched CPU tracer, literal authored samples, independent expected values and wrong-sample comparator control. |
| AUD-03 | “Audio cadence is derived from guest time and never from a host clock” [VERIFIED: .planning/REQUIREMENTS.md:26] | NTSC rational sample schedule, partition/interleave tests, and active-write policy. |
| RUN-06 | “User can capture raw video frames and raw audio sample buffers from a headless run, as files with metadata, separately from host presentation” [VERIFIED: .planning/REQUIREMENTS.md:27] | Actual runner consumers, raw bundle schema, and no-clobber transaction/failure tests. |
</phase_requirements>

# Phase 08: Audio Interface and Raw Capture — Research

**Researched:** 2026-11-18  
**Domain:** Deterministic N64 AI timing, raw A/V observers, host-side capture  
**Confidence:** HIGH for documented register fields and existing seams; MEDIUM for selected software-profile policies

## Summary

Implement AI only in a new composite profile based on the qualified authored Phase 07 RDP/VI profile. Keep DMA, queue, sample phase, MI source, partial packet bytes, and every other future-affecting value per machine and in canonical digest/state. Preserve the old profile identities and its current AI refusals. The core produces bounded raw packets; the real headless runner drains them into a staged raw-file bundle. No codec, host audio clock, playback, or core file access is needed.

The Nintendo-authored RCP register header defines address/length double buffering, address-before-length, 8-byte alignment, register masks, status bits, DACRATE/BITRATE fields, NTSC video clock and the ai_full falling-edge interrupt. The SDK function manual adds the remaining-length API behavior, buffer-size guidance, and one narrowly specified DMA boundary failure. The manual does not settle sample-source latching, disabled-DMA behavior, live DACRATE/BITRATE writes, exact sample-vs-register same-tick ordering, or exported PCM byte convention. Give these corners explicit reversible software-profile rules and label them as such.

**Primary recommendation:** Begin with one fixed CPU-fetched diagnostic and its wrong-sample control. Add the narrow AI profile/register/scheduler/packet path, then the real runner consumer and transactional bundle. Do not expand the fixture set until the CPU-to-file witness passes.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| AI registers, DMA descriptors, cadence, MI AI source | Canonical machine core | — | Future-affecting device behavior must be deterministic and per-instance. |
| Raw audio/video info and copy-out | Canonical machine observer API | Runner | Core owns immutable packet bytes; callers own only copied output. |
| Files and manifest finalization | Headless runner | Host filesystem | File errors and paths must not change guest time or canonical machine policy. |
| Exact authored fixture and CPU trace | Build-only fixture + real runner | Core trace observer | Establish that fetched guest stores reach real packet bytes and metadata. |

## Project Constraints (from AGENTS.md)

[VERIFIED: AGENTS.md:24-42,53-58,68-73]

- Keep one deterministic machine separate from host clocks, files, presentation, and durable storage; separate guest progress/time and raw A/V.
- Make device behavior observable at meaningful boundaries and qualify support claims by exact profile, build, rights-cleared content, and evidence tier.
- Prefer few dependencies; preserve existing rights, authority, physical-test, publication, and destructive-action stops.
- An implementation or planned test is not evidence that it passed. Do not treat another emulator's behavior, rights, license, or API as Tendies64 evidence.

## Standard Stack

| Component | Use |
|-----------|-----|
| Existing C17 core and CMake/CTest workflow | Machine/device implementation and validation. [VERIFIED: 08-CONTEXT.md:69-74] |
| Existing per-machine scheduler, MI, profile, digest, VI packet API | Reuse local patterns; add AI state and a separate bounded audio info/copy seam. [VERIFIED: 08-CONTEXT.md:89-103] |
| Existing real runner and fixture builder | Add raw packet consumers and a local/build-only diagnostic; keep file handling outside the core. [VERIFIED: 08-CONTEXT.md:92-103] |
| New external dependencies | None. Use plain raw files and JSON as locked by D-06/D-08. |

The configured gate preset uses CTest with four jobs. Preserve the two existing hosted jobs and ten-minute limit; research did not run builds or tests. [VERIFIED: CMakePresets.json:28-44; 08-CONTEXT.md:69-79]

## Hardware Evidence and Field Table

The RCP facts below are from the Nintendo-authored RCP register header (archival mirror); SDK call guidance is from Nintendo's N64 SDK function manual. These sources describe documented hardware/API behavior, not emulator implementation.

| Register / field | Documented fact | Implementation boundary |
|---|---|---|
| AI base and address | Base is 0x04500000; AI_DRAM_ADDR is write-only, address bits [23:0], 8-byte aligned. Header says address must be written before length. [CITED: [RCP header, AI section](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm)] | Use bus offset decode. Preserve an address-staged bit/value; reject/refuse invalid profile operations before mutation. Do not invent readback for the write-only address register. |
| AI_LEN | Header: v1.0 [14:0], v2.0 [17:0], low three bits ignored; field is R/W. [CITED: [RCP header, AI section](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm)] | Phase profile is final-release-sized: mask to 18 bits and apply 8-byte length granularity. SDK osAiGetLength reports bytes remaining in the active DMA. Define exact readback for idle/queued states as profile policy. |
| AI_CONTROL | Write-only bit 0 enables DMA. [CITED: [RCP header, AI section](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm)] | Profile policy: accept only documented bit 0; unsupported bits refuse atomically. Define deterministic disable/re-enable handling and test it; do not claim unsourced reset/abort effects. |
| AI_STATUS | Read: full is bit 31 (legacy header also lists bit 0), busy is bit 30. Write clears audio interrupt. A full 1→0 transition sets interrupt. [CITED: [RCP header, AI section](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm)] | Use final-release full bit 31. Connect MI AI source bit 0x04. Raise on the specified full falling edge, not on each submission or start. Status-write acknowledgement must clear only the MI AI source. |
| AI_DACRATE | Writable [13:0]; sample rate is video clock divided by (dperiod+1). [CITED: [RCP header, AI section](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm)] | NTSC profile uses the cited 48,681,812 Hz clock. Keep exact rational phase; no host clock or floating point. |
| AI_BITRATE | Writable [3:0]; DAC clock is video clock divided by 2*(aclockhp+1); source notes abus clock stops when aclockhp is zero. Header constrains (dperiod+1) ≥ 66*(aclockhp+1). [CITED: [RCP header, AI section](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm)] | Preserve register value and apply a named deterministic policy for zero/incompatible values and writes during an active DMA. Do not silently normalize values. |
| SDK transfer constraints | SDK manual says address 64-bit aligned, byte count multiple of 8, maximum call size 262,144 bytes, and status full rejects/stops a new setup. It describes a v1.0 15-bit length and final 18-bit length. [CITED: [osAiSetNextBuffer](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/n64man/os/osAiSetNextBuffer.htm)] | **Source tension:** 18-bit AI_LEN with low 3 bits ignored yields a largest aligned register value of 0x3fff8 (262,136 bytes), while the SDK API says 262,144 bytes. Use the register's 18-bit effective mask for MMIO and document the API prose as a separate SDK transfer ceiling; do not expand the MMIO mask to 19 bits. |
| Known DMA boundary failure | SDK documents one failure when ((address+length)&0x3fff)==0x2000 and says its SDK function patches it. [CITED: [osAiSetNextBuffer, Known Issue](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/n64man/os/osAiSetNextBuffer.htm)] | Do not generalize to all 8 KiB crossings or claim the software workaround is hardware behavior. Preserve as one explicit authored boundary witness/caveat. |

The SDK manual says the FIFO-full status is set when both DMA registers have been programmed and busy indicates active transfer. It also describes rejecting a next-buffer setup while full. [CITED: [osAiSetNextBuffer](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/n64man/os/osAiSetNextBuffer.htm)] The RCP header calls address/length double-buffered. Model a two-entry descriptor queue and test one-entry completion separately from two-to-one promotion. Exact register write side effects beyond those statements remain profile policy.

## Architecture Patterns

### Deterministic machine path

1. Decode AI register accesses only for the new composite profile. Keep the old warm-device zero/refusal behavior intact. The current bus maps the AI window through the warm-device path; the warm profile reads AI status as zero and accepts only zero control/status writes. [VERIFIED: core/t64_bus.c:18,133-139; core/t64_warm_devices.c:98-101,156-167]
2. Store staged address, active and queued descriptors, byte/sample cursor, rational phase, latched settings, packet bytes, queue indices/count, MI pending source, and sequence/overflow state in the machine. Include future-affecting state and packet content in canonical digest/state; exclude caller destinations and observer drain cursor. Existing digest explicitly excludes VI packet head/count as observer state while hashing producer packet data. [VERIFIED: core/t64_digest.c:677-682]
3. Schedule each semantic sample boundary against the existing guest tick domain. The project profile uses a 62.5 MHz machine tick and NTSC 48,681,812 Hz clock as locked by D-02. Maintain exact remainder across partial advances; preserve scheduler tick/sequence tie ordering.
4. Recommended cadence calculation: for elapsed tick span T and phase remainder r, sample count is floor((T*48,681,812+r)/(62,500,000*(DACRATE+1))). Use checked integer arithmetic or quotient/remainder decomposition so neither multiplication nor addition wraps. Schedule the next exact boundary with ceil division. This formula is derived from the two clocks and is not an extra hardware claim.
5. Recommended reversible source policy: fetch each four-byte interleaved stereo frame from current RDRAM at its sample event. This makes writes to not-yet-consumed RAM visible and preserves partial-DMA behavior. Do not snapshot all source bytes at submission or defer every RAM read until completion. Label source timing as selected software-profile behavior.
6. Recommended latching policy: snapshot DACRATE and BITRATE when a DMA descriptor becomes active; writes during it affect the next descriptor. Keep active and pending descriptor state explicit. This is deterministic policy, not sourced hardware behavior.
7. Recommended packet unit: one packet for each completed DMA descriptor, with descriptor sequence, sample start/count, rational rate, guest tick bounds, and exact raw bytes. Keep at most two completed/reserved packets; reserve the output slot before the descriptor's first sample. If no output slot is available, scheduler admission holds the same descriptor-start/promotion/sample-producing event before it mutates phase, source/DMA cursor, packet bytes, or MI. Draining frees capacity without advancing guest time.
8. Keep packet copy-out all-or-nothing. A short/invalid destination reports required length where possible and leaves both destination and queue unchanged, matching the established video output contract. The scheduler already has producer-event admission before dispatch; extend that pattern to AI rather than dropping output.

### Profile and compatibility

The current newest authored profile is ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9, with descriptor/model/state versions 10/18/23. [VERIFIED: core/t64_machine.c:905-971; verbatim values at lines 909-910, 964-966.] Add a distinct composite AI profile derived from that profile, with a named AI policy and updated descriptor, machine, and state versions. Recommendation for the next profile: increment those versions to 11/19/24 and use a clear -ai-v1 profile suffix. These new numbers are a planning recommendation; confirm the active source head when implementing. Preserve all old profile hashes, state bytes, and refusal behavior.

### Actual raw capture path

Extend runner/t64_real.c to drain general video and audio packet info/copy APIs after each actual bounded machine advance. Its current drain_video is fixture-specific and validates a fixed 4x3 observation; that observation cannot satisfy RUN-06. [VERIFIED: runner/t64_real.c:380-397,855; .planning/phases/08-audio-interface-and-raw-capture/08-CONTEXT.md:99-103]

Recommended no-dependency bundle:

- One requested bundle directory containing manifest.json, numbered video/frame-000000.rgba8 files, and numbered audio/packet-000000.s16be files.
- Manifest names schema version, exact profile/build/content/input identities, video frame sequence/ticks/geometry/stride/encoding, and audio packet sequence/first sample index/frame count/rate numerator and denominator/tick bounds/byte length.
- Export audio bytes as signed 16-bit, big-endian, interleaved stereo by explicit software file convention. Primary register docs do not establish this format; raw machine packet remains exact fetched bytes.
- Capture the two streams independently; metadata relates their guest ticks but makes no invented cross-stream ordering guarantee.
- Create a sibling staging directory using exclusive safe creation; cap total output bytes; write and close all packet files and manifest; then finalize with atomic no-replace behavior. If the target exists, a path is unsafe, the cap is exceeded, or any write/close/finalize step fails, leave the requested target untouched and report failure with no successful final manifest. If the host cannot provide safe no-clobber finalization, refuse capture on that platform. Do not put path, file, or I/O state in the core.
- Do not reuse the supervisor's stdout/trace capture or fixed video-observation record as the raw A/V artifact.

### Tracer-first task decomposition

1. **Witness first:** build-only rights-cleared fixture performs fetched CPU SWs to AI address/length/control/rate registers. Retain CPU trace PC/opcode/address/value, fixture/build/profile identities, exact observer bytes, and packet metadata. The actual runner compares independently authored literal signed stereo sample values across a DMA boundary. Add a same-setup wrong-sample control and require that it reaches and fails the sample comparator.
2. **Core path:** add named AI policy/profile and state; register masks and refusal postconditions; two-entry descriptor queue; MI AI status/ack and full 1→0 behavior; exact rational sample scheduler; selected source and latching policies; output packet info/copy and canonical digest/state.
3. **Behavior witnesses:** register and queue matrix, exact sample/rate/tick witnesses, partial DMA, in-flight RAM mutation, disabled policy, active rate writes, sample tie ordering, and split/whole/interleaved-machine controls.
4. **Real capture:** add the no-dependency packet bundle writer/reader checks and staging/no-clobber failure behavior. Extend actual runner to drain both real observer queues at each yield.
5. **Integration:** run the tracer and representative capture controls before the expensive CTest inventory and combined suites; preserve owning LastTest/JUnit/log inventory and the existing CI job count, time ceiling, and CTest parallelism.

## Don't Hand-Roll

| Problem | Do not build | Use |
|---|---|---|
| Guest cadence | Host timer, float accumulator, or wall-clock callback | Integer rational phase tied to canonical scheduler ticks |
| Bounded output | Drop-oldest ring or an unbounded vector | Fixed per-instance queue and scheduler admission/backpressure |
| Core file serialization | Audio codec, WAV/PAM writer, or filesystem dependency in core | Exact packet API; simple runner-owned raw files and JSON manifest |
| DMA bug handling | Broad address wrap/carry workaround | The single documented boundary condition as a named test/caveat |
| Diagnostic evidence | Non-silence or hash-only observation | Fetched CPU trace plus exact independent sample comparator and wrong-sample control |

## Common Pitfalls

- **IRQ on every buffer:** AI interrupt is tied to the full 1→0 transition. Test one-buffer and two-to-one cases; do not raise on every start.
- **Lost partial-DMA semantics:** batching must preserve each sample boundary and selected RAM-read rule. Include a source mutation between sample events.
- **Backpressure after mutation:** preflight output capacity before the scheduler pops the producing event; queue-full must not advance phase, cursor, packet data, or MI state.
- **Observer state in digest:** host drain/copy destinations do not belong to canonical state; packet content and future-affecting producer state do.
- **Wrong evidence consumer:** fixed 4x3 video observation does not prove raw RUN-06 capture. Drain actual packets from both APIs in the real runner.
- **Unsafe success on I/O error:** never publish a successful manifest over partial output; preserve existing targets byte-for-byte and test every failure edge.
- **Overreading the SDK workaround:** the documented boundary expression is one condition, not a general 8 KiB crossing rule.
- **Profile leakage:** do not change old profile identities, bytes or warm-profile AI refusals to add new semantics.

## Validation Architecture

### Test framework

| Property | Value |
|---|---|
| Framework | Existing C unit tests and CTest |
| Config | CMake presets; four CTest jobs [VERIFIED: CMakePresets.json:28-44] |
| Commands | Add focused AI core and raw-capture runner CTest groups; run focused group first, then the existing exact inventory and combined suites. Preserve existing two hosted jobs and 10-minute/job limit. No research-phase build or test was run. |

### Requirement-to-test map

| Requirement | Required witness | Test level |
|---|---|---|
| AUD-01 | MMIO masks/read/write/refusals; final length field; one/two queue; partial length; full→not-full MI AI transition/ack; sample index and rate metadata | Core unit + scheduler integration |
| AUD-02 | Actual CPU fetched writes, exact authored samples across DMA boundary, wrong-sample comparator control | Build-only fixture + real runner |
| AUD-03 | NTSC non-divisor rates; exact before/on/after-boundary samples; whole vs partitioned advance; active DACRATE/BITRATE write rule; separate interleaved machine phases | Core/scheduler unit |
| RUN-06 | Real video and audio packet copies to files; manifest agreement; complete bytes; transactional target and I/O failure controls | Runner integration |

### Required edge matrix

- Two buffers enqueue: full set. Complete/promote second: full clears and one AI MI pending transition occurs; one-buffer-only completion does not fabricate the full falling edge.
- Read and write every AI register boundary; address-before-length, field masks, alignment, RDRAM span, zero/partial length, queue-full and disabled behavior. Assert every refusal leaves state/digest unchanged.
- Exact sample cadence on both sides of a rational boundary; split/whole guest advances, equal-tick CPU store/sample ordering, overflow preflight, reset, and distinct interleaved machines.
- Mutate source RAM between sample events and assert only later consumed samples change under the chosen profile rule.
- Fill completed/reserved packet capacity; assert the same producer event remains pending and phase/cursor/bytes/MI stay fixed. Drain then resume to the same bound and verify exactly-once output. Short and invalid copies must preserve caller bytes and queue.
- Manifest parses without extra dependencies; every raw file length/metadata matches observer packets. Existing file/directory, symlink/unsafe parent, byte-cap, zero/missing/truncated output, write/close/finalize failure leave no successful bundle and do not modify a preexisting target.

## Security Domain

Security enforcement is enabled at ASVS level 1. [VERIFIED: .planning/config.json:20-21]

| Area | Applies | Control |
|---|---|---|
| Input validation / ASVS V5 | Yes | Checked register masks, aligned RDRAM spans, bounded lengths, checked cadence arithmetic, bounded output bytes, manifest escaping, and refusal before mutation. |
| Filesystem boundary | Yes | Runner-only file access; safe path validation, exclusive staging, no-follow handling, no-replace finalization, and untouched existing targets. |
| Authentication/session / V2–V3 | No | No network/API sessions in this phase. |
| Access control / V4 | Limited | Capture writes only the caller-requested safe bundle target. |
| Cryptography / V6 | No new scheme | Keep existing content/build/profile identities; add no custom cryptography. |
| Rights/privacy | Yes | Diagnostic and raw captures remain authored, rights-cleared, local/build-only; publication requires the existing review. |

## Assumptions Log

| # | Claim | Section | Risk if wrong |
|---|-------|---------|---------------|
| A1 | AI reads four bytes per sample as interleaved stereo; exported file labels those bytes signed 16-bit big-endian. This is an explicit software profile/output convention, not a hardware-doc claim. | Architecture / raw capture | Raw sample interpretation or consumer tooling could disagree; authored literal witness and schema version make changes explicit. |
| A2 | DACRATE/BITRATE are latched at descriptor activation and source bytes are fetched at each sample event. These are chosen reversible software-profile rules; primary sources do not settle them. | Architecture | Changes sample output under active writes or RAM mutation. |
| A3 | New profile version recommendation 11/19/24 is the next monotonic set after the currently read 10/18/23 profile. | Profile | Recheck actual live profile head before implementation and select non-colliding versions. |

## Open Questions

1. **AI length maximum wording:** the hardware field plus ignored low bits imply 262,136 bytes maximum aligned value; the SDK manual says a maximum transfer size of 262,144 bytes. Preserve the 18-bit MMIO mask and keep both statements distinct. No blocker to planning.
2. **Source reads, setting latches, control disable effects, idle/queued AI_LEN reads, and same-tick order:** no primary answer found in the cited sources. Use the explicit reversible policy above and exact authored witnesses; reopen only for a concrete primary contradiction or a failing witness.

## Sources

### Primary Nintendo-authored documentation

- [RCP register header (revision 1.21, 1998-07-31; archival mirror)](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm) — AI register offsets/field masks, NTSC clock, queue/status/interrupt and divisor constraints.
- [N64 SDK osAiSetNextBuffer manual](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/n64man/os/osAiSetNextBuffer.htm) — SDK alignment/size guidance, remaining-byte API, FIFO full behavior, and specific documented boundary failure.

### In-repository sources read this session

- .planning/phases/08-audio-interface-and-raw-capture/08-CONTEXT.md, .planning/REQUIREMENTS.md, .planning/AUTONOMY.md, .planning/STATE.md, .planning/research/SUMMARY.md, AGENTS.md, and .planning/config.json.
- core/t64_bus.c, core/t64_warm_devices.c, core/t64_mi.c, core/t64_scheduler.c, core/t64_vi.c, core/t64_digest.c, core/t64_machine.c, core/t64_internal.h, core/include/tendies64/t64_machine.h, and runner/t64_real.c.

## Metadata

- **Confidence:** HIGH for documented fields and code seams; MEDIUM for explicit reversible software-profile policies.
- **Security:** included because security enforcement is enabled.
- **Nyquist validation:** included because workflow.nyquist_validation is true.
- **Research date:** 2026-11-18.
- **Validation performed:** none; research only.
