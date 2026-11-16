# Phase 6 RDRAM and boot decision

**Status:** Recommended default for Phase 6 planning; no hardware-accuracy claim.

## Options

| Option | Pros | Cons | Complexity | Recommendation |
|---|---|---|---|---|
| Add explicit Tier A and Tier B profiles over the same cleared open IPL3 bytes | Preserves existing warm-profile goldens and refusals, directly exercises the upstream cold branch by selecting `RI_SELECT=0`, keeps preset and protocol behavior separately attributable | Requires distinct profile and state identity for each policy, exact calibration readback remains a named model choice | Profile/reset, RI/RDRAM/MI bus state, digest and focused tests -- Risk: accidentally changing old-profile behavior or implying measured calibration | **Rec if Phase 6 must prove both modes while keeping prior profile IDs and state vectors frozen** |
| Change the existing warm profile to select preset or cold startup at runtime | Fewer descriptors, shares the existing fixture route | Makes one profile identity select different future machine behavior, invalidates its frozen state/refusal contract, weakens result attribution | Profile API, reset path, result identity and digest -- Risk: old evidence no longer names one deterministic machine | Rec only if the warm profile contract is explicitly retired; current project constraints rule this out |
| Build a separate authored cold-path IPL3 variant | Isolates cold-start experiments from the original fixture | The pinned program already contains a `RI_SELECT` cold branch, so a fork adds bytes and provenance without need; a patched fixture would not prove the existing open IPL3 path | New fixture, admission/rights evidence, source transformation and tests -- Risk: altered boot path and avoidable rights ambiguity | Rec only if the exact pinned bytes are shown unable to reach their own cold branch under the permitted HLE entry |

**Rationale:** Use two new, explicitly named profile identities for Tier A's prepared RI/RDRAM state and Tier B's cold register protocol, derived from the approved CPU software-compatibility profile. Keep every existing profile, initial-state vector, refusal matrix, and fixture identity unchanged. The pinned open IPL3 source already selects its cold initialization path when `RI_SELECT` is zero and the warm path when it is nonzero, so the same cleared boot bytes should cover both cases; that source-level conclusion still needs the bounded early guest-startup experiment before expanding native RDRAM support.

## Decision contract

- **Boot/profile identity:** Retain the existing warm-entry profile as a historical regression. Add distinct Tier A and Tier B descriptor IDs/policy strings, each binding the same HLE-PIF route, open IPL3 identity, 4 MiB NTSC cartridge-only topology, no Expansion Pak, and approved CPU compatibility/timing policy. The A descriptor selects the named prepared configuration; the B descriptor selects cold protocol startup (`RI_SELECT=0`). Use explicit capability/policy predicates, not numeric model-version ordering, to select new RI behavior. Give new state fields a new serialization identity; do not rewrite old descriptor hashes, state bytes, or refusal effects.
- **Tier A:** Treat the already qualified nonzero-`RI_SELECT` path as the preset reference, but define its new profile's register readback explicitly. The old warm profile remains byte-for-byte and behaviorally frozen, including its narrow absent-memory probe.
- **Tier B and topology:** First execute the same rights-cleared, byte-pinned IPL3 artifact with the deterministic HLE seed that takes its cold branch. Model only the 4 MiB first-profile memory and the device-selection/broadcast behavior the pinned routine actually exercises. Derive the minimum logical device slots from that path's observed register transcript; do not infer chip count from capacity alone or claim a physical module arrangement. Keep 8 MiB/Expansion Pak support deferred. If the routine probes above 4 MiB, specify each reached address, width, read result, and write effect as a bounded profile rule; refuse every unlisted span without aliasing it into installed RAM.
- **Calibration and readback:** Nintendo's Programming Manual §6.10 describes boot code's aggregate responsibility to initialize RDRAM, but does not establish exact post-IPL2 RI/RDRAM state or analog calibration readbacks. The pinned software's calibration loop and register accesses are software expectations, not hardware measurements. Represent required readback behavior with a versioned, deterministic software model derived from the source path and capacity checks; label it as modeled/unverified. Do not copy numeric constants from another emulator or invent an analog calibration value. If the cold branch cannot be made to complete without an ungrounded value, report that exact unresolved model assumption and keep it outside any silicon-accuracy claim.
- **MI repeat mode:** Extend MI/RDRAM mode semantics only under the new profile policy. Store mode and device-selection state in the canonical machine digest and make the accepted command sequence explicit. Do not broaden the old warm profile's accepted writes or reinterpret its current MI reset policy.
- **Timing:** Use the already approved versioned CPU software-compatibility profile and implement Phase 6 T1 Count/Random costs as a separate named timing policy. Keep `timing`, `cycle`, and `cop0hazard` evidence separate; T2 remains deferred. A suite group result does not define the timing tier.
- **Rights and lanes:** Continue using only the existing cleared open IPL3 and authored hello for the public smoke path. Keep the pending external diagnostic fixture local-only under its existing manifest restrictions; do not put it in public CI, upload it, or commit built ROM bytes before the recorded rights decision changes. D2 must resolve before enabling hosted diagnostic lanes. The other diagnostic groups can still be represented as classified, skipped/blocked-by-rights, or executed-local-only according to their saved status; never convert unavailable evidence into a pass.

