---
phase: 07-rdp-reference-renderer-and-vi-scanout
plan: "06"
subsystem: emulation
tags: [n64, rdp, combiner, rgba8, c17]
requires:
  - phase: 07-05
    provides: [bounded textured triangles and logical coverage]
provides:
  - One/two-cycle versioned color/alpha combiner and native RGBA8 texture route
  - Independent raw consumer, selector, feedback, noise and refusal controls
  - Exact pipeline source/body/object ownership and mutation controls
affects: [phase-07, rdp, policy-identity, profile-serialization]
key-files:
  created: [core/t64_rdp_pipeline.c, core/tests/test_rdp_combiner.c]
  modified: [core/CMakeLists.txt, core/include/tendies64/t64_machine.h, core/t64_internal.h, core/t64_machine.c, core/t64_digest.c, core/t64_isviewer.c, core/t64_rdp.c, core/t64_rdp_raster.c, core/t64_rdp_tmem.c, runner/t64_real.c, docs/spec/rdp.md, docs/spec/rdp-vi-cases.json, scripts/check-test-inventory.sh, scripts/check-rsp-no-hle.py, scripts/tests/test_rsp_no_hle.py]
requirements-completed: []
requirements-contributed: [VID-01, VID-05]
status: complete
completed: 2026-11-17
commits: 4
plan_head_before: 28cc5b192b2cdcd4dc12f36b0267b25e7e64c707
plan_head_after: 29bc425089f39b7f413ca599df9a5a36add4ae54
---

# Plan07-06: Color and alpha combining

The new v5 profile routes scheduled triangle pixels through a color/alpha
combiner, retaining native RGBA8 texture inputs. Copy/fill retain bypass paths.
This records implementation completion; independent review and hosted
qualification follow before the next slice.

## Contract and ownership

Descriptor8/model14/state19 binds the new combiner policy. Raw selector widths,
zero aliases and two-cycle feedback come from the primary tables in
[the checked raw contract](07-06-RAW-CONTRACT.md). Unsigned RGBA8 sources,
denominator255, signed floor division, per-cycle clamp and cycle0 evaluation
for unequal one-cycle groups remain explicit provisional software behavior.
Noise advances seed1 xorshift32 once per accepted covered pixel; its low byte
is shared across noise channels and both cycles. Refusals consume no noise.
No hardware precision/cadence claim follows.

Combiner registers, key data, feedback and noise are per instance and participate
in the v19 digest/policy identity. Necessary native-channel TMEM access, public
profile serialization and runner/ISViewer wiring extend the plan's estimated
file list without a new scope/approval boundary. Previous profiles retain their
identities, bytes and refusal behavior.

## Verification and retained evidence

- Valid compiled RED used raw SetCombine `0xFC55566C8877FFFF` and accepted
  primitive/environment commands through DPC and an edge triangle. Old RDRAM
  `0xCE73`/VI206-gray differed from expected `0x5295`/VI82-gray, with no setup
  refusal. Classifier `RED_EVIDENCE_OK` is saved in
  `build/gate/07-06-tdd-red-evidence.json`. Profile/register acceptance scaffold
  existed before RED; actual combining behavior followed. The RED commit is
  test/registration only; chronology is not relabeled as all production edits
  occurring afterward.
- Eleven registered combiner cases pass normal and focused ASAN/UBSan runs,
  covering literal arithmetic/selector controls, feedback, exact noise,
  interleaving, refusal preservation and policy/digest effects. Accepted fill
  and comparator controls, DPC, triangle/texture and profile/ISViewer owners pass.
- Strict guard passes79source/object edges and37mutation controls, including
  injected combiner effects and a missing pipeline object.
- Actual `sh scripts/gate.sh` passes150/150 CTests, fixture/inventory/self-test
  stages and final core lint92files/zero violations in49s. Full ASAN inventory
  was not run. Exact full-gate artifacts are at
  `build/gate/07-06-final-retained-artifacts/binding.json`.
- A final test-only addition fetches literal SetKeyR/GB and SetPrimColor and
  checks decoded values plus key/primitive-LOD selector effects. Normal suite
  and focused ASAN pass; production and guards are unchanged, so broad checks
  were not repeated. Its missing-fixture and misplaced literal-byte failures
  remain saved. Exact additional consumers/logs are at
  `build/gate/07-06-key-command-retained/binding.json`.
- Parent verified44full-gate artifact/source entries. The full-gate test source
  maps exactly to commit4adc30b; the later test-only source maps to29bc425.
  Parent verified13additional focused binding entries. Earlier source_revision
  annotations are preserved alongside exact source hashes, with no retroactive
  pre-use or newest-binary claim.
