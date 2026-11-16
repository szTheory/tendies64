# Architecture Patterns: Observable N64 Machine

**Domain:** Deterministic Nintendo 64 emulator core  
**Researched:** 2026-11-15  
**Scope:** v1.1 phases 7–9 (RDP/VI, AI/capture, SI/PIF/save/state/replay)

## Recommendation

Extend opaque per-instance `t64_machine` as the canonical guest machine. Keep
registers, pending work, timing phase, and save bytes there; route work through
the existing bus and `(guest_tick, sequence)` scheduler. Device modules must
not introduce parallel machines, host clocks/callbacks, or independent order.

Make profile identity the capability/behavior boundary. Add versioned device
policies/profiles; preserve old IDs, serialized bytes, hashes and digest
domains. Apply the additive policy in one schema change: extend
`descriptor_version` serialization and add a machine/state version. Keep the
public C surface provisional through Phase 9; Phase 10 owns ABI freeze.

## Existing Seams (Code Observations)

| Seam | Current behavior | Consequence |
|---|---|---|
| [`t64_internal.h`](../../core/t64_internal.h) | One machine owns RAM, CPU/RSP, PI/SP, profile, time, queue, MI, device latches, output. | Add RDP/VI/AI/SI/PIF/save state here; separate host cursors. |
| [`t64_scheduler.c`](../../core/t64_scheduler.c) | Bounded FIFO by `(tick, sequence)`; events synchronize CPU time then dispatch. Reserve/commit can fail before mutation. | All delayed/cadence work joins this queue; test same-tick order. |
| [`t64_bus.c`](../../core/t64_bus.c) | DP/VI/AI/SI/PIF are explicit unsupported windows; PI, MI, SP have owned handlers. | Add profile-gated device dispatch; preserve explicit refusals elsewhere. |
| [`t64_machine.c`](../../core/t64_machine.c) | Named profiles, serialization/hash, reset, and feature gates are explicit. | Add profiles and lifecycle setup; do not broaden existing profiles in place. |
| [`t64_digest.c`](../../core/t64_digest.c) | Hashes semantic future state by model/state version; excludes trace and output-drain cursor. | Hash new future-affecting device state/media; exclude presentation and host cursor. |
| [`t64_observe.c`](../../core/t64_observe.c), [`t64_trace.c`](../../core/t64_trace.c) | Copy-out observers and bounded trace are outside execution state. | Reuse for diagnostics; specify raw A/V metadata and loss behavior separately. |
| [`t64_machine.h`](../../core/include/tendies64/t64_machine.h) | Profile, input and output operations exist but are provisional. | Evolve under versioned tests; do not promise ABI yet. |

The [machine contract](../../docs/spec/machine-contract.md) documents checked
bus spans, explicit refusal, FIFO order, identity and transactional admission;
these patterns do not prove unsupported-device hardware behavior.

## Boundaries and Likely Files

| Phase | Add | Modify or reuse |
|---|---|---|
| 7: DP/RDP + VI | `core/t64_rdp.c`, `core/t64_vi.c`; `core/tests/test_rdp.c`, `test_vi.c` | `t64_internal.h`, `t64_bus.c`, `t64_machine.c`, `t64_scheduler.c`, `t64_digest.c`, `core/CMakeLists.txt`; reuse RDRAM, MI, trace, authored fixture harness. |
| 8: AI + capture | `core/t64_ai.c`; `core/tests/test_ai.c`; runner capture writer | Same state/bus/scheduler/digest seams; `runner/t64_real.c` or a small runner module owns files and metadata. |
| 9: SI/PIF, saves, state/replay | `core/t64_si.c`, `core/t64_pif.c`, `core/t64_save_media.c`, `core/t64_state.c`; SI/save/state tests | `t64_internal.h`, input handler in `t64_observe.c`, bus/profile/reset/digest, `core/CMakeLists.txt`, runner replay serialization. Keep EEPROM and SRAM as distinct typed devices. |

Keep register ownership with its device; use the central bus for decode and
dispatch, and the central scheduler for delayed work.

## Data and Time Flow

```text
guest bus -> profile-gated device -> preflight/reserve/commit -> scheduler
  -> device memory/MI effects -> raw output copy-out -> host runner
```

**Code fact:** one monotonic guest tick and FIFO sequence order already exist.
**Recommendation:** DP completion, VI scan/interrupts, AI cadence/DMA and SI
completion all use that timeline. Convert video/audio domains with profile
ratios or rational phase accumulators; include phase in the canonical digest so
host advancement partitioning cannot add/drop device events. Host wall time is
presentation only. The CPU:RSP quantum remains profile identity.

