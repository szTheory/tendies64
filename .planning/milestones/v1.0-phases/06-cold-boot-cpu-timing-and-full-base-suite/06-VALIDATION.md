---
phase: "06"
slug: "cold-boot-cpu-timing-and-full-base-suite"
status: validated
nyquist_compliant: true
wave_0_complete: true
created: "2026-11-15"
---

# Phase 06 — Validation Strategy

## Test Infrastructure

| Property | Value |
|---|---|
| Framework | CMake/CTest native C, shell/Python authored qualification controls |
| Config | `CMakePresets.json`, owning `CMakeLists.txt` |
| Quick check | Build all owning targets, then `ctest --preset gate --no-tests=error -R '<owning selection>'` |
| Full checks | Canonical gate and `ctest --preset asan --no-tests=error`, after all-target instrumented build |
| Runtime | Measure first actual selections; do not invent estimates or change guest caps |

## Sampling Rate

Run owning automated checks after substantive tasks; use canonical gates at
coherent source boundaries and before phase verification. Four-job presets
apply when no wall-capped diagnostic is active. Do not rebuild under CTest.
No three consecutive implementation tasks may lack automated verification.
Every plan must supply executable owning commands and explicit failure cases.

## Per-Task Verification Map

Task ordinals below are the exact order in each PLAN. Every automated command
is copied from that task, assumes repository-root cwd, and is paired with its
observable failure below. The commands below retain their original planning contracts. Current owning
checks and saved execution evidence are reconciled in the audit below; historical
failed attempts and versioned CLI changes are not replayed or relabeled. ASVS level 1 applies; unresolved high/critical threats block.

| Task | Wave | Requirement | Threat ref | Owning check | Status |
|---|---|---|---|---|---|
| 06-01-T1 | 1 | BUS-03, CPU-08 | T-06-01 | Fetch one authored instruction through both permanent cold profile identities | verified |
| 06-01-T2 | 1 | CPU-08 | T-06-02 | Charge a real static T1 instruction interval before cold use | verified |
| 06-09-T1 | 2 | BUS-03, CPU-08 | T-06-24 | Wire the bounded cold probe through production admission and canonical execution | verified |
| 06-09-T2 | 2 | BUS-03, CPU-08 | T-06-03 | Observe the first actual bounded cold startup and retain its command | verified |
| 06-02-T1 | 3 | VER-02 | T-06-04 | Authenticate one authored member against the complete source denominator in C | verified |
| 06-02-T2 | 3 | VER-02 | T-06-05, T-06-07 | Qualify immutable target contexts through the complete pre-use entry | verified |
| 06-02-T3 | 3 | BUS-03, CPU-08, VER-02 | T-06-06, T-06-07 | Build and run one qualified real member before broader expansion | original unusable; corrected later in06 |
| 06-03-T1 | 4 | BUS-03 | T-06-08 | Execute the first observed cold register sequence through the production bus | verified |
| 06-03-T2 | 4 | BUS-03 | T-06-09 | Complete deterministic device, broadcast, repeat and calibration transitions | verified |
| 06-03-T3 | 4 | BUS-03 | T-06-10 | Bind all future state and prove exact installed/absent access ownership | verified |
| 06-04-T1 | 5 | CPU-08 | T-06-11 | Run fetched Wired, MDU, Random and TLBWR through real CP0 boundaries | verified |
| 06-04-T2 | 5 | CPU-08 | T-06-12 | Prove variable-interval FIFO, idle, DMA/RSP and partition invariants | verified |
| 06-05-T1 | 6 | BUS-03, CPU-08 | T-06-13 | Complete both admitted hello paths and compare exact output bytes | verified |
| 06-05-T2 | 6 | BUS-03, CPU-08 | T-06-14 | Prove per-profile boot determinism and publish only cleared evidence | verified |
| 06-06-T1 | 7 | VER-02 | T-06-15 | Aggregate one real saved target record into an independently complete classification | verified |
| 06-06-T2 | 7 | BUS-03, CPU-08, VER-02 | T-06-16 | Execute and retain every permitted required target member under immutable contexts | verified |
| 06-06-T3 | 7 | VER-02 | T-06-17 | Separate informational, experimental and scheduled stress lanes | verified |
| 06-07-T1 | 8 | VER-02 | T-06-18, T-06-19 | Validate one actual saved record through the complete evidence entry | verified |
| 06-07-T2 | 8 | VER-02 | T-06-20 | Bind the first-profile support statement to cleared exact evidence | verified |
| 06-08-T1 | 9 | BUS-03, CPU-08, VER-02 | T-06-21 | Review the complete source and original-evidence path before final gating | verified |
| 06-08-T2 | 9 | BUS-03, CPU-08, VER-02 | T-06-22 | Close accepted findings through their owning bounded source slices | verified |
| 06-08-T3 | 9 | BUS-03, CPU-08, VER-02 | T-06-22, T-06-23 | Qualify the final stable artifact and reconcile bounded support evidence | verified |

