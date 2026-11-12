## VERIFICATION PASSED — CURRENT VERDICT (ITERATION 3)

**Phase:** Complete VR4300 CPU  
**Plans verified:** 18 across 17 waves; 37 tasks  
**Current status:** No unresolved blocker or warning. Iteration 3 confirms the Plan 04-16 post-change sanitizer correction. This is plan verification only; it is not implementation or test-run evidence.

The historical findings below are retained as audit trail. Iteration 1 and iteration 2 issues have been resolved in the saved plan set.

## ITERATION 1 — ISSUES FOUND (HISTORICAL)

**Phase:** Complete VR4300 CPU  
**Plans checked:** 16  
**Issues:** 1 blocker, 3 warnings

### Coverage and plan structure

All ten roadmap requirement IDs are claimed and have task coverage: CPU-01 through CPU-07, CPU-09, VER-01, and VER-03. The task actions cover the four roadmap criteria: integer/control/CP0/TLB/memory/cache behavior, integer COP1 with independent vectors, cache scope, and the pinned private CPU diagnostic. The locked D-01–D-13 decisions are represented; deferred timing/pipeline, JIT, broader RSP/video, cold reset, and public diagnostic publication are excluded. D2/D4 and other rights, source, asset, and package stops remain explicit. No native-only substitute for SC4 is planned.

All 16 `verify.plan-structure` queries returned valid. There are 33 tasks, each with files, action, automated verify, and measurable done criteria. The dependency graph is acyclic and wave assignments are consistent. Plans 04-01 and 04-14 are disjoint in wave 1; the remaining plans are ordered through wave 15. No same-wave file conflict or undeclared shared-writer coupling was found.

Plan 11’s large copied closure is treated as the explicitly justified atomic-copy exception: individual source destinations and provenance records are listed. Other individual tasks list at most five paths. The final ASan selector names the existing `fuzz.rom-admission` and `sanitizer.negative-control` registrations and includes `controls.real.*`; these identities also appear in the existing ASan inventory. This verifies the planned selector text against current registrations, not that the checks have run. The saved path probe is `not_applicable` (33 commands, zero reported blocker/warning); it is not evidence that the commands executed or that their paths resolved. The saved failing-direction probe reports 33 commands and zero issues. Probe artifacts were consumed as supplied, not re-derived.

All plan estimates are below the 100,000-token smart-zone budget and have low confidence (`sample_count: 0`). The highest is Plan 11 at 76,000 tokens. No builds, tests, CI, or implementation were run for this review.

### Blocker

**1. [key_links_planned] The production diagnostic and final aggregation must use one closed, identity-bound batch contract.**

- Plan: 04-15, with the unresolved consumer carried into 04-16
- Evidence: Plan 04-14 creates the `t64.cpu-inventory` / `t64.cpu-batch` parser, encoder, and cross-batch aggregator. Plan 04-15 Task 1 describes a “strict batch validator” in its behavior, but its action and `must_haves.key_links` do not connect `runner/t64_cpu_diagnostic.c` to `runner/t64_cpu_diag_protocol.c/.h`. Plan 04-16 describes deriving `required-inventory.json`, validating per-value results, and emitting group status, without requiring those records to use the Plan 04-14 schema or aggregator. The protocol can therefore remain isolated behind its own tests while the production run uses a separate, unspecified text/status path; that leaves exact identity and cross-batch completeness unwired at the SC4 boundary.
- Required property: Every production per-value batch and final group status uses one specified identity-bound inventory/batch contract, with the full exact membership and cross-batch completeness checks applied by the production path.
- Example fix (non-binding): Make the adapter emit and validate the Plan 04-14 batch records and make Plan 04-16 aggregate those same records, with an explicit key link and an acceptance check proving the adapter uses the protocol.

### Warnings

**1. [pattern_compliance] Task actions should name the mapped implementation pattern or the research pattern for new roles.**

- Plans: 04-01 through 04-16
- Evidence: Every plan imports `04-PATTERNS.md`, and tasks provide `read_first` paths, but none of the 33 `<action>` blocks explicitly identifies a mapped analog or says which pattern it reuses. For example, Plan 04-02 Task 1 adds `core/t64_exception.c`, whose mapped analogs are `core/t64_cpu.c` and `core/t64_scheduler.c`, but the action does not name either pattern.
- Required property: Each task action identifies the applicable PATTERNS analog, or cites RESEARCH guidance where PATTERNS has no analog, so the intended integration pattern is executable without inference.
- Example fix (non-binding): Add a brief analog/research reference to affected actions and name the contract being reused.

**2. [scope_sanity] Plan 04-11 is above the context warning threshold, with an uncalibrated estimate.**

- Plan: 04-11
- Evidence: Its estimate is 76,000 / 100,000 tokens (76%, `over_budget: false`, confidence `low`, zero actual samples), above the ~70% warning threshold. Its two-task count and the user-authorized atomic SoftFloat closure are accounted for; this warning is about context load, not the closure’s file-count exception.
- Required property: Each plan remains at or below the ~70% context warning threshold, or has a bounded execution scope supported by calibrated evidence.
- Example fix (non-binding): Reassess the estimate and reduce/slice non-atomic work if the 76% projection reflects likely execution load, while retaining the audited per-file closure.

