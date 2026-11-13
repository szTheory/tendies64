---
phase: 04-complete-vr4300-cpu
verified: 2026-11-13T18:13:14.394611+00:00
status: passed
score: 4/4 success criteria verified
behavior_unverified: 0
human_verification: []
decision_coverage: {honored: 13, total: 13, not_honored: []}
covered_files: [".planning/phases/04-complete-vr4300-cpu/04-01-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-01-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-02-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-02-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-03-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-03-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-04-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-04-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-05-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-05-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-06-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-06-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-07-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-07-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-08-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-08-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-09-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-09-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-10-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-10-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-11-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-11-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-12-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-12-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-13-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-13-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-14-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-14-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-15-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-15-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-16-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-16-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-17-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-17-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-18-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-18-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-19-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-19-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-20-PLAN.md", ".planning/phases/04-complete-vr4300-cpu/04-20-SUMMARY.md", ".planning/phases/04-complete-vr4300-cpu/04-PROBE-LEDGER.md", ".planning/phases/04-complete-vr4300-cpu/04-REGRESSION-EVIDENCE.md", ".planning/phases/04-complete-vr4300-cpu/04-REVIEW-DISPOSITION.md", ".planning/phases/04-complete-vr4300-cpu/04-REVIEW.md", ".planning/phases/04-complete-vr4300-cpu/04-SECURITY.md", ".planning/phases/04-complete-vr4300-cpu/04-VALIDATION.md", "core/CMakeLists.txt", "core/softfloat/f32_add.c", "core/softfloat/f32_div.c", "core/softfloat/f32_eq.c", "core/softfloat/f32_le.c", "core/softfloat/f32_lt.c", "core/softfloat/f32_mul.c", "core/softfloat/f32_sqrt.c", "core/softfloat/f32_sub.c", "core/softfloat/f32_to_f64.c", "core/softfloat/f32_to_i32.c", "core/softfloat/f32_to_i64.c", "core/softfloat/f64_add.c", "core/softfloat/f64_div.c", "core/softfloat/f64_eq.c", "core/softfloat/f64_le.c", "core/softfloat/f64_lt.c", "core/softfloat/f64_mul.c", "core/softfloat/f64_sqrt.c", "core/softfloat/f64_sub.c", "core/softfloat/f64_to_f32.c", "core/softfloat/f64_to_i32.c", "core/softfloat/f64_to_i64.c", "core/softfloat/i32_to_f32.c", "core/softfloat/i32_to_f64.c", "core/softfloat/i64_to_f32.c", "core/softfloat/i64_to_f64.c", "core/softfloat/internals.h", "core/softfloat/platform.h", "core/softfloat/primitiveTypes.h", "core/softfloat/primitives.h", "core/softfloat/s_addMagsF32.c", "core/softfloat/s_addMagsF64.c", "core/softfloat/s_approxRecip32_1.c", "core/softfloat/s_approxRecipSqrt32_1.c", "core/softfloat/s_approxRecipSqrt_1Ks.c", "core/softfloat/s_approxRecip_1Ks.c", "core/softfloat/s_countLeadingZeros32.c", "core/softfloat/s_countLeadingZeros64.c", "core/softfloat/s_countLeadingZeros8.c", "core/softfloat/s_mul64To128.c", "core/softfloat/s_normRoundPackToF32.c", "core/softfloat/s_normRoundPackToF64.c", "core/softfloat/s_normSubnormalF32Sig.c", "core/softfloat/s_normSubnormalF64Sig.c", "core/softfloat/s_roundPackToF32.c", "core/softfloat/s_roundPackToF64.c", "core/softfloat/s_roundToI32.c", "core/softfloat/s_roundToI64.c", "core/softfloat/s_shiftRightJam32.c", "core/softfloat/s_shiftRightJam64.c", "core/softfloat/s_shiftRightJam64Extra.c", "core/softfloat/s_shortShiftRightJam64.c", "core/softfloat/s_subMagsF32.c", "core/softfloat/s_subMagsF64.c", "core/softfloat/softfloat.h", "core/softfloat/softfloat_types.h", "core/softfloat/sources.cmake", "core/softfloat/specialize.h", "core/t64_bus.c", "core/t64_cache.c", "core/t64_cop1.c", "core/t64_cp0.c", "core/t64_cpu.c", "core/t64_digest.c", "core/t64_exception.c", "core/t64_internal.h", "core/t64_isviewer.c", "core/t64_machine.c", "core/t64_memory.c", "core/t64_mmu.c", "core/t64_pi.c", "core/t64_scheduler.c", "core/t64_softfloat.c", "core/t64_sp.c", "core/tests/test_boot_entry.c", "core/tests/test_cop1.c", "core/tests/test_cpu_cache.c", "core/tests/test_cpu_cp0.c", "core/tests/test_cpu_exceptions.c", "core/tests/test_cpu_integer.c", "core/tests/test_cpu_link.c", "core/tests/test_cpu_memory.c", "core/tests/test_cpu_mmu.c", "core/tests/test_cpu_path.c", "core/tests/test_cpu_time.c", "core/tests/test_digest_trace.c", "core/tests/test_profile.c", "core/tests/test_softfloat.c", "docs/evidence/phase04-native-cpu.md", "docs/spec/cpu-cache.md", "docs/spec/cpu-compatibility.md", "docs/spec/cpu-cop1.md", "docs/spec/cpu-cp0.md", "docs/spec/cpu-diagnostic.md", "docs/spec/cpu-memory.md", "fixtures/manifest.json", "runner/CMakeLists.txt", "runner/t64_cpu_diag_protocol.c", "runner/t64_cpu_diag_protocol.h", "runner/t64_cpu_diagnostic.c", "runner/tests/test_cpu_diag_protocol.c", "runner/tests/test_cpu_diagnostic.c", "scripts/apply-cpu-diagnostic-overlay.py", "scripts/build-cpu-diagnostic.sh", "scripts/build-qualified-cpu-diagnostic.py", "scripts/check-test-inventory.sh", "scripts/prepare-cpu-diagnostic-vendor.py", "scripts/private-tests/accepted-qualification-baseline.py", "scripts/run-cpu-diagnostic.py", "scripts/tests/test_cpu_diagnostic_build_controls.py", "scripts/tests/test_cpu_diagnostic_overlay_controls.py", "scripts/tests/test_cpu_diagnostic_qualification_controls.py", "scripts/tests/test_cpu_diagnostic_run_controls.py", "scripts/verify-cpu-diagnostic-build.py", "scripts/verify-cpu-diagnostic-qualification.py", "scripts/verify-cpu-diagnostic-source-closure.py", "scripts/verify-cpu-diagnostic-unit-graph.py", "tests/cpu-diagnostic/overlay.patch", "tests/cpu-diagnostic/replacements/allocator.rs", "tests/cpu-diagnostic/replacements/exception_imports.rs", "tests/cpu-diagnostic/replacements/exception_park.rs", "tests/cpu-diagnostic/replacements/graphics_mod.rs", "tests/cpu-diagnostic/replacements/linker.ld", "tests/cpu-diagnostic/replacements/main.rs", "tests/cpu-diagnostic/replacements/panic.rs", "tests/cpu-diagnostic/replacements/print.rs", "tests/cpu-diagnostic/replacements/randomized_imports.rs", "tests/cpu-diagnostic/replacements/randomized_signature.rs", "tests/cpu-diagnostic/replacements/rsp_mod.rs", "tests/cpu-diagnostic/replacements/tests_prefix.rs", "tests/cpu-diagnostic/replacements/tests_runner.rs", "tests/cpu-diagnostic/replacements/text_out.rs", "tools/fixture/t64_rom_pack.c", "tools/lint/core-lint.sh", "tools/lint/lint-objects.sh"]
covered_digest: "v4:sha256:c88d862ed65de6544aa296f89dcf2ab1c1a0fd87219082c38d50e580c9fcb856"
---

