# Phase 3 spec-less probe and assumption ledger

No SPEC supplied edge/prohibition sections. The installed fallback is ON. This ledger preserves all 14 surfaced edge rows and the full recall→precision prohibition pass; no row is silently dropped. Acceptance is authored into the corresponding PLAN must_haves.truths and tasks. These are planning assertions, not passing execution results.

## Edge coverage

| # | Requirement/category | Lifecycle / verification | Authored acceptance predicate | Plan/task and planned proof |
|---|---|---|---|---|
| 1 | RUN-01/boundary | resolved / explicit (planned) | Schedules accept 0 and 128 events within 65536 bytes; 129 events or 65537 bytes are refused before guest launch, and events at the guest bound execute while later events stay pending. | 03-04/1–3; isviewer/input-partition |
| 2 | RUN-01/precision | resolved / explicit (planned) | Guest ticks use checked uint64 decimal strings; equal-tick events retain source-array order and overflow/reversed times are refused without partial submission. | 03-04/1–3; isviewer/input-partition |
| 3 | RUN-03/unclassified | **FLAG: unresolved classification**; no automatic green | A real guest with zero captured bytes is boot-fail at both authored halt and the guest bound; harness errors still take precedence. | 03-06/1–3,03-07/1; strict v2/actual hello/real controls |
| 4 | RUN-05/empty | resolved / explicit (planned) | Zero-byte flush appends nothing; lengths 1,3,4,512 copy exactly those bytes and length 513 is refused without partial output. | 03-04/1–3; isviewer/input-partition |
| 5 | RUN-05/encoding | resolved / explicit (planned) | ISViewer length and equality are byte-based with big-endian word/halfword lanes; flushes exclude padding and never normalize text. | 03-04/1–3; isviewer/input-partition |
| 6 | RUN-05/idempotency | resolved / explicit (planned) | Repeating a nonzero flush appends the same requested bytes again; a repeated drain with no new bytes returns empty and does not alter guest state. | 03-04/1–3; isviewer/input-partition |
| 7 | RUN-05/concurrency | resolved / explicit (planned) | Each instance owns its output; advance and drain are serialized by the caller, alias/refused drains leave state unchanged, and interleaving two instances does not mix output. | 03-04/1–3; isviewer/input-partition |
| 8 | PROF-05/unclassified | **FLAG: unresolved classification**; no automatic green | Equivalent actual profile/content/input/log/guest-bound state compares equal across partitions/interleaving/trace toggles and both hosted compilers while truthful compiler build IDs may differ. | 03-07/2,03-08/1; actual determinism and hosted pair |
| 9 | BOOT-01/unclassified | **FLAG: unresolved classification**; no automatic green | The warm profile copies the exact admitted pinned IPL3 payload, enters its linked CPU address, retires actual instructions and reports unsupported execution explicitly; it cannot skip IPL3. | 03-01/1,3; boot-entry and identity controls |
| 10 | BOOT-02/unclassified | **FLAG: unresolved classification**; no automatic green | Exact 4096-byte IPL3 identity includes verified 64-hex SHA256,32-hex MD5, pinned revision, separate source/binary rights and Tendies64 maintenance ownership; missing/truncated/changed bytes are refused. | 03-01/1,3; boot-entry and identity controls |
| 11 | BOOT-03/adjacency | resolved / explicit (planned) | When completion and guest bound coincide, queued events at the bound are included and the authored completion verdict is evaluated from exact bytes and CPU milestones; empty output remains boot-fail. | 03-06/1–3,03-07/1; strict v2/actual hello/real controls |
| 12 | BOOT-03/empty | resolved / explicit (planned) | A missing input asset is missing-fixture; a present real guest producing empty output is boot-fail, and empty or missing boot-limits evidence cannot support a boot claim. | 03-06/1–3,03-07/1; strict v2/actual hello/real controls |
| 13 | BOOT-03/ordering | resolved / explicit (planned) | Outcome precedence is host/setup/report/capture error, missing fixture, explicit unsupported/overflow disposition, empty-output boot-fail, exact completed transcript pass, wrong completed text guest-fail, then nonempty incomplete bound timeout. | 03-06/1–3,03-07/1; strict v2/actual hello/real controls |
| 14 | BOOT-04/unclassified | **FLAG: unresolved classification**; no automatic green | Only the authored exact transcript plus actual IPL3 entry, loader DMA, hello entry and fixture-bound completion milestones can yield a real pass; wrong text or synthetic/model records cannot. | 03-06/1–3,03-07/1; strict v2/actual hello/real controls |

