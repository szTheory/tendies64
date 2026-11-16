# Requirements: Tendies64

**Milestone:** v1.1 Observable Machine (planning identifier; no release version)
**Defined:** 2026-11-15
**Core value:** Every support claim is backed by a named profile, pinned build and reproducible, rights-cleared guest evidence.

## v1.1 Requirements

These17 requirements are carried verbatim from the v1.0 rolling requirements
archive. No completion is inferred from prior CPU/RSP work. CORE-02 retains
its partial scheduler foundation; remaining device integration must be proved.

### RDP and VI

- [ ] **VID-01**: An in-tree software RDP renders command lists (fill, copy, textured and shaded triangles, combiner, blender, coverage, Z) into RDRAM
- [ ] **VID-02**: SYNC_FULL and the DP interrupt behave per the references
- [ ] **VID-03**: A software VI produces raw frames from RDRAM using the profile's TV timing, interlace, pixel format and filtering, never assuming 320×240
- [ ] **VID-04**: RDP conformance is reported as differential evidence with labeled oracle versions (Angrylion-Plus and parallel-rdp used out-of-tree only), never as hardware truth
- [ ] **VID-05**: Undefined-behavior choices (noise seed, undefined memory color) are recorded as named policy choices
- [ ] **VID-06**: Raw video output stays separate from host scaling and filtering

### Audio and raw capture

- [ ] **AUD-01**: AI DMA, the two-deep queue, DACRATE cadence and interrupt behavior produce raw audio buffers with sample index and rate metadata
- [ ] **AUD-02**: An authored audio diagnostic checks exact sample values across DMA boundaries; "non-silent" is never accepted as evidence
- [ ] **AUD-03**: Audio cadence is derived from guest time and never from a host clock
- [ ] **RUN-06**: User can capture raw video frames and raw audio sample buffers from a headless run, as files with metadata, separately from host presentation

### Controller, saves and reproducibility

- [ ] **SAVE-01**: SI and the PIF state machine parse joybus and deliver the standard controller from timed logical input
- [ ] **SAVE-02**: EEPROM (4 Kbit and 16 Kbit), with busy window and block rules, and SRAM are supported as explicitly selected save devices, never guessed from the title
- [ ] **SAVE-03**: Save media can be imported and exported as bytes, with endianness handling and a refusal that leaves the digest unchanged
- [ ] **SAVE-04**: Unknown or unsupported save device requests are refused with defined postconditions
- [ ] **CORE-02**: Device activity (CPU, RSP, RDP, VI, AI, PI, SI, MI) is ordered by the scheduler, with the CPU-to-RSP synchronization quantum recorded in the profile identity
- [ ] **CORE-04**: User can export and import a versioned machine state with a canonical digest, and a restore-then-digest round trip is exact
- [ ] **CORE-05**: User can record and replay timed input as an artifact bound to profile, build and content hash

## Evidence and authority limits

VID-04 remains a specific blocked acceptance: D5 local-oracle authority is open
and parallel-rdp needs a per-file/asset rights audit. Authored/native RDP/VI
implementation may proceed. Authored expectations or stable regression pins
do not replace the required labeled differential evidence. No oracle execution,
protected input use/publication, physical measurement or release is authorized
by this requirements file. Existing D2/D4/D5/D6/D7 stops remain unchanged.

Phase8 queue/interrupt assertions must follow the cited primary queue transition,
not the old provisional phrase implying IRQ on every DMA start. The original
AUD-01 wording already requires interrupt behavior without that assumption.

## Future requirements — v1.2

- **VER-05**: Maintainer can see a clean installed package run the same permitted headless fixture through an independent consumer, with relocation and negative controls
- **API-01**: A C-ABI instance API defines lifetime, profile selection, transactional content admission, timed input, bounded advance, output ring buffers with backpressure, reset and unload
- **API-02**: Errors fall into three classes: refused (state unchanged), guest-stop (not an error) and defect (instance poisoned), each with tested postconditions
- **API-03**: The API is frozen only after the postconditions are exercised through the installed consumer
- **API-04**: The core owns no files, clocks, entropy or threads; the host supplies them
- **API-05**: A minimal host (optional SDL3 target, separate from the core) presents raw video and audio and maps a physical controller
- **API-06**: A rights-cleared playable homebrew path (authored here or built from a pinned libdragon toolchain) runs both headlessly and in the minimal host
- **API-07**: LibFuturo fit is evaluated against its then-current contract, and any adapter is optional and does not shape the core API

