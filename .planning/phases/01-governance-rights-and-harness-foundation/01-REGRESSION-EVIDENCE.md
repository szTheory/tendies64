# Phase 1 regression evidence after Phase 2

**Rechecked:** 2026-11-11T00:03:21Z
**Phase 1 baseline:** `41af2d5`
**Current hosted source:** `34b43f760e0858cdbc3a37e031058f176eea4073`

This is a bounded regression refresh of Phase 1's four previously verified
roadmap criteria. The historical initial measurements and provenance remain in
`01-VERIFICATION.md`.

## Changed gate inputs

The diff from the Phase 1 baseline changes only `scripts/gate.sh` and
`tools/lint/core-lint.sh` among the Phase 1 implementation inputs:

- `scripts/gate.sh` preserves the existing syntax, lint self-test, configure,
  build, CTest and core-lint steps. It adds inventory self-tests and an exact
  gate-inventory check between build and CTest.
- `tools/lint/core-lint.sh` keeps the source and object lint layers, extends the
  production source set to include `tools/common/t64_sha256.c` and `.h`, and
  adds a narrow include-resolution exception for that shared header. The
  default production scan excludes host-only `core/tests`; its self-test
  verifies a forbidden production clock call is still reported while the
  host-only test path is excluded.
- `git diff --check 41af2d5 -- scripts/gate.sh tools/lint/core-lint.sh` passed.
  The other checked Phase 1 inputs (ADRs, provenance/claims policies, CI
  workflow/checker, classifier and rights scanner) are unchanged from baseline.

## Regression evidence

| Phase 1 criterion | Current evidence |
|---|---|
| Governance, provenance and claims rules remain recorded | The Phase 1 ADRs, copy-provenance policy and claims template are unchanged from baseline. |
| One gate still runs in CI with the reviewed runner/action setup | `.github/workflows/ci.yml` and `scripts/check-ci-yml.sh` are unchanged. Hosted run [42925122280](https://github.com/szTheory/tendies64/actions/runs/42925122280), at the exact SHA above, passed on Linux/GCC and macOS/Clang. |
| Synthetic runner classification and inventory controls remain effective | `runner/t64_classify.c` is unchanged. Phase 2's exact inventory pins all 61 Phase 1 test names plus 12 Phase 2 names (73 gate tests); both hosted lanes passed the full gate. The inventory self-test also rejected each single-name removal and malformed inventory controls, as recorded in `02-VALIDATION.md`. |
| Rights, manifest and deterministic core checks remain in the gate | `scripts/rights-scan.sh` is unchanged. The two changed scripts retain all previous gate steps and extend the production lint set. The exact hosted run passed; final Phase 2 local evidence records 15 linted source files and zero violations. |

Phase 2's `02-VERIFICATION.md` and `02-VALIDATION.md` record the hosted run,
exact 73-name gate inventory, both passing lanes, and inventory removal
controls. `02-FINAL-LOCAL-EVIDENCE.md` records the 15-file source lint result.
These are corroborating records; this refresh did not rerun tests or CI.

## Fingerprint scope

The refreshed Phase 1 fingerprint covers its implementation inputs and this
phase-owned evidence snapshot. It no longer hashes the shared append-only
`docs/ci-reviews/LOG.md`: the exact Phase 2 CI row and run link are captured
here, while the original Phase 1 CI provenance remains in
`01-VERIFICATION.md`. This keeps unrelated future CI rows from changing the
Phase 1 evidence fingerprint.
