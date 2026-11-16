# Phase 3: First Headless ROM Through Open IPL3 — Research

**Researched:** 2026-11-10  
**Domain:** Deterministic C17 emulator core, open IPL3 boot, headless runner  
**Confidence:** HIGH for project contracts and artifact provenance; MEDIUM for the minimum CPU/device path pending executable qualification

<user_constraints>
## User Constraints

### Locked Decisions

- **D-01:** Pin n64brew/libdragon commit `98154dafd5ac50e74ee09a5de1035d24a53f54f0`, `boot/bin/ipl3_compat.z64`, the r8-source compatibility variant. It is the flat loader, with direct Unlicense source/binary coverage documented in Decision A. Tendies64 maintainers own the pin, notices, review and updates. Verify actual bytes/digest lengths and source provenance before admission; recommendations do not substitute for the fixture manifest and production checks.
- **D-02:** The baseline is an upstream committed generated artifact retrieved from the pinned source, with byte identity checked. Its original compiler and signer are unknown. A deterministic fetch/assembly recipe can reproduce the selected fixture input without proving regeneration from source. Qualify a clean source rebuild separately before making that stronger claim; pin tools, remove stale compat output, distinguish signature-region differences and retain the full digest. No full SDK or unsigned alternative silently replaces the pin.
- **D-03:** Keep every downloaded manual/document outside the repo and CI artifacts. Commit the primary evidence index, URLs, exact hashes, identity, sections and claim scope only. Hardware facts, upstream software expectations, project model policies and unknowns must remain distinct. CIC checksum signing does not authorize or require Nintendo firmware bytes.
- **D-04:** Add a clearly named, versioned NTSC/cartridge/4 MiB prepared-RDRAM warm-entry profile. Its HLE-PIF transition copies the owned open IPL3 payload and starts CPU execution at the source-defined entry; it does not skip IPL3. Prepared memory/reset type, boot artifact identity, HLE seed/version and CPU/cache/time policies belong to profile identity and documented limits. Exact original IPL2 handoff values remain unknown. Select concrete numeric seed/state as an explicit software contract before implementation, from the pinned path's requirements; do not label guessed constants as hardware observations.
- **D-05:** The first executable slice pins tools, inspects/disassembles the actual linked artifact and records the warm path's opcode/device inventory. This is a prerequisite to bounded CPU implementation, not a source-level guess or permission checkpoint. Support the reachable integer/control/delay-slot, load/store, CP0 and cache behavior with independent expectations; unsupported execution stops explicitly and never becomes an implicit no-op or pass.
- **D-06:** CPU execution uses the existing per-instance deterministic scheduler; define instruction retirement, guest ticks, device deadlines and stop ordering together. Host clocks remain backstops only. Include all future-affecting CPU, CP0, prepared-memory, boot and device state in the versioned canonical digest; update profile/digest versions and independent goldens deliberately.
- **D-07:** Implement the bounded reset/register/probe subset the actual warm path consumes (RI preset, memory-capacity/zero-source policy, MI/SP/PI and SI/AI reset writes), with explicit policy labels where hardware evidence is absent. CPU code executing from SP DMEM is not RSP instruction execution. Keep the RSP engine in Phase 5 and RI/RDRAM cold calibration in Phase 6. Verify virtual KSEG accesses before routing them to the existing physical debug bus.
- **D-08:** Author a tiny MIPS assembly hello plus linker script and narrow deterministic cartridge packer. Use a source/byte-pinned cross assembler/linker toolchain for that concrete capability; avoid the full SDK and a new custom assembler. No nemu64-test or commercial/protected bytes. Record source and binary rights separately, tool identities, recipe, size/hash, expected text provenance, permitted CI/redistribution and boot/profile requirements.
- **D-09:** Model the narrow libdragon ISViewer compatibility contract, not an assertion about original ISViewer hardware: magic, 512-byte buffer, big-endian stores and byte-count flush at +0x14, with a separately specified +0x16 halfword lane. Flush exact requested bytes without alignment padding. Bound output, handle zero/partial/full/oversize/repeated flushes, and never let lost/truncated capture produce pass. Output crosses a core-owned bounded copy seam, not host I/O callbacks during advance. Define/drain/test it before promising an API.
- **D-10:** Parse a bounded canonical timed logical-input schedule, bind its digest to the result and emit events at their guest ticks through the existing trace seam. This phase proves scheduling/trace only; it does not claim SI/PIF controller consumption. Trace-on/off and advance partitions must preserve guest outcomes and final state at the same bound.
- **D-11:** Close WR-06 before any real guest result: bounded copy and hash into a private per-run file, finalize/read-only/reopen/verify, unlink and pass the already-open descriptor only. Real runner admission reads that descriptor's bytes and binds the result to their digest, never reopens the original path. Protect input schedule bytes equivalently. Transactional failures must not launch a guest or produce pass; close/reap/clean every descriptor/process path.
- **D-12:** The existing real-protocol baseline is `t64.result` **v1**, currently synthetic-only; no v2 exists yet. Add explicit strict real **v2** and dual-read support while retaining byte-identical v1 behavior/goldens and separate native `t64.model-result` v1. Require real build/profile/fixture/input/log/state identity and guest stop/time fields. Keep unknown-field/version/refusal rules strict; internal schema versioning is separate from the unresolved release channel.
- **D-13:** Pass requires the authored fixture's exact transcript and actual CPU progress through IPL3 and hello. Preserve the six outcomes and harness-error precedence. Empty output is boot-fail even at a guest bound; non-empty incomplete bounded output is timeout; unsupported execution and refusal are explicit. Host kills/crashes/report failures remain harness errors. Synthetic misbehavior controls retain their own scope and cannot serve as CPU execution evidence.
- **D-14:** Use the existing Linux/GCC and macOS/Clang gate jobs. Both must load identical authorized fixture/input/profile bytes, assert exact guest text and compare the same canonical state digest; compiler build identities may differ. Partition/interleave/trace checks and meaningful wrong-text/empty-output, unsupported-opcode, source-replacement/descriptor and tool/fixture-identity controls must fail for the right reason. Review each hosted run's measured cost.

