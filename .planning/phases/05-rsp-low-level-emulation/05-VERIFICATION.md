---
phase: 05-rsp-low-level-emulation
verified: 2026-11-15T02:48:44Z
status: passed
score: 36/36 must-haves verified
covered_files:
  - .github/workflows/rsp-stress.yml
  - .planning/phases/05-rsp-low-level-emulation/05-01-PLAN.md
  - .planning/phases/05-rsp-low-level-emulation/05-01-SUMMARY.md
  - .planning/phases/05-rsp-low-level-emulation/05-02-PLAN.md
  - .planning/phases/05-rsp-low-level-emulation/05-02-SUMMARY.md
  - .planning/phases/05-rsp-low-level-emulation/05-02-review-evidence.md
  - .planning/phases/05-rsp-low-level-emulation/05-03-PLAN.md
  - .planning/phases/05-rsp-low-level-emulation/05-03-SUMMARY.md
  - .planning/phases/05-rsp-low-level-emulation/05-03-review-evidence.md
  - .planning/phases/05-rsp-low-level-emulation/05-04-PLAN.md
  - .planning/phases/05-rsp-low-level-emulation/05-04-SUMMARY.md
  - .planning/phases/05-rsp-low-level-emulation/05-04-review-evidence.md
  - .planning/phases/05-rsp-low-level-emulation/05-05-PLAN.md
  - .planning/phases/05-rsp-low-level-emulation/05-05-SUMMARY.md
  - .planning/phases/05-rsp-low-level-emulation/05-05-review-evidence.md
  - .planning/phases/05-rsp-low-level-emulation/05-06-PLAN.md
  - .planning/phases/05-rsp-low-level-emulation/05-06-SUMMARY.md
  - .planning/phases/05-rsp-low-level-emulation/05-06-review-evidence.md
  - .planning/phases/05-rsp-low-level-emulation/05-07-PLAN.md
  - .planning/phases/05-rsp-low-level-emulation/05-07-SUMMARY.md
  - .planning/phases/05-rsp-low-level-emulation/05-07-review-evidence.md
  - .planning/phases/05-rsp-low-level-emulation/05-08-PLAN.md
  - .planning/phases/05-rsp-low-level-emulation/05-08-SUMMARY.md
  - .planning/phases/05-rsp-low-level-emulation/05-08-review-evidence.md
  - .planning/phases/05-rsp-low-level-emulation/05-09-PLAN.md
  - .planning/phases/05-rsp-low-level-emulation/05-09-SUMMARY.md
  - .planning/phases/05-rsp-low-level-emulation/05-09-review-evidence.md
  - .planning/phases/05-rsp-low-level-emulation/05-10-PLAN.md
  - .planning/phases/05-rsp-low-level-emulation/05-10-REVIEW-CLOSURE.md
  - .planning/phases/05-rsp-low-level-emulation/05-10-REVIEW-FOLLOWUP.md
  - .planning/phases/05-rsp-low-level-emulation/05-10-SUMMARY.md
  - .planning/phases/05-rsp-low-level-emulation/05-10-V6-REVIEW.md
  - .planning/phases/05-rsp-low-level-emulation/05-REVIEW.md
  - .planning/phases/05-rsp-low-level-emulation/05-SECURITY.md
  - .planning/phases/05-rsp-low-level-emulation/05-VALIDATION.md
  - LICENSES/ISC.txt
  - core/CMakeLists.txt
  - core/t64_bus.c
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_machine.c
  - core/t64_rsp.c
  - core/t64_rsp_divider.c
  - core/t64_rsp_divider.h
  - core/t64_sp.c
  - core/tests/test_rsp_compare.c
  - core/tests/test_rsp_control.c
  - core/tests/test_rsp_divider.c
  - core/tests/test_rsp_fuzz.c
  - core/tests/test_rsp_multiply.c
  - core/tests/test_rsp_scalar.c
  - core/tests/test_rsp_transfers.c
  - core/tests/test_rsp_vector.c
  - core/tests/test_sp_dma.c
  - docs/ci-reviews/LOG.md
  - docs/evidence/phase05-authored-rsp.md
  - docs/evidence/phase05-final-rsp-evidence.json
  - docs/evidence/phase05-native-rsp.md
  - docs/evidence/phase05-review.md
  - docs/evidence/phase05-security.md
  - docs/evidence/rsp-divider-provenance.md
  - docs/spec/rsp-diagnostic.md
  - docs/spec/rsp-instructions.md
  - docs/spec/rsp-profile.md
  - docs/spec/rsp-smoke.md
  - fixtures/manifest.json
  - fixtures/rsp/rsp-smoke.S
  - fixtures/rsp/rsp-smoke.ld
  - provenance/copies.json
  - runner/CMakeLists.txt
  - runner/t64_cpu_diag_protocol.c
  - runner/t64_diag_common.c
  - runner/t64_diag_common.h
  - runner/t64_rsp_diag_protocol.c
  - runner/t64_rsp_diag_protocol.h
  - runner/t64_rsp_diagnostic.c
  - runner/t64_rsp_smoke.c
  - runner/tests/test_cpu_diag_protocol.c
  - runner/tests/test_rsp_diag_protocol.c
  - scripts/audit-rsp-divider.py
  - scripts/build-qualified-rsp-diagnostic.py
  - scripts/build-rsp-smoke.sh
  - scripts/check-rsp-evidence.py
  - scripts/check-rsp-no-hle.py
  - scripts/check-test-inventory.sh
  - scripts/prepare-rsp-diagnostic.py
  - scripts/rsp-stress.sh
  - scripts/run-rsp-diagnostic.py
  - scripts/test-rsp-smoke.sh
  - scripts/tests/test_rsp_build.py
  - scripts/tests/test_rsp_divider_audit.py
  - scripts/tests/test_rsp_evidence.py
  - scripts/tests/test_rsp_no_hle.py
  - scripts/tests/test_rsp_qualification.py
  - scripts/tests/test_rsp_run.py
  - scripts/verify-rsp-diagnostic-build.py
  - scripts/verify-rsp-diagnostic-qualification.py
  - tests/test_rsp_smoke.py
  - tools/lint/core-lint.sh
