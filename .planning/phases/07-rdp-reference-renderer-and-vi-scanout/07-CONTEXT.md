# Phase 7: RDP Reference Renderer and VI Scanout - Context

**Gathered:** 2026-11-16
**Status:** Ready for planning; VID-04 acceptance specifically blocked

<domain>
## Phase Boundary

Implement VID-01–06: in-tree software RDP into RDRAM, FullSync/DP completion,
profile-timed VI raw frames, explicit undefined-behavior policy and separate
raw output. Required oracle-labeled differential evidence remains incomplete
while D5 and the parallel-rdp per-file/asset audit are unresolved. Permitted
authored/native work proceeds. No audio/SI/save implementation, ABI freeze,
installed consumer, host window, physical tests or release is pulled forward.
</domain>

<decisions>
## Implementation Decisions

### Real consumer before expansion

- **D-01:** Lead with an authored CPU-written DP fill list through ordinary
  bus/DP dispatch, RDRAM, VI and the actual headless runner. Independently
  derive literal RDRAM pixels and raw-frame expectations. A wrong-pixel control
  reuses the successful setup and fails at the intended comparator. Record
  first differing address/coordinate, not only a final image hash. Run this
  cheap accepted path before long native/instrumented suites. It is a tracer,
  not completion of copy/triangle/combiner/blender/coverage/Z/filtering work.
- **D-02:** Stay with current C17/CMake/CTest and authored diagnostics. Add no
  renderer, codec, serialization dependency, cache or production concurrency
  without a concrete capability and measured need. Runtime commands execute
  in the background; existing rights/resource limits remain unchanged.

### DP command and completion ownership

- **D-03:** Keep DP registers, pointers, status, RDP/TMEM state, pending work
  and completion fences in the canonical machine. Use scheduler-owned
  incremental command processing, not complete synchronous rendering hidden
  inside an END register write. FullSync raises MI DP only after preceding
  modeled pipeline effects complete; CURRENT==END alone is insufficient.
  Exact tick costs/tie policies need a named sourced or software-model basis.
- **D-04:** Reserve bounded capacity before mutation. Register admission checks
  register/address/profile constraints; fetched commands validate their full
  required words before their own effects. Do not add a whole-list snapshot
  or scan merely to simplify validation without explaining observable effects.
  Refused admission preserves its defined state/digest/tick. An unsupported
  encountered opcode stops at that command, with no effects or synthetic
  completion from it; earlier accepted/completed commands remain committed.
  Keep busy/pending/completion distinct and test masks, DP clear and FullSync
  held behind earlier work. This stop policy is software behavior where
  unsupported-encoding hardware behavior is unknown.

### VI time, raw output and refusal effects

- **D-05:** Derive VI phase from canonical elapsed guest time using bounded
  exact rational phase/remainder, separate from CPU retirement and host time.
  Include future-affecting VI phase/pending events in machine state/digest.
  Source the selected TV constants and register latch/field behavior; name
  any unresolved timing choices as profile software policy. Preserve existing
  guest tick ownership; do not call each instruction a VI cycle.
- **D-06:** Use a bounded lossless raw packet queue and observer copy-out.
  Reserve complete output capacity before committing its producing boundary;
  output-full stops resumably with the producing event pending, without loss,
  overwrite or fabricated guest progress. Host drain/cursor/file writes do
  not advance guest time. Insufficient destination capacity reports required
  size without partial copy or cursor movement; invalid spans are refused.
  Test equal guest-boundary digests and byte-identical output across drain
  cadence, advance partitions and interleaved instances.
- **D-07:** Specify raw capture encoding and metadata explicitly (an initial
  RGBA8 software packet is suitable): actual geometry/stride, source pixel
  representation, field/parity, VI filter state, guest timing, sequence and
  profile/build identity. Keep RDRAM source bytes, VI scanout and host scaling
  distinct. No assumed320x240. Host cursors/output buffers stay outside the
  canonical digest; retain every guest-future-affecting device field. Detailed
  queue sizing and raw copy-out schema remain implementation discretion.

### Identity, evidence and rights

- **D-08:** Apply the already approved compatibility policy: preserve established
  semantics/identities where unchanged; version concrete changed composite
  behavior, descriptor/state layouts and policy fields as needed. No profile
  per device, version per fix, or additional compatibility promise. Keep the
  public C surface provisional through Phase9; Phase10 owns ABI freeze.
