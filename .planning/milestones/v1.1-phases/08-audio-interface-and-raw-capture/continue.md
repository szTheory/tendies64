# Current checkpoint: Phase 8 complete

Phase 8 (Audio Interface and Raw Capture) is complete and qualified for its authored
scope: native normal/ASan/GCC13 and both hosted jobs pass on commit `3a41734`
(run 44549490908), the exact-binding verifier accepts the records, and the
independent security audit (SECURED, 28/28) and goal verification (passed, 7/7)
are recorded here. See `08-05-SUMMARY.md`, `docs/evidence/phase08-audio.md` and
`docs/claims/phase08.md`.

Next unblocked work: plan Phase 9 (Controller, Save Media and Replay), which depends
on Phase 8. Phase 7 Plan 13 / VID-04 stays stopped on oracle authority and the
parallel-rdp rights audit; do not clear it.

Follow-ups, none blocking: cache the pinned Binutils build in CI (CI review log
F-096, changes `ci.yml` so it needs its own native + hosted cycle), the duplicate
`repo.rsp-no-hle` run (F-097), and the supervisor's ~68 KiB per-file capture limit
before larger frames can be captured (`docs/spec/raw-capture.md`).

Rule worth keeping: the files in `SOURCE_PATHS` of `scripts/qualify-audio.py` are
bound to a qualification; any change to them needs a new native run, push, hosted
run and collection. `--verify-evidence` requires HEAD to equal the tested commit.
