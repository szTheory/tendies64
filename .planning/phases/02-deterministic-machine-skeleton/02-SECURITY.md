---
phase: "02"
slug: deterministic-machine-skeleton
status: verified
threats_open: 0
asvs_level: 1
block_on: high
created: "2026-11-10"
---

# Phase 2 — Security

## Trust Boundaries

| Boundary | Data crossing | Controls |
|---|---|---|
| Caller to machine | Untrusted ROM bytes, allocator reports, descriptors and output spans | Bounded validation, explicit caller readable-span preconditions, overlap checks and transactional refusal |
| Guest requests to devices | Bus addresses, register values and scheduled DMA descriptors | Checked arithmetic/ranges, bounded queue, nonwrapping tokens and policy-labeled deadlines |
| Core to observer | Trace records and digest output | Copy-only bounded ring, visible drops and checked output spans |
| Host model tools to files/processes | Paths, result bytes and test children | Nonblocking regular-file checks, closed schema, bounded deadlines and child cleanup |
| Model evidence to support claims | Native result records | Separate schema; guest result/seal/suite/claims tools reject native model evidence |

## Threat Register

Authored plan register, deduplicated by ID. Mitigation and test paths are recorded
in `02-SECURITY-AUDIT.json`; source-delta review verified changed controls after
the code-review fixes. This is scoped L1 evidence, not a claim of bug freedom.

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|---|---|---|---|---|---|---|
| T-02-01 | Tampering | instance lifecycle | high | mitigate | Validate reported capacity/alignment/ranges before use; callbacks only lifecycle/admission; count allocations/releases | closed |
| T-02-02 | Spoofing | profile/build identity | medium | mitigate | Canonical field audit, changed-field tests and explicit unqualified target labels | closed |
| T-02-SC | Tampering | package installs | high | mitigate | No package installs; reuse authored source only; any future install requires legitimacy audit and blocking human gate for assumed/suspect packages | closed |
| T-02-03 | Tampering | ROM admission | high | mitigate | Validate all sizes/order before access; checked ranges; explicit header loads; candidate commit | closed |
| T-02-04 | Denial of service | lifecycle/admission | high | mitigate | 64 MiB cap, denial injection, bounded allocation and cleanup without touching prior content | closed |
| T-02-05 | Tampering | allocation overlap | high | mitigate | Half-open range checks, alias-release rules, reported capacity/alignment tests and honest host preconditions | closed |
| T-02-06 | Denial of service | scheduler | high | mitigate | Capacity, sequence/tick/counter preflight and 4096-event budget with explicit resource-stop | closed |
| T-02-07 | Tampering | digest | high | mitigate | Explicit endian field audit, logical queue serialization, inclusion/exclusion mutation tests | closed |
| T-02-08 | Tampering | trace ring | medium | mitigate | Copy-only bounded ring, visible drops and off/on/overflow digest equality | closed |
| T-02-09 | Tampering | bus decode | high | mitigate | Full-range checked byte/word access, no alias fallback, endpoint and wrap tests | closed |
| T-02-10 | Tampering | MI | medium | mitigate | Six-bit separated fields, command truth tables, set-wins policy and read-only pending tests | closed |
| T-02-11 | Tampering | PI request | high | mitigate | Whole-span preflight, immutable descriptors and reserved completion capacity | closed |
| T-02-12 | Tampering | PI cancellation | high | mitigate | Nonwrapping transaction tokens, canceled-no-copy completion and stale-event negative cases | closed |
| T-02-13 | Spoofing | PI evidence | medium | mitigate | Named atomic/timing policies and source-labeled independent expected ticks | closed |
| T-02-14 | Tampering | SP row arithmetic | high | mitigate | Check every complete row and all payload/deadline arithmetic; endpoint/wrap/skip tests | closed |
| T-02-15 | Denial of service | SP queue promotion | high | mitigate | Reserve both completion events before acceptance; private queue/sequence/token exhaustion tests | closed |
| T-02-16 | Tampering | SP source visibility | high | mitigate | Direct completion-time reads, mutation tests, no scheduled snapshot allocation | closed |
| T-02-17 | Tampering | fuzz harness | high | mitigate | Full public admission per case, exact seed/inventory and independent normalized field observations | closed |
| T-02-18 | Spoofing | sanitizer control | high | mitigate | Separate child, report+nonzero requirement, no-report and zero-exit negative cases | closed |
| T-02-19 | Spoofing | model-result consumer | high | mitigate | Distinct schema, mandatory native-model-probe scope, closed model outcomes, unit-tested stamp, strict malformed/false-evidence rejection | closed |
| T-02-20 | Tampering | complete machine state | high | mitigate | Interleave/partition/trace equivalence plus independent expected bytes/counters and independent reviews | closed |
| T-02-21 | Spoofing | model/guest evidence boundary | high | mitigate | Distinct canonical model schema and real result/identity/seal/suite/claim rejection tests with positive harness control | closed |
| T-02-22 | Tampering | test registration inventory | high | mitigate | Explicit complete name set before CTest; single-registration removal controls use production inventory checker | closed |

## Accepted Risks Log

No accepted risks.

## Security Audit Trail

| Audit date | Scope | Result | Run by |
|---|---|---|---|
| 2026-11-10 | Authored plan register before source-review fixes | All declared threats closed; source review still mandatory | gsd-security-auditor |
| 2026-11-10 | Changed digest spans, profile timing/sequence/serialization, build identity, file handling, process deadlines and fuzz snapshot oracle | SECURED, no open threats or accepted risks; identified stale component sanitizer evidence | gsd-security-auditor source-delta review |
| 2026-11-10 | Corrected sanitizer evidence and final alias test coverage | Full build and refreshed component/control selection recorded; valid unchanged fuzz retained | gsd-code-fixer and independent final recheck |

## Evidence and Limits

`02-REVIEW-FIX-2.md` supersedes the first fix report's stale component sanitizer
result. The corrected full ASan build refreshed all targets and the selected
13 component/model/control tests passed. The production admission fuzz run
performed 250,148 calls in 69.10 seconds within its 120-second limit; its binary
and unchanged harness were independently checked. The final recheck records the
retained-log limitations. AddressSanitizer and UndefinedBehaviorSanitizer were
active; this host cannot enable LeakSanitizer. No protected content, external
emulator source or new package dependency was added. DMA deadlines and native
model success do not establish hardware accuracy or boot compatibility.

## Sign-Off

- [x] All declared threats have a mitigation and are closed
- [x] No accepted risks
- [x] `threats_open: 0`
- [x] Scoped source review and correction evidence recorded

**Approval:** verified 2026-11-10 for the authored L1 register and reviewed delta.
