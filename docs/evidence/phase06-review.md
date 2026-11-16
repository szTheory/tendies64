# Phase 06 Source Review

**Reviewed:** 2026-11-16T01:07:34Z  
**Depth:** deep, adversarial boundary review  
**Snapshot:** `a80b82281b7aeffb32946b50f6fe11839516de8e..cc1e3f209fee1cc276bb01ecb26899ca6a3a853f`  
**Scope source:** `build/gate/06-08-review-scope.json` (51 paths; exact list below)  
**Verdict at original snapshot:** BLOCKED by one proven evidence-integrity defect (T06-18, also reported by the security auditor). The scoped correction re-review below closes T06-18; final phase disposition remains with the parent-owned gates.

## Review coverage and limits

The review traced the profile predicates and reset/digest state; RDRAM protocol, bus-span validation, feedback reads through bus/cache/SP DMA, and refusal ownership; T1 MDU issue costs, CP0 Count/Random, successor-event reservation and FIFO boundaries; and the whole BASE chain from independent source derivation and immutable pre-use snapshot through native C records, saved-record reclassification and closed public receipt validation. The original private evidence was handled through the validator with output suppressed; this report contains no member identities, counts, scores, raw records or outcomes.

The bounded controls observed during review were `python3 -B scripts/tests/test_base_evidence.py` (11 authored controls passed), the same control with `T64_BASE_EVIDENCE_ACTUAL_TEST=1` (12 controls passed, including a fully resealed reduction refusal), and `python3 -B scripts/check-base-evidence.py --check-current` (exit 0; output suppressed). The controls exercise accepted full-entry dispatch, independent denominator reduction, original terminal/capture/profile/cleanup/missing-record mutations, and public private-field/quoted-alias rejection. These checks establish those paths only. I did not run a CMake build, native or sanitizer suite, guest replay, image build, network install, long suite, service, or private upload; no such result is claimed here.

Detailed adversarial inspection focused on `core/t64_rdram.c`, `core/t64_bus.c`, `core/t64_cache.c`, `core/t64_cpu.c`, `core/t64_sp.c`, `core/t64_mi.c`, `core/t64_machine.c`, `core/t64_digest.c`, `runner/t64_base_diag_protocol.c`, `runner/t64_base_diagnostic.c`, `scripts/derive-base-diagnostic-source.py`, `scripts/verify-base-diagnostic-qualification.py`, `scripts/run-base-diagnostic.py`, `scripts/check-base-evidence.py`, and their BASE timing, protocol, and evidence controls. No additional defect was proven in those inspected paths. The irreversible historical Plan02 invocation gap remains unfilled and is outside this source finding.

## Findings

### BLOCKER — Saved evidence accepts a caller-supplied selected-ROM digest (T06-18)

**Evidence:** `scripts/check-base-evidence.py:1203-1234` validates saved member paths and forwards each saved manifest row, including `expected_rom_sha256`, into the retained C consumer. It checks the regenerated report and source completeness at `scripts/check-base-evidence.py:1244-1255`, but does not independently derive the selected image digest from the exact retained context, selector, and composed ROM before reclassification. The normal run path computes that digest from the selected image at `scripts/run-base-diagnostic.py:707-718`; the saved check trusts the value present in the persisted classification input. The test covering original-record mutation at `scripts/tests/test_base_evidence.py:409-444` does not mutate and reseal this selected-ROM field against an unchanged context.

**Impact:** A saved manifest and corresponding record can agree on an image digest that is not the independently selected ROM for that source member. Re-sealing the classification output then lets the saved checker accept a result bound to the wrong image. This breaks the checker’s evidence-integrity claim for retained member results.

**Correction queue:** Owning task: `06-07-PLAN.md`, Task 1, “Validate one actual saved record through the complete evidence entry” (T-06-18). Recompute each dispatchable member’s selected-ROM digest from the verified context’s exact retained composition/base image and selector, and compare it before invoking C classification. Add a full-entry negative control that changes the saved selected-ROM digest and dependent record/report hashes while leaving the verified image/context fixed; assert refusal. Keep the actual private input unchanged.

