# Phase 5: RSP Low-Level Emulation - Research

**Researched:** 2026-11-13
**Domain:** Deterministic N64 RSP/SU/VU emulation and SP integration
**Confidence:** MEDIUM

## User Constraints

### Locked decisions

- Add a distinct immutable RSP-capable profile; keep existing profile identities, state bytes, boot policies, and refusals unchanged. Reuse CPU primitives and use explicit capability predicates, not numeric version ordering.
- Use a portable C17 scalar/lane VU as the permanent canonical reference. No SIMD, JIT, or dependency without a measured need; do not call tests differential when there is no second implementation.
- Use a deterministic functional execution model with explicit guest-time and issue policy. Implement published SU/VU order and hazards; do not claim cycle accuracy or invent unspecified arbitration, active-PC overwrite, or asynchronous halt behavior.
- Start with an authored fetched-IMEM scalar + one VU operation + DMEM + BREAK tracer. Qualify an actual RSP component build/composition/startup/refusal attempt early, before broad opcode expansion.
- Keep a sealed RSP diagnostic contract separate from CPU membership while sharing canonical parser, encoding, and aggregation primitives. Classify each selected member; never count refusals or missing runs as passes.
- Keep external diagnostic inputs and derived outputs private/local-only under existing rights and caps. No physical testing, public external diagnostic CI, allowlist expansion, protected asset, paid service, or release is authorized.
- Preserve existing test inventory and hosted lanes; add real native inventory entries, sanitizer coverage, independent expected values, and bounded per-opcode state fuzzing. No HLE microcode fallback in the canonical core.

### the agent's Discretion

No additional discretion areas are listed in the phase context; routine reversible implementation details remain delegated under `.planning/AUTONOMY.md`.

### Deferred ideas (out of scope)

Host SIMD/JIT optimization, physical-console capture, public external diagnostic use, RDP/video execution, and cold boot remain deferred. Unknown timing/corruption effects stay visible profile limits.

## Phase Requirements

| ID | Description | Research support |
|---|---|---|
| RSP-01 | “The RSP runs SU and VU (COP2) instructions from IMEM/DMEM, with SP_STATUS, semaphore, halt, break and SP_PC behavior” | Add the fetched instruction path, vector state, SP register semantics, and authored ROM/native coverage. Divider table uncertainty below limits any broad support claim. |
| RSP-02 | “A scalar-portable VU reference implementation exists and is the permanent oracle for any optimized path; per-opcode fuzzing shows zero difference” | Keep one portable reference; fuzz state edges/invariants and use independent vectors. Report no differential result while no optimized path exists. |
| RSP-03 | “RSP DMA follows its alignment, bank-wrap and queue rules” | Extend native SP DMA tests for documented alignment/length/stride/queue behavior and deterministic profile policy. |
| RSP-04 | “The `nemu64-test` RSP groups pass or are classified individually” | Add a component-scoped inventory/batch/adapter and authenticate every selected run or explicit incomplete/refused result. Local-only rights boundary applies. |
| RSP-05 | “No HLE microcode path exists in the canonical core” | Add a source/control check that unsupported fetched RSP instructions stop explicitly and cannot route to a microcode shortcut. |

## Summary

Use the SGI/Nintendo RSP guide as the primary architectural source for documented issue ordering, hazards, vector operations, SP control, and DMA shape. Build a separate RSP profile and state domain, reuse deterministic scheduler and shared reporting primitives, and keep CPU inventories and historical profiles byte-for-byte stable. The guide is not a complete arbitration or timing specification; the implementation must state model policies and unsupported cases.

The guide describes reciprocal and reciprocal-square-root lookup mechanics but does not publish either table's contents or settle all special inputs. Do not copy emulator tables or substitute host floating point. Refuse the divider instruction family before state mutation until an independent, rights-cleared source/vector closes this gap. **Consequence:** the phase can proceed, but it cannot claim complete VU/RSP-01 support or passing compatibility members that require those operations until the gap is resolved.

**Primary recommendation:** Implement the small authored IMEM tracer first, then prove an actual qualified RSP component can build, compose, start, and reach an explicit unsupported-operation result; only then expand scalar/vector coverage.