### Agent discretion

- **D-15:** Choose small module boundaries, command spelling, precise bounded capacities and tool pins from the above contracts and existing patterns. Resolve reversible engineering choices autonomously. Any new rights conflict, protected content use, physical test, publication/paid/destructive action outside standing policy is an explicit stop. No owner acknowledgment is needed for ordinary implementation, tests, CI, reviews, fixes or tracking.

### Deferred Ideas (OUT OF SCOPE)

- Full VR4300 ISA/CP0/TLB/cache/COP1 qualification: Phase 4.
- RSP scalar/vector execution: Phase 5; true cold RI/RDRAM/calibration: Phase 6.
- RDP/VI/audio and guest-visible controller/save/replay: later roadmap phases.
- New IPL3 variants, full libdragon SDK/ELF path and general archive parsers.
- Owner stops remain unchanged: nemu64-test rights, release channel, copy-license expansion, oracle rights, protected owner files and physical hardware testing.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Requirement (verbatim) | Research support |
|---|---|---|
| BOOT-01 | “The machine boots through an HLE-PIF step that seeds post-IPL2 state and then runs a pinned open IPL3 as guest code, with no Nintendo boot bytes in the repository” | Versioned prepared-warm profile, owned IPL3 bytes in SP DMEM, actual CPU entry, explicit software seed and unknown handoff limits. |
| BOOT-02 | “The pinned IPL3 binary has recorded sha256 and md5, source revision, license and a named maintenance owner” | Pin and byte-verify the 4,096-byte upstream `ipl3_compat.z64`; record both digests, commit, Unlicense and Tendies64 maintenance owner in fixture provenance. |
| BOOT-03 | “A boot-limits document lists unmodeled PIF challenge/response, CIC variants and timeout behavior, so \"boots\" is never reported as \"boot solved\"” | Publish exact profile/boot limits and constrain the claim to authored warm-entry execution. |
| BOOT-04 | “A hello ROM, authored in this project or rights-reviewed, prints through ISViewer under the headless runner on the first profile” | Pinned `.S` toolchain, narrow ROM packer, bounded ISViewer contract and real-run transcript oracle. |
| RUN-01 | “User can run a ROM headlessly with a fixed reset, a named profile, timed logical input and a guest-time limit, and receive a machine-readable result” | Runner input schedule, bounded guest execution, strict real-result v2 and descriptor-bound fixture. |
| RUN-03 | “User can see a result classified as boot-fail, not pass, when a guest produces empty output” | Preserve outcome precedence and add real empty-halt and empty-bound controls. |
| RUN-05 | “User can capture guest log output through an ISViewer-style interface, with the length-register and buffer semantics specified in writing” | Explicit libdragon compatibility behavior, exact byte-lane, partial flush and overflow/refusal tests. |
| PROF-05 | “User gets identical state digests for identical profile, content, input and guest-time bounds, across two operating systems and two compilers” | Linux/GCC and macOS/Clang run the same authorized inputs and compare canonical machine-state identity, excluding host build identity. |
</phase_requirements>

