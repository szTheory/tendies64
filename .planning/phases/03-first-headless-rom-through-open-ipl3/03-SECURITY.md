---
phase: "03"
slug: first-headless-rom-through-open-ipl3
status: verified
threats_open: 0
threats_total: 27
asvs_level: 1
block_on: high
source_revision: 7c0ea9b5f3076122d506074aa7d3a2283cf9e2a6
created: "2026-11-11"
---

# Phase 03 — Security

All planned code mitigations were verified read-only by the security auditor.
This verifies implementation placement, not phase completion or hardware accuracy.
The mandatory actual hosted artifact pair is still pending in plan 13.

## Trust Boundaries

| Boundary | Description | Data Crossing |
|---|---|---|
| Pinned source to machine | Qualify rights, identity, spans and owned admission | IPL3 asset, authored ROM, profile |
| Guest to host observation | Bound copies, time, output and versioned state | Guest instructions, DMA, input, captured bytes |
| Supervisor and CI to evidence consumer | Owned descriptors, strict reports, exact scoped artifacts | Untrusted child JSON and capture, hosted metadata |

## Threat Register

The source register reconciles all thirteen PLAN threat models. The shared
T-03-SC definition is identical across plans and counted once. All dispositions
are mitigate; no human approval, risk acceptance or transfer is asserted.
References below identify the actual validation call or control and its tests.

| Threat ID | Category | Component | Severity | Disposition | Mitigation evidence | Status |
|---|---|---|---|---|---|---|
| T-03-01 | Tampering | pinned source regions and CPU entry | high | mitigate | core/t64_machine.c:186,200,467; core/tests/test_tracer.c:345 | closed |
| T-03-02 | Denial of service | CPU stop/scheduler/profile fields | high | mitigate | core/t64_cpu.c:869; core/t64_scheduler.c:329; core/t64_digest.c:195; core/tests/test_tracer.c:555 | closed |
| T-03-SC | Tampering | qualified build inputs | high | mitigate | docs/evidence/zlib-host-prerequisite-audit.md; scripts/build-boot-fixtures.sh:190; fixtures/boot-contract.json:120 | closed |
| T-03-03 | Tampering | authored fixture packer/tool inputs | high | mitigate | scripts/build-boot-fixtures.sh:94,285,485,551; tools/fixture/t64_rom_pack.c:192 | closed |
| T-03-04 | Information disclosure | rights-bearing generated inputs | high | mitigate | fixtures/manifest.json:107,115; scripts/build-boot-fixtures.sh:478; .github/workflows/ci.yml:39 | closed |
| T-03-05 | Tampering | boot identity/gate registration | high | mitigate | core/CMakeLists.txt:146,163; core/tests/test_boot_entry.c:621; scripts/check-test-inventory.sh:84,244 | closed |
| T-03-06 | Information disclosure | boot claims and primary sources | high | mitigate | docs/spec/boot-limits.md:3,175; 03-PRIMARY-EVIDENCE-INDEX.md:5,27; tools/claims/CMakeLists.txt:9; tools/rights/CMakeLists.txt:18,30 | closed |
| T-03-07 | Tampering | CPU/KSEG/CP0/cache execution | high | mitigate | core/tests/test_cpu_path.c:776,796; core/CMakeLists.txt:204,208 | closed |
| T-03-08 | Denial of service | retirement and guest bounds | high | mitigate | core/t64_cpu.c:942,1026; core/t64_scheduler.c:341; core/tests/test_tracer.c:555; core/tests/test_boot_determinism.c:425; 03-04-SUMMARY.md; docs/evidence/phase3-ci.md | closed |
| T-03-09 | Tampering | warm device ranges/reset/DMA | high | mitigate | core/t64_warm_devices.c:34,113; core/t64_sp.c:309; core/tests/test_warm_devices.c:584,709,841 | closed |
| T-03-10 | Elevation of privilege | guest memory transfer bounds | high | mitigate | core/t64_sp.c:15,102,334; core/t64_pi.c:38,69; core/tests/test_warm_devices.c:709; 03-05-SUMMARY.md:119 | closed |
| T-03-11 | Tampering | ISViewer lengths and lanes | high | mitigate | core/t64_isviewer.c:29,130,163; core/tests/test_isviewer.c:251,884,935,988 | closed |
| T-03-12 | Denial of service | guest output capacity | high | mitigate | core/t64_isviewer.c:119,130; core/tests/test_isviewer.c:1229; runner/t64_real.c:696 | closed |
| T-03-13 | Tampering | owned output caller-copy spans | high | mitigate | core/t64_observe.c:27,37,67; core/t64_digest.c:228; core/tests/test_isviewer.c:397 | closed |
| T-03-14 | Tampering | input submission and future state | high | mitigate | core/t64_observe.c:103,115; core/t64_scheduler.c:103; core/tests/test_isviewer.c:578,622 | closed |
| T-03-15 | Denial of service | schedule JSON/count/tick parse | high | mitigate | runner/t64_input.h:9; runner/t64_input.c:92,101,123,216,241; runner/tests/test_input.c:127 | closed |
| T-03-16 | Tampering | canonical input identity and scheduling trace | high | mitigate | runner/t64_input.c:158,188; runner/tests/test_input.c:238,470,492,521; core/t64_observe.c:86 | closed |
| T-03-17 | Tampering | supervisor-to-runner bytes | high | mitigate | runner/t64_verified_input.c:244; runner/t64_real.c:118; runner/tests/test_fixture_descriptor.c:1075; runner/tests/test_real_runner.c:513 | closed |
| T-03-18 | Denial of service | descriptor/process cleanup | high | mitigate | runner/t64_verified_input.c:177,219,231,254; runner/t64_supervisor.c:1505; runner/tests/test_fixture_descriptor.c:1336 | closed |
| T-03-19 | Spoofing | real v2 identity and guest evidence | high | mitigate | runner/t64_result_check.c:794,886,890,1097; runner/tests/test_result_v2.c:545,547 | closed |
| T-03-20 | Tampering | outcome/stop classification | high | mitigate | runner/t64_real.c:730,780,803; runner/t64_supervisor.c:1434,1459; runner/tests/test_real_controls.c:61,879 | closed |
| T-03-21 | Spoofing | actual CPU/output milestones | high | mitigate | runner/t64_real.c:621,674,780,810; runner/tests/test_real_runner.c:769,786 | closed |
| T-03-22 | Tampering | child report/capture/admission | high | mitigate | runner/t64_supervisor.c:792,1425,1434; runner/tests/test_real_runner.c:513,934 | closed |
| T-03-23 | Repudiation | real negative verdicts | high | mitigate | runner/tests/test_real_controls.c:597,894; runner/tests/test_real_runner.c:925; runner/CMakeLists.txt:128 | closed |
| T-03-24 | Tampering | canonical state and required tests | high | mitigate | core/t64_digest.c:181; core/tests/test_boot_determinism.c:425,662; scripts/check-test-inventory.sh:84,103,244 | closed |
| T-03-25 | Tampering | exact-SHA hosted evidence delivery | high | mitigate | .github/workflows/ci.yml:35; scripts/collect-boot-evidence.sh:157,183,209,248,267; runner/t64_result_check.c:1661; scripts/compare-boot-evidence.sh:45,70; scripts/check-ci-yml.sh:68,196,222,344,363 | closed |
| T-03-26 | Information disclosure | published evidence and final claims | high | mitigate | .github/workflows/ci.yml:31,57; scripts/collect-boot-evidence.sh:75,99; docs/evidence/phase3-ci.md; 03-REVIEW-FIX.md | closed |

