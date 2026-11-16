---
phase: 06-cold-boot-cpu-timing-and-full-base-suite
plan: "07"
subsystem: original-evidence-validation
status: complete
completed: 2026-11-16
duration: 272min
requires:
  - phase: 06-06
    provides: Original C records and immutable execution archives
provides:
  - Independent full-source authority and archived C original-record reclassification
  - Closed public claims bound to original authored command/outcome/log and exact artifact bytes
  - Authored full-entry controls and local-only fully resealed actual-record mutation
requirements-completed: [BUS-03, CPU-08, VER-02]
affects: [06-08]
actuals:
  tokens: 27118
  tasks: 2
  commits: 3
coverage:
  - deliverable: Original-record and exact-artifact validator
    human_judgment: false
    verification:
      - {kind: ctest, ref: harness.base-evidence, status: pass}
      - {kind: command, ref: "python3 -B scripts/check-base-evidence.py --check-current", status: pass}
      - {kind: command, ref: "python3 -B scripts/check-base-evidence.py --check-artifacts", status: pass}
      - {kind: local-only-mutation, ref: build/gate/06-07-corrected-actual.log, status: pass}
  - deliverable: Bounded authored support claims
    human_judgment: false
    verification:
      - {kind: authored-controls, ref: scripts/tests/test_base_evidence.py#PublicEvidenceControls, status: pass}
      - {kind: claims, ref: docs/claims/phase06.md, status: pass}
---

# Phase 6 Plan 07: Original evidence and bounded claims

The checker authenticates the independently sealed CPU/RSP source universe before
passing retained original records and captures through each batch's exact archived
C classifier. C remains the result/completeness authority. Required and optional
contexts remain separate; private identities, counts and outcomes stay local.
The fully resealed reduction control accepts the original baseline, copies the
batch and exact snapshot, reseals every dependent identity after reduction, then
refuses at independent membership verification. Original archives are untouched.

The closed public receipt binds only the cleared authored cold-boot observations
to original CTest blocks, unique command/outcome records, their log digests, and
archived runner/test/ROM bytes. Profile hashes, terminal exceptions, retirement
tails and literal transcript come from those originals. Model/state fields absent
from the originals were removed; prose reports are not result authority. Claims
retain exact profile/build/fixture/oracle scope and fixed exclusions. No broad
compatibility, hardware timing, external diagnostic passing or release claim is
made. Integrity assumes the reviewed producer and trusted local owner; hashes
are not same-owner execution attestation or rights permission.

Saved artifact verification is independent of current live build drift. The
separate --check-preuse mode retains the live executable guard. Neither mode
starts a guest, rebuilds an image or fills a historical evidence gap. Original
Plan02 missing-executable history remains irrecoverable and disclosed.

## Verification

Both all-target presets built successfully. Focused harness.base-evidence passed
normally and under ASan/UBSan, with LastTest copies retained before discovery.
Exact inventories and removal/malformed controls passed. Authored tests pass from
clean temporary storage without private diagnostic directories. Original-record
and saved-artifact commands, claim validation and rights/PII tree scans pass.
The final full phase gate, compiler cross-check, complete source/security review
and hosted pair remain Plan08. No unchanged core fuzz was repeated for host-only
validator corrections. Evidence is under build/{gate,asan}/06-07-precheck/ and
build/{gate,asan}/06-07-corrected-*; raw host-specific captures remain ignored.

## Corrections and process deviations

The first symlink RED was invalid because a regex setup refusal masked the guard.
After proving the real root accepted, the corrected behavioral RED was classified;
the guard was then fixed. The checker/test were committed together rather than in
a separate RED-only commit; that deviation is retained, not rewritten.

Parent review rejected the initial completion claim: inventory tests had not
resealed context/binding/snapshot, public observations came from a report, and
later test indentation left public mutation functions undiscovered. Corrections
added independent derivation, actual full resealing, original-format authored
records and explicit test discovery. The original-log parser was also corrected
for unquoted CTest names, compact success output, combined startup fields, two
terminal policy records, architectural BREAK without a core stop shortcut, and
distinct A/B protocol observations. FIFO/path traversal guards are bounded.

A long correction handoff was stopped after its saved findings were reconciled;
the parent completed the narrow parser and tests inline. Lessons are recorded in
AUTONOMY.md. Actuals above use diff characters per four, not model billing.
Ready for Plan08 review/fixes and one coherent final gate; no user action needed.
