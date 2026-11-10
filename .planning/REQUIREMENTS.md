# Requirements: Tendies64

**Defined:** 2026-11-09
**Core Value:** Every Tendies64 support claim is backed by evidence: a named profile, a pinned build and a reproducible, rights-cleared guest result.

**Evidence rule for this document.** Hardware-behavior requirements here are
hypotheses taken from community references and implementation observations.
No researcher verified CPU, RSP, RDP or boot details against the NEC VR4300 or
Nintendo manuals. Each hardware requirement must be re-confirmed against a
primary document (pinned by SHA) or a hardware-derived test before its phase
plan is final. See `.planning/research/SUMMARY.md` ("Gaps to Address").

## v1 Requirements

Requirements for the verified-core path: from governance and a deterministic
skeleton, through a headless CPU/RSP/RDP/VI/AI machine, to an installed-consumer
API and a playable rights-cleared homebrew. Each maps to a roadmap phase.

### Governance, Rights and Harness (GOV)

- [x] **GOV-01**: Maintainer can read a written decision record (ADR) for the implementation language, build system and ABI approach, with the research default (C17, CMake 3.25, Ninja) and the evidence that would reverse it
- [x] **GOV-02**: Maintainer can see a copy-provenance policy: allowed licenses (ISC, MIT, BSD-3, Unlicense), the rule that GPL and Angrylion-Plus code is never copied, the rule for reading GPL sources, and a per-copy record of source, commit SHA, license, notice and maintenance owner
- [x] **GOV-03**: Maintainer can see a fixture manifest in which every fixture entry records source and revision, build recipe and toolchain, digest, required profile, rights status, notices, and expected-result provenance
- [ ] **GOV-04**: Maintainer can see a claims and evidence-tier template, so each support statement names its profile, build identity and evidence tier
- [x] **GOV-05**: Maintainer can run one command that runs the fast pull-request gate locally and is the command CI calls
- [x] **GOV-06**: Maintainer can read recorded ADRs for CPU timing-tier numbering, the policy on non-hardware emulator-extension opcodes, wall-clock timeout classification and the determinism-lint mechanism
- [x] **GOV-07**: Maintainer can see no protected or unlicensed material (Nintendo PIF/IPL/CIC data, commercial ROMs, saves, 64DD media, unreviewed generated ROMs) in the repository, in public CI inputs or in uploaded artifacts, enforced by an automated check

### Test Harness and Result Protocol (RUN)

- [ ] **RUN-01**: User can run a ROM headlessly with a fixed reset, a named profile, timed logical input and a guest-time limit, and receive a machine-readable result
- [x] **RUN-02**: User can distinguish six outcomes in a result: pass, guest-fail, boot-fail, timeout, missing-fixture and harness-error
- [ ] **RUN-03**: User can see a result classified as boot-fail, not pass, when a guest produces empty output
- [x] **RUN-04**: Maintainer can see negative controls in the suite: a wrong expected value fails; a missing, malformed, timed-out or crashed run never passes; a changed test inventory is detected
- [ ] **RUN-05**: User can capture guest log output through an ISViewer-style interface, with the length-register and buffer semantics specified in writing
- [ ] **RUN-06**: User can capture raw video frames and raw audio sample buffers from a headless run, as files with metadata, separately from host presentation

### Profiles and Determinism (PROF)

- [ ] **PROF-01**: User can select a named machine profile that fixes console revision, TV type, RAM size, boot route, expansion and peripheral set, and the first profile is NTSC, cartridge-only, 4 MB RDRAM with an open IPL3 boot route
- [ ] **PROF-02**: User can see the profile identity, and the build identity, recorded in every result
- [ ] **PROF-03**: User can see TV type and region come from the profile and never inferred from the ROM header country byte
- [ ] **PROF-04**: User can see unsupported, deferred and unknown profile features documented, with the evidence that would promote each
- [ ] **PROF-05**: User gets identical state digests for identical profile, content, input and guest-time bounds, across two operating systems and two compilers
- [ ] **PROF-06**: Maintainer can see two interleaved instances in one process produce digests equal to isolated runs
- [x] **PROF-07**: Maintainer can see a mechanical lint that fails the build when the core reads host time, files, threads, entropy or mutable globals

