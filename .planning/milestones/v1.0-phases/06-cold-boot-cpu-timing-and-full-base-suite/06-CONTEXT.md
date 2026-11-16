# Phase 6: Cold Boot, CPU Timing and Full Base Suite — Context

**Gathered:** 2026-11-15
**Status:** Ready for implementation research and planning

<domain>
## Phase boundary

Deliver BUS-03, CPU-08 and VER-02: authored hello through RDRAM Tier A preset
and Tier B register initialization with the same output; a versioned T1
instruction-class CPU cost model; the complete source-derived BASE inventory
classified per group/lane; and evidence-scoped first-profile claims. Preserve
the four roadmap success criteria. Video/RDP implementation, 8 MiB expansion,
T2 pipeline accuracy, physical measurements and release publication are outside
this phase. A device/rights-blocked member remains in the inventory with its
own reason; it does not authorize implementing Phase 7 here.
</domain>

<decisions>
## Decisions under delegated autonomy

### Profiles, boot and RDRAM

- **D-01 — Two new explicit profiles.** Add distinct Tier A preset and Tier B
  register-protocol profile IDs, both using the new T1 policy and the already
  approved compat-v2 CPU semantics plus functional RSP capability. Keep all
  five existing profiles, descriptors, state bytes, refusal contracts and
  qualified fixtures frozen. Select capabilities by explicit profile/policy
  predicates, never numeric model/state ordering. Share one machine/core and
  primitives. The two new profiles can share a new serialization layout while
  their profile hashes distinguish their boot policies. Exact names and
  version numbers are implementation discretion; do not create an unnecessary
  third timing-only profile.
- **D-02 — Reuse the cleared boot bytes first.** The pinned open IPL3 source
  describes a cold branch selected by zero `RI_SELECT` and a preset branch
  selected by nonzero `RI_SELECT`. This is a source inference until the exact
  admitted binary executes that branch. Reuse the cleared IPL3 and authored
  hello before proposing another boot variant. Define deterministic HLE-PIF
  seed/reset values as software policy, not original IPL2 or physical power-on
  observations. The first bounded Tier B startup must record its first reached
  RI/RDRAM operation and stop before a long native device expansion. Create a
  new boot variant only if that actual experiment contradicts the reuse path,
  with affected provenance/rights/composition checks before use.
- **D-03 — Protocol model, not analog calibration.** Implement source-reached
  RI/RDRAM delay, device selection/ID, broadcast, MI repeat mode, calibration
  readback and capacity transitions as a bounded deterministic state machine.
  Derive the minimum logical device slots from the actual cold path; capacity
  alone does not establish physical chip topology. Numeric readbacks must be
  supported by primary definitions or explicitly versioned software-model
  rules derived from the permitted initialization algorithm. No copied
  emulator constants, guessed analog measurement, PC/ROM/hash-specific shortcut
  or special path that merely forces hello success. Test protocol transitions
  with independent authored sequences beyond the one boot trace. Serialize
  every future-affecting mode, device, calibration and pending state in the new
  state domain, preserving old encodings.
- **D-04 — Separate installed backing from access policy.** Keep 4 MiB and no
  Expansion Pak. Never alias uninstalled memory into installed RAM or broaden
  DMA spans. Preserve the selected compat-v2 CPU data-read policy for its
  unpopulated windows, including its exclusions, rather than replacing it with
  a blanket new refusal. Instruction fetch, stores, DMA, modeled absent probes
  and RI/RDRAM register replies retain their distinct owners and complete-span
  checks. New protocol/absent-device rules need exact address/width/effect
  contracts. 8 MiB support remains EXP-01.

### Instruction-granular time

- **D-05 — A small real T1 table.** Do not relabel the existing fixed one-tick
  schedule as T1. Use a named, versioned static instruction-class table on the
  two new profiles. Start with one ordinary issue interval and narrowly
  documented integer multiply/divide stall classes. Verify the actual cited
  NEC Table 3-12 before freezing literal weights; the proposed base-plus-stall
  mapping is explicit model policy rather than an assertion of hardware
  latency. Keep the architectural delay-slot instruction separate without
  charging it twice. No dynamic cache/bus timing workstream or cycle pipeline
  is added. Additional classes require concrete evidence and a versioned policy.
- **D-06 — Reuse timer and compatibility ownership.** Retain the selected
  model time unit/CPU:RSP ratio, guest-time Count synchronization and half-phase,
  Compare equality/clearing and interrupt sampling order. Random steps once
  per retirement; TLBWR consumes its pre-step selection. Inherit compat-v2's
  approved Wired/Random high-bit behavior, including its modulo64 case, rather
  than silently substituting the strict family's range. Pin actual guest
  Wired/Random/TLBWR ordering with independent assertions. Guest exceptions
  and sampled interrupts consume the selected base boundary without retirement
  or Random stepping; Count follows elapsed time. Preserve unsupported/refused
  effects. Preflight cost arithmetic, successor event/deadline and resource
  failures before architectural mutation. Check idle gaps, equal-tick device
  FIFO, DMA/RSP interleave, advance partitions and separate instances.
