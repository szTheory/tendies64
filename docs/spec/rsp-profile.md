# RSP functional profile

## Profile boundary

`ntsc-cart-4m-openipl3-rsp-functional-v1` is a deterministic software profile
for one narrow, fetched RSP path. Its descriptor version is 3, machine model
version is 6, and state serialization version is 12. It keeps the NTSC US
4 MiB cartridge and prepared `hle-pif+open-ipl3` boot route, with the boot
status `prepared-rsp-functional-entry-unqualified`.

This profile is development coverage. Its tests author IMEM instructions in a
synthetic machine fixture; they do not run a ROM or diagnostic asset. No result
here qualifies a physical N64, all VU instructions, full RSP software,
compatibility, timing accuracy, or the general boot route.

Capabilities are selected by exact profile predicates. The RSP state capability
is model/state tuple `(6, 12)`, and the CPU compatibility extension explicitly
accepts `(5, 11)` and `(6, 12)`. Numeric ordering does not imply capability.
Profile admission also checks the pinned descriptor identity and hash; changing
an existing profile's descriptor does not make it an RSP profile.

The descriptor binds these selected policies:

| Field | Value |
| --- | --- |
| Guest tick | `model-rsp-cycle` |
| CPU:RSP ratio | `1:1` |
| Timing tier | `functional-rsp-issue-model-policy-v1` |
| SP model | `rsp-functional-issue-dma-v1` |
| SP register policy | `rsp-issue-status-dma-policy-v1` |
| DMA visibility policy | `rsp-one-beat-per-issue-v1` |
| SP duration base / divisor | `12` model cycles / `8` bytes |

These values identify emulator policy. They are not measurements of hardware
frequency or bus timing.

## Canonical RSP state

State domain `t64.state.rsp.functional.12` hashes every future-affecting field
explicitly, without C padding:

- `gpr[32]`, `vector[32][16]` byte lanes, and `accumulator[8]` masked to 48
  bits;
- VCO, VCC, VCE, PC, delay target, and delay-slot state;
- divider input/result/high-valid latches;
- scalar, vector, and accumulator readiness arrays;
- next issue tick, issue sequence, retired instruction count, and stall slots;
- SP status and semaphore state;
- DMA beat cursor, setup remaining, active/pending tokens, and DMA event
  sequence.

The RSP observation fields used by the test tracer are diagnostics. They are
excluded from canonical identity, and the refusal test checks that changing
only those observations leaves the digest unchanged. The same test checks that
an unsupported fetched opcode leaves canonical RSP state unchanged.

## Implemented fetched slice

The scheduler fetches big-endian 32-bit words from the 4 KiB SP IMEM bank at
the current RSP PC. The registered authored path covers:

- NOP and ADDIU scalar execution;
- MTC2 and MFC2 for aligned even byte elements;
- VADD with vector or documented scalar element selection, signed 16-bit
  addition, VCO carry-in, signed saturation, low accumulator-slice update, and
  VCO clear;
- aligned SW to the 4 KiB SP DMEM bank, with explicit big-endian byte writes;
- BREAK setting HALT and BROKE, plus the optional MI SP interrupt.

Supported non-BREAK instructions reserve and schedule the following issue
event before applying their effects. Unsupported or malformed instructions are
refused before instruction effects. The synthetic tracer hand-derives its
expected `0x7fff` VADD result, accumulator low slice `0xffff` with VCO carry-in,
and DMEM bytes `00 00 7f ff`.

This slice does not implement branches, general scalar loads/stores, CP0
transfers, other vector operations, divider behavior, or RSP DMA execution.
The new profile refuses the legacy atomic SP DMA path; its declared future-state
DMA fields do not imply DMA execution support. Unsupported operations remain
outside the tested coverage and do not retire.

## Evidence and extension

The registered default SP test continues to run its existing assertions and
the RSP tracer. `unit.core.rsp-tracer` also runs the RSP admission, digest,
fetched-operation, refusal, and old-profile refusal checks independently.
The implementation remains unqualified software behavior until later phase
work expands and validates its boundaries. Adding support does not create a new
profile identity; changes to the declared capability or policy require an
explicit descriptor decision.
