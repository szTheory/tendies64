# Phase 4 synthesized recommendation — corrected

This is an agent decision under AUTONOMY, not an owner decision or an
implementation result. The corrections in 04-ADVERSARIAL.md are incorporated
in 04-CONTEXT.md, which is the authoritative planning contract. The phase boundary and all four roadmap success criteria
remain unchanged. The three decision reports are its evidence inputs.

1. Keep one canonical portable interpreter and shared scheduler, memory and
   observation implementation. Retain the qualified warm-v1 identity/policies
   and its historical state-v8 vectors. Introduce a separately named CPU profile
   with explicit descriptor/model/state version changes for architectural
   exceptions, CP0, translation, functional caches and COP1. Policy dispatch
   must not become a second interpreter or duplicate machine.
2. Architectural guest exceptions update precise CP0/EPC/BD state and resume
   handlers; model-unsupported stops and host failures stay distinct. Validate
   before architectural commit, including fetch/store/unaligned and delay-slot
   effects. Full MIPS III integer/branch/CP0/TLB/unaligned/LLSC/COP1 scope remains.
3. Implement all 32 TLB entries, legal segment/privilege modes, ASID/global and
   paired-page rules, exact refill/invalid/modified/XTLB/address distinctions,
   and per-register CP0 fields/side effects. Derive masks from fields and test
   them independently rather than trusting a copied aggregate constant.
4. Count must free-run in selected model guest-clock units independently of
   retirement, with a serialized clock phase and exact Compare equality/clear
   behavior. Define units and rate in the new profile before implementation;
   instruction cost tables and pipeline accuracy remain Phase 6/later work.
5. Implement functional split I/D caches, including line data/tags/dirty state,
   writeback, cacheable/uncached alias behavior and meaningful CACHE operations.
   Define external-agent coherence and reservation invalidation as explicit
   model policies wherever the primary evidence is incomplete. Do not turn
   unsupported/undefined operations into silent no-ops.
6. Preserve the manual's uncertainty: the rendered p.181 table/prose conflict
   for BEV=1 general vector, invalid PageMask encodings, SC link clearing and
   invalidation granule, and DMA coherence are not hardware qualification.
   Proposed deterministic policies and evidence that would reverse them are in
   04-DECISION-CPU.md; scrutinize them before locking context.
7. Prefer the exact Berkeley SoftFloat 3e commit and minimal generic binary32/
   binary64 closure. Before copying, complete the per-file BSD-3-Clause audit,
   full-commit provenance, notices and dependency closure. Thread explicit
   operation context through mutable rounding/tininess/flags; neither ambient
   globals nor TLS is acceptable. Do not adopt host FP or a global-state wrapper.
8. The Tendies64 COP1 layer owns FPR/FR mapping, FCR31 causes/enables/flags,
   unmaskable E, FS choices, destination suppression and architectural exceptions.
   Generic IEEE arithmetic does not establish VR4300 behavior. Derive expected
   vectors independently for all four rounding modes and both formats.
9. Preserve SC4: locally build and run a qualified pinned nemu64-test CPU variant
   with exact project-owned test/value/group inventories and explicit per-group
   status. Native vectors alone do not replace that acceptance criterion; an
   aggregate upstream score is also insufficient. Separate failures, skipped
   non-CPU tests, blocked/incomplete execution and harness errors.
10. Keep D2: no external ROM, captured output or derived diagnostic results in
    commits/public CI/uploads. Qualify a local source overlay that removes the
    unclear font/framebuffer assets, explicitly selects the cleared IPL3 and
    instruments exact test status without altering tested CPU semantics. Audit
    tool/provider/locks/checksums/build hooks and asset composition before any
    tool installation or guest run. Do not infer binary rights from root MIT.
11. Use a separate local host adapter over the same canonical core, reusing
    verified descriptors and bounded execution. The public seven-fixture hello
    registry/collector stays scoped to its cleared evidence. No new CI lane,
    cache, timeout or paid service is justified. No Rust in the core build.
12. Sequence vertical slices with behavior tests: profile/state and exception
    seam; integer/control and precise faults; CP0/timer; TLB; unaligned/LLSC;
    cache visibility; qualified integer arithmetic/COP1; local fixture adapter
    and exact inventory; cross-instance/determinism and phase verification.
    Qualification tasks must precede use, and each executable slice supplies
    meaningful independent positive/edge/negative tests immediately.

## Concrete issues for the adversarial check

- CPU decision's Status mask `0xFF5FFFFF` appears inconsistent with its stated
  requirement to zero reserved bit 19; derive the actual field mask from the
  complete manual page, not this recommendation.
- Warm-v1 compatibility must not force a forked machine implementation or
  disguise a new behavior under an old identity. New-profile boot/reset/cache
  state must actually permit the planned guest entry path.
- The Count timebase/phase must be unambiguous despite the old profile's model
  CPU:RSP ratio and lack of T1 costs; no hardware rate claim may be inferred.
- An explicit-context SoftFloat closure, independent expected values and the
  local-only external suite must be feasible without weakening the copy policy,
  scope, inventory requirements or rights stop.
- An upstream constructor count is not a runtime value denominator. Every
  local CPU group must have a complete exact expected inventory before a pass.

No source, copied helper, dependency or diagnostic ROM is authorized by this
document beyond standing project policy. Concrete unqualified rights or supply
chain inputs stop their affected use; routine reversible work proceeds.

## Adversarial disposition

All four required corrections are incorporated into the final context: reserved
Status fields/RP, nested EPC+BD, clock/write/interrupt order with exception
rescheduling, and a separately prepared CPU-profile seed and identity. No
recommendation depends on owner risk acceptance or a weakened success criterion.
