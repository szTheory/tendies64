---
phase: 07-rdp-reference-renderer-and-vi-scanout
plan: "07"
subsystem: emulation
tags: [n64, rdp, blender, coverage, c17]
requires:
  - phase: 07-06
    provides: [native RGBA8 combining and deterministic noise]
provides:
  - Profile-bound blender and address-owned coverage
  - CPU/cache/PI/SP backing-write ownership and RGBA8888 raw VI
  - Independent fetched selector and complementary-edge controls
requirements-completed: []
requirements-contributed: [VID-01, VID-05]
status: complete
completed: 2026-11-17
plan_head_before: 4b71083420a41b54cf143514e4effa3652941f4e
plan_head_after: a6e1c5765e26251d9218172580e78cdd87eaada2
---

# Plan07-07: Blender and memory coverage

V6 (descriptor9/model15/state20) adds primary-table blender selectors/modes,
explicit integer software precision/dither policy and a bounded per-halfword
coverage sidecar. The prior logical sample plane remains separate. RGBA16
uses the sidecar without inferred physical hidden packing; RGBA8888 stores
the documented three-bit coverage code with an explicit low-alpha policy.
CPU scalar/masked/cache writeback, PI and both SP DMA routes invalidate only
after accepted backing writes. Fill/copy/raster author coverage, with reset,
admission, destruction and canonical digest ownership. Earlier profiles keep
their existing behavior and identity. Necessary profile/allocation/VI/header
wiring extends the plan's file estimate within its approved contract.

## Execution and review

[Execution evidence](07-07-EXECUTION-EVIDENCE.md) retains initial commands,
failures and scope. A compiled fetched packet reached raster/RDRAM/VI and
failed at its pixel assertion before blender behavior. Profile/registration
scaffolding preceded that observation; green task commits group the work,
so this does not claim a separate RED commit or all production edits afterward.
A fixture initially targeted15samples instead of5; it was corrected before
using coverage3 plus old6 to prove wrap1. Initial compiler/include, index,
expected-population and asymmetric-VI-digest errors remain retained.

[Independent review](07-07-REVIEW-INITIAL.md) found descriptor9 fell through
validation/serialization branches ending at8. A public-API regression against
the retained old library reproduced unchanged hashes/bytes and accepted
known-ID mutations for all11 omitted policy/boot fields, plus accepted empty
or unterminated policy strings. Explicit v9 branches and a VI policy naming
both source formats repair that identity contract. Final normal/instrumented
profile tests pass793checks; v3/v4/v5 hashes and serialized lengths match the
retained pre-repair artifact. The first reproduction exhausted its tracking
allocator after repeated erroneous acceptance; the corrected per-case allocator
reproduction independently exposes100failed assertions. Both are retained.

The [independent profile recheck](07-07-REVIEW-PROFILE-RECHECK.md) resolves the
blocker. The [orchestrator test-only recheck](07-07-REVIEW-TEST-RECHECK.md)
resolves two oracle gaps:32 fixed EF packets now exercise every selector/slot
in both cycles through actual raster/RDRAM/raw VI, and separate fresh instances
assert exact disjoint0x8cef/0x7310 masks before checking their accumulated union.
Independent integer calculations check all literal fields/RGB expectations.
The overview now lists five registered profiles; the initial review's suggested
six-profile count is withdrawn, while its actual omission is corrected.
[Current review](07-07-REVIEW.md) has no open finding.

## Final verification and identity

- Final registered10-case blender suite passes normal and ASAN/UBSan, including
  fetched16/32-bit output, all source selectors, modes/dither/alpha/coverage,
  aliases, sidecar bounds/lifecycle/digest and actual CPU/cache/PI/SP ownership.
- Required affected production owners passed16normal/14instrumented before
  the narrow identity/test repairs. Final repaired profile/blender owners pass;
  unchanged production owners were not repeated for test-only additions.
- Accepted runner, wrong-pixel and ISViewer controls pass after repair. Exact
  inventory151, strict79-source/object guard and48mutation controls pass.
- Read-only/network-disabled GCC13 source/test warning compiles pass, retaining
  the core-only frame limit only on core translation units. Explicit command,
  outcome and diagnostic records distinguish success from an empty log.
- Final `sh scripts/gate.sh` passes all151normal checks plus its required lint
  tail in64s. Original wrapper log, exit0 and LastTest are retained in
  `build/gate/07-07-review-final-full-gate*`. CTest34s, inventory selftest13s,
  lint8s dominate locally; no cache/dependency/concurrency change is justified.
  Full ASAN inventory remains unrun; broader phase qualification stays in07-11.

Initial source/artifacts were verified and retained with29entries before repair
rebuilds. Profile repair artifacts have a separate14-entry binding. Final
normal/ASAN bindings in `build/gate/07-07-review-final-retained-artifacts/`
verify52/26entries including21source files and exact consumers/logs. These
are post-verification worktree bindings; they are not retroactive pre-use
qualification. Source provenance in tested binaries names6a154a9; exact dirty
body hashes match this repair commit. Build identity is content-derived,
`t64-build-v1-283625562c2560c1b97a861a2ef4292a0d24392d4815efcef35cebd96726dc53`.
HEAD annotation changes do not justify semantic retesting.

Hosted qualification of the reviewed checkpoint is next before depth
implementation. The narrow depth primary preflight is saved independently.
No requirement is closed by this slice alone. D2/D5/D6 and expired windows
remain unchanged; no private/oracle/hardware use, VID04 or release is claimed.

## Hosted qualification

Reviewed checkpoint15cb0f6 passes [both hosted jobs](https://github.com/szTheory/tendies64/actions/runs/44119209396). Exact successful run/attempt1 selected authored boot records were collected and compared: guest identities, bounds, state and transcript match. Records: `build/evidence/07-07-hosted-44119209396/`; status/collection/comparison/metrics: `build/gate/07-07-hosted-*`. Native RDP assertions execute in each gate; the uploaded boot records do not establish RDP hardware accuracy. Depth implementation may now proceed under the checked primary/software contract. Rights/oracle/release stops remain unchanged.
