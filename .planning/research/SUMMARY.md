# Project Research Summary

**Project:** Tendies64  
**Milestone:** v1.1 Observable Machine  
**Researched:** 2026-11-15  
**Confidence:** MEDIUM overall; strong for scope and observed project seams, lower for device timing and hardware-visible behavior.

The current milestone extends a deterministic, headless N64 machine through raw video, audio transport, controller/save devices, and reproducible state and input artifacts. Keep the existing three-phase sequence and 17 requirements. Use the current C17 core, scheduler, CTest harness, and runner; the research finds no need for new dependencies. The first useful delivery should prove a real consumer path: an authored RDP fill writes RDRAM, VI scans it into a raw frame, and the runner retrieves the frame with independently calculated expected pixels and a wrong-pixel control.

This proves a small, inspectable software behavior, not broad game compatibility or hardware accuracy. Keep one guest-time order, explicit output/refusal ownership, and separately versioned behavior where semantics change. A key limit remains: VID-04 calls for labeled differential evidence using Angrylion-Plus and parallel-rdp out of tree. D5 authority for Angrylion-Plus is unresolved, and parallel-rdp assets still need a per-file rights audit. Continue permitted authored/native work, but preserve VID-04 as a specific blocked acceptance until those gates are resolved; authored tests cannot be described as differential evidence.

## Key Findings

### Stack and architecture

- **Stack:** Existing C17, CMake 3.25+, Ninja, CTest, in-tree test harness, sanitizer/fuzzer paths, and headless runner are sufficient. Do not add a renderer, codec, serializer, database, or test-framework dependency for v1.1.
- **Machine ownership:** Extend the existing per-instance machine and `(guest_tick, sequence)` scheduler. RDP, VI, AI, SI, MI, and CPU activity share guest time; host clocks, physical input mapping, presentation, files, and durable storage stay outside the machine.
- **Outputs:** Core devices produce raw buffers and guest-time metadata. The runner writes capture files. Keep the host output cursor outside canonical guest state/digests; define bounded output and resume/refusal effects so consumption cannot silently drop data or change guest progress.
- **State and refusal:** Serialize named future-affecting device state and timing phase in explicit versioned bytes. Validate imports before mutation; refused imports preserve the prior digest. Bind replay to profile, build, content, and timed logical inputs.
- **Profile compatibility:** Apply the approved versioning pattern in [CPU compatibility policy](../../docs/spec/cpu-compatibility.md) and [Phase 6 context](../milestones/v1.0-phases/06-cold-boot-cpu-timing-and-full-base-suite/06-CONTEXT.md): preserve old identities/bytes when their meaning is unchanged and add only fields or versions required by concrete semantic changes. The generic architecture note is not authority for a stricter compatibility promise, a profile per device, or gratuitous version churn.

### Requirements and evidence

Carry forward the 17 existing Phase 7–9 requirements without broadening the first profile: six VID requirements, three AUD requirements, RUN-06 capture, four SAVE requirements, and CORE-02/04/05.

- **Phase 7:** RDP command classes write RDRAM; SYNC_FULL and DP interrupt; profile-driven VI scanout; named undefined-behavior policies; raw output separate from host presentation; VID-04 differential evidence.
- **Phase 8:** Authored PCM through AI DMA, two-deep queue, exact samples and guest-time cadence; runner-owned raw A/V capture. Correct the roadmap's “IRQ at DMA start” phrasing against the Nintendo-authored RCP header: test the `ai_full` 1→0 transition rather than assuming one interrupt per submission/start.
- **Phase 9:** Timed logical controller input through SI/PIF; explicitly selected EEPROM/SRAM; byte import/export with transactional refusal; exact machine-state round trip and identity-bound input replay. Close CORE-02 only after cross-device ordering is checked across all in-scope devices.

### Main pitfalls to prevent

1. **Self-confirming pixels:** Calculate expected fill pixels independently of renderer code; check RDRAM and VI output separately and require a deliberately wrong pixel to fail.
2. **Host-controlled determinism:** Keep output drains, capture writes, and presentation from changing guest time, event order, digest, or raw bytes. Specify backpressure and resume behavior.
3. **Accidental hardware claims:** Name deterministic noise/undefined-memory policies and bind machine-visible choices to profile identity. A stable output or emulator match is not silicon evidence.
4. **Unversioned semantic drift:** Preserve existing profile behavior and hashes where semantics are unchanged; version only the concrete profile/state behavior that changes.
5. **Partial state mutation or weak replay binding:** Parse imports into temporary state, validate all identity/schema fields, then commit atomically; compare digests around refusals and checkpoint replay.

### Confidence and unresolved points

| Area | Confidence | Basis and gap |
|---|---|---|
| Stack | HIGH for current project/toolchain observations; MEDIUM for recommendation | Existing targets and seams were inspected; no implementation or build was verified by this research. |
| Features | HIGH for retained owner/requirement scope; MEDIUM for device behavior | Requirements are explicit; timing and protocol details still need phase-level checks against primary references. |
| Architecture | HIGH for existing machine/scheduler boundaries; MEDIUM for new device timing | Code observations are direct; exact RDP/VI/AI timing and state schema remain design work. |
| Pitfalls | MEDIUM | Failure modes follow the project contract and device boundaries; recommendations do not demonstrate current code passes. |
| Overall | MEDIUM | Good enough to order phases and define the first slice; not enough to claim silicon qualification. |

