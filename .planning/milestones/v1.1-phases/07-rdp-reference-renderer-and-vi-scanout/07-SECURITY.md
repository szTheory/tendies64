---
phase: "07"
slug: rdp-reference-renderer-and-vi-scanout
status: blocked
threats_open: 2
implemented_authored_threats_open: 0
asvs_level: 1
audit_source: 2c8e9ef487792970b588b14ea253f4a706d0a861
---

# Phase07 security audit

The independent GSD security auditor verified all 41 authored plan rows (29
unique IDs). Implemented authored scope is SECURED: 39 applicable rows closed,
with no reachable high/critical mitigation gap found. The whole-phase verdict
is OPEN_THREATS: two high-severity future oracle rows remain open. They block
whole-phase completion and oracle use, not the authorized authored qualification.
These are not implemented mitigations or accepted risks.

## Trust boundaries

Untrusted guest command/register spans enter the canonical machine; bounded raw
output crosses into host observers; original source/process/artifact records
cross into claims and separately scanned public exports. Oracle authority and
publication remain a separate, currently unavailable boundary.

## Complete threat register

All rows retain their authored severity high and disposition mitigate. Supply-chain
occurrences remain separate per plan. Source references below are audit observations;
planned tests alone are not passing evidence.

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
| --- | --- | --- | --- | --- | --- | --- |
| T-07-01 | Tampering | DPC spans and CPU fixture (plan01) | high | mitigate | core/t64_rdp.c:177,871; malformed-span/truncation controls in core/tests/test_rdp.c:1207 | closed |
| T-07-02 | Information disclosure | video copy-out and packet allocation (plan01) | high | mitigate | core/t64_vi.c:56,1287; atomic copy refusal at core/tests/test_vi.c:1987 | closed |
| T-07-03 | Tampering | fixture/provenance and production ownership (plan01) | high | mitigate | scripts/build-rdp-fill-fixture.sh:37,216; runner/t64_real_fixture.c:51; strict source/composition mutation controls | closed |
| T-07-SC | Tampering | Package installs (plan01) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |
| T-07-04 | Tampering | DPC fetch and fence state (plan02) | high | mitigate | core/t64_rdp.c:793,871,904; repeated-fence/prior-work controls at core/tests/test_rdp.c:680,1115 | closed |
| T-07-05 | Elevation of privilege | RSP profile dispatch (plan02) | high | mitigate | core/t64_rsp.c:1827,2253; accepted/new and refused/old fetched transport controls | closed |
| T-07-SC | Tampering | Package installs (plan02) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |
| T-07-06 | Tampering | TMEM/TLUT loads (plan03) | high | mitigate | core/t64_rdp_tmem.c:227; full span/format checks before apply; core/tests/test_rdp_copy.c:510,589 | closed |
| T-07-07 | Denial of service | copy raster coordinate arithmetic (plan03) | high | mitigate | core/t64_rdp_tmem.c:511; bounded scheduled row at core/t64_rdp.c:312; overflow/zero-area controls | closed |
| T-07-SC | Tampering | Package installs (plan03) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |
| T-07-08 | Tampering | raw triangle parser (plan04) | high | mitigate | core/t64_rdp_raster.c:150; complete packet at core/t64_rdp.c:871; all-group prefix controls | closed |
| T-07-09 | Denial of service | edge/shade fixed-point raster (plan04) | high | mitigate | core/t64_rdp_raster.c:40,164,595; checked products/clips and bounded rows; negative/extreme controls | closed |
| T-07-SC | Tampering | Package installs (plan04) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |
| T-07-10 | Tampering | texture interpolation/TMEM (plan05) | high | mitigate | core/t64_rdp_raster.c:64; W crossing refusal; TMEM format preflight at core/t64_rdp_tmem.c:898 | closed |
| T-07-11 | Denial of service | textured raster (plan05) | high | mitigate | core/t64_rdp.c:312; core/t64_rdp_raster.c:595; partition/interleave regression in texture tests | closed |
| T-07-SC | Tampering | Package installs (plan05) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |
| T-07-12 | Tampering | combiner arithmetic/selectors (plan06) | high | mitigate | core/t64_rdp_pipeline.c:132; independent selectors, signed floor/clamp and saturation controls | closed |
| T-07-13 | Information disclosure | noise/undefined memory (plan06) | high | mitigate | core/t64_rdp.c:664; initialized per-instance RNG/temporaries, refusal and digest at core/t64_digest.c:597 | closed |
| T-07-SC | Tampering | Package installs (plan06) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |
| T-07-14 | Information disclosure | coverage sidecar (plan07) | high | mitigate | core/t64_rdp_pipeline.c:209,252; machine reset/allocation and bounded backing-write invalidation; alias/lifecycle controls | closed |
| T-07-15 | Tampering | blend/coverage modes (plan07) | high | mitigate | core/t64_rdp_pipeline.c:374; independent blender/coverage selector, destination and overlap controls | closed |
| T-07-SC | Tampering | Package installs (plan07) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |
| T-07-16 | Tampering | depth image/hidden state (plan08) | high | mitigate | core/t64_rdp_depth.c:52,68,83,149; aligned complete spans; boundary/alias regressions | closed |
| T-07-17 | Denial of service | depth packet/interpolation (plan08) | high | mitigate | core/t64_rdp_depth.c:96; preflight before effects at core/t64_rdp_raster.c:272; Z-prefix/recovery matrix | closed |
| T-07-SC | Tampering | Package installs (plan08) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |
| T-07-18 | Tampering | VI geometry/source spans (plan09) | high | mitigate | core/t64_vi.c:56; checked source geometry/backing span distinct from output capacity; high-row/overflow controls | closed |
| T-07-19 | Denial of service | VI rational phase/events (plan09) | high | mitigate | core/t64_vi.c:218,234,1150; checked rational phase and event reservation; whole/split/interleave controls | closed |
| T-07-SC | Tampering | Package installs (plan09) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |
| T-07-20 | Information disclosure | VI filter neighborhoods (plan10) | high | mitigate | core/t64_vi.c:279; initialized fallback and checked RDRAM for each tap; positive next-row and actual RAM-end controls | closed |
| T-07-21 | Denial of service | raw packet backpressure (plan10) | high | mitigate | core/t64_scheduler.c:276; core/t64_vi.c:989; producing-event hold before dispatch; full/resume/no-loss controls | closed |
| T-07-22 | Tampering | observer spans/cursor (plan10) | high | mitigate | core/t64_vi.c:1287; disjoint spans/capacity validated before copy/drain; atomic invalid-span controls | closed |
| T-07-SC | Tampering | Package installs (plan10) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |
| T-07-23 | Spoofing | qualification originals and cases (plan11) | high | mitigate | runner/tests/test_rdp_video.c:473; original command/outcome/executable binding, independent denominator and public bytes; native source/process-group controls | closed |
| T-07-24 | Information disclosure | public hosted artifacts (plan11) | high | mitigate | private originals precede text projection; exact five-file upload whitelist; CI rights/PII before upload; no ROM or boot bytes exported | closed |
| T-07-25 | Repudiation | hosted artifact identity (plan11) | high | mitigate | exact source/run/attempt/job and artifact-ID API checks; bounded ZIP, producer status, API digest before strict extraction; decoded host bytes compared | closed |
| T-07-SC | Tampering | Package installs (plan11) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |
| T-07-26 | Elevation of privilege | oracle authority admission (plan12) | high | mitigate | Reviewed complete request/audit/decision and exact scope/revision/expiry admission; real CLI refuses absent authority; build/corpus readiness remains blocked; see 07-12-SECURITY-DELTA.md | closed |
| T-07-SC | Tampering | Package installs (plan12) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |
| T-07-27 | Spoofing | oracle/input/output identity (plan13) | high | mitigate | Plan13 differential runner/tests/evidence absent: oracle command/binary/output identity controls are not implemented or reached | open — blocked future route |
| T-07-28 | Information disclosure | oracle-derived output/publication (plan13) | high | mitigate | Plan13 publication route absent: oracle pre-use rights and separate derived-data publication enforcement are not implemented | open — blocked future route |
| T-07-SC | Tampering | Package installs (plan13) | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | closed |


