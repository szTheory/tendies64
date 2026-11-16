# Functional calibration feedback decision

The actual cold run reached the permitted IPL3 calibration loop but returned
zero capacity. The model's always-perfect backing made the first manual
sample's accuracy one at logical CCI zero. The source therefore computed
weighted sum zero, rejected that device, and returned zero memory. PI then
correctly refused a destination beyond the installed 4 MiB. Actual register
capture confirmed the cartridge source was in bounds; an earlier static
analysis had incorrectly interpreted a LUI immediate and is withdrawn.

D-03 explicitly permits versioned deterministic software readback derived from
the permitted algorithm. It forbids treating that choice as an analog
measurement or bypassing the guest by PC, ROM identity or output. The pinned
[initialization algorithm](https://github.com/n64brew/libdragon/blob/98154dafd5ac50e74ee09a5de1035d24a53f54f0/boot/rdram.c)
remains unmodified.

| Choice | Consequence | Disposition |
| --- | --- | --- |
| Versioned zero-origin feedback | Manual CCI zero hides read output; nonzero manual codes and AUTO expose backing. AUTO reports requested logical CCI. Guest calibration and enumeration still execute. | Selected under D-03 |
| Read-sample latch | Adds read side effects and a new sample-event assumption; initial enable at CCI63 precedes the actual CCI0 sweep. | Rejected as unnecessary state and ordering complexity |
| Leave cold calibration unresolved | Avoids modeled feedback but cannot provide the selected Tier B profile. | Fallback if software-model policy is withdrawn |

The configured read-only advisor compared these choices and checked the
zero-origin refinement. This is a software handshake, not a chosen physical
current or analog threshold. A single pure visibility owner covers bus reads,
cache hits/misses and both SP DMA read paths. It preserves backing and cache
contents, filters only consumed output, and applies to later mode changes too.
All controlling register/state fields are already serialized in state v13;
there is no hidden counter or host input. Old profiles bypass the new rule.

Independent native tests failed at fourteen intended assertions before the
change, then passed for both logical slots, uncached/wide reads, instruction
and data cache hits, DMA, AUTO readback, preserved backing and reset. Two older
raw-AUTO-echo assertions were updated to the newly specified logical readback,
with the intermediate report retained. Fast normal owner checks and the exact
inventory preceded instrumented checks. Both configurations passed, followed
by an actual bounded A/B comparison with exact equal hello bytes, authenticated
terminal exception and observed Tier B calibration/capacity families.

Original records and tested executables are retained in the ignored
`build/gate/06-05-feedback-*` artifacts and cold-hello attempt06. None of this
establishes physical topology, analog behavior or general compatibility.
