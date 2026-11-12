---
phase: "04"
slug: complete-vr4300-cpu
status: verified
threats_open: 0
asvs_level: 1
security_block_on: high
created: "2026-11-12"
scope:
  included: ["04-01 through 04-15", "04-17", "04-18 native verification"]
  excluded: ["04-16 external-tool/asset qualification and private SC4 run"]
---

# Phase 04 — Security

This is the persisted security result for the executed native CPU scope. The
saved audit verified 33 distinct threats from Plans 01–15 and 17; a bounded
recheck closed the output-boundary finding after its fix. A first-time Plan18
audit then closed its two native evidence threats. Plan16 remains pending and
is not represented as secured here.

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Guest input to deterministic machine | ROM admission and CPU execution must validate bounded, identified input before machine state changes. | Authored fixture bytes and model state. |
| Local diagnostic output to repository or host temp | Authored diagnostic execution and batch output are local-only and must not enter public evidence routes. | Local test output; no private results in this report. |
| Tested source to hosted evidence | Local and hosted results must identify the same exact public source and build configuration. | Source revision, build identity, test outcomes. |
| Rights-bearing external material to qualification | External test source, tools, and assets require a separate pre-use audit. | Excluded here; Plan16 remains pending. |

## Threat Register

The 33-threat saved result covers Plans 01–15 and 17. Plan18's two additional
threats were audited separately against final source
`900833d85e23df2c8c02e68055f1b81e4a76f4f2`. Every listed disposition is
`mitigate`; no risks were accepted or transferred. The `T-04-SC` closure is
limited to incorporation within the executed native scope and does not close
Plan16's external tool/package/asset qualification.

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
| T-04-SC | Tampering | copied arithmetic and external supply chain | high | mitigate | Close only the audited native copy/provenance scope; preserve the separate Plan16 pre-use audit gate. | closed |

## Accepted Risks Log

No accepted risks.

## Scope Exclusions and Threat Flags

- Plan16 and its T-04-31/T-04-32 external qualification were not audited here.
  The closed native-scope T-04-SC row does not approve external tool, package,
  asset, ROM, or result use.
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

**Approval:** verified 2026-11-12 for executed native scope. Plan16 remains pending.
