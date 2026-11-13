# VR4300 integer memory model

This describes the strict native model in
`ntsc-cart-4m-openipl3-vr4300-v1` and the versioned software-compatibility
policy in `ntsc-cart-4m-openipl3-vr4300-compat-v1`. These are independently
expected software behaviors, not physical-console qualification. Warm-v1
retains its existing behavior.

## Access and commit boundaries

The CPU stages scalar loads/stores and LWL/LWR/LDL/LDR/SWL/SWR/SDL/SDR
through the MMU before committing a destination or store. Merge instructions
select bytes from their own aligned container and preserve untouched bytes.
A pair comprises two guest instructions: if its second member faults, the
first member's already committed effect remains valid. Word merge results
are sign-extended to 64 bits. The six doubleword memory operations require
the selected 64-bit execution mode.

Scalar accesses retain their bus width. Wide or masked operations that the
device model cannot support are refused before device mutation; they are
not decomposed into side-effecting byte accesses. Unsupported device/model
operations remain distinct from guest address, TLB and bus exceptions.
Scratch staged effects are not serialized; committed architectural state is.

## Write-back data cache

The prepared profile uses a 512-entry, 16-byte-line direct-mapped D-cache.
Cacheable loads and stores select by virtual VA[12:4] and match the translated
physical line. A load/store miss stages the full line before replacing an old
entry; a dirty victim is written from its stored physical tag only after the
entire destination range is accepted. A cacheable CPU store changes selected
line bytes and marks the line dirty while backing remains unchanged. A refused
fill or writeback preserves the old line, its dirty bytes, backing, and LL
reservation.

RDRAM and SP DMEM/IMEM are accepted writeback destinations. Admitted cartridge
bytes can be read for fills, but cartridge writeback is refused because that
range is read-only. An unmapped or physical-range writeback failure raises the
guest DBE exception (ExcCode 7) before retirement; mapped unsupported device
interactions remain model stops. Cacheable and uncached aliases, plus scheduled
PI/SP DMA, intentionally do not snoop D-cache lines. A later cached access can
therefore see the old line until replacement or an applicable CACHE operation.
Committed CPU byte stores invalidate only overlapping LL bytes. DMA invalidates
only bytes it actually commits. Cache writeback is a separate writer: it does
not snoop and does not invalidate an unrelated LL reservation.

## Reverse-endian selection

Config.BE stays one. Status.RE applies in user mode; instruction fetch and
kernel data accesses keep their selected big-endian behavior. Physical
backing bytes are stored in canonical big-endian address order.

For a user mapping VA `0x1000` to PA `0x2000`, store physical bytes
`11 22 33 44 55 66 77 88` at PA through PA+7. With RE set, the selected
scalar results are:

| Instruction at VA | Physical bytes accessed | Result |
| --- | --- | --- |
| LBU | PA+7 | `0x88` |
| LHU | PA+6 through PA+7 | `0x7788` |
| LW | PA+4 through PA+7 | `0x55667788`, sign-extended |
| LD | PA through PA+7 | `0x1122334455667788` |

For independent stores with source `0xA1B2C3D4E5F60718`, SB writes `18`
at PA+7, SH writes `07 18` at PA+6, SW writes `E5 F6 07 18` at PA+4,
and SD writes `A1 B2 C3 D4 E5 F6 07 18` at PA. There is no additional
byte reversal within the selected physical transfer.

The byte and halfword rules follow the explicit physical-address XOR and
operand-position equations. LD/SD use the unchanged doubleword address.
The width-four rule is **selected model policy inferred from adjacent
primary-source operations**: this manual edition's LW/SW entries omit the
RE positioning printed for LL/LWC/SWC and the partial-word tables. The model
uses the coherent physical-lane XOR of four. It does not treat that inference
as resolved silicon behavior. A better primary specification or an authorized,
independent physical measurement contradicting those lanes would reopen it.
The discarded fixed-address byte-swap interpretation is not valid evidence.

## Watch and fault ordering

WatchLo read bit 1 and write bit 0 control matching of the selected physical
address above its low three bits. EXL masks Watch exceptions; a masked access
can complete. CACHE does not generate Watch. Delay-slot Watch exceptions
preserve the branch EPC and BD context. These checks do not establish an
additional deferred pending-latch mechanism.

Native tests exercise alignment, segment/TLB permissions, Watch, physical
range faults, merge offsets, page boundaries and refused-device effects.

## LL/SC reservation policy

The manual defines LLAddr as the physical address bits `[31:4]` of the last
load-linked operation. Tendies64 records that diagnostic value while keeping
the full linked physical start and transfer width in per-machine state. A
successful, translated LL/LLD replaces the reservation only after the load
completes.

The following are explicit model choices for this selected profile, not
claims about undocumented silicon behavior:

- SC/SCD check access faults before producing a result or attempting a store.
  A completed decision succeeds only when the physical start and width match
  the active reservation. Either completed result consumes it once; a fault
  preserves the reservation through fault entry, and ERET clears it. The
  source value is captured before an aliased result register is written, and
  `rt=0` still performs the conditional store decision.
- Committed external writes invalidate only when their physical byte interval
  intersects the linked interval. Touching intervals remain separate; a
  single shared byte invalidates. Rejected or canceled writes have no
  invalidation effect. Bus masked writes report only selected committed bytes.