**Fix slice 1 (two files):** `scripts/check-base-evidence.py`; `scripts/tests/test_base_evidence.py`. Owning test: `harness.base-evidence` / `scripts/tests/test_base_evidence.py`; include an accepted context-bound digest control and a fully resealed wrong-digest refusal. Re-review these two files before final gates.

## Exact reviewed snapshot scope

The 51 paths in `build/gate/06-08-review-scope.json` at the snapshot above:

```text
core/CMakeLists.txt
core/t64_bus.c
core/t64_cache.c
core/t64_cpu.c
core/t64_digest.c
core/t64_exception.c
core/t64_internal.h
core/t64_isviewer.c
core/t64_machine.c
core/t64_mi.c
core/t64_rdram.c
core/t64_sp.c
core/t64_warm_devices.c
core/tests/test_boot_determinism.c
core/tests/test_boot_entry.c
core/tests/test_bus.c
core/tests/test_cpu_time.c
core/tests/test_profile.c
core/tests/test_rdram.c
core/tests/test_warm_devices.c
docs/claims/phase06.md
docs/evidence/phase06-cold-boot.md
docs/evidence/phase06-final.json
docs/evidence/phase06-native.md
docs/spec/base-diagnostic.md
docs/spec/cpu-profile.md
docs/spec/cpu-timing.md
docs/spec/rdram-boot.md
fixtures/manifest.json
runner/CMakeLists.txt
runner/t64_base_diag_protocol.c
runner/t64_base_diag_protocol.h
runner/t64_base_diagnostic.c
runner/t64_cold_boot.c
runner/tests/test_base_diag_protocol.c
runner/tests/test_cold_boot.c
scripts/apply-base-diagnostic-overlay.py
scripts/base-diagnostic-lanes.py
scripts/build-qualified-base-diagnostic.py
scripts/check-base-evidence.py
scripts/check-test-inventory.sh
scripts/derive-base-diagnostic-source.py
scripts/run-base-diagnostic.py
scripts/run-cpu-diagnostic.py
scripts/tests/test_base_classification.py
scripts/tests/test_base_evidence.py
scripts/tests/test_base_image.py
scripts/tests/test_base_lanes.py
scripts/tests/test_base_qualification.py
scripts/tests/test_base_run.py
scripts/verify-base-diagnostic-qualification.py
```

The worktree still has pre-existing owner edits in `AGENTS.md` and `.planning/config.json`; this review did not modify them or any source file. No commit was created.

## Correction slice 1 implementation (scoped re-review complete)

The two-file owning slice now loads the context-bound combined image through
the existing reviewed loader, checks source/selector coverage, recomputes the
selected image from the bound base bytes plus four-byte selector, and compares
its digest before submitting original records to C. Blocked/unexecuted entries
retain empty expected-image identity.

Normal and ASan/UBSan all-target builds and harness.base-evidence passed. The
local-only control first accepts the retained batch, copies context/inputs and
consumer, changes a selected digest in both record and classification input,
and regenerates a coherent report through original C. The new independent guard
then refuses the copied evidence. Original archives are unchanged. Qualification
and lifecycle controls, saved-record/artifact checks, documentation and rights/PII
scans pass. Logs: build/{gate,asan}/06-08-fix-* and build/gate/06-08-image-*.
Scoped code/security re-review and the final stable gate were pending when this section was first written; see the completed scoped re-review below.

## Scoped re-review: f537baf21c3a6de62851694f58d507f915f4f5c0

**Comparison:** `cc1e3f209fee1cc276bb01ecb26899ca6a3a853f..f537baf21c3a6de62851694f58d507f915f4f5c0`
**Scope:** `scripts/check-base-evidence.py` and `scripts/tests/test_base_evidence.py` only. The review covered the new historical-image loader and selected-image check, their `_validate_batch` call path, and the new positive and resealed-negative controls. No unrelated Phase06 files were re-reviewed in this slice.

