# Phase 3 source coverage and dependency audit

Planning coverage only; no Phase 3 behavior has passed. Discovery reuses the complete Phase 3 research and tracked C17 patterns; no new library/SDK/service research is needed. The binutils/linked-byte/HLE numeric qualification is a bounded execution task before ISA expansion. Graph context is disabled. No agent skills or project-local skill directories were configured. Estimate calibration returned factor 1, zero samples, confidence low; plan raw/calibrated estimates are equal.

## Four-source coverage

| Source | ID | Required outcome/constraint | Plan/task | Status |
|---|---|---|---|---|
| GOAL | Goal | Actual hello through HLE-PIF and pinned IPL3, exact ISViewer and real pass/equal cross-host state | 01–08 | COVERED |
| GOAL | SC1 | Primary source/hash-only evidence and citations for every hardware claim | 01/1,01/3,08/2 | COVERED |
| GOAL | SC2 | Fixed reset/profile/input/bound real run, exact text, written byte semantics and scheduled trace | 04/1–3,06/1–3 | COVERED |
| GOAL | SC3 | IPL3 SHA256/MD5/revision/license/owner and no protected bytes in repo/artifacts | 01/1–3,07/1,08/1–2 | COVERED |
| GOAL | SC4 | Empty output boot-fail and explicit PIF/CIC/timeout/cold limits | 01/3,06/1,07/1,08/2 | COVERED |
| GOAL | SC5 | Two OS/compiler real runs with equal canonical state | 07/2,08/1 | COVERED |
| GOAL | SC6 | Actually admitted bytes match supervisor identity under synchronized source replacement | 05/1–2,06/2–3 | COVERED |
| REQ | BOOT-01 | Actual owned IPL3 CPU entry/retirement and loader-to-hello | 01/1,02/1–2,03/1–2,06/2 | COVERED |
| REQ | BOOT-02 | Pinned bytes/digests/rights/maintainer | 01/1–2,05/1,07/1,08/1–2 | COVERED |
| REQ | BOOT-03 | Prepared warm scope and PIF/CIC/timeout limits | 01/3,03/1–2,06/1,08/2 | COVERED |
| REQ | BOOT-04 | Rights-cleared authored hello, actual exact output | 01/2,04/1–2,06/2–3,07/1 | COVERED |
| REQ | RUN-01 | Fixed reset/profile/canonical input/bounded execution/real result | 04/3,05/1–2,06/1–3 | COVERED |
| REQ | RUN-03 | Real no-output halt and bound never pass | 06/1–3,07/1 | COVERED |
| REQ | RUN-05 | Written exact ISViewer byte protocol and bounded copy seam | 04/1–2,07/1 | COVERED |
| REQ | PROF-05 | Equivalent guest state across partition/interleave/trace and OS/compiler | 02/1–2,03/1–2,07/2,08/1 | COVERED |
| RESEARCH | Artifact baseline | Committed generated 4096-byte licensed input; original compiler/signer unknown, no source-regeneration prerequisite | 01/1–2 | COVERED |
| RESEARCH | Qualification | Actual linked bytes/disassembly, indirect-target uncertainty and tool/source pin precede CPU expansion | 01/1,02/1–2 | COVERED |
| RESEARCH | Warm policy | Numeric software HLE tuple, exact linked entry, profile 2/state 6, all future state/goldens | 01/1–3,07/2 | COVERED |
| RESEARCH | CPU | Only inventory-reachable MIPS III integer/delay/load-store/KSEG/CP0/CACHE; unsupported stop | 02/1–2 | COVERED |
| RESEARCH | Devices | Enumerated RI/reset/probe/zero-source plus checked existing delayed DMA; no RSP/cold calibration | 03/1–2 | COVERED |
| RESEARCH | Fixture | Authored assembly/linker/flat packer, pinned binutils only, exact transcript provenance | 01/2 | COVERED |
| RESEARCH | Output/input | 512-byte ISViewer/4096 total, exact lane/count/repeat/copy; 65536-byte/128-event canonical trace-only schedule | 04/1–3 | COVERED |
| RESEARCH | Attestation | Private immutable copies/unlinked inherited descriptors, same protected input schedule, zero-launch faults | 05/1–2,06/2–3 | COVERED |
| RESEARCH | Result | New strict real v2, unchanged synthetic v1/model, exact CPU+text pass and six outcomes | 06/1–3,07/1 | COVERED |
| RESEARCH | Validation | 17 exact new names, preserve 73/75 baseline, producer dependencies, negative controls/current sanitizers | 01–08 | COVERED |
| RESEARCH | Hosted/security | Existing two lanes, early GCC, truthful build IDs, rights/PII/sanitized evidence, ASVS1 high blocks | 01/3,08/1–2 | COVERED |
| CONTEXT | D-10 | Locked decision implemented verbatim in cited actions | 04/2–3,06/2–3,07/2 | COVERED |
| CONTEXT | D-11 | Locked decision implemented verbatim in cited actions | 05/1–2,06/2–3 | COVERED |
| CONTEXT | D-12 | Locked decision implemented verbatim in cited actions | 06/1–3 | COVERED |
| CONTEXT | D-13 | Locked decision implemented verbatim in cited actions | 06/1–3,07/1 | COVERED |
| CONTEXT | D-14 | Locked decision implemented verbatim in cited actions | 01/3,07/1–2,08/1–2 | COVERED |
| CONTEXT | D-15 | Locked decision implemented verbatim in cited actions | All objectives; routine reversible choices and existing owner stops | COVERED |
| CONTEXT | D-01 | Locked decision implemented verbatim in cited actions | 01/1–2 | COVERED |
| CONTEXT | D-02 | Locked decision implemented verbatim in cited actions | 01/1–2,08/1 | COVERED |
| CONTEXT | D-03 | Locked decision implemented verbatim in cited actions | 01/1,01/3,08/1–2 | COVERED |
| CONTEXT | D-04 | Locked decision implemented verbatim in cited actions | 01/1–3 | COVERED |
| CONTEXT | D-05 | Locked decision implemented verbatim in cited actions | 01/1,02/1–2 | COVERED |
| CONTEXT | D-06 | Locked decision implemented verbatim in cited actions | 01/1,02/1–2,03/2,07/2 | COVERED |
| CONTEXT | D-07 | Locked decision implemented verbatim in cited actions | 01/1,03/1–2 | COVERED |
| CONTEXT | D-08 | Locked decision implemented verbatim in cited actions | 01/2,07/1 | COVERED |
| CONTEXT | D-09 | Locked decision implemented verbatim in cited actions | 04/1–2,07/1 | COVERED |

