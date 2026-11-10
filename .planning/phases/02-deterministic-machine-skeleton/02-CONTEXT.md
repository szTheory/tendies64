# Phase 2: Deterministic Machine Skeleton - Context

**Gathered:** 2026-11-10
**Status:** Ready for planning
**Method:** Three independent decision-cluster researchers, synthesized under
AUTONOMY.md. Routine reversible choices use delegated authority. Planning research
must adversarially check this synthesis before the plans are finalized.

<domain>
## Phase Boundary

Deliver a deterministic C17 machine skeleton: explicit profile, per-instance
state, bounded guest-time scheduler, physical bus, 4 MiB RDRAM, MI, scheduled PI/SP
DMA, transactional raw-ROM admission, state digest and observational tracing.
No CPU execution, boot success, VI scanout, audio, save devices or frozen ABI is
claimed. Synthetic instruction/frame events verify counters, not those devices.

Requirements: CORE-01, CORE-02, CORE-03, CORE-06, PROF-01, PROF-02, PROF-03,
PROF-04, PROF-06, BUS-01, BUS-02, BUS-04. All six ROADMAP success criteria apply.

</domain>

<decisions>
## Implementation Decisions

### Instance and content ownership
- **D-01:** Keep C17/CMake/Ninja/CTest and the provisional opaque-instance API.
  The host supplies allocation/release callbacks; the core owns each allocated
  machine and admitted canonical image until destruction/replacement. Callbacks
  run only during lifecycle/admission, never scheduled device execution. No
  per-access host memory callbacks, mutable globals, host clocks/files or threads.
  Caller spans are readable and stable during admission; reject overlap with
  instance-owned storage. Each instance has disjoint writable RAM and metadata.
- **D-02:** Admission validates and builds a private candidate, normalizes bytes,
  hashes content and commits once complete; allocation failure and every other
  refusal preserve the prior machine digest and observable state. Release the
  old image only after success. Successful replacement resets guest state by an
  explicit documented rule. No fixed 128 MiB dual-ROM arena is required.

### Profiles and identity
- **D-03:** Define a named first NTSC/cartridge/4 MiB/open-IPL3-route skeleton
  profile with explicit console target, timing tier/model versions, integer tick
  unit, CPU:RSP synchronization quantum and scheduler-order version. The route
  identifies intended boot ownership, not implemented boot. Hash a versioned
  canonical descriptor containing every behavior-affecting value. Build identity
  is separate, has no timestamps/private paths, and is absent from state digest.
  Every skeleton test result reports profile ID/hash, build identity and its
  honest evidence tier. Preserve the Phase 1 synthetic runner's identity.
- **D-04:** Header country metadata never selects TV type/region. Test identical
  content under two explicit profile descriptors; the second is a test-only
  contrast and is not a PAL support claim. Document unsupported/deferred/unknown
  behavior and the evidence that would promote each.

### Scheduling and observations
- **D-05:** Per-instance bounded queue ordered by `(tick, enqueue_sequence)`.
  Equal-tick FIFO is project policy, not bus arbitration evidence. No past events,
  silent drops, arithmetic wrap or unbounded zero-time loops. Reject impossible
  scheduling before mutation. Reserve completion capacity before accepting DMA.
  Queue/sequence exhaustion has an explicit tested postcondition.
- **D-06:** Absolute guest-time bounds are inclusive: dispatch all events through
  the bound and stop at exactly that tick, including idle time. Instruction/frame
  bounds stop immediately at their cumulative counter event; no remaining event
  is dispatched beyond that counter bound. A lower-than-current bound refuses
  unchanged; equal/current and halt-at-bound precedence must be explicit. An
  impossible counter bound reports quiescence without falsely claiming success.
  Test one-shot vs partitioned advancement and isolated vs interleaved instances.
- **D-07:** Canonical versioned state serialization hashes fixed-width explicit
  endian fields, profile/content identities, counters, RAM, registers and queued
  request metadata in logical order. Hash no padding, pointers, build identity,
  unused queue slots or trace configuration. Audit every future-affecting field.
  Reuse one authored SHA-256 implementation inside the linted core boundary.
- **D-08:** Trace device-boundary copies into a bounded per-instance observer
  ring; drain outside advancement. Overflow is visible as an observer drop count
  and cannot block or change machine execution. Trace-on/off and overflow runs
  must finish with the same state digest. No host sink calls inside handlers.

### Physical bus and DMA evidence
- **D-09:** Use the physical map, MI encodings and SP fields in DECISION-B with
  their source locators and pins. Bounds-check 4 MiB RDRAM and 4 KiB SP banks;
  explicit unsupported/unmapped results do not alias RAM. MI pending and masks
  are separate; mask writes never acknowledge pending. Paired mask commands use
  documented set-wins policy. No SP interrupt from DMA completion alone.