## Architectural Responsibility Map

| Capability | Primary tier | Secondary tier | Rationale |
|---|---|---|---|
| RSP instruction execution, registers, flags, accumulator, divider latches | Deterministic native core | — | Guest state and instruction effects belong to the canonical machine. |
| SP status, semaphore, PC, and DMA | Deterministic native core | Scheduler/bus | Register effects and transfer progress must be guest-time owned. |
| Diagnostic qualification and aggregation | Host-side test harness | Shared protocol library | It reports evidence about a fresh canonical machine without changing CPU membership. |

## Standard Stack

### Core

| Component | Version | Purpose | Why standard |
|---|---|---|---|
| C | C17 | Canonical scalar SU/VU implementation | Existing build contract contains `set(CMAKE_C_STANDARD 17)` [VERIFIED: `CMakeLists.txt:1-4`]. |
| Existing CMake/Ninja gate | Installed versions below | Build and native test orchestration | Reuse the established project gate and inventory; add no package. |

### Alternatives considered

| Instead of | Could use | Tradeoff |
|---|---|---|
| Portable scalar VU | Host SIMD/JIT | Defer until a named benchmark shows need and full per-opcode differential coverage exists. |
| Functional issue model | Full cycle pipeline | Adds state and apparent precision without complete port-arbitration evidence; only add when a required observable behavior demands it. |

No external package is recommended or installed; package legitimacy audit is not applicable.

## Architecture Patterns

### Recommended project structure

```text
core/t64_rsp.c                 # fetched SU/VU decode, execution, issue/hazard policy
core/t64_sp.c                  # SP register/status/semaphore and DMA integration
core/t64_internal.h            # per-machine RSP state and narrow internal contracts
core/t64_machine.c             # immutable profile capability/admission
core/t64_digest.c              # new versioned state domain; retain old encodings
core/tests/test_rsp_*.c        # independent native vectors and authored tracer
runner/t64_rsp_diagnostic.c    # component adapter; keep CPU membership frozen
runner/t64_rsp_diag_protocol.* # component contract over shared parser primitives
```

These are recommended ownership seams, not files observed to exist. Current observations: machine state contains SP banks/state and CPU state, with no RSP interpreter state; SP DMA reserves scheduler events and reads/writes data at completion; digest/profile logic branches on legacy versions. Add exact RSP capability predicates instead of treating a larger version number as stronger capability [VERIFIED: `core/t64_internal.h:431-475,477-498`; `core/t64_sp.c:123-160,297-356`; `core/t64_digest.c:120-210`; `core/t64_machine.c:151-200`].

### Minimal deterministic execution/issue contract

Use one profile-defined guest tick as one **model RSP issue opportunity**. Fetch in IMEM program order; issue at most one SU and one VU instruction in a slot, pairing only adjacent candidates in original order when the documented register rules permit it. Never reorder to fill a slot. A documented hazard consumes a deterministic stall slot; track readiness in per-machine integer state. This is a functional issue model, not a representation of all five internal pipeline stages and not a cycle-accuracy claim [CITED: guide pp. 39, 41-44].

Model documented result delays/hazards and branch delay/bubble behavior explicitly. Put the CPU:RSP tick ratio and every selected stall/ordering policy in the new profile hash. For DMA, model a fixed setup charge selected within the guide's published 6-12 clock range and at most one 8-byte beat per model issue tick; label both as software policy. If an instruction and DMA beat would require an undocumented same-bank arbitration result, stop with an explicit profile limitation before partial mutation. Do not reuse old atomic completion timing as a hardware claim [CITED: guide pp. 39, 43-44, 96-97; VERIFIED: `core/t64_sp.c:123-160,297-356`].

Implement architectural effects before broad performance timing: explicit-byte vector registers and lane accessors; unsigned masked 48-bit accumulator arithmetic; complete VCO/VCC/VCE; opcode-specific vector transfer transforms; CP0 moves, PC/delay behavior, BREAK/HALT/BROKE and optional interrupt; semaphore read-to-set; DMA length/alignment/stride/bank wrap/queue. Do not map vector forms to a generic `memcpy` [CITED: guide pp. 34-38, 47-56, 60-79, 84-97, 170].