## v2 Requirements

Deferred. Each item names the evidence that would promote it.

### Performance (PERF)

- **PERF-01**: Named benchmark workloads with output held constant, reporting throughput and latency separately from CI time (promote after a repeatable correctness baseline)
- **PERF-02**: Cached interpreter and RSP decode cache (promote when PERF-01 shows the bottleneck)
- **PERF-03**: Block JIT with lockstep digest equality against the interpreter (promote last)

### Accuracy and Timing (ACC)

- **ACC-01**: Cycle-accurate CPU timing and CP0 hazard behavior (promote after tier 1 and a primary-document timing source)
- **ACC-02**: Hardware-derived oracle vectors from documented public sources, where they improve a material claim

### Expansion and Accessories (EXP)

- **EXP-01**: Expansion Pak 8 MB profile, and RDRAM behavior above 4 MB without it
- **EXP-02**: PAL and MPAL profiles (requires DACRATE and VI timing evidence)
- **EXP-03**: Other CIC variants and the PIF challenge/response model
- **EXP-04**: FlashRAM
- **EXP-05**: Controller Pak, then Rumble Pak, then Transfer Pak (Transfer Pak needs separately cleared Game Boy content)
- **EXP-06**: RTC
- **EXP-07**: 64DD as a separate system profile (needs a rights-cleared disk route)

### Player Features (PLAY)

- **PLAY-01**: Save-state UI, rewind, fast-forward
- **PLAY-02**: GPU renderer backend as an optional host-side presentation path
- **PLAY-03**: GDB stub and debugger front end over the trace seam

## Out of Scope

| Feature | Reason |
|---------|--------|
| Bundled Nintendo PIF/IPL/CIC data, commercial ROMs, saves, 64DD media | Owner rights stop; a hash proves identity, not rights |
| HLE microcode or GLideN64-style graphics hacks in the canonical core | Breaks determinism and accuracy claims; RSP is LLE |
| Per-game hack database or title-string inference | Compatibility claims must come from profiles and evidence |
| Plugin architecture or `dlopen` plugins | One canonical core; plugins add ABI risk and nondeterminism |
| CPU overclock and wall-clock limits inside the core | Guest time is the only clock |
| Cheats, widescreen patches and texture packs inside the machine | Host concerns; they change guest-visible behavior |
| Netplay, RetroAchievements | Player features not needed to prove the core; revisit after v1 |
| Copying GPL emulator code or Angrylion-Plus | Incompatible with Apache-2.0 or restricted by its license |
| Vendoring the Retro Regen test suite | Its terms forbid modification and rebranding |
| Wholesale clone of PeterLemon/N64 | Contains a boot-code blob of undocumented origin and hundreds of prebuilt ROMs |
| Branding, logos or store listings | "Tendies64" is not trademark-cleared (D7) |
| Claiming a market gap | The research found partial incumbents for each slice; the claim is unproven |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| VID-01 | Phase 7 | Pending |
| VID-02 | Phase 7 | Pending |
| VID-03 | Phase 7 | Pending |
| VID-04 | Phase 7 | Pending — oracle authority/asset gate unresolved |
| VID-05 | Phase 7 | Pending |
| VID-06 | Phase 7 | Pending |
| AUD-01 | Phase 8 | Pending |
| AUD-02 | Phase 8 | Pending |
| AUD-03 | Phase 8 | Pending |
| RUN-06 | Phase 8 | Pending |
| SAVE-01 | Phase 9 | Pending |
| SAVE-02 | Phase 9 | Pending |
| SAVE-03 | Phase 9 | Pending |
| SAVE-04 | Phase 9 | Pending |
| CORE-02 | Phase 9 | Partial — scheduler foundation verified; remaining device integration open |
| CORE-04 | Phase 9 | Pending |
| CORE-05 | Phase 9 | Pending |

**Coverage:**17 active;17 mapped exactly once;0 unmapped. Eight future v1.2 requirements plus v2 outlines remain deferred.
