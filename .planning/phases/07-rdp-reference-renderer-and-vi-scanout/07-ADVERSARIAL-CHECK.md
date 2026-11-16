# Phase 7 Adversarial Research

**Scope:** Independent check of the proposed Phase 7 discussion recommendation. Read-only review of the current roadmap, requirements, v1.1 research, project policy, and relevant code seams. No oracle, build, test, guest, or hardware run was performed.

## Decision under review

Proceed with a dependency-free, clean-room C17 RDP and VI in the existing canonical machine. First prove one accepted, rights-cleared
authored DP fill through the actual headless runner: RDP writes RDRAM, VI scans it, runner retrieves raw pixels. Check the RDRAM words and
frame bytes separately against independently derived expected literals; include a wrong-pixel control that fails at the pixel assertion. Do
this before long suites. Keep VID-04 blocked until both its authority and asset gates resolve. This is a software-path acceptance slice, not
conformance or hardware-accuracy evidence.

## Evidence labels and current boundaries

- **[OWNER / REQUIREMENT]** The roadmap requires VID-01–06, a raw runner path, independent pixels, a wrong-pixel control, and explicitly
  keeps VID-04 blocked while D5 and the parallel-rdp file/asset audit are unresolved. See [roadmap Phase
  7](../../ROADMAP.md#phase-7-rdp-reference-renderer-and-vi-scanout) and [requirements](../../REQUIREMENTS.md#rdp-and-vi). - **[PRIMARY
  SOURCE]** Existing project research cites Nintendo's RDP programming and Full Sync sections and VI overview. These support device
  responsibilities and documented command/register semantics; the archived manuals are not measured Tendies64 behavior. See [RDP
  programming](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro12/index.htm), [RDP
  synchronization](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro12/12-02.htm), and [VI
  overview](https://jrra.zone/n64/doc/pro-man/pro30/30-01.htm). - **[CODE FACT]** Current `core/t64_scheduler.c` dispatches queued events at
  guest ticks; `core/t64_internal.h` already separates the observer output cursor from machine state. `core/t64_bus.c` still classifies DP
  and VI windows as unsupported, and RSP DP register operations explicitly refuse. The proposed device path is new behavior, not already
  established by the skeleton. - **[CODE FACT]** Current machine profiles, serialized identity, digest, and refusal behavior are explicit
  (`core/t64_machine.c`, `core/t64_digest.c`). Existing output state is not a raw-video queue. Reuse of these seams requires new owned state
  and contracts; their existence alone does not prove the output path. - **[INFERENCE]** A positive fill path plus a same-path wrong-pixel
  control is the cheapest useful discriminator for whether the RDP→RDRAM→VI→runner evidence chain is actually connected. It cannot
  distinguish all shared misunderstandings of the manual from silicon behavior. - **[UNKNOWN]** Exact command timing, RDP/VI bus contention,
  video-domain clock conversion, coverage/hidden-bit behavior, and undocumented/undefined memory results have not been established for this
  profile. Do not turn an invented deterministic policy into a timing or hardware fact.

## Evidence approaches

| Option | Pros | Cons | Complexity | Recommendation |
|--------|------|------|------------|----------------|
| Authored, independently calculated vectors through real consumer | Inspectable expected literals, rights-cleared inputs, identifies RDRAM versus VI/runner faults, wrong-pixel control tests assertion reachability | Narrow vectors can share a mistaken spec interpretation, a pass says nothing about untested modes, regression pins only prove repeatability | Existing C17 core, fixture inventory, runner seam -- Risk: expected values accidentally derived from production or protected data | **Rec for initial acceptance and each authored mode; label as software/spec-derived evidence** |
| Authored metamorphic/property checks alongside literal vectors | Broadly probes invariants and edge combinations, useful for clipping/partition/order/refusal boundaries without imported expected data | Can miss common-mode errors, invariants may be weaker than requirements, randomized generators need deterministic bounds and independent reachability evidence | Extends authored harness and source-derived case inventory -- Risk: green fuzz/property counts hide unvisited required command classes | **Rec if bounded generators add cases after a literal baseline; never substitute for per-mode expected results** |
| Out-of-tree emulator differential comparison | Can expose disagreements across many cases and implementations | Angrylion-Plus execution/data use is stopped by unresolved D5; parallel-rdp test files/assets are not audited; agreement is emulator-to-emulator, not hardware truth | Separate pinned tool/assets and local evidence path -- Risk: unauthorized use/derived data or mislabeled VID-04 completion | **Rec only after authority and per-file/asset gates resolve; VID-04 stays blocked meanwhile** |

## Adversarial checks on the proposed sequence

1. **Prove the test is reached before trusting its pixels.** The authored fixture must be admitted and the DP operation accepted through the
   normal headless runner, with semantic progress showing the intended command executed. Require an exact discovered/registered test
   inventory and the required command-class denominator; a green but filtered CTest selection or a helper-only call is not phase evidence.
   The wrong-pixel control must reuse the successful setup, alter only one expected literal, and fail specifically at the corresponding
   pixel assertion (not at setup, rights, address, or timeout). 2. **Keep the first path layered.** Assert authored command effects in RDRAM
   before VI conversion, then independently assert the raw frame before any host scaling/filtering. A final image hash alone can hide a
   shared packer/stride error, and a VI-only mismatch cannot locate an RDP write fault. Report first differing address/coordinate and bytes,
   not only pass/fail or a whole-frame digest. 3. **Full Sync is a scheduled completion boundary.** Preserve command order and make
   completion/MI DP interrupt a scheduler-owned effect. Test pending and masked DP interrupt behavior through existing MI observables,
   including command-before-FullSync and an ordering/tie boundary. A command submission or immediate register write is not proof the list
   completed. Existing references justify synchronization/notification semantics, but not a guessed cycle count; label modeled latency as
   unknown unless separately sourced. 4. **Make VI phase partition-invariant.** Use an explicit rational video-domain phase/accumulator for
   scanline/frame cadence and include future-affecting phase in canonical machine state/digest. Check equivalent whole versus split advances
   and equal-tick order. This is a deterministic model recommendation; exact source clock, phase origin, and physical timing accuracy remain
   unproven. Do not assume a fixed 320×240 frame or silently reset phase on host drain. 5. **Specify raw-output bounds before claiming
   capture.** Keep raw frame production, queue capacity, refusal/full behavior, and host drain cursor distinct. A full output buffer must
   not silently overwrite, duplicate, or discard frames or let host polling change guest time/digest. Compare output bytes and machine
   digest across different drain cadence and advance partitions; exercise full/resume or an explicit refusal postcondition. The existing
   observer cursor pattern is relevant precedent, not an implemented video-output guarantee. 6. **Name undefined policies without laundering
   them into facts.** Give noise seed and undefined-memory color explicit deterministic policy names and bind only semantics that actually
   change to the affected profile behavior. Prove repeatability across instances/allocations and host pacing; avoid uninitialized C memory,
   allocator contents, wall-clock entropy, or compiler-dependent behavior. A stable chosen value is an emulator policy, not a recovered
   hardware value. State coverage/hidden-bit handling and undocumented RAM reads as unknown until source-backed or separately measured. 7.
   **Treat rights as fixture provenance, not just a hash.** Author command words, data, and expected literals from cleared sources with a
   reviewable derivation and fixture manifest. Do not copy upstream test code/vectors, ROM assets, emulator output, or derived protected
   data. Keep Angrylion-Plus and parallel-rdp outside execution/use until their specific gates resolve; neither a MIT top-level license nor
   local-only storage substitutes for the required audit. Do not publish oracle-derived expectations or results under current D5. 8.
   **Version concrete behavior, not each device.** Preserve established profile identities, serialized bytes and refusal behavior where
   semantics are unchanged. Add only the capability/policy/state version fields required by concrete RDP/VI behavior under the already
   approved compatibility policy; avoid one profile per device or a profile explosion. If an old profile's accepted operations, timing,
   digest domain, or output changes, either preserve its old semantics or give the changed composite behavior an explicit versioned
   identity. Record rationale and reversal evidence. 9. **Bound the claim to its evidence tier.** The early fill establishes one authored
   software path only. It does not discharge VID-01's remaining copy/triangle/combiner/blender/coverage/Z modes, VID-02 timing, VI
   filtering/interlace, undefined-policy evidence, or VID-04. Authored expected values and later regression pins must never be labeled
   differential conformance. Keep VID-04 explicitly incomplete until D5 authority and the parallel-rdp per-file/asset audit are both cleared
   and a separately identified comparison is actually run.

## Finding

The recommendation survives adversarial review with those guards. The cheapest discriminating control is an accepted authored fill through
the actual runner, independent literal checks at both RDRAM and raw-frame boundaries, and a same-path wrong-pixel mutation that reaches and
fails the intended comparator. Run that control before broad suites; keep scheduler timing, VI phase, output backpressure, full command
coverage, and rights qualification as separately evidenced work. The current documents and code support proceeding with native/authored
implementation, but do not supply new timing facts or authorization to resolve VID-04 by assumption.