### Machine Core (CORE)

- [ ] **CORE-01**: The core advances in guest time using a single-threaded event queue with a total order, and the host never drives guest time
- [ ] **CORE-02**: Device activity (CPU, RSP, RDP, VI, AI, PI, SI, MI) is ordered by the scheduler, with the CPU-to-RSP synchronization quantum recorded in the profile identity
- [ ] **CORE-03**: The core runs for a bounded stop condition (guest time, instruction count or frame event) and reports why it stopped
- [ ] **CORE-04**: User can export and import a versioned machine state with a canonical digest, and a restore-then-digest round trip is exact
- [ ] **CORE-05**: User can record and replay timed input as an artifact bound to profile, build and content hash
- [ ] **CORE-06**: Maintainer can inspect guest-time-stamped trace events at device boundaries without changing results

### Bus, Memory and Devices (BUS)

- [ ] **BUS-01**: The core implements the memory map, 4 MB RDRAM, cartridge ROM access through PI and the MI interrupt registers, with masked-versus-pending interrupt behavior per the references
- [ ] **BUS-02**: PI and SP DMA copy data with their documented length, alignment and timing rules, never as an instantaneous memory copy
- [ ] **BUS-03**: RDRAM tier A (preset RI configuration) works first, and RDRAM tier B (register protocol, MI repeat mode, calibration read-back) follows for the full cold-boot path
- [ ] **BUS-04**: A ROM is admitted through a documented, bounded procedure: size limits, header parse, byte-order handling, rejection of malformed media, and unchanged state on refusal

### VR4300 CPU (CPU)

- [ ] **CPU-01**: The VR4300 integer interpreter runs MIPS III 64-bit operations with correct 32-bit sign-extension behavior
- [ ] **CPU-02**: Branches, branch-likely and delay slots behave correctly, including exceptions in delay slots
- [ ] **CPU-03**: CP0 (Count, Compare, Status, Cause, Random, writable masks), exceptions and interrupts behave per the primary manual
- [ ] **CPU-04**: Address translation includes the TLB (refill, invalid, modified, XTLB) and address errors
- [ ] **CPU-05**: Unaligned loads and stores, LL/SC and LLD/SCD behave correctly
- [ ] **CPU-06**: A functional cache model exists, with a written scope decision on what cache behavior is guest-visible
- [ ] **CPU-07**: COP1 uses integer soft-float with correct rounding modes, FCR31 and unimplemented-operation behavior; host floating point is never the canonical path
- [ ] **CPU-08**: CPU timing at tier 1 (instruction-granular cost) is available, and the cycle-accurate tier is documented as deferred
- [ ] **CPU-09**: Native component tests use independently sourced expected values, with edge and negative cases for each of the above

### Boot (BOOT)

- [ ] **BOOT-01**: The machine boots through an HLE-PIF step that seeds post-IPL2 state and then runs a pinned open IPL3 as guest code, with no Nintendo boot bytes in the repository
- [ ] **BOOT-02**: The pinned IPL3 binary has recorded sha256 and md5, source revision, license and a named maintenance owner
- [ ] **BOOT-03**: A boot-limits document lists unmodeled PIF challenge/response, CIC variants and timeout behavior, so "boots" is never reported as "boot solved"
- [ ] **BOOT-04**: A hello ROM, authored in this project or rights-reviewed, prints through ISViewer under the headless runner on the first profile

### Fixtures and CI (VER)

- [ ] **VER-01**: Maintainer can build and run the `nemu64-test` `base` suite headlessly from a pinned commit, with per-group status, once its rights are documented (owner stop D2); until then the run is local-only and nothing derived is uploaded
- [ ] **VER-02**: Maintainer can see timing, cycle and cop0hazard groups reported as informational, experimental and stress groups kept out of required CI, and a scheduled lane for stress groups
- [ ] **VER-03**: Maintainer can see test inventories pinned by exact counts, so a missing or reduced test set fails
- [ ] **VER-04**: Maintainer can see CI use explicit runner labels, pinned action SHAs and a measured critical-path review recorded after each run
- [ ] **VER-05**: Maintainer can see a clean installed package run the same permitted headless fixture through an independent consumer, with relocation and negative controls

