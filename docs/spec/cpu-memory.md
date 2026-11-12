# VR4300 integer memory model

This describes the native model in `ntsc-cart-4m-openipl3-vr4300-v1`.
It is independently expected software evidence, not physical-console
qualification. Warm-v1 retains its existing behavior.

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

The registered CPU-memory matrix and CPU-path integration tests contain
independent expected bytes. The [04-07 portable RED record](../../.planning/phases/04-complete-vr4300-cpu/04-07-RED-evidence.md)
and [04-08 portable RED record](../../.planning/phases/04-complete-vr4300-cpu/04-08-RED-evidence.md)
distinguish valid behavioral failures from discarded vectors and setup errors.
