---
phase: 06-cold-boot-cpu-timing-and-full-base-suite
verified: 2026-11-16T02:24:14Z
status: passed
score: 30/30 truths verified
covered_files:
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-01-PLAN.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-01-SUMMARY.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-02-PLAN.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-02-SUMMARY.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-03-PLAN.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-03-SUMMARY.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-04-PLAN.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-04-SUMMARY.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-05-PLAN.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-05-SUMMARY.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-06-PLAN.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-06-SUMMARY.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-07-PLAN.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-07-SUMMARY.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-08-PLAN.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-08-SUMMARY.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-09-PLAN.md
  - .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-09-SUMMARY.md
  - core/CMakeLists.txt
  - core/t64_bus.c
  - core/t64_cache.c
  - core/t64_cpu.c
  - core/t64_digest.c
  - core/t64_exception.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_mi.c
  - core/t64_rdram.c
  - core/t64_sp.c
  - core/t64_warm_devices.c
  - core/tests/test_boot_determinism.c
  - core/tests/test_boot_entry.c
  - core/tests/test_bus.c
  - core/tests/test_cpu_time.c
  - core/tests/test_profile.c
  - core/tests/test_rdram.c
  - core/tests/test_warm_devices.c
  - docs/ci-reviews/LOG.md
  - docs/claims/phase06.md
  - docs/evidence/phase06-cold-boot.md
  - docs/evidence/phase06-final.json
  - docs/evidence/phase06-native.md
  - docs/evidence/phase06-review.md
  - docs/evidence/phase06-security.md
  - docs/spec/base-diagnostic.md
  - docs/spec/cpu-profile.md
  - docs/spec/cpu-timing.md
  - docs/spec/rdram-boot.md
  - fixtures/manifest.json
  - runner/CMakeLists.txt
  - runner/t64_base_diag_protocol.c
  - runner/t64_base_diag_protocol.h
  - runner/t64_base_diagnostic.c
  - runner/t64_cold_boot.c
  - runner/tests/test_base_diag_protocol.c
  - runner/tests/test_cold_boot.c
  - scripts/apply-base-diagnostic-overlay.py
  - scripts/base-diagnostic-lanes.py
  - scripts/build-qualified-base-diagnostic.py
  - scripts/check-base-evidence.py
  - scripts/check-test-inventory.sh
  - scripts/derive-base-diagnostic-source.py
  - scripts/run-base-diagnostic.py
  - scripts/run-cpu-diagnostic.py
  - scripts/tests/test_base_classification.py
  - scripts/tests/test_base_evidence.py
  - scripts/tests/test_base_image.py
  - scripts/tests/test_base_lanes.py
  - scripts/tests/test_base_qualification.py
  - scripts/tests/test_base_run.py
  - scripts/verify-base-diagnostic-qualification.py
covered_digest: "v4:sha256:e7720e299737ecb9bfe1ef134b3a5c0a5cff847737a04adf63174b8cb013d6e0"
behavior_unverified: 0
overrides_applied: 0
decision_coverage:
  honored: 12
  total: 12
  not_honored: 0
---

# Phase 6: Cold Boot, CPU Timing and Full Base Suite Verification Report

