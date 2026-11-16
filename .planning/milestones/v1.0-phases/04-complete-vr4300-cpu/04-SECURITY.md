---
phase: "04"
slug: complete-vr4300-cpu
status: verified
threats_open: 0
asvs_level: 1
security_block_on: high
created: "2026-11-12"
scope:
  included: ["04-01 through 04-15", "04-17", "04-18 native verification", "04-16 registered pipeline mitigations", "04-19 compatibility profile mitigations", "04-20 compatibility v2 mitigations"]
  excluded: ["SC4 functional completion and hardware qualification"]
---

# Phase 04 — Security

This is the persisted security result for the executed native CPU scope. The
saved audit verified 33 distinct threats from Plans 01–15 and 17; a bounded
recheck closed the output-boundary finding after its fix. A first-time Plan18
audit then closed its two native evidence threats. The incremental Plan16
audit closes the registered pipeline qualification and private-output
mitigations. Plan16 functional completion and SC4 remain open.

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Guest input to deterministic machine | ROM admission and CPU execution must validate bounded, identified input before machine state changes. | Authored fixture bytes and model state. |
| Local diagnostic output to repository or host temp | Authored diagnostic execution and batch output are local-only and must not enter public evidence routes. | Local test output; no private results in this report. |
| Tested source to hosted evidence | Local and hosted results must identify the same exact public source and build configuration. | Source revision, build identity, test outcomes. |
| Rights-bearing external material to qualification | Source, tools, packages, hooks and composed assets are qualified before affected use. | Verified in the incremental Plan16 scope; private records remain local. |

## Threat Register

