# Phase 2 Final Recheck — WR-05

**Result:** WR-05 is fixed. The ASan refresh and retained fuzz evidence are consistent with the scoped fix reports. No source changes or test reruns were made during this recheck.

## WR-05 alias coverage

At `core/tests/test_profile.c:544-553`, the regression receives the built-in `ntsc-cart-4m-skeleton` profile returned successfully by `t64_profile_lookup`. The invalid `changed` copy is separate from `valid_mutable` (`:341-342,364-365`), so it does not poison the alias case.

The test first serializes `valid_mutable` into separate storage and requires `T64_STATUS_OK` plus the expected length (`:387-391`). It then copies that valid descriptor into `source_alias.profile` and passes `source_alias.bytes` with capacity `sizeof source_alias.bytes` (`:392-399`). The output capacity is exactly the declared array extent in the same union that backs the source descriptor; the length output is a separate local. The byte snapshot covers the full declared backing array and must remain unchanged (`:394-401`).

The serializer checks profile validity and address-range validity before overlap detection, then rejects overlap before its write path (`core/t64_machine.c:355-380`). Given the successful separate-buffer serialization and the identical union base address for source and byte output, this call passes the validity/range checks and reaches the source/output overlap guard. The alias regression would fail if that guard were removed.

## ASan and fuzz evidence

- `build/asan/CMakeCache.txt` identifies a Debug build with `-fsanitize=address,undefined`. The refreshed `test_profile` embeds build ID `t64-build-v1-43235b5150ec1f6bf5bb419365585e6e02e9b54e3c23ab2be7eef06b4cfe871f` and production source revision `c739c25df618bdac8f485e1859b40a8cd60278b6`. The test source timestamp precedes its object/binary timestamp; the test-only commit `05b298f` followed that build. This is consistent with the report that the changed working-tree test was compiled before committing it; the build ID covers production core inputs, not the test source.
- The core component executables and the runner's `test_skeleton_probe` and `test_model_result` have the refreshed build timestamp recorded in `build/asan`. `ctest --preset asan -N` confirms the reported selection contains 13 tests: 11 `unit.core.*` entries, `unit.model-result`, and `sanitizer.negative-control`. `02-REVIEW-FIX-2.md` records a full ASan build followed by that refreshed 13/13 selection. The retained `LastTest.log` contains only start/end timestamps, so it does not preserve individual CTest pass lines. The sanitizer control fixture itself was unchanged and was invoked by the selected negative-control test.
- The earlier 14-test ASan result used stale component/runner executables and is superseded; it is not counted as fresh evidence here. The corrected 13-test selection excludes the long fuzz campaign.
- The valid full fuzz result remains the prior recorded run: 250,000 mutations plus 148 fixed cases (250,148 calls) in 69.10 seconds. It is recorded in `02-REVIEW-FIX-2.md` and independently retained by `02-RECHECK-HOST.md`; it was not rerun for this bounded recheck.

## Scope limits

This was a source and artifact recheck of WR-05 and the existing ASan/fuzz records. No full suite or fuzz campaign was rerun. The individual 13-test pass transcript is not retained in `LastTest.log`; the 13/13 result is supported by the fix report, while the ASan binary identities, build configuration, refresh timestamps, and exact test selection were spot-checked here.