**Phase Goal:** The machine reaches IPL3 through the full RDRAM register protocol with instruction-granular CPU timing, and the complete `nemu64-test` base suite is classified per group, by lane, with pinned counts.
**Verified:** 2026-11-16T02:24:14Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|---|---|---|
| 1 | The hello ROM boots both with the RDRAM tier A preset and through the tier B register protocol (delay, device ID, broadcast, MI repeat mode, calibration read-back), and both runs produce the same ISViewer output. | ✓ VERIFIED | The authored cold-hello integration test passes for both named profiles; the runner checks the same literal output bytes and the ordered Tier B protocol trace. The exact fixture and oracle are bound in `docs/evidence/phase06-final.json`. This demonstrates the implemented software profile, not physical silicon accuracy. |
| 2 | Count and Random advance under the tier 1 instruction-granular cost model, and the tests cite their expectation sources. The cycle-accurate tier is documented as deferred, with the evidence needed to promote it. | ✓ VERIFIED | `core/t64_cpu.c` selects the explicit T1 MDU table for the new profiles and updates Count/Random at retirement; `core/tests/test_cpu_time.c` exercises instruction classes, delay slots, exceptions, interrupts, idle time, and failure boundaries. `docs/spec/cpu-timing.md` cites the expectation sources and sets out the T2 promotion evidence. The saved native and sanitizer test logs pass. |
| 3 | The full base suite reports per-group status with its total count pinned. The timing, cycle and cop0hazard groups are reported as informational, experimental groups stay out of the required gate, and stress groups run in a scheduled lane. | ✓ VERIFIED | Independent source derivation, C-owned denominator/classification, lane wrapper, and original-record validator are wired together. The bounded `--check-current` check verified the retained classification without replaying guests. The report distinguishes source classification, authenticated execution, and passing outcomes; blocked RDP source rows retain their own reasons. Informational groups remain outside the required gate, and the stress entry is local-only and scheduled. No private member data is reproduced here. |
| 4 | The milestone's support statement for the first profile uses the claims template: profile, build identity, evidence tier and fixture-manifest entry for every reported result. It makes no claim beyond those results. | ✓ VERIFIED | `scripts/check-claims.sh --files docs/claims/phase06.md` passed. The public receipt and exact archived authored artifacts bind profile, source/build identity, evidence tier, fixture manifest, and oracle; the support statement is limited to those authored results. |
| 5 | Two explicit new profiles fetch through the canonical core with compat-v2 CPU and functional RSP semantics, while all five existing profiles retain their identities and refusals. | ✓ VERIFIED | `core/t64_machine.c` has explicit profile descriptors and capability predicates; profile, boot-entry, and determinism tests in the final saved gates cover the new and existing profiles. |
| 6 | The new profiles apply a named static T1 cost table, including real MDU distinctions, before the first bounded cold startup. | ✓ VERIFIED | The T1 policy is explicit by profile tuple, and the cold-boot integration runs after the T1 implementation. The saved `unit.core.cpu-time` and `integration.cold-hello` results pass. |
| 7 | The full pinned default BASE denominator exists independently of the CPU-only projection, retaining blocked non-CPU members. | ✓ VERIFIED | The source deriver constructs the full BASE authority separately from the CPU projection; C classification retains non-CPU rows. The current retained-record check accepted the pinned source and classified archive. |
| 8 | A complete independently anchored pre-use verifier and normal dispatch accept authored controls and reject unauthorized closure changes before guest directory creation. | ✓ VERIFIED | `scripts/verify-base-diagnostic-qualification.py` is connected to normal dispatch before run-directory creation. Authored acceptance and mutation controls passed in the retained final test logs; `--check-preuse` passed during this verification. |
| 9 | One qualified actual member is attempted on the Tier B/T1 target before long protocol/timing expansion, with original failure or completion retained. | ✓ VERIFIED | The original bounded Plan02 invocation and its launch failure are retained in the validation record. Its missing executed-binary identity makes it unusable as guest-execution evidence; this report does not call it a test result. Later Plan06 execution qualification was independently checked and is the evidence for the completed classification path. |
| 10 | Independent authored register sequences drive deterministic delay, device selection/ID, broadcast, MI repeat and calibration/capacity transitions on the new profiles. | ✓ VERIFIED | `core/t64_rdram.c`, the bus decoder, and `core/tests/test_rdram.c` implement and exercise the bounded protocol transitions, including readback and ordering. The saved `unit.core.rdram-protocol` test passes. |
| 11 | Installed 4 MiB backing remains separate from compat-v2 data access, fetch, store, DMA and register/absent-probe policies. | ✓ VERIFIED | The RDRAM and bus paths keep backing memory and profile-specific access policy distinct; tests cover CPU fetch/data access, stores, DMA, and absent-memory probes. The saved native and sanitizer gates pass. |
| 12 | Every future-affecting protocol state changes the new digest, and refused operations preserve every named latch and historical encoding. | ✓ VERIFIED | State-v13 digest tests cover future-affecting protocol fields; refusal tests compare the relevant snapshots and preserved encodings. These tests pass in the retained final gates. |
| 13 | Actual guest Wired/Random/TLBWR sequences preserve compat-v2 pre-step selection and six-bit high-Wired behavior under T1. | ✓ VERIFIED | The CPU timing and CP0 tests exercise actual instruction sequences and Random selection, including Wired modulo behavior; the final saved native and sanitizer logs pass. |
| 14 | Count/Compare follows elapsed time through ordinary, MDU, exception, interrupt and idle intervals with no false retirement. | ✓ VERIFIED | `core/tests/test_cpu_time.c` exercises those elapsed-time and retirement boundaries against the scheduler; saved `unit.core.cpu-time` passes. |
| 15 | Overflow/resources and equal-tick device/RSP/DMA interleave remain transactional and deterministic across partitions and instances. | ✓ VERIFIED | CPU successor preflight and failure-atomicity checks cover tick overflow and exhausted resources; scheduler tests cover same-tick device/RSP/DMA ordering, partitioning, and independent instances. Saved core tests pass. |
| 16 | Tier B completion includes observed delay, device-ID, broadcast, MI repeat, calibration and capacity operations rather than a content-specific shortcut. | ✓ VERIFIED | The cold runner checks the ordered bus-operation trace and protocol state in addition to the exact hello output and terminal boundary. The saved cold-hello integration test passes. |
| 17 | Each named profile is deterministic across advances and instances; profile/state differences between A and B are preserved. | ✓ VERIFIED | Boot determinism and profile tests compare split/whole advances and independent instances while retaining the intentionally distinct profile and state digests. Final native and sanitizer gates pass. |
| 18 | Every full BASE source member has one authenticated original result or identity-bound blocked/unexecuted reason for the Tier B/T1 target. | ✓ VERIFIED | The C aggregator reconciles source authority with original records and identity-bound blocked/unexecuted classifications. `--check-current` verified the complete retained local record set; no private member result is disclosed. |
| 19 | Timing/cycle/cop0hazard are informational, experimental-only members stay outside required membership and stress has a distinct scheduled local-only path. | ✓ VERIFIED | The source selection and lane wrapper implement these boundaries; controls reject experimental membership and lane contamination, and preserve a separate local scheduled stress entry. Retained `harness.base-lanes` and classification tests pass. |
| 20 | Group classification retains pinned private counts and differentiates full classification from authenticated completion and passing results. | ✓ VERIFIED | The C report emits group/lane classifications against the pinned denominator, while separate predicates represent source completeness, authenticated execution completeness, and all-pass. The exact-count retained-evidence check passes; private counts are intentionally absent from this report. |
| 21 | The new evidence validator authenticates original actual records, independent source membership and exact archived executable/binding identities rather than trusting summaries. | ✓ VERIFIED | `scripts/check-base-evidence.py` checks original record digests and C-owned classification against independently pinned source and exact artifact bindings. `--check-artifacts` and `--check-current` both passed. |
| 22 | Its authored acceptance and mutation tests exercise complete entry and normal dispatch from clean storage without private inputs. | ✓ VERIFIED | The authored evidence test suite includes complete-entry acceptance and resealed mutation controls; its final normal and sanitizer gate records pass. No private inputs were added to those tests. |
| 23 | First-profile public claims bind only cleared authored results to exact profile/build/tier/manifest/oracle; private diagnostic evidence stays private. | ✓ VERIFIED | The claims checker and exact receipt bind the authored smoke result; the allowlisted receipt schema rejects private fields. Claims and rights reviews are recorded, and the authored claims check passes. |
| 24 | Source and security review cover both machine changes and new evidence-validator/qualification threats before the final coherent gate. | ✓ VERIFIED | `docs/evidence/phase06-review.md` records source findings and scoped re-reviews; `docs/evidence/phase06-security.md` records the full threat audit. The final gates were run against the coherent tested source after fixes. |
| 25 | Accepted in-scope fixes and scoped re-review finish before source/build freeze, with no unresolved high/critical threat. | ✓ VERIFIED | The scoped review dispositions close the findings; the security audit records all listed threats mitigated. The source freeze and final gate receipts bind the tested artifacts. |
| 26 | Final normal/instrumented gates, admission fuzz/negative control, exact inventory, actual artifact bindings and authored-only hosted evidence refer to the tested source/artifact honestly. | ✓ VERIFIED | The retained normal gate passed 139/139 and the ASan/UBSan gate passed 141/141, including admission and negative controls. Binding receipts identify the exact tested artifact; both hosted jobs passed for source `aa1e531e05b9c66f0c57d5066c9c367cb1e82c23`, and the exact artifact comparison passed. |
| 27 | All four roadmap criteria and three requirements are reconciled without publishing private external evidence or implying release authority. | ✓ VERIFIED | Decision coverage reports 12/12 context decisions honored. BUS-03, CPU-08, and VER-02 are mapped below; the claims and evidence files stay within authored, profile-specific support. |
| 28 | Only the exact cleared hello/IPL3 closure and two new cold/T1 profiles reach the bounded production probe. | ✓ VERIFIED | `runner/t64_cold_boot.c` admits only the bound fixture/profile combinations and enforces the expected IPL3 completion boundary. The saved cold integration test passes for the named profiles. |
| 29 | The actual zero-RI_SELECT startup preserves its first reached RI/RDRAM operation and original outcome before qualification and long native expansion. | ✓ VERIFIED | The original startup probe and its reached-operation/outcome are recorded in `docs/evidence/phase06-cold-boot.md` and `06-VALIDATION.md`; the follow-on cold-startup control covers the refusal boundary. This early probe is not described as a completed hello boot. |
| 30 | New profile and deterministic boot-seed policies are documented without changing historical profile contracts or claiming hello completion. | ✓ VERIFIED | The profile and RDRAM specifications document the new policies and compat-v2 scope; existing profile tests retain historical behavior. The startup probe is explicitly described as incomplete, separate from the later passing hello test. |

