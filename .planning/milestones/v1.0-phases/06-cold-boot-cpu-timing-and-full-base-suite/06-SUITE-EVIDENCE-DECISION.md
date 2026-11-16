# Phase 6 suite evidence decision

## Scope

Design the source-derived inventory, execution lanes, qualification lifecycle, and Phase 6 result claim for the full default BASE set. External source names, member identities, ROM identities, hashes, detailed counts, and raw or canonical outcomes stay in the ignored owner-only evidence area. The exact local inventory partition is in the private appendix, which must not be copied into tracked docs or chat.

## Source-derived context

The accepted local executor defaults to its BASE feature closure, which includes its quick registrations. BASE selects CPU and RDP level groups; the current CPU-only projection's non-CPU omission cannot serve as Phase 6's denominator. The executor has separate timing, cycle, and CP0-hazard level toggles, a quirk level, and individually enabled stress features. Its manifest also declares an experimental RDP feature, but only source registrations actually selected by that feature belong in an experimental lane. Keep timing, cycle, and CP0-hazard informational, keep experimental-only registrations out of the required gate, and run stress in a scheduled local lane. Determine each member from the pinned source and feature closure, not README totals or a hand-maintained list.

The executor runs a registration once when its value list is empty, and repeated display names remain distinct by source order. Preserve both behaviors in the inventory. Keep every required row when its device or asset is unavailable, with a bounded per-member reason; a blocked or unexecuted row is never a pass or an omission. In particular, validate font, bitmap, framebuffer, boot, and device closure before compilation. Do not compile a path that requires an uncleared asset.

## Options

| Option | Pros | Cons | Complexity | Recommendation |
|--------|------|------|------------|----------------|
| Phase-owned C inventory and aggregate, with immutable per-run context records | One source-anchored denominator covers CPU, RSP, RDP, and other BASE groups; C remains the only completeness and authentication authority; preserves accepted CPU/RSP component contracts and exact original member records; routine harness corrections create a new context record without changing the protocol version | Adds a phase-wide C join and controls; the join must preserve every source index and exact profile/build/input identity; component summaries alone are insufficient inputs | Source projection, C protocol/aggregate, runner dispatch and controls -- Risk: falsely complete results if context data can alter membership | Rec if existing component contracts remain stable and the C join can verify each original member record without recomputing completeness in Python |
| One stable phase-wide C member protocol used directly by all component runners | A single interface for inventory, per-member batches, and group status; one source-anchored membership check and one aggregation authority | Requires changes to existing CPU/RSP adapters and result boundaries; larger review surface; risks invalidating accepted component evidence if old and new records are mixed | CPU/RSP adapters, C protocol and controls, source projection -- Risk: protocol migration breaks reusable qualification and artifact bindings | Rec if existing component records cannot be joined losslessly under one phase-level C owner |
| One monolithic BASE image and run | Simple launch shape; one boot path; no cross-component adapter join | Cannot compile or use any required member whose asset closure is uncleared; one timeout can hide later member outcomes; discards the existing per-member qualification and bounded recovery path | Source overlay, guest build, runner, evidence parser -- Risk: rights violation or lost member-level failure evidence | Rec only after every selected asset and device path is cleared and member-level bounds and outcomes remain observable |

**Rationale:** Use the phase-owned C aggregate with immutable local context records as the working design while CPU and RSP component formats remain accepted. Its inventory must be independently anchored to the pinned executor's actual default feature closure and BASE level dispatch, and it must include the RDP and other device rows selected by that closure. Keep existing qualified source, tools, locks, boot bytes, and test images when their exact audited inputs are unchanged; rebuild the emulator consumer and rerun affected members when production content or compiler/build policy changes. The stable protocol version changes only when its meaning changes. Routine verifier or runner corrections receive a new immutable context record, full-entry authored controls, and a qualified artifact binding when needed, rather than another hard-coded version constant. If the C join cannot authenticate member outputs without duplicate completeness logic, switch to the direct phase-wide member protocol.

