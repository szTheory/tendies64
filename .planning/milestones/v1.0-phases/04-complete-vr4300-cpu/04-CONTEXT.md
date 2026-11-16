# Phase 4: Complete VR4300 CPU - Context

**Gathered:** 2026-11-11
**Status:** Ready for planning

<domain>
## Phase Boundary

Complete the portable MIPS III integer, precise exceptions/CP0, TLB, unaligned
access/LLSC, functional cache and integer COP1 path. Verify independent native
edge/negative vectors and a pinned local CPU diagnostic with exact per-group
status. All four roadmap criteria and ten requirements remain binding.
Timing tables, pipelines, RSP execution, video and cold-boot expansion remain
their later phases. No public diagnostic-ROM input/output is authorized.
</domain>

<decisions>
## Implementation Decisions

### One machine and explicit profile policies

- **D-01:** Keep one interpreter, scheduler, bus and machine. Preserve the
  qualified `ntsc-cart-4m-openipl3-warm-v1` semantics, descriptor v2/state v8
  bytes and seven hello fixtures. Add `ntsc-cart-4m-openipl3-vr4300-v1` with
  descriptor/model v3 and state v9 for architectural behavior. Share primitives;
  dispatch named policy choices rather than fork a machine or interpreter.
- **D-02:** The new profile has a separately reviewed executable warm seed:
  Config fixed/read-only fields and big-endian state, PRId, Status, Random/Wired,
  Count/Compare phase, invalid TLB/cache lines, IPL3/image entry and boot-policy
  digest. Derive exact fields from primary references and independent seed
  tests before first admission. Zero CP0 is not an architectural reset claim.
  Profile v3 serialization includes all boot metadata; no v2-only fallthrough.
- **D-03:** Architectural faults update guest CP0 and resume handlers; model
  unsupported and host errors remain separate. Fetch/access validation precedes
  architectural commit. Old EXL=0 captures EPC and BD together; old EXL=1
  preserves both while updating applicable cause/address context. Annulled
  likely slots cannot execute or fault. Non-retiring exceptions consume a CPU
  attempt and schedule its next boundary; handler progress cannot depend on
  successful retirement. New-profile BREAK is architectural; warm-v1 keeps its
  already qualified fixture-bound debug completion policy.

### Translation, registers, time and caches

- **D-04:** Implement 32 paired TLB entries, legal 32/64-bit segments and
  privilege/ASID/global rules, page sizes, V/D, TLBP/R/ WI/WR, Wired/Random and
  distinct refill/invalid/modified/XTLB/address exceptions. Centralize CP0
  field masks/side effects. Status's defined writable mask is `0xFF57FFFF`
  before RP selection; fix unmodeled RP27 to zero for this profile, yielding
  `0xF757FFFF`, without claiming NUS silicon qualification. Preserve TS and zero
  RFU fields. Test fields independently, including transfer-width semantics.
- **D-05:** One new-profile guest tick is one model RSP-cycle unit, with selected
  model CPU:RSP=1:1. Count advances every two ticks independently of retirement;
  serialize phase and timer state. This is a deterministic model rate, not
  measured hardware timing. At each boundary: synchronize clock and latch any
  newly reached equality/IP7, then sample eligible CPU interrupts, then execute
  and commit that instruction's CP0 effects. Compare writes clear IP7 at their
  commit; Count writes preserve phase, reschedule equality and latch equality
  if the written Count equals Compare. Do not re-latch merely for resampling an
  unchanged equal value after a Compare write. Observe new pending interrupts
  at the next eligible CPU boundary. Preserve deterministic FIFO device order;
  derive timer deadlines over idle jumps/wrap without per-tick host loops.
  Test same-tick writes, interrupt masks, delay slots, faults and partitions.
- **D-06:** Implement functional split I/D cache line data, physical tags,
  dirty/writeback state, cached/uncached aliases and documented primary CACHE
  operations. No cache timing claim. New-profile DMA updates backing memory
  without snooping valid cache lines, as explicit model policy. Track linked
  physical access and width; completed SC/SCD decisions consume the link,
  different-address/width requests fail without store, ERET and overlapping
  external writes/targeted invalidations clear it. Higher-priority faults
  resolve before SC writes or result. Exact invalidation granule and SC rules
  are policy where primary evidence is incomplete.
- **D-07:** Retain the genuine p.181 table/prose vector conflict. Select
  table-derived bootstrap general vector `0xBFC00380` with explicit uncertainty
  and focused tests. Invalid PageMask and manual-undefined cache combinations
  stop as unsupported before partial mutation. Do not present these choices,
  Count ratio or DMA coherence as hardware facts. Record evidence that would
  reverse/version each choice. Every future-affecting field enters v9 digest.

### COP1 and permitted copied arithmetic

- **D-08:** Prefer a minimal generic binary32/binary64 Berkeley SoftFloat 3e
  closure at `f74b1e48110ac3a27dd49b787d164e55e42d81d1`. Before copying, audit
  every selected file/notice, dependency closure, full-SHA provenance and
  maintenance owner under existing BSD-3-Clause rules. The official archive
  hash is corroboration, not a substitute for the required commit pin.
  No license expansion, target-specific specialization or unrelated format.