**Primary-source facts:** SDK references describe FullSync/DP completion,
separate VI scanout, AI rate from video clock/DAC period, and SI/PIF DMA and
interrupts. These motivate shared guest time, not Tendies64 event costs/order.

## Ownership, Outputs, and Refusal

| Data | Owner | Digest/state export |
|---|---|---|
| Device registers/queues, timing phase, MI state, selected EEPROM/SRAM bytes | `t64_machine` | Yes: future-affecting guest state. |
| Raw framebuffer/sample blocks and profile/tick/index metadata | Copy-out capture seam; no callbacks | No, unless the buffer affects guest-visible future behavior. Keep cursors/loss separate. |
| Save/state/replay files and durable writes | Headless runner/host | Checked byte spans only; core performs no path or filesystem I/O. |
| Physical input mapping, scaling, playback, pacing | Frontend | Submit logical inputs at guest ticks; wall time never enters guest state. |

For bounded capture, recommend a deterministic pull queue and a resumable
`output-full` stop at a declared device-output boundary. The core must not call
the host or silently overwrite pending samples/frames. Drain copies bytes and
advances only an observer cursor. A later live-host drop mode must be explicit
and report loss; it cannot claim complete capture. A too-small drain buffer
reports required capacity and leaves data/cursor unchanged, as the current
output-drain seam does.

Define each refusal before the API freeze:

- Validate profile/device, widths/ranges, tick, event budget, sequence/token
  and output capacity before mutation.
- Queue/time exhaustion, invalid spans, unsupported device and failed import
  preserve guest state/time/digest. Semantic refusals may change a latch/error
  only under a named profile policy.
- Reserve resources before multi-step DMA; validate full state/profile/content
  before atomic import. Save bytes are canonical in-memory state; host
  persistence is explicit typed I/O, never an automatic flush.
- Stops resume at the same guest time; observers and presentation cannot move it.

PI/SP already demonstrate reserve-before-busy. Specify new raw-latch and error
postconditions per operation. ISViewer is guest-addressable communication
state; its digest treatment does not imply raw A/V is guest state.

## First Vertical Slice and Controls

Start with one authored rights-cleared path: DP list → fill RDRAM → FullSync/DP
pending → VI raw scanout → headless copy-out. Name renderer policies in profile.
Then add one AI cadence path, timed controller input, and selected save transfer
with state round-trip.

Before long suites, prove the consumer selects the profile, accepts one device
operation, advances guest time and retrieves output. Check old hashes/bytes;
same-tick CPU/DP/VI/AI/SI/MI order; event exhaustion; long-vs-split equivalence;
capture-full resume; and failed operations/imports preserving state and time.
Negative refusals alone do not prove a profile can accept work.

Do not use physical hardware, private ROMs, `nemu64-test`, or Angrylion-Plus in
this architecture work. D2 keeps those ROMs out of public CI; D5 keeps oracle
data/claims out-of-tree; D6 bars release. Rights and authored expected values
remain phase gates.

## Tradeoff and Reversal Trigger

| Approach | Decision |
|---|---|
| One event-driven machine with device modules | **Recommend:** fits existing ownership/order/digest seams and replay. |
| Per-device schedulers or host callbacks | Reject: hides event ties and host pacing in machine behavior. |
| Broaden old profiles or freeze API now | Reject: invalidates evidence; Phase 10 owns ABI. |

Reverse only if Phase 7 shows the existing integer tick cannot represent
required boundaries without changing qualified profiles. Add a named timing
profile and state domain; do not reinterpret old identities.

## Sources and Confidence

- **Code observations — HIGH:** linked core files above, [machine contract](../../docs/spec/machine-contract.md), and [v1.1 roadmap](../ROADMAP.md).
- **Register facts — MEDIUM:** archived Nintendo SDK [RCP](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm), [SI](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro26/26-01.html), [EEPROM](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/pro-man/pro26/26-06.htm), and [RDP sync](https://ultra64.ca/files/documentation/online-manuals/man/n64man/gdp/gDPSync.html) references. No manual text or expected data copied; verify rights before publishing derived vectors.

## Gaps

- Phase-specific research remains for RDP/VI timing, AI cadence/carry/queue
  edges, Joybus/EEPROM behavior, and state schema; Phase 10 must test output
  and state-file postconditions before freezing the public contract.