- D-cache writeback publishes its cache line to backing without treating the
  writeback as an external snooper or invalidating unrelated LL links.
- PI and SP DMA invalidate the physical backing bytes they actually commit:
  PI writes RDRAM, while SP DMA can write RDRAM or SP DMEM/IMEM. DMA does not
  snoop or update the modeled D-cache. A targeted D-cache invalidate applies
  the selected 16-byte physical line interval to the reservation.

These choices keep the coherent-functional memory model deterministic and
make the invalidation granule observable in tests. Reopen them if a stronger
primary VR4300 specification or an authorized independent hardware
measurement establishes different SC/SCD matching, consumption, fault, or
invalidation behavior. That evidence must identify the tested processor and
transfer widths and include the byte-level result; another emulator's output
or an unlicensed source tree is not reversal evidence.

## Versioned software-compatibility policy

`ntsc-cart-4m-openipl3-vr4300-compat-v1` uses machine-model version 4 and
state-serialization version 10. It shares the full VR4300 CPU state with the
strict model, while selecting only the deterministic undefined-operation
rules below. The strict model remains model 3/state 9 with its existing stops
and digest behavior.

The VR4300 manual says a branch in a delay slot is not detected and its result
is undefined; it also says division by zero and LL/LLD in non-cacheable memory
are undefined. Compatibility expectations select these deterministic software
rules. They are not claims about silicon behavior:

- A branch in a pending delay slot executes and retires. Its link, if any,
  receives the branch instruction's PC+8. The outer branch's pending target
  supplies the inner branch's one delay-slot instruction; after that
  instruction commits, control transfers to the inner branch's selected
  target. A not-taken likely branch annuls that displaced instruction and
  continues at the inner branch's PC+8. A fault in the displaced slot sets
  BD and records the inner branch as EPC. The branch origin is explicit in
  v10 serialized CPU state.
- Word and doubleword DIV/DIVU/DDIV/DDIVU with a zero divisor complete without
  an exception. Signed word DIV keeps the existing sign-dependent quotient
  (`0x7fffffff` for a nonnegative dividend, `0x80000001` for a negative one)
  and the dividend as remainder. Unsigned word DIVU uses quotient
  `0xffffffff` and the word dividend as remainder. Signed doubleword DDIV
  uses quotient `-1` for a nonnegative dividend and `1` for a negative
  dividend; DDIVU uses an all-one quotient. Both retain the dividend as the
  remainder. Word results are sign-extended to 64 bits. Signed minimum-value
  divided by `-1` wraps the quotient to that minimum value and produces zero
  remainder. These results use unsigned host arithmetic and commit HI/LO
  before normal retirement.
- In model 4/state 10 only, translated uncached LL/LLD reads backing memory
  and records the same physical start and width as cached operations. SC/SCD
  succeeds only for that exact reservation, writes through the uncached bus
  path, and consumes the reservation once. Strict model 3/state 9 continues to
  stop on uncached LL/SC.

The compatibility profile is intentionally narrow. Other undefined
operations, unimplemented instructions, and unsupported device interactions
remain model stops unless a separately reviewed policy and test add them.

## Source and evidence

NEC, *VR4300/VR4305/VR4310 64-Bit Microprocessor User's Manual*, seventh
edition, August 2000, U10504EJ7V0UMJ1, SHA-256
`b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`
([public PDF](https://hack64.net/docs/VR43XX.pdf)). Printed page references:
Figure 3-2, pp.62–63; endian and memory adapter definitions, pp.365–368;
LBU/LD, pp.439–441; LHU, pp.450–451; SB/SD/SH, pp.486/491/501;
LW/SW omissions, pp.458/515; LL/LWC/SWC, pp.453/460/516;
LWR/SWR tables, pp.466/523; partial merges, pp.445–446/497/499–500;
LLAddr, p.154; LL, pp.452–453; LLD, pp.454–455; Watch, pp.175/198.
Branch-in-delay undefined behavior, p.369; DIV/DIVU and DDIV/DDIVU details,
pp.410–416.

The registered CPU-memory matrix and CPU-path integration tests contain
independent expected bytes. The [04-07 portable RED record](../../.planning/phases/04-complete-vr4300-cpu/04-07-RED-evidence.md)
and [04-08 portable RED record](../../.planning/phases/04-complete-vr4300-cpu/04-08-RED-evidence.md)
distinguish valid behavioral failures from discarded vectors and setup errors.


## Compat-v2 unpopulated data windows

The separate model5/state11 profile selects zero-valued CPU data reads in
unpopulated RDRAM from the configured RAM end to `0x03f00000`, excluding the
modeled absent-RDRAM probe, and in unbacked Domain1C
`[0x1fd00000,0x80000000)`. The complete read or D-cache line must fit one
window; empty, excessive and overflowed ranges are rejected. The public bus
decoder, DMA and other cartridge domains are unchanged.

Uncached fallback requires an unsupported/unmapped bus status and the v2
range predicate. Cached reads use the corresponding D-cache fill policy.
Mapped MMIO failures and instruction fetches are never converted to zero.
Corresponding stores keep the model-unsupported stop before bus/cache commit,
with registers, reservations and resident lines preserved. Alignment,
privilege, Watch, TLB and high-address faults retain priority over this policy.
A valid large-page data read can complete with zero, while its adjacent invalid
TLB page still faults. No system-bus hardware data value is established.