**Score:** 30/30 truths verified (0 present, behavior-unverified).

### Required Artifacts

| Artifact | Expected | Status | Details |
|---|---|---|---|
| `core/t64_machine.c`, `core/t64_cpu.c`, `core/t64_rdram.c`, `core/t64_bus.c` | Named profile, T1, and RDRAM protocol implementation | ✓ EXISTS + SUBSTANTIVE | Explicit profile capability predicates, scheduler/CPU cost dispatch, protocol state and bus integration are present; native component tests exercise these paths. |
| `core/tests/test_cpu_time.c`, `core/tests/test_rdram.c`, `core/tests/test_boot_determinism.c` | CPU timing, protocol, refusal, and determinism controls | ✓ EXISTS + SUBSTANTIVE | Tests exercise the implemented state transitions and failure boundaries; saved final gate logs show them passing. |
| `runner/t64_cold_boot.c`, `runner/tests/test_cold_boot.c`, `fixtures/manifest.json` | Bound cold-boot fixture and exact output/protocol checks | ✓ EXISTS + SUBSTANTIVE | Runner binds the cleared fixture and A/B profiles, observes actual bus activity and ISViewer output, and checks the terminal state. |
| `runner/t64_base_diag_protocol.c`, `runner/t64_base_diagnostic.c` | Full source membership and sole C result authority | ✓ EXISTS + SUBSTANTIVE | C reconciles source membership with original records and exposes separate completeness/pass predicates and group/lane summaries. |
| `scripts/derive-base-diagnostic-source.py`, `scripts/verify-base-diagnostic-qualification.py`, `scripts/run-base-diagnostic.py` | Independent BASE source seal and bounded dispatch | ✓ EXISTS + SUBSTANTIVE | Source authority is checked independently; normal dispatch runs the pre-use verifier before creating an execution directory. |
| `scripts/base-diagnostic-lanes.py` and `scripts/tests/test_base_lanes.py` | Required, informational, and scheduled stress lane boundaries | ✓ EXISTS + SUBSTANTIVE | Explicit local-only entry and lane refusal controls are wired and pass in retained tests. |
| `scripts/check-base-evidence.py`, `scripts/tests/test_base_evidence.py` | Exact original artifact and record validation | ✓ EXISTS + SUBSTANTIVE | Independent context/source/artifact binding, C classification, and resealed mutation refusal are covered; bounded evidence checks pass. |
| `docs/evidence/phase06-final.json`, `docs/claims/phase06.md` | Exact artifact receipt and bounded public claims | ✓ EXISTS + SUBSTANTIVE | Receipt binds the authored fixture/result to profile, build, tier, manifest and oracle; claim checks pass. |
| `docs/evidence/phase06-review.md`, `docs/evidence/phase06-security.md`, `docs/ci-reviews/LOG.md` | Review, threat mitigation, and hosted timing evidence | ✓ EXISTS + SUBSTANTIVE | Source/security dispositions and exact hosted run records exist and align to final source/artifact identity. |

