# Phase 2 CI portability correction

The first Phase 2 push passed the macOS gate but Linux GCC 13 rejected the
allocation-size guard with `-Werror=type-limits`: a 32-bit descriptor field
cannot exceed the 64-bit size limit. The local Apple Clang build accepted it.

`core/t64_machine.c` now converts the validated RDRAM byte count into the
`size_t` allocation total, checks the same subtraction bound, then adds the
machine size. The overflow/refusal rule and allocation order are unchanged;
strict warnings remain enabled. No timing, profile or parser semantics changed.
The existing lifecycle and profile tests both passed and core lint reported no
violations. Full sanitizer and replacement hosted CI results are recorded in
`02-VALIDATION.md` and `docs/ci-reviews/LOG.md` when available.

The first push run was reviewed for critical-path cost. A redundant manual
workflow dispatch caused by a delayed run listing was cancelled before jobs
started; future dispatch decisions use the exact-head REST response. No cache,
additional job, dependency or concurrency change was justified.

The replacement Linux build compiled the production core, then exposed two
test-only GCC warnings: an unused shared test header in `test_pi_dma.c` and
missing grouping around addition before a bit mask in `test_determinism.c`.
The header was removed and arithmetic parenthesized without changing expected
values. Both focused tests passed. The final-source sanitizer campaign remains
applicable because production sources and the fuzz harness are unchanged.

The next Linux build reached `test_skeleton_probe.c` and reported that nested
`snprintf` path joins could truncate equal-size 512-byte arrays. Each temporary
path buffer now derives its capacity from the actual parent array and literal
suffix. This preserves filenames while making the maximum combined span
explicit. The focused producer/consumer test passed; production and fuzz inputs
remain unchanged. Hosted retest is still required.

The first focused probe retest failed because the test executable had a refreshed
core identity but its subprocess probe executable had not been rebuilt by the
single-target build. This is a build-graph defect, not a model mismatch.
`runner/CMakeLists.txt` now makes the two new consumer test targets depend on
all executable producers/checkers they invoke. A targeted rebuild automatically
relinked the stale probe, and both normal consumer tests passed. The corrected
buffer test and build dependencies also passed their focused ASan/UBSan tests.
No warning was suppressed and no extra package or hosted lane was added.