covered_digest: "v4:sha256:e17bf25507ad21ad6790e4a7ea7f85110c05254b19c32e4ba72df949fd2d6c03"
behavior_unverified: 0
overrides_applied: 0
decision_coverage:
  honored: 8
  total: 8
  not_honored: []
---

# Phase 05: RSP Low-Level Emulation Verification Report

**Phase Goal:** The RSP runs real microcode on its scalar and vector units from IMEM/DMEM, with DMA and status semantics, checked against a permanent scalar VU reference and the nemu64-test RSP groups.
**Verified:** 2026-11-15T02:48:44Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

The four roadmap success criteria and all 32 plan truths are verified against the implementation and saved actual evidence. No prior verification report existed. The machine is wired through a distinct functional RSP profile; this verifies the documented software contract and its tests, not silicon timing or hardware accuracy.

### Observable Truths

| # | Truth | Status | Evidence |
|---|---|---|---|
| R1 | ROM and native tests execute scalar/vector instructions from IMEM/DMEM and check SP status, semaphore, halt, break and PC behavior. | VERIFIED | core/t64_rsp.c fetches IMEM into the decoder; core/t64_sp.c and the authored smoke route exercise control, DMA, DMEM observations and BREAK. Saved native gate and authored normal/sanitizer owner records pass; the smoke consumer checks independent output and terminal state. |
| R2 | Per-opcode fuzzing supports the scalar VU reference; scheduled long authored stress remains outside the required gate. | VERIFIED | core/tests/test_rsp_fuzz.c covers each defined operation with bounded independent state-vector/invariant checks, and the authored stress workflow is separate. There is no second optimized VU backend, so no optimized differential execution or differential-pass claim is made. |
| R3 | DMA alignment, bank-wrap, stride and queue behavior pass exact-byte/event tests. | VERIFIED | core/t64_sp.c stages DMA admission and commits against SP memory; core/tests/test_sp_dma.c and test_rsp_control.c cover byte and event semantics. Saved normal and ASan/UBSan gates include these owners. |
| R4 | Required local RSP diagnostic members pass or receive individual reasoned classifications, and the canonical core has no task-specific HLE path. | VERIFIED | The final receipt is bound to source-derived membership and original C records; scripts/check-rsp-evidence.py validates the complete classification and artifact bindings. The no-HLE source/object route guard and mutation controls are registered in the canonical test gate. Classification completeness is not described as a passing diagnostic suite. |
| P01.1 | An authored IMEM program reaches BREAK through real scalar and vector execution and writes independently expected DMEM bytes. | VERIFIED | Fetched execution is in core/t64_rsp.c; tests exercise the production scheduler/retirement path. The authored ROM evidence records normal and sanitizer integration passing with CPU-checked bytes, status, PC, semaphore and negative variants. |
| P01.2 | All five historical profiles retain their descriptors, state bytes and accepted/refused behavior. | VERIFIED | core/t64_machine.c keeps the RSP functional profile distinct; saved full native gate and preserved profile/state regression owners pass. No previous profile is redirected to the new implementation. |
| P01.3 | Every future-affecting RSP field belongs to the distinct new state domain. | VERIFIED | core/t64_internal.h defines separate RSP state; core/t64_digest.c serializes the RSP execution, readiness, divider and DMA future state under the new state domain. |
| P02.1 | SP control starts and stops real fetched RSP code without changing legacy profiles. | VERIFIED | core/t64_sp.c routes the scoped control operations into core/t64_rsp.c; control tests and the authored CPU-to-SP-to-RSP smoke evidence exercise start, halt, break and resume behavior. |
| P02.2 | Alignment, wrapping, stride and queue semantics are visible in exact bytes and bounded events. | VERIFIED | The SP DMA implementation and test_sp_dma/test_rsp_control assertions check memory bytes, queue state and bounded event effects. Saved final native and instrumented owner selections pass. |
| P02.3 | Unsupported arbitration/control requests refuse before effects. | VERIFIED | DMA preflight validates the complete projected request before committing its beat or promoting the queue; refusal controls compare state and memory. Saved owning normal and sanitizer checks pass. |
| P03.1 | Exact source-derived RSP membership is sealed before any external execution. | VERIFIED | The qualification tools construct the source/value inventory and verify the complete pre-use closure before the runner can execute; the final evidence checker independently revalidates the pinned source anchor and records. Private member identities are omitted here. |
| P03.2 | An actual component build/composition attempt occurs before long native opcode expansion. | VERIFIED | The guarded qualification/build flow checks source/tool/asset closure, invokes component build and composition verification, and retains the original pre-use attempt history. Final evidence validation passes. |
| P03.3 | New RSP assets receive their own pre-use rights/closure decision and stay private. | VERIFIED | docs/spec/rsp-diagnostic.md and the qualification tools separate selected source/assets, require a rights decision, and keep external diagnostics in ignored local evidence. The public receipt and hosted lane contain authored-only evidence. |
| P04.1 | A real RSP startup/refusal attempt precedes full scalar/vector expansion. | VERIFIED | Saved qualification and early-attempt records preserve the startup/refusal boundary; later native and authored tests exercise actual fetched RSP operations. The final checker validates the retained evidence without replaying private inputs. |
| P04.2 | Every RSP member outcome is authenticated and component-bound; refusals never pass. | VERIFIED | runner/t64_rsp_diag_protocol.c and runner/t64_rsp_diagnostic.c validate the closed RSP protocol and terminal evidence; scripts/check-rsp-evidence.py validates original batches and classifications, including nonpass refusal handling. |
| P04.3 | CPU canonical bytes and required inventory remain frozen. | VERIFIED | The RSP uses a separate protocol/schema and source path; saved canonical gate/inventory evidence retains the CPU protocol and required inventory. |
| P05.1 | Guide-defined scalar/control and vector transfer operations execute from IMEM with independent results. | VERIFIED | core/t64_rsp.c implements fetched scalar/COP2 dispatch and transfer operations; core/tests/test_rsp_scalar.c and test_rsp_transfers.c assert independent values and bytes through production code. The instruction ledger maps defined operations to owners. |
| P05.2 | All defined and reserved encoding slots have explicit coverage/refusal dispositions. | VERIFIED | docs/spec/rsp-instructions.md provides the complete slot ledger; unit tests and the fetched decoder implement listed operations and explicit reserved/unknown refusal behavior. |
| P05.3 | DP register requests refuse transactionally because RDP execution is outside this phase. | VERIFIED | DP accesses are outside the implemented RSP scope and refuse before state effects; the fetched request/refusal tests cover the boundary. |
| P06.1 | Portable vector arithmetic and comparison execute from IMEM with independent expected values. | VERIFIED | core/tests/test_rsp_vector.c, test_rsp_compare.c and test_rsp_multiply.c drive the canonical fetch path with independent constants and complete result checks. |
| P06.2 | Every accumulator slice, flag and element selection has explicit assertions. | VERIFIED | Vector/multiply/compare tests assert accumulator slices, flags, lane selection, and full state. The authored smoke independently checks selected arithmetic, carry, lane and accumulator results. |
| P06.3 | Defined operations are implemented; reserved/unknown hardware behavior has named observable policy/refusal. | VERIFIED | The instruction ledger enumerates supported encodings, named software policies and refusals. Decoder behavior and per-opcode test/fuzz owners follow that ledger. |
| P07.1 | Divider families execute through the canonical portable reference using an audited permitted algorithm. | VERIFIED | core/t64_rsp_divider.c is integer-only and is called by the canonical RSP implementation; provenance and license ownership are documented. Divider tests exercise both helper and fetched latch behavior. |
| P07.2 | Correctness evidence includes independent integer/rational and guide-supported expectations. | VERIFIED | core/tests/test_rsp_divider.c uses rational cross-products and integer square bounds alongside guide-based fetched cases; saved native owner evidence passes. |
| P07.3 | Unknown physical ROM/special-edge behavior remains explicit and no hardware accuracy is claimed. | VERIFIED | Profile and instruction specifications identify the divider and timing policies as software behavior and preserve unknown silicon edges as limits; public reports make no hardware-accuracy claim. |
| P08.1 | Every defined opcode has nonempty bounded independent state-vector/invariant fuzz coverage. | VERIFIED | core/tests/test_rsp_fuzz.c iterates the defined-operation table and checks bounded complete-state invariants against independent expectations; it is part of the saved full gate and instrumented owner evidence. |
| P08.2 | No successful optimized-path differential claim exists without an actual separate implementation. | VERIFIED | The canonical implementation has one scalar VU path; there is no optimized second backend and no differential pass is claimed. Per-opcode independent invariant fuzz remains the evidence for this implementation. |
| P08.3 | The canonical build cannot bypass fetched execution with a task-specific HLE replacement; mutation controls prove detection. | VERIFIED | scripts/check-rsp-no-hle.py checks implementation helpers, SP entry routes and built objects; its mutation controls are registered as a canonical test. Actual evidence records those controls passing. |
| P08.4 | Long authored stress has its own scheduled lane; external material remains disabled under D2. | VERIFIED | .github/workflows/rsp-stress.yml invokes the bounded authored-only stress script on its own schedule/dispatch lane. Private diagnostic execution remains local and is not part of hosted jobs. |
| P09.1 | A real authored ROM drives SP DMA/control and executes RSP code from IMEM to observed DMEM output. | VERIFIED | fixtures/rsp/rsp-smoke.S, runner/t64_rsp_smoke.c and tests/test_rsp_smoke.py connect an authored ROM to the canonical machine and CPU-visible checks; saved normal and sanitizer owner tests pass. |
| P09.2 | Authenticated completion and meaningful negative variants prove the production path. | VERIFIED | The smoke consumer authenticates terminal CPU/RSP retirement and expected output. Saved owner evidence includes wrong-result, unsupported opcode, missing BREAK, altered delay and DMA-wrap negative controls. |
| P09.3 | Authored normal/sanitizer evidence is independent of private diagnostics and makes no silicon claim. | VERIFIED | The fixture builder admits authored-only inputs; normal and instrumented records are separately saved. docs/evidence/phase05-authored-rsp.md explicitly separates this path from private and hardware evidence. |
| P10.1 | Every sealed required private RSP member is authenticated or individually classified with a reason. | VERIFIED | The final source-anchored partition is checked through the production C protocol and independent public evidence checker; unexecuted/refused rows remain nonpasses. No private names, counts or outcomes are repeated here. |
| P10.2 | Final native and instrumented evidence covers the full implemented instruction/control/DMA scope and preserved historical behavior. | VERIFIED | Saved final native gate and all-target ASan/UBSan owner records include RSP, shared CPU protocol, DMA and admission controls. The actual final source is host-only; the saved provenance refresh retains unchanged semantic build-policy identities. |
| P10.3 | Configured code/security reviews and final evidence/artifact checks are complete; unresolved high findings block. | VERIFIED | The preserved review dispositions close accepted findings; the final configured security follow-up is secured with no open high findings. The owning evidence checker passes on the current public receipt and saved artifact bindings. |
| P10.4 | Required hosted pair stays authored-only; no private result, physical claim or release is implied. | VERIFIED | The saved Linux/macOS hosted pair ran the full authored gate; the workflow has no private diagnostics or protected inputs. Reports explicitly avoid hardware, broad compatibility, private-suite-pass and release claims. |

