---
phase: 04
review: 04-REVIEW.md
titles: json
findings:
  - id: CR-01
    severity: critical
    disposition: fixed
    title: "Receipt does not bind the executables that the gate runs"
  - id: CR-02
    severity: critical
    disposition: fixed
    title: "A symlinked build parent can move the private trust root outside the repository"
  - id: CR-03
    severity: critical
    disposition: fixed
    title: "Git-sourced Cargo packages are omitted from the audited dependency set"
  - id: WR-01
    severity: warning
    disposition: fixed
    title: "The six refusal controls exit before writing or checking any receipt"
  - id: WR-02
    severity: warning
    disposition: fixed
    title: "The synthetic receipt's Rust standard-library archive name disagrees with the verifier"
  - id: CR-04
    severity: critical
    disposition: fixed
    title: "Cargo path packages are omitted from the reviewed dependency set"
  - id: CR-05
    severity: critical
    disposition: fixed
    title: "Overlay targets can rewrite Git metadata"
  - id: WR-03
    severity: warning
    disposition: fixed
    title: "A later file-write failure leaves earlier overlay files replaced"
  - id: WR-04
    severity: warning
    disposition: fixed
    title: "Boolean and floating schema versions pass the integer version check"
  - id: WR-05
    severity: warning
    disposition: fixed
    title: "Failed rollback deletes the original recovery copy"
  - id: WR-06
    severity: warning
    disposition: fixed
    title: "A mode-setting failure still discards a complete recovery copy"
  - id: WR-07
    severity: warning
    disposition: fixed
    title: "Entry and park sentinels are not pinned by the linker"
  - id: WR-08
    severity: warning
    disposition: fixed
    title: "BSS is zeroed before the heap and stack boundary is checked"
  - id: CR-06
    severity: critical
    disposition: fixed
    title: "Pre-clear guard compares different MIPS64 address representations"
  - id: CR-07
    severity: critical
    disposition: fixed
    title: "Composition report depends on an uncommitted qualifier return field"
  - id: CR-08
    severity: critical
    disposition: fixed
    title: "Invoked objdump identity is not compared to the approved receipt"
  - id: CR-09
    severity: critical
    disposition: fixed
    title: "Source preparation accepts redirected or exposed private roots"
  - id: WR-09
    severity: warning
    disposition: fixed
    title: "Unit-graph input paths can traverse outside private storage"
  - id: WR-10
    severity: warning
    disposition: fixed
    title: "Source-closure stdout exposes private derived fields"
  - id: WR-11
    severity: warning
    disposition: fixed
    title: "Boolean values pass unit-graph integer checks"
  - id: WR-12
    severity: warning
    disposition: fixed
    title: "Reused overlay checkout is not checked against manifest-derived bytes"
  - id: CR-10
    severity: critical
    disposition: fixed
    title: "Production adapter is not bound to the qualified build"
  - id: WR-13
    severity: warning
    disposition: fixed
    title: "Successful build commands can leave descendants running"
  - id: WR-14
    severity: warning
    disposition: fixed
    title: "A zero exit during cleanup can hide an expired deadline"
  - id: P19-WR-01
    severity: warning
    disposition: fixed
    title: "Nested likely-branch annul path has no regression control"
  - id: P19-WR-02
    severity: warning
    disposition: fixed
    title: "Compatibility uncached LL/SC fault atomicity is untested"
  - id: P19-WR-03
    severity: warning
    disposition: fixed
    title: "v2 compatibility seal control does not exercise inventory binding"
  - id: P20-WR-01
    severity: warning
    disposition: fixed
    title: "V2 FR0 aliasing applies to every conversion"
open: 0
total: 28
recorded: 2026-11-13
---