## Accepted Risks Log

No accepted risks.

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|---|---|---|---|---|
| 2026-11-11 | 27 | 26 | 1 | gsd-security-auditor, initial ASVS1 audit |
| 2026-11-11 | 27 | 27 | 0 | gsd-security-auditor, bounded T-03-SC recheck |

The initial finding was the missing legitimacy audit for the guarded Ubuntu
zlib development prerequisite. The [retrospective audit](../../../docs/evidence/zlib-host-prerequisite-audit.md)
remedies the missing current record. The required pre-install check did not
occur; the late audit does not rewrite that history. The sole SUMMARY
supply-chain flag maps to T-03-SC. No unregistered flags remain.

CR-01 and CR-02 were reverified at their actual entry points; CR-03 and WR-01
were checked against the current guard fix and mutation controls. All four are
fixed. No additional builds or tests were launched by the security auditor.

T-03-25 is code-mitigated, but the hosted pair has not been established.
The collector shape-checks and records GitHub archive digest metadata; it
separately hashes extracted JSON, without independently recomputing the archive
digest. The current implementation does not claim that stronger guarantee.

## Sign-Off

- [x] Every planned threat has a verified disposition.
- [x] No accepted risks were inferred.
- [x] No blocking planned threats remain.
- [x] Implementation audit verified; hosted proof remains a separate gate.

**Verification:** automated read-only code audit, 2026-11-11. No owner sign-off is claimed.
