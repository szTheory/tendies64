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
