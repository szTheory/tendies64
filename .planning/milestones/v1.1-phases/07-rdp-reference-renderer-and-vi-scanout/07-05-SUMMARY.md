---
phase: 07-rdp-reference-renderer-and-vi-scanout
plan: "05"
subsystem: emulation
tags: [n64, rdp, texture, tmem, c17]
requires:
  - phase: 07-04
    provides: [bounded edge/shade raster and canonical logical coverage]
provides:
  - Versioned textured and shade-textured triangle pipeline with bounded sampling
  - Independent filter, format, coordinate, raw VI and deterministic-state cases
  - Exact texture route source/body/object ownership and mutation controls
affects: [phase-07, rdp, texture, profile-serialization]
key-files:
  created: [core/tests/test_rdp_texture.c]
  modified: [core/t64_rdp_raster.c, core/t64_rdp_tmem.c, core/t64_rdp.c, core/t64_internal.h, core/t64_machine.c, core/t64_digest.c, core/t64_isviewer.c, core/CMakeLists.txt, runner/t64_real.c, docs/spec/rdp.md, docs/spec/rdp-vi-cases.json, scripts/check-test-inventory.sh, scripts/check-rsp-no-hle.py, scripts/tests/test_rsp_no_hle.py]
requirements-completed: []
requirements-contributed: [VID-01]
status: complete
completed: 2026-11-17
commits: 3
plan_head_before: 603a44273dce3d2e96de0069174221074ef8549e
plan_head_after: 2a04371943a25f3e6ec8985bc77caa76ed08e631
---

# Plan07-05: Textured triangles

Textured and shade-textured triangles now use the ordinary scheduled raster
pipeline, with fetched LoadTile input and independently checked raw VI output.
This records implementation completion; independent source review and hosted
qualification follow before the next slice.

## Behavior and policy

The v4 composite profile adds affine/perspective interpolation, checked signed
coordinates/W policy, point/box/three-point sampling, tile addressing, LOD,
detail/sharpen and shade modulation. The seventeen-case matrix includes ten
supported format/size pairs, mirror boundaries and odd TMEM bank rows, negative
gradients, W-zero refusal/negative-W acceptance, fractional ties, prior-output
and full logical-coverage preservation on truncation, v3 refusal, and split/
interleaved digests. Packet-relative fields and independently calculated input
and output are pinned in [the raw contract](07-05-RAW-CONTRACT.md).

The profile is descriptor7/model13/state18. Retained full pending packet words
support resumable interpolation without new accumulators. Direct compatibility,
runner and digest plumbing beyond the original file list implements the already
approved policy; v3 keeps its identity/bytes/refusals. Numeric, filter-rounding,
LOD and provisional combiner behavior remain named software policy, not vendor
precision or hardware proof. The later combiner and coverage-memory plans still
own their full contracts.

## Commands and evidence

- Compiled intended RED reached raw0x0A after accepted setup, verified original
  word/refusal/current pointer and outside sentinel, and failed only expected
  pixel. Retained at `build/gate/07-05-red-{cases.log,texture-LastTest.log}`.
  Valid RED preceded production behavior. No stronger claim about every edit
  preceding the RED commit is made from the executor's qualified handoff.
- Commits: `905a80f` RED/registration; `83de02d` production/matrix;
  `2a04371` strict source/object controls.
- `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite texture
  --run`: seventeen discovered cases pass. ASAN/UBSan owning suite also passes.
  Affected normal and instrumented selections each pass10/10, including actual
  accepted runner fill, profile, ISViewer, digest, DPC and inherited RDP owners.
- `sh scripts/gate.sh` reaches its final `[gate] PASS`:149/149 CTests, fixture
  and inventory controls, final core lint91files/zero violations;48s total.
  `build/gate/07-05-full-gate.log` and `07-05-full-gate-LastTest.log` retain the
  complete wrapper output. The surrounding zsh capture then failed on reserved
  variable `status`; that wrapper-capture failure is not relabeled as exit0.
  All required gate stages are present, so a duplicate broad run is unnecessary.
- Pre-commit accepted normal/ASAN binaries and binding:
  `build/gate/07-05-retained-artifacts/binding.json`. Source_head predates the
  production commits; hashes bind the actual tested body. Parent verified all
  twenty-one source/artifact entries against current source and saved bytes.
