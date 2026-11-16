# Domain Pitfalls

**Domain:** Deterministic Nintendo 64 machine core — v1.1 Observable Machine  
**Researched:** 2026-11-15  
**Overall confidence:** MEDIUM

## Evidence and Decision Status

- **Owner/project contracts (HIGH):** one canonical deterministic machine; host clocks, files, physical-input mapping, presentation and durable storage stay outside it. Preserve profile identity and refused-operation postconditions. D2/D5/D6 remain owner stops. See [PROJECT.md](../PROJECT.md), [ROADMAP.md](../ROADMAP.md) and [AUTONOMY.md](../AUTONOMY.md).
- **Hardware boundary facts (MEDIUM):** the Nintendo manuals describe RDP input, RDRAM framebuffer, VI scanout, CPU/RSP audio synthesis and AI PCM transport as distinct responsibilities. Sources are archival mirrors of Nintendo manuals, not new hardware measurements.
- **Recommendations below (INFERENCE):** these are ways to make the milestone's authored evidence isolate faults. They do not assert that current Tendies64 behavior passes or that any behavior is hardware-accurate.

## Recommendation

Keep the C17/no-new-dependency default and use a short authored evidence chain: independently expected RDP fill pixels through RDRAM and VI first; authored PCM through AI cadence next; then timed SI/PIF input, transactional save import, and exact state/replay round trips. Keep every device on the canonical guest scheduler, keep the host output cursor outside the machine digest, and preserve existing profile semantics. Add broader graphics cases only after the simple pixel path has an independently wrong-pixel control.

**Reverse this order or dependency default only if** a phase-level source audit shows the authored chain cannot isolate a required behavior, or a measured implementation need demonstrates a maintained dependency is safer and cheaper than a small provenance-recorded helper. D5 closure could permit separately labeled differential evidence; it would not make another emulator a hardware oracle.

## Critical Pitfalls

### 1. A renderer regression baseline that repeats the implementation's mistake

**What goes wrong:** A screenshot captured from the new renderer, a second implementation sharing its equations, or a broad triangle scene becomes the expected answer. The same defect then appears on both sides or its first failing stage is hidden.

**Why:** The RDP writes framebuffer state in RDRAM and VI reads display data from RDRAM; a final raw frame can fail because of either side of that chain. Nintendo's Chapter 8 manual separates these boundaries, but does not supply Tendies64 golden pixels. [Nintendo Programming Manual, Chapter 8 (archival mirror)](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro08/08-01.html)

**Prevention:** Begin with an authored fill case whose expected pixel values are calculated independently of production raster code. Verify the RDRAM result and VI raw frame separately. Include a deliberately wrong expected pixel or controlled output mutation that must be rejected before adding broad triangles. Keep command completion/interrupt assertions separate from pixel assertions.

**Detection:** A golden changes whenever renderer code changes; the wrong-pixel control passes; or a failure reports only a final image hash with no first differing coordinate or boundary.

### 2. Guest time or canonical state depends on host output consumption

**What goes wrong:** Polling video/audio more often, draining a ring at a different rate, or advancing in different chunks changes device order, guest time, digest, or emitted bytes. A full output buffer silently drops samples/frames or lets the machine advance inconsistently.

**Why:** VI, AI and SI are guest-timed devices, while capture-file writes, host playback and physical-input polling are host operations. Mixing the host output cursor with guest scheduler state makes determinism depend on when a frontend asks for data.

**Prevention:** Use one guest-time scheduler and an explicit tie-break order for coincident CPU, RSP, RDP, VI, AI, SI and MI events. Keep consumption position/capture destination outside the canonical digest. Specify bounded output/backpressure semantics before implementation: no silent loss or duplication, and any refused/bounded advance follows the project's exact state-preservation contract. Test different advance partitions and drain cadence against the same logical inputs and guest-time target; compare canonical digests and the raw byte stream.

**Detection:** Output cadence changes with host pacing; a second drain yields different data; an advance that reports refusal still changes any machine latch; or event order depends on subsystem loop order.

### 3. “Undefined” pixels or noise become accidental host randomness

**What goes wrong:** Uninitialized memory, implicit noise, or unspecified color is filled from allocator contents, wall time, host entropy, or compiler-dependent behavior. Raw captures and replay then differ across runs, and an emulator choice is mislabeled as hardware fact.

**Why:** The v1.1 plan explicitly requires undefined-behavior choices to be named. A stable output is not proof that the chosen value matches silicon.

**Prevention:** Give each undefined-data behavior a named, versioned policy and deterministic seed/source; bind any policy that changes machine-visible results to profile identity. Avoid leaving C objects or serialized bytes indeterminate. Mark the result as an emulator policy, not a hardware-derived fact. Repeat under different host pacing and allocation patterns.

**Detection:** A raw frame or digest changes with process layout, run order, host, or output-drain timing, or a report calls an implementation-defined color/noise choice “accurate” without independent evidence.

### 4. New devices silently change an older profile or its state identity

**What goes wrong:** Adding RDP/VI, AI or SI changes scheduler ordering, digest contents, default device selection, or refusal behavior for an already named profile. Old evidence remains labeled with the old profile name despite changed semantics.

