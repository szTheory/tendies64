# Phase 07 Plan 10 execution evidence

## Delivered behavior

Plan 10 adds the versioned VI filter profile `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v9` while preserving the preceding profile identities and filter behavior. The selected path applies bounded integer AA, resampling, divot, gamma, gamma dither, and dither filtering to fetched VI output and records the selected filter and source metadata. The software policy and its limits are documented in `docs/spec/vi.md`.

The video output queue reserves a complete packet before production. A full queue leaves the VI event pending at its original tick; draining an older packet permits one exact resume. Copy-out validates the full destination before writing, and output observer occupancy does not enter the guest digest. The supervised runner compares actual packet records and machine digests at 16-tick and 32-tick advance intervals.

## Owning checks

The normal gate build was configured and built with `cmake --preset gate` and `cmake --build --preset gate`. Focused owners completed:

- `build/gate/core/test_profile`: 1,309 checks, zero failures; v9 profile mutation, serialization, identity, and canonical admission coverage.
- `T64_TEST_RDP_FILL_PATH="$PWD/build/fixtures/rdp-fill.z64" build/gate/core/test_vi --suite vi-filters`: 667 checks, zero failures.
- `T64_TEST_RDP_FILL_PATH="$PWD/build/fixtures/rdp-fill.z64" build/gate/core/test_video_output --suite video-output`: 294 checks, zero failures, including a pending full-queue event, drain, same-boundary resume, and exactly-once output.
- The accepted fetched CPU/DPC/RDP fill path reached the raw-frame consumer. The same-setup wrong-pixel control reached its first-difference comparator at `(0,0,0)` (`expected fe`, `actual ff`).
- The actual supervised runner at 16 and 32 guest-tick chunks produced the same packet record and state digest: `955216df937b27d68115de6e6799cbdb5187fc8b052442ebfdcf778af302b6b6`.
- `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite vi-filters --run --artifact-prefix 07-10-final` passed with a nonempty owning suite. `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite video-output --run --artifact-prefix 07-10-output` passed both registered output tests.
- `sh scripts/check-test-inventory.sh --preset gate` passed: 156 CTest entries and 84 CPU opcodes.
- `python3 -B scripts/check-rsp-no-hle.py --root . --build build/gate` passed with 80 reviewed source/object routes. `T64_RSP_NO_HLE_BUILD_DIR="$PWD/build/gate" python3 -B scripts/tests/test_rsp_no_hle.py` passed 52 mutation controls, including the filtered VI renderer body mutation.
- Existing warning compilation in the read-only `gcc:13` container passed for changed core, test, runner, and supervisor units. The 2,048-byte frame warning limit applies only to core units.
- Normal baseline CTest passed 6/6. The focused ASAN/UBSan owner set passed 9/9, including VI fill/timing/filter/output, profile, accepted runner, cadence, wrong-pixel, and replacement-fixture refusal.

Owning normal and instrumented `LastTest.log` snapshots are retained before and after their runs at `build/gate/07-10-before-baseline-LastTest.log`, `build/gate/07-10-baseline-LastTest.log`, `build/gate/07-10-final-vi-filters-LastTest.log`, `build/gate/07-10-output-video-output-LastTest.log`, `build/asan/07-10-before-focused-LastTest.log`, and `build/asan/07-10-focused-LastTest.log`.

## Initial failures and limits

The accepted v9 AA packet initially disagreed with six independently derived output pixels; the retained v8 negative control remained unfiltered. The v9 implementation then passed the same literals. These are preserved at `build/gate/07-10-v9-aa-red.log` and `build/gate/07-10-retained-v8-aa-red.log`.

Several early attempts were setup or harness issues, not behavior REDs: the first fetched-RDP producer attempt did not advance far enough to fetch its packet (`07-10-rdp-producer-setup-failure.log`); the next compile attempt had an unresolved test link (`07-10-rdp-producer-first-compile-failure.log`); initial case discovery found the manifest omitted cases from one executable (`07-10-case-inventory-discovery-failure.md`); the first GCC 13 warning compile lacked `_XOPEN_SOURCE=700` for `realpath` (`07-10-gcc13-warning-compile.log`); and the first mutation run did not yet include the new seed count. Each original artifact is retained; corrected owning checks passed.

The 128-tick runner attempt at `build/gate/07-10-runner-128-debug.log` stopped in `drain_trace` with a full 256-record observer ring and 117 dropped records. It did not reach the runner's resource-yield/no-progress guard, so it establishes exhaustion of the existing trace observer capacity for that attempt, not VI output backpressure or a same-tick progress failure. A separate accepted core test proves output-full drain and resume. The runner guard currently recognizes new trace records when it retries a resource yield, while the runner cadence check proves only 16/32 intervals; no accepted runner case demonstrated the guard misclassifying a drain-enabled resume. That possible interaction remains unproven for independent review. No observer or execution bounds were increased.

These are authored software tests and measurements. They do not establish silicon accuracy, broad game compatibility, or VID-04 completion.

## Exact artifact binding

`build/gate/07-10-before-final-build-retained/binding.json` binds the final normal source snapshots, core/test/runner executables, archive, fixture, and initial owner logs. All 40 entries were rehashed against both their source artifact and retained copy with zero mismatches. The bound content-derived build identity is `t64-build-v1-03245d5d78932eee06635136770864c7ffc208eda1a62a31e6e416a226b2155a`; its base-revision annotation is not treated as semantic test evidence. Relevant source and output hashes remain in that binding. The final normal/ASAN logs and their own SHA-256 entries are catalogued in `build/gate/07-10-final-artifact-binding.json`.
