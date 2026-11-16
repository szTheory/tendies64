# Phase 7 DP Command Admission, Completion, and State Ownership

**Decision scope:** Choose how DP display-list work enters the machine, when it is complete, and what busy/refused/unsupported cases may change. Retain C17, deterministic in-tree integer RDP, no new dependency, first real-consumer path `DP fill → RDRAM → VI → runner frame`, no ABI freeze, and existing profile identities.

## Evidence boundary

- **Primary-document facts:** SGI's *Nintendo 64 RSP Programmer's Guide* (cover v1.1 / relevant sections Revision 1.0, pp. 88–91, 100–103) describes separate RDP command DMA from DRAM/DMEM, 8-byte transfers, `CURRENT` advancing toward `END`, and buffered `START`/`END` updates. SDK `rcp.h` rev. 1.21 (1998-07-31) maps DP registers and busy/valid bits. Nintendo Online Manuals v5.2, RDP Programming §12.2.2 and `gDPFullSync`, says FullSync is the final frame command and its DP event follows completion of the last pipeline command. These facts do not establish Tendies64 event costs.
- **Current code observations:** [bus decode](../../../core/t64_bus.c) refuses DP registers; [RSP issue](../../../core/t64_rsp.c) records DP access unavailable; [scheduler](../../../core/t64_scheduler.c) reserves/commits bounded `(tick, sequence)` events; [MI](../../../core/t64_mi.c) owns DP pending `0x20`, mask, and IRQ line. Machine state/profile serialization owns device state and names timing, queue, and refusal policies.
- **Software-policy unknowns:** Primary references do not pin Tendies64 per-command/pixel tick costs, queue-full backpressure, same-tick CPU/RDP/VI ordering, VI latch point, unsupported-opcode outcome, or exact fill/copy/triangle/TMEM completion phases. Integer scheduler ordering is a project policy, not RDP clock accuracy.

## Options

| Option | Pros | Cons | Complexity | Recommendation |
|---|---|---|---|---|
| Synchronously decode and render the full list on `DPC_END` | Small path to first fill pixels, straightforward command-local refusal | Hides guest-time interleaving, cannot expose progressive `CURRENT`/busy state, FullSync can falsely interrupt before future asynchronous work | DP bus handler and renderer; little scheduler work — Risk: fake completion/order becomes embedded in output | Rec only for a throwaway fill prototype explicitly making no guest-time or completion claim |
| Scheduler-owned incremental decode with completion fences | Uses current queue/tie order, gives DP state a machine owner, separates command fetch from pixel/pipeline completion, supports later copy/triangle/TMEM without redefining FullSync | More latches and resume/refusal paths; costs and some busy transitions remain declared software policy | DP bus/device, scheduler, MI, profile/digest, runner — Risk: model timing may be mistaken for hardware timing | Rec if Phase 7's observable machine and later shared-device ordering remain in scope |

**Recommendation (reversible):** Use scheduler-owned decode. Admit through DP registers; keep DP pointers/status, RDP/TMEM state, pending work, and completion tokens in `t64_machine`; represent command fetch and pipeline completion as distinct events. FullSync may raise MI DP only after prior pipeline work completes; `CURRENT == END` alone is insufficient. Reconsider only if the actual-consumer slice shows no guest-time/device-order boundary; retain the scheduler boundary before adding asynchronous operations.

## Admission and completion policy to carry into planning

- Use a new explicit RDP/VI profile or versioned policy; preserve qualified profiles. Hash future-affecting DP state, queued work, and timing phase; make exact descriptor/state-version edits only with the concrete schema diff.
- Validate address space, bounds, alignment, complete commands, profile capability, and event capacity before committing DP latches/queue entries. Reserve first; refused register/list admission preserves tick, DP pointers/status, RDRAM, MI pending/mask, and digest.
- Busy means admitted or in-flight work, not error/completion. If bounded scheduler capacity blocks a later step, pause/resume at the same guest boundary or refuse before mutation. Do not clear busy, pass an unretired command, or synthesize FullSync/MI progress.
- For an unsupported opcode, stop at its address with no effects from that command or synthetic completion/DP interrupt; earlier completed commands remain committed. Actual hardware handling of all unsupported encodings is unresolved, so label this profile software policy.
- MI-mode DP-clear currently routes through warm/RDRAM-profile handling. Reuse MI source/mask/line state, while ensuring the new RDP profile clears DP pending without inheriting unrelated RDRAM-protocol semantics.

## Minimal actual-consumer preflight (proposal; not run)

Use the actual headless runner and selected new profile, not a renderer-only helper: admit one rights-cleared authored fixture, send its CPU-written RDRAM list through the DP bus path, advance through fill/FullSync, and obtain one profile-described raw VI frame. Independently compute expected pixels; inspect RDRAM and VI bytes separately. Observe `CURRENT`, DP pending, MI mask/clear, guest tick, and frame metadata. Proposal only; no implementation or hardware claim.

**Minimum negative controls:** (1) a deliberately wrong expected pixel fails; (2) bad bounds/alignment, unsupported command, and saturated event queue do not retire a command, jump pointers, write pixels, complete FullSync, or raise DP; compare digest/tick and DP/MI latches; (3) hold earlier modeled work pending at FullSync and require DP pending to remain low until its completion fence. Assert preservation for the refused command only; do not roll back earlier accepted work.

VID-04 remains blocked: D5 oracle authority is unresolved, so oracle use is not authorized; parallel-rdp assets need a per-file audit. No oracle/protected fixture was used; authored pixels cannot close differential evidence.

## Primary references

- [SGI, *Nintendo 64 RSP Programmer's Guide*, cover v1.1, Revision 1.0 sections, pp. 88–91, 100–103](https://ultra64.ca/files/documentation/silicon/SGI_Nintendo_64_RSP_Programmers_Guide.pdf)
- [Nintendo, Online Manuals v5.2, *RDP Programming*, §12.2.2 “Synchronization”](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/pro-man/pro12/12-02.htm); [Nintendo OS function reference, v5.1, `gDPFullSync`](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/n64man/gdp/gDPFullSync.htm)
- [Nintendo SDK `rcp.h`, revision 1.21, 1998-07-31](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm)
- [Nintendo OS function reference, Online Manuals v5.2, `osDpSetStatus` / `osDpSetNextBuffer`](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/n64man/os/osDpSetStatus.htm)