### T06-18 — CLOSED

The checker now loads the bound composed image and validates its dispatch seam against the saved source authority at `scripts/check-base-evidence.py:1144-1154`. It derives each dispatchable image digest by splicing the row’s bounded registration/value selector into the bound base bytes, checks the composition’s selected-image digest, and requires the manifest’s expected digest to equal that independently derived value at `scripts/check-base-evidence.py:1157-1175`. `_validate_batch` obtains the saved source authority and applies this check to each parsed record before reclassification at `scripts/check-base-evidence.py:1236-1248`. The existing C classifier also constrains blocked/unexecuted records to have empty selected-image identity and zero guest progress, and requires other records to match the supplied expected digest (`runner/t64_base_diag_protocol.c:724-750`). Together these checks remove the saved-manifest digest as an authority source.

The controls include a synthetic selector-byte positive/negative test at `scripts/tests/test_base_evidence.py:406-418` and an actual resealed mutation control at `scripts/tests/test_base_evidence.py:356-400`. The latter copies retained inputs, changes both the saved record digest and manifest digest, regenerates the report through the retained C consumer, then asserts the independent guard refuses. This directly covers the original defect while preventing a stale-report hash from being the refusal reason.

No new actionable defect was proven in this two-file correction. The source-level scoped verdict is **PASS; T06-18 closed**. The parent reports that the owning normal/ASan all-target builds, `harness.base-evidence`, and the actual resealed control passed, with the actual control completing in about 7.6 seconds; those executions were not rerun as part of this source-only re-review. No broad gate, guest replay, image build, network install, long suite, service, or private upload was run by this reviewer. The final stable-gate result remains parent-owned.

## Correction slice 2 queue: stale no-HLE production seal

The first final normal gate reached every test and failed only repo.rsp-no-hle.
It retained the pre-Phase06 core production file/object inventory and hashes
for t64_sp_complete/t64_sp_rsp_dma_commit, so it rejected the reviewed RDRAM
source addition and pure RDRAM read-visibility filtering. This is a stale seal,
not authorization to bypass scanning or weaken mutation detection.

Owning earlier scope: Plan03 RDRAM and Plan05 feedback/SP-DMA wiring. Exact
correction files: scripts/check-rsp-no-hle.py and scripts/tests/test_rsp_no_hle.py
(two-file policy/test slice). Explicitly add the reviewed core file/object and
update only the two reviewed normalized body seals; preserve all injected-HLE,
unknown-file/object and changed-body negative controls. Owning test is
repo.rsp-no-hle, run cheaply before long suites. Re-review this policy diff before
the stable final gate; retain the failed gate/LastTest and execute core-lint and
the previously unreached sanitizer/fuzz tail after the cause is fixed.

Slice 2 implementation adds only t64_rdram.c to the canonical production
source/object inventory and refreshes the two normalized SP function seals.
Two new injected-effect mutations exercise exactly those functions; all earlier
unknown-source/object and fetched-route controls remain. Direct owner controls
pass against both normal and instrumented archives before the longer gate.
Logs: build/{gate,asan}/06-08-no-hle-control.log. Scoped re-review complete; see below.

## Scoped re-review: acde6e3e9bc328789f27206c742100acf35d3481

**Comparison:** `f537baf21c3a6de62851694f58d507f915f4f5c0..acde6e3e9bc328789f27206c742100acf35d3481`
**Scope:** `scripts/check-rsp-no-hle.py` and `scripts/tests/test_rsp_no_hle.py` only. Reviewed the canonical production source/object roster addition, two refreshed normalized function-body digests, and two added host-entry mutation controls.

### Verdict — PASS; no new findings