## Exact Automated Commands and Failure Contracts

### 06-01-T1

Automated: `cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^unit.core.(profile|boot-entry|digest-trace)$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^unit.core.(profile|boot-entry|digest-trace)$'`

Fails when: Either profile cannot retire the selected opcode, either seed selects the wrong boot policy, any historical vector changes, or any focused owner is absent.

### 06-01-T2

Automated: `cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^unit.core.(cpu-time|cpu-exceptions|profile)$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^unit.core.(cpu-time|cpu-exceptions|profile)$'`

Fails when: Literal next-event ticks disagree with any cost class, a fault retires or steps Random, overflow mutates architecture, or a historical profile acquires T1 costs.

### 06-09-T1

Automated: `sh scripts/build-boot-fixtures.sh --qualify && cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^runner.cold-boot-controls$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^runner.cold-boot-controls$'`

Fails when: An accepted fixture fails before canonical execution, identity mutation launches, a reached-operation assertion is missing, or a control bypasses production dispatch.

### 06-09-T2

Automated: `ctest --preset gate --no-tests=error -R '^integration.cold-startup$'`

Fails when: The qualified bytes never reach the cold RI/RDRAM seam, the test accepts arbitrary halt/output as completion, input mutation launches, the deadline is exceeded, or the original observation/command is absent.

### 06-02-T1

Automated: `cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^runner.base-diagnostic-protocol$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^runner.base-diagnostic-protocol$'`

Fails when: A blocked member disappears, a duplicate name aliases another source identity, an empty-value registration has zero executions, a reduced resealed pair passes, or Python declares completion.

### 06-02-T2

Automated: `python3 -B scripts/tests/test_base_qualification.py`

Fails when: An accepted fixture never reaches final dispatch, a negative case fails during setup instead of its intended guard, context data widens source authority, either archive half is missing, or any refusal creates a run directory.

### 06-02-T3

Automated: `python3 -B scripts/tests/test_base_qualification.py && python3 -B scripts/tests/test_base_run.py && cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^(runner.base-diagnostic-protocol|harness.base-(qualification|run))$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^(runner.base-diagnostic-protocol|harness.base-(qualification|run))$' && python3 -B scripts/build-qualified-base-diagnostic.py --qualify-build && python3 -B scripts/run-base-diagnostic.py --run-one --adapter build/gate/runner/t64-base-diagnostic`

Fails when: Actual use occurs before full pre-use/composition guards, a blocked asset compiles, the member uses another profile, the original outcome is missing, or a wrapper labels refusal/timeout/setup error as a pass.

### 06-03-T1

Automated: `cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^unit.core.(rdram-protocol|bus|warm-device-policy)$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^unit.core.(rdram-protocol|bus|warm-device-policy)$'`

Fails when: The authored instruction never reaches the intended register, old profiles admit the new sequence, bad spans partially mutate, or a changed unrelated latch escapes the snapshot comparison.

### 06-03-T2

Automated: `cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^unit.core.(rdram-protocol|warm-device-policy|bus|profile)$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^unit.core.(rdram-protocol|warm-device-policy|bus|profile)$'`

Fails when: Any transition lacks a reached independent assertion, a readback has no provenance/model rule, repeated commands produce unspecified effects, or calibration/MI changes historical behavior.

### 06-03-T3

Automated: `cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^unit.core.(rdram-protocol|warm-device-policy|bus|profile|digest-trace|pi-dma|sp-dma)$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^unit.core.(rdram-protocol|warm-device-policy|bus|profile|digest-trace|pi-dma|sp-dma)$'`

Fails when: A future state field is digest-invisible, rejected spans change any named state, installed backing aliases an absent range, or partition/instance/repeated-input results disagree.

### 06-04-T1

