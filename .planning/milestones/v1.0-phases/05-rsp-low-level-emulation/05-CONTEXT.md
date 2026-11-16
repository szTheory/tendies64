# Phase5: RSP Low-Level Emulation — Context

**Gathered:** 2026-11-13
**Status:** Ready for implementation research and planning

<domain>
## Phase boundary

Execute real RSP scalar/vector microcode from IMEM/DMEM, with SP control and
DMA semantics, a permanent portable VU reference, native/ROM-level evidence,
individually reasoned local RSP diagnostic classification and no canonical
HLE path. RSP-01 through RSP-05 and all four roadmap success criteria remain
binding. Video/RDP execution, cold boot and physical console testing stay out.
</domain>

<decisions>
## Selected decisions under delegated autonomy

- **D-01 — Immutable capability profile.** Add a distinct RSP-capable profile,
  sharing the canonical CPU primitives. Preserve every existing profile's ID,
  descriptor/state bytes, boot policy and supported/refused operations. Resolve
  CPU family, compatibility, SP and state capabilities explicitly; do not
  assume model/state numeric ordering implies a capability. Every new RSP
  register, flag, pending result, divider latch and event affecting future
  execution belongs to the new state domain. No frozen public API yet.
- **D-02 — Portable reference first.** Implement the VU in readable C17 with
  explicit bytes/lanes, masked unsigned48-bit accumulator arithmetic, complete
  flags and divider state. This is the permanent canonical/reference path.
  Add no SIMD, JIT or new dependency without measured need. Differential fuzz
  is required for any optimized path if one is introduced; absence of a second
  path must be reported honestly, not called a successful differential test.
  Native independently derived vectors and bounded per-opcode state fuzz still
  exercise the reference's edge/invariant behavior.
- **D-03 — Evidence-bound timing and refusals.** Begin with a named deterministic
  functional execution model and explicit guest-time/issue policy. Research
  must translate the guide's documented SU/VU order, hazards and timing
  constraints before plans freeze; do not inherit the old atomic DMA timing as
  hardware fact. Prefer the smallest model that satisfies the stated output
  criteria. A cycle-pipeline proposal must justify concrete required behavior,
  not the label. Unknown exact arbitration, running-memory corruption and
  asynchronous halt/single-step effects remain unsupported or explicit model
  policies with reversal evidence; never invent their silicon result. This
  follows the original architecture's instruction to declare unsupported
  oddities rather than guess. No cycle-accuracy claim is authorized.
- **D-04 — Primary entry-gate ledger.** Use the SGI/Nintendo RSP guide pinned
  in the instruction research for documented issue/hazard/DMA/status/VMOV/clamp
  facts. Preserve conflicting or incomplete vector evidence. The evidence
  check found no defined exact halt-corruption or DMA port-arbitration result;
  that is an unknown, not a successful hardware check. Before final plans,
  map every named gate area to primary evidence, a permitted vector, or an
  executable refusal/explicit profile limitation. The reciprocal ROM contents
  and special inputs also need provenance or a declared supported boundary;
  do not paste tables from emulator code or use host floating point.
- **D-05 — Early actual integration.** Qualify a component-scoped source/value
  projection, tool/dependency/asset closure and selector/boot image before
  affected private use. Reuse unchanged accepted CPU tool/source receipts
  where applicable, but the CPU-only overlay is not RSP qualification. Plan an
  actual bounded RSP build/composition/startup/refusal attempt near the front,
  before a long native expansion sequence. An observed unsupported operation
  guides the next slice. Preserve failed attempts and original limits.
- **D-06 — Shared canonical reporting primitives.** Preserve the CPU inventory,
  batch schemas and exact membership contracts. Add an independently sealed
  RSP component contract on shared parse/encoding/aggregation primitives;
  avoid a parallel script completeness calculator. Each selected required
  RSP registration/value receives authenticated execution or an explicit
  individually reasoned incomplete/refused classification. Never equate
  classified failures/refusals with passing microcode or hidden dropped cases.
- **D-07 — Same rights and host limits.** All external source-derived inventory,
  built ROMs, identities, captures and results remain ignored/private, and
  external tests stay absent from public CI. No new physical test, copy
  allowlist expansion, protected asset, paid tool or release is authorized.
  Apps/tests run headlessly in background; preserve exact current caps and
  specific no-retry instructions. New native work uses the canonical gate,
  all-target sanitizer build before owning tests, current fuzz bounds and the
  existing two required hosted platform/compiler jobs. Long stress is a separate
  scheduled lane under the roadmap; external diagnostic material stays disabled
  there until D2 rights permit it.