The guide says overwriting instructions at the active PC is undefined. Keep that case unsupported or refuse it transactionally; do not invent “halt corruption.” Single-step/asynchronous halt effects and exact CPU/RSP/DMA port priority remain explicit unknowns [CITED: guide pp. 85-88, 96; phase decision records].

### First tracer

Create a project-authored IMEM program that executes one scalar operation, one independently expected VU operation, a DMEM store/load observation, and BREAK. Assert fetched words/PC, complete destination bytes and flags, halt/broke/interrupt effects, and unchanged state on refusal. Expand later with branch delay, vector load/store partial-byte cases, saturation/accumulator slices, hazard stalls, status commands, semaphore, then DMA and ROM-level execution. Expected values must be hand-derived or sourced independently; the VU reference cannot be its own oracle [CITED: guide pp. 34-44, 47-79, 84-97; VERIFIED: phase context `05-CONTEXT.md`].

## Don't Hand-Roll

| Problem | Don't build | Use instead | Why |
|---|---|---|---|
| SU instruction semantics | A new MIPS integer core | Existing CPU decode/ALU primitives behind explicit RSP capability checks | Preserves one semantic implementation and old profile refusals. |
| Test JSON/canonical aggregation | A second general parser or completeness calculator | Existing canonical parser/encoding/aggregation helpers with a separate sealed RSP schema and membership | Prevents parser drift and CPU denominator changes. |
| Vector portability | Host-endian casts or host float arithmetic | Byte/element accessors and integer arithmetic | Makes lane order, overflow, and flags deterministic. |
| Divider tables | Emulator table copy or host `1/x`/`1/sqrt(x)` | Independent permissible vectors or derivation; otherwise explicit refusal | Table bytes and complete edge behavior remain unverified. |

## Common Pitfalls

- **Relabeling old SP behavior:** Existing completion is atomic and model-timed. Treat it as a legacy profile policy, not silicon timing [VERIFIED: `core/t64_sp.c:123-160,297-356`].
- **Profile leakage:** Do not broaden old profile IDs, digest bytes, or refusals. New RSP state, flags, divider latches, pending writes, issue/stall state, and future-affecting events need the new digest domain [VERIFIED: `core/t64_machine.c:18-59,151-200`; `core/t64_digest.c:120-210`].
- **Incomplete vector semantics:** Generic arithmetic misses byte order, load/store partial lanes, accumulator slices, and flag clear/consume behavior [CITED: guide pp. 34-38, 47-79].
- **False differential evidence:** With one implementation, report vector/invariant fuzzing only; do not report “zero differences.”
- **False diagnostic completion:** A refusal, missing run, timeout, or harness error must remain distinct from a pass. Keep external results private/local-only and no external ROM/data in public CI [VERIFIED: `docs/policy/rights-and-fixtures.md:8-21,32-45,84-89`].
- **HLE fallback:** A failed or unsupported fetched instruction must stop explicitly; it must never dispatch to a hidden microcode replacement.

## Validation Architecture

### Test framework and baseline

| Property | Value |
|---|---|
| Framework | Existing CTest/native C tests |
| Quick/full command | `sh scripts/gate.sh` (existing project gate; not run during research) |
| Existing inventory | Quote from saved evidence: “The exact inventories were 111 gate and 113 ASan registrations.” [VERIFIED: `docs/evidence/phase04-native-cpu.md:42-46`] |

Add RSP registrations to `core/CMakeLists.txt` and `scripts/check-test-inventory.sh`, preserving all existing members and updating exact gate/ASan inventories by the real additions. Suggested owned tests: fetched tracer and unsupported/refusal postconditions; SU/PC/delay; VU lane/flags/48-bit slices/transfers; divider boundary/refusal; SP status/semaphore; DMA alignment, wrap, stride, queue, and scheduler ordering; profile preservation/digest sensitivity; HLE-path negative control. Run the all-target sanitizer build before owning tests, as the locked project context requires. These are planned tests, not executed results.

