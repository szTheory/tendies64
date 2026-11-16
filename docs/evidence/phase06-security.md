---
phase: 06
asvs_level: 1
block_on: high
verdict: OPEN_THREATS
threats_total: 33
threats_open: 3
---

# Phase 06 security mitigation audit

The configured security auditor reviewed the complete phase at source snapshot
cc1e3f2, using all nine plan-time threat models, the eight execution summaries,
and the exact 51-file canonical diff saved in build/gate/06-08-review-scope.json.
All declared rows are high/mitigate. Supply-chain rows retain each plan context.
No risk acceptance is recorded. No new dependency or installation was introduced.

The original verdict closed 29/33 rows and left four open: one implementation
blocker and three review/final-evidence obligations. The concurrent conclusive
code review now supplies the missing review half, closing T-06-21. The final
stable gate and hosted reconciliation are still obligations, not executed proof.

## Blocking finding and correction queue

T-06-18: _validate_batch passed editable expected_rom_sha256 to archived C;
C bound that caller digest and only compared the selected digest with it.
Coherently changing record, input and retained report could lose the independent
image anchor without changing the immutable context. Existing saved batches
were checked read-only and match independently calculated image hashes; this
is missing protection, not evidence of forged current records.

Owning correction: Plan07 Task1, scripts/check-base-evidence.py and
scripts/tests/test_base_evidence.py (two-file slice). Reuse reviewed bound-image
loading and source/selector seam, recompute selected-ROM bytes for each four-byte
selector, compare before C classification. Required control: accepted original
baseline, then copied record/input/report with a changed digest and unchanged
context/image; refusal must reach the independent image guard. Owning check:
harness.base-evidence in both presets, followed by scoped code/security re-review.
Parent recorded this queue before implementing the slice. Final gates wait for
its disposition. No guest replay, history repair or new rights is authorized.

## Complete declared register

References below describe the reviewed snapshot; later correction lines may move.
The auditor used source and bounded read-only evidence, not a final full suite.