The canonical roster change is additive: `core/t64_rdram.c` is now included at `scripts/check-rsp-no-hle.py:36`, and the current production build graph has its `t64_rdram.c.o` compile edge and archive membership. The checker uses that roster for source inventory and expected object construction (`scripts/check-rsp-no-hle.py:311-327, 541-551`), so the update closes the stale-roster rejection without weakening unreviewed-source or object checks.

I independently recomputed both new whitespace-normalized function-body hashes from the current comment/literal-stripped C source using the checker’s extraction and normalization algorithm. The `t64_sp_complete` and `t64_sp_rsp_dma_commit` hashes match the refreshed entries at `scripts/check-rsp-no-hle.py:146,188`. The original inventory and digest mismatch behavior remains active.

The added mutations target exactly those two reviewed functions through the existing host-entry mutation loop at `scripts/tests/test_rsp_no_hle.py:29-34,215-230`; each inserts an anonymous DMEM-dependent effect and must be rejected. The mutation count remains checked against the declared seed list at `scripts/tests/test_rsp_no_hle.py:389-394`. No weakening or specific regression was found. The parent reports the bounded normal and ASan archive controls passed (`build/{gate,asan}/06-08-no-hle-control.log`); I did not rerun tests or builds in this source-only re-review.

## Scoped re-review: bf7046f27cc6671ed332dcad875b568ef5130d09

**Comparison:** `acde6e3e9bc328789f27206c742100acf35d3481..bf7046f27cc6671ed332dcad875b568ef5130d09`
**Scope:** `scripts/check-base-evidence.py` and `scripts/tests/test_base_evidence.py` only. The diff also updates the Plan07 correction queue in this report. Reviewed exact original-attempt resolution, receipt order and hash checks, and the repeated-output control.

### Verdict — PASS; no new findings

The parser now consumes the two `execution_record.attempts` references in the known test order and checks that the list has exactly those two ordered references (`scripts/check-base-evidence.py:410-435`). Each reference is closed to an expected test name and bounded attempt-path format; command and outcome digests must match the exact files, the recorded outcome must be successful, child output must appear in the corresponding original CTest section, and the command must match the original normal-gate shape (`scripts/check-base-evidence.py:306-334, 279-303`). The prior directory-wide search and equal-output fallback are removed, so later normal or instrumented runs cannot substitute for the receipt-pinned original. Missing or changed pinned files refuse.

The private-free control adds an identical later copy, changes its command to the ASan runner, verifies the pinned original remains accepted, then removes the pinned outcome and requires refusal (`scripts/tests/test_base_evidence.py:470-485`). This covers both the historical ambiguity and missing-original boundary without mutating retained production records. No specific regression or new actionable finding was proven.

The parent reports the authored controls, normal and ASan checks, qualification and artifact/current/pre-use checks, and rights/privacy scan passed; I performed source inspection only and did not run tests or builds. The final hosted gate remains parent-owned.

## Correction slice 3 queue: original attempt lookup after later tests

The complete normal gate and instrumented suite passed at 7554ef0, including
admission fuzz/negative control. Final original-artifact readback then found an
authored parser defect: searching every historical attempt by child output
becomes ambiguous after legitimate later runs produce identical startup bytes;
an instrumented command can also be considered before the original. The public
receipt already pins exact attempt paths and command/outcome hashes.

Owning Plan07 Task2 correction, two files: scripts/check-base-evidence.py and
scripts/tests/test_base_evidence.py. Resolve only the closed receipt's exact
attempt reference, verify command/outcome digests plus original CTest embedding,
and reject missing/changed/foreign references. Add an accepted repeated-output
control. No record is deleted or relabeled. Re-review only this parser slice;
reuse unaffected full native/sanitizer semantics and run the changed owner in
both presets. Final hosted gate covers the published source.

Parser-slice re-review is complete: **PASS; no new findings**. The exact-reference
checks and repeated-output/missing-original control are documented above. The
parent reports the changed-owner checks passed; the hosted gate remains pending
with the parent.
