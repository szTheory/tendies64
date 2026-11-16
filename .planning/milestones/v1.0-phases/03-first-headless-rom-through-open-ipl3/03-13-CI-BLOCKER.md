---
phase: 03
plan: "13"
status: hosted-proof-collected
source_commit: 7c0ea9b5f3076122d506074aa7d3a2283cf9e2a6
observed_at: 2026-11-11T21:42:09Z
---

# Plan 13: exact-SHA hosted proof collected

The required actual Linux/macOS artifact pair has been collected and compared
for this source. The plan can now close in `03-13-SUMMARY.md`; Phase 3 goal
verification remains with the parent executor, so this note does not mark the
phase complete.

## Successful hosted pair

The normal push run [43103669084](https://github.com/szTheory/tendies64/actions/runs/43103669084)
completed successfully for SHA `7c0ea9b5f3076122d506074aa7d3a2283cf9e2a6`,
attempt 1. `gate-linux` job `130801302890` and `gate-macos` job
`130801302572` both passed Gate, evidence emission, rights and PII scans, and
the scoped JSON upload. The exact-run collector published the pair to the
ignored local directory `build/evidence-run-43103669084`; the comparator
reports matching guest identities, bound, state and authored transcript.
Different truthful compiler build IDs are allowed.

Linux artifact `13892259569` has API archive metadata size 962 bytes and
digest `sha256:6317ebb793436ba93abc737c909266665d86763a6fd36f4a209bb612d802e2df`;
its extracted 1587-byte JSON record hashes to
`cd2a21c8471de0e1f2040aeacaeaf03b57f04e8b5cf157782e4e0b1787a0c013`.
macOS artifact `13892669317` has API archive metadata size 962 bytes and
digest `sha256:e0aceddaa6f661d8590c9899f18a5db23bb94122423ce0806a204cd7857a05db`;
its extracted 1587-byte JSON record hashes to
`edc14bf88b6bebe74c4a427a2e4f5034e685873e517046585a9309f3ce950e61`. The
archive digest and extracted record hash/size describe separate layers; the
archive digests were not recomputed locally.

Both records report v2 pass for profile `ntsc-cart-4m-openipl3-warm-v1`,
guest tick 530438, 530437 retired instructions, authored completion bound
530500, final PC `ffffffff80000478`, the same 30-byte log digest and the same
state digest. Full result details and the measured CI critical path are in
`docs/evidence/phase3-ci.md`.

## Earlier blockage and superseded queue entry

Predecessor run `43094128662` is now terminal/cancelled. Its Linux job passed
and uploaded one record, but the macOS upload step was cancelled at the
unchanged 10-minute job timeout. The retrieved log shows three retries where
the upload action expected JSON but received HTML (`Unexpected token '<'`)
before backoff. This is the observed symptom; the underlying cause remains
unknown. Its Linux artifact was not combined with this run. The historical
deferred timing row remains in `docs/ci-reviews/LOG.md`; follow-up F-034 records
the final timings and log symptom.

Queued run `43098235914` for source
`8eea81c35ab2d7069ea413997c3d9d36dbd99c2e` is superseded and was not used.
No timeout, action pin, hosted lane, or artifact was changed or deleted for
recovery.