| Requirement | Behavior to verify | Test type |
|---|---|---|
| RSP-01 | Fetched IMEM execution changes DMEM; SP control/PC/BREAK behavior | Native plus authored ROM smoke |
| RSP-02 | Independent per-opcode vectors and bounded edge/invariant fuzz | Native/fuzz; differential only if a second path is later added |
| RSP-03 | Alignment, bank wrap, row stride, queue promotion/refusal | Native component tests |
| RSP-04 | Every selected component member gets an authenticated pass/fail or explicit incomplete/refused classification | Local-only qualified integration |
| RSP-05 | No canonical HLE dispatch; unsupported fetched operations stop | Source/control negative test |

### Wave 0 gaps

- Add the RSP state/model, focused native test registration, sanitizer registration, and sealed component-report contract before broad implementation.
- First integration wave must close source/tool/dependency/asset rights and composition before invoking the external tool; then build/inspect/start one bounded component and observe an actual start or refusal before expanding opcodes.
- Current CPU-only qualification does not establish an RSP-capable overlay or startup path; it cannot be relabeled as RSP qualification [VERIFIED: `05-PROFILE-INTEGRATION-DECISIONS.md`; phase context].

## Environment Availability

| Dependency | Available | Version | Fallback/action |
|---|---:|---|---|
| CMake | Yes | 4.4.3 | Existing build system |
| Ninja | Yes | 1.13.2 | Existing build tool |
| Apple Clang | Yes | 21.0.0 | Existing local compiler |
| RSP-qualified external diagnostic composition | Not established | — | Re-run component-scoped pre-use closure; CPU-only qualification is insufficient |

No build, package install, ROM run, private diagnostic run, or physical-console run was performed during this research.

## Security Domain

| ASVS area | Applies | Control |
|---|---|---|
| V2 Authentication | No | No authentication surface is added. |
| V3 Session Management | No | No session surface is added. |
| V4 Access Control | No | No user/tenant authorization surface is added. |
| V5 Input Validation | Yes | Bounds-check instruction fetch, opcode fields, lane/element access, DMEM/IMEM/DMA spans and diagnostic records; refuse unsupported operations transactionally; fuzz production decoders under existing sanitizers. |
| V6 Cryptography | Limited | Reuse the existing digest implementation for profile/state identity; do not invent a cryptographic primitive. |

Threats include malformed ROM/program bytes causing out-of-bounds access or undefined arithmetic, hidden HLE dispatch disguising unsupported behavior, and rights-bearing diagnostic/ROM material entering public artifacts. Keep memory math unsigned and bounded, refusal side-effect-free, diagnostic assets local-only, and public support claims tied to profile/build/evidence tier [VERIFIED: `docs/policy/rights-and-fixtures.md:8-21,32-45,84-89`; project AGENTS.md].

## Open Questions

1. **Exact reciprocal/rsqrt ROM data and special inputs.** The guide documents normalization/index/result assembly and refinement, but no table bytes. A search this pass did not establish a rights-cleared independent vector set or a complete integer derivation. Do not use emulator literals or host floating point. If no permissible source/vector becomes available, keep divider opcodes explicitly unsupported before effects; affected RSP-01 support and diagnostic members remain incomplete/refused.
2. **Undocumented timing outcomes.** Exact CPU/RSP/DMA same-bank arbitration, active-PC overwrite corruption, and asynchronous halt/single-step effects remain unsupported or named model policies. No new physical testing is authorized or required to describe them as unknown.

## Sources

### Primary

- Silicon Graphics Computer Systems, *Nintendo Ultra64 RSP Programmer’s Guide*, Version 1.1 (cover; internal revision 1.0), ©1996. [Public PDF](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf); downloaded source SHA-256 verified as `6ec646ba9bd836af9aa30ae90099af55dbaf291fbd8b39ff8be5664ada3bd668`. Checked printed pp. 34-44 (registers, pairing, pipeline/hazards), 47-56 (vector transfers), 60-79 (arithmetic, VMOV, divider algorithm), 84-97 (SP control/DMA), and 170 (BREAK interrupt). [CITED: official guide]
- Project policies and source contracts: `AGENTS.md`, `.planning/AUTONOMY.md`, phase 5 context/decision records, `docs/spec/machine-contract.md`, `docs/spec/cpu-profile.md`, `docs/spec/cpu-diagnostic.md`, `docs/policy/rights-and-fixtures.md`, `docs/policy/copy-provenance.md`.