# Phase4 — VR4300 CPU goal verification

The scoped software CPU, native evidence and qualified local reporting goal
are achieved. This is completion with disclosed non-authoritative judgment
flags, not hardware certification or an all-passing compatibility claim.

## Observable truths

| Success criterion | Status | Source wiring and behavioral evidence |
|---|---|---|
| Integer, branches/delay exceptions, CP0, TLB, memory and LL/SC have independently expected native edge/negative tests | VERIFIED | cpu_step routes full-width branch/exception, prepared CP0 and memory effects before retirement. CMake registers integer/control/exceptions/cp0/time/mmu/memory/link owners. Final-source hosted gates pass; fresh cpu-control spot check passes, including annulled side effects and exact retirement. |
| Integer soft-float COP1 supports specified formats, rounding, FCR31 and traps without host FP | VERIFIED | t64_cop1_execute stages transfer/arithmetic results; explicit rounding translation and FCR31 handling feed commit only after fault checks. The copied integer closure and sources.cmake are linked into the canonical core. softfloat32/64/isolation and cop1-state/arithmetic/exceptions pass; source/object lint passes. Final scalar-result regression preserves odd destination, upper word and neighbor. |
| Cache behavior has a written guest-visible scope and functional tests | VERIFIED | cpu_step dispatches CACHE after translation/privilege preflight into t64_cache.c. I/D virtual indices, physical tags, dirty/writeback and reservation effects have explicit operations and refusal paths. cpu-icache/dcache/cache-ops pass; cpu-cache.md states timing/arbitration and selected-model limits. |
| Qualified pinned local diagnostic produces exact complete per-group CPU reporting | VERIFIED | build/qualification seals feed production runner selection; scripts preserve canonical batches and invoke the same shared C aggregator. aggregate_is_complete rejects empty groups, invalid/duplicate sets, unseen members and non-pass/fail outcomes. The actual bounded attempt and authoritative completion command were observed successful; private receipt/report/artifact bindings were checked. No external result, denominator or artifact is included here. |