**3. [scope_sanity] Plan 04-16 lists 11 unique modified paths, one above the per-plan warning threshold.**

- Plan: 04-16
- Evidence: Frontmatter lists 11 unique paths across three tasks; each individual task lists at most five paths, and the estimate is 65% of budget. The plan-wide file threshold marks 10 as a warning.
- Required property: Each plan stays within the per-plan file warning threshold or demonstrates that its wider file set remains a bounded execution unit.
- Example fix (non-binding): Confirm the three sequential qualification/run slices keep this plan bounded; if they do not, adjust the plan boundary without splitting individual atomic checks.

### Structured issues

```yaml
issues:
  - plan: "04-15"
    dimension: "key_links_planned"
    severity: "blocker"
    required_property: "Every production per-value batch and final group status uses one specified identity-bound inventory/batch contract, with the full exact membership and cross-batch completeness checks applied by the production path."
    description: "Plan 04-14 creates the t64.cpu-inventory/t64.cpu-batch parser, encoder, and aggregator, but Plan 04-15 does not link the production adapter to them and Plan 04-16 does not require its private inventory/status flow to consume them. The contract can remain test-only while the actual SC4 path uses an unspecified parallel format."
    fix_hint: "Make the adapter emit and validate the Plan 04-14 batch records and make Plan 04-16 aggregate those same records, with an explicit key link and an acceptance check proving the adapter uses the protocol."
  - plans: ["04-01", "04-02", "04-03", "04-04", "04-05", "04-06", "04-07", "04-08", "04-09", "04-10", "04-11", "04-12", "04-13", "04-14", "04-15", "04-16"]
    dimension: "pattern_compliance"
    severity: "warning"
    required_property: "Each task action identifies the applicable PATTERNS analog, or cites RESEARCH guidance where PATTERNS has no analog."
    description: "All 33 task actions rely on imported pattern context/read_first paths but do not explicitly name the mapped analog or research pattern in the action."
    fix_hint: "Add a brief analog/research reference to affected actions and name the contract being reused."
  - plan: "04-11"
    dimension: "scope_sanity"
    severity: "warning"
    required_property: "Each plan remains at or below the ~70% context warning threshold, or has a bounded execution scope supported by calibrated evidence."
    description: "The estimate is 76,000 / 100,000 tokens (76%); confidence is low with zero actual samples. It is under the 100,000 budget and preserves the accepted atomic copied-file closure."
    fix_hint: "Reassess the estimate and reduce/slice non-atomic work if the projection reflects likely execution load, while retaining the audited per-file closure."
  - plan: "04-16"
    dimension: "scope_sanity"
    severity: "warning"
    required_property: "Each plan stays within the per-plan file warning threshold or demonstrates a bounded execution unit."
    description: "Frontmatter lists 11 unique paths across three tasks, above the 10-file warning threshold; each task itself lists at most five paths."
    fix_hint: "Confirm the three sequential qualification/run slices keep this plan bounded; if not, adjust the plan boundary without splitting individual atomic checks."
```

### Recommendation

One blocker and three warnings require plan revision before execution. The blocker is the missing production link between the per-value adapter and the exact batch protocol/aggregator. The ASan fuzz/fault selector identities are correct as currently registered; the saved path probe remains not applicable and supplies no execution evidence.

## Iteration 2 recheck

**Plans checked:** 18 across 17 waves  
**Tasks:** 37  
**Status:** The four iteration 1 findings are resolved in the saved plan text. One new warning remains.

### Dispositions of iteration 1 findings

1. **Resolved — production protocol wiring.** Plan 04-15’s actual action and `must_haves.key_links` make the adapter and authored controls link the same `t64_cpu_diag_protocol` target created by Plan 04-14. The action explicitly routes canonical inventory/batch parsing, validation, encoding, and aggregation through that module and adds production-path malformed, missing-done, wrong-identity, duplicate, omission, and complete-set controls. Plan 04-16’s action and key links require `--run-all` to preserve those canonical records and `--verify-complete` to invoke that same aggregate CLI over the sealed inventory. The tasks forbid a second parser or script-local sum.
2. **Resolved — pattern references.** Inspected action text in all 37 saved tasks; each names a mapped PATTERNS analog/integration contract or the applicable RESEARCH guidance for a new role.
3. **Resolved — Plan 04-11 estimate warning.** The revised estimate is 59,000 / 100,000 tokens (59%; `over_budget: false`). Confidence remains low with zero actual samples, so this is a planning estimate, not measured execution cost. The broad independent arithmetic matrix now has its own Plan 04-17 at 25,000 tokens; Plan 04-11 retains the atomic, per-file-audited closure and immediate production smoke/context controls. Plan 04-12 depends on 04-17, so the matrix precedes its consumers.
4. **Resolved — Plan 04-16 scope warning.** Frontmatter now lists 10 unique paths across three sequential tasks, each listing at most five. The private qualification/build/run unit is bounded at 57,000 estimated tokens and clearly keeps six generated private outputs ignored. The earlier 11-path condition no longer applies.

