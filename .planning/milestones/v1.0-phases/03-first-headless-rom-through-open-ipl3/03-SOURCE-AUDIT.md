# Phase 3 source coverage and dependency audit

Planning coverage only; no Phase 3 behavior has passed. Discovery reuses the complete Phase 3 research and tracked C17 patterns; no new library/SDK/service research is needed. The binutils/linked-byte/HLE numeric qualification is a bounded execution task before ISA expansion. Graph context is disabled. No agent skills or project-local skill directories were configured. Estimate calibration returned factor 1, zero samples, confidence low; plan raw/calibrated estimates are equal.

## Four-source coverage

| Source | ID | Required outcome/constraint | Plan/task | Status |
|---|---|---|---|---|
| GOAL | Goal | Actual hello through HLE-PIF and pinned IPL3, exact ISViewer and real pass/equal cross-host state | 03-01–13 | COVERED |
| GOAL | SC1 | Primary source/hash-only evidence and citations for every hardware claim | 03-01/1,03-03/1,03-13/3 | COVERED |
| GOAL | SC2 | Fixed reset/profile/input/bound real run, exact text, written byte semantics and scheduled trace | 03-02/1,03-06–08,03-10–11 | COVERED |
| GOAL | SC3 | IPL3 SHA256/MD5/revision/license/owner and no protected bytes in repo/artifacts | 03-01–03,03-12/1,03-13/1–3 | COVERED |
| GOAL | SC4 | Empty output boot-fail and explicit PIF/CIC/timeout/cold limits | 03-03/1,03-10/1,03-12/1,03-13/3 | COVERED |
| GOAL | SC5 | Two OS/compiler real runs with equal canonical state | 03-12/2,03-13/1–2 | COVERED |
| GOAL | SC6 | Actually admitted bytes match supervisor identity under synchronized source replacement | 03-09/1–2,03-11/1–2 | COVERED |
| REQ | BOOT-01 | Actual owned IPL3 CPU entry/retirement and loader-to-hello | 03-01/1–2,03-04/1–2,03-05/1–2,03-11/1–2 | COVERED |
| REQ | BOOT-02 | Pinned bytes/digests/rights/maintainer | 03-01/1,03-02/1,03-09/1,03-12/1,03-13 | COVERED |
| REQ | BOOT-03 | Prepared warm scope and PIF/CIC/timeout limits | 03-03/1,03-05,03-10/1,03-12/1,03-13/3 | COVERED |
| REQ | BOOT-04 | Rights-cleared authored hello, actual exact output | 03-02/1,03-06,03-11,03-12/1 | COVERED |
| REQ | RUN-01 | Fixed reset/profile/canonical input/bounded execution/real result | 03-07–11,03-13/1–2 | COVERED |
| REQ | RUN-03 | Real no-output halt and bound never pass | 03-10–11,03-12/1 | COVERED |
| REQ | RUN-05 | Written exact ISViewer byte protocol and bounded copy seam | 03-06–07,03-11,03-12/1 | COVERED |
| REQ | PROF-05 | Equivalent guest state across partition/interleave/trace and OS/compiler | 03-01/2,03-04–05,03-07–08,03-12/2,03-13/1–2 | COVERED |
| RESEARCH | Artifact baseline | Committed generated 4096-byte licensed input; original compiler/signer unknown, no source-regeneration prerequisite | 03-01/1,03-02/1 | COVERED |
| RESEARCH | Qualification | Actual linked bytes/disassembly, indirect-target uncertainty and tool/source pin precede CPU expansion | 03-01/1–2,03-04/1–2 | COVERED |
| RESEARCH | Warm policy | Numeric software HLE tuple, exact linked entry, profile 2/state 6, all future state/goldens | 03-01/1–2,03-03/1,03-12/2 | COVERED |
| RESEARCH | CPU | Only inventory-reachable MIPS III integer/delay/load-store/KSEG/CP0/CACHE; unsupported stop | 03-04/1–2 | COVERED |
| RESEARCH | Devices | Enumerated RI/reset/probe/zero-source plus checked existing delayed DMA; no RSP/cold calibration | 03-05/1–2 | COVERED |
| RESEARCH | Fixture | Authored assembly/linker/flat packer, pinned binutils only, exact transcript provenance | 03-02/1 | COVERED |
| RESEARCH | Output/input | 512-byte ISViewer/4096 total, exact lane/count/repeat/copy; 65536-byte/128-event canonical trace-only schedule | 03-06–08 | COVERED |
| RESEARCH | Attestation | Private immutable copies/unlinked inherited descriptors, same protected input schedule, zero-launch faults | 03-09/1–2,03-11/1–2 | COVERED |
| RESEARCH | Result | New strict real v2, unchanged synthetic v1/model, exact CPU+text pass and six outcomes | 03-10/1,03-11/1–2,03-12/1 | COVERED |
| RESEARCH | Validation | 17 exact new names, preserve 73/75 baseline, producer dependencies, negative controls/current sanitizers | 03-01–13 | COVERED |
| RESEARCH | Hosted/security | Existing two lanes, early GCC, truthful build IDs, rights/PII/sanitized evidence, ASVS1 high blocks | 03-03/1,03-13/1–3 | COVERED |
| CONTEXT | D-10 | Locked decision implemented verbatim in cited actions | 03-07–08,03-11,03-12/2 | COVERED |
| CONTEXT | D-11 | Locked decision implemented verbatim in cited actions | 03-09/1–2,03-11/1–2 | COVERED |
| CONTEXT | D-12 | Locked decision implemented verbatim in cited actions | 03-10/1,03-11/1–2 | COVERED |
| CONTEXT | D-13 | Locked decision implemented verbatim in cited actions | 03-10–11,03-12/1 | COVERED |
| CONTEXT | D-14 | Locked decision implemented verbatim in cited actions | 03-03/1,03-12/1–2,03-13/1–3 | COVERED |
| CONTEXT | D-15 | Locked decision implemented verbatim in cited actions | All objectives; existing explicit stops preserved | COVERED |
| CONTEXT | D-01 | Locked decision implemented verbatim in cited actions | 03-01/1,03-02/1 | COVERED |
| CONTEXT | D-02 | Locked decision implemented verbatim in cited actions | 03-01/1,03-02/1,03-13/1 | COVERED |
| CONTEXT | D-03 | Locked decision implemented verbatim in cited actions | 03-01/1,03-03/1,03-13 | COVERED |
| CONTEXT | D-04 | Locked decision implemented verbatim in cited actions | 03-01/1–2,03-03/1 | COVERED |
| CONTEXT | D-05 | Locked decision implemented verbatim in cited actions | 03-01/1–2,03-04/1–2 | COVERED |
| CONTEXT | D-06 | Locked decision implemented verbatim in cited actions | 03-01/1–2,03-04–05,03-12/2 | COVERED |
| CONTEXT | D-07 | Locked decision implemented verbatim in cited actions | 03-01/1–2,03-05 | COVERED |
| CONTEXT | D-08 | Locked decision implemented verbatim in cited actions | 03-02/1,03-12/1 | COVERED |
| CONTEXT | D-09 | Locked decision implemented verbatim in cited actions | 03-06–07,03-12/1 | COVERED |