Open work: VID-04's D5 authority and parallel-rdp file/asset audit; RDP/VI timing and video-domain phase; AI cadence/carry and queue edge; Joybus/EEPROM timing; exact capture/state schemas. No physical silicon measurement or broad compatibility is established. Protected fixtures and external oracle use remain subject to project rights stops.

## Implications for Roadmap

**Recommendation:** Retain Phases 7 → 8 → 9 and begin Phase 7 with the real-consumer fill-to-raw-frame path. It tests the newly added device through the existing headless runner and isolates the first pixel error at RDRAM or VI. The adversarial check is that a simple fill can pass while complex RDP modes or VI filtering remain wrong; therefore keep it as an early vertical slice, not as completion evidence for the remaining VID requirements. Reverse the phase order only if phase evidence shows the path cannot isolate required behavior or the existing guest-time representation cannot express a required boundary without changing qualified profiles. Any such change needs a concrete, versioned rationale.

### Phase 7 — RDP reference renderer and VI scanout

**Rationale:** Phase 4 already supplies command-list producers, and raw video is the prerequisite for Phase 8 capture. Start with one accepted authored operation through the actual consumer: fill list → RDRAM pixels → VI raw frame → runner output, with independent expected pixels and a wrong-pixel control. Then add the remaining required RDP modes, completion/interrupt, VI timing/filter cases, and deterministic named policies.

**Acceptance boundary:** VID-04 remains a named blocker. Its retained requirement calls for oracle-labeled differential evidence with Angrylion-Plus and parallel-rdp out of tree. D5 is unresolved and parallel-rdp assets are unaudited; do not run/use/copy oracle material or claim VID-04 complete until the applicable authority and asset gates are resolved. Continue the authored/native implementation and independent tests meanwhile. Do not replace the differential criterion with authored vectors or a generic statement that results are “conformant.”

**Research flag:** Deep planning research remains appropriate for command timing/TMEM/coverage, SYNC_FULL ordering, VI filters, video clock phase, and the VID-04 rights/evidence path. These gaps do not block the permitted first slice.

### Phase 8 — Audio interface and raw capture

**Rationale:** Add AI after the guest-timed output model is established; capture becomes useful once both raw video and audio exist. Verify authored PCM values across DMA boundaries, queue behavior, sample index/rate, and host-throttling independence. Align the interrupt assertion to the documented `ai_full` falling edge.

**Research flag:** Moderate planning research for DACRATE cadence/phase, 8 KiB carry cases, queue transitions, and exact interrupt boundary. Keep RSP audio-microcode support out of this transport requirement.

### Phase 9 — Controller, save media, state and replay

**Rationale:** SI/PIF timed input and explicit save-device selection precede replay/state acceptance. Finish with byte-exact state/replay round trips after all in-scope device state is serializable; check malformed/refused imports preserve state and digest. Then verify CORE-02 event ordering, equal-tick ties, split/whole advances, and interleaved instances across CPU/RSP/RDP/VI/AI/PI/SI/MI.

**Research flag:** Moderate planning research for Joybus response timing, EEPROM write/busy/range behavior, and untrusted import validation. Keep FlashRAM and other save accessories deferred.

**Ordering and scope:** The existing 17 requirements and phase boundaries remain the roadmap input. No new dependency or broad profile is implied. Keep API/ABI freeze and the installed independent public consumer in Phase 10, and native host/presentation work in Phase 11. Tie every support statement to the exact profile, build, fixture rights, and evidence tier.

## Sources

- Current phase research: [STACK.md](STACK.md), [FEATURES.md](FEATURES.md), [ARCHITECTURE.md](ARCHITECTURE.md), [PITFALLS.md](PITFALLS.md).
- Project decisions: [ROADMAP.md](../ROADMAP.md), [AUTONOMY.md](../AUTONOMY.md), [archived v1.0 requirements, lines 45–128](../milestones/v1.0-REQUIREMENTS.md), [CPU compatibility policy](../../docs/spec/cpu-compatibility.md), and [Phase 6 context](../milestones/v1.0-phases/06-cold-boot-cpu-timing-and-full-base-suite/06-CONTEXT.md).
- Nintendo primary references checked by researchers: [RCP register header](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm) (AI queue/interrupt fields), [RDP programming](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro12/index.htm), [RDP synchronization](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro12/12-02.htm), and [VI overview](https://jrra.zone/n64/doc/pro-man/pro30/30-01.htm).
- Oracle/asset references recorded in STACK.md: [Angrylion-Plus license](https://github.com/ata4/angrylion-rdp-plus/blob/master/MAME%20License.txt), [parallel-rdp-standalone README](https://github.com/Themaister/parallel-rdp-standalone/blob/master/README.md), and [parallel-rdp-standalone license](https://github.com/Themaister/parallel-rdp-standalone/blob/master/LICENSE). These sources do not resolve the project's D5 authority or per-file asset audit.

---
*Research synthesized: 2026-11-15*  
*Ready for roadmap: yes, with VID-04 retained as a specific blocked acceptance and Phase 7–9 scope unchanged.*