The 33-threat saved result covers Plans 01–15 and 17. Plan18's two additional
threats were audited separately against final source
`900833d85e23df2c8c02e68055f1b81e4a76f4f2`. Every listed disposition is
`mitigate`; no risks were accepted or transferred. The `T-04-SC` closure is
retained for incorporation within the native scope. The incremental audit
separately verifies its external tool/package/asset extension.

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-04-01 | Tampering | descriptor/admission | high | mitigate | Validate the immutable v3 descriptor and IPL3 asset/payload before admission; reject changed boot metadata transactionally. | closed |
| T-04-02 | Information disclosure | profile/build serialization | medium | mitigate | Hash only relative source identifiers and canonical integers; exclude pointers and host paths. | closed |
| T-04-03 | Tampering | instruction commit | high | mitigate | Stage destinations/stores and validate fetch/fault priority before architectural commit. | closed |
| T-04-04 | Denial of service | exception progress | high | mitigate | Bound guest-exception rescheduling with event, guest, and wall limits. | closed |
| T-04-05 | Tampering | integer/branch state | high | mitigate | Use unsigned limb arithmetic, explicit sign extension, and staged commits with extrema and delay-fault tests. | closed |
| T-04-06 | Denial of service | divide/shift edge encodings | medium | mitigate | Handle zero divisors and boundary shifts without undefined host operations; retain instrumented edge tests. | closed |
| T-04-07 | Elevation of privilege | CP0 and address mode | high | mitigate | Enforce KSU/EXL/ERL/CU0 and defined CP0 ownership on every path. | closed |
| T-04-08 | Tampering | CP0 writable fields | high | mitigate | Test fixed/zero/reserved fields and preserve state on invalid writes. | closed |
| T-04-09 | Tampering | timer/interrupt ordering | high | mitigate | Use one synchronization primitive and serialized equality generation; test same-tick ordering. | closed |
| T-04-10 | Denial of service | timer wrap/idle advance | high | mitigate | Compute checked deadlines and modular Count distance without per-tick loops or queue flooding. | closed |
| T-04-11 | Elevation of privilege | segment/TLB translation | high | mitigate | Validate address width and privilege before translation; test forbidden aliases. | closed |
| T-04-12 | Tampering | TLB writes/duplicate matches | high | mitigate | Validate PageMask/index before mutation; duplicate mappings trigger shutdown rather than first-match selection. | closed |
| T-04-13 | Tampering | partial unaligned stores | high | mitigate | Validate all touched bytes before commit and preserve untouched bytes on faults. | closed |
| T-04-14 | Elevation of privilege | unaligned page boundary | high | mitigate | Translate each actually touched range under the same privilege policy; avoid speculative adjacent-page access. | closed |
| T-04-15 | Tampering | conditional store atomicity | high | mitigate | Resolve faults before SC and stage memory plus register results together. | closed |
| T-04-16 | Tampering | unobserved external writer | high | mitigate | Notify link-overlap state for bus, PI, and SP memory commits. | closed |
| T-04-17 | Tampering | instruction cache alias state | high | mitigate | Use virtual index and physical tag with checked line fill; test stale code and synonyms. | closed |
| T-04-18 | Denial of service | line-fill fault | medium | mitigate | Validate backing range and staged allocation before replacement; preserve state on failed fill. | closed |
| T-04-19 | Tampering | dirty-line writeback | high | mitigate | Validate the full backing destination and commit data/tag state together. | closed |
| T-04-20 | Tampering | DMA/cache/link coherence | high | mitigate | Keep DMA unsnooped and route device writes through link-overlap notification. | closed |
| T-04-21 | Tampering | copied arithmetic closure | high | mitigate | Audit the pinned per-file source, notices, hashes, dependency closure, provenance, and independent tests. | closed |
| T-04-22 | Information disclosure | ambient arithmetic state | high | mitigate | Use explicit arithmetic context; interleaving tests and lint reject ambient mutable state. | closed |
| T-04-23 | Elevation of privilege | CU1 and COP1 register access | high | mitigate | Validate coprocessor enable, encoding, and register layout before FPR/FCR effects. | closed |
| T-04-24 | Tampering | FCR31 and destination commit | high | mitigate | Separate causes/enables/sticky flags and suppress disallowed destination writes. | closed |
| T-04-25 | Tampering | rounding and exception translation | high | mitigate | Use explicit rounding/flags, independent tie vectors, and separate VR4300 exception handling. | closed |
| T-04-26 | Information disclosure | cross-instance FP flags | high | mitigate | Interleave machines with separate FPR/FCR/context state and lint out ambient state. | closed |
| T-04-27 | Tampering | diagnostic protocol/inventory | high | mitigate | Validate exact schema and source/profile/build/overlay/inventory/ROM identities and full membership. | closed |
| T-04-28 | Denial of service | diagnostic transcript bounds | high | mitigate | Precompute encoded bounds and reject overflow/truncation while retaining the 4096-byte limit. | closed |
| T-04-29 | Tampering | input snapshot/selector | high | mitigate | Use immutable verified snapshots, exact digest/size/descriptor checks, and closed selector receipts. | closed |
| T-04-30 | Information disclosure | private diagnostic output | high | mitigate | Refuse CI/required-fixture execution; restrict batch destinations to ignored repository paths or secure temp; fail closed if Git inspection is unavailable. | closed |
| T-04-33 | Tampering | raw arithmetic expectations | high | mitigate | Require independent integer/bit/rational derivations and complete operation/category/rounding coverage through production wrappers. | closed |
| T-04-34 | Information disclosure | arithmetic context isolation | high | mitigate | Interleave rounding/flag cases and test explicit context isolation with source/object lint. | closed |
| T-04-35 | Information disclosure | public native evidence | high | mitigate | Sanitize authored-only evidence and pass rights, PII, and provenance scans before publication. | closed |
| T-04-36 | Tampering | current-source evidence binding | high | mitigate | Record local source/build identity and verify both hosted jobs against the final exact source. | closed |
| T-04-31 | Tampering | diagnostic tool/source/composition correspondence | high | mitigate | Qualify exact inputs and hooks before use; verify applied source, actual composition, complete source membership and sealed production adapter. | closed |
| T-04-32 | Information disclosure | local diagnostic inputs and outputs | high | mitigate | Confine ignored private storage, refuse CI use and pass publication rights/PII checks; public uploads contain authored evidence only. | closed |
| T-04-SC | Tampering | copied arithmetic and external supply chain | high | mitigate | Preserve native copy/provenance verification and the separately audited external source/tool/package/asset qualification. | closed |
| T-04-38 | Tampering | strict/compatibility policy and identity isolation | high | mitigate | Canonical immutable profiles, per-machine policies, distinct v10 origin/state domain and strict vector/refusal controls. | closed |
| T-04-39 | Tampering | sealed compatibility selection and result binding | high | mitigate | Closed profile selection, before/after immutable seal and executable checks, actual profile/core/inventory comparison and shared cross-profile aggregation controls. | closed |
| T-04-40 | Tampering | v2 version/instance/state/fault isolation | high | mitigate | Immutable canonical profile, v11 latch domain, staged coprocessor effects and old-profile controls. | closed |
| T-04-41 | Tampering | targeted evidence and v2 selector/full-set binding | high | mitigate | Preserve full source membership and shared aggregation; close profile mapping and immutable identity checks, with cross-profile controls. | closed |
| T-04-42 | Tampering | zero-data exclusions and store atomicity | high | mitigate | Bound complete v2 data ranges, exclude probe/apertures/fetch/DMA, and reject writes before cache/register/reservation mutation. | closed |

