# VR4300 prepared CPU profile

## Scope and evidence

`ntsc-cart-4m-openipl3-vr4300-v1` is a named, deterministic software seed for
an unqualified `NUS-CPU-03` target. It is a prepared warm entry; it is not a
cold-reset measurement or a claim that this board's clocks and implementation
fields have been measured. The profile uses descriptor v3, machine model v3,
state serialization v9, and HLE seed-reset policy v2.

The first integration check admits the tracked, cleared IPL3 compatibility
fixture, starts at `0xA4000040`, and executes one scheduled instruction. The
fixture contains opcode `0x3044D236`, decoded as `ANDI a0,v0,0xD236`; with the
seeded zero `v0`, it writes zero to `a0`. This establishes the pinned entry
path only. It does not establish full IPL3 completion or general ROM support.

Hardware field references are NEC, *VR4300/VR4305/VR4310 64-Bit
Microprocessor User's Manual*, U10504EJ7V0UMJ1, 7th edition, August 2000,
SHA-256 `b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`
([public PDF](https://hack64.net/docs/VR43XX.pdf)). The cited Config and PRId
fields are in §§5.4.5–5.4.6, pp.151–153; Count and Compare are in §6.3.3,
p.164 and §6.3.4, p.165. Prepared values and emulator clock mapping remain
software choices where noted. The project
[CPU decision record](../../.planning/phases/04-complete-vr4300-cpu/04-DECISION-CPU.md)
records the manual evidence, unresolved alternatives, and reversal criteria.

## Prepared seed

The ROM admission path first clears machine, RDRAM, SP DMEM, and SP IMEM state,
then copies the checked IPL3 payload from ROM offset `0x40` to DMEM offset
`0x40`. The admitted asset is 4096 bytes; its payload is 4032 bytes. The
descriptor pins asset SHA-256
`f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068`, payload
SHA-256
`f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5`, and boot
policy SHA-256
`faa54438c8b0cdd60e1e388b61566a963ad3d1feb05c23cbf207b4645157064c`.
Descriptor v3 serializes all three hashes and the seed-reset version.

| State | Prepared value | Basis |
| --- | ---: | --- |
| PC | `0xA4000040`; full address `0xFFFFFFFFA4000040` | Existing checked IPL3 entry contract; canonical sign extension |
| GPR19–GPR23 | `0, 1, 1, 0, 1` | Selected IPL3 ABI seed |
| Other GPRs, HI, LO | zero | Cleared prepared software state |
| Config | `0x7006E463` | EP=0; EC=7 selected model value; BE=1; CU=0; K0=3; fixed/read-only implementation fields follow manual §5.4.6 |
| PRId | `0x00000B00` | Imp=0x0B follows manual §5.4.5; revision 0 is selected software policy |
| Status | `0x34000000` | CU0, CU1 and FR set; kernel mode; IE, EXL, ERL and BEV clear; prepared software policy |
| Random / Wired | `31 / 0` | Selected prepared instruction boundary |
| Count / Compare | `0 / 0` | Selected prepared boundary; no initial equality latch |
| FCR0 / FCR31 | `0x00000B00 / 0` | FCR0 matches the selected implementation ID; control state cleared |
| TLB | 32 invalid paired entries with zero fields | Prepared invalid state |
| I-cache | 512 invalid lines, 32 bytes each, zero tags and data | 16 KiB geometry; empty prepared state |
| D-cache | 512 invalid clean lines, 16 bytes each, zero tags and data | 8 KiB geometry; empty prepared state |
| LL link | invalid, address 0, width 0 | No prepared reservation |
| Pending exception / TLB shutdown | clear | No prepared exception or shutdown |

Config is formed from `EC=7` at bits 31:28, `EP=0`, fixed bits 23:16=`00000110`,
`BE=1` at bit 15, fixed bits 14:4=`11001000110`, `CU=0`, and `K0=3`. The EC
selection, PRId revision, Status choice, initial Random value, and complete
prepared register set are software policy for this unqualified target. They
must not be described as reset-silicon observations.

All other CP0 slots are zero in this prepared state. Random, PRId, and FCR0 are
represented by their selected values above. The initial Count phase is zero;
`count_compare_deadline = UINT64_MAX` means no later equality deadline has
been materialized before the first positive guest-time synchronization, and
the equality latch and its generation start clear. The first positive
synchronization computes the next strict equality deadline, including a full
32-bit wrap when prepared Count and Compare are initially equal. In the new
profile, Count advances once per two guest ticks regardless of retirement;
this selected mapping is deterministic model policy, not a physical clock-rate
measurement. Warm-v1 retains its exact retirement-based Count behavior.

## Internal state contracts

`core/t64_internal.h` keeps this state per machine. It defines raw CPU
operation-context, exception, MMU translation, cache-operation, and COP1 result
records with no host callbacks. These are internal contracts for later CPU
slices. Their presence does not mean those operations execute today.

The prepared v9 CPU fields are:

1. Full-width current, previous, delay-slot, delay-target, and stop addresses.
   The existing 32-bit PC fields remain for the bounded interpreter; v9 also
   binds their canonical 64-bit forms.
2. Attempt outcome (`none`, `retired`, `guest exception`, `model unsupported`,
   `software stop`, or `host error`), CPU semantic-policy version, and the
   Config initialization-window flag (false in the prepared profile).
3. TLB shutdown and pending-exception validity, code, bad address, EPC, and
   branch-delay flag.
4. Thirty-two paired TLB records. Each stores PageMask, VPN2, region, ASID,
   EntryLo0, and EntryLo1. Valid, dirty, global, and cache attributes remain
   encoded in the EntryLo values rather than being stored twice.
5. Count synchronization tick, half-clock phase, Compare equality deadline,
   latch generation, and equality-latched flag.
6. Physical LL link address, access width, and validity.
7. Split I-cache and D-cache arrays. Every line stores its physical tag,
   validity and bytes; D-cache lines also store dirty state.
8. Thirty-two 64-bit FPR values, FCR0, and FCR31.

The initial v9 digest is
`dfd84ba75420a9cf7f0883c18b78fbf8999cf13f41dd79d28879a5d724b6720e` for the
pinned fixture and prepared seed. The profile test pins descriptor hash
`d1f620cfef32824759c40c4e22a7e9f6cab3796981f34ac1e1dfd55c78f197a1`.
Both vectors are checked against independent test-side serialization or
fixed-golden assertions.

## v9 state identity

The v9 domain is `t64.state.v9`. It keeps the existing v8 field order and
appends the fields above immediately after the existing CPU stop fields.
Counts and numeric values use fixed-width big-endian encoding; booleans are
encoded as `u32` 0 or 1. Arrays include their declared counts and every
element, including invalid
tags, line data, and zero FPR values. No C padding, host pointer, trace cursor,
or output drain cursor is serialized. Retained guest output and its exactness
counters continue through the established versioned output branch.

Descriptor v3 explicitly writes asset hash, payload hash, boot-policy hash,
and HLE seed-reset version. Descriptor v2 and state v8 retain their previous
bytes and fixed vectors. The tests check each added descriptor field and each
v9 CPU-state field for digest sensitivity, reset to the prepared digest, and
independent mutable state across two machines.

## Limits and unresolved evidence

The VR4300 profile implements paired TLB translation and management across
the selected 32-bit and extended address modes, with precise refill, invalid,
modified, and address faults. Cache attributes are carried by translation.
The functional I-cache fetch, fill, stale-code, and targeted invalidation
policy is documented in the [cache model](cpu-cache.md). D-cache
hit/refill/writeback behavior and COP1 arithmetic remain later work.
LL/SC follows the tested selected physical-address-and-width reservation policy
in the [memory contract](cpu-memory.md); that model behavior remains separate
from hardware qualification. Integer scalar and merge accesses, user
reverse-endian physical lanes, and Watch exceptions have native model tests.
The memory contract records exact byte selection, fault boundaries and the
inferred width-four RE rule; none is a physical-console qualification.
Count/Compare synchronize against guest ticks,
merge timer, MI, and software interrupt ownership, and are included in the v9
digest. The timer handler path is model evidence only; it does not establish a
physical clock rate or broad ROM compatibility. The BEV=1 general exception
vector discrepancy on manual p.181 remains unresolved: its table implies
`0xBFC00380`, while its prose gives `0x80000180`. The selected handler path
uses BEV=0 vector `0x80000180`; the BEV=1 policy remains unqualified.

The warm-v1 profile retains descriptor v2, machine model v2, state v8, and
its existing seven fixtures and exact golden bytes. The new profile's current
evidence covers its pinned entry, a deterministic Count/Compare interrupt
handler, and a guest TLB-refill handler that installs a mapping and retries the
load. Native tests cover all 32 TLB entries, seven page sizes, modes, permissions,
and management operations. It does not establish broad software compatibility, physical
clock accuracy, or hardware accuracy.

TLB tag/ASID matching precedes the V check (NEC §5.4.9, pp.155–157). Invalid
entries therefore still participate in probe and duplicate detection. The
prepared zeroed entries can produce duplicate matches for virtual tag zero
with ASID zero; the selected model sets TS and enters shutdown until reset.
This is a documented consequence of the software seed, not a claim about
hardware reset contents. Unsupported masks or physical-width requests are
refused before partial architectural effects. Warm-v1 keeps its separate
direct-only translation and CACHE subset.