- **D-07 — Explicit timing limits.** Profile identity includes T1 and its table
  version; future table changes require another policy/profile version. Bind
  any future-affecting pending timing state in the digest, reusing the queued
  CPU event tick where sufficient. T2 remains deferred pending a named hardware
  target and primary or separately authorized independent evidence for pipeline,
  hazards, cache/bus stalls, exception stages and CPU/RSP/device arbitration.
  Software suite results and emulator agreement do not provide that evidence.

### Complete membership and qualified execution

- **D-08 — Full source denominator and one target.** Derive the actual default
  features, BASE-level dispatch, aliases, source ordinals and value semantics
  before sealing. The existing CPU-only projection omits non-CPU BASE groups;
  it is not the Phase 6 denominator. Empty value lists execute once and repeated
  display names retain distinct source identities. Keep exact counts and all
  external identities/results private. Use the new Tier B/T1 profile as the
  complete BASE target, with Tier A as the separate authored boot comparison.
  Do not pool historical warm-profile results into the new target's gate.
  Reuse input images only when their exact audited closure remains valid;
  historical results retain their original profile/build identity.
- **D-09 — One C aggregation authority.** Prefer a phase-owned C inventory and
  aggregate that authenticates original member records and exact membership
  against an independently qualified pinned-source seal. Preserve existing
  CPU/RSP schema/default contracts. Python may derive the source projection,
  prepare commands and retain outputs, but must not become a second completeness
  calculator. Join component records only if their identities/outputs can be
  verified losslessly for the selected target. If that is impossible, use a
  separate phase-wide C member protocol over shared primitives; do not weaken
  legacy parsers or summarize away missing members. A blocked asset/device gets
  its own identity-bound reason and remains in the denominator. Do not compile
  an uncleared font/bitmap/framebuffer path even when it would not execute.
- **D-10 — Immutable contexts without per-fix code versions.** Introduce the
  minimum phase-owned qualification/context seam needed for reviewed source,
  independent membership, exact profile/build/compiler policy, host verifier
  content, source/tool/lock/boot/image closure and preserved original C outputs.
  Routine source/harness corrections create fresh immutable context records;
  protocol versions change when their meaning changes, not on every fix.
  Context data cannot authorize arbitrary source deltas, commands, profiles or
  assets by merely repeating hashes. Require an independent checked source
  authority and actual tested-artifact binding. Hash executables before use;
  archive exact executables and binding together before every later rebuild.
  Preserve failed contexts. Full independent pre-use verification must pass
  before creating a guest run directory. Accepted/rejected authored fixtures
  invoke that complete entry and normal dispatch, not helper validators only.
- **D-11 — Early bounded use, then cohesive evidence.** After concrete pre-use
  source/tool/asset checks and necessary owning native controls, perform the
  small actual authored cold-path startup and an early qualified real member
  before long implementation expansion. Missing behavior observed there guides
  the next slice. Keep headless/background operation, exact CPU/RSP case limits,
  capture bounds, event budgets and specific no-retry instructions. Do not widen
  a timeout or renew a window. Diagnose the cause before a justified bounded
  retest. Serialize wall-capped actual diagnostics after long sanitizer/fuzz
  work. Build all owning normal/instrumented targets before focused checks;
  use canonical gates at coherent source boundaries and final qualification.
  Preserve exact inherited inventories plus real additions, admission fuzz and
  its negative control. Separate source annotation from content/compiler-policy
  build identity and reuse unaffected semantic evidence honestly. Parent owns
  routine summary/state closeout.

### Lanes, claims and stops

- **D-12 — Explicit lanes and bounded claims.** Classify the full BASE set by
  group/lane with independent completeness. Timing, cycle and CP0-hazard evidence
  is informational and separate; experimental-only selections stay out of the
  required gate. Stress has a separate scheduled lane, using the existing
  authored lane where applicable and an explicitly local-only qualified path
  for external selections. Do not call a lane executed when only a definition
  exists, or call an authored scheduled control an external-suite run. Retain
  individual rights/device/resource blockers and report their limits. No new
  self-hosted runner, recurring OS service, dependency, cache, concurrency or
  paid service is inferred. The existing two hosted gates use cleared authored
  inputs and permanent hello smoke; external hosted use/uploads remain disabled
  under D2. Build the first-profile support statement from the claims template,
  binding each reported result to exact profile/build/evidence tier/fixture
  manifest and oracle. Public claims cover cleared authored evidence; private
  diagnostic records stay private. D6 still blocks release publication only.