Exclusions: only CONTEXT Deferred Ideas and research items explicitly assigned to later phases (full ISA/TLB/COP1, RSP engine, RI cold calibration, video/audio/controller consumption/saves/replay/SDK/new variants). No assigned requirement or success criterion is omitted.

## Dependency graph and wave ownership

| Wave/plan | Needs | Creates | Coupling/ordering |
|---|---|---|---|
| 1/03-01 | Verified Phase 2 core and primary evidence index | Qualify and execute the pinned warm entry | Leading actual-entry tracer before expansion |
| 2/03-02 | 03-01 | Expand the qualified CPU path through integer, CP0 and cache work | Shared checked inventory/build tree and/or core/runner CMake/state/digest files require sequential execution |
| 3/03-03 | 03-02 | Run the actual warm reset, capacity probes and delayed DMA | Shared checked inventory/build tree and/or core/runner CMake/state/digest files require sequential execution |
| 4/03-04 | 03-03 | Capture exact guest bytes and schedule canonical timed input | Shared checked inventory/build tree and/or core/runner CMake/state/digest files require sequential execution |
| 5/03-05 | 03-04 | Attest exact fixture and schedule bytes through inherited descriptors | Shared checked inventory/build tree and/or core/runner CMake/state/digest files require sequential execution |
| 6/03-06 | 03-05 | Produce and check an attested actual-guest result | Shared checked inventory/build tree and/or core/runner CMake/state/digest files require sequential execution |
| 7/03-07 | 03-06 | Prove real failure classification and deterministic state | Shared checked inventory/build tree and/or core/runner CMake/state/digest files require sequential execution |
| 8/03-08 | 03-07 | Verify both hosted compilers and bounded current-source sanitizers | Shared checked inventory/build tree and/or core/runner CMake/state/digest files require sequential execution |

All plans are autonomous. No same-wave file overlaps exist because shared build output, source identity/digest, fixture state and exact inventory serialize these slices; configured non-worktree execution must not run concurrent builds against the shared gate/asan trees. Each task has at most five modified files and immediate automated checks. No newly surfaced one-way choice: real v2 is already settled by D-12. No auth/session, external runtime API, ORM, frontend or AI integration.

## Prominent retained judgment flags

- Warm-entry/source-policy evidence must not be sold as authentic IPL2/cold boot/hardware accuracy/broad compatibility.
- Timed trace scheduling must not be described as controller consumption.
- Artifact identity must not be presented as rights clearance or proof of IPL3 source regeneration.

These are non-authoritative judgment flags; no human validation or owner acceptance is claimed. AUTONOMY permits completion with these flags only after automated requirements pass and concrete rights/authority/evidence stops are clear.
