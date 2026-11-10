# Tendies64 profile limits

## Current named profile

`ntsc-cart-4m-skeleton` is a deterministic model target. Its descriptor names a
NUS-CPU-03 console target as **unqualified**, NTSC and US region, cartridge
media, 4 MiB RDRAM, no expansion, no peripherals and no save device. Its
`hle-pif+open-ipl3` boot route records intended ownership; boot is
unimplemented.

The descriptor sets `GuestTick` to one model RSP-cycle unit, a CPU:RSP ratio of
1:1 and a synchronization quantum of one model tick. These are selected model
units, not measured clock rates. The timing tier is `skeleton-atomic`, event
ordering is `fifo-v1`, and the named PI/SP policies are
`pi_dma_skeleton_v1` and `sp_dma_skeleton_v1`. Candidate durations are
`1 + N/2` model ticks for PI and `12 + payload/8` model RSP cycles for SP.
They do not model cycle-accurate arbitration or progressive DMA visibility.

The profile descriptor also records the 4 KiB to 64 MiB raw image bounds,
256-entry event queue, 64-bit event sequence, 4096-event per-advance budget,
ROM byte-order policy, queue and sequence refusal policies, register/error and
cancellation policies, and descriptor, machine and state-serialization
versions. These fields participate in a versioned canonical serialization:
UTF-8 text values use a big-endian 32-bit byte length, and numeric values use
fixed-width big-endian encoding. No padding, pointers or map iteration order
participates. The current descriptor uses state-serialization version 5 to
include PI and SP request/latch state in the `t64.state.v5` digest. Trace
capacity is observer-only and excluded from behavior identity and state digest.

Build identity is a separate `t64-build-v1-` SHA-256 fingerprint over the
project version, compiler/build settings and SHA-256 hashes of production
source, headers, and build configuration inputs. It contains no timestamp or
local path. The Git source revision is recorded separately in native test
evidence. Neither build identity nor source revision enters machine state
digest. A native result label such as “unit-tested synthetic counters” reports
only what the test exercised; it is not guest execution or hardware evidence.

## Unsupported, deferred and unknown behavior

| Area | Current disposition | What this profile says | Evidence needed to promote it |
|---|---|---|---|
| VR4300 CPU | Unsupported | No instruction execution is implemented. | Pinned primary CPU documentation, authored instruction/exception vectors and independently checked results for the named revision. |
| PIF and IPL3 boot | Unsupported | The open-IPL3 route is intent only; no PIF bytes or boot sequence are installed. | Primary boot documentation plus a rights-cleared, identified boot input and reproducible boot-boundary evidence. |
| PI/SP timing and RDRAM arbitration | Deferred | Versioned atomic timing policies define the skeleton model; they are not hardware timings. | Primary register/arbitration specification or rights-cleared hardware traces covering request size, concurrent agents and observed completion/visibility. |
| VI scanout and AI audio | Deferred | No raw video or audio generation is implemented. | Pinned device documentation, deterministic raw-output tests and named profile/build evidence at the device boundary. |
| Hidden and undocumented register bits | Unknown | The skeleton does not claim behavior for unobserved bits. | Primary documentation or controlled, rights-cleared hardware observations with boundary and negative-control cases. |
| Expansion, peripherals and save devices | Unsupported | The profile selects none of these devices. | A separately named profile, exact device model, bounded bus/register tests and rights-cleared guest or hardware evidence where required. |
| Stable external ABI | Deferred | The C surface is provisional; ownership and refusal behavior may still evolve. | Completed lifecycle/error contracts, compatibility tests across supported consumers, and an explicit ABI/version decision. |

The console target, boot route, profile name, deterministic output, passing
tests or agreement with another emulator does not qualify the hardware target
or establish broad compatibility.