**Why:** Profile identity is the boundary for reproducible claims; v1.0 explicitly retained older profile identities and refusal behavior. The v1.1 scheduler requirement adds cross-device ordering, which is a likely integration seam.

**Prevention:** Keep newly supported behavior behind explicit profile capabilities and version state/profile formats when semantics change. Retain one pre-existing authored success and refusal control per affected profile. Test coincident-event ordering and partition/interleave equivalence for the new scheduler without rewriting old profile meaning.

**Detection:** A pre-existing profile's output or digest changes without a version/capability change, or an unknown save device becomes accepted by default.

### 5. AI transport tests are described as real audio-microcode support

**What goes wrong:** Authored PCM pushed through AI is taken as evidence that commercial audio tasks or real RSP audio microcode work.

**Why:** Nintendo's introductory manual says waveform synthesis is cooperative CPU/RSP work and AI reads waveform data from an RDRAM buffer as 16-bit stereo samples at a constant interval. AI transport therefore exercises a later boundary than sample synthesis, not the synthesis itself. [Nintendo N64 Introductory Manual, audio architecture (archival mirror)](https://ultra64.ca/files/documentation/online-manuals/man/kantan/step2/3-1.html), [audio flow (archival mirror)](https://ultra64.ca/files/documentation/online-manuals/man/kantan/step2/3-3.html)

**Prevention:** Label authored PCM evidence as AI DMA/cadence only. Keep sample expectations independent, and cover the stated two-deep DMA queue, DMA-start interrupt, boundary carry and DACRATE/guest-time cadence separately. Explicitly leave real audio-microcode compatibility unclaimed unless that microcode path is exercised with separately authorized evidence.

**Detection:** Reports call the authored-PCM path “sound compatibility,” or output depends on host wall-clock pacing rather than guest time.

### 6. Save import partially mutates state, or replay omits an identity input

**What goes wrong:** A truncated, oversized, wrong-version, wrong-endian or identity-mismatched save changes some device bytes before rejection. A replay runs against a different profile, build or content and still appears to match because it checks only a final screenshot/output hash.

**Why:** Save formats and archives are untrusted inputs; the project requires malformed/refused operations to leave state unchanged. A replay is meaningful only when it binds the machine and content identity plus timed logical inputs. Raw host structure dumps also risk making stored bytes depend on compiler layout or host byte order.

**Prevention:** Parse with explicit size limits and field encodings into temporary state; validate version, complete structure, endianness and profile compatibility before one commit point. Compare the full canonical digest before/after every refused import. Bind replay to profile, build, content hash and timed input sequence; compare per-checkpoint digests, not just final pixels. Keep durable save-file I/O outside the core.

**Detection:** Failure leaves a changed digest or save bytes; a parser accepts unknown/trailing data by accident; or a replay passes after any bound identity field is altered.

## Rights and Evidence Stops

- **D2 — external assets local-only:** do not place external diagnostic ROMs, saves, firmware or derived protected material in public CI, commits, or artifacts without documented rights. Authored fixtures remain the default.
- **D5 — oracle unresolved:** do not run or copy Angrylion-Plus or other external oracles for this milestone. Do not derive a golden from an emulator comparison. If this gate changes later, report comparisons only as differential evidence with exact oracle/version/modification provenance.
- **D6 — no release:** the release channel/versioning decision is still open; milestone research or passing local evidence does not authorize a release or version tag.

## Phase-Specific Warnings

| Phase | Likely pitfall | Prevention |
|---|---|---|
| 7 — RDP/VI | Broad raster cases obscure whether framebuffer writes or VI sampling are wrong. | Independent fill pixels, separate RDRAM/raw-frame assertions, and a wrong-pixel control first. |
| 8 — AI/raw capture | Host playback/capture cadence changes the guest stream; PCM transport is overstated as RSP synthesis. | Guest-time sample cadence, explicit queue/backpressure, host cursor separation, and bounded claims. |
| 9 — SI/save/replay | Host input timing leaks into guest events; malformed imports partially apply; replay binds too little identity. | Logical guest timestamps, defined refusal postconditions, staged import, exact identity binding and digest checks. |

## Sources

- [Project contract](../PROJECT.md), [rolling roadmap](../ROADMAP.md), [autonomy and owner stops](../AUTONOMY.md), and [v1.0 retrospective](../RETROSPECTIVE.md) — owner decisions, current scope and observed process lessons.
- [Nintendo 64 Programming Manual, Chapter 8](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro08/08-01.html) — archival mirror of Nintendo-authored hardware boundary descriptions. Cross-checked against the manual PDF copy in search results. Confidence: MEDIUM (primary authored document, archival hosting).
- [Nintendo 64 Introductory Manual, audio architecture](https://ultra64.ca/files/documentation/online-manuals/man/kantan/step2/3-1.html) and [audio flow](https://ultra64.ca/files/documentation/online-manuals/man/kantan/step2/3-3.html) — archival mirrors of Nintendo-authored descriptions of CPU/RSP synthesis and AI PCM transport. Confidence: MEDIUM.
- Remaining prevention steps are recommendations inferred from the owner contracts and v1.0 lessons; they are phase test targets, not claims of existing passing behavior.