**Score:** 36/36 truths verified (0 present, behavior-unverified)

### Decision Coverage

The GSD decision-coverage query returned 8 honored of 8 decisions, with no unhonored decisions. The phase's accepted model and refusal policies are reflected in the code and evidence. In particular, DMA admission is checked before effects; software issue/divider policies remain labeled as policies; unsupported physical behavior remains a limitation; and no optimized VU backend or differential claim was added.

### Required Artifacts

The GSD artifact query reports all 37 declared artifact entries across the ten plans present and substantive. The repeated RSP profile specification is declared by two plans; all other entries are also independently checked.

| Plan | Declared artifact entries | Status |
|---|---|---|
| 05-01 | core/t64_rsp.c; docs/spec/rsp-profile.md | VERIFIED (2/2) |
| 05-02 | core/t64_sp.c; core/tests/test_rsp_control.c; docs/spec/rsp-profile.md | VERIFIED (3/3) |
| 05-03 | scripts/prepare-rsp-diagnostic.py; scripts/verify-rsp-diagnostic-qualification.py; scripts/build-qualified-rsp-diagnostic.py; scripts/verify-rsp-diagnostic-build.py; docs/spec/rsp-diagnostic.md | VERIFIED (5/5) |
| 05-04 | runner/t64_diag_common.c; runner/t64_rsp_diag_protocol.h; runner/t64_rsp_diagnostic.c; scripts/run-rsp-diagnostic.py | VERIFIED (4/4) |
| 05-05 | docs/spec/rsp-instructions.md; core/tests/test_rsp_scalar.c; core/tests/test_rsp_transfers.c | VERIFIED (3/3) |
| 05-06 | core/tests/test_rsp_vector.c; core/tests/test_rsp_multiply.c; core/tests/test_rsp_compare.c | VERIFIED (3/3) |
| 05-07 | docs/evidence/rsp-divider-provenance.md; core/t64_rsp_divider.c; core/tests/test_rsp_divider.c; provenance/copies.json | VERIFIED (4/4) |
| 05-08 | core/tests/test_rsp_fuzz.c; scripts/rsp-stress.sh; .github/workflows/rsp-stress.yml; scripts/check-rsp-no-hle.py | VERIFIED (4/4) |
| 05-09 | fixtures/rsp/rsp-smoke.S; scripts/build-rsp-smoke.sh; runner/t64_rsp_smoke.c; docs/evidence/phase05-authored-rsp.md | VERIFIED (4/4) |
| 05-10 | scripts/run-rsp-diagnostic.py; docs/evidence/phase05-review.md; docs/evidence/phase05-security.md; scripts/check-rsp-evidence.py; docs/evidence/phase05-native-rsp.md | VERIFIED (5/5) |

