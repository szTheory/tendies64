# VR4300 CPU timing policy

## Profile scope

The static T1 instruction-cost table applies to these exact software profiles:

| Profile | Model | State | Descriptor | Timing policy |
| --- | ---: | ---: | ---: | --- |
| `ntsc-cart-4m-openipl3-rdram-a-t1-v1` | 7 | 13 | 4 | `t1-instruction-cost-v1` |
| `ntsc-cart-4m-openipl3-rdram-b-t1-v1` | 8 | 13 | 4 | `t1-instruction-cost-v1` |

Both profiles use `model-rsp-cycle` as the guest tick unit and a selected 1:1
CPU:RSP ratio. The exact model/state tuples select T1; the implementation does
not infer capabilities from a version number being greater than another.
Earlier profiles keep their existing fixed one-tick CPU interval and frozen
identities.

## T1 instruction-cost-v1

The NEC VR43XX manual's §3.2.2 Table 3-12 reports the following integer MDU
stall counts. The emulator policy adds one issue interval to each stall count
and uses that sum as the interval before the next CPU event:

| Instruction class | SPECIAL `funct` | Manual stalls (PCycles) | T1 interval (model ticks) |
| --- | ---: | ---: | ---: |
| Ordinary instruction | — | — | 1 |
| `MULT` | `0x18` | 5 | 6 |
| `MULTU` | `0x19` | 5 | 6 |
| `DIV` | `0x1a` | 37 | 38 |
| `DIVU` | `0x1b` | 37 | 38 |
| `DMULT` | `0x1c` | 8 | 9 |
| `DMULTU` | `0x1d` | 8 | 9 |
| `DDIV` | `0x1e` | 69 | 70 |
| `DDIVU` | `0x1f` | 69 | 70 |

The function values are the raw SPECIAL instruction fields. The independently
authored tests use literal opcodes with `op=0`, `rs=1`, `rt=2`, `rd=0`, and
`shamt=0`. The manual anchors those encodings on the instruction pages for
MULT (p. 479), MULTU (p. 481), DIV (p. 415), DIVU (p. 417), DMULT (p. 421),
DMULTU (p. 422), DDIV (p. 413), and DDIVU (p. 414).

The primary source is NEC, [*VR4300/VR4305/VR4310 64-Bit Microprocessor User's
Manual*](https://hack64.net/docs/VR43XX.pdf), document U10504EJ7V0UMJ1, 7th
edition (August 2000), SHA-256
`b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`.
Printed page 76, Table 3-12 supplies the stall classes. Those stall counts are
manual facts. The issue-plus-stall conversion and mapping to `model-rsp-cycle`
are selected emulator software policy; they are not measured NUS-CPU-03 clock
values or a claim of hardware cycle accuracy.

Every instruction outside the eight MDU classes uses the one-tick baseline.
An architectural delay-slot instruction is charged by its own class once; the
branch does not add a second delay-slot cost. Guest exceptions and sampled
interrupts schedule their next boundary after one base tick and do not retire
an instruction or step CP0 Random. CP0 Count follows elapsed guest time at the
profile's selected half-PClock mapping, independently of retirement. Random
steps once when an instruction retires, regardless of its T1 interval.

Before guest effects are committed, the core checks the successor tick for
overflow and checks event-queue and event-sequence capacity. A rejected
successor leaves the instruction's registers, memory, CP0 effects, and
retirement counters unchanged. Device-side events caused by a store are
included in the resource preflight.

## Evidence limits and T2 promotion

T1 is a deterministic, serialized class-cost model. It does not model
overlapped pipeline stages, inter-instruction hazards, result forwarding,
variable or operand-dependent MDU latency, cache misses, RDRAM or bus wait
states, or a separate floating-point instruction cost. The five-stage smooth
pipeline throughput and additional cache/refill timing described in §§4.1,
4.6.4, 11.2, and 11.3 require evidence and behavior beyond this table.

Promote to a separately identified T2 profile only when rights-cleared authored
expectations and independent hardware-derived or primary evidence demonstrate
behavior that cannot be represented by this fixed table, such as overlap,
interlocks, or stage ordering. Promotion evidence should include exact target
and build identity, raw boundary observations for dependent and independent
instruction sequences, Count/Compare and Random ordering, delay slots,
exceptions, and equal-tick CPU/device events. A changed table or deeper model
gets a new profile identity; it does not rewrite existing profile behavior.

### T2 evidence checklist

Before specifying or implementing T2, retain a reviewable evidence packet with:

- Exact emulator revision, build configuration, profile identity, target model,
  state, boot/firmware inputs, and rights-cleared authored instruction streams.
- Raw pipeline boundary observations for independent and dependent sequences,
  including forwarding/interlocks, load-use, branch/delay-slot, and MDU cases.
- Cache hit, miss, refill, bus/RDRAM wait, and arbitration observations, with
  cache and memory state recorded at each boundary.
- Exception and interrupt observations that identify the stage at which they
  are recognized and their interaction with delay slots and architectural state.
- Count/Compare, Random, and CPU/RSP equal-tick ordering, including DMA work
  dispatched during an MDU interval and repeatable one-shot/partitioned runs.
- Independent primary or hardware-derived support for each timing claim,
  separated from emulator measurements and software-policy choices.
- Reproducible raw traces, literal expected values, and a documented account of
  each source, measurement method, uncertainty, and unresolved disagreement.

This checklist records evidence required for a future decision. It does not
assign T2 costs, add dynamic latency, or change current profile behavior.

See [ADR-0003](../adr/0003-cpu-timing-tiers.md) for the tier definitions and
the [Phase 6 CPU timing decision](../../.planning/milestones/v1.0-phases/06-cold-boot-cpu-timing-and-full-base-suite/06-CPU-TIMING-DECISION.md)
for the selected policy, evidence boundary, and reversal criteria.