Exclusions: only CONTEXT Deferred Ideas and research items explicitly assigned to later phases (full ISA/TLB/COP1, RSP engine, RI cold calibration, video/audio/controller consumption/saves/replay/SDK/new variants). No assigned requirement or success criterion is omitted.

## Dependency graph, split scope and wave ownership

| Wave/plan | Needs | Creates | Declared files / tasks |
|---|---|---|---|
| 1/03-01 | Verified Phase 2 plus primary index | Qualify and execute the actual warm-entry tracer | 7 / 2 |
| 2/03-02 | 03-01 | Build the authored flat hello from qualified tools | 5 / 1 |
| 3/03-03 | 03-02 | Register warm-entry evidence and coherent gate provisioning | 5 / 1 |
| 4/03-04 | 03-03 | Expand the qualified CPU integer, CP0 and cache path | 7 / 2 |
| 5/03-05 | 03-04 | Execute bounded warm devices, probes and delayed loader DMA | 8 / 2 |
| 6/03-06 | 03-05 | Capture exact bounded ISViewer guest bytes | 5 / 1 |
| 7/03-07 | 03-06 | Copy owned output and submit deterministic timed input | 5 / 1 |
| 8/03-08 | 03-07 | Parse canonical timed-input schedules and prove their trace | 5 / 1 |
| 9/03-09 | 03-08 | Attest immutable fixture and schedule descriptors | 7 / 2 |
| 10/03-10 | 03-09 | Define the strict real-result v2 checker contract | 5 / 1 |
| 11/03-11 | 03-10 | Run and supervise actual IPL3-to-hello execution | 8 / 2 |
| 12/03-12 | 03-11 | Prove real failure controls and guest determinism | 8 / 2 |
| 13/03-13 | 03-12 | Collect exact-SHA hosted evidence and verify current sources | 9 / 3 |

Thirteen sequential plans have 21 tasks, maximum nine files per plan and five per task. Focused one-task splits preserve existing implementation boundaries without boilerplate setup or added feature scope. All waves are autonomous and serialize shared source/digest/inventory/build trees. The leading meaningful tracer remains first; its private CPU/profile changes are explicit in internal.h and its public-header/scheduler expansion is separately owned within the same first plan. Every tracked modified file is read before editing.

## Qualification and hosted handoff corrections

Whole upstream 4096-byte source asset SHA256/MD5, qualified executable payload span/SHA256, numeric DMEM copy/entry layout and authored ROM header/whole-cartridge identity are separate. Authored header transformations are documented; execution checks the actual embedded payload and provenance uses the source-asset identity. No unknown original compiler/signer or source-regeneration proof is invented.

Each exact-SHA existing hosted lane MUST publish its sole sanitized canonical record after its successful gate. The REQUIRED post-run collector selects exact repo/workflow/SHA/run/attempt, validates two unique successful named jobs and matching artifacts, retrieves only their records into one workspace and runs the REQUIRED comparator. Missing/duplicate/wrong-SHA inputs or state/identity mismatch fail; local same-host samples cannot satisfy cross-host proof. Only authorized JSON is published, not raw/private/manual/tool/unqualified binary content.

## Prominent retained judgment flags

- Prepared warm/software evidence is not authentic IPL2, cold boot, broad compatibility or hardware-accuracy proof.
- Timed scheduled trace is not guest-visible controller consumption.
- Hash identity/retrieval is not rights clearance or proof of upstream source regeneration.

These remain non-authoritative flags; no human validation or owner acceptance is claimed. Five original unclassified edge flags also remain explicit. AUTONOMY soft-gate routing is unchanged; concrete rights/authority/evidence stops remain binding.
