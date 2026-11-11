---
phase: 03
plan: "13"
status: external-ci-blocked
source_commit: 8eea81c35ab2d7069ea413997c3d9d36dbd99c2e
observed_at: 2026-11-11T21:01:13Z
---

# Plan 13: implementation present, hosted proof pending

Do not mark this plan or phase complete. No `03-13-SUMMARY.md` exists because
the mandatory actual two-host artifact pair has not been collected/compared.
Do not redispatch the implementation or fabricate an async-job manifest: the
installed external-job capability is disabled and has no GitHub adapter.

The latest pushed source is the commit above. Its push run is
[43098235914](https://github.com/szTheory/tendies64/actions/runs/43098235914),
attempt 1, workflow `.github/workflows/ci.yml`; it is queued with zero jobs.
The predecessor run 43094128662 still reads `in_progress` despite its macOS
upload being cancelled at its ten-minute job boundary. Normal and force cancel
both return 409, and log retrieval returns 502/timeouts. The underlying upload
cause is unavailable. Its Linux artifact is partial evidence only and must not
be combined with another run/attempt/SHA. Preserve all prior artifacts.

The source implementation, common gate, exact 91/93 inventories, full sanitizer
run and later affected-path retests are recorded in `docs/evidence/phase3-ci.md`.
Pre-CI review blockers were fixed; final incremental code/security reviews are
the parent's next work while GitHub remains blocked. No owner decision is
needed for these reviews, and the ten-minute job limits remain unchanged.

Resume the existing `phase3_execute13` agent after reviews and cloud state permit;
if its session is unavailable, reconcile the existing commits and resume only
pending hosted collection/evidence/summary work, not all implementation tasks.
Do not use interactive mode to introduce a new routine owner gate.

```sh
sh scripts/collect-boot-evidence.sh --collect --repo szTheory/tendies64 --sha 8eea81c35ab2d7069ea413997c3d9d36dbd99c2e --run 43098235914 --attempt 1 --out build/evidence-run-43098235914
sh scripts/compare-boot-evidence.sh --sha 8eea81c35ab2d7069ea413997c3d9d36dbd99c2e --run 43098235914 --attempt 1 build/evidence-run-43098235914/gate-linux.json build/evidence-run-43098235914/gate-macos.json
```

Only run collection after that exact attempt has completed successfully with
both required jobs/artifacts. Any later source fix needs its own explicit
tested selectors; never relabel these records as a later build.
