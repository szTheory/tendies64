# Phase 4 CPU decision: interpreter, exceptions, CP0, TLB, LL/SC, caches

**Status:** recommended contract for Phase 4 planning. This is a design decision,
not implementation or hardware-qualification evidence.

**Scope:** CPU-01 through CPU-06 and CPU-09. COP1 arithmetic remains in its
separate implementation work. This decision covers the exception, CP0, memory
translation, cache, and deterministic-state boundaries those instructions
depend on.

**Primary source checked:** NEC, *VR4300/VR4305/VR4310 64-Bit Microprocessor
User's Manual*, U10504EJ7V0UMJ1, 7th edition, August 2000, SHA-256
`b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`,
[PDF](https://hack64.net/docs/VR43XX.pdf). The downloaded PDF's hash matches
the Phase 3 [primary evidence index](../03-first-headless-rom-through-open-ipl3/03-PRIMARY-EVIDENCE-INDEX.md).
Printed and PDF page numbers match for the cited pages.

## Decision matrix

| Option | Pros | Cons | Complexity | Recommendation |
|--------|------|------|------------|----------------|
| Canonical CPU: one portable C interpreter, one instruction boundary at a time | One source of truth for integer, exception, TLB and cache behavior; precise guest state is inspectable; fits existing C core | Lower peak throughput than generated code; decode and dispatch cost | CPU decoder, memory path and state -- risk partial state updates before faults | Rec for the verified CPU profile; revisit acceleration only after a named benchmark shows the interpreter is limiting |
| Canonical CPU: interpreter plus JIT or block cache | Can raise throughput after the reference path exists | Duplicates architectural behavior; complicates self-modifying code, TLB/cache invalidation, precise exceptions and cross-compiler digests | New code generator, block invalidation and parity checks -- risk backend drift | Rec only if PERF-01 later establishes a material bottleneck and lockstep digests remain exact |
| Fault handling: architectural guest exceptions with CP0 entry and handler resume | Satisfies delay-slot, CP0 and TLB behavior; guests can repair faults and resume; expected guest faults remain guest results | Requires precise fault ordering and persistent exception state | CPU, bus, CP0 and scheduler boundary -- risk guest faults being mislabeled as host failures | Rec for the CPU-enabled profile |
| Fault handling: stop on first architectural fault | Simple diagnostic path; limits partial execution in the current subset | Cannot run exception handlers or satisfy CPU-02/03/04; conflates guest behavior with incomplete implementation | Few CPU paths -- risk a false CPU pass | Rec only for the preserved warm-v1 diagnostic policy or a separately classified implementation-unsupported stop |
| Address translation: 32-entry MIPS III TLB plus 32/64-bit segment checks and permissions | Covers refill, invalid, modified and XTLB paths; models ASID/global, page sizes, wired/random replacement and uncached aliases | Adds TLB and CP0 state; invalid mappings and overlaps need explicit handling | CPU memory access, CP0, TLB and digest -- risk address-mode and endian mistakes | Rec for the CPU-enabled profile |
| Address translation: retain the current direct KSEG translation | Keeps the Phase 3 warm path small and deterministic | Cannot satisfy CPU-04 or execute TLB-managed code; cannot distinguish TLB miss from invalid, modified or address error | Existing translation helper -- risk false support claims | Rec only for the named warm-v1 profile |
| CP0 writes: per-register defined masks and side effects | Preserves read-only and reserved fields; matches Compare, Cause, Status, TLB and cache tag behavior | Requires a centralized, auditable register contract | CP0 decoder and tests -- risk masks differ across transfer forms | Rec for the CPU-enabled profile |
| CP0 writes: store every MTC0 value as a raw 32-bit register | Easy to implement and inspect | Lets software change read-only state, accepts RFU bits, and misses Compare's timer-clear side effect | Small CPU change -- risk broad guest-visible errors | Rec only for temporary instruction prototypes, never for a support claim |
| Count: increment with each retired instruction | Matches the current Phase 3 implementation and is easy to test | Manual says Count free-runs at half PClock even when no instruction executes; fails during stalls, idle advance and other-device events | Small CPU helper -- risk confusing retirement count with guest time | Rec only for the old warm-v1 compatibility policy |
| Count: derive Count from guest time using a versioned rational half-PClock phase | Matches the manual's free-running rule; separates timer interrupts from instruction retirement and stop bounds | Needs a model clock ratio and phase state; the project's 1:1 CPU:RSP ratio is selected model data, not a measured NUS clock | Scheduler, timer state and digest -- risk implying hardware clock accuracy | Rec for the CPU profile, labeled as a deterministic model rate; preserve T1 instruction costs for Phase 6 |
| LL/SC: one per-machine link record with physical LL address, width, explicit invalidation and SC result | Represents LLAddr/LLbit and lets DMA/cache invalidation break the link; permits deterministic documented tests | Exact invalidation granule and successful-SC clearing are not fully stated by this manual | CPU load/store, cache and every RDRAM writer -- risk missing a writer or clearing too broadly | Rec for the CPU profile with the bounded policy below and an explicit unknown-evidence label |
| LL/SC: one unaddressed boolean LLbit | Mirrors the visible link bit with minimal state | Cannot reject or define a different SC address; cannot observe RCP writes or cache invalidation | CPU only -- risk false successes after external writes | Rec only for a short-lived bring-up stub, not the Phase 4 contract |
| Cache: functional split I/D caches with manual line geometry, write-back D-cache and documented CACHE operations; no timing model | Meets CPU-06 with guest-visible cache behavior; models I-cache staleness, D-cache dirtiness, tag operations and writeback | Adds line data, tags and invalidation paths; host DMA and aliases require careful interaction | CPU memory path, cache state, bus writers, digest and tests -- risk stale-line or lost-write bugs | Rec for the new CPU profile because Phase 4 explicitly requires a functional cache model |
| Cache: keep coherent-functional shared backing and treat CACHE as metadata/no-op | Preserves the already qualified warm behavior; small state and simple instruction fetch | Does not model the manual's write-back D-cache or distinct I/D visibility; cache op success is not cache fidelity | Existing cache observer state -- risk presenting a compatibility policy as VR4300 cache behavior | Rec only for the preserved warm-v1 profile |
| Identity: add a separate CPU profile and version its descriptor/state | Makes exception, timer, TLB, LL/SC and cache behavior explicit; leaves prior goldens interpretable | Requires profile selection and a new independent digest vector | Profile, model and state schema -- risk accidental old/new policy mixing | Rec for Phase 4: keep warm-v1 descriptor v2/state v8 and add a CPU profile with new descriptor/model/state versions (recommended v3/v3/v9) |
| Identity: broaden warm-v1 in place | Avoids adding a profile selector | Changes old guest behavior and digest meaning under the same identity; the debug BREAK becomes an exception and caches cease to be coherent-functional | Existing profile and historical fixtures -- risk invalidating Phase 3 evidence | Do not use for CPU support; the only valid use is an explicit migration that retires the old identity and requalifies every dependent claim |

## Recommended contract

### Execution and faults

Keep one portable C interpreter as the canonical implementation. Each instruction
attempt is an explicit boundary: fetch and decode, validate privilege/address
mode, perform translation and memory access, then commit register and PC effects.
The interpreter must not publish a destination register, store, link result or
retirement count before all higher-priority faults for that instruction have
been resolved. The current scheduled instruction-step policy may remain for
Phase 4, but it is not a CPU cost table or a hardware timing claim; CPU timing
T1 remains Phase 6 work under [ADR-0003](../../../docs/adr/0003-cpu-timing-tiers.md).

Distinguish three outcomes:

1. **Guest exception:** update architectural CP0 state, select the documented
   exception vector, clear the pending delay-slot execution state as required,
   and continue from the handler. The faulting instruction does not retire.
2. **Model unsupported:** stop at the instruction without fabricating a guest
   exception when the instruction encoding is architecturally valid but the
   selected profile deliberately omits a documented undefined/unsupported
   operation. Record PC/opcode and classify it separately from a guest verdict.
3. **Host/core error:** report internal invariant, scheduling/resource, or
   storage failure through the existing host error path. Do not write a guest
   Cause code for a host failure.

For an exception in a delay slot, record the preceding branch/jump PC in EPC and
set Cause.BD when old EXL is clear; otherwise record the faulting instruction PC and clear BD only when old EXL is clear. A
not-taken branch-likely annuls its delay-slot instruction, so that instruction
cannot fault or retire. An exception taken while EXL is clear records EPC and
sets EXL; nested exceptions preserve both the first EPC and its paired BD and use the common vector. See manual Figures 6-14/6-15, pp.201/203, as checked in 04-ADVERSARIAL.md.
Apply the manual's exception priority, including fetch faults before data-side
faults and TLB modification after invalid. The new CPU profile treats BREAK as
the architectural breakpoint exception. Preserve the current exact,
non-retiring BREAK completion policy only in warm-v1.

### CP0 and address translation

Use a per-register `cp0_write` path; do not treat CP0 as a raw 32-slot array
whose full values are writable. Key field rules from the manual are:

| Register/field | Contract for the CPU profile |
|---|---|
| Status | Writable defined fields, defined-field mask `0xFF57FFFF` before the RP policy; this phase fixes unmodeled RP27 to zero, yielding `0xF757FFFF`. TS bit 21 is read-only and reserved bits 23 and 19 read as zero. Apply IE/IM/EXL/ERL and KSU/KX/SX/UX effects to interrupt eligibility and address checks. |
| Cause | Software may write only IP1:IP0, mask `0x00000300`; exception code, BD, coprocessor number and device/timer pending bits remain hardware-owned. |
| Count / Compare | Both are 32-bit values. Compare write clears timer IP7. Count equality with Compare latches IP7; clearing it requires a later Compare write. |
| Index | TLBP owns probe-result P; only the low six index bits are software data and only bits 4:0 select one of 32 entries. Bit 5 is retained/ignored for TLB indexing. |
| Random / Wired | Random is hardware-stepped over Wired..31; writing Wired sets Random to 31. The five low bits select TLB entries; software-visible bit 5 is ignored for selection. |
| PageMask | Bits 24:13 are the mask (`0x01FFE000`). Accept the seven manual encodings for paired 4KB through 16MB pages; behavior for other encodings is explicitly undefined by the manual and must use the model-unsupported policy pending evidence. |
| EntryHi | Keep VPN2, region and ASID fields by active 32/64-bit mode; zero RFU fields. A 32-bit write mask is `0xFFFFE0FF`; 64-bit region/VPN2 handling follows the manual's separate layout. |
| EntryLo0/1 | PFN bits 25:6, C bits 5:3, D bit 2, V bit 1 and G bit 0; RFU bits read zero. Use the global match only when both entry halves carry G. |
| Context / XContext | CP0 software writes update only PTEBase (Context bits 31:23 / 64-bit bits 63:23; XContext bits 63:33); BadVPN2 and XContext region fields are hardware-owned. RFU bits read zero. |
| EPC / ErrorEPC | Writable exception-return addresses at the active address width; EPC is preserved on a nested exception while EXL is already set. |
| BadVAddr / PRId / CacheErr | Read-only. BadVAddr changes only on address/TLB exceptions; PRId is a named profile value; CacheErr reads zero for this processor. |
| Config | Defined writable fields are EP[27:24], BE[15], CU[3] and K0[2:0], mask `0x0F00800F`; preserve read-only implementation fields. EP and BE writes outside their manual initialization window are undefined. |
| LLAddr | Diagnostic physical address from LL/LLD: PA[31:4] in bits 27:0, upper four bits read zero. Keep the architectural link-valid bit separate from this diagnostic register. |
| WatchLo / WatchHi | WatchLo PAddr[31:3] plus R/W bits (mask `0xFFFFFFFB`); WatchHi's four compatibility address bits are readable though physical addresses above bit 31 are unsupported. |
| PErr / TagLo / TagHi | PErr diagnostic bits 7:0 are read/write; TagLo PTag[27:8]/PState[7:6] mask `0x0FFFFFC0`; TagHi has no implemented VR4300 physical-address bits and reads zero. |

The mask constants above are derived from the named manual fields; keep the
field-level definitions beside the implementation and tests so a future
reviewer can recompute them. MFC0 reads and MTC0 writes must apply the
instruction's width/sign-extension rules. Invalid or reserved instruction
encodings raise RI; they are not silently accepted because the host has a slot
for the register.

Model 32 TLB entries with PageMask, VPN2/region, ASID, both EntryLo halves,
valid, dirty, global and cache attribute. Translation returns physical address
and access attributes, not only an address. Check direct-mapped segments,
privilege, legal 32/64-bit address forms, alignment, permissions and TLB lookup
before touching memory. Raise distinct refill, invalid, modified and address
errors. Implement TLBP/TLBR/TLBWI/TLBWR, Random stepping and Wired exclusion
with software-visible CP0 effects. Multiple matching entries must not use
first-match-wins: set the documented TLB shutdown state and stop normal CPU
execution until reset, with TS read-only.

The new profile is still a prepared warm entry, not a claim about cold-reset
silicon. The new CPU profile needs its own reviewed executable prepared seed: explicit
big-endian Config and fixed/read-only implementation fields, PRId, Status,
Random/Wired and Count phase, with TLB/cache lines invalid. Do not reuse a zero
CP0 array as architectural initialization. The old zero-filled seed remains
only warm-v1 policy. Exact fields and independent seed vectors must be fixed
in the first profile slice before guest entry, with manual citations and a
versioned boot-policy digest.
After each architecturally retired instruction, step Random within
Wired..31; a TLBWR consumes the pre-step Random value. This fixes a stable
instruction-boundary policy while cycle-level CP0 hazards remain deferred.

Use the manual's vector offsets for TLB refill (0), XTLB refill (0x80) and
other exceptions (0x180), with BEV selecting the base. For the 32-bit BEV=1
general vector, the p.181 table's bootstrap base plus `0x180` gives
`0xBFC00380`, consistent with the 64-bit address, but the same page's prose
states `0x80000180`. This artifact chooses the table-derived `0xBFC00380` for
deterministic tests and marks the discrepancy unqualified. Do not hide it as a
settled NEC fact; reverse this policy only with an erratum or hardware-derived
vector. The 32-bit BEV=0 vectors are `0x80000000`, `0x80000080` and
`0x80000180`; 64-bit addresses are the manual's sign-extended equivalents.

### Count and deterministic guest time

The manual says Count advances continuously at half PClock whether or not the
CPU retires instructions, and Compare equality raises timer pending IP7. The
current implementation increments Count once per CPU retirement, which is a
Phase 3 software policy, not the documented timer rule.

For the new profile, derive Count from guest time with a rational phase
accumulator, independent of `retired_instructions`. On the current selected
1:1 CPU:RSP model ratio, implement the deterministic profile rate as one Count
increment per two model guest ticks. Carry phase across arbitrary advance
partitions, CPU stalls, other-device events and idle time. Schedule or otherwise
derive the exact Compare equality point so IP7 appears at the correct guest
tick, including wraparound. A Count write changes the base count without
rephasing the selected clock; a Compare write clears IP7. Add the rate and
phase policy to the profile/state digest.

This is a model-clock choice, not a claim that the current NUS-CPU-03 profile's
selected ratio equals physical PClock or establishes absolute VR4300 timing.
If hardware-derived clock data changes the model ratio, create a new profile
identity. Do not add instruction-class cycle costs or CP0 pipeline hazards in
Phase 4.

### LL/SC and caches

Store one canonical CPU link record in machine state: LLbit, LLAddr physical
address and linked access width. LL/LLD set it; SC/SCD translate and validate
the target before deciding success, store only on success, and write 1/0 to
`rt`. For deterministic handling of the manual's undefined different-address
case, fail SC/SCD with `rt=0` and no store when the physical address or width
does not match. Consume the link after every SC/SCD attempt as the profile's
one-shot conditional-store policy. Clear it on ERET, an accepted write to the
linked access bytes by another machine device, and a cache invalidation that
targets the linked D-cache line. These invalidation details are project policy:
the manual says another processor/device can break the link and cache
invalidation resets it, but does not fully specify the N64 device-write
granule. A hardware vector may refine the policy in a new profile version.

Implement functional primary I/D cache state for the new profile, not cycle
timing: direct-mapped 16KiB I-cache with 32-byte lines, virtual indexing and
physical tags; direct-mapped 8KiB D-cache with 16-byte lines, virtual indexing
and physical tags; write-back D-cache; cacheability from KSEG0 Config.K0 and TLB C bits. Fetches,
loads and stores consult their respective caches. Uncached accesses bypass
them. Implement documented primary CACHE operation combinations and tag
register effects. Manual-defined CACHE operations in uncached regions and
unlisted cache/operation combinations are undefined; report a profile
model-unsupported stop before mutation for those cases. Cache miss and
writeback costs, arbitration and instruction timing are outside Phase 4. Seed
line contents/tags invalid as a prepared-profile policy. For PI/SP DMA writes,
update backing RDRAM without snooping or invalidating existing I/D cache lines;
guest-visible stale lines therefore persist until a CPU cache operation or
replacement. Clear the LL link when such a write overlaps the linked access
bytes. This deterministic non-snooping rule is a profile choice, not a manual
claim about N64 DMA/cache coherence.

Keep `ntsc-cart-4m-openipl3-warm-v1` on its current coherent-functional cache,
exact BREAK stop, retirement-tick Count policy, profile descriptor v2 and
state v8 digest. Add a separately named CPU-enabled profile (suggested
`ntsc-cart-4m-openipl3-vr4300-v1`) with descriptor v3, machine model v3 and
state serialization v9. Its v9 digest includes all TLB and CP0 state, link
state, Count phase/timer state, cache line contents/tags/dirty bits, delay-slot
state and any pending event that can change future guest behavior. Keep the
existing v8 expected digest as a historical vector; do not reinterpret v8
bytes as a CPU-profile state.

## Evidence ledger

| Kind | Evidence or policy |
|---|---|
| Manual fact | NEC manual §5.3/§5.4: 32 TLB entries, paired even/odd mappings, 4KB–16MB PageMask encodings, ASID/global matching, and Wired/Random semantics (printed/PDF pp.141–158). |
| Manual fact | NEC manual §6.3: Count/Compare (pp.164–165), Status (pp.166–170), Cause and exception codes (pp.171–173), EPC and address context (pp.174–179). |
| Manual fact | NEC manual §6.4: TLB/XTLB refill, invalid and modified causes, branch-delay EPC/BD, vectors and exception priority (pp.181–188). p.181 contains the stated BEV=1 general-vector conflict. |
| Manual fact | NEC manual §11.2–§11.3: cache geometry, physical tags, D-cache writeback and cache miss behavior (printed/PDF pp.275–282); §16 CACHE operation encodings and undefined combinations (pp.401–405). |
| Manual fact | NEC manual §3.1, §16 LL/LLD/SC/SCD and ERET: LLAddr/LLbit, alignment, result register, different-target undefined behavior, external-write caveat and ERET clear (pp.60, 452–455, 487–489). Successful-SC link clearing is not explicit in its pseudocode. |
| Project policy | Phase 3's warm-v1 profile uses prepared zero CP0 state, a coherent-functional cache, exact non-retiring BREAK debug completion, retirement-tick Count, descriptor v2 and state v8; see [boot limits](../../../docs/spec/boot-limits.md), [machine contract](../../../docs/spec/machine-contract.md) and `core/t64_cpu.c`. These are prepared-profile policies, not reset-state measurements. |
| Project policy | Use model `CPU:RSP=1:1` with guest tick in model RSP-cycle units only as selected profile data. ADR-0003 defers T1 cost tables to Phase 6 and T2 pipeline/hazard work. |
| Unknown | Physical CPU:RSP/Count rate for the unqualified NUS-CPU-03 target; TLB duplicate-match response beyond the manual's shutdown indication; invalid PageMask behavior; exact LL/SC invalidation granule and SC link-clear details; 32-bit BEV=1 general vector conflict; external DMA/cache snooping details. |

## Independent validation contract

Use project-authored instruction snippets and hand-computed expected values.
For every case, record whether the expected result comes from a cited manual
field, a hardware-derived vector, or an explicit project policy. Do not derive
expected values from another emulator's output.

- **Integer and control flow:** test 32-bit word-result sign extension, signed
  overflow without destination mutation, wraparound arithmetic, branch and
  branch-likely outcomes, and a faulting delay-slot instruction with the
  branch PC in EPC and Cause.BD set. Check an annulled likely slot neither
  fetches data nor retires.
- **CP0 and timer:** test each writable mask with set/clear patterns, read-only
  bits, RFU readback, MFC0 sign extension, Compare clearing IP7, Count wrap and
  Compare equality. Advance the same guest workload one-shot and in partitions;
  force idle/stall spans and verify Count phase/IP7 remain identical.
- **TLB and exceptions:** test all seven legal PageMask values, paired pages,
  ASID mismatch/match, both-global rule, V/D permission, TLBP miss/hit, TLBR,
  indexed/random writes, Wired boundary, and TLB refill/invalid/modified versus
  address error. Use independent vector sentinels for BEV and EXL cases,
  including both documented readings of the 32-bit BEV=1 general-vector
  discrepancy; the selected sentinel is `0xBFC00380` pending better evidence.
- **LL/SC:** test aligned LL and LLD data/LLAddr, successful and failed SC/SCD
  results, exact-address policy, unaligned exceptions before writes, ERET
  clearing, exception precedence over SC failure, and overlapping/non-overlap
  scheduled DMA writes under the chosen project invalidation policy.
- **Caches:** test I/D line sizes and index aliases, physical-tag hits, dirty
  eviction/writeback, cached versus uncached aliases, self-modifying code
  before/after I-cache invalidation, documented CACHE/TagLo operations, and
  unsupported operation refusal without partial cache or memory mutation. No
  cache-cycle claim is part of these tests.
- **State and compatibility:** verify every future-affecting v9 CPU/cache
  field changes the new digest and that old warm-v1/state-v8 fixed vectors are
  unchanged. Keep the existing seven known warm-profile ROM fixtures pinned;
  do not broaden that inventory as part of CPU coverage.
- **`nemu64-test`:** report CPU groups individually with exact pinned
  inventories. Keep runs local and derived ROM/output artifacts unuploaded
  while D2 fixture rights remain unresolved; native project-authored vectors
  remain the independent required lane.

## Adversarial cases and reversal evidence

Plans must explicitly guard against these failure modes:

- Cause.BD is lost because the delay-slot flag is cleared before EPC is
  captured; the branch destination is incorrectly resumed instead of the
  branch instruction being retried.
- A faulting load/store or overflow writes a GPR or memory before exception
  entry; a TLB-invalid store is mislabeled modified; a fetch-side fault is
  reported as a data-side exception.
- A timer fires only when instructions retire, so it vanishes during a stall or
  an idle guest-time advance; changing the advance partition changes IP7.
- Reserved fields overwrite TS, Cause exception bits or Random; writing Wired
  fails to reset Random; a TLB miss, invalid mapping and modified mapping all
  take one vector.
- Duplicate TLB matches silently select the first entry; ASID matching ignores
  the two-half global rule; an invalid PageMask is accepted as a legal size.
- SC succeeds after ERET or an overlapping DMA write, a different target
  stores anyway, or an exception takes the SC result path before its fault.
- D-cache dirty bytes are dropped on invalidate/eviction, I-cache never goes
  stale after code writes, or the implementation reports writeback timing as
  hardware evidence.
- New CPU semantics alter old warm-v1 logs/digests, or the test harness treats
  a guest architectural exception as a host crash / model-unsupported stop.

Reverse or version the selected policy only when evidence changes the contract:

- A NEC erratum or controlled hardware-derived vector resolves the p.181
  bootstrap-vector ambiguity or PageMask undefined cases.
- Hardware-derived NUS-CPU-03 clock evidence changes the model Count ratio or
  its relation to guest ticks.
- A rights-cleared hardware-derived LL/SC vector establishes the successful-SC
  clear rule, write invalidation granule, or DMA interaction.
- A primary cache specification or hardware-derived cache vector contradicts
  the line/tag/writeback model or its external-agent coherence policy.

Agreement with another emulator, passing a ROM, or a digest match alone is not
reversal evidence for hardware behavior. Such changes require a new named
profile/model policy and state digest version, preserving the old profile's
historical vectors.

## Synthesis corrections

The adversarial review in 04-ADVERSARIAL.md corrected the Status mask/RP policy,
nested EPC+BD capture and executable seed above. 04-CONTEXT.md owns the final
Count/Compare boundary order and exception rescheduling contract. Earlier
recommendation wording is not authority to reuse warm-v1 for the diagnostic.
