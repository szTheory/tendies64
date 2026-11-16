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
| 07-01.1 | VID-01,VID-02,VID-03,VID-05,VID-06 | T-07-01,T-07-02,T-07-03; Prove one CPU fill to raw-frame path end to end | tracer | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite fill --run` | Planned; created by owning task | pending execution |
| 07-01.2 | VID-01,VID-02,VID-03,VID-05,VID-06 | T-07-01,T-07-02,T-07-03; Reject malformed fill admission and prove tracer ownership guards | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite fill --run` | Planned; created by owning task | pending execution |
| 07-02.1 | VID-01,VID-02 | T-07-04,T-07-05; Expose source-backed DPC transport and real FullSync interrupt ordering | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite dp --run` | Planned; created by owning task | pending execution |
| 07-02.2 | VID-01,VID-02 | T-07-04,T-07-05; Run fetched RSP DP access and transport edge controls | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite dp --run` | Planned; created by owning task | pending execution |
| 07-03.1 | VID-01,VID-05 | T-07-06,T-07-07; Copy texture data through source-backed TMEM and tile state | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite copy --run` | Planned; created by owning task | pending execution |
| 07-03.2 | VID-01,VID-05 | T-07-06,T-07-07; Prove copy vectors and texture-boundary refusals through normal dispatch | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite copy --run` | Planned; created by owning task | pending execution |
| 07-04.1 | VID-01 | T-07-08,T-07-09; Resolve packet contradictions and render edge/shade triangles | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite triangles --run` | Planned; created by owning task | pending execution |
| 07-04.2 | VID-01 | T-07-08,T-07-09; Expand edge adjacency, degenerate and packet-length controls | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite triangles --run` | Planned; created by owning task | pending execution |
| 07-05.1 | VID-01 | T-07-10,T-07-11; Render textured and shade-textured triangles through the real pipeline | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite texture --run` | Planned; created by owning task | pending execution |
| 07-05.2 | VID-01 | T-07-10,T-07-11; Qualify texture coordinate, format and packet-boundary controls | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite texture --run` | Planned; created by owning task | pending execution |
| 07-06.1 | VID-01,VID-05 | T-07-12,T-07-13; Combine pixel and alpha sources using exact documented precision | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite combiner --run` | Planned; created by owning task | pending execution |
| 07-06.2 | VID-01,VID-05 | T-07-12,T-07-13; Prove combiner denominator and policy identity across instances | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite combiner --run` | Planned; created by owning task | pending execution |
| 07-07.1 | VID-01,VID-05 | T-07-14,T-07-15; Define coverage memory mapping and render blender modes | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite blender --run` | Planned; created by owning task | pending execution |
| 07-07.2 | VID-01,VID-05 | T-07-14,T-07-15; Prove external-write ownership, overlap and boundary effects | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite blender --run` | Planned; created by owning task | pending execution |
| 07-08.1 | VID-01 | T-07-16,T-07-17; Render Z-enabled primitives with source-backed depth state | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite depth --run` | Planned; created by owning task | pending execution |
| 07-08.2 | VID-01 | T-07-16,T-07-17; Qualify depth boundaries, overlap and canonical state | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite depth --run` | Planned; created by owning task | pending execution |
| 07-09.1 | VID-03,VID-06 | T-07-18,T-07-19; Scan multiple geometries and fields from exact guest time | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite vi-timing --run` | Planned; created by owning task | pending execution |
| 07-09.2 | VID-03,VID-06 | T-07-18,T-07-19; Prove latch, partition, equal-tick and geometry boundary behavior | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite vi-timing --run` | Planned; created by owning task | pending execution |
| 07-10.1 | VID-03,VID-05,VID-06 | T-07-20,T-07-21,T-07-22; Filter VI pixels using independently derived bounded equations | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite vi-filters --run` | Planned; created by owning task | pending execution |
| 07-10.2 | VID-03,VID-05,VID-06 | T-07-20,T-07-21,T-07-22; Prove lossless output-full resume and atomic observer delivery | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite video-output --run` | Planned; created by owning task | pending execution |
| 07-11.1 | VID-01,VID-02,VID-03,VID-05,VID-06 | T-07-23,T-07-24,T-07-25; Qualify the complete authored mode and mutation denominator before long gates | auto | `cmake --preset gate && cmake --build --preset gate && python3 -B scripts/qualify-rdp-vi.py --selftest` | Planned; created by owning task | pending execution |
| 07-11.2 | VID-01,VID-02,VID-03,VID-05,VID-06 | T-07-23,T-07-24,T-07-25; Run required coherent qualification and report bounded support evidence | auto | `python3 -B scripts/qualify-rdp-vi.py --native && python3 -B scripts/qualify-rdp-vi.py --collect-hosted && python3 -B scripts/qualify-rdp-vi.py --verify-evidence` | Planned; created by owning task | pending execution |
| 07-12.1 | VID-04 | T-07-26; Prepare the exact bounded authority request and per-file/asset audit | auto | `python3 -B scripts/check-rdp-oracle-authority.py --selftest && python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` | Planned; created by owning task | blocked authority lane |
| 07-12.2 | VID-04 | T-07-26; preserve stop until real authority | checkpoint:decision | `python3 -B scripts/check-rdp-oracle-authority.py --selftest` | Planned; created by owning task | blocked authority lane |
| 07-13.1 | VID-04 | T-07-27,T-07-28; Implement the gated original-record differential harness | auto | `python3 -B scripts/run-rdp-differential.py --selftest && python3 -B scripts/check-rdp-oracle-authority.py --require-approved` | Planned; created by owning task | blocked authority lane |
| 07-13.2 | VID-04 | T-07-27,T-07-28; Acquire actual authorized comparisons and retain honest conformance status | auto | `python3 -B scripts/run-rdp-differential.py --run-authorized && python3 -B scripts/run-rdp-differential.py --verify-evidence` | Planned; created by owning task | blocked authority lane |
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