| Plan | Threat | Status | Mitigation evidence or obligation |
| --- | --- | --- | --- |
| 01 | T-06-01 | CLOSED | core/t64_internal.h:657; core/t64_machine.c:373; core/t64_digest.c:57; core/tests/test_profile.c:743 |
| 01 | T-06-02 | CLOSED | core/t64_cpu.c:1454,1513,2299; core/tests/test_cpu_time.c:982 |
| 02 | T-06-04 | CLOSED | scripts/verify-base-diagnostic-qualification.py:331,362,500; runner/t64_base_diag_protocol.c:265 |
| 02 | T-06-05 | CLOSED | scripts/verify-base-diagnostic-qualification.py:141,448,564; scripts/tests/test_base_run.py:342 |
| 02 | T-06-06 | CLOSED | scripts/build-qualified-base-diagnostic.py:998; scripts/verify-base-diagnostic-qualification.py:230; scripts/run-base-diagnostic.py:135,163,506 |
| 02 | T-06-07 | CLOSED | scripts/build-qualified-base-diagnostic.py:260,380,459,649,738; runner/t64_base_diagnostic.c:49 |
| 03 | T-06-08 | CLOSED | core/t64_bus.c:160; core/t64_rdram.c:428,484; core/tests/test_rdram.c:316,417,761 |
| 03 | T-06-09 | CLOSED | docs/spec/rdram-boot.md; core/t64_rdram.c:402; core/tests/test_rdram.c:417,1262 |
| 03 | T-06-10 | CLOSED | core/t64_bus.c:53,475,522; core/t64_cache.c:178,195,363; core/t64_sp.c:601,965; core/t64_digest.c:57 |
| 04 | T-06-11 | CLOSED | core/t64_cpu.c:1454; core/t64_exception.c; core/tests/test_cpu_time.c:1446,1447 |
| 04 | T-06-12 | CLOSED | core/t64_cpu.c:1513,2299; core/tests/test_cpu_time.c:970,1229 |
| 05 | T-06-13 | CLOSED | runner/t64_cold_boot.c:899,966,564,581,1148; runner/tests/test_cold_boot.c:161 |
| 05 | T-06-14 | CLOSED | core/tests/test_boot_determinism.c:855; core/CMakeLists.txt:188; runner/CMakeLists.txt:381,388 |
| 06 | T-06-15 | CLOSED | runner/t64_base_diag_protocol.c:660,687,781,922,937,956; scripts/run-base-diagnostic.py:794,825 |
| 06 | T-06-16 | CLOSED | scripts/run-base-diagnostic.py:644,697; runner/t64_base_diagnostic.c:850; scripts/run-cpu-diagnostic.py:29,301 |
| 06 | T-06-17 | CLOSED | scripts/base-diagnostic-lanes.py:25; scripts/run-base-diagnostic.py:598; scripts/tests/test_base_lanes.py:68,92,103 |
| 07 | T-06-18 | OPEN | Historical checker forwarded caller expected_rom_sha256 without independent bound-image recomputation |
| 07 | T-06-19 | CLOSED | scripts/check-base-evidence.py:38,76,181,685; scripts/tests/test_base_evidence.py PublicEvidenceControls |
| 07 | T-06-20 | CLOSED | scripts/check-base-evidence.py original CTest/attempt/artifact reconstruction and bounded claim equality; scripts/tests/test_base_evidence.py PublicEvidenceControls |
| 08 | T-06-21 | CLOSED | Conclusive code review docs/evidence/phase06-review.md plus this complete security register |
| 08 | T-06-22 | OPEN | Pending correction/re-review and stable final gate/artifact qualification |
| 08 | T-06-23 | OPEN | Pending exact authored-only hosted pair, artifacts/readback, publication scans |
| 09 | T-06-24 | CLOSED | runner/t64_cold_boot.c:899,1178; runner/tests/test_cold_boot.c:161; runner/CMakeLists.txt:377,384 |
| 09 | T-06-03 | CLOSED | runner/t64_cold_boot.c first successful bus observation plus retirement; runner/tests/test_cold_boot.c:254,306 |
| 01 | T-06-SC | CLOSED | In-tree code and existing tools only; offline locked/frozen qualified build at scripts/build-qualified-base-diagnostic.py:696; no added dependency/installer. |
| 02 | T-06-SC | CLOSED | In-tree code and existing tools only; offline locked/frozen qualified build at scripts/build-qualified-base-diagnostic.py:696; no added dependency/installer. |
| 03 | T-06-SC | CLOSED | In-tree code and existing tools only; offline locked/frozen qualified build at scripts/build-qualified-base-diagnostic.py:696; no added dependency/installer. |
| 04 | T-06-SC | CLOSED | In-tree code and existing tools only; offline locked/frozen qualified build at scripts/build-qualified-base-diagnostic.py:696; no added dependency/installer. |
| 05 | T-06-SC | CLOSED | In-tree code and existing tools only; offline locked/frozen qualified build at scripts/build-qualified-base-diagnostic.py:696; no added dependency/installer. |
| 06 | T-06-SC | CLOSED | In-tree code and existing tools only; offline locked/frozen qualified build at scripts/build-qualified-base-diagnostic.py:696; no added dependency/installer. |
| 07 | T-06-SC | CLOSED | In-tree code and existing tools only; offline locked/frozen qualified build at scripts/build-qualified-base-diagnostic.py:696; no added dependency/installer. |
| 08 | T-06-SC | CLOSED | In-tree code and existing tools only; offline locked/frozen qualified build at scripts/build-qualified-base-diagnostic.py:696; no added dependency/installer. |
| 09 | T-06-SC | CLOSED | In-tree code and existing tools only; offline locked/frozen qualified build at scripts/build-qualified-base-diagnostic.py:696; no added dependency/installer. |

## Limits

The historical Plan02 executable loss remains disclosed and unusable. Forward
archive-before-activation and exact-pair-before-use guards exist; no replay or
retrospective identity was accepted. Native models and software policy are not
physical hardware measurements. The original summaries had no unmapped threat
flags. Private diagnostic identities, counts and results remain local. Integrity
assumes a reviewed producer and trusted owner, not same-owner cryptographic
execution attestation. No user action is needed; parent implements/re-reviews
the queued correction and runs the already planned gates.