All artifact paths declared by the nine plans passed `query verify.artifacts`. The typed key-link query could not interpret the plans' semantic endpoint labels as file paths; the connections were checked manually against source and retained tests, as summarized below.

### Key Link Verification

| Plan links | Status | Details |
|---|---|---|
| Profile lookup → scheduler/CPU | ✓ WIRED | `core/t64_machine.c` capability predicates select the T1 and RDRAM policies used by the canonical scheduler and CPU paths. |
| Pinned source → C inventory → dispatch/consumer | ✓ WIRED | Independent source derivation feeds C inventory; normal dispatch invokes pre-use checks before run creation; original records bind the exact consumer executable and artifact. |
| Bus decoder/MI state → RDRAM protocol → digest | ✓ WIRED | Complete-span bus decoding gates the protocol; MI repeat and named per-instance state drive transitions; future-affecting state participates in the digest. |
| CPU retirement/cost → Random and Count/Compare | ✓ WIRED | Retirement updates Random once; scheduler elapsed ticks reach Count/Compare; selected costs reserve successor capacity before architectural effects. |
| Bound hello fixture → profile → observer/result | ✓ WIRED | The runner binds fixture identity and rights to named profiles, and compares actual retired completion and canonical ISViewer bytes; partition runs compare per-profile digests. |
| Full source/original records → group/lane report | ✓ WIRED | C authenticates source ordinal, target context, and original records before aggregation; scheduled stress dispatch reaches the qualified local-only runner with fixed limits. |
| Evidence validator → original records/privacy allowlist | ✓ WIRED | Validator hashes the original inputs and exact bindings, delegates completeness to C, and checks the public receipt against a closed schema. |
| Reviewed snapshot → final gate → qualification/hosted pair | ✓ WIRED | Scoped fixes and re-reviews precede the final gate; consumer executable and qualification context are hash-bound; hosted jobs and comparison receipts identify the tested source/artifact. |
| Cold startup runner → admitted IPL3 fixture and retained first outcome | ✓ WIRED | Runner validates the cleared fixture/profile, traces the initial RI operation and retains the original startup result; the later complete hello path is independently tested. |

### Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
|---|---|---|---|---|
| Cold-boot runner | ISViewer transcript and terminal state | Executed authored fixture through the canonical machine and output observer | Yes | ✓ FLOWING |
| CPU timing path | instruction ticks, Count, Random | Retired opcodes and scheduler time | Yes | ✓ FLOWING |
| BASE classification | source rows and outcome categories | Pinned source authority plus authenticated original records or bound blocked reasons | Yes | ✓ FLOWING |
| Claims/evidence validator | profile/build/fixture/oracle result binding | Original archive, exact command/outcome receipts, and manifest | Yes | ✓ FLOWING |

### Behavioral Spot-Checks

| Behavior | Evidence command or retained execution | Result | Status |
|---|---|---|---|
| Both profile hello boot and output comparison | Retained `integration.cold-hello` result in `build/gate/06-08-final/gate-attempt03.log` | Passed | ✓ PASS |
| MDU cost, Count/Random, exception, idle, and failure-atomicity behavior | Retained `unit.core.cpu-time` result in `build/gate/06-08-final/gate-attempt03.log` and ASan/UBSan `LastTest.log` | Passed | ✓ PASS |
| Tier B protocol transitions and bus integration | Retained `unit.core.rdram-protocol` result in normal and ASan/UBSan logs | Passed | ✓ PASS |
| Lane and classification controls | Retained `harness.base-lanes` and `harness.base-classification` results in final native gate | Passed | ✓ PASS |
| Original BASE artifact/context classification without guest replay | `PYTHONDONTWRITEBYTECODE=1 python3 -B scripts/check-base-evidence.py --check-current` | Verified; sanitized status only | ✓ PASS |
| Authored artifact and pre-use validation | `PYTHONDONTWRITEBYTECODE=1 python3 -B scripts/check-base-evidence.py --check-artifacts` and `--check-preuse` | Both verified | ✓ PASS |
| Public claim template and binding | `scripts/check-claims.sh --files docs/claims/phase06.md` | Passed | ✓ PASS |

