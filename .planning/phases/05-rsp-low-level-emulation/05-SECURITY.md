---
phase: "05"
status: secured
asvs_level: 1
block_on: high
threats_open: 0
---

# Phase 05 configured security audit

The configured `gsd-security-auditor` completed a read-only audit of source
through `0b8786b`, all ten authored threat models and `05-VALIDATION.md`.
The actual dispatch used project-resolved Codex `gpt-6.1-sol`, effort `high`.
The auditor returned **OPEN_THREATS**: 22 of 26 unique authored mitigations
closed, three high threats open and one medium threat open. No risk was
accepted or transferred. This is an automated, non-authoritative review;
there is no human validation or hardware claim.

## Blocking findings

| Threat | Finding | Required closure |
|---|---|---|
| T-05-04 | `t64_rsp_issue_event` commits the current DMA opportunity before an MTC0 c2/c3 bus write reaches DMA admission. An invalid span/full queue can refuse after memory or queue promotion changes. | Preflight the whole opportunity before mutation. Fetched baseline and invalid-span/full-queue tests cover concurrent setup/final-beat/promotion and compare future-affecting state. |
| T-05-23 | Final evidence checker, mutation controls and exact final artifact qualification are pending. | Implement the planned checker and tests; bind actual native/instrumented/hosted evidence to source, profile and artifact identities. |
| T-05-24 | Final report privacy, rights/provenance scans and clean authored-only hosted closure remain pending. | Complete final scans and private-leak control; exercise authored setup without private roots; retain exact final hosted pair. |

T-05-25 (medium) remains open until actual review reports, dispositions and
focused fix evidence are persisted. Starting a reviewer does not close it.

## Verified mitigations

| Threats | Source/evidence owners inspected |
|---|---|
| T-05-01/02 | Bounded fetched RSP dispatch and staged vector access; profile predicate and canonical state hashing; unsupported-opcode and future-field controls. |
| T-05-03 | All-row DMA span/overflow admission, queue/descriptor and beat bounds; control notifications. Whole-opportunity ordering is separately open above. |
| T-05-05/06 | Restrictive/exclusive private storage; closed source/tool/dependency/asset qualification; actual ELF/payload/ROM composition and v5 source/executable binding. |
| T-05-07/08/09 | Component/profile/build/terminal/transcript authentication; independent child/outer/capture limits and cleanup; shared closed CPU parser and canonical-byte regressions. |
| T-05-10/11 | Scalar/CP0 and branch/link/delay controls; staged bounded transfers with element/alignment/wrap/alias comparisons. Concurrent DMA remains open above. |
| T-05-12/13 | Masked 48-bit arithmetic, bounded products, staged clip/compare/flag effects; literal arithmetic/selector/wrap and complete function-slot ownership. |
| T-05-14/15 | Selected divider source/history/license/provenance controls; bounded integer table generation; independent rational and squared-integer expectations and instance-owned latches. |
| T-05-16/17 | Nonempty fetched per-opcode fuzz and complete-state refusals; explicit absent differential execution; source/route/Ninja/archive/symbol checks with bypass controls. These are scoped controls, not universal proof about C. |
| T-05-18 | Authored bounded standalone stress, exact workflow contract, read-only checkout and no upload. |
| T-05-19/20/21 | Closed smoke fixture identities, terminal/delay/semantic authentication; pre-use source/tool/boot and final composition checks; distinct per-case and aggregate bounds. |
| T-05-22 | Source-derived exact inventory, duplicate-resistant partition and member binding; retained private reasons or authenticated terminal evidence. Classification completeness does not establish suite success. |
| T-05-SC | No package installation in scope; existing tools remain qualified and builds offline/locked/frozen. |

The source owners are the files listed in `build/phase05-review-files.txt`;
durable task and threat ownership is in `05-VALIDATION.md` and the plans.
The auditor inspected saved native/instrumented logs and read-only private
record consistency; it ran no build, guest, install or broad gate. Existing
logs do not replace pending final-source qualification. No additional unmapped
threat flag was identified. The DMA defect was independently corroborated by
the code reviewer; the initial audit did not execute a new reproduction.

## Follow-up

Preserve this initial open verdict. After fixes and final evidence, request a
scoped follow-up for these four rows and affected controls rather than repeating
the full closed register. Owner approval and private diagnostic reruns are not
implied or required by this audit.


## Configured scoped security follow-up

The configured `gsd-security-auditor`, Codex `gpt-6.1-sol` with `high` effort,
returned **SECURED** for the four previously open rows after inspecting the
corrected source, actual saved receipt, native/instrumented logs, hosted
records and affected private archive boundary. ASVS1 and the high-severity
block threshold remain unchanged. This closes the remaining four of the26
unique authored mitigations; the earlier22 closures were not repeated.

| Threat | Final disposition and concrete evidence |
|---|---|
| T-05-04 | CLOSED: projected admission precedes DMA commit; origin-aware status/self-HALT and accepted/refused complete-state controls are present, with saved normal/instrumented fix results. |
| T-05-23 | CLOSED: fixed binaries and runtime IDs, strict registered passing rows, independent pinned-source membership, original C outputs/attempts, fully resealed shrink rejection, and full-entry/dispatch/undefined-global controls; actual saved receipt check passes. |
| T-05-24 | CLOSED: selected-ROM/quoted privacy controls, clean rights/privacy tree and range logs, isolated authored setup, exact final hosted records and retained restrictive archives. |
| T-05-25 | CLOSED: original reports preserved, explicit parent fix dispositions, setup failure and historical artifact-retention limits disclosed, preliminary assessment separated from auditor verdict. |

The affected new host pipeline belongs to T-05-23/24; no additional unmapped
flag was found within that scoped review. Runner verification precedes fresh
run-directory creation. Current archive bytes match their binding and hashes;
original archives and permissions remain intact.

The auditor ran no build, guest, install, broad test, Git mutation or file
write. Core semantic evidence retains its original identity alongside the
refreshed exact artifacts and current host checks. Provider execution metadata
was not independently attested. No risk was accepted/transferred, and no human
validation, suite-pass, hardware, optimized-differential or release claim is
implied. Existing owner stops remain effective. Remaining blocking predicate:
none within the audited scope.
