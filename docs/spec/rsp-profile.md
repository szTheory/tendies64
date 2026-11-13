# Functional RSP profile timing and control

This contract applies to profile
`ntsc-cart-4m-openipl3-rsp-functional-v1` (machine model 6, state domain 12).
The profile uses a deterministic 1:1 CPU-tick/RSP-opportunity software model.
It is a functional profile, not a cycle-accuracy claim.

The profile is development coverage. Tests author IMEM instructions and DMA
descriptors in synthetic machine fixtures; they do not run a ROM or diagnostic
asset. Passing tests establish only this selected software behavior. They do
not qualify physical hardware, all RSP instructions, general compatibility,
or the broader boot route. Capabilities are selected by the exact profile
identity and model/state tuple, not by numeric version ordering.

Canonical state domain `t64.state.rsp.functional.12` hashes the RSP registers,
vector and accumulator state, PC and branch delay state, readiness ticks,
issue sequence, retired count, status/semaphore state, active and pending DMA
descriptors, and their cursors/tokens. Tracer observations and refusal reasons
are diagnostics, excluded from canonical identity. Refused-operation tests
compare the complete future-state digest as well as the relevant memory and
queue state.

## Issue and dependency model

The guide describes in-order scalar-unit (SU) and vector-unit (VU) issue and
documents the dependency delays below. Tendies64 models these constraints as
integer ready ticks; it does not simulate the five pipeline stages.

| Operation or dependency | Model timing | Source basis and boundary |
| --- | --- | --- |
| SU arithmetic result | Available at the next RSP opportunity | The guide says SU results are available in the next cycle. [Guide, p. 44](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf) |
| SU load or move into an SU register (MFC2 here) | Ready at issue tick + 3; a consumer needs two intervening opportunities | The guide gives a three-cycle SU destination delay for SU loads and moves into SU registers. [Guide, p. 43](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf) |
| VU result, including MTC2 | Ready at issue tick + 4; a consumer needs three intervening opportunities | The guide gives a four-cycle VU destination delay, including moves into VU registers. [Guide, p. 43](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf) |
| Load or COP move followed by a store | A store waits until the recorded memory-access ready tick; the model produces the required one-opportunity bubble for the documented two-cycle case | The guide says a load followed by a store two cycles later causes one bubble and counts COP moves in that rule. [Guide, p. 43](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf) |
| SU/VU pairing | At most one adjacent SU followed by one VU, in order, in one opportunity; this implementation pairs the supported VADD form only when register dependencies are clear | In-order issue and no reordering are documented. The current opcode subset and one-opportunity-per-tick rate are software policy. [Guide, p. 39](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf) |
| Branch delay | One delay-slot instruction; a taken branch consumes one additional opportunity. Branches issue alone in this model. | The delay slot and taken-branch bubble are documented. The guide also describes branch pairing; this profile intentionally does not implement it yet. [Guide, p. 43](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf) |

Status command bits are applied in ascending command order: clear action first,
then set action for the same field. If both are present, set wins. This is a
deterministic software policy; the guide lists the clear and set command bits
but does not define their simultaneous precedence. [Guide, pp. 86–87](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf)

## SP control contract

The SP status register reports HALT, BROKE, single-step, interrupt-on-break,
and signal 0–7. DMA_BUSY and DMA_FULL are derived from the active and pending
descriptors. IO_FULL and DP execution are not modeled. Status command bits
clear/set HALT, BROKE, SP interrupt, single-step, interrupt-on-break, and the
eight signal flags. BREAK sets HALT and BROKE; when interrupt-on-break is set,
the model raises the SP interrupt. [Guide, p. 170](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf)

Reading SP semaphore returns its prior value and sets it. Writing zero clears
it; a nonzero write is refused. The guide documents read-to-set and zero-write
release semantics. [Guide, p. 88](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf)

At BREAK this functional profile leaves SP_PC on the BREAK instruction.
Resuming a later instruction therefore requires an explicit halted SP_PC
write before clearing HALT. This is the current software policy; the cited
BREAK description alone does not establish the subsequent hardware PC.

SP_PC is at `0x04080000`; the profile masks writes to the aligned 12-bit
instruction address (`value & 0x0ffc`). Running PC writes are refused with
`RUNNING_PC_WRITE`. The guide identifies the CPU-visible PC address and its
12-bit width. [Guide, p. 95](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf)

The profile refuses asynchronous HALT and single-step requests while an RSP
issue event is already in flight. CPU or DMA writes overlapping the active
IMEM instruction are refused with `ACTIVE_IMEM_WRITE`. The guide calls loading
microcode over the executing PC undefined; refusing it is this profile's
transactional policy, not a claim about the hardware outcome. [Guide, p. 96](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf)

## DMA model and conflict policy

SP DMA uses the RSP opportunity stream, including while HALT is set. Each
accepted descriptor has 12 setup opportunities followed by one committed
8-byte beat per opportunity. The guide documents 6–12 setup clocks and a peak
8-byte transfer rate; selecting 12 fixed setup opportunities is profile
policy. [Guide, p. 96](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf)

Address and shape decoding is:

- SP bank is selected once from memory-address bit 12; the bank does not
  change during the transfer.
- SP start is `mem_address & 0x0ff8`; the low three bits are ignored.
- Bytes per row are `((raw_length & 0x0fff) | 7) + 1`.
- Row count is `((raw_length >> 12) & 0xff) + 1`.
- Row skip is `(raw_length >> 20) & 0x0fff`.
- DRAM source/destination rows are checked before queue admission. SP bank
  offsets wrap within the selected 4 KiB bank.

The guide documents ignored low address/length bits, rectangular rows, the
double-buffered active/pending queue, and single-port memories. It does not
specify exact simultaneous issue/DMA arbitration. [Guide, pp. 96–97](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf)

Accordingly, when an RSP fetch conflicts with an IMEM DMA beat, or an RSP
DMEM load/store conflicts with a DMEM DMA beat, the model refuses the entire
opportunity before committing either the instruction/pair or DMA beat. The
preflight leaves guest memory, readiness ticks, descriptors, and queued events
unchanged. The scheduled opportunity itself still consumes a guest tick and
records a no-issue stall; this is deterministic refusal behavior, not a
prediction of silicon arbitration. Active-PC IMEM overlap has its more
specific `ACTIVE_IMEM_WRITE` refusal reason.

Unsupported opcodes, including CP0 accesses to DPC registers, are refused and
do not dispatch DP work. A full DMA queue and an invalid DRAM span are refused
before descriptor, token, queue, or memory mutation. CPU reservation observers
are notified for each committed DMA destination byte.

## Source

SGI/Nintendo, *N64 RSP Programmer's Guide*, Revision 1.0. Source URL:
[ultra64.ca guide PDF](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf).
The locally verified SHA-256 is
`6ec646ba9bd836af9aa30ae90099af55dbaf291fbd8b39ff8be5664ada3bd668`.
The digest identifies the cited document; it does not change the evidence
boundary above.