## Summary

Plan this as an actual open-IPL3 warm-entry vertical slice. First qualify and byte-pin Agent A's upstream generated artifact, select a versioned software HLE seed/preset contract, and disassemble the pinned executable to establish the exact reachable opcode/device inventory. Then build the smallest real CPU entry path with an explicit unsupported stop. Add the authored hello, bounded ISViewer capture, descriptor-backed runner and strict real-result v2 after that entry path is observable.

The hardware evidence does not establish exact IPL2-to-custom-IPL3 registers. Nintendo §6.10 is an aggregate boot-code overview; NEC documents CPU/address/cache behavior but not the N64 IPL2 handoff. Report the result as a deterministic prepared-RDRAM warm-entry smoke profile, not cold boot or “boot solved.” The phase deliberately excludes RSP instruction execution, cold RDRAM calibration, controller consumption, and broad CPU qualification. [CITED: Nintendo Programming Manual v5.2 §6.10; NEC VR4300 User's Manual §§4.2–4.3, 5.2.4, 5.4.9, 11.3–11.5; exact URLs/hashes in `03-PRIMARY-EVIDENCE-INDEX.md`]

**Primary recommendation:** Use the pinned r8-source `ipl3_compat.z64` generated artifact as an independent verified input; do not require upstream source rebuild to use it. Pin a separate cross-toolchain for the authored `.S` fixture. Make all HLE seed, prepared memory, timing, cache and result choices explicit software policies in profile identity.

## Project Constraints (from AGENTS.md)

- Keep one deterministic machine core separate from host files, clocks, input mapping, output sinks and lifecycle.
- Use named profiles and evidence tiers; a boot or digest alone does not establish hardware accuracy or broad compatibility.
- Treat firmware, ROMs, manuals, fixtures, parsers and generated binaries as rights-bearing or untrusted. Hashes establish identity, not rights.
- Keep the dependency tree small; do not use protected fixtures, nemu64-test, Nintendo PIF/IPL/CIC bytes, physical tests or unlicensed emulator code.
- Preserve the established C17/CMake/Ninja, ownership, refusal-postcondition and determinism-lint patterns. Phase 3 does not freeze the public ABI.

## Architectural Responsibility Map

| Capability | Primary owner | Secondary owner | Boundary |
|---|---|---|---|
| HLE profile, CPU/CP0, virtual bus, prepared RDRAM and reset devices | `core/` | — | Per-instance guest state and guest-time only. |
| Bounded ISViewer guest-byte capture | `core/` | runner | Core owns data/limits; runner drains a copy after advance; no host callback during execution. |
| Timed logical-input schedule | runner | core scheduler/trace | Runner validates/canonicalizes; core emits guest-tick trace. No SI/PIF input consumption claim. |
| ROM fixture and IPL3 provenance | fixture/build tools | rights manifest and CI | Authored assembly plus narrow packer; pinned IPL3 remains a separate input. |
| Result, attestation and process lifecycle | supervisor/runner | result checker | Supervisor passes only the bytes it verified; v1 and model-result stay separate. |

## Standard Stack

| Area | Use | Guidance |
|---|---|---|
| Core | Existing C17 library and allocator/ownership seams | Add CPU/device state per instance; preserve warnings-as-errors and core determinism lint. |
| Build and tests | Existing CMake 3.25+, Ninja, CTest, `./scripts/gate.sh` | No new runtime C dependency. Add explicit CTest targets and update the checked inventory. |
| Guest fixture | Pinned MIPS cross assembler/linker plus `objcopy`/`objdump`; authored `.S`, linker script, narrow packer | Agent A identified an official libdragon toolchain asset (GCC 16.2.0, binutils 2.45; asset hash is in Decision A). Use it only as a pinned build tool if target qualification succeeds; do not imply it built the upstream IPL3. No Newlib/full SDK linkage is needed for a freestanding hello. |
| Runner | Existing POSIX supervisor, JSON/SHA helpers, result checker and manifest | Extend current seams; do not add a separate test framework or schema package. |

**Tool availability:** The current host lacks `mips64-elf-*` tools. Pin and provision the authored fixture toolchain in the build recipe/CI. The IPL3's original compiler and signer remain unknown; the verified committed binary is usable without a source-reproducibility claim. [CITED: Decision A and Decision C, accessed 2026-11-10]

## Architecture Patterns and Implementation Seams

### Boot and CPU path

The current profile is still the Phase 2 skeleton; `t64_machine_advance` delegates to the event scheduler, and `t64_machine` contains no CPU/CP0 execution state ([`core/t64_machine.c:18-63,490-499`](../../../core/t64_machine.c), [`core/t64_internal.h:120-157`](../../../core/t64_internal.h)). The physical debug bus is not a guest virtual bus ([`docs/spec/machine-contract.md:93-120`](../../../docs/spec/machine-contract.md)). Implement the work in this order:

1. Verify fixture manifest, exact 4,096-byte IPL3 and SHA-256/MD5; disassemble the actual pinned artifact and record reachable instructions, CP0 register numbers/operations, cache ops, memory addresses, DMA and reset writes. Do not infer full coverage from source-level opcode guesses.
2. Create the named NTSC/cartridge/4 MiB prepared-warm profile. Copy the admitted IPL3 bytes to its linked SP-DMEM entry, seed the source-defined arguments and start the real CPU there. A deterministic software contract can use `s3=0` (cart), `s4=1` (NTSC), `s5=1` (warm), `s6=0` (fixed unused seed), and `s7=1` (project boot-info version); label the latter values as profile policy, never hardware facts. [CITED: pinned libdragon boot README/source in Decision A/B]
3. Implement only the disassembled reachable integer/control, delay-slot, load/store, CP0 and CACHE behavior. Translate 32-bit KSEG0/KSEG1 before physical bus access. Stop with a specific unsupported-execution reason for every unimplemented opcode/address; never retire it as a no-op or pass.
4. Add the precise warm-path RI/RDRAM probe and MI/SP/PI/SI/AI reset behaviors the artifact consumes. Give the absent-RAM probe and unbacked SP-DMA zero source explicit profile policy IDs; keep 4 MiB as installed RAM. Do not generalize these exceptions into a hardware open-bus claim.

Use the existing scheduler's `(tick, sequence)` order for CPU steps and device deadlines ([`core/t64_scheduler.c:10-13,315-351`](../../../core/t64_scheduler.c)). Before the first CPU event, specify retirement/guest-tick cost, whether same-tick CPU or DMA goes first (sequence order), inclusive-bound behavior, halt/unsupported precedence and event-budget stop behavior. A fixed instruction-granular profile is a deterministic model, not cycle accuracy. Increment `instruction_count` only on actual retirement. Preserve SP/PI DMA deadlines and interleaving, not instantaneous copies.

Keep only the CP0 register behavior reached in the qualified path and independently test masks/state transitions. A named CP0 initialization policy is synthetic HLE state; do not claim hardware reset values. Implement the exact reachable CACHE operations under a versioned policy and test affected memory/fetch behavior. Full TLB, exceptions, COP1, general cache fidelity and CPU breadth remain Phase 4; RSP scalar/vector instruction execution remains Phase 5. IPL3 executing CPU code from SP DMEM does not require an RSP engine. [CITED: NEC manual and pinned compat source; Decision B]

### Profile and canonical state

Version the profile and digest serialization when adding CPU, CP0, boot, prepared-RDRAM, cache or output state. Bind the IPL3 digest, HLE argument/seed version, reset kind, capacity/zero-source rules, CPU quantum, CP0/cache policy, event ordering and ISViewer contract to profile identity. Include every future-affecting CPU/device/boot field in the canonical state digest; exclude wall time, host paths and compiler identity. The current profile descriptor version is 1 and canonical state serialization version is 5; extending them needs deliberate version changes and independently checked goldens ([`core/t64_machine.c:18-63`](../../../core/t64_machine.c)).

### Hello fixture, output and input trace

Build a project-authored `.S` hello with a linker script and no SDK runtime. The packer handles only the header, byte order and flat ROM layout required by the selected compat loader; it is not an assembler or generic archive parser. Pin every build tool/version/hash and record authored-source and ROM-binary rights, recipe, expected transcript provenance and CI distribution policy separately from the IPL3 artifact. Keep the exact cross-toolchain identity in fixture provenance.

Implement only the documented libdragon-compatible ISViewer path: magic check, 512-byte guest buffer at `+0x20`, big-endian stores, exact requested-byte flush at the specified `+0x14` count with the project-defined `+0x16` halfword lane. This is a software compatibility contract, not original hardware semantics. Core owns bounded bytes and overflow state; host drains a bounded copy outside `advance`. Lost/truncated/over-limit output can never pass. Test zero, 1, 3, 4, 512, 513 bytes, partial words, repeated flush, chunk boundary and wrong lane.

Parse a bounded canonical input schedule in the runner, bind its digest to v2 result identity and queue events at guest ticks through the existing trace seam. Trace reports scheduling only; no controller behavior is implemented. Toggling trace and varying advance partitions must leave guest result/state unchanged.

### Real result and fixture attestation

Keep `t64.result` v1 behavior/goldens byte-identical and keep `t64.model-result` v1 separate. Add a closed real v2 writer/checker with actual build/profile, fixture, input, log and state digests plus guest tick/stop fields. Preserve six outcome names and precedence: empty output is `boot-fail` even at the bound; only non-empty incomplete output at the guest bound is timeout; refusal, unsupported execution, process failure and report/capture errors cannot become pass. Pass requires exact authored transcript and observed CPU milestones through IPL3 and hello. [VERIFIED: `docs/spec/result-protocol.md:15-23,44-54,263-273`; the v1 stamp is synthetic-only and the path-reopen TOCTOU gap is explicitly recorded there.]

Before any real result, close WR-06: bounded copy/hash to an exclusive private file, finish writes, set read-only, reopen and verify, unlink, rewind and pass only the open descriptor. Runner admission hashes/uses those descriptor bytes and reports the admitted digest; never reopen caller paths. Apply equivalent identity to input schedule. On any copy/hash/open/validation failure, do not launch and do not emit pass; clean all descriptors/processes on every path. Add synchronized source-replacement test.

## Don't Hand-Roll

| Problem | Avoid | Use |
|---|---|---|
| IPL3 provenance | Rebuilding and claiming byte reproduction without tool/signing evidence | Upstream committed byte-pinned artifact, manifest and separate optional clean-build qualification. |
| Instruction encoding | Hand-encoded hello or a custom assembler | Pinned N64 assembler/linker and disassembly. |
| CPU scheduling | Separate host-clock loop or independent CPU timer | Existing deterministic event queue with explicit retirement/deadline policy. |
| Result JSON/hash | New serializer, crypto or result schema dependency | Existing canonical JSON/SHA helpers and strict checker patterns. |
| Guest output | Host callback/file writes inside core advance | Core-owned bounded capture and explicit drain/copy. |
| ROM parsing | Generic archive/SDK loader | Narrow authored packer for agreed flat ROM layout. |

## Common Pitfalls

- Calling the prepared warm-entry profile a cold boot or authentic post-IPL2 state. Exact IPL2 register state is unknown; cold RI/RDRAM calibration is Phase 6.
- Treating libdragon source arguments, RI behavior or emulator ISViewer lane as hardware facts. Label upstream software expectations and project model policy separately.
- Using source opcode guesses instead of disassembling the pinned executable; an unhandled opcode must fail explicitly.
- Skipping IPL3 while reporting boot success, or counting synthetic scheduler events as retired CPU instructions.
- Implementing a broad CPU, RSP, ELF loader or full SDK to satisfy this one path.
- Hashing a fixture by path and later letting the runner reopen that path; this leaves a real TOCTOU gap.
- Letting any dropped log bytes, wrong transcript, empty output, malformed v2, unknown field or unsupported instruction classify as pass.
- Comparing whole result JSON across host compilers when truthful build IDs differ; compare machine/profile/content/input/log identity and state digest.
- Claiming controller input consumption from timed trace events, or original ISViewer compatibility from libdragon behavior.
- Rebuilding the upstream IPL3 with a new toolchain and calling it byte-reproducible; its original compiler/signer are unknown and source rebuild is separate qualification.

## Security Domain

No network service, user authentication or session exists in this phase; auth/session/access-control controls do not apply. Input validation applies to ROM, schedule, addresses, lengths, opcodes and result fields. Treat fixture and schedule as untrusted, bound all copies/parses, preserve refusal-without-mutation, and pass only verified open descriptors. Use existing SHA-256 for identity; do not invent a new cryptographic mechanism. Run rights and PII/private-path scans before CI evidence is retained; manual bytes and raw host paths stay out of the repository/artifacts.

## Validation Architecture

Use the existing 73-entry gate inventory as baseline, preserve every current ID and append these 17 phase identities; if none are renamed/removed, the proposed gate inventory is 90. Update `scripts/check-test-inventory.sh` and its reviewed inventory/selftests with the additions. No test was run for this research artifact.

| CTest identity | Coverage |
|---|---|
| `unit.core.boot-entry` | HLE profile/seed identity, artifact digest check, DMEM copy and exact CPU entry. |
| `unit.core.cpu-path` | Qualified instruction decode/retirement, delay slots, virtual KSEG mapping and explicit unsupported stop. |
| `unit.core.cp0-cache` | Reachable CP0 masks/state and CACHE semantics under the named software policy. |
| `unit.core.warm-device-policy` | RI preset, absent-memory probe, zero-source DMA and reset-register subset. |
| `unit.core.isviewer` | Magic/buffer/lane/count/partial/repeated/overflow semantics and bounded drain. |
| `runner.result-v2` | Strict real v2, dual-read v1, unknown-field/version rejection, six outcomes and independent golden. |
| `runner.fixture-descriptor` | Admitted bytes come from verified unlinked descriptor; source replacement cannot alter guest input/result digest. |
| `integration.open-ipl3-hello` | Actual pinned IPL3 and authored hello execute; assert CPU milestones and exact ISViewer bytes. |
| `integration.open-ipl3-determinism` | Same process with different advance partitions and trace on/off yields same guest output/state. |
| `controls.real.empty-halt` | Empty halted guest is boot-fail, never pass. |
| `controls.real.empty-bound` | Empty output at guest bound is boot-fail, not timeout/pass. |
| `controls.real.wrong-transcript` | Wrong or incomplete non-empty output is guest-fail/timeout by contract, never pass. |
| `controls.real.unsupported-opcode` | Unsupported instruction stops explicitly and cannot pass. |
| `controls.real.output-overflow` | Truncated/lost capture is rejected as pass. |
| `controls.real.source-replaced` | Replace original fixture after supervisor verification; descriptor bytes/digest remain the admitted original. |
| `controls.real.identity-mismatch` | Altered IPL3/tool/fixture/profile identity fails before or during admission as specified. |
| `controls.real.input-partition` | Same canonical timed schedule is traced at the same guest ticks under partition/trace toggles. |

Also require:

- `./scripts/gate.sh` on both existing Linux/GCC and macOS/Clang jobs; exact same authorized IPL3/hello/input/profile bytes and guest transcript, with equal state digests. Retain truthful per-host build IDs; inspect measured hosted-run cost.
- CTest inventory selftest and repository `core-lint` source plus object-symbol check; include all new core files. Disassemble the pinned artifact and authored `.S`/linked ROM using the pinned toolchain as the separate ISA/fixture audit.
- Rights scan confirms only manifested, cleared authored/Unlicense artifacts and no Nintendo boot bytes; PII/private-path scan runs before evidence retention.
- Existing ASan/UBSan preset on the current host with `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; include new CPU/bus/ISViewer/descriptor paths in the sanitizer inventory. Keep intentional sanitizer negative control separate from production tests.

## Open Qualification Tasks (no owner decision)

1. Pin actual IPL3 and fixture bytes in the manifest; verify expected lengths and both IPL3 hashes. Source/byte identity, rights and maintenance records are separate fields.
2. Pin/provision the cross-assembler/linker for the authored `.S` fixture; confirm target flags and reproducible hello bytes across hosted lanes. No upstream IPL3 source-rebuild claim is needed.
3. Disassemble the actual IPL3 and map every reachable opcode, CP0 register access, CACHE operation, branch/delay slot, memory range and device write. This inventory gates CPU implementation scope.
4. Finalize the explicit software HLE tuple, CP0/cache reset policy, guest tick/retirement rule, same-tick event order, absent-RAM probe and zero-DMA source policies before implementation. Hardware documents do not establish exact original IPL2 state.
5. Finish the boot-limits and output protocol documents; verify exact transcript and descriptor-based runner admission before cross-host pass claims.

## Sources

- Primary evidence index and retrieval hashes: [`03-PRIMARY-EVIDENCE-INDEX.md`](03-PRIMARY-EVIDENCE-INDEX.md). Manuals were retrieved 2026-11-10; exact downloaded bytes remain private.
- NEC, *VR4300/VR4305/VR4310 User's Manual*, U10504EJ7V0UMJ1, 7th ed. (2000), §§4.2–4.3, 5.2.4, 5.4.9, 11.3–11.5: [archival mirror](https://hack64.net/docs/VR43XX.pdf), SHA-256 indexed. [CITED]
- Nintendo, *Programming Manual v5.2*, §6.10 boot overview: [archival mirror](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/pro-man/pro06/06-10.htm), SHA-256 indexed. It does not specify exact IPL2-to-custom-IPL3 state. [CITED]
- Pinned libdragon source/artifact and Unlicense: [`03-DECISION-A.md`](03-DECISION-A.md), commit `98154dafd5ac50e74ee09a5de1035d24a53f54f0`; exact artifact SHA-256/MD5 and source facts are recorded there. [CITED]
- Project behavior contracts: [`docs/spec/machine-contract.md`](../../../docs/spec/machine-contract.md), [`docs/spec/result-protocol.md`](../../../docs/spec/result-protocol.md), [`core/t64_scheduler.c`](../../../core/t64_scheduler.c), [`runner/t64_supervisor.c`](../../../runner/t64_supervisor.c), [`scripts/check-test-inventory.sh`](../../../scripts/check-test-inventory.sh).

**Confidence breakdown:** core seams and requirements HIGH (opened local sources); artifact/source rights HIGH per Decision A; hardware handoff LOW/unknown; exact reachable instruction/device scope LOW until disassembly; prepared-RDRAM and cache policies are explicit model choices, not console facts.
