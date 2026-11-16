# Compatibility CPU validation record

This record covers authored controls and the approved software policy. It
does not publish external diagnostic inputs, identities or results, or claim
hardware qualification. Plan16 completion is separate.

## Executed checks

- `sh scripts/gate.sh`: passed the complete native gate, all 111 registered
  tests, inventory controls and core lint. Total 125 seconds.
- All ASan targets built. The owning core/profile/legacy-determinism,
  diagnostic protocol/inventory/adapter, admission fuzz and sanitizer negative
  control selection passed (39 tests). The admission fuzz stayed within its
  existing 120-second bound. This establishes instrumented native behavior;
  it does not establish external-suite completion.
- After the review controls were added, rebuilt the affected normal and
  instrumented test targets. CPU integer/control/link checks passed in both
  configurations; the final production-runner controls passed in both normal
  and instrumented builds (14 authored routing controls plus the existing
  build/composition controls).
- After the final assertion correction, the all-target instrumented build and
  all three exact diagnostic CTests passed again. No production semantic
  source changed during these test-only follow-ups.
- The compatibility profile hash is independently fixed in `test_profile.c`.
  The independent v10 serializer agrees with the production serializer and
  fixed prepared-state vector. V10 CPU/output/origin sensitivity and v9
  exclusion of the new origin field are asserted. Existing strict/warm
  fixed vectors and refusal controls passed in the native gate.

## Review controls

The initial [review](04-19-REVIEW.md) found three missing regression controls,
with no confirmed runtime defect. Added nested likely-branch taken/annul cases,
read-only uncached conditional-store refusal atomicity, and a synthetic v2
compatibility loader/complete aggregation/cross-profile refusal fixture. The
last fixture uses real C runtime, inventory, encoding and aggregation calls;
only the separate upstream qualification verifier is stubbed. Its temporary
files are authored and independent of private diagnostic inputs.

The focused independent re-review closed the memory and routing controls and
requested a direct annulled-register assertion. Added that assertion and exact
retirement count; the final owning normal and ASan CPU-control runs passed.
All three findings are dispositioned. The [security audit](04-19-SECURITY.md)
returned SECURED for both added threats and bounded changed-mitigation checks.

## Failed attempts and evidence limits

- Initial profile lookup returned unknown-profile before implementation.
  Misplaced strict-profile assertions were a test-authoring error and moved
  to the compatibility test before accepted verification.
- Direct CPU test invocations without fixture environment failed setup and
  are not behavioral RED. Initial nested-branch checks also stopped at missing
  compatibility dispatch; only the later run, with a retired outer branch,
  establishes the inner-branch refusal. The sequential inner-slot hypothesis
  was replaced with the outer-target policy and a distinct-target regression.
- CP0 controls were added without an original preimplementation run. A later
  failure exposed an invalid supposedly valid PageMask test source; it was
  corrected to a defined low-word encoding and the owning test passed. This
  is postimplementation evidence, not reconstructed original RED.
- Adding explicit branch origin superseded the earlier uncommitted v10
  prepared-state golden. Production and independent reference agreed on the
  new digest; the obsolete fixed vector was updated and the native gate passed.
- The first loader control expected the structural C inventory validator to
  reject a different profile. The owning loader correctly rejected it at the
  actual runtime identity comparison instead. Corrected the expected boundary.
- The first instrumented loader control selected the normal adapter path.
  Its fixture now explicitly binds the owning instrumented artifact, following
  the existing substitution controls. The production path rule is unchanged.

Raw captures stay ignored under `build/`. No failed setup, incomplete attempt,
native control or mitigation audit substitutes for the actual Plan16 gate.
