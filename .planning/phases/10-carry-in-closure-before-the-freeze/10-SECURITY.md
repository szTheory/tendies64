---
phase: "10"
slug: carry-in-closure-before-the-freeze
status: verified
threats_open: 0
asvs_level: 1
block_on: high
created: "2026-11-25"
audited_source: 50098d6
---

# Phase 10 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| State blob importer | Untrusted serialized machine state (state 26, container v2) applied to a live machine | Crafted depth scalars, hidden table, container/state words |
| Oracle authority record | Phase 7 request that must keep local_decision blocked | Manifest row identities, decision pins |
| CI and qualification | Bound CI source, timeouts, private qualification records | Workflow YAML, JUnit, private records under build/ |
| Authored composite guest | Authored-only ROM built from authored sources | Guest/ROM bytes, transcripts, save image |
| Published repository | Public commits and evidence docs | Commit content (PII, paths, protected material) |

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-10-01 | Info disclosure | preflight replay output, PIN-IMPACT | medium | mitigate | preflight check_pii and replay_cleanup; pii tree and range scans 0 findings | closed |
| T-10-02 | Tampering | p10-mutants worktree | low | mitigate | driver refuses a dirty tree (exit 2) | closed |
| T-10-03 | Tampering | digest sweep and shared fixtures | medium | mitigate | sweep roots under build/; fixtures read-only; selftest PASS | closed |
| T-10-04 | Repudiation | shipped-id equality claim | medium | mitigate | digest-sweep selftest proves both directions | closed |
| T-10-05 | Elevation of privilege | oracle authority checker | high | mitigate | trusted decision pins None; --require-approved exits 2; --expect-blocked exits 0; only orchestrator commit b64098f touched the checker | closed |
| T-10-06 | Tampering | per-row oracle binding | medium | mitigate | bound_rows checks refuse duplicate/missing/changed rows; selftest 26 controls | closed |
| T-10-07 | Repudiation | rebind record | low | mitigate | identity-only rebind section in phase07-oracle-authority.md | closed |
| T-10-08 | Tampering | check-ci-yml.sh rule change | high | mitigate | only CI-TIMEOUT rule changed with mutation cases; selftest 138 cases | closed |
| T-10-09 | DoS | cold-cache timeout | medium | mitigate | gate-macos timeout-minutes 15; measured cold 731 s of 900 s | closed |
| T-10-10 | Tampering | closed Phase 8 qualifier | medium | mitigate | qualify-audio.py and audio-cases.json unchanged since Wave 0 base | closed |
| T-10-11 | Info disclosure | pushed commits | high | mitigate | preflight pii step on tree and range; no ROM/save extension tracked | closed |
| T-10-12 | Tampering | shared build tree in review | medium | mitigate | procedural: reviews ran in a detached clone under build/10-review | closed |
| T-10-13 | Repudiation | VG literals (compression) | medium | mitigate | no production z_compress/z_decompress calls in test_vg_gaps.c; derivation comments | closed |
| T-10-14 | Tampering | oracle-bound test files | medium | mitigate | bound files absent from the diff; authority check exits 0 | closed |
| T-10-15 | Repudiation | VG literals | medium | mitigate | derivation comments; nine cases pass; named mutants killed | closed |
| T-10-16 | Tampering | bound Phase 7 test files | medium | mitigate | same as T-10-14 | closed |
| T-10-17 | Info disclosure | pushed commits, review notes | high | mitigate | pii tree and range 0 findings; build/ ignored | closed |
| T-10-18 | Tampering | machine create under unregistered id | high | mitigate | create refuses before allocation (allocations==0 test) | closed |
| T-10-19 | Tampering | shipped-id semantics via widened predicate | high | mitigate | exact tuple predicates; 25-id matrix; k1-predicate-widened mutant; sweep 0 differences | closed |
| T-10-20 | Repudiation | v26 digest completeness | medium | mitigate | flip-every-member and strip-differential tests; omission mutants killed | closed |
| T-10-21 | Tampering | stale depth sidecar | medium | mitigate | depth-composite-sidecar test; invalidation-removed mutant killed | closed |
| T-10-22 | Repudiation | overclaiming hardware accuracy | medium | mitigate | tier text "unit-tested, authored scope"; check-claims ok | closed |
| T-10-23 | Tampering | format/state pairing | medium | mitigate | state_request_admitted pairing; closed set {v1,v2}; wrong pairs refused at tick 0 | closed |
| T-10-24 | Info disclosure | exported state blobs | medium | mitigate | 0600 files, 0700 dirs, under ignored build/ | closed |
| T-10-25 | DoS | oversized v26 blob | low | mitigate | export-v26-bounds test; 64 MiB ceiling unchanged | closed |
| T-10-26 | Tampering | crafted depth address or extents | high | mitigate | ADDR_RANGE rule; extent and Z-product bound re-run from blob values; hostile rows; 16,689 checks | closed |
| T-10-27 | Tampering | hidden-table bytes | medium | mitigate | exact length plus per-byte ENUM_CLOSED rows | closed |
| T-10-28 | DoS | sanitizer-time growth | medium | mitigate | measured TIMEOUTs; preflight timeout audit | closed |
| T-10-29 | Tampering | partial apply on refusal | high | mitigate | validate-then-commit importer; hostile rows assert unchanged allocation | closed |
| T-10-30 | Repudiation | completeness of the identity bump | high | mitigate | completeness flip test over every member; omission mutants | closed |
| T-10-31 | Info disclosure | pushed commits | high | mitigate | preflight pii step; range scan 0 findings | closed |
| T-10-32 | Info disclosure | protected material in guest or ROM | high | mitigate | composite rows authored-here/build-only; rights and provenance scans 0 findings | closed |
| T-10-33 | Elevation of privilege | oracle request via manifest edit | high | mitigate | PINNED_REQUEST_SHA256 changed only in b64098f; per-row binding | closed |
| T-10-34 | DoS | capture over the per-file limit | low | mitigate | two-run passes under RLIMIT_FSIZE (artifact cap + 64 KiB); 4x3 window | closed |
| T-10-35 | Tampering | raw-capture exclusion | high | mitigate | supervisor refusal lines untouched; combined invocation refused in test | closed |
| T-10-36 | Repudiation | expected values | medium | mitigate | composite_oracle.py --check equals the expectation | closed |
| T-10-37 | Info disclosure | retained run dirs and logs | medium | mitigate | mkdtemp 0700 under ignored build/gate | closed |
| T-10-38 | Tampering | raw-capture exclusion | high | mitigate | same as T-10-35 | closed |
| T-10-39 | Repudiation | closed-phase records | medium | mitigate | no Phase 8/9 claim or evidence file changed | closed |
| T-10-40 | Info disclosure | doc edits | low | mitigate | pii tree scan 0 findings | closed |
| T-10-41 | Info disclosure | private qualification record and JUnit | medium | mitigate | record root 0700, record 0600, symlink refused; only sanitized summaries committed | closed |
| T-10-42 | Repudiation | rehearsal presented as final | medium | mitigate | --verify refuses a rehearsal record as final | closed |
| T-10-43 | Tampering | raw-capture exclusion | high | mitigate | runner diff adds pairing only; no raw-capture line changed | closed |
| T-10-44 | Elevation of privilege | authority state at phase end | high | mitigate | --require-approved exit 2; trusted pins None | closed |
| T-10-45 | Info disclosure | pushed commits | high | mitigate | pii range 0 findings; rights scan clean | closed |
| T-10-46 | DoS | cold-cache timeout loop | medium | mitigate | 900 s limit; cold run 731 s with cache saved | closed |
| T-10-47 | Info disclosure | evidence and claims | high | mitigate | pii/rights scans 0; no workstation path or email in phase10 docs | closed |
| T-10-48 | Repudiation | overclaiming | medium | mitigate | check-claims: 22 blocks, 0 errors; limits paragraph | closed |
| T-10-49 | Tampering | Phase 9 claim-block manifest binding | medium | mitigate | pinned manifest identity plus 39-row view; closed records unchanged; recorded exception flagged for owner review | closed |
| T-10-50 | Tampering | closed Phase 9 claims via the manifest commit | high | mitigate | no Phase 9 claim/evidence file in the diff; request pin unchanged since b64098f | closed |
| T-10-51 | Elevation of privilege | ISViewer reachable via widened predicate | medium | mitigate | SI-family ids only; v26-isviewer test accepts 8, refuses outside | closed |
| T-10-52 | Tampering | container version word vs state word | medium | mitigate | state_container_version_for pairing at writer and reader; (1,26) and (2,25) refused before apply | closed |
| T-10-SC | Tampering | package installs | low | accept | no dependency manifest or lockfile changed; no installs in Phase 10 | closed |

*Status: open · closed · open — below high threshold (non-blocking). Severity: critical > high > medium > low.*

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-10-01 | T-10-SC | Phase 10 acquires no packages, images or tools; the diff shows no dependency manifest or lockfile change | Plan disposition (accept) | 2026-11-25 |

Not a threat acceptance, but flagged for owner review: the plan 10-03 Task 4 comparator change for the Phase 9 claim-block manifest binding (T-10-49; see docs/evidence/phase10-qualification.md, "Recorded exceptions for owner review"). It rests on delegated autonomy and is not an owner approval.

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-11-25 | 53 | 53 | 0 | gsd-security-auditor (ASVS 1, block_on high; audited source 50098d6) |

Auditor re-ran the PII, rights, provenance, claims, CI-scanner, oracle-authority and digest-sweep checks and the new Phase 10 core and runner test binaries directly, all with zero findings or failures. Not re-run by the auditor: sanitizer qualification, mutant re-kills, a full ctest (covered by the native qualification record). Observations, no threat opened: T-10-12 is a process mitigation evidenced by SUMMARY records; no committed test asserts individual raw-capture file sizes beyond the supervisor limit.

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-11-25