## Evidence and limits

The auditor checked actual source boundaries and owning registered regressions,
verified unchanged core/common source against the prior independent reviews,
and matched saved actual-producer/mock-transport control hashes to current code.
The [initial review](07-11-SEGMENT-A-REVIEW-INITIAL.md),
[repair recheck](07-11-SEGMENT-A-REVIEW-RECHECK.md) and
[final recheck](07-11-SEGMENT-A-REVIEW-FINAL.md) retain findings and dispositions.
The independent review found and closed forged public bytes, process-tree
cleanup, raw ZIP extraction/digest, dirty source binding, and streaming size
issues. The final collector control has 34 rejection cases and explicitly
reports mocked transport; it establishes no live hosted result.

The later [native qualification](07-11-NATIVE-EVIDENCE.md) passed the complete
normal gate, full ASan/UBSan inventory and pinned GCC route. Exact live hosted raw/boot qualification also passed; see phase07-hosted.json. No private diagnostic, oracle,
physical hardware, release, or dependency acquisition was performed.

## Accepted risks

No accepted risks. Existing D2/D5/D6 stops and expired private windows remain
unchanged. Plan12 authored admission is implemented; Plan13 and VID-04 remain blocked by actual authority and readiness.

## Security audit trail

| Audit source | Rows | Closed | Open high | Implemented open high | Auditor |
| --- | ---: | ---: | ---: | ---: | --- |
| 2c8e9ef | 41 | 38 | 3 | 0 | Independent gsd-security-auditor, ASVS1 |

The [Plan12 delta](07-12-SECURITY-DELTA.md) closes authority admission after independent review and reached controls: 39/41 rows closed, two future high rows open. Whole-phase sign-off stays blocked; no acknowledgment is needed to preserve the stop.

## Orchestrator note (2026-11-20)

The two open high-severity rows belong to the future oracle work of plan 07-13, which the owner deferred to milestone v1.2 together with VID-04 (2026-11-20). They stay OPEN, are not accepted, and continue to block any oracle execution, acquisition, copying or publication until the owner's D5 decision and the rights audit resolve them. The implemented authored scope remains SECURED (0 open authored threats).