## Artifacts and links

| Artifacts | Substantive implementation and wiring |
|---|---|
| t64_cpu/exception/cp0/mmu/memory/cache.c and internal state | Real decoder, staged effects, fault priority, retirement and instance state; CPU event enters these paths. Undefined strict operations remain explicit refusals. |
| t64_cop1.c and core/softfloat closure | Integer result/flag machinery and CPU-owned register/trap commit; no host floating-point canonical path. |
| t64_machine.c and t64_digest.c | Canonical immutable strict/v1/v2 descriptors and per-version future-state domains; v11 alone includes the COP2 latch. Fixed reference/golden and exclusion controls pass. |
| runner protocol, production CLI and local scripts | One shared parser/aggregator linked into CLI and tests; --compat remains v1, --compat-v2 selects v2. Closed seal/runtime/inventory identity checks bind each invocation before/after. |
| tests, specifications, validation and native evidence | Registered value/behavior assertions, primary citations or independent bit/rational derivations, failure provenance, exact inventories and final-source hosted verification. No placeholder or missing key link was found in the checked scope. |

## Requirement coverage

| Requirement | Status | Evidence owners |
|---|---|---|
| CPU-01 | SATISFIED in stated profile scope | Integer/path matrices, extension/overflow/HI-LO and zero register |
| CPU-02 | SATISFIED in stated profile scope | Control/exceptions/COP1 branches, full PC/EPC/BD and annul effects |
| CPU-03 | SATISFIED in stated profile scope | CP0 masks/transfers/time, interrupt/write/retirement ordering |
| CPU-04 | SATISFIED in stated profile scope | MMU/memory/TLB and privilege/fault matrices |
| CPU-05 | SATISFIED in stated profile scope | All merge offsets, linked width/range/one-shot and refusal atomicity |
| CPU-06 | SATISFIED in stated profile scope | Written cache scope and I/D/tag/writeback/refusal controls |
| CPU-07 | SATISFIED in stated profile scope | Integer closure, four rounding modes, FCR31/trap and isolation tests/lint |
| CPU-09 | SATISFIED | Independent native expectations and edge/negative owners; selected undefined software-policy checks are labeled separately |
| VER-01 | SATISFIED for the approved local CPU slice | Actual pre-use qualified pinned build/startup and authoritative complete reporting; public-use rights remain stopped |
| VER-03 | SATISFIED for this phase | Exact native inventory/removal controls and separately sealed private membership/union verification |

