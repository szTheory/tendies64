---
phase: "07"
slug: rdp-reference-renderer-and-vi-scanout
status: blocked
threats_open: 3
implemented_authored_threats_open: 0
asvs_level: 1
audit_source: 2c8e9ef487792970b588b14ea253f4a706d0a861
---

# Phase07 security audit

The independent GSD security auditor verified all 41 authored plan rows (29
unique IDs). Implemented authored scope is SECURED: 38 applicable rows closed,
with no reachable high/critical mitigation gap found. The whole-phase verdict
is OPEN_THREATS: three high-severity future oracle rows remain open. They block
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

| Plan | Threat | Category | Component | Severity | Disposition | Actual mitigation / limit | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 01 | T-07-01 | Tampering | DPC spans and CPU fixture | high | mitigate | core/t64_rdp.c:177,871; malformed-span/truncation controls in core/tests/test_rdp.c:1207 | CLOSED |
| 01 | T-07-02 | Information disclosure | video copy-out and packet allocation | high | mitigate | core/t64_vi.c:56,1287; atomic copy refusal at core/tests/test_vi.c:1987 | CLOSED |
| 01 | T-07-03 | Tampering | fixture/provenance and production ownership | high | mitigate | scripts/build-rdp-fill-fixture.sh:37,216; runner/t64_real_fixture.c:51; strict source/composition mutation controls | CLOSED |
| 01 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |
| 02 | T-07-04 | Tampering | DPC fetch and fence state | high | mitigate | core/t64_rdp.c:793,871,904; repeated-fence/prior-work controls at core/tests/test_rdp.c:680,1115 | CLOSED |
| 02 | T-07-05 | Elevation of privilege | RSP profile dispatch | high | mitigate | core/t64_rsp.c:1827,2253; accepted/new and refused/old fetched transport controls | CLOSED |
| 02 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |
| 03 | T-07-06 | Tampering | TMEM/TLUT loads | high | mitigate | core/t64_rdp_tmem.c:227; full span/format checks before apply; core/tests/test_rdp_copy.c:510,589 | CLOSED |
| 03 | T-07-07 | Denial of service | copy raster coordinate arithmetic | high | mitigate | core/t64_rdp_tmem.c:511; bounded scheduled row at core/t64_rdp.c:312; overflow/zero-area controls | CLOSED |
| 03 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |
| 04 | T-07-08 | Tampering | raw triangle parser | high | mitigate | core/t64_rdp_raster.c:150; complete packet at core/t64_rdp.c:871; all-group prefix controls | CLOSED |
| 04 | T-07-09 | Denial of service | edge/shade fixed-point raster | high | mitigate | core/t64_rdp_raster.c:40,164,595; checked products/clips and bounded rows; negative/extreme controls | CLOSED |
| 04 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |
| 05 | T-07-10 | Tampering | texture interpolation/TMEM | high | mitigate | core/t64_rdp_raster.c:64; W crossing refusal; TMEM format preflight at core/t64_rdp_tmem.c:898 | CLOSED |
| 05 | T-07-11 | Denial of service | textured raster | high | mitigate | core/t64_rdp.c:312; core/t64_rdp_raster.c:595; partition/interleave regression in texture tests | CLOSED |
| 05 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |
| 06 | T-07-12 | Tampering | combiner arithmetic/selectors | high | mitigate | core/t64_rdp_pipeline.c:132; independent selectors, signed floor/clamp and saturation controls | CLOSED |
| 06 | T-07-13 | Information disclosure | noise/undefined memory | high | mitigate | core/t64_rdp.c:664; initialized per-instance RNG/temporaries, refusal and digest at core/t64_digest.c:597 | CLOSED |
| 06 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |
| 07 | T-07-14 | Information disclosure | coverage sidecar | high | mitigate | core/t64_rdp_pipeline.c:209,252; machine reset/allocation and bounded backing-write invalidation; alias/lifecycle controls | CLOSED |
| 07 | T-07-15 | Tampering | blend/coverage modes | high | mitigate | core/t64_rdp_pipeline.c:374; independent blender/coverage selector, destination and overlap controls | CLOSED |
| 07 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |
| 08 | T-07-16 | Tampering | depth image/hidden state | high | mitigate | core/t64_rdp_depth.c:52,68,83,149; aligned complete spans; boundary/alias regressions | CLOSED |
| 08 | T-07-17 | Denial of service | depth packet/interpolation | high | mitigate | core/t64_rdp_depth.c:96; preflight before effects at core/t64_rdp_raster.c:272; Z-prefix/recovery matrix | CLOSED |
| 08 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |
| 09 | T-07-18 | Tampering | VI geometry/source spans | high | mitigate | core/t64_vi.c:56; checked source geometry/backing span distinct from output capacity; high-row/overflow controls | CLOSED |
| 09 | T-07-19 | Denial of service | VI rational phase/events | high | mitigate | core/t64_vi.c:218,234,1150; checked rational phase and event reservation; whole/split/interleave controls | CLOSED |
| 09 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |
| 10 | T-07-20 | Information disclosure | VI filter neighborhoods | high | mitigate | core/t64_vi.c:279; initialized fallback and checked RDRAM for each tap; positive next-row and actual RAM-end controls | CLOSED |
| 10 | T-07-21 | Denial of service | raw packet backpressure | high | mitigate | core/t64_scheduler.c:276; core/t64_vi.c:989; producing-event hold before dispatch; full/resume/no-loss controls | CLOSED |
| 10 | T-07-22 | Tampering | observer spans/cursor | high | mitigate | core/t64_vi.c:1287; disjoint spans/capacity validated before copy/drain; atomic invalid-span controls | CLOSED |
| 10 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |
| 11 | T-07-23 | Spoofing | qualification originals and cases | high | mitigate | runner/tests/test_rdp_video.c:473; original command/outcome/executable binding, independent denominator and public bytes; native source/process-group controls | CLOSED |
| 11 | T-07-24 | Information disclosure | public hosted artifacts | high | mitigate | private originals precede text projection; exact five-file upload whitelist; CI rights/PII before upload; no ROM or boot bytes exported | CLOSED |
| 11 | T-07-25 | Repudiation | hosted artifact identity | high | mitigate | exact source/run/attempt/job and artifact-ID API checks; bounded ZIP, producer status, API digest before strict extraction; decoded host bytes compared | CLOSED |
| 11 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |
| 12 | T-07-26 | Elevation of privilege | oracle authority admission | high | mitigate | Plan12 authority checker/tests/record absent: exact scope/revision/rights admission is not implemented; existing authority stop remains | OPEN — blocked future route |
| 12 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |
| 13 | T-07-27 | Spoofing | oracle/input/output identity | high | mitigate | Plan13 differential runner/tests/evidence absent: oracle command/binary/output identity controls are not implemented or reached | OPEN — blocked future route |
| 13 | T-07-28 | Information disclosure | oracle-derived output/publication | high | mitigate | Plan13 publication route absent: oracle pre-use rights and separate derived-data publication enforcement are not implemented | OPEN — blocked future route |
| 13 | T-07-SC | Tampering | Package installs | high | mitigate | No new package/lock/dependency or install operation introduced; existing two jobs, action pins and ten-minute bounds retained; future acquisition remains unapproved | CLOSED |


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
normal gate, full ASan/UBSan inventory and pinned GCC route. Exact live hosted
raw-artifact qualification remains pending. No private diagnostic, oracle,
physical hardware, release, or dependency acquisition was performed.

## Accepted risks

No accepted risks. Existing D2/D5/D6 stops and expired private windows remain
unchanged. Plans12/13 and VID-04 remain incomplete.

## Security audit trail

| Audit source | Rows | Closed | Open high | Implemented open high | Auditor |
| --- | ---: | ---: | ---: | ---: | --- |
| 2c8e9ef | 41 | 38 | 3 | 0 | Independent gsd-security-auditor, ASVS1 |

Whole-phase security sign-off remains blocked. No owner action is needed for
the continuing authored hosted qualification.