### RSP (RSP)

- [ ] **RSP-01**: The RSP runs SU and VU (COP2) instructions from IMEM/DMEM, with SP_STATUS, semaphore, halt, break and SP_PC behavior
- [ ] **RSP-02**: A scalar-portable VU reference implementation exists and is the permanent oracle for any optimized path; per-opcode fuzzing shows zero difference
- [ ] **RSP-03**: RSP DMA follows its alignment, bank-wrap and queue rules
- [ ] **RSP-04**: The `nemu64-test` RSP groups pass or are classified individually
- [ ] **RSP-05**: No HLE microcode path exists in the canonical core

### RDP and VI (VID)

- [ ] **VID-01**: An in-tree software RDP renders command lists (fill, copy, textured and shaded triangles, combiner, blender, coverage, Z) into RDRAM
- [ ] **VID-02**: SYNC_FULL and the DP interrupt behave per the references
- [ ] **VID-03**: A software VI produces raw frames from RDRAM using the profile's TV timing, interlace, pixel format and filtering, never assuming 320×240
- [ ] **VID-04**: RDP conformance is reported as differential evidence with labeled oracle versions (Angrylion-Plus and parallel-rdp used out-of-tree only), never as hardware truth
- [ ] **VID-05**: Undefined-behavior choices (noise seed, undefined memory color) are recorded as named policy choices
- [ ] **VID-06**: Raw video output stays separate from host scaling and filtering

### Audio (AUD)

- [ ] **AUD-01**: AI DMA, the two-deep queue, DACRATE cadence and interrupt behavior produce raw audio buffers with sample index and rate metadata
- [ ] **AUD-02**: An authored audio diagnostic checks exact sample values across DMA boundaries; "non-silent" is never accepted as evidence
- [ ] **AUD-03**: Audio cadence is derived from guest time and never from a host clock

### Input and Saves (SAVE)

- [ ] **SAVE-01**: SI and the PIF state machine parse joybus and deliver the standard controller from timed logical input
- [ ] **SAVE-02**: EEPROM (4 Kbit and 16 Kbit), with busy window and block rules, and SRAM are supported as explicitly selected save devices, never guessed from the title
- [ ] **SAVE-03**: Save media can be imported and exported as bytes, with endianness handling and a refusal that leaves the digest unchanged
- [ ] **SAVE-04**: Unknown or unsupported save device requests are refused with defined postconditions

### API and Host (API)

- [ ] **API-01**: A C-ABI instance API defines lifetime, profile selection, transactional content admission, timed input, bounded advance, output ring buffers with backpressure, reset and unload
- [ ] **API-02**: Errors fall into three classes: refused (state unchanged), guest-stop (not an error) and defect (instance poisoned), each with tested postconditions
- [ ] **API-03**: The API is frozen only after the postconditions are exercised through the installed consumer
- [ ] **API-04**: The core owns no files, clocks, entropy or threads; the host supplies them
- [ ] **API-05**: A minimal host (optional SDL3 target, separate from the core) presents raw video and audio and maps a physical controller
- [ ] **API-06**: A rights-cleared playable homebrew path (authored here or built from a pinned libdragon toolchain) runs both headlessly and in the minimal host
- [ ] **API-07**: LibFuturo fit is evaluated against its then-current contract, and any adapter is optional and does not shape the core API

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

Which phases cover which requirements. Updated during roadmap creation (2026-11-09). Phases 1-6
form milestone v1.0, Phases 7-9 form v1.1, and Phases 10-11 form v1.2. Milestone IDs are GSD
planning cycles, not software versions. Phases in v1.1 and v1.2 are provisional and may be
re-sequenced at milestone close.