## Execution Dependency and Exact Test Map

Every plan has two tasks. Plans 07-01 through 07-11 are the authorized native lane;
07-12 is the real authority checkpoint and 07-13 is conditional actual differential work.
Waves 1–13 follow the matching plan number; shared canonical source, registration and
case inventory make those dependencies explicit. No same-wave file overlap is hidden.
The phase tracer spans integration files because one real CPU-to-runner path must exist
before expansion; it is restricted to one fill command path, not a renderer-wide first task.

| Plan | Exact CTest registration(s) | Case inventory source | Owning threat refs |
|---|---|---|---|
| 07-01 | unit.core.rdp.fill; unit.core.vi.fill; runner.rdp-fill; controls.real.rdp-wrong-pixel | docs/spec/rdp-vi-cases.json suite fill and actual --list-cases dispatch | T-07-01–03 |
| 07-02 | unit.core.dp.transport | suite dp | T-07-04–05 |
| 07-03 | unit.core.rdp.copy | suite copy | T-07-06–07 |
| 07-04 | unit.core.rdp.triangles | suite triangles | T-07-08–09 |
| 07-05 | unit.core.rdp.texture | suite texture | T-07-10–11 |
| 07-06 | unit.core.rdp.combiner | suite combiner | T-07-12–13 |
| 07-07 | unit.core.rdp.blender | suite blender | T-07-14–15 |
| 07-08 | unit.core.rdp.depth | suite depth | T-07-16–17 |
| 07-09 | unit.core.vi.timing | suite vi-timing | T-07-18–19 |
| 07-10 | unit.core.vi.filters; unit.core.vi.output; controls.real.rdp-drain-cadence | suites vi-filters and video-output | T-07-20–22 |
| 07-11 | harness.rdp-vi-qualification plus complete inherited normal/ASan inventories and exact authored hosted pair | qualification helper --selftest/--native/--collect-hosted/--verify-evidence | T-07-23–25 |
| 07-12 | harness.rdp-oracle-authority | authored authority checker selftests and real fail-closed authority record | T-07-26 |
| 07-13 | harness.rdp-differential | original-record harness --selftest then authorized actual corpus | T-07-27–28 |

