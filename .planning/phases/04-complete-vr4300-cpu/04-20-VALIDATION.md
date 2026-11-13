# Compatibility v2 validation scope

Status: local implementation, required checks and review dispositions pass;
hosted verification and Plan16 completion remain separate gates.
No external diagnostic input, name, count, identity or outcome is published.

- Canonical v2 admission and prepared-state checks pass. Profile digest is
  `1086516cd466bf48133533a8edf33a12cc8fd9e27dcd2b493e92b2bcf21a7634`.
  The independent state11 serializer agrees with production and fixed vector
  `f943e44778ae482f12798f681335608b0c57812874874b84583674495e70ce2e`.
  COP2 latch sensitivity applies only to v11; published v1 and strict vectors
  and behavior remain covered.
- The owning 12-test core selection reached the intended preimplementation
  failures. After policy implementation, ten passed; two remaining fixtures
  fetched/referenced the wrong target. Corrected full-PC replay and discovery
  after admission; the two owning reruns pass with original expected values.
  See [RED evidence](04-20-RED-evidence.md) for exact provenance and limits.
- The complete native gate passes, including all111 registered checks,
  inventory controls and core lint. All instrumented targets build. The
  instrumented owning selection passes all39 checks serially after the normal
  gate, including admission fuzz and the sanitizer negative control, within
  existing limits. Command records establish successful exits, no deadline
  expiry and empty process groups. After the review's conversion destination
  correction, complete normal/instrumented target builds and the six owning
  COP1/three-diagnostic checks pass in each configuration. Earlier broad
  evidence retains its pre-correction identity; unchanged semantic checks are
  reused without relabeling the old artifact.
- The configured source reviewer resolves its one conversion destination
  finding after a meaningful regression and narrow follow-up. The security
  auditor returns SECURED for all three registered threats and confirms the
  correction addendum. No user risk acceptance is inferred.
- Actual local use revalidates existing qualified source/tool/asset/image
  inputs, binds the current v2 profile and executable, and keeps the original
  source-derived membership and all execution caps. Bounded targeted records
  stay private and are explicitly insufficient for whole-set completion.
  The shared C aggregator remains authoritative; no native control or targeted
  denominator replaces its complete-set gate.

The native command sequence is recorded privately through the existing
process-group-bounded command helper. It runs the normal gate, all-target
instrumented build and owning core/profile/legacy/protocol/adapter/fuzz/
sanitizer controls without overlapping a complete actual diagnostic run.
Raw output remains ignored; final source/build identities and results are
recorded after the sequence finishes.

## Fixture corrections after implementation

Conversion result checks initially ran the next instruction because the
replay helper reset only low PC. Provenance assertions exposed the wrong
full PC/opcode. The helper now resets full/low PC, clears stale execution
state and invalidates the rewritten I-cache line. Original arithmetic
expectations pass; no conversion policy was changed to satisfy that fixture.

The zero-data fixture discovered an address before admission initialized
device policy. That selected the absent-RDRAM probe later excluded by the
correct production predicate. Discovering after admission fixes the fixture;
the initial lost-page-offset inference was withdrawn. Neither the approved
zero-data windows nor the expected results were widened.