- Post-commit annotation/accepted-owner check and exact normal consumers:
  `build/gate/07-05-final-commit-artifacts/normal/binding.json`, nineteen entries
  verified by parent. T64_BUILD_ID tracks content/compiler policy; HEAD belongs
  to separate T64_SOURCE_REVISION provenance. This check does not replace or
  relabel the original full-gate cohort. Full ASAN inventory remains unrun.
- Frame diagnostics: `build/asan/07-05-frame-warnings.log`. New bounds/product/
  row estimates are2432/2144/2880B, bounded/nonrecursive call paths. The large
  digest ASAN Debug frame already measured18592B in Plan04, versus current
  compile-only18784B; the initially cited10016B was an older Phase06 baseline
  and is withdrawn as a Plan04 comparison. Fixed per-function estimates are
  not runtime peak measurements or evidence of a large new texture buffer.
- Wave hooks: no schema drift, codebase drift skipped for absent STRUCTURE.md,
  no frontend/UI gate applicable. Protected owner-dirty files remain unchanged.

## Corrections and limits

Early small tests caught a short literal packet, extra post-packet word,
incorrect odd-bank test address operation and unequal-channel test texel. The
original failures remain; corrected tests reach the intended operations before
broad suites. The executor inspected the seal controls after first production
edits rather than before; it stopped edits, inspected exact inventories/body/
object rules, and retained strict mutation refusal. No reset or repeated RED
was needed. These chronology limits are explicit, not rewritten as ideal order.

No full VID requirement, hardware accuracy, external-renderer agreement or
release is claimed. D2/D5/D6 and all private-input/resource limits remain.
No user action is needed: parent owns source review, any fixes, hosted
qualification and the next approved plan.

## Independent review and repair

The [initial review](07-05-REVIEW-INITIAL.md) found two production defects and
two evidence gaps. Parent-added regressions reproduced27 failures before repair
(`build/gate/07-05-review-regressions-red.log`). W0 tile50:48 and max-level53:51
now govern preflight and relative LOD selection; the entire declared chain is
validated before effects and cannot extend past descriptor7. No tile0 fallback
or implicit extension from unrelated valid descriptors remains. Box/three-point
one-bit alpha now floors like RGB under the recorded software contract.

New controls cover only-named-tile validity, tile0 decoys, highest tile, declared
level caps and no-effect out-of-range chains; all16 alpha patterns for box,
diagonal tie and both triangle halves; independent 0x0A/0x0E all-row coefficient
literals with four expected pixels on a nonuniform tile; and exact arithmetic/
W-zero refusal preserving the full seeded framebuffer and coverage plane after
prior accepted work. Independent Fraction calculations also verify that zeroing
any of nine derivative lanes changes an expected sample.

The [focused recheck](07-05-REVIEW-RECHECK.md) resolves all four findings.
Final normal and ASAN inventories pass20registered cases; strict source/object
and mutation controls pass. Only five normalized body seals changed. Bounded
GCC13 compilation passes all eight affected C units under their target warning
policy; the final additional alpha test patterns also compile separately.
`sh scripts/gate.sh` passes149tests and final lint in48s on the final repair/test
cohort; its exit capture uses a task-specific variable. Earlier passing and
failed cohorts remain separate, including the prior surrounding-shell error.

Exact final consumers/logs and16source hashes are retained and verified in
`build/{gate,asan}/07-05-review-final-retained-artifacts/binding.json`. Capture is
post-verification worktree retention, with no retroactive pre-use claim. These
fixes implement the existing unshipped profile contract without new persistent
state or another version. Full ASAN inventory and hardware accuracy remain
unqualified. Hosted qualification is the next action; no user action is needed.

## Hosted qualification

Reviewed repair checkpoint `9a39ab4c0cd3c872ce37440ccf66543e58026cb1` passes
[both hosted jobs in run44054005627](https://github.com/szTheory/tendies64/actions/runs/44054005627).
Exact selected authored boot artifacts were collected with source/run/attempt/job
selectors and compared: guest identities, bounds, canonical state and transcript
match. This is boot evidence, not uploaded texture images or hardware evidence;
the native texture matrix passed within both gates. Records and logs:
`build/evidence/07-05-hosted-44054005627/`,
`build/gate/07-05-hosted-{collect,compare}.log`.

macOS job421s/Gate405s is critical; Linux job168s/Gate156s, queues3/2s.
The local final gate remains48s. Preserve the required jobs/limits; no cache,
dependency or concurrency change is justified from this cohort alone. Plan06's
focused combiner preflight is next. Full ASAN inventory/VID-04 remain open.