### Recheck summary

All 18 saved plan structures validate. The dependency graph is acyclic and wave-consistent; the 01/14 wave-1 pair remains disjoint. All ten roadmap requirement IDs are claimed. The revised split keeps the public native closeout in Plan 04-18 after 04-15 and before private Plan 04-16; Plan 04-16 still owns the actual complete private SC4 run and cannot substitute native tests for it. Plan 04-17 precedes the COP1 consumers. The existing 20 inventory additions and 111/113 totals remain unchanged. Plan 04-11 and Plans 04-16/17/18 estimates remain under the 100,000-token budget with low confidence.

The saved path probe is `not_applicable` for 37 commands with zero reported blockers/warnings; it is not execution or path-validity evidence. The saved failing-direction probe reports `ok` for 37 commands with no issues/read error. Neither result was re-derived. No source, build, test, CI, package, ROM, or research action was run during this recheck.

### Remaining warning

**1. [nyquist_compliance] Tracked adapter/test changes in Plan 04-16 occur after the final instrumented evidence in Plan 04-18.**

- Plan: 04-16
- Evidence: Plan 04-18 runs the all-target ASan build and focused instrumented controls at wave 16. Plan 04-16 follows at wave 17 and lists tracked `runner/t64_cpu_diagnostic.c` and `runner/tests/test_cpu_diagnostic.c` as modified files across its tasks. Its task verifies run the gate build/CTest and private CLI checks, but no ASan rebuild or focused instrumented adapter/protocol check follows those planned C changes. Plan 04-18’s ASan evidence therefore does not cover the final planned versions of those paths.
- Required property: Tracked C/CTest adapter changes made after the broad instrumented run receive an affected-target sanitizer rebuild and focused instrumented verification before phase completion.
- Example fix (non-binding): After Plan 04-16’s final tracked adapter/test edits, rebuild the affected ASan target(s) and run the focused adapter/protocol tests under the inherited sanitizer environment. Reuse Plan 04-18’s broad native, CI, inventory, and unaffected sanitizer evidence; do not repeat those broad checks solely for metadata.

### Final structured verdict

## ISSUES FOUND

**Phase:** Complete VR4300 CPU  
**Plans checked:** 18  
**Current issues:** 0 blockers, 1 warning

```yaml
issues:
  - plan: "04-16"
    dimension: "nyquist_compliance"
    severity: "warning"
    required_property: "Tracked C/CTest adapter changes made after the broad instrumented run receive an affected-target sanitizer rebuild and focused instrumented verification before phase completion."
    description: "Plan 04-18 runs the all-target ASan build and focused instrumented controls before Plan 04-16. Plan 04-16 then modifies runner/t64_cpu_diagnostic.c and runner/tests/test_cpu_diagnostic.c, but its task verification contains only gate CTests and private CLI checks; there is no post-change ASan rebuild or focused instrumented adapter/protocol test."
    fix_hint: "After Plan 04-16's final tracked adapter/test edits, rebuild the affected ASan target(s) and run focused adapter/protocol tests under the inherited sanitizer environment, reusing Plan 04-18's broad evidence for unaffected paths."
```

One warning remains before execution: add bounded final-source sanitizer verification for the tracked adapter/test edits in Plan 04-16. The four earlier findings are resolved; no blocker remains.

## Iteration 3 targeted recheck — final disposition

**Resolved — Plan 04-16 post-change sanitizer evidence.** Inspected the actual verify clauses for all three Plan 04-16 tasks. Each configures the `asan` preset, builds all targets, then runs with the inherited `ASAN_OPTIONS` and `UBSAN_OPTIONS` exactly these anchored CTest names: `runner.cpu-diagnostic`, `runner.cpu-diagnostic-protocol`, and `runner.cpu-diagnostic-inventory`. The three names match the existing registrations and Plan 04-VALIDATION.md inventory map (protocol/inventory added by Plan 04-14; adapter by Plan 04-15); Plan 04-16 does not change registration or inventory files. Each clause has an adjacent failure direction covering build failure, missing/empty control selection, and failed instrumented assertions.

The checks use public authored controls against the shared production adapter/protocol. Plan 04-16 still keeps ROMs, captures, and derived statuses under ignored private output paths and excludes them from public evidence. The actions require affected subsystem checks/review and a refresh of affected public native evidence for relevant later source fixes; broader gates are conditional on those changes invalidating prior evidence, not repeated for metadata. No implementation or tests were run during this targeted plan recheck.

### Current structured verdict

## VERIFICATION PASSED

**Phase:** Complete VR4300 CPU  
**Plans verified:** 18 across 17 waves; 37 tasks  
**Current issues:** 0 blockers, 0 warnings

```yaml
issues: []
```

The iteration 2 warning is resolved in the actual saved Plan 04-16 task clauses. The earlier `ISSUES FOUND` sections above are historical; the current verdict is passed.