## Accepted Risks Log

No accepted risks.

## Scope Exclusions and Threat Flags

- The historical native audits excluded Plan16. The 2026-11-13 incremental
  audit verifies its registered mitigation scope without retroactively upgrading
  earlier unbound attempts or changing first-use history.
- No Plan16 SUMMARY exists while its completion requirement is unresolved; an
  executor Threat Flags section therefore could not be checked. This is an
  explicit audit limitation, not an invented absence of flags.
- The Plan18 summary contains no `## Threat Flags` section. The 16 prior
  summaries in the saved audit also contained no such flags.
- No user approval or risk acceptance was inferred.

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-11-12 | 33 distinct native-scope threats; T-04-30 rechecked after fix | 33 | 0 | gsd-security-auditor (Plans 01–15 and 17; bounded rechecks) |
| 2026-11-12 | 2 Plan18 native evidence threats | 2 | 0 | gsd-security-auditor (Plan18 only) |

The Plan18 audit tied its findings to source `900833d85e23df2c8c02e68055f1b81e4a76f4f2`, both exact-source hosted jobs, the build identities, and the portable evidence record. Local rights/PII/provenance scan outcomes were supplied from the orchestrator's recorded execution; the auditors did not rerun tests or scans.

## Sign-Off

- [x] All in-scope threats have a disposition.
- [x] Accepted risks documented; none were accepted.
- [x] `threats_open: 0` for this executed native scope.
- [x] `status: verified` set in frontmatter.

**Approval:** native scope verified 2026-11-12; registered Plan16 pipeline
mitigations verified 2026-11-13. This is a security mitigation verdict, not phase
completion, hardware evidence, or authority to change the verification target.

## Plan16 Incremental Mitigation Verification

The configured security auditor returned SECURED for T-04-31, T-04-32 and the
external extension of T-04-SC. It inspected saved pre-use source/tool/package/
hook/asset records, current source correspondence and actual composition
receipts. Sealed artifact and adapter digests matched. Saved actual build
receipts recorded successful commands, no deadline expiry and process-group
cleanup. The final focused normal and ASan checks and current tree rights,
PII and provenance scans passed. The auditor repeated no tests, installations
or package audits.

The implementation enforces qualification before Cargo, exact applied overlay
bytes, complete reviewed package/hook closure, actual image composition and
source-derived inventory membership. The adapter is selected by a sealed
canonical path and binary digest checked before and after every real
invocation; C also checks the selected core/profile identities. Ignored private
storage, confined owner-only paths, CI refusal and restricted public evidence
uploads enforce the disclosure boundary. Publication history scanning remains
an orchestrator gate before a push.

No external diagnostic names, denominators, statuses, images, captures or
identities are reported here. No risk was accepted or transferred. SC4 remains
unmet, and the owner verification-target decision is still pending.

## Security Audit 2026-11-13

| Metric | Count |
|---|---|
| Threats found | 37 |
| Closed | 37 |
| Open | 0 |

## Plan19 incremental security verification

The owner approved the separate compatibility target on 2026-11-13, resolving
the earlier pending target decision. The configured auditor returned SECURED
for the two additional registered threats and bounded rechecks of changed
arithmetic, CP0, reservation and runner mitigations. The register now contains
39 closed threats and none open. [Plan19 security evidence](04-19-SECURITY.md)
records the exact scope, supplied verification and limitations. No actual
diagnostic result was inspected or published, and this verdict does not close
SC4 or establish silicon behavior. Publication scans remain a separate gate.

## Plan20 incremental security verification

The configured auditor returned SECURED for three additional registered threats
and the changed mitigation paths, then confirmed the narrow COP1 destination
correction. The register now contains42 closed threats and none open.
[Plan20 evidence](04-20-SECURITY.md) preserves source scope, test-evidence
limits, the missing-SUMMARY flag-check limitation and the correction addendum.
It does not close SC4, clear publication rights or establish silicon behavior.

## Final summary reconciliation

Plans16/20 now have completed summaries with explicit Threat Flags sections.
They introduce no unregistered threat or accepted risk. The published source
is the same reviewed implementation; final exact-source hosted checks and
private gate observation add evidence without changing mitigation code. The
saved42-threat closure remains the security scope; functional reporting and
silicon qualification remain distinct. Earlier pending-plan statements above
are historical checkpoints, not current open threats.