- **D-09:** Adapt mutable rounding/tininess/flags to explicit operation context;
  no ambient globals or TLS. COP1 owns FPR/FR, FCR31 causes/enables/sticky flags,
  unmaskable E, FS, destination suppression and guest exceptions. SoftFloat
  supplies raw integer arithmetic only. Include the closure in build identity,
  production source/object lint and inter-instance tests. No host FP or lint
  exception. Independent bit/rational vectors cover both formats/four rounding
  modes and VR4300 edge/trap cases; the implementation cannot generate its own
  expected values. An authored integer fallback is possible only if the audited
  closure proves unsuitable, with equivalent scope and independent checks.

### Private external diagnostic and evidence

- **D-10:** Preserve SC4 and D2. Pin nemu64-test to
  `9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c`; qualify a local CPU-only source
  overlay with project-owned exact names, value denominators and group mapping.
  Account for every required CPU registration/value; neither the illustrative
  README count nor 758 observed constructors is a runtime denominator. Separate
  timing/cycle/hazard/quirk and omitted non-CPU lanes. Executed failures may be
  reported; missing/reduced/unreached members cannot satisfy completeness.
- **D-11:** Before any installation/build/run, audit exact Rust/channel/components,
  selected package/source/locks/checksums/transitives/build hooks and final asset
  composition. Remove the unclear Geneva/font/framebuffer path; explicitly
  select and qualify the already cleared IPL3's format/startup. Root MIT does
  not clear composed ROM assets. An unresolved input blocks that use; it never
  creates inferred risk acceptance. Record audits before use, not afterward.
- **D-12:** Use a separate local host adapter over the same canonical core and
  new CPU profile, sharing immutable descriptors and bounded execution seams.
  Bind source/overlay/inventory/tool/profile/ROM/result identities. Keep ROMs,
  captures and derived diagnostic reports private/ignored; no public CI,
  uploads or committed derived results. Keep public hello allowlist/collector
  fail-closed. Native authored CPU vectors remain public independent evidence,
  and cannot substitute for the required local diagnostic.
- **D-13:** Keep the existing two hosted jobs, limits and flat core dependency
  tree. No new job/cache/paid service or Rust dependency in the core. Add tests
  with each behavioral slice and protect exact inventories. Reuse passing
  evidence after metadata-only changes; parent owns routine summary/state
  closeout. Full applicable gates, sanitizers, reviews/fixes and bounded local
  checks remain authorized, headless/background by default.

### Agent discretion

Routine reversible file/module seams, private adapter CLI and slice sizes are
delegated. Do not weaken acceptance, copy policy, rights, identity or explicit
stops. No human approval/risk acceptance is recorded by these agent decisions.
</decisions>

<canonical_refs>
## Canonical References

- `.planning/ROADMAP.md` Phase 4 and `.planning/REQUIREMENTS.md` CPU/VER — scope.
- `.planning/AUTONOMY.md` and `AGENTS.md` — authority, explicit stops, efficiency.
- `.planning/phases/04-complete-vr4300-cpu/04-DECISION-CPU.md` — pinned manual sections and policy/evidence ledger.
- `.planning/phases/04-complete-vr4300-cpu/04-DECISION-FPU.md` — exact SoftFloat provenance and COP1 primary references.
- `.planning/phases/04-complete-vr4300-cpu/04-DECISION-TESTS.md` — local suite, rights/toolchain and inventory distinctions.
- `.planning/phases/04-complete-vr4300-cpu/04-ADVERSARIAL.md` — four corrected contradictions; final context resolves them.
- `.planning/phases/03-first-headless-rom-through-open-ipl3/03-PRIMARY-EVIDENCE-INDEX.md` — manual identity/hash; no raw documents committed.
- `docs/spec/machine-contract.md`, `docs/spec/boot-limits.md`, `docs/spec/result-protocol.md` — existing compatibility and evidence boundaries.
- `docs/adr/0003-cpu-timing-tiers.md`, `docs/adr/0007-copy-and-reading-policy.md`, `docs/policy/copy-provenance.md`, `provenance/copies.json` — timing, reading and copy policy.
</canonical_refs>

<code_context>
## Existing Code Insights

Reuse `core/t64_cpu.c`, `t64_scheduler.c`, `t64_bus.c`, `t64_digest.c` and profile
preparation in `t64_machine.c`; add narrow internal seams for MMU/cache/COP1
rather than host callbacks. Existing source/object lint, bounded admission,
verified-input snapshots, canonical JSON/result helpers, independent tests and
exact inventory controls stay active. Extend `core/CMakeLists.txt` production
identity/lint coverage for every copied helper, not only top-level files.
</code_context>

<specifics>
## Specific Ideas

The decision-focused three-way fan-out was synthesized and adversarially checked.
All four requested corrections are incorporated above. This completes the
CPU-area primary-document entry check for planning; remaining per-copy, seed,
toolchain and asset qualification must precede their concrete use. No hardware,
package, ROM or implementation check is represented as having run.
</specifics>

<deferred>
## Deferred Ideas

JIT/block compilation, host-FP fast paths, hardware timing/pipeline accuracy,
RSP/video/PIF breadth, cold reset and public external diagnostic publication.
</deferred>
