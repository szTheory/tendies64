---
phase: "07"
slug: "rdp-reference-renderer-and-vi-scanout"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-16"
---

# Phase 7 — Validation Strategy

This is a prospective contract. No new RDP/VI test has run or passed yet.
The planner fills exact task/test mappings as plans are generated.

## Test Infrastructure

| Property | Value |
|---|---|
| Framework | Existing C17/CMake/CTest; authored runner fixtures |
| Config | CMakePresets.json, core/CMakeLists.txt, runner/CMakeLists.txt |
| Quick run | `ctest --test-dir build/gate --no-tests=error -R '^unit\.core\.(rdp|vi|dp)' --output-on-failure` after exact names are registered |
| Full normal gate | `./scripts/gate.sh` |
| Instrumented | Existing ASan/UBSan preset and inherited inventory; exact commands in owning plans |
| Runtime | New checks unmeasured. Prior full normal gate77s / ASan+UBSan212s are baseline observations only |

## Sampling Rate

- After each cohesive task: exact owning accepted/refusal/mutation checks.
- Before a long suite: successful real consumer, reached wrong-pixel control,
  exact nonempty registration/inventory, source/body/composition and rights checks.
- At integrated waves: normal inherited regression gate after quick checks pass.
- At phase qualification: complete normal/instrumented gates and exact hosted
  artifact readback/comparison. Do not replay fuzz or private guests for metadata.
- Target quick feedback within30s; measure before claiming this target is met.

## Per-Task Verification Map

| Task/plan | Requirement | Threat/secure behavior | Type | Automated command | Exists | Status |
|---|---|---|---|---|---|---|
| First tracer,07-01 | VID-01,02,03,06 | Checked DP/VI spans, capacity-before-effect, no synthetic completion | Real-runner + literal pixel + wrong-value control | Wave0: exact registered owning commands supplied by plan | No,Wave0 | pending |
| Expansion plans, assigned by planner | VID-01 | Copy/triangle/TMEM/combiner/blender/coverage/Z bounds; independently verified packet lengths | Authored per-class literals + truncation/boundary mutations | Wave0: exact nonempty CTest inventory supplied by plans | No,Wave0 | pending |
| Completion/VI/output plans | VID-02,03,06 | Pending/masked/clear ordering, rational phase, queue-full/resume, bad copy-out span | Native/refusal/partition/interleave | Wave0: exact owning commands supplied by plans | No,Wave0 | pending |
| Policy plan | VID-05 | No uninitialized/global/host entropy; policy binds identity | Repeatability + profile/digest | Wave0: exact owning commands supplied by plan | No,Wave0 | pending |
| Differential evidence plan | VID-04 | D5/asset authority preserved; exact oracle identity, no hardware claim | Authorized differential only | Blocked: no oracle execution is currently authorized | No,authority gate | blocked |

## Wave 0 Requirements

- [ ] Derive first command words/pixel literals independently from checked
  primary fields, including endpoint, packing, endian and six-bit decode.
- [ ] Add real-consumer tracer and same-setup wrong-pixel assertion control;
  register exact nonempty names and source-derived case inventory.
- [ ] New production files and dispatch changes appear in build identity,
  source/body seals and inherited no-HLE checks before broad tests.
- [ ] Planner replaces prospective map with exact task IDs/test commands/threats.
- [ ] Resolve triangle packet-layout contradictions in its owning slice before
  decoder/fixture construction; never mirror a guessed encoder as expected data.

## Manual-Only / Authority Checks

| Behavior | Requirement | Why gated | Required next action |
|---|---|---|---|
| Angrylion-Plus/parallel-rdp differential evidence | VID-04 | D5 unresolved, per-file/asset rights audit incomplete | Preserve stop; complete authorized native work first; owner/legal authority and documented rights before actual use |

No physical measurement is authorized or promised. Authored acceptance is not
substituted for blocked differential evidence.

## Validation Sign-Off

- [ ] Every task has owning automated verification or explicit Wave0 dependency
- [ ] Exact case/test denominator and fails_when signals reject vacuous success
- [ ] Sampling continuity: no3 consecutive tasks without owning verification
- [ ] First real accepted path and wrong-pixel control pass before full suites
- [ ] Full inherited normal/instrumented gates and hosted artifact equality pass
- [ ] VID-04's authority and actual evidence satisfied before completion claim
- [ ] No watch mode or foreground/shared-desktop activation
- [ ] nyquist_compliant updated only from real verification

**Approval:** pending verification; routine execution is already authorized
within project policy, while the specific oracle stop remains unresolved.