## Qualification and run sequence

1. Seal the entire pinned source inventory before compilation. Derive default features, aliases, level dispatch, repeated-name ordinals, and zero-value execution from the accepted source and receipts. Record exact per-group and per-lane counts only in private evidence. Never reduce the denominator to make a lane pass.
2. Make the C verifier the sole owner of source membership and final per-member aggregation. Python may prepare bounded commands and collect original outputs, but it must not calculate a second completeness result. Every required member gets exactly one authenticated result or an individual bounded reason.
3. Before making a run directory, run the full independent pre-use verifier through its normal dispatch. Authored accept/reject controls must cover that complete entry point, accepted source membership, reduced or resealed membership, identity mismatch, blocked assets, and setup/write failures. The source anchor must be independent from the inventory being checked.
4. Reuse unchanged accepted inputs. Before any later rebuild, archive the exact executable together with its binding. A source revision annotation is provenance; `T64_BUILD_ID` remains tied to production content and compiler/build policy. Refresh provenance identity without repeating semantic checks when that closure is unchanged.
5. Start one headless, qualified real member early, before the long Phase 6 implementation sequence. Keep sanitizer and fuzz work separate from wall-capped diagnostic runs. Preserve current limits and do not widen or retry a timed-out attempt: RSP remains at its existing 10M guest ticks, 16-tick advances, 10-second child bound, 12-second outer deadline, and 4096-byte capture; CPU uses its existing explicit bounds. Diagnose a failed bounded case, fix its cause, then make only the justified retest.
6. Keep diagnostic execution local until the existing publication/right gate is resolved. The project-authored hello fixture remains the permanent public smoke lane. A full inventory, an aggregate, or a test pass is evidence about the stated software fixture set only.

## Claim and release boundary

Build any Phase 6 result statement from the claims template after the exact final run. Bind each reported result to its exact machine profile, build identity, evidence tier, fixture-manifest entry, and oracle. State omitted lanes and blocked rows clearly; do not infer hardware accuracy, full compatibility, another profile, or general game support. D6 remains a release gate only; this decision does not select a software version or release channel.

## Reversal criteria

- If the accepted pinned source's default feature or BASE-level dispatch differs from the read-only source audit, stop inventory sealing and derive the complete partition again before any build or run.
- If source/tool/lock/asset/boot bytes or the selected feature closure change, requalify that closure; do not reuse the old qualification by relabeling it.
- If production content or compiler/build policy changes, rebuild and bind the exact new consumer, then rerun affected diagnostic members. If only the source annotation changes, refresh provenance and retain the tested binary identity.
- If the central C join cannot verify the original member identities and outputs without a second calculator, use the direct phase-wide C member protocol.
- If rights are newly established for a blocked asset, update only the affected source-derived member closure and qualification record before enabling its build or execution. This research authorizes no new asset use.

## Canonical references

- [Phase 6 roadmap criteria](../../ROADMAP.md#phase-6-cold-boot-cpu-timing-and-full-base-suite)
- [Autonomous workflow and evidence policy](../../AUTONOMY.md)
- [CPU compatibility profile](../../../docs/spec/cpu-compatibility.md)
- [CPU diagnostic contract](../../../docs/spec/cpu-diagnostic.md)
- [RSP diagnostic and bounds](../../../docs/spec/rsp-diagnostic.md)
- [CPU source qualification verifier](../../../scripts/verify-cpu-diagnostic-qualification.py)
- [CPU diagnostic build and run paths](../../../scripts/build-qualified-cpu-diagnostic.py), [CPU runner](../../../scripts/run-cpu-diagnostic.py)
- [RSP evidence checker](../../../scripts/check-rsp-evidence.py), [RSP qualification verifier](../../../scripts/verify-rsp-diagnostic-qualification.py)
- [Claims template](../../../docs/claims/TEMPLATE.md)
- [Authored hello fixture](../../../fixtures/hello/hello.S), [fixture manifest](../../../fixtures/manifest.json)