Automated: `cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^unit.core.(cpu-time|cpu-cp0|cpu-exceptions|cpu-integer|cpu-control)$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^unit.core.(cpu-time|cpu-cp0|cpu-exceptions|cpu-integer|cpu-control)$'`

Fails when: A sequence does not retire its intended instruction, any literal class boundary is wrong, TLBWR uses post-step Random, high-Wired follows the strict policy, or nonretirement changes Random.

### 06-04-T2

Automated: `cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^unit.core.(cpu-time|scheduler|determinism|sp-dma|pi-dma|rsp-control)$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^unit.core.(cpu-time|scheduler|determinism|sp-dma|pi-dma|rsp-control)$'`

Fails when: Any equal-tick reorder, lost intervening device work, different partition/instance digest, unsynchronized idle Count or partial resource failure occurs.

### 06-05-T1

Automated: `cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^runner.cold-boot-controls$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^runner.cold-boot-controls$' && ctest --preset gate --no-tests=error -R '^integration.cold-hello$'`

Fails when: Either profile fails genuine terminal retirement, capture differs from authored bytes, any Tier B protocol family is unobserved, or identity/partial-output controls are accepted.

### 06-05-T2

Automated: `cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && sh scripts/check-test-inventory.sh --selftest && sh scripts/check-test-inventory.sh --preset gate && sh scripts/check-test-inventory.sh --preset asan && ctest --preset gate --no-tests=error -R '^(unit.core.(boot-entry|profile)|integration.cold-determinism)$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^(unit.core.(boot-entry|profile)|integration.cold-determinism)$'`

Fails when: Any intended event/operation does not execute, a within-profile partition/state/capture differs, reset retains prior protocol state, clean setup needs private inputs, or inherited test names disappear.

### 06-06-T1

Automated: `cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^runner.base-diagnostic-protocol$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^runner.base-diagnostic-protocol$'`

Fails when: Any source member silently vanishes, order changes identity, a blocker becomes pass, reduced membership is accepted, or original bytes/terminal identity are not authenticated.

### 06-06-T2

Automated: `python3 -B scripts/tests/test_base_qualification.py && python3 -B scripts/tests/test_base_run.py && cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^harness.base-(qualification|run)$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^harness.base-(qualification|run)$' && python3 -B scripts/run-base-diagnostic.py --run-all --adapter build/gate/runner/t64-base-diagnostic && python3 -B scripts/run-base-diagnostic.py --verify-complete --adapter build/gate/runner/t64-base-diagnostic`

Fails when: Source membership is omitted, stale profile/build results are pooled, an interrupted context is overwritten, blocked assets compile, or Python process success hides a C incomplete/failed aggregate.

### 06-06-T3

Automated: `python3 -B scripts/tests/test_base_lanes.py && cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^harness.base-lanes$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^harness.base-lanes$' && python3 -B scripts/base-diagnostic-lanes.py --run-informational --local-only && python3 -B scripts/base-diagnostic-lanes.py --run-stress --local-only`

Fails when: Informational/experimental/stress results enter required success, a mere lane definition becomes execution evidence, external execution works in CI, or a selection escapes source/rights/resource authority.

### 06-07-T1

Automated: `python3 -B scripts/tests/test_base_evidence.py && cmake --preset gate && cmake --build --preset gate && cmake --preset asan && cmake --build --preset asan && ctest --preset gate --no-tests=error -R '^harness.base-evidence$' && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^harness.base-evidence$' && python3 -B scripts/check-base-evidence.py --check-current`

Fails when: Any accepted control misses final dispatch, a negative fails only at setup, two consistently reduced snapshots pass, original data is replaced by summary assertions, artifact substitution succeeds, or actual private aliases leak.

### 06-07-T2

Automated: `python3 -B scripts/tests/test_base_evidence.py && python3 -B scripts/check-base-evidence.py --check-current && sh scripts/check-claims.sh --files docs/claims/phase06.md && sh scripts/rights-scan.sh --tree && sh scripts/pii-scan.sh --tree`

Fails when: A claim lacks exact profile/build/tier/fixture/oracle binding, uses private diagnostics as public support, claims an unexecuted lane or hardware/general compatibility, or a sanitized receipt fails original-record validation.

### 06-08-T1

Automated: `python3 -B scripts/tests/test_base_qualification.py && python3 -B scripts/tests/test_base_evidence.py && python3 -B scripts/check-base-evidence.py --check-current && sh scripts/check-docs.sh`

