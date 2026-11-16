# Phase 06 Source Review

**Reviewed:** 2026-11-16T01:07:34Z  
**Depth:** deep, adversarial boundary review  
**Snapshot:** `a80b82281b7aeffb32946b50f6fe11839516de8e..cc1e3f209fee1cc276bb01ecb26899ca6a3a853f`  
**Scope source:** `build/gate/06-08-review-scope.json` (51 paths; exact list below)  
**Verdict:** BLOCKED by one proven evidence-integrity defect (T06-18, also reported by the security auditor).

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

## Correction slice 1 implementation (re-review pending)

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
Scoped code/security re-review and the final stable gate remain pending.
