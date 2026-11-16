# Phase 1 code review: finding dispositions

Source: `01-REVIEW.md` (37 findings: 4 critical, 19 warning, 14 info). Outcomes below were verified by the
orchestrator (gate green at HEAD, asan preset green, rights and PII scans clean, spike amendment ordering checked).

Status values: `fixed` (code or doc change with a guarding test where code changed), `deferred` (explicit,
recorded reason), `recorded` (accepted limitation written into a project document).

| ID | Severity | Status | Commit | Note |
|----|----------|--------|--------|------|
| CR-01 | critical | fixed | eb9f161 | guard `unit.schema-sanitize` plus two manifest seeds |
| CR-11 | critical | fixed | 266a1d9 | commit-message framing forgery |
| CR-12 | critical | fixed | 7660ed0 | `-diff` attribute could blank the scans |
| CR-13 | critical | fixed | 235eada | Layer 2 now allow-lists read-only sections |
| WR-01 | warning | fixed | 7465482 | supervisor FIFO hang |
| WR-02 | warning | fixed | 7465482 | capture length consistency |
| WR-03 | warning | fixed | 0694d5e | result cross-field invariants |
| WR-04 | warning | fixed | 89bd438 | process group killed on every exit path (limit: `setsid` escapees, stated in spec) |
| WR-05 | warning | fixed | 0c7f59f | saturating deadline |
| WR-06 | warning | deferred | 56d94cb | fixture-digest TOCTOU recorded as a Phase 3 requirement in `docs/spec/result-protocol.md` |
| WR-07 | warning | fixed | 6d709d0 | manifest checker re-validates shapes under a permissive schema |
| WR-11 | warning | fixed | e5f90ab | denied-extension normalisation |
| WR-12 | warning | fixed | a2fe5dc | bare user-path patterns; one history waiver for commit 097cc7e (see below) |
| WR-13 | warning | fixed | 00ed04e | copied-looking detector: BOM and comment leaders |
| WR-14 | warning | fixed | 7105a2e | docs-assets validation, narrow waiver classes, CODEOWNERS |
| WR-15 | warning | fixed | d228c66 | CI checker covers every workflow, trigger and secret spelling |
| WR-16 | warning | fixed | eb8f1ba | cycle-counter builtins and quoted includes |
| WR-17 | warning | fixed | f82839f | policy states exactly what is detected |
| WR-18 | warning | fixed | b128e2d | `--range` validation, all refs in history mode |
| WR-21 | warning | fixed | 68e8c2c, a4e0002 | spike decision three-valued, PREREG Amendment 1, run 2 recorded |
| WR-22 | warning | fixed | 7e65a34 | lint tool hashes recorded as a disclosed gap |
| WR-23 | warning | recorded | a8e5f05 | ADR-0002 "Evidence limits"; Phase 2 success criterion 6 (production admission parser fuzz) |
| WR-24 | warning | fixed | 51aecda | report-cap assertion isolated |
| IN-01 | info | fixed | 1c6c21f | |
| IN-02 | info | fixed | 1c6c21f | no failing test possible (allocation removed) |
| IN-03 | info | deferred | none | schema-subset anchored-pattern check is superficial |
| IN-04 | info | deferred | none | runner inherits duplicate stdout/stderr descriptors |
| IN-11 | info | fixed | 4b4da94 | |
| IN-12 | info | fixed | 80ad4b5 | |
| IN-13 | info | fixed | dcc44be | |
| IN-14 | info | deferred | none | PII scan does not read author names, tags or non-HEAD refs for names |
| IN-21 | info | fixed | ef908b9 | |
| IN-22 | info | fixed | ef908b9 | facade undefined cases documented as provisional |
| IN-23 | info | fixed | ef908b9 | |
| IN-24 | info | deferred | none | file is in the frozen spike hash table; PREREG forbids amending admission files |
| IN-25 | info | deferred | none | can only push toward a false trigger |
| IN-26 | info | fixed | ef908b9 | |

## Notes for the owner

- **History waiver:** `tools/rights/pii-history-waivers.tsv` waives exactly 5 hits of the fictitious example
  user name in the immutable, unpushed commit 097cc7e. It never applies to the working tree, commit messages
  or metadata, and any other hit count fails the scan. It can be deleted if that commit is reworded before
  the first push.
- **Language decision evidence:** see ADR-0002 "Evidence limits". The review found real memory-safety bugs in
  the C tools (CR-01, WR-07) that the plan's tests did not. The decision remains C17 as a reversible default.
- **Sanitizer lane:** measured at 44-45 s for the full asan preset (finding F-017 in `docs/ci-reviews/LOG.md`),
  not added to the gate; the log recommends a gate step that excludes six slow shell selftests.