### Key Link Verification

The generic key-link query found 4 of 28 links by literal text matching. I manually traced the remaining links through source calls, CMake registration, CLI dispatch and report validation because many plan arrows describe document lineage or point opposite the caller-to-consumer direction. No required production path is orphaned.

| From | To | Via | Status | Details |
|---|---|---|---|---|
| core/t64_sp.c | core/t64_rsp.c | SP control calls schedule/issue route | WIRED | SP start/advance control reaches the fetched RSP scheduler; issue reads actual IMEM words. |
| core/t64_machine.c | core/t64_digest.c | profile/state-domain selection and state serialization | WIRED | The profile predicate selects the scoped model, and digest code serializes its future-affecting RSP state. |
| scripts/build-qualified-rsp-diagnostic.py | scripts/verify-rsp-diagnostic-qualification.py | pre-use qualification before build | WIRED | The build wrapper invokes qualification before composing/building; the plan's arrow is reversed from the actual call direction. |
| runner/CMakeLists.txt | runner/t64_rsp_diagnostic.c | linked common/RSP protocol and adapter | WIRED | The adapter uses the shared protocol and component-specific schema through registered runner targets. |
| scripts/run-rsp-diagnostic.py | runner/t64_rsp_diagnostic.c | bounded local dispatch and completion validation | WIRED | The Python wrapper invokes the C adapter and checks authenticated complete output; refusal is not promoted to pass. |
| core/CMakeLists.txt | core/tests/test_rsp_*.c | canonical core build and registered owners | WIRED | RSP sources join the canonical library; each owner is registered with the full gate and inventory. |
| core/t64_rsp_divider.c | core/tests/test_rsp_divider.c | production helper plus fetched operation tests | WIRED | The divider helper is part of the canonical core; its independent provenance audit is a separate registered check. |
| core/tests/test_rsp_fuzz.c | .github/workflows/rsp-stress.yml | required invariant fuzz vs optional authored stress | WIRED | Invariant fuzz is a canonical required test; long authored stress is separately invoked by the scheduled lane. |
| scripts/check-rsp-no-hle.py | core/CMakeLists.txt | source/object route guard and mutation test | WIRED | The guard is run as a separate canonical CTest owner; it is not hidden inside the stress workflow. |
| fixtures/rsp/rsp-smoke.S | runner/t64_rsp_smoke.c | builder, image consumer and CPU assertions | WIRED | CMake test integration builds the authored fixture and passes it to the real machine consumer; the consumer checks CPU-visible results. |
| scripts/check-rsp-evidence.py | docs/evidence/phase05-final-rsp-evidence.json | actual receipt and saved artifact validation | WIRED | The checker validates source anchoring, native evidence bindings and public report constraints. |

