---
phase: 07-rdp-reference-renderer-and-vi-scanout
plan: "08"
subsystem: emulation
tags: [n64, rdp, depth, c17]
requires:
  - phase: 07-07
    provides: [blending and address-owned coverage]
provides:
  - Versioned visible/hidden depth with primary compression fields
  - Four depth modes and complete Z triangle transport
  - Independent coefficient, alias, refusal and recovery controls
requirements-completed: []
requirements-contributed: [VID-01]
status: complete
completed: 2026-11-17
plan_head_before: 084ccc4ef4056e79ec81ce7114d6d28e618ed862
plan_head_after: d64ccdd6b81f9c8bdbc47dde71335da0646cbb5e
---

# Plan07-08: Depth mapping and comparison

V7 descriptor10/model16/state21 adds an identity-bound depth policy, aligned
Z image, per-halfword hidden metadata, four mode rules, compare/update gates,
primary8-row compression and Z-enabled triangle paths. Primitive conversion,
Q16 stepping, delta priority/zero, tolerance, mode precedence and logical
hidden addressing remain explicit software choices, not hardware proof.
Existing registered profiles preserve their identity and behavior. Required
header/profile/allocation/digest/write-hook wiring extends the estimated file
list within the approved behavior; no API/ABI freeze is implied.

## Execution, findings and repairs

[Execution evidence](07-08-EXECUTION-EVIDENCE.md) records initial checks,
commands, failures and retained identity. The initial setup refusal was not
RED. With accepted scaffolding, the fetched triangle produced correct green
color and failed only visible/hidden depth; actual depth behavior followed.
Scaffolding preceded RED and source history is not relabeled otherwise.

[Independent review](07-08-REVIEW-INITIAL.md) found coverage-only output using
a backing-write invalidator, erasing aliased hidden depth despite no RAM write.
A reached same-address packet reproduced83→00 hidden state; its next fragment
then failed a depth comparison that required the preserved delta tolerance.
Calling invalidation only when color bytes are written repairs it. The
[focused independent recheck](07-08-REVIEW-ALIAS-RECHECK.md) resolves that defect.

Test repair exposed [a second production defect](07-08-REVIEW-ADDITIONAL.md):
both texture preflight and sampling selectedW4 rather thanW12 for0f. The old
fixture mirrored that group order, and primitive-only combining hid the error.
Both helpers now useW12 for0e/0f, with old0a/0b and unsupported-profile behavior
preserved. The [independent offset recheck](07-08-REVIEW-OFFSET-RECHECK.md)
resolves it. Only three exact reviewed helper/body seals changed.

The [orchestrator test-only recheck](07-08-REVIEW-TEST-RECHECK.md) closes the
three warnings. Literal packet lengths no longer depend on the production
length helper. A correct0f texture×shade packet uses a nonuniform tile and
expects0x8231 plus rawVI(132,66,198,255). Paired base/Dx/De/Dy signed/fractional
vectors each survive compression and change a visible or hidden result;
independent integer arithmetic checks all8 vectors. Compare/update0x04 completes
the four combinations. All52 incomplete prefixes across09/0b/0d/0f follow
accepted work, preserve color/depth/coverage/masks/RNG and permit a real valid
same-instance recovery. The bad image span includes an in-bounds first row;
seeded RAM-tail/hidden/coverage snapshots catch partial prefix writes.

Fresh strengthened tests against the retained old core fail10 assertions at
0f RDRAM/VI and alias metadata/future-color results. Current source passes.
The first integrated repair's color expectations accidentally used the depth
word;214 failures are retained, then corrected to green0x07c1. GCC also caught
fallible setup followed by uninitialized result reads; explicit initialization
and reached guards fix the test failure path. Initial parent probe include/path
mistakes remain setup failures, not RED/GREEN evidence. No hardware inference
follows from these software regressions.

## Final qualification

Final12-case depth suite passes normal and ASAN/UBSan. Ten affected final
instrumented owners pass with a unique aggregate JUnit file, LastTest and exit0;
unchanged SP/DMA cohorts reuse their prior focused evidence. Core/test GCC13
warnings-as-errors compiles pass with core frame limits only on core units.
Accepted runner/wrong-pixel/profile/ISViewer consumers, exact inventory152,
strict source/object80 and50mutation controls pass.

Final `sh scripts/gate.sh` passes all152normal tests plus required lint in58s;
original wrapper log, exit0 and LastTest are retained under
`build/gate/07-08-review-final-full-gate*`. Local stages: CTest31s, inventory
selftest12s, lint7s. No new cache/dependency/concurrency change is justified.
Full ASAN inventory remains unrun; full phase qualification stays in07-11.

The original91-entry archive was verified against retained and then-current
files before repairs. Its normal aggregate LastTest was overwritten and early
profile-label compiler output was not saved; those historical limits remain.
The final integrated gate supplies new evidence, not a reconstructed old pass.
Final normal/ASAN bindings under `build/gate/07-08-review-final-retained-artifacts/`
verify44/29entries including17source files and exact consumers/logs. They are
post-verification worktree bindings. Tested core build IDs are
`t64-build-v1-264e7c8c30efafbc0b0fb05671a65e25b52abbc372c99ce0c7bb28320b24a752`
and instrumented
`t64-build-v1-4a5ff44a03603e03955f91578876de565f4639eb234eb27fc041bc128a568875`.
Exact body hashes bind this repair; HEAD annotation changes alone do not require
semantic retests. [Current review](07-08-REVIEW.md) has no open finding.

Hosted qualification is next before VI implementation. The VI primary preflight
is saved, with clock-unit/latch/geometry gaps explicitly separated from source
facts. No requirement is closed by this slice alone. D2/D5/D6, expired windows,
private/oracle/hardware and release stops remain; VID04 is unqualified.

## Hosted qualification

Reviewed checkpoint4559440 passes [both hosted jobs](https://github.com/szTheory/tendies64/actions/runs/44146970240). Exact successful run/attempt1 selected authored boot records were collected and compared: guest identities, bounds, state and transcript match. Records: `build/evidence/07-08-hosted-44146970240/`; status/collection/comparison/metrics: `build/gate/07-08-hosted-*`. Native RDP assertions execute in each gate; selected boot uploads are not an independent RDP hardware oracle. VI implementation may proceed under its checked primary/software contract. All existing stops remain unchanged.