Fails when: Actual evidence no longer validates, review omits the new validator/source authority/privacy threats, either review is missing/inconclusive, or an unresolved high/critical threat has no blocking disposition.

### 06-08-T2

Automated: `python3 -B scripts/tests/test_base_qualification.py && python3 -B scripts/tests/test_base_run.py && python3 -B scripts/tests/test_base_evidence.py && python3 -B scripts/check-base-evidence.py --check-current`

Fails when: Any accepted finding lacks an exact owning source/test correction and passing scoped evidence, changed code lacks required re-review, a high/critical threat remains, or a stale binding is relabeled after rebuild.

### 06-08-T3

Automated: `sh scripts/gate.sh && cmake --preset asan && cmake --build --preset asan && sh scripts/check-test-inventory.sh --preset asan && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error && python3 -B scripts/check-base-evidence.py --check-artifacts && python3 -B scripts/check-base-evidence.py --check-current && sh scripts/check-claims.sh --files docs/claims/phase06.md && sh scripts/rights-scan.sh --tree && sh scripts/pii-scan.sh --tree`

Fails when: The gate/inventory/fuzz/negative control fails or is absent, source changes during a gate, final artifacts fail exact archive/binding/composition checks, a required hosted job lacks actual successful evidence, private data leaks, or any claim exceeds its actual evidence.

## Wave 0 and Execution Ordering

No new framework, dependency or installation. New owners are created with
their behavioral tasks before checks consume them. Register these exact new
names while preserving all inherited normal/instrumented names: Plan09
`runner.cold-boot-controls`, `integration.cold-startup`; Plan02
`runner.base-diagnostic-protocol`, `harness.base-qualification`,
`harness.base-run`; Plan03 `unit.core.rdram-protocol`; Plan05
`integration.cold-hello`, `integration.cold-determinism`; Plan06
`harness.base-lanes`; Plan07 `harness.base-evidence`.
The inherited baseline is 127 normal / 129 instrumented; final counts are
that exact inherited name set plus these real additions. Removal, disabled,
duplicate, unexpected and empty inventories fail.

All-target normal and instrumented builds precede focused native checks.
The existing four-job presets apply; no bare serial --test-dir substitution.
Protocol and timing expansions use separate waves despite disjoint files:
they share canonical core and gate/asan build trees. Source/build mutation
stops during every test/gate. Long sanitizer/fuzz work exits before actual
wall-capped guests. First actual cold startup is 06-09-T2; first qualified
actual member is 06-02-T3, both before long expansion. Archive exact consumer
executable AND binding before any later rebuild. No widened cap/window or
automatic retry is permitted.

Execution is 01 → 09 → 02 → 03 → 04 → 05 → 06 → 07 → 08, waves 1–9.
The appended number preserves existing identities; Plan02 explicitly depends
on Plan09. No qualification or native expansion intervenes before the actual
cold probe, and the early qualified member remains before protocol/timing
expansion. Plan01 now owns nine files/two tasks; Plan09 owns six files/two
tasks. All original affected files and checks remain assigned. Threat T-06-03
moves with actual cold use; new T-06-24 covers its extracted admission task.

## Scope Warning Dispositions

The 15-file blocker is removed by the real cold-runner split above. Keep four
advisory union warnings visible: Plan02 has 13 files/three tasks, Plan03 11/
three, Plan05 10/two, and Plan06 13/three. Every one of their tasks owns exactly
five files; no affected file is hidden or removed. Their projections include
implementation, reads and verification: 46k, 41k, 30k and 39k tokens respectively,
below half a conservative 200k context window. Current calibration has zero
samples, factor 1 and low confidence; estimates are projections rather than
measurements. Each plan records its concrete shared interfaces and task-local
read strategy. Retaining these cohesive existing seams avoids handoff/reread
cost without relaxing any membership, state, lane, test or authority contract.

## Multi-Source Coverage Audit

Every in-scope source item has owning executable coverage and retained passing
current checks; no private inventory/count/result is copied here. Historical
failed first-use attempts remain explicitly unusable and are not inferred passes.