### Data-Flow Trace (Level 4)

| Artifact | Data | Source | Produces real data | Status |
|---|---|---|---|---|
| core/t64_rsp.c | fetched instruction word | RSP IMEM read through the machine bus | Yes; decoder consumes actual guest bytes | FLOWING |
| core/t64_sp.c | DMA source/destination bytes and queue events | RDRAM and SP DMEM/IMEM banks | Yes; staged request commits to machine memory | FLOWING |
| runner/t64_rsp_smoke.c | expected output/status and retirements | authored ROM executing in the canonical machine | Yes; CPU observes the resulting DMEM and SP state | FLOWING |
| scripts/check-rsp-evidence.py | partition and artifact validity | source-anchored receipt, original C records and current saved build evidence | Yes; rejects missing, forged, misbound or nonpassing classifications | FLOWING |

### Saved Runtime Evidence

The reports and logs are evidence from actual runs, not fresh test executions by this verifier. They record a complete 127-test native gate and lint pass, an all-target sanitizer inventory with the RSP/shared CPU protocol/admission owners and sanitizer negative control, and the exact hosted authored-only pair. The current-source final evidence check was run read-only by this verifier:

| Behavior | Command | Result | Status |
|---|---|---|---|
| Final public receipt/artifact bindings agree with saved actual evidence | python3 -B scripts/check-rsp-evidence.py --check | PASS: final RSP evidence and artifact bindings (exit 0) | PASS |