All plans also retain reserved T-07-SC; there are no new packages to install.
No test in this table exists or passed merely because this strategy lists it.
Registration must precede each owning task's first RED/verification, including the first
task in a plan; a registration promised only by a later task is insufficient.
Every automated verify has an immediately following fails_when that rejects nonzero
commands, missing/unreached assertions, empty selection and absent original owning logs.

## Spec-less Edge Coverage and Explicit Assumptions

Input: build/gate/07-edge-coverage.json (8 applicable unresolved rows; 5 unclassified).
No source SPEC supplies an edge section. These are prospective authored criteria;
the probe input is retained unchanged, and planning is not verification.

| Probe requirement/category | Explicit planned criterion | Owner/status |
|---|---|---|
| VID-01 adjacency | Shared/touching triangles have no duplicate/missing samples; adjacent TMEM tiles do not alias; alpha/depth ties follow documented equations | 07-03.2, 07-04.2, 07-07.2, 07-08.2; pending |
| VID-01 empty | Empty list, zero-area rectangle, degenerate triangle, single texel/sample and malformed null/span cases reach their defined result/refusal without synthetic output/completion | 07-01.2, 07-03.2, 07-04.2; pending |
| VID-01 ordering | Equal-tick primitives retain central tick/sequence order and overlapping colors demonstrate it; FullSync cannot overtake prior effects | 07-02.1–2, 07-04.2, 07-08.2; pending |
| VID-02 unclassified | No inferred generic edge predicate; explicit fence/pending/mask/clear tests apply, but unclassified-probe assumption remains flagged until execution review | 07-02; flagged unresolved assumption |
| VID-03 unclassified | No inferred generic edge predicate; explicit geometry/latch/phase/filter tests apply; unclassified assumption remains flagged | 07-09–10; flagged unresolved assumption |
| VID-04 unclassified | No inferred generic edge predicate; actual authority and comparison identity are required; generic probe remains unclassified | 07-12–13; blocked, flagged unresolved assumption |
| VID-05 unclassified | No inferred generic edge predicate; seeded/undefined-policy identity and refusal-cadence tests apply; generic probe remains flagged | 07-06; flagged unresolved assumption |
| VID-06 unclassified | No inferred generic edge predicate; atomic copy/full/resume/drain-cadence tests apply; generic probe remains flagged | 07-10; flagged unresolved assumption |

Recalled prohibitions are projected as flat must_haves.prohibitions entries in 07-12:
do not call differential agreement hardware truth, use/copy restricted oracle material
without authority, or substitute authored pins for VID-04. They are descriptorless,
unresolved judgment items and **flagged-unverified**, not mechanically green.
projectProhibitions supports flat statement/status/verification/reason and optional
check_kind/check_target/check_rule/check_violation_fixture/check_clean_fixture;
no nonexistent node-test/lint descriptor or fail-first proof is fabricated.
The real authority/comparator controls are substantive planned behavior and do not
retroactively manufacture prohibition producer evidence.

## Multi-Source Coverage Audit

