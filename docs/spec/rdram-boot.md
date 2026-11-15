# Deterministic RDRAM boot seed and first Tier B observation

## Scope

This note records the software seed and the first bounded Tier B startup
observation for the rights-cleared, authored `hello-openipl3` fixture. The
fixture is 4,280 bytes with SHA-256
`fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c`; its
cleared 4,032-byte IPL3 payload has SHA-256
`f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5`. This
is a deterministic emulator startup experiment, not a physical cold-reset
measurement or a general ROM support claim.

## Seed ownership

The canonical machine owns the HLE prepared state. Admission clears the
machine, RDRAM, SP DMEM, and SP IMEM, then copies the pinned IPL3 payload from
ROM offset `0x40` to DMEM offset `0x40` and starts at `0xA4000040`. The
deterministic common seed retains the existing selected S3–S7 values and
VR4300 state documented in [CPU profile](cpu-profile.md). The two Phase 6
profiles bind the device selection explicitly:

| Profile | Installed topology | `MI_VERSION` | `RI_SELECT` | Timing policy |
| --- | --- | ---: | ---: | --- |
| `ntsc-cart-4m-openipl3-rdram-a-t1-v1` | 4 MiB, no expansion | 0 | 5 | `t1-instruction-cost-v1` |
| `ntsc-cart-4m-openipl3-rdram-b-t1-v1` | 4 MiB, no expansion | 0 | 0 | `t1-instruction-cost-v1` |

These values are profile-selected software inputs. In particular, zero
`RI_SELECT` is the seed for the source-described cold path, not a claimed
observation of a console's IPL2 handoff or register reset values. The existing
warm-v1 identity and behavior remain frozen.

## First Tier B result

One production `integration.cold-startup` invocation used the cleared fixture
and Tier B profile. The first observed RI operation was:

| Field | Observation |
| --- | --- |
| Operation | Read |
| Physical address | `0x0470000c` (`RI_SELECT`) |
| Width | 4 bytes |
| Value | `0x00000000` |
| CPU PC / opcode | `0xA4000094` / `0x8c42000c` (`lw v0,0xc(v0)`) |
| Guest tick | 53 |
| Bounded run stop | `CPU_UNSUPPORTED` at tick 75 |
| Retained output | 0 bytes; empty-prefix SHA-256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| Dropped trace records | 0 |

The run therefore reached the selector read and recorded the seeded zero. It
did not establish successful RDRAM initialization, an authored hello pass, or
the exact terminal unsupported instruction. The traced selector read itself
was serviced successfully; the saved terminal record has no stop PC/opcode or
bus refusal status/address, so it cannot distinguish an unsupported CPU
encoding from a later unsupported bus/register operation. The stop reason is
an observed model boundary, not guest completion. The exact invocation and
unmodified runner result are retained in the ignored local execution
checkpoint beneath `build/gate/06-cold-startup/`.

## Limits

The result applies only to this build, profile, cleared fixture, and bounded
machine run. It does not establish the next RDRAM operation, the full source
branch, device topology, calibration values, Expansion Pak support, physical
boot behavior, or hardware accuracy. Do not infer a hello success from the
selector read or the CPU stop. Further modeled behavior requires its own
source-reached operation and exact address, width, effect, state, and refusal
controls while preserving the historical profile contracts.