# Phase 04: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| CR-01 | critical | fixed | Normal/sanitizer controls and review at 79cc519 confirm executable paths/digests are checked before use. |
| CR-02 | critical | fixed | Normal/sanitizer controls and review at 79cc519 confirm ancestor/root confinement and symlink refusal. |
| CR-03 | critical | fixed | Review at 79cc519 confirms remote non-registry sources refuse; the separate source-less gap is CR-04. |
| WR-01 | warning | fixed | Checked fixture writes, reached-case assertions and review at 79cc519 replace the withdrawn six-case coverage. Its invalid history is retained. |
| WR-02 | warning | fixed | Review at 79cc519 confirms the component key and accepted local baseline with isolated refusal mutations. Public controls are self-contained. |
| CR-04 | critical | fixed | Review at 395131b confirms full lock-entry accounting, one manifest-matched root, and refusal of other source-less or unsupported sources. |
| CR-05 | critical | fixed | Review at 53dba86 confirms Git metadata component refusal before writes. |
| WR-03 | warning | fixed | Review at 53dba86 confirms staged publication and restoration after ordinary later-file failures; failed restoration is separately WR-05. |
| WR-04 | warning | fixed | Review at 53dba86 confirms exact integer schema versions and type-refusal controls. |
| WR-05 | warning | fixed | Review at 999681b confirms recovery bytes/mode remain available after an injected failed recovery replacement; the original regression and passing controls are recorded in 04-REVIEW-FIX.md. |
| WR-06 | warning | fixed | Review at 04be456 confirms byte readiness precedes mode restoration, so an injected chmod failure retains and names the complete original-byte copy without claiming verified permissions. |
| WR-07 | warning | fixed | Review at c3c4844 confirms dedicated retained entry/park sections, fixed addresses and exact park opcode; the authored linker fixture verifies placement. |
| WR-08 | warning | fixed | Review at c3c4844 confirms link-time BSS/stack limits and a guard before memory clearing; its separate address-representation defect is CR-06. |
| CR-06 | critical | fixed | Review at 8aa0921 confirms canonical runtime bounds match linker addresses without truncating high bits, and the replacement digest matches. Actual Rust ELF/startup evidence is still required. |
| CR-07 | critical | fixed | Review at 5f01201 confirms report creation works through the committed qualifier API. A new complete authored main-path control first reached the missing-field failure, then passed with a private bound report. |
| CR-08 | critical | fixed | Review at cb23a95 confirms the receipt snapshot and approved tool digest bind inspection; a post-validation tool replacement refuses before invocation and report creation. |
| CR-09 | critical | fixed | Review at ec76c39 confirms canonical ancestor/mode checks before source preparation and closure writes, restrictive creation permissions, and adjacent vendor/post-build early-read confinement. Authored controls assert refused effects stay unreached. |
| WR-09 | warning | fixed | Review at ec76c39 confirms strict resolved private-file containment and traversal/symlink refusal for each graph input class. |
| WR-10 | warning | fixed | Review at ec76c39 confirms source-closure stdout is generic; private derived fields remain in restricted reports. |
| WR-11 | warning | fixed | Review at ec76c39 confirms graph versions, root indices and dependency indices use exact integer type/range checks, with Boolean/float controls. |
| WR-12 | warning | fixed | Review at 212fc16 confirms every applied output matches the production applier's pinned-base/manifest plan before sealing; matching, stale, mutated and symlinked cases are covered. |

| CR-10 | critical | fixed | Qualified adapter path and digest are sealed and checked before and after every real invocation, including startup and aggregation; matching-identity substitution and in-place changes are refused. |
| WR-13 | warning | fixed | An unreaped leader reserves the job identity while its process group is terminated and checked for disappearance, including successful leader exits. |
| WR-14 | warning | fixed | An observed timeout independently forces refusal even when the leader exits zero and cleanup succeeds; the regression reaches those exact conditions. |

The configured incremental review is clean across the retained 130-path union.
The final normal and sanitizer diagnostic selections pass, including the
registered authored build, routing, substitution and lifecycle controls.
Historical findings and their evidence remain above and in the review report.
No requirement or phase completion follows from these source reviews; Plan16
and SC4 remain open pending the verification-target decision.

## Compatibility review dispositions

The earlier 24 pipeline findings remain above. Plan-local identifiers below
are qualified to avoid replacing historical findings with reused IDs.

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| P19-WR-01 | warning | fixed | Nested taken/annul controls plus explicit annulled-slot side-effect and retirement assertions pass normal/instrumented checks; see 04-19-REVIEW.md. |
| P19-WR-02 | warning | fixed | Uncached read-only SC refusal preserves reservation, GPR, bytes and retirement; independent follow-up and owning tests confirm. |
| P19-WR-03 | warning | fixed | Real loader/C encoder/shared aggregator cover compatibility inventory binding and cross-profile refusal. |
| P20-WR-01 | warning | fixed | Scalar conversion destinations retain their requested register and upper word; meaningful RED, owning normal/instrumented GREEN and focused independent re-review are recorded in 04-20-REVIEW-FIX.md and 04-20-REVIEW.md. |

Current canonical review coverage is 135 paths with zero open findings.
The earlier statement that the target decision is pending is historical;
the owner-approved policy is recorded in AUTONOMY.md. Source review does
not itself establish actual diagnostic completion or hardware accuracy.
