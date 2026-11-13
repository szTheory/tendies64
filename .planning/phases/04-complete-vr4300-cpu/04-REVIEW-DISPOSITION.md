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
open: 0
total: 11
recorded: 2026-11-13T00:44:32Z
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

These findings concern the local receipt gate and its authored controls. The
earlier native core and inventory-v3 reviews retain their documented scope.
No external toolchain installation or diagnostic build/run is authorized by
the withdrawn control result. The reviewed gate/applier findings are closed;
the executor continues the actual overlay/source closure and private build/run
qualification. The renderer supplement at a02ade6 is clean. These reviews and
authored controls do not establish SC4 or overall Plan 16 completion.