Routine reversible seams, exact profile names, state field layout, CLI spelling
and slice sizes are delegated. These are agent recommendations selected under
AUTONOMY, not new owner risk acceptance or hardware validation. Existing C17,
Apache-2.0/public repository, working name and approved CPU compatibility
decisions are carried forward without another approval.
</decisions>

<canonical_refs>
## Canonical references

- `AGENTS.md`, `.planning/AUTONOMY.md`, `.planning/PROJECT.md`,
  `.planning/REQUIREMENTS.md`, `.planning/ROADMAP.md` — authority and locked scope.
- `.planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-RDRAM-BOOT-DECISION.md`
- `.planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-CPU-TIMING-DECISION.md`
- `.planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-SUITE-EVIDENCE-DECISION.md`
  — bounded fan-out findings; use their primary-source citations and limits.
- `docs/spec/boot-limits.md`, `docs/spec/machine-contract.md`,
  `fixtures/boot-contract.json`, `scripts/build-boot-fixtures.sh`,
  `fixtures/hello/hello.S`, `fixtures/manifest.json` — existing cleared boot path.
- `.planning/phases/03-first-headless-rom-through-open-ipl3/03-PRIMARY-EVIDENCE-INDEX.md`
  — pinned primary documents outside the repository.
- `docs/adr/0003-cpu-timing-tiers.md`, `docs/spec/cpu-profile.md`,
  `docs/spec/cpu-cp0.md`, `docs/spec/cpu-compatibility.md`,
  `docs/spec/cpu-memory.md` — existing clock, compatibility and memory policies.
- `.planning/phases/04-complete-vr4300-cpu/04-20-CONTEXT.md` — approved additional
  CPU software policies; do not fall back to the earlier strict-only model.
- `docs/spec/cpu-diagnostic.md`, `docs/spec/rsp-diagnostic.md`,
  `scripts/verify-cpu-diagnostic-qualification.py`,
  `scripts/verify-rsp-diagnostic-qualification.py`,
  `scripts/check-rsp-evidence.py` — source/tool/asset and original-result trust seams.
- `.planning/phases/05-rsp-low-level-emulation/05-10-SUMMARY.md`,
  `docs/evidence/phase05-native-rsp.md`, `docs/evidence/phase05-review.md`,
  `docs/evidence/phase05-security.md`, `docs/ci-reviews/LOG.md` — actual prior
  evidence, setup failure, retention and efficiency lessons.
- `docs/claims/TEMPLATE.md`, `docs/policy/rights-and-fixtures.md`,
  `docs/policy/copy-provenance.md` — claim, rights and copied-input boundaries.
</canonical_refs>

<code_context>
## Existing code insights

- `core/t64_machine.c`, `core/t64_internal.h`, `core/t64_digest.c` own explicit
  profile/reset and serialized future state. Extend new profiles only.
- `core/t64_bus.c`, `core/t64_warm_devices.c`, `core/t64_mi.c` are the existing
  backing/probe/RI-select/MI integration seams; no RDRAM protocol is implemented.
- `core/t64_cpu.c`, `core/t64_exception.c`, `core/t64_cp0.c` and
  `core/t64_scheduler.c` own the fixed next attempt, non-retiring handler,
  Count/Compare/Random and deterministic event-order seams for T1.
- `core/tests/test_cpu_time.c`, `core/tests/test_cpu_cp0.c`,
  `core/tests/test_profile.c`, `core/tests/test_warm_devices.c`,
  `core/tests/test_boot_entry.c` are the closest native owners.
- Existing C diagnostic primitives, immutable descriptors, source qualifiers,
  fixture builders and two authored-only hosted jobs are reusable. The full
  BASE join is new; the CPU-only denominator is not reusable as a whole-base seal.
</code_context>

<specifics>
## Specific delivery focus

Make the first exact cold branch and complete pre-use entry run early. Preserve
the actual result instead of broadening wrappers or restarting passing audits.
Independent raw instruction/primary-table anchors must precede timing expected
values; field-helper output is not its own oracle. Tier A/B output bytes should
match; their different profile/state digests need not match. Determinism is
checked within each named profile and across advance partitions/instances.
</specifics>

<deferred>
## Deferred work

T2 pipeline/hazard accuracy, dynamic cache/bus latency, Expansion Pak support,
RDP execution, physical measurements, external public CI rights and release
channel/versioning are not silently added. Record concrete evidence that would
promote them; preserve their existing responsible owner and stops.
</deferred>