## Evidence boundaries and controls

| Boundary | Positive evidence | Negative evidence |
|---|---|---|
| Startup path | The pinned asset is admitted; Tier A reaches authored hello; Tier B starts at cold-branch entry, retires the RI/RDRAM setup, reaches authored hello, and emits the exact same ISViewer bytes as Tier A | Altering the boot identity, using an unsupported cold profile, failing the calibration loop, or failing to reach the manifest-bound guest stop cannot classify as hello success |
| Protocol/state | Trace the source-reached RI/RDRAM/MI operations, mode transitions, device IDs/broadcasts, readbacks, memory test, and capacity result; the full post-run digest includes all future-affecting register state | Unsupported address/width/sequence and bad readback controls leave latches, installed RAM, MI state, scheduler state, and the full digest unchanged; no unlisted address above 4 MiB aliases installed RAM |
| Timing and suite | Count/Random assertions cite their expectation basis and the named T1 cost table; report every base group by lane with pinned total and separate outcome classes | Include wrong-profile/altered-delay and incomplete-result controls; do not pass on process exit, output hash alone, missing group, timeout, or fixture setup failure |

The first implementation evidence should be a bounded, headless startup attempt with the exact cleared IPL3 and authored hello, starting with the Tier B seed before a long native RDRAM expansion. Capture the first cold-path register transcript and guest stop; then implement only the missing accesses that transcript and the existing source contract establish. Do not use another emulator's output as the expected value. This is a software-model experiment, not a physical calibration test.

## Existing code and artifacts to reuse

- `core/t64_machine.c`: profile descriptors, reset, and prepared boot entry; current boot entry sets `RI_SELECT=1` for the existing profiles.
- `core/t64_bus.c`: the 4 MiB memory extent, unsupported RDRAM-register window, and current RI-window routing.
- `core/t64_warm_devices.c`: current RI implementation is limited to `RI_SELECT`, plus the exact absent-RDRAM word rule; the cold protocol belongs behind new profile-scoped behavior.
- `core/t64_internal.h` and `core/t64_digest.c`: per-machine guest-device state and profile-specific digest/state encoding; extend only new profile state and preserve old vectors.
- `core/t64_mi.c`: canonical MI pending/mask behavior; extend the separate new MI repeat-mode state only where the cold protocol requires it.
- `core/tests/test_profile.c`, `core/tests/test_warm_devices.c`, and `core/tests/test_boot_entry.c`: closest profile, refusal, device, and actual boot-path controls.
- `scripts/build-boot-fixtures.sh`, `fixtures/boot-contract.json`, `fixtures/hello/hello.S`, and `fixtures/manifest.json`: reuse the existing pinned IPL3 verification/recipe, authored output fixture, and rights boundary; do not introduce a downloaded diagnostic ROM into core or hosted smoke builds.

## Risks, unknowns, and reversal evidence

- **Unknown hardware facts:** Exact IPL2-to-IPL3 register values, physical device topology, analog calibration result, and hardware timing are not established by the current primary-source index. This decision avoids claiming them.
- **Source-level inference:** Existing Phase 3 source analysis says the pinned compat IPL3 has warm/cold branches keyed by `RI_SELECT`; the current implementation has only the warm setup. The first Tier B startup must verify the exact compiled bytes reach the source-described cold branch.
- **Suite rights:** Hosted diagnostic use remains gated by the recorded rights decision. That does not block the authored hello and RDRAM implementation work.
- **Reverse this recommendation only if** the exact pinned artifact does not reach the cold branch from the authorized HLE-PIF entry, a primary technical source supplies conflicting device/readback requirements, or bounded startup controls show the chosen logical topology cannot support the 4 MiB path. Prefer refining a named software policy; do not promote to hardware accuracy without independent primary evidence or separately authorized physical measurements.

## Canonical references

- [Phase 6 roadmap and BUS-03 criteria](../../ROADMAP.md#phase-6-cold-boot-cpu-timing-and-full-base-suite)
- [Open IPL3 warm-entry limits](../../../docs/spec/boot-limits.md)
- [Provisional machine contract](../../../docs/spec/machine-contract.md)
- [Pinned boot contract and evidence boundary](../../../fixtures/boot-contract.json)
- [Phase 3 source-path decision](../03-first-headless-rom-through-open-ipl3/03-DECISION-B.md)
- [Phase 3 primary evidence index](../03-first-headless-rom-through-open-ipl3/03-PRIMARY-EVIDENCE-INDEX.md)
- [CPU timing tiers](../../../docs/adr/0003-cpu-timing-tiers.md) and [approved CPU compatibility decision](../04-complete-vr4300-cpu/04-DECISION-CPU.md)
- [Fixture rights manifest](../../../fixtures/manifest.json)
- [Nintendo 64 Programming Manual v5.2 §6.10](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/pro-man/pro06/06-10.htm): aggregate boot responsibilities; not evidence for specific calibration constants or custom IPL3 state.
- [Nintendo SDK `rcp.h`](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm): register map/command definitions cited by the existing machine contract; not a calibration measurement.