## Assumptions Log

| # | Claim | Risk if wrong |
|---|---|---|
| A1 | One profile-defined guest tick can serve as a deterministic abstract RSP issue opportunity while implementing the guide-backed pair/hazard rules. | If required software depends on undocumented pipeline/port overlap, the model must refuse or be revised with new evidence; no cycle-accuracy claim follows. |
| A2 | A fixed setup charge within the published DMA setup range plus one beat per model tick is a useful first software policy. | Timing-sensitive compatibility may remain incomplete; profile policy and tests must make this visible. |

## Metadata

**Confidence breakdown:** Primary guide facts MEDIUM (published source, incomplete timing/table details); code seams HIGH (read-only source inspection); exact divider support LOW/unresolved.
**Research date:** 2026-11-13
**Valid until:** 2026-12-13 for stable architecture/code seams; recheck mutable tool/qualification state before use.

## Bounded evidence correction — permitted algorithm route

The initial RESEARCH BLOCKED verdict is preserved above as the researcher's
finding, not silently rewritten. It over-constrained the available source
route: ADR0007 and copy-provenance.md explicitly permit reading ares ISC source
and adapting a small allowed-license helper with a full per-copy record. The
owner need not expand the allowlist for ISC. This does not make another
emulator an independent hardware oracle.

A concrete permitted candidate was inspected at ares commit
`0b928930af4cc59ed99e942996f0d3c815379388`:

- [Root ares ISC notice](https://github.com/ares-emulator/ares/blob/0b928930af4cc59ed99e942996f0d3c815379388/LICENSE#L1-L15)
  names the ares team/Near and supplies the allowed notice. Other bundled
  components have separate licenses and are not candidates for this slice.
- [rsp.cpp](https://github.com/ares-emulator/ares/blob/0b928930af4cc59ed99e942996f0d3c815379388/ares/n64/rsp/rsp.cpp#L152-L169)
  generates reciprocal and inverse-square-root lookup words with pure integer
  equations/search; it does not require copying a literal hardware ROM dump.
- [interpreter-vpu.cpp](https://github.com/ares-emulator/ares/blob/0b928930af4cc59ed99e942996f0d3c815379388/ares/n64/rsp/interpreter-vpu.cpp#L1334-L1368)
  contains explicit software input normalization, result/latch assembly and
  edge policies. This is an implementation observation, not silicon evidence.

Retrieved source content and acquisition hashes are retained in ignored
research storage. The selected rsp.cpp SHA256 is
`d475d3f891710750123fbde27f4064913b3d9f5e7f8a0d1ca8fc10854f489390`;
interpreter-vpu.cpp is
`563e7d8a65fe2e4fc2312ee9e439ba0507ac92608f65af08bb4fa0bcc2ba82cf`.
The root ISC notice excerpt was checked before permitted code reading.
No implementation was copied into the core and no candidate was executed.

**Planning consequence:** include a bounded divider slice that audits exact
selected-file/history/license closure before adaptation, records the adapted
helper/notice/source/owner/changes/exposure in the existing provenance system,
and uses per-instance or immutable integer-derived tables. Prefer a small
standalone integer helper over a library dependency. Keep its selected edge
policies explicit in the new profile. Independent integer/rational derivations
and guide-supported cases must verify the equations, boundary behavior and
latch sequencing; comparing only with ares output is insufficient. A provenance
contradiction blocks that affected adaptation and retains explicit refusal.
The new profile's software implementation can proceed by this authorized route;
the guide's missing physical ROM/special-case evidence remains an accuracy
limit, not an invented hardware validation. This parallels the generic
soft-float/CPU-owned policy distinction, without broad copying of emulator code.

The first role handoff returned no saved artifact and was interrupted after
checkpoint requests. Its resumed recovery wrote the report without more
research. This narrowly scoped source/policy correction was then performed by
the orchestrator against a concrete contradiction; no general research loop
or second complete research run occurred.

The context's scheduled-lane clarification preserves the roadmap requirement:
keep the two required native platform jobs, separate long stress from them,
and leave external diagnostic material disabled in hosted scheduling until
D2 permits it. No public external lane or resource expansion was approved.