- **D-08 — Cohesive slices and evidence.** Lead with an authored fetched IMEM
  scalar/vector-to-DMEM-to-BREAK program, independent expected bytes/flags and
  control transitions. Add SP scheduling/DMA and actual private integration
  before completing every opcode. Tracers prove production routes, not test
  helper shortcuts. Meaningful RED must show intended fetched opcode and
  state, not setup failure. Review substantive coherent slices; reuse
  unaffected verification for narrow fixes and metadata-only closeout.

The owner delegated these reversible implementation choices in AUTONOMY;
no new owner risk acceptance or hardware authority is inferred.
</decisions>

<canonical_refs>
## Canonical references

- `AGENTS.md` and `.planning/AUTONOMY.md` — authority, stops, bounded workflow.
- `.planning/ROADMAP.md` Phase5 and `.planning/REQUIREMENTS.md` RSP section.
- `.planning/phases/05-rsp-low-level-emulation/05-INSTRUCTION-DECISIONS.md` —
  primary guide identity/pages, portable-reference decision and unknowns.
- `.planning/phases/05-rsp-low-level-emulation/05-DMA-TIMING-DECISIONS.md` —
  DMA/issue alternatives and concrete evidence limits.
- `.planning/phases/05-rsp-low-level-emulation/05-PROFILE-INTEGRATION-DECISIONS.md`
  — distinct profile and component qualification/reporting boundary.
- `docs/spec/machine-contract.md`, `docs/spec/cpu-profile.md`,
  `docs/spec/cpu-compatibility.md`, `docs/spec/cpu-diagnostic.md` — preserved
  old machine and reporting contracts.
- `docs/adr/0003-cpu-timing-tiers.md`, `docs/adr/0007-copy-and-reading-policy.md`,
  `docs/policy/copy-provenance.md`, `docs/policy/rights-and-fixtures.md`.
- `.planning/phases/03-first-headless-rom-through-open-ipl3/03-PRIMARY-EVIDENCE-INDEX.md`
  — existing public primary-reference identity ledger.
- `.planning/research/ARCHITECTURE.md` RSP section and
  `.planning/research/PITFALLS.md` RSP section — dated leads only; unverified
  community claims do not override the new primary evidence ledger.
</canonical_refs>

<code_context>
## Existing code insights

- `core/t64_sp.c` owns bounded DMA with one active/one pending descriptor;
  current atomic completion, timing and warm no-RSP stubs must stay versioned.
- `core/t64_internal.h` owns per-machine IMEM/DMEM and SP state; scheduler
  events use guest ticks plus stable sequence. `t64_mi.c` owns interrupts.
- `core/t64_bus.c` maps SP backing; CPU/cache/link paths already observe DMA
  destination writes. New writers must use the same reservation notifications.
- `core/t64_machine.c` canonical descriptors/admission and
  `core/t64_digest.c` per-version serializers are integration boundaries.
- Existing `core/tests/test_sp_dma.c`, profile/digest/partition/interleaving
  tests, cleared fixture builders and exact CTest inventory are reusable.
- CPU diagnostic code has real shared C parsing/aggregation and immutable
  execution seals; reuse primitives without broadening CPU membership.
</code_context>

<specifics>
## Synthesized adversarial check

The DMA adviser favored a cycle-pipeline profile. Its own counterargument is
material: the guide is not an arbitration specification, and cycle labels
could create false precision while delaying first use. Select the functional
reference baseline with documented issue constraints and explicit unsupported
boundaries; implementation research must justify any additional pipeline work
against a required observable behavior. Keep the ambitious long-term timing
goal; this is evidence/delivery order, not a permanent accuracy ceiling.

The instruction adviser's first tracer spans too many opcode families if
implemented as one large prerequisite. Keep its end-to-end production purpose
but start with the smallest scalar + one vector operation + DMEM + BREAK path,
then add its hazard/VMOV/DMA controls with the behavior they verify. This avoids
an apparently small tracer becoming a full instruction-layer plan.

Reject extending old CPU profiles, treating skipped CPU inventory members as
RSP coverage, and using the reference's own output as its only expected value.
The private source-route observation establishes a missing RSP integration
path, not a failed actual guest run or a prequalified RSP asset closure.
</specifics>

<deferred>
## Deferred areas

Host SIMD/JIT optimization waits for measured need and full differential
verification. Physical console capture and public external diagnostic use
remain explicit owner stops. RDP/video and cold boot retain later phases.
Unknown timing/corruption effects are visible profile limits, not quietly
implemented assumptions or evidence that this phase's supported behavior ran.
</deferred>