| Requirement | Phase | Status |
|-------------|-------|--------|
| GOV-01 | Phase 1 | Complete |
| GOV-02 | Phase 1 | Complete |
| GOV-03 | Phase 1 | Complete |
| GOV-04 | Phase 1 | Pending |
| GOV-05 | Phase 1 | Complete |
| GOV-06 | Phase 1 | Complete |
| GOV-07 | Phase 1 | Complete |
| RUN-01 | Phase 3 | Pending |
| RUN-02 | Phase 1 | Complete |
| RUN-03 | Phase 3 | Pending |
| RUN-04 | Phase 1 | Complete |
| RUN-05 | Phase 3 | Pending |
| RUN-06 | Phase 8 | Pending |
| PROF-01 | Phase 2 | Pending |
| PROF-02 | Phase 2 | Pending |
| PROF-03 | Phase 2 | Pending |
| PROF-04 | Phase 2 | Pending |
| PROF-05 | Phase 3 | Pending |
| PROF-06 | Phase 2 | Pending |
| PROF-07 | Phase 1 | Complete |
| CORE-01 | Phase 2 | Pending |
| CORE-02 | Phase 2 | Pending |
| CORE-03 | Phase 2 | Pending |
| CORE-04 | Phase 9 | Pending |
| CORE-05 | Phase 9 | Pending |
| CORE-06 | Phase 2 | Pending |
| BUS-01 | Phase 2 | Pending |
| BUS-02 | Phase 2 | Pending |
| BUS-03 | Phase 6 | Pending |
| BUS-04 | Phase 2 | Pending |
| CPU-01 | Phase 4 | Pending |
| CPU-02 | Phase 4 | Pending |
| CPU-03 | Phase 4 | Pending |
| CPU-04 | Phase 4 | Pending |
| CPU-05 | Phase 4 | Pending |
| CPU-06 | Phase 4 | Pending |
| CPU-07 | Phase 4 | Pending |
| CPU-08 | Phase 6 | Pending |
| CPU-09 | Phase 4 | Pending |
| BOOT-01 | Phase 3 | Pending |
| BOOT-02 | Phase 3 | Pending |
| BOOT-03 | Phase 3 | Pending |
| BOOT-04 | Phase 3 | Pending |
| VER-01 | Phase 4 | Pending |
| VER-02 | Phase 6 | Pending |
| VER-03 | Phase 4 | Pending |
| VER-04 | Phase 1 | Pending |
| VER-05 | Phase 10 | Pending |
| RSP-01 | Phase 5 | Pending |
| RSP-02 | Phase 5 | Pending |
| RSP-03 | Phase 5 | Pending |
| RSP-04 | Phase 5 | Pending |
| RSP-05 | Phase 5 | Pending |
| VID-01 | Phase 7 | Pending |
| VID-02 | Phase 7 | Pending |
| VID-03 | Phase 7 | Pending |
| VID-04 | Phase 7 | Pending |
| VID-05 | Phase 7 | Pending |
| VID-06 | Phase 7 | Pending |
| AUD-01 | Phase 8 | Pending |
| AUD-02 | Phase 8 | Pending |
| AUD-03 | Phase 8 | Pending |
| SAVE-01 | Phase 9 | Pending |
| SAVE-02 | Phase 9 | Pending |
| SAVE-03 | Phase 9 | Pending |
| SAVE-04 | Phase 9 | Pending |
| API-01 | Phase 10 | Pending |
| API-02 | Phase 10 | Pending |
| API-03 | Phase 10 | Pending |
| API-04 | Phase 10 | Pending |
| API-05 | Phase 11 | Pending |
| API-06 | Phase 11 | Pending |
| API-07 | Phase 11 | Pending |

**Coverage:**
- v1 requirements: 73 total
- Mapped to phases: 73
- Unmapped: 0
- By milestone: v1.0 (Phases 1-6) 49; v1.1 (Phases 7-9) 16; v1.2 (Phases 10-11) 8

---
*Requirements defined: 2026-11-09*
*Last updated: 2026-11-09 after roadmap creation*
