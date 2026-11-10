---
phase: "02"
review: 02-REVIEW.md
titles: json
findings:
  - id: CR-01
    severity: critical
    disposition: fixed
    title: "Digest output can overwrite the live machine"
  - id: WR-01
    severity: warning
    disposition: fixed
    title: "PI timing ignores the selected profile's duration fields"
  - id: WR-02
    severity: warning
    disposition: fixed
    title: "Scheduler does not enforce profile sequence width"
  - id: WR-03
    severity: warning
    disposition: fixed
    title: "Multi-config Debug and Release builds share one build ID"
  - id: WR-04
    severity: warning
    disposition: fixed
    title: "Profile serialization can overwrite its own output length buffer"
  - id: WR-50
    severity: warning
    disposition: fixed
    title: "Special-file paths can block the model tools"
  - id: WR-51
    severity: warning
    disposition: fixed
    title: "New subprocess tests have no hang bound"
  - id: WR-52
    severity: warning
    disposition: fixed
    title: "Refusal snapshot can silently shrink"
  - id: WR-05
    severity: warning
    disposition: fixed
    title: "Source-alias regression uses an invalid profile"
open: 0
total: 9
---

# Phase 2: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---|---|---|---|
| CR-01 | critical | fixed | 02-REVIEW-FIX.md; independent rechecks |
| WR-01 | warning | fixed | 02-REVIEW-FIX.md; independent rechecks |
| WR-02 | warning | fixed | 02-REVIEW-FIX.md; independent rechecks |
| WR-03 | warning | fixed | 02-REVIEW-FIX.md; independent rechecks |
| WR-04 | warning | fixed | 02-REVIEW-FIX.md; independent rechecks |
| WR-50 | warning | fixed | 02-REVIEW-FIX.md; independent rechecks |
| WR-51 | warning | fixed | 02-REVIEW-FIX.md; independent rechecks |
| WR-52 | warning | fixed | 02-REVIEW-FIX.md; independent rechecks |
| WR-05 | warning | fixed | 02-REVIEW-FIX-2.md; independent rechecks |