These are the retained phase executions and bounded read-only evidence checks; no broad gate, guest suite, or guest replay was repeated for this report.

### Probe Execution

No `scripts/*/tests/probe-*.sh` files or explicit shell probe paths were declared in the phase plans. The bounded cold startup and completed hello behaviors are covered by the retained integration tests above.

### Requirements Coverage

| Requirement | Source Plan(s) | Description | Status | Evidence |
|---|---|---|---|---|
| BUS-03 | 06-01, 06-02, 06-03, 06-05, 06-06, 06-07, 06-08, 06-09 | RDRAM tier A (preset RI configuration) works first, and RDRAM tier B (register protocol, MI repeat mode, calibration read-back) follows for the full cold-boot path. | ✓ SATISFIED | Both named profile paths and full Tier B protocol are exercised by the cold integration test; RDRAM transitions and protocol reads are covered by core tests. |
| CPU-08 | 06-01, 06-02, 06-04, 06-05, 06-07, 06-08, 06-09 | CPU timing at tier 1 (instruction-granular cost) is available, and the cycle-accurate tier is documented as deferred. | ✓ SATISFIED | T1 costs and CP0 time updates pass saved component tests; the timing specification documents T2 scope and promotion evidence. |
| VER-02 | 06-02, 06-06, 06-07, 06-08 | Maintainer can see timing, cycle and cop0hazard groups reported as informational, experimental and stress groups kept out of required CI, and a scheduled lane for stress groups. | ✓ SATISFIED | Source classification and lane controls preserve informational, experimental, and scheduled stress boundaries; the stress path is local-only and does not grant public external CI or OS-service authority. |

No other Phase 06 requirements are allocated in `REQUIREMENTS.md`; no orphaned requirement was found.

### Decision Coverage

`query check.decision-coverage-verify` reports all 12 trackable decisions from `06-CONTEXT.md` honored by shipped artifacts.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---|---|---|---|
| — | — | No actionable stub or debt markers in the reviewed source/evidence scope | — | Grep matches were `XXXXXX` temporary-file templates, a URL substring, or legitimate initializers/sentinels; none flows as missing behavior. |

### Human Verification Required

None. The phase goal concerns deterministic emulator code and authored evidence, all exercised by retained tests and bounded validators. Physical-hardware accuracy and release/private-publication authority are explicitly outside this phase's claim.

### Gaps Summary

No gaps found in the roadmap goal or plan must-haves. The historical Plan02 launch attempt is retained as a failure because its executed-binary identity was missing; it is not represented as a guest result or pass. The later Plan06 qualification independently verifies the actual classification capability. The machine's support statement remains limited to the authored evidence tier and exact profile/build/fixture bindings.

---
_Verified: 2026-11-16T02:24:14Z_  
_Verifier: the agent (gsd-verifier)_