- **D-09:** Explicitly name deterministic noise/undefined-memory policies and
  bind their affected semantics to profile identity. Do not infer silicon
  behavior from stable output, secondary code, or an emulator comparison.
  Keep source facts, code observations, choices, measurements and unknowns
  distinct. Each remaining command class needs independent expected results.
- **D-10:** VID-04 remains blocked/incomplete. No Angrylion execution/copying/use
  or oracle-derived publication is authorized here. Parallel-rdp requires its
  per-file/asset audit. Authored pins cannot replace the required differential
  evidence. D2 external diagnostic assets/results remain local-only; D4/D6/D7
  and physical/paid/destructive stops persist without repeated confirmation.

### Agent discretion

Concrete internal layouts, bounded integer algorithms, safe queue limits,
profile/schema names and version numbers follow the approved policies and
primary evidence. Make the smallest verifying implementation promptly;
reopen a decision only for a concrete contradiction. No rights/risk acceptance
is delegated by routine implementation discretion.
</decisions>

<canonical_refs>
## Canonical References

- `.planning/AUTONOMY.md` and `AGENTS.md` — authority, limits and efficiency.
- `.planning/ROADMAP.md` Phase7 and `.planning/REQUIREMENTS.md` VID-01–06.
- `.planning/research/SUMMARY.md` — scoped milestone synthesis.
- `.planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-COMMAND-OPTIONS.md` — primary DP/register/FullSync locators and options.
- `.planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-OUTPUT-OPTIONS.md` — primary VI locators, output/time alternatives.
- `.planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-ADVERSARIAL-CHECK.md` — independent counterexamples and discriminating controls.
- `docs/spec/machine-contract.md` — lifetime, tick, refusal and bus contracts.
- `docs/spec/cpu-compatibility.md` — approved versioned behavior pattern.
- `docs/spec/cpu-timing.md` — existing guest-time domain.
- `docs/spec/rsp-instructions.md` and `docs/spec/rsp-timing.md` — current DP refusal and RSP scheduling boundaries.
- `.planning/milestones/v1.0-phases/06-cold-boot-cpu-timing-and-full-base-suite/06-CONTEXT.md` — composite profile/version and early consumer lessons.
- `.planning/RETROSPECTIVE.md` — cheap accepted-path, source-seal and evidence-retention checks.

The linked primary Nintendo/SGI manuals and SDK register definitions are
references, not permission to redistribute documents, protected inputs or code.
Planning must check/pin the RDP/VI facts used before implementation claims.
</canonical_refs>

<code_context>
## Existing Code Insights

Reuse `core/t64_internal.h` canonical state, `core/t64_scheduler.c` bounded
(tick,sequence) reserve/commit, `core/t64_mi.c` interrupt source/mask ownership,
`core/t64_bus.c` profile-gated dispatch and `core/t64_digest.c` explicit semantic
serialization. RSP DP operations currently refuse; enable only the new intended
profile path. `core/t64_observe.c` demonstrates observer-only cursors, but is
not implemented raw-video evidence. Runner owns files and supervision; extend
its real fixture path and inherited inventory. Retain source/body seals and
rights/provenance checks when production files or dispatch bodies change.
</code_context>

<specifics>
## Specific Ideas

The owner repeatedly requested a sanity check on churn and cheap consistency
checks before long processes. Use successful real-consumer acceptance plus
reached negative controls, source/composition checks and exact inventories
before expensive suites; no retry, concurrency or cache is inferred.
</specifics>

<deferred>
## Deferred Ideas

GPU rendering, codecs, live preview loss policy, physical measurement, broad
compatibility, new media/regions, ABI freeze and native presentation stay in
later scope. Oracle-derived acceptance waits on its existing specific gates.
</deferred>

## Owner update — triangle numeric policy (2026-11-17)

**D-11:** Owner approved proceeding with an explicit two's-complement
software-profile policy for triangle coefficient encodings whose primary numeric
representation remains unresolved. This relaxes only Plan04's primary-source-only
numeric decoder stop. The reconciled primary packet lengths and field positions
remain mandatory. Authored tests prove the chosen software behavior; they do not
close the source gap, establish hardware accuracy, or discharge VID-04. Keep the
policy named/versioned under D-08, and preserve all D2/D5/D6 and other authority
stops. No further per-field confirmation is required within this decision.
