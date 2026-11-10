# CI critical-path review log

Phase-1-base: 57227793345c59259ab6eac12d1044bc408dfd60

This log is append-only (VER-04). Rows are added at the end of a table by
`scripts/ci-review.sh RUN_ID --append docs/ci-reviews/LOG.md`, and findings with
`--finding "id|finding|taken/declined|reason"`. An existing row is never edited
or removed. A mistake is corrected by a new finding row that names the old one.

`Phase-1-base` is `git rev-parse origin/main` recorded before the first push of
Phase 1. The pushed SHAs of the phase are `git rev-list <Phase-1-base>..origin/main`.

## Review policy

- Every completed CI run for any pushed SHA gets one Runs row, red runs
  included (AUTONOMY section 2: inspect measured critical-path delays after
  every CI run).
- The first run's row is the minimum the Phase 1 gate (VER-04) requires.
- A dated deferral row is allowed only when a run's data cannot be read. It
  keeps the same leading `| date | run id |` columns and states the reason in
  the last column.
- A commit that changes only this file carries `[skip ci]` in its message, so
  recording a review never triggers a run that would itself need a review.
  Every code-bearing commit triggers a run, and that run is reviewed. The rights
  and PII scans still run locally before each push.
- A finding is resolved as `taken` (applied, with the distinct evidence kept
  intact) or `declined` (with the reason), so it is not rediscovered at every
  milestone.

## Columns

Runs: `date (UTC)` is the run's creation date. `wall s` is the run's
`created_at` to `updated_at`. For each job, `queue` is the job's `started_at`
minus `created_at` and `run` is `completed_at` minus `started_at`, both in
seconds, written `queue/run`. `critical path s` is the maximum over jobs of
queue plus run. `slowest step` is the longest single step across all jobs.

Findings: `id` is `F-NNN` and unique. `run id` is the run the finding was read
from.

## Runs

| date (UTC) | run id | sha (12) | event | conclusion | wall s | critical path s | gate-linux queue/run s | gate-macos queue/run s | slowest step |
|---|---|---|---|---|---|---|---|---|---|

## Findings

| id | run id | finding | taken/declined | reason |
|---|---|---|---|---|