The five unclassified rows are BOOT-01, BOOT-02, BOOT-04, RUN-03 and PROF-05. Each has concrete acceptance and owning commands now; its original unclassified lifecycle remains visible rather than manufacturing an engine verdict. No generic review flag is owner acceptance or a new acknowledgment gate under AUTONOMY. Concrete missing execution/rights evidence still blocks the corresponding claim.

## Prohibition recall → precision: no-silent-drop record

Per requirement, ten candidates answer what the feature could silently become that the author would not want. Normal engineering concerns stay in tests/edges/code review. Canon security goes to the phase threat model and secure-phase rather than a fabricated prohibition descriptor.

| Requirement | Candidate | Precision disposition and reason |
|---|---|---|
| BOOT-01 | 1. Ignore bad/missing input | DROP routine engineering; closed input/fixture boundary controls already own this. |
| BOOT-01 | 2. Silently overflow arithmetic/lengths | DROP routine engineering; checked integer edges and sanitizer review own this. |
| BOOT-01 | 3. Leak handles/owned memory on failure | DROP routine engineering; descriptor/refusal cleanup controls own this. |
| BOOT-01 | 4. Produce unstable ordering | DROP routine engineering; scheduler/equal-tick/partition tests own this. |
| BOOT-01 | 5. Lose/truncate evidence without error | DROP routine engineering; output/report/inventory controls own this. |
| BOOT-01 | 6. Accept unknown schema/version/operation | DROP routine engineering; closed contracts and unsupported-stop tests own this. |
| BOOT-01 | 7. Use shared mutable host state | DROP routine engineering; deterministic core source/object lint and interleave tests own this. |
| BOOT-01 | 8. Allow path traversal, injection or unsafe input access | DROP canon referral: ASVS1 threat model and $gsd-secure-phase own bounded address/path/input validation; no ESLint applies to this C17 project. |
| BOOT-01 | 9. Skip actual IPL3 but claim a boot | DROP routine execution correctness; actual entry/DMA/hello milestone controls test this. |
| BOOT-01 | 10. Describe warm software seeds as authentic IPL2/cold/hardware truth | KEEP → PROH-03-01 judgment; scoped evidence is a product transparency constraint. |
| BOOT-02 | 1. Ignore bad/missing input | DROP routine engineering; closed input/fixture boundary controls already own this. |
| BOOT-02 | 2. Silently overflow arithmetic/lengths | DROP routine engineering; checked integer edges and sanitizer review own this. |
| BOOT-02 | 3. Leak handles/owned memory on failure | DROP routine engineering; descriptor/refusal cleanup controls own this. |
| BOOT-02 | 4. Produce unstable ordering | DROP routine engineering; scheduler/equal-tick/partition tests own this. |
| BOOT-02 | 5. Lose/truncate evidence without error | DROP routine engineering; output/report/inventory controls own this. |
| BOOT-02 | 6. Accept unknown schema/version/operation | DROP routine engineering; closed contracts and unsupported-stop tests own this. |
| BOOT-02 | 7. Use shared mutable host state | DROP routine engineering; deterministic core source/object lint and interleave tests own this. |
| BOOT-02 | 8. Allow path traversal, injection or unsafe input access | DROP canon referral: ASVS1 threat model and $gsd-secure-phase own bounded address/path/input validation; no ESLint applies to this C17 project. |
| BOOT-02 | 9. Describe hashes as rights clearance | KEEP → PROH-03-03 judgment; identity and permission are different evidence. |
| BOOT-02 | 10. Describe retrieval of committed generated IPL3 as source regeneration | KEEP → PROH-03-03 judgment; same transparency prohibition, merged explicitly. |
| BOOT-03 | 1. Ignore bad/missing input | DROP routine engineering; closed input/fixture boundary controls already own this. |
| BOOT-03 | 2. Silently overflow arithmetic/lengths | DROP routine engineering; checked integer edges and sanitizer review own this. |
| BOOT-03 | 3. Leak handles/owned memory on failure | DROP routine engineering; descriptor/refusal cleanup controls own this. |
| BOOT-03 | 4. Produce unstable ordering | DROP routine engineering; scheduler/equal-tick/partition tests own this. |
| BOOT-03 | 5. Lose/truncate evidence without error | DROP routine engineering; output/report/inventory controls own this. |
| BOOT-03 | 6. Accept unknown schema/version/operation | DROP routine engineering; closed contracts and unsupported-stop tests own this. |
| BOOT-03 | 7. Use shared mutable host state | DROP routine engineering; deterministic core source/object lint and interleave tests own this. |
| BOOT-03 | 8. Allow path traversal, injection or unsafe input access | DROP canon referral: ASVS1 threat model and $gsd-secure-phase own bounded address/path/input validation; no ESLint applies to this C17 project. |
| BOOT-03 | 9. Advertise boot solved or broad compatibility | KEEP → PROH-03-01 judgment; same scoped-claim prohibition, merged explicitly. |
| BOOT-03 | 10. Hide unmodeled PIF/CIC/cold behavior | KEEP → PROH-03-01 judgment; same scoped-claim prohibition, merged explicitly. |
| BOOT-04 | 1. Ignore bad/missing input | DROP routine engineering; closed input/fixture boundary controls already own this. |
| BOOT-04 | 2. Silently overflow arithmetic/lengths | DROP routine engineering; checked integer edges and sanitizer review own this. |
| BOOT-04 | 3. Leak handles/owned memory on failure | DROP routine engineering; descriptor/refusal cleanup controls own this. |
| BOOT-04 | 4. Produce unstable ordering | DROP routine engineering; scheduler/equal-tick/partition tests own this. |
| BOOT-04 | 5. Lose/truncate evidence without error | DROP routine engineering; output/report/inventory controls own this. |
| BOOT-04 | 6. Accept unknown schema/version/operation | DROP routine engineering; closed contracts and unsupported-stop tests own this. |
| BOOT-04 | 7. Use shared mutable host state | DROP routine engineering; deterministic core source/object lint and interleave tests own this. |
| BOOT-04 | 8. Allow path traversal, injection or unsafe input access | DROP canon referral: ASVS1 threat model and $gsd-secure-phase own bounded address/path/input validation; no ESLint applies to this C17 project. |
| BOOT-04 | 9. Use synthetic/native output as guest execution evidence | DROP routine evidence correctness; schema/milestone negative tests already enforce. |
| BOOT-04 | 10. Generalize one authored hello to commercial/hardware compatibility | KEEP → PROH-03-01 judgment; same scoped-claim prohibition, merged explicitly. |
| RUN-01 | 1. Ignore bad/missing input | DROP routine engineering; closed input/fixture boundary controls already own this. |
| RUN-01 | 2. Silently overflow arithmetic/lengths | DROP routine engineering; checked integer edges and sanitizer review own this. |
| RUN-01 | 3. Leak handles/owned memory on failure | DROP routine engineering; descriptor/refusal cleanup controls own this. |
| RUN-01 | 4. Produce unstable ordering | DROP routine engineering; scheduler/equal-tick/partition tests own this. |
| RUN-01 | 5. Lose/truncate evidence without error | DROP routine engineering; output/report/inventory controls own this. |
| RUN-01 | 6. Accept unknown schema/version/operation | DROP routine engineering; closed contracts and unsupported-stop tests own this. |
| RUN-01 | 7. Use shared mutable host state | DROP routine engineering; deterministic core source/object lint and interleave tests own this. |
| RUN-01 | 8. Allow path traversal, injection or unsafe input access | DROP canon referral: ASVS1 threat model and $gsd-secure-phase own bounded address/path/input validation; no ESLint applies to this C17 project. |
| RUN-01 | 9. Describe scheduled input traces as controller consumption | KEEP → PROH-03-02 judgment; intended product capability must be represented honestly. |
| RUN-01 | 10. Use wall time as deterministic guest time | DROP routine architectural correctness; core lint and scheduler/bound tests own this. |
| RUN-03 | 1. Ignore bad/missing input | DROP routine engineering; closed input/fixture boundary controls already own this. |
| RUN-03 | 2. Silently overflow arithmetic/lengths | DROP routine engineering; checked integer edges and sanitizer review own this. |
| RUN-03 | 3. Leak handles/owned memory on failure | DROP routine engineering; descriptor/refusal cleanup controls own this. |
| RUN-03 | 4. Produce unstable ordering | DROP routine engineering; scheduler/equal-tick/partition tests own this. |
| RUN-03 | 5. Lose/truncate evidence without error | DROP routine engineering; output/report/inventory controls own this. |
| RUN-03 | 6. Accept unknown schema/version/operation | DROP routine engineering; closed contracts and unsupported-stop tests own this. |
| RUN-03 | 7. Use shared mutable host state | DROP routine engineering; deterministic core source/object lint and interleave tests own this. |
| RUN-03 | 8. Allow path traversal, injection or unsafe input access | DROP canon referral: ASVS1 threat model and $gsd-secure-phase own bounded address/path/input validation; no ESLint applies to this C17 project. |
| RUN-03 | 9. Pass a zero-output guest | DROP routine contract correctness; actual empty-halt/empty-bound controls own this. |
| RUN-03 | 10. Hide timeout versus harness failure | DROP routine contract correctness; strict real-report/process precedence cases own this. |
| RUN-05 | 1. Ignore bad/missing input | DROP routine engineering; closed input/fixture boundary controls already own this. |
| RUN-05 | 2. Silently overflow arithmetic/lengths | DROP routine engineering; checked integer edges and sanitizer review own this. |
| RUN-05 | 3. Leak handles/owned memory on failure | DROP routine engineering; descriptor/refusal cleanup controls own this. |
| RUN-05 | 4. Produce unstable ordering | DROP routine engineering; scheduler/equal-tick/partition tests own this. |
| RUN-05 | 5. Lose/truncate evidence without error | DROP routine engineering; output/report/inventory controls own this. |
| RUN-05 | 6. Accept unknown schema/version/operation | DROP routine engineering; closed contracts and unsupported-stop tests own this. |
| RUN-05 | 7. Use shared mutable host state | DROP routine engineering; deterministic core source/object lint and interleave tests own this. |
| RUN-05 | 8. Allow path traversal, injection or unsafe input access | DROP canon referral: ASVS1 threat model and $gsd-secure-phase own bounded address/path/input validation; no ESLint applies to this C17 project. |
| RUN-05 | 9. Present libdragon compatibility lanes as proven original ISViewer hardware | KEEP → PROH-03-01 judgment; same scoped hardware-claim prohibition, merged explicitly. |
| RUN-05 | 10. Treat truncated output as a pass | DROP routine correctness; bounded flush/capture overflow controls own this. |
| PROF-05 | 1. Ignore bad/missing input | DROP routine engineering; closed input/fixture boundary controls already own this. |
| PROF-05 | 2. Silently overflow arithmetic/lengths | DROP routine engineering; checked integer edges and sanitizer review own this. |
| PROF-05 | 3. Leak handles/owned memory on failure | DROP routine engineering; descriptor/refusal cleanup controls own this. |
| PROF-05 | 4. Produce unstable ordering | DROP routine engineering; scheduler/equal-tick/partition tests own this. |
| PROF-05 | 5. Lose/truncate evidence without error | DROP routine engineering; output/report/inventory controls own this. |
| PROF-05 | 6. Accept unknown schema/version/operation | DROP routine engineering; closed contracts and unsupported-stop tests own this. |
| PROF-05 | 7. Use shared mutable host state | DROP routine engineering; deterministic core source/object lint and interleave tests own this. |
| PROF-05 | 8. Allow path traversal, injection or unsafe input access | DROP canon referral: ASVS1 threat model and $gsd-secure-phase own bounded address/path/input validation; no ESLint applies to this C17 project. |
| PROF-05 | 9. Claim equal emulator digests prove hardware accuracy | KEEP → PROH-03-01 judgment; same scoped-claim prohibition, merged explicitly. |
| PROF-05 | 10. Force equal compiler build IDs or compare entire JSON | DROP routine identity correctness; cross-host subset comparator owns this. |

## Kept judgment prohibitions and routing

| ID | Statement | Plan source / current verdict |
|---|---|---|
| PROH-03-01 | Warm entry, software policies, a single hello or emulator agreement must not be presented as authentic IPL2/cold boot/hardware accuracy/broad compatibility or original ISViewer proof. | 03-08 must_haves.prohibitions; **FLAG: non-authoritative, human review recommended**. |
| PROH-03-02 | Timed input scheduling/trace must not be presented as implemented controller consumption. | 03-08 must_haves.prohibitions; **FLAG: non-authoritative, human review recommended**. |
| PROH-03-03 | Artifact hashes/retrieval must not be presented as rights clearance or source-regeneration proof. | 03-08 must_haves.prohibitions; **FLAG: non-authoritative, human review recommended**. |

No check_kind/check_target/check_rule/violation descriptor is invented. Lifecycle resolved/judgment records explicit intent only, never a mechanical pass or human validation. Three merged domain prohibitions survive 80 recalled candidates; every routine/canon/duplicate disposition is recorded above. Autonomous soft-gate routing comes from .planning/AUTONOMY.md; an actual protected-content, authority or evidence stop remains blocking.
