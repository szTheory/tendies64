---
phase: 05-rsp-low-level-emulation
plan: "03"
subsystem: rsp-private-qualification
requires:
  - phase: 05-02
    provides: Distinct functional RSP profile and SP control/DMA reference.
provides:
  - Separate source/value membership and pre-use RSP closure.
  - Actual bounded offline build and inspected private ELF/payload/ROM.
  - Authored qualification/build controls independent of private inputs.
affects: [05-04, rsp-local-evidence]
plan_head_before: 1684cc48712e9446a85dd6a08df1a3af80431a5d
plan_head_after: d37aa274e5f7c019a9a2814083d392b2f975341f
actuals: {tasks: 2, commits: 3}
requirements-completed: []
completed: 2026-11-14
status: complete
---

# Plan03 — Qualified RSP component build

The RSP source selection has its own exact registration/value partition,
default feature/level semantics, overlay and pre-use receipt. Repeated names,
aliases and zero-value registrations retain their source identities. Existing
CPU receipts are reused with digest/context checks; CPU schemas and membership
remain unchanged. No upstream identity, denominator, asset detail or test
result is published by this summary.

The selected headless default closure excludes unqualified nondefault
bitmap-dependent modules with explicit reasons while preserving every default
member. Unresolved asset rights remain a stop for those modules; no rights
were inferred from a repository license alone.

After pre-use closure passed, an actual offline/locked/frozen build ran under
the existing resource limits. Produced ELF/payload/selector/boot/ROM composition
was independently checked. Failed inspector setup and an incorrect section
flag assumption were preserved; existing outputs were inspected without
recompiling. No ROM or guest execution occurred.

## Verification and corrections

- All-target normal and instrumented builds pass. Final owning CTest selections
  pass2/2 in each preset, using the approved four-job policy and sanitizer
  environment. Exact inventories are115/117 with only the two planned
  harness registrations added. Original CPU registrations remain intact.
- Final authored qualification/build controls pass11 and2 tests, including
  omitted/re-digested membership, duplicate/cross-component/open schema,
  source/tool/lock/asset drift, unbound executable refusal before launch and
  mandatory current-output recovery inspection.
- Parent private pre-use verification and fresh inspection of the existing
  actual artifacts exit0. No Cargo or guest execution was repeated.
- Docs, diff, rights and PII checks pass. Public evidence is generic; detailed
  source-derived records and raw logs remain owner-only under ignored build.
- The absent-CLI first test was invalid setup evidence despite formal classifier
  acceptance. Later omission controls are corrective regression evidence;
  the initial sequencing deviation is retained, not rewritten as original RED.

[Review evidence](05-03-review-evidence.md) records parent fixes: a changed
identity reader must be bound before invocation; recovery must actually inspect
current outputs and retain real command records. An isolated prior-commit
control reproduced the recovery defect. Configured full phase code/security
reviews remain Plan10 work; this summary does not claim those gates ran.

Saved final native logs: build/gate/05-03-review-ctest.log and
build/asan/05-03-review-ctest.log; Python controls and exact inventories have
matching05-03-review logs. Private receipts include qualification.json,
rsp-preuse-closure.json, qualified-build.json, the original actual attempt,
current-core-identity-binding-v2.json and parent-review-composition-command.json
under build/private-rsp-diagnostic. These records preserve original and current
identities separately; metadata annotation changes do not relabel old artifacts.

Hosted run43534252172 passed both platform gates on exact source d37aa27,
including all115 checks, lint84/0, authored evidence and publication scans.
The two harness tests use clean authored fixtures; they do not use private
inputs or establish external RSP execution. F-065 in docs/ci-reviews/LOG.md
records measured costs.

## Next action and limitations

Plan04 extracts shared reporting primitives with canonical CPU equivalence
controls, builds the separate RSP execution adapter, and attempts one actual
bounded member before broad ISA expansion. Build/composition evidence is not
startup, instruction correctness, all-suite success or hardware accuracy.
The original diagnostic compiler output is reusable when its input closure
is unchanged; ordinary native rebuilds do not justify another Cargo build.

The existing CPU executable serves only as an identity reader here. Plan04
must bind its actual execution adapter separately, preserve exact hashes and
receipt lineage, and distinguish the scoped private profile hash from the
runtime descriptor. Approved reporting extraction may require a deliberate
tested artifact binding; do not trust replacements through self-report alone
or turn metadata annotation changes into repeated build/rebinding loops.

The historical identity-reader executable and its binding were archived
privately before the next native rebuild, preserving exact tested bytes.
No user action is needed. Parent continues to Plan04.
