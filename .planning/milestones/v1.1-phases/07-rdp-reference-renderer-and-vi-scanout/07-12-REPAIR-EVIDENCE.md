# Phase07 Plan12 Authority Review Repairs

**Outcome:** all seven findings from the initial review are addressed in the prepared request and admission controls. Oracle authority remains blocked; VID-04 remains incomplete. No oracle repository was acquired, built, run, copied, or published.

## Corrections

| Finding | Repair |
| --- | --- |
| CR-01 | Both local and publication decision schemas carry `request_sha256`, and each validator requires it to equal the canonical digest of the entire request. Authored mutations to storage root, output cap, build command, and operation restrictions refresh the request source pin but retain the old decision; both local and publication admission reject each stale decision. Trusted production pins remain `None`. |
| WR-01 | The 216 items are explicitly suite-scoped native test-membership rows across 11 suites. Classification and mapping are `blocked`; exported comparison corpus and replay adapter are `missing`. Live execution requires all four readiness records to have pinned identities. No corpus exporter or replay adapter was built. |
| WR-02 | The request proposes 900-second acquisition, 1,800-second build, 60-second per-case, and 3,600-second aggregate wall caps. Proposed timeout cleanup sends SIGTERM to the whole isolated process group, waits five seconds, sends SIGKILL, verifies exit, records the timeout, and refuses without retry. These are proposals only; they create no live run window. |
| WR-03 | The proposed root is `${TMPDIR}/tendies64-oracle/phase07/<oracle-id>/<revision>/<run-id>/`. Admission resolves the current TMPDIR path for both pinned oracle/revision pairs without creating it and refuses paths inside the worktree, including existing symlinks. Authored path controls accept an external sibling and refuse an in-tree child and a symlink into the synthetic worktree. A real acquisition process must recheck its exact run root immediately before use. |
| WR-04 | Nested strings are scanned for embedded POSIX private paths, `file:` paths, and Windows private paths. Authored controls cover prose, nested URL fields, and list entries. The tree PII scan remains an independent check. |
| WR-05 | The positive authored control supplies a complete synthetic request, cleared synthetic rights audit, local decision, and short-lived test-only pin. It traverses `_require_local_approved` end-to-end under a scoped standard-library patch that restores the real source pins. The public CLI and production trust pins cannot accept this context. Expiry controls still test unchanged trusted local and publication bytes against a later clock. |
| WR-06 | The outer integration test compares the exact 22-control name set and count, including incomplete-rights and request-revision controls. It also exercises the real blocked CLI, forged local/publication grants, and symlink-resolved storage-root refusal. |

The standalone upstream `parallel-rdp` `COMMIT` revision in the request/evidence is 40 hexadecimal characters: `1cecd042b2619bc505c12bfdc713808386f2b54d`.

## Bounded verification

All Python commands used `python3 -B` with `PYTHONDONTWRITEBYTECODE=1`. Final command output and focused CTest artifacts are retained in ignored local build storage under `build/gate/07-12-repair/` and `build/asan/07-12-repair/`.

| Command | Result |
| --- | --- |
| `python3 -B scripts/check-rdp-oracle-authority.py --selftest` | Exit 0; exact 22 controls passed, including full-path positive admission, all request mutations, expiry controls, the proposed timeout contract, private paths, and path containment. |
| `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` | Exit 0; current request and rights audit are valid and still blocked. |
| `python3 -B scripts/tests/test_rdp_oracle_authority.py` | Exit 0; exact control set/count, real blocked CLI, forged local and publication grant refusals, and resolved symlink root control passed. |
| `python3 -B scripts/check-rdp-oracle-authority.py --require-approved` | Exit 2 as required; no competent local decision is source-pinned. Both production trust pins are unset. |
| `ctest --test-dir build/gate --no-tests=error --output-on-failure --output-junit 07-12-repair/post-root-change/normal-junit.xml --tests-regex '^harness\.rdp-oracle-authority$'` | Exit 0; normal CTest 1/1. JUnit: `build/gate/07-12-repair/post-root-change/normal-junit.xml`. |
| `ctest --test-dir build/asan --no-tests=error --output-on-failure --output-junit 07-12-repair/post-root-change/normal-junit.xml --tests-regex '^harness\.rdp-oracle-authority$'` | Exit 0; ASAN-configured CTest 1/1. CTest resolves its JUnit path under the selected build directory: `build/asan/07-12-repair/post-root-change/normal-junit.xml`. |
| `sh scripts/check-test-inventory.sh --preset gate` | Exit 0; exact inventory 158. |
| `sh scripts/check-test-inventory.sh --preset asan` | Exit 0; exact inventory 160. |

Before each CTest/discovery, its owning `LastTest.log` was copied and hash-checked. The final normal and ASAN JUnit XML and post-test `LastTest.log` copies are retained with SHA-256 records beside them. Inventory pre-discovery `LastTest.log` copies and command logs are retained under `build/gate/07-12-repair/post-root-change/inventory/`. The ASAN JUnit file is intentionally named `normal-junit.xml` because that was the exact unique argument passed to CTest; it is under the ASAN build tree and its execution passed.

Python AST parsing was used for syntax validation; bytecode was disabled. No full gate, full ASAN suite, GCC matrix, hosted CI, private guest, live oracle command, download, install, or new run window was used. These results establish fail-closed request admission only, not source rights, oracle agreement, broad renderer accuracy, or VID-04 completion.

## Bound request identity

- Canonical full request SHA-256: `ae787914b967cf102501ac17f9bec8f255fcc3b7deb074c4e618c60da2be3434`.
- Rights-audit SHA-256: `7506efecad2511fc2c535ed93b2f65b99dfc2e013508ba8ef760242251f1ff37`.
- Local decision: `blocked`, with no request digest, authority, evidence, or approved scope.
- Publication decision: `not-authorized`, separately pinned to no authority; no request digest or publication scope grant.
- Exported corpus, case mapping/classification, and replay adapter: not ready; no execution approval can pass.

## Remaining limits

Competent owner/legal authority and the outstanding per-file/build-input rights audit remain unresolved. The proposal does not authorize acquisition, build, execution, copying, publication, package installation, guest/boot assets, physical-console use, paid operations, or any new execution window. No user action is needed to preserve this stop. Any future scope change requires a new actual decision and a reviewed source pin for both the complete request and that decision.

## Final storage recheck

The independent recheck found two further storage warnings. The final helper
never chmods an existing path: it checks each build ancestor with lstat, refuses
symlinks/non-directories and requires an already private final directory. A
symlink and parent-symlink control verifies refusal without target side effects.
Storage proposals now require mode0700 and refuse a0777 mutation inside the
existing control inventory. The reviewer reran direct integration and marked all
findings resolved. Tree PII, rights, provenance and diff checks passed after
replacing literal authored paths in the uncommitted initial review with prose.
No public history or scanner waiver changed.