| Source | ID / item | Planned coverage | Status |
|---|---|---|---|
| GOAL | Cold RDRAM path with real T1 and complete classified BASE | 01–09 | COVERED |
| GOAL | Success criterion 1: Tier A/B actual hello, all protocol families, equal exact output | 09-T1..T2, 03-T1..T3, 05-T1..T2 | COVERED |
| GOAL | Success criterion 2: Count/Random T1, expectation sources, T2 promotion evidence | 01-T2, 04-T1..T2 | COVERED |
| GOAL | Success criterion 3: full pinned private denominator, informational/experimental/stress lanes | 02-T1, 06-T1..T3 | COVERED |
| GOAL | Success criterion 4: claims template and exact profile/build/tier/fixture/oracle | 07-T2, 08-T3 | COVERED |
| REQ | BUS-03 | 01, 09, 02, 03, 05, 06, 07, 08 | COVERED |
| REQ | CPU-08 | 01, 09, 02, 04, 05, 06, 07, 08 | COVERED |
| REQ | VER-02 | 02, 06, 07, 08 | COVERED |
| RESEARCH | Explicit policy predicates, new state domain, frozen old profiles | 01-T1, 03-T3 | COVERED |
| RESEARCH | Actual cleared zero-RI_SELECT probe before device expansion | 09-T1..T2 | COVERED |
| RESEARCH | Protocol topology/readback source/model provenance and exact spans | 03-T1..T3 | COVERED |
| RESEARCH | NEC Table 3-12 literal MDU anchors and real static T1 table | 01-T2, 04-T1 | COVERED |
| RESEARCH | Count phase/Compare, pre-step Random/TLBWR and compat-v2 high-Wired | 04-T1 | COVERED |
| RESEARCH | Failure preflight, FIFO, DMA/RSP, idle, partitions/instances | 01-T2, 04-T2 | COVERED |
| RESEARCH | Full pinned default/BASE features, aliases/ordinals/empty values | 02-T1, 06-T1..T3 | COVERED |
| RESEARCH | Independent source seal and sole C completeness authority | 02-T1, 06-T1 | COVERED |
| RESEARCH | Immutable context, full pre-use/normal dispatch, tested artifacts | 02-T2..T3, 06-T2 | COVERED |
| RESEARCH | Source/tool/lock/asset exclusion before build; composition after build before use | 02-T2..T3 | COVERED |
| RESEARCH | Complete-entry mutation controls and real saved-format validation | 02-T2, 07-T1 | COVERED |
| RESEARCH | Both shrunk/resealed membership and actual privacy alias controls | 06-T1, 07-T1 | COVERED |
| RESEARCH | Exact inventories, admission fuzz/negative control, normal/ASan and source stability | 05-T2, 08-T3 | COVERED |
| RESEARCH | Bounds/no-retry, early use and workload serialization | 09-T1..T2, 02-T3, 06-T2..T3, 08-T3 | COVERED |
| RESEARCH | Verification-scope, source annotation versus content identity, honest evidence reuse | 05-T2, 08-T2..T3 | COVERED |
| RESEARCH | New validator threats reviewed before combined gate; scoped fixes and planned artifact checks | 07-T1, 08-T1..T3 | COVERED |
| RESEARCH | Authored-only hosted pair, claims/privacy and parent closeout | 07-T2, 08-T3 | COVERED |
| CONTEXT | D-01: exactly two cold/T1 identities; freeze existing five | 01-T1, 09-T2, 03-T3, 05-T1 | COVERED |
| CONTEXT | D-02: reuse cleared IPL3; actual early cold branch | 09-T1..T2, 05-T1 | COVERED |
| CONTEXT | D-03: deterministic protocol and complete future state | 03-T1..T3, 05-T1 | COVERED |
| CONTEXT | D-04: 4 MiB backing and distinct access owners | 03-T1..T3 | COVERED |
| CONTEXT | D-05: real small static cost table, independent weights | 01-T2, 04-T1 | COVERED |
| CONTEXT | D-06: existing timer/compatibility/order/resource ownership | 04-T1..T2 | COVERED |
| CONTEXT | D-07: versioned timing identity/state; T2 evidence limits | 01-T1..T2, 03-T3, 04-T2, 07-T2 | COVERED |
| CONTEXT | D-08: full private source denominator and one Tier B target | 02-T1, 06-T1..T3 | COVERED |
| CONTEXT | D-09: one C authority, lossless-record fallback, blocked members | 02-T1, 06-T1 | COVERED |
| CONTEXT | D-10: immutable contexts and independent full pre-use | 02-T2..T3, 06-T2, 07-T1 | COVERED |
| CONTEXT | D-11: early bounded uses, all-target builds/caps, final review then gate | 09-T1..T2, 02-T3, 08-T1..T3 | COVERED |
| CONTEXT | D-12: lanes, scoped claims and explicit stops | 06-T3, 07-T2, 08-T3 | COVERED |

