---
phase: "06"
slug: cold-boot-cpu-timing-and-full-base-suite
status: verified
threats_open: 0
asvs_level: 1
created: 2026-11-16
---

# Phase 06 security contract

Configured full and scoped audits, exact snapshots, mitigation evidence and
final local/hosted closure are retained in [the detailed security report](../../../docs/evidence/phase06-security.md). This canonical index exposes the completed
audits to GSD hooks; no second full audit or human approval is claimed.

## Complete declared mitigation register

All33 rows are high/mitigate. Duplicate supply-chain IDs retain plan context.

| Plan | Threat | Status | Mitigation evidence |
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
| 07 | T-06-18 | CLOSED | Historical checker forwarded caller expected_rom_sha256 without independent bound-image recomputation |
| 07 | T-06-19 | CLOSED | scripts/check-base-evidence.py:38,76,181,685; scripts/tests/test_base_evidence.py PublicEvidenceControls |
| 07 | T-06-20 | CLOSED | scripts/check-base-evidence.py original CTest/attempt/artifact reconstruction and bounded claim equality; scripts/tests/test_base_evidence.py PublicEvidenceControls |
| 08 | T-06-21 | CLOSED | Conclusive code review docs/evidence/phase06-review.md plus this complete security register |
| 08 | T-06-22 | CLOSED | Reviewed corrections and stable normal/instrumented gate, exact archives and final private qualification; see closure below |
| 08 | T-06-23 | CLOSED | Exact hosted pair43788405776 attempt1 on aa1e531; original artifacts validated/compared, scans passed; docs/ci-reviews/LOG.md records measured review. |
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

## Accepted risks

No accepted risks. Original archive loss, source classification versus passing,
software model limits and external rights/release/physical stops stay explicit.

## Audit trail

Initial configured security audit:29/33 closed, implementation image-binding
gap plus review/final evidence obligations open. Scoped corrections passed
configured re-audits. Final canonical/instrumented artifacts and exact authored
hosted pair/readback close remaining evidence rows. Final33/33 closed.
