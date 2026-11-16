---
phase: 06
asvs_level: 1
block_on: high
verdict: OPEN_THREATS
threats_total: 33
threats_open: 1
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
| 07 | T-06-18 | CLOSED | Historical checker forwarded caller expected_rom_sha256 without independent bound-image recomputation |
| 07 | T-06-19 | CLOSED | scripts/check-base-evidence.py:38,76,181,685; scripts/tests/test_base_evidence.py PublicEvidenceControls |
| 07 | T-06-20 | CLOSED | scripts/check-base-evidence.py original CTest/attempt/artifact reconstruction and bounded claim equality; scripts/tests/test_base_evidence.py PublicEvidenceControls |
| 08 | T-06-21 | CLOSED | Conclusive code review docs/evidence/phase06-review.md plus this complete security register |
| 08 | T-06-22 | CLOSED | Pending correction/re-review and stable final gate/artifact qualification |
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

## Scoped correction re-audit

At f537baf the configured security auditor returned SECURED for T-06-18
(1/1, zero scoped open threats). The context-bound image loader and full-source
seam feed independent base-byte/selector hashing before C classification.
The copied actual-record control coherently changes record/input/report while
keeping context/image fixed, and reaches the named independent-image refusal.
The default authored selector control and both normal/instrumented owner logs
support the same guard. No specific scoped regression or unregistered flag was
found. Code re-review also returned a conclusive closure of the same two-file
slice; unaffected full-phase review is retained. Only T-06-22 and T-06-23 remain
open for the planned final artifact gates and hosted readback.

## Strict seal policy re-audit

The first final gate reached all normal tests and rejected a stale pre-RDRAM
no-HLE roster plus two old SP body seals. The failed gate is retained under
build/gate/06-08-final/; its unreached core-lint and sanitizer tail cannot count
as passed. At acde6e3 both configured reviewers verified the exact two-file
policy correction: the reviewed RDRAM source/object is explicit, replacement
SP hashes match independent normalization, and new injected-effect controls
cover those same functions. Unknown sources/objects and body changes remain
rejected. No C source changed, no regression/unregistered threat was found.
Direct controls passed against normal/instrumented archives. T-06-22/23 still
require the stable final gates and hosted readback.

## Final local artifact closure and original-reference re-audit

The complete canonical normal gate passed on 7554ef0; the all-target ASan/UBSan
suite, exact inventory, admission fuzz and sanitizer negative control also
passed. Source/diff freeze and retained log hashes were checked. Exact tested
executables are copied and hashed in build/gate/06-08-final/inputs/binding.json.
The existing local GCC13 image passed the network-disabled, read-only warnings
check. Final private source/tool/lock/asset/image composition qualification and
exact consumer/binding retention passed; no diagnostic guest was replayed.

Final readback found that later identical authored startup outputs made a
glob-based historical lookup ambiguous. Slice3 at bf7046f resolves the receipt's
exact closed path and command/outcome hashes, verifies the original CTest
embedding and normal command, and has no fallback to later attempts. Both
configured scoped reviews closed T-06-20 without regression. The private-free
repeat-output control and normal/instrumented owning CTests passed; saved
artifact/original-record/live pre-use checks pass. The full native suite's C,
compiler and build inputs are unchanged by this host parser slice; its focused
evidence is retained separately rather than relabeling the native executable.
T-06-22 is closed by that combined scoped evidence. Only T-06-23 awaits the
exact authored hosted pair and artifact readback.
