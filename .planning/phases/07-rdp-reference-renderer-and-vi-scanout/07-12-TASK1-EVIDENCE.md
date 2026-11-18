# Phase07 Plan12 Task1 Evidence

**Task outcome:** bounded authority request prepared; live oracle authority remains blocked. VID-04 remains incomplete. No oracle code/assets were acquired, built, run, copied, or published.

## Prepared request and authority boundary

- Canonical request: `docs/evidence/phase07-oracle-authority.json`; readable scope and rights findings: `docs/evidence/phase07-oracle-authority.md`.
- Corpus identity binds 11 suites and 216 suite-scoped case rows. Repeated binary/case names across suites are valid distinct rows; the verifier’s uniqueness key is `(suite, binary path, case name)`.
- Angrylion-Plus is pinned to the `v1.6` lightweight tag’s commit `3744ec766ed8d2c15fd5cbb4ac3c241b852b6e69`. Its exact README recipe is recorded as proposed, not authorized. The selected revision's MAME license notice requires an owner/legal decision; implementation and generated build inputs remain unaudited.
- The second repository is specifically `Themaister/parallel-rdp-standalone` at `388d70f5835b352d841d9d9e5a08c5de01470f41`, distinct from its upstream `parallel-rdp` library pointer `1cecd042b2619bc505c12bfdc713808386f2b54`. Its metadata tree is complete (198 entries), but identifies no self-contained executable/build entrypoint. Per-file, generated-shader, test/data and transitive build rights remain blocked. The root MIT classification is not treated as per-file clearance.
- Commit and tree identity used patch-free Git database metadata endpoints plus exact `git ls-remote` URLs. No patch-bearing commit endpoint, implementation source, header, patch or diff was fetched, stored or exposed.
- Proposed storage/output caps and private-only retention are documented in the JSON. Publication is separately unauthorized and cannot ride a local-use decision.

## Actual controls

Final focused controls ran with `python3 -B` and `PYTHONDONTWRITEBYTECODE=1`:

| Command | Result |
| --- | --- |
| `python3 -B scripts/check-rdp-oracle-authority.py --selftest` | Exit 0; 18 controls passed, including blocked/missing/duplicate, partial scope, both revision/identity and expiry paths, unchanged trusted bytes after expiry, altered bytes against an existing pin, synthetic/live separation, and publication-specific scope/expiry/pin controls. |
| `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` | Exit 0; current versioned no-authority record accepted as blocked. |
| `python3 -B scripts/check-rdp-oracle-authority.py --require-approved` | Exit 2 by design; no competent decision is pinned, so oracle operations remain blocked. |
| `python3 -B scripts/tests/test_rdp_oracle_authority.py` | Exit 0; actual CLI refuses a self-declared, correctly scoped local grant and a self-declared publication grant against a complete temporary mirror of the authored corpus. |
| `ctest --test-dir build/gate --no-tests=error --output-on-failure --output-junit build/gate/07-12-task1/final-ctest/normal-junit.xml --tests-regex '^harness\.rdp-oracle-authority$'` | Normal focused CTest passed 1/1; JUnit and owning `LastTest.log` are retained under ignored `build/gate/07-12-task1/final-ctest/`. |
| `ctest --test-dir build/asan --no-tests=error --output-on-failure --output-junit build/gate/07-12-task1/final-ctest/asan-junit.xml --tests-regex '^harness\.rdp-oracle-authority$'` | ASAN focused CTest passed 1/1; JUnit and owning `LastTest.log` are retained beside the normal result. |
| `sh scripts/check-test-inventory.sh --preset gate` / `--preset asan` | Exact inventory passed with 158 normal and 160 ASAN entries. Inventory self-test also passed. |
| Python AST and shell syntax checks | Passed with Python bytecode disabled. |
| `sh scripts/rights-scan.sh --tree` | Passed; zero findings. |
| `sh scripts/pii-scan.sh --tree` | Passed; zero findings. |
| `sh scripts/check-provenance.sh` | Passed; zero errors. |

CTest resolves a relative `--output-junit` path under its build directory. The actual XMLs were byte-compared with retained copies in `build/gate/07-12-task1/final-ctest/`; exact argv, resolved paths, test counts, SHA-256 and `LastTest.log` hashes are in `build/gate/07-12-task1/final-ctest/command-bindings.json`. The focused CTests are the only CTest executions for this task. The full gate, full ASAN suite, GCC matrix, hosted CI and all oracle operations were not run. Before each CTest/discovery run, the prior owning `LastTest.log` was copied and hash-checked in the private retained evidence directory.

## Failed bring-up attempts and TDD classification

- The first test invocation occurred before the checker file existed and failed at CLI setup. That is an invalid/setup RED, not evidence of semantic rejection; the original command/process/stdout/stderr records are retained in `build/gate/07-12-task1/red-test-before-checker/` and are not counted as a passing control.
- An early verifier attempt rejected valid repeated case labels because it keyed uniqueness without suite identity. The authored denominator confirms 216 rows across 11 suites; the verifier now keys rows by suite, binary and case.
- An early positive self-test did not mark the proposed build recipes ready inside its test-only record. The synthetic-only control now sets both recipes ready, while the actual request stays blocked and neither CLI nor test harness can turn that control into live authority.

## Retained outputs

Final self-test and actual CLI command logs and JSON source bindings are in `build/gate/07-12-task1/final-controls-current/`. Earlier inventory command logs are in `build/gate/07-12-task1/final-controls/`. Focused normal/ASAN JUnit, `LastTest.log`, and pre-discovery copies are in `build/gate/07-12-task1/final-ctest/`. These are local review records; they do not contain oracle output or a publication artifact.