| Source | ID/item | Plan | Status/limits |
|---|---|---|---|
| GOAL | In-tree RDP to RDRAM | 07-01–08 | COVERED in executable scope; not executed |
| GOAL | Profile TV timing/raw VI | 07-01,09–10 | COVERED; programmed diagnostic first, standard NTSC later |
| GOAL | Labeled differential, never hardware truth | 07-12–13 | COVERED by genuine checkpoint then actual work; acceptance blocked |
| REQ | VID-01 all required modes | 07-01–08,11 | COVERED; every class independently expected |
| REQ | VID-02 FullSync/DP interrupt | 07-01–02,08,11 | COVERED |
| REQ | VID-03 geometry/timing/interlace/formats/filtering | 07-01,09–11 | COVERED |
| REQ | VID-04 both labeled out-of-tree oracles | 07-12–13 | COVERED; not substituted by authored tests, remains blocked |
| REQ | VID-05 named undefined policies | 07-01,03,06–07,10–11 | COVERED |
| REQ | VID-06 separate raw output | 07-01,09–11 | COVERED |
| RESEARCH | Six-bit opcode/original bytes/fill primary endpoint-packing gate | 07-01 | COVERED |
| RESEARCH | Incremental DP, DPC status/XBUS, sync discrepancy/FullSync/MI | 07-02 | COVERED |
| RESEARCH | 4 KiB TMEM/eight tiles, formats, load/copy controls | 07-03,05 | COVERED |
| RESEARCH | Triangle primary source contradiction before decoder/vectors | 07-04,08 | COVERED by owning-slice evidence gate |
| RESEARCH | Shading, textures, combiner, blender, coverage and Z | 07-04–08 | COVERED |
| RESEARCH | Hidden coverage/depth mapping and external-write ownership | 07-07–08 | COVERED; primary fact versus named software policy explicit |
| RESEARCH | VI source clocks, active geometry/latches/field and filters | 07-09–10 | COVERED |
| RESEARCH | Rational guest-time mapping, work costs/tie policies | 07-01–02,09 | COVERED as named software model where not sourced |
| RESEARCH | Lossless queue, atomic copy, host/canonical digest separation | 07-01,10 | COVERED |
| RESEARCH | Source/object/body seals, exact inventory and cheap before long checks | 07-01–11 | COVERED |
| RESEARCH | Full inherited normal/instrumented/compiler/hosted and original evidence | 07-11 | COVERED |
| CONTEXT | D-01 real accepted tracer and reached wrong-pixel | 07-01,11 | COVERED |
| CONTEXT | D-02 current tools/no dependencies/background/limits | all plans | COVERED |
| CONTEXT | D-03 canonical incremental pending/fence ownership | 07-01–02,08 | COVERED |
| CONTEXT | D-04 reserve before effects/refusal/partial-list semantics | 07-01–10 | COVERED |
| CONTEXT | D-05 rational elapsed guest time/serialized phase | 07-01,09–10 | COVERED |
| CONTEXT | D-06 bounded lossless packets/atomic observer/drain independence | 07-01,10 | COVERED |
| CONTEXT | D-07 geometry/format/filter/field/time/profile/build metadata | 07-01,09–10 | COVERED |
| CONTEXT | D-08 approved concrete compatibility policy, no ABI freeze | all plans, especially07-01,09 | COVERED |
| CONTEXT | D-09 named deterministic policy/independent expectations/evidence labels | 07-01,03–11,13 | COVERED |
| CONTEXT | D-10 oracle/rights/physical/publication stops | all plans, especially07-12–13 | COVERED; VID-04 remains incomplete |

Deferred GPU/codecs/presentation/regions/ABI/controller/AI/save/replay scope is excluded
as directed, not silently omitted. No required Phase07 rendering mode is deferred.
No new SPEC/UI/AI spec, external API service, ORM schema push, package install or intel
hook is applicable. The active planner contribution fragments were read in their supplied
order: external API coverage, assumption delta, schema push and ASVS threat modeling.
The internal raw-output C seam is provisional machine/observer integration, not an
external service capability surface. Identity remains the approved composite profile;
variable geometry is a packet field, not a new per-device identity. ASVS1/high blocks
are implemented by the per-plan concrete threat register and qualification review.

## Planning Status

DRAFT remains intentional: nyquist_compliant=false and wave_0_complete=false.
Structural plan validation is not behavioral evidence. Native plans 07-01–11 are
ready for independent plan checking and implementation. Only actual scope authority
can release 07-13, and even authorized execution must still produce actual complete
differential evidence before VID-04 can close.