- New ASAN frame estimates: RGB selector2496B and native RGBA8 reader2624B;
  fixed nonrecursive helper paths, no large local buffers. These are compile
  estimates, not runtime peak measurements.
- Wave hooks show no schema drift; codebase check skips for absent STRUCTURE.md;
  no frontend/UI gate applies. Protected owner-dirty files remain unchanged.

## Commits and next action

`cb7575c` RED; `85588f9` production; `4adc30b` tests/guards;
`29bc425` focused fetched-register test. Original failed and successful cohorts
remain separate. The raw preflight's initial inconsistent Primitive D arithmetic
was mechanically corrected before implementation and its draft retained.

No full VID requirement, oracle agreement, hardware accuracy or release is
claimed. D2/D5/D6 and exact private/resource stops persist. Parent owns source
review, fixes and hosted qualification; no user action is needed.

## Review repairs and final local qualification

The [initial review](07-06-REVIEW-INITIAL.md) found the v5 missing-SetCombine
legacy bypass and four evidence/policy gaps. Parent source fixes require a
latched SetCombine before triangle scheduling and prevent v5 legacy fallback;
missing mode refuses with RENDER_RANGE and no own pixels, coverage or RNG
consumption. Prior fill/copy work remains. The existing refusal helper clears
pending/busy state, so subsequent DP_START remains supported; the provisional
static lockup concern was withdrawn after tracing that helper.

A separate [primary recheck](07-06-REVIEW-ADDITIONAL.md) found primitive LOD
fraction decoded from raw47:40 instead of39:32. The earlier test had been moved
to mirror that incorrect decoder; its green fetched-LOD claim is withdrawn.
SDK producer fields and literal FA0012A7636E7984 distinguish min18 from fraction167.
The corrected source and regression now consume the fraction byte. This source
fact is not an authorized software-precision substitution.

The delegated repair owned only the test file and case inventory while the
parent owned stable source/docs/seals. Sixteen registered cases now include all
cycle1 RGB/alpha slots and zero aliases, real two-tile RGBA32→combiner→RDRAM/VI
with a quantization discriminator, real noisy raster/clip cadence with one byte
shared across cycles, fetched signed K4/K5 boundaries and corrected primitive
LOD, and missing-mode atomicity after prior fill. K4/K5 signed9-to-byte clamping
is explicit reversible policy. Both code defects reproduce against the saved
pre-fix archive (four failing cases are consequences of those two defects).

All sixteen normal and ASAN/UBSan cases pass. Exact source/object guard79edges
and37mutations pass; only two source-body seals changed. All eight affected
production C units and final test compile pass the bounded GCC13 check.
Final `sh scripts/gate.sh` passes150tests and final lint in64s on repaired source.
Full ASAN inventory remains unrun. Initial fixture-clamp and VI131-vs132 errors
are retained; the parent also corrected a stale explanatory alpha comment to
cycle0=255/cycle1=190 without changing the asserted0x73e1 pixel. Independent
arithmetic confirms that native-input oracle.

Subtask evidence is retained under `build/gate/07-06-subtask-combiner-artifacts/`;
parent verified31source/artifact entries before preserving its exact test source.
Final normal and focused instrumented consumers/logs plus18source hashes are
retained under `build/gate/07-06-review-final-retained-artifacts/{normal,asan}/`.
The independent [recheck](07-06-REVIEW-RECHECK.md) resolves all six findings.
Capture labels remain post-verification worktree bindings, not retroactive
pre-use qualification. Fixes implement the existing unshipped v5 contract;
no extra profile version is minted per correction. Hosted qualification is next,
with no user action needed and all rights/oracle/release stops preserved.

## Hosted qualification

Reviewed checkpoint `2e0c170` passes [both hosted jobs](https://github.com/szTheory/tendies64/actions/runs/44090997512). Exact successful run/attempt1 selected authored boot records were collected and compared: guest identities, bounds, state and transcript match. Retained records: `build/evidence/07-06-hosted-44090997512/`; status/collection/comparison logs: `build/gate/07-06-hosted-*`. Native RDP assertions run in each gate; the uploaded boot records do not independently establish RDP hardware accuracy. macOS job389s/Gate373s is critical; Linux job162s/Gate156s, queues3/2s. Both hosts pass150 native checks and required gate/publication tail. Plan07 is ready after its narrow preflight; no private/oracle/release authority changes.