CPU timing tiers, other diagnostic groups and later subsystem requirements
remain assigned to later roadmap phases. They are not silently closed here.

## Test quality and evidence

| Linked test group | Active / skipped assessment | Provenance and assertion strength |
|---|---|---|
| Integer/control/CP0/MMU/memory/link/cache | Registered and run by both final hosted gates; no requirement disabled by a skip marker | Constant/matrix and multi-step register/memory/exception/refused-effect assertions; manual citations and explicit selected-policy distinctions |
| Soft-float/COP1 | Registered and green; no sole-owner disabled case found | Independent exact bit/rational boundary vectors, all rounding modes, flag/trap suppression and instance isolation |
| Profile/digest/boot | Registered and green | Independent serializers/fixed vectors and old-domain exclusion; historical identity goldens prove preservation, not hardware accuracy |
| Protocol/inventory/adapter | Registered and green | Real C CLI positive/negative routing, malformed/duplicate/omitted and profile-substitution controls; fake qualification input is kept distinct from actual local evidence |

The anti-pattern scan found temporary-file XXXXXX templates, not unfinished
XXX markers. No disabled requirement owner, circular correctness golden or
status-only replacement for the claimed behavioral tests was identified in
the examined sources. Legacy output hashes are preservation evidence only.

Both final hosted jobs in [run43476881467](https://github.com/szTheory/tendies64/actions/runs/43476881467)
pass all111 native checks, inventory/selftests, lint and publication tail on
f702f0c. The fresh verification spot check is
`ctest --preset gate --no-tests=error -R '^unit.core.cpu-control$' --output-on-failure`:
one registered test passes. The saved final normal/instrumented owning checks
and correctly scoped earlier broad sanitizer/fuzz runs remain in native evidence.
No duplicate full gate, fuzz run or actual guest attempt was launched for prose.

## Decision coverage

The owning decision-coverage gate reports all13 trackable CONTEXT decisions
honored, none missing. Source/plan/summaries preserve separate canonical
machine state, old profiles, local rights boundary and bounded execution.

## Security, review and non-authoritative flags

The configured independent source reviews and follow-ups have zero open
findings across their recorded union. All42 registered threats have closed
mitigations. Final summaries introduce no unregistered threat or accepted risk.
Those audits do not establish silicon accuracy or grant diagnostic publication.

Four transparency prohibitions remain **unverified by authoritative judgment**:
selected machine policies must not become silicon claims; generic IEEE tests
must not become VR4300/game claims; expected-value provenance must not be
misrepresented; native/reduced/unreached reporting must not be called complete
or confer public rights. The five unclassified probe flags remain retained in
the probe ledger. The report does not pretend automated checks settle these
judgments. AUTONOMY's explicit autonomous soft gate permits completion with
these flags after the actual automated requirements pass. No human validation
or owner risk acceptance is recorded.

## Human verification and limits

No human-only action is needed for the stated software/native/local-reporting
criteria; the local CLI and its output were exercised programmatically.
Physical hardware qualification and public diagnostic use remain outside the
authorized evidence scope. Complete reporting includes executed diagnostic
failures; it never implies every comparison passes or every game works.

## Verification provenance

Initial phase verification; no prior phase4 report existed. The configured
typed verifier was dispatched, remained silent despite bounded checkpoint
requests, then was interrupted. The owning status query confirmed no report
was saved. This report is the orchestrator's direct fallback verification,
not a successful independent verifier result. Existing independent code and
security reviews are retained and are not relabeled as this report.
The owning CLI generated the covered-input fingerprint verbatim.

No missing phase success criterion or new reproducible blocker was found.
The next authorized action is roadmap/state closure, then Phase5 discussion.