## Spec-Less Fallback and No-Silent-Drop Contract

Both SPEC sections are absent. Resolve the actual probe's BUS-03 idempotency
edge with 03-T2/T3 repeated reads/writes/reset sequences and exact state
assertions; resolve its concurrency/interruption edge with 03-T3 separate
instances, reset-mid-sequence and partition interleave. Core use remains
single-threaded per owning API; no host locking or concurrency is inferred.

The probe's CPU-08 and VER-02 unclassified rows remain **flagged assumptions**,
not fabricated detector categories. CPU-08's concrete assumptions are the
software issue-plus-stall mapping and selected CP0 order, asserted in 01-T2
and 04-T1/T2. VER-02's concrete assumptions are actual pinned feature/level
semantics and classified-versus-passing completion, asserted in 02-T1 and
06-T1/T3. A contradiction routes to the owning task/source decision and is
reported; no silent denominator reduction or feature drop is permitted.

No external API/SDK/service integration is planned: this phase extends the
in-tree machine, local source-qualified diagnostics and existing hosted jobs.
No ORM/database schema push applies. The generalized phase-member identity
and direct C protocol decision are recorded in Plan02; historical component
contracts remain frozen.

## Prohibitions and Final Evidence Boundary

No T2 pipeline, 8 MiB expansion, dynamic cache/bus timing, RDP implementation,
new dependencies, inferred asset rights, physical test, release publication,
external hosted use/upload, new OS service/self-hosted runner, extra concurrency
or widened resource window. Blocked non-CPU/RDP/asset rows remain in full BASE.
A complete classification is distinct from authenticated execution completion
and all-pass; Python cannot supply another completeness calculator.
D2 and D6 remain owned external/publication stops and do not require routine
acknowledgment. Private identities/counts/results stay in ignored owner-only
storage. No new foreground or human-only verification is required.

## Validation Sign-Off

- [x] Exact per-task commands and phase-unique threat mapping planned
- [x] Every goal, requirement, research constraint and locked decision mapped
- [x] New owner/registration ordering and inherited inventories specified
- [x] Full-entry controls, early uses, review/fix/final gate ordering specified
- [x] Actual native/guest/hosted commands executed and retained
- [x] Final exact artifacts and all source/threat claims reconciled
- [x] `nyquist_compliant: true` after actual validation

**Status:** Automated coverage validated against retained owning checks and final
artifact evidence. Independent phase goal verification follows.

## Execution coverage reconciliation

| Requirement | Current owning evidence | Coverage |
| --- | --- | --- |
| BUS-03 | unit.core.rdram-protocol/bus/warm-device-policy/profile; integration.cold-startup/cold-hello; original authored A/B observations and deterministic boot controls | COVERED |
| CPU-08 | unit.core.cpu-time/cpu-cp0/cpu-exceptions and partition/FIFO controls; docs/spec/cpu-timing.md literal T1 source anchors and T2 evidence limits | COVERED |
| VER-02 | runner.base-diagnostic-protocol; harness.base-classification/qualification/image/run/lanes/evidence; retained separate lane originals, pinned complete source authority, closed claims | COVERED |

All inherited and added registered tests passed the full normal gate and
instrumented suite, including admission fuzz/negative control. Source freeze
and exact archived consumers/bindings were verified; host-only parser repairs
ran both owning presets and scoped re-review, with unchanged native semantics
retained honestly. Both authored hosted jobs and exact artifact comparison
passed for aa1e531, run43788405776 attempt1. Detailed evidence is in the nine
plan summaries and docs/evidence/phase06-{native,review,security}.md.

The original Plan02 invocation has no valid member result and lacks its
executed binary. Its planned early-success chronology was not achieved and
cannot be reconstructed. Later independently qualified real target execution
and original-record classification in Plan06 cover the capability; they do not
retroactively turn the failed early attempt into a pass. Initial masked RED,
missed separate RED commit and undiscovered mutation-function deviations are
retained in Plan07 and corrected controls. No manual-only requirement is added
for deferred hardware, private publication or release decisions outside scope.

## Validation Audit 2026-11-16

| Metric | Count |
|---|---|
| Gaps found | 0 |
| Resolved | 0 |
| Escalated | 0 |