The checker does not execute a guest or disclose private membership. The older historical private executable was not retained; its record remains tied to its original evidence and was not replayed or relabeled. The current exact artifacts and source binding are retained separately.

### Probe Execution

No probe scripts are declared in the phase plans or summaries, and no conventional scripts/*/tests/probe-*.sh files are present. Probe execution is not applicable.

### Requirements Coverage

| Requirement | Source plan | Description | Status | Evidence |
|---|---|---|---|---|
| RSP-01 | 05-01, 05-02, 05-05, 05-09 | Scalar/vector instructions execute from IMEM/DMEM with SP status, semaphore, halt, break and PC semantics. | SATISFIED | Fetched unit and authored ROM evidence exercises the production path and checks state/output. |
| RSP-02 | 05-06, 05-07, 05-08 | Permanent portable scalar VU reference, per-opcode evidence and optimized-path comparison when such a path exists. | SATISFIED | One canonical scalar VU implementation is the reference. Independent per-opcode vector/invariant fuzz and arithmetic tests pass. No optimized backend exists, and no differential execution is claimed. |
| RSP-03 | 05-02, 05-05 | RSP DMA alignment, bank-wrap, stride and queue behavior. | SATISFIED | Exact-byte, queue and refusal controls pass in saved native/instrumented evidence. |
| RSP-04 | 05-03, 05-04, 05-10 | Local nemu64-test RSP groups pass or are classified individually with reasons. | SATISFIED | The source-derived complete partition is authenticated and each nonpass remains individually reasoned. This is classification evidence, not an assertion that every member passed. |
| RSP-05 | 05-05, 05-08 | No HLE microcode path exists in the canonical core. | SATISFIED | Source/object/entry-route guard, mutation controls and canonical CTest registration are present and passed in saved evidence. |

There are no orphaned phase requirements. REQUIREMENTS.md remains unchanged by this verification; lifecycle checkboxes are owned by phase closeout.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---|---|---|---|
| — | — | None found in the 63 planned modified-file entries. | — | No unresolved TBD/FIXME/XXX, TODO/HACK/PLACEHOLDER, empty output stubs, console-only implementations or disabled owner tests. The one empty-map return in the HLE checker is an error sentinel for failed source inventory, not a stub or success fallback. |

### Human Verification Required

None. This phase is an emulator-core and harness foundation with no UI or user-facing workflow; its plans contain no deferred human-check blocks. Runtime state transitions have saved normal and instrumented owner-test evidence. Silicon behavior and physical timing remain outside the accepted phase scope rather than pending human validation.

### Gaps Summary

No must-have or requirement gap remains. The software implementation is wired into the canonical machine and tested through fetched instructions, SP DMA and an authored ROM path. The only comparison limitation is explicit: because the code has no separate optimized VU implementation, the phase makes no optimized differential claim. Private diagnostic rows remain local and are classified honestly; no broad hardware, compatibility, release or private-suite-pass claim follows from this verification.

---

_Verified: 2026-11-15T02:48:44Z_  
_Verifier: the agent (gsd-verifier)_