- **D-10:** PI/SP DMA stages an immutable descriptor and transfers bytes only at
  scheduled completion, not request time. Read source bytes at completion under
  this named atomic-transfer model; do not imply progressive hardware visibility.
  SP has active+pending slots, rounded 8-byte line lengths/count/skip, selected
  bank wrap, and explicit full-queue behavior. PI permits one active transfer,
  validates the whole ROM/RAM range, sets PI pending on completion, and handles
  reset/cancellation without stale writes or duplicate completion. Specify register
  latch/readback, queue promotion, same-tick order, busy/error/IRQ and invalid
  request postconditions in the executable contract.
- **D-11:** Unknown PI duration/arbitration and SP edge semantics stay named
  profile policies: candidate PI duration `1 + N/2` model ticks; SP duration
  `12 + payload/8` RSP cycles with an explicit integer tick conversion. Validate
  these against the sources and preserve lower evidence labels. PI `N-1`, SP
  bank-wrap/full-queue, and simultaneous MI clear/set are documented inferences
  or model choices where primary evidence is incomplete. No cycle-accuracy claim.
  Native expectations cite a source or policy version, independently derive
  exact bytes/deadlines, and cover boundary, overlap, queue-full and cancellation.

### Admission safety and verification
- **D-12:** Admit raw spans only, initially 4096..67108864 bytes, word aligned in
  length, with one of the three known dump-order signatures. Normalize to big
  endian. Parse a fixed 64-byte header using explicit bounded byte loads. Retain
  raw fixed-width title/game-code/version metadata; unknown metadata is allowed.
  Reject null/short/oversized/non-word/unknown-order input before commit; never
  unpack archives or infer profile, save device, rights or CIC from a title.
- **D-13:** Fuzz the production admission/parser/normalization code under ASan
  and UBSan, using authored runtime-generated bytes, every short-prefix and
  field boundary and all three valid byte orders. Use a pinned seed and bounded
  deterministic campaign, record reached branches/fields plus case inventory,
  and prove sanitizers catch a seeded fault in a separate control. Test real
  admission atomicity and allocator denial, not just a four-byte stub. No clean
  campaign is proof of memory safety; do not reinterpret the Phase 1 language
  experiment or silently broaden its registered switching criteria.
- **D-14:** Extend the existing gate with focused CTest component and seam
  evidence; keep one public gate command and the existing two hosted lanes.
  Run sanitizer evidence locally with the existing asan preset; add recurring
  CI cost only for a demonstrated distinct failure class. Inspect CI critical
  path after any push, retain exact tested source identity, run independent code
  review/security/goal verification and fix findings before phase completion.

### The agent's Discretion
Exact provisional function/file names, bounded queue capacity, trace record
shape, canonical binary encoding, allocation alignment mechanism and test
organization are implementation choices. Keep plans small enough for independent
execution, with dependency order and shared-file ownership explicit. Resolve
routine reversibility and evidence choices without additional owner prompts.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `core/include/tendies64/t64_facade.h`: provisional synthetic runner seam.
- `tools/common/t64_sha256.*`: authored streaming SHA-256 with vector tests;
  relocate or compile the exact implementation under core lint, without pulling
  host JSON/file helpers into the core.
- CMake gate/asan presets, `scripts/gate.sh`, core source/object lint, rights/PII
  and provenance gates, canonical result checker, supervisor negative controls.

### Established Patterns
- Strict C17 warnings, fixed-width/endian data, no mutable core globals.
- Project-authored text test inputs; no downloaded ROM/boot bytes or manuals in git.
- Unit/spec-derived/synthetic evidence is not hardware-derived evidence.

### Integration Points
- Replace the behavioral stub through `core/CMakeLists.txt`; keep synthetic facade
  users compatible. Phase 3 supplies real boot/CPU/ISViewer runner integration.
- `02-DECISION-A.md`, `02-DECISION-B.md`, `02-DECISION-C.md` retain alternatives,
  rationale, source pins, failure modes and reversal evidence.

</code_context>

<specifics>
## Specific Ideas

The owner's Claude-to-Codex handoff reiterates the real parser sanitizer fuzzing
requirement and existing stops; it does not reopen C17 or authorize unreviewed
fixtures. Source-evidence gaps do not become support claims. The UI keyword gate
matched "Component tests"; this phase has no frontend or human interaction UI.

</specifics>

<deferred>
## Deferred Ideas

CPU/RSP instructions, boot/IPL3 selection, real VI/audio, arbitration and
cycle-exact timing, state export/replay and ABI freeze remain in their roadmap
phases. Rights decisions, releases/versioning, branding, copy-allowlist expansion
and physical hardware stay under existing owner stops. Branch protection and
the narrow public-history scanner waiver are unchanged repository-policy items.

</deferred>
