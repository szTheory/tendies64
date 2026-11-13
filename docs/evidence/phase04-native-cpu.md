# Phase 04 native CPU verification

This record covers the public, authored native CPU implementation and local
diagnostic controls. It does not report external diagnostic results or qualify
the physical console.

## Tested source and profile

| Item | Measured value |
| --- | --- |
| Original full native closure source revision | `900833d85e23df2c8c02e68055f1b81e4a76f4f2` |
| Host | Darwin arm64; Apple clang 21.0.0 (`clang-2100.1.1.101`) |
| CMake / Ninja | 4.4.3 / 1.13.2 |
| Gate build identity | `t64-build-v1-9cfb72dbf9f6e4cd81e57e46b6ced340abddcadac00a50338f5313ad91219f5a` |
| ASan build identity | `t64-build-v1-1465ef57588ffd3aca6b20cec75b4499a4763425af071bf36f61c4f448c99daa` |

The build identities describe the production core inputs and build policy;
the source revision separately binds the result to the complete repository
revision. Both original closure configurations embed the tested revision above.
The build identities are unchanged by the runner-only source fixes; the full
source revision is the separate identity binding.

The named software profile is
`ntsc-cart-4m-openipl3-vr4300-v1`: a prepared, deterministic software seed for
an unqualified `NUS-CPU-03` target. The descriptor-v3 CPU profile accepts an
authored flat image up to `0x00401000` bytes (a 4 MiB payload plus the IPL3
area). The separate warm-v1 profile retains its 1 MiB image limit and all
seven legacy fixture hashes. The authored startup control exercises an image
above that warm-v1 boundary. These are native software-model results, not
measured hardware timing or general ROM compatibility. See the [CPU profile
contract](../spec/cpu-profile.md) and [profile limits](../profile-limits.md).

Expected arithmetic values remain independent of the implementation under
test: the raw arithmetic matrix uses bit, integer, rational, and interval
derivations, with hardware-specific fields tied to the cited primary manual.
See the [independent arithmetic evidence](softfloat-closure-audit.md).

## Native gate

Command: `sh scripts/gate.sh`

The full gate passed on `c8e59ef127630d74face3af1db1daf6577fd1713`: all 111
registered tests passed, core lint covered 83 files with zero violations, and
the inventory self-test passed 224 single-registration removal controls and
five malformed-inventory controls. The exact inventories were 111 gate and
113 ASan registrations. Shell syntax, authored fixture qualification,
evidence-collector self-tests, rights, PII, provenance, and hostile-input
controls passed. The existing seven warm-v1 fixture identities remained
unchanged.

After the final portability-only delta at
`900833d85e23df2c8c02e68055f1b81e4a76f4f2`, the gate configuration and every
target rebuilt successfully, and all three CPU diagnostic protocol, inventory,
and adapter controls passed. Both exact inventories passed again.

## Instrumented build and controls

All ASan targets were configured and built before running the focused selection.
The inherited environment and selection were:

```sh
cmake --preset asan && cmake --build --preset asan
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --preset asan --no-tests=error \
  -R '^(unit\.core\..*|runner\.(fixture-descriptor|result-v2|cpu-diagnostic.*)|integration\.open-ipl3-(hello|determinism)|controls\.real\..*|fuzz\.rom-admission|sanitizer\.negative-control)$' \
  --output-on-failure
```

On `c8e59ef127630d74face3af1db1daf6577fd1713`, the all-target ASan/UBSan build
passed and all 51 selected tests passed in 108.22 seconds. This included the
three CPU diagnostic protocol, inventory, and adapter controls; the complete
core unit selection; authored IPL3 startup and determinism; real-runner
refusal controls; the sanitizer negative control; and the ROM-admission fuzz
campaign. The fuzz test completed in 67.06 seconds, within its existing
120-second bound.

After the final portability-only delta at
`900833d85e23df2c8c02e68055f1b81e4a76f4f2`, all ASan targets rebuilt, all
three CPU diagnostic tests passed under the inherited sanitizer environment,
and both exact inventories passed. The final gate and ASan build metadata
embed that revision.

The normal gate retains the oversized supervised guest control. That case
exceeded the unchanged child wall bound under ASan instrumentation in the
earlier owning check, so it is intentionally selected out of sanitizer runs.
The instrumented run still covers the large in-process IPL3 startup and a
supervised small guest. No production timeout or sanitizer limit was changed.
The sanitizer build emitted existing large-frame and duplicate-library
warnings; these are recorded diagnostics and do not establish general stack
safety.

## Review and local-output boundary

The configured code review covered the 101 tracked native source/test paths
changed across Plans 01–15 and 17, then reviewed each Plan18 source delta: the
three-file output-boundary fix and one-file strict-POSIX portability fix. It
reports no remaining findings. The configured ASVS Level 1 security audit
closed the high-severity output-boundary finding for the executed native
scope. The fix refuses authored diagnostic execution when `CI=true` or
`T64_REQUIRE_FIXTURES=1`; it permits batch output only in an ignored
repository path or a secure temporary directory, and fails closed if Git
inspection is unavailable. Tests cover public repository destinations, a
Git-unavailable path, and both environment refusals. Output files remain
exclusive and mode `0600`.

A final bounded security recheck confirmed the strict-POSIX sticky-bit
fallback preserves the same directory acceptance predicate. No high-severity
finding remains in the executed native scope; Plan16 is not included.

The [review report](../../.planning/phases/04-complete-vr4300-cpu/04-REVIEW.md)
records the source scope and result. The audit covered executed native phase
work only; Plan16's separate external-tool and private qualification remains
unverified and outside this report.

## Hosted verification

The first hosted run after the output-boundary fix exposed a portability
failure: Linux strict-POSIX compilation hid `S_ISVTX`; the macOS job passed on
that earlier source. The guarded `CPU_STICKY_DIRECTORY_MODE` fallback in
`900833d85e23df2c8c02e68055f1b81e4a76f4f2` preserves the same sticky-bit
check. After the bounded local rebuild and diagnostic regression checks, the
exact-source hosted run `43293884688` passed both existing jobs:
`gate-linux` (`131446911234`) and `gate-macos` (`131446911538`). Both jobs
passed 111/111 tests, linted 83 files with zero violations, and completed all
evidence and scan steps.

The Linux job took 141 seconds, including a 135-second gate (86 seconds for
fixture qualification, 19.22 seconds for CTest, and 4.67 seconds for the
diagnostic step). The macOS job took 335 seconds, including a 294-second gate
(189 seconds for fixture qualification, 39.82 seconds for CTest, and 5.13
seconds for the diagnostic step). Run wall time was 567 seconds and measured
critical path was 564 seconds; the macOS queue wait was 229 seconds and its
fixture qualification was the largest gate step. The [CI review log](../ci-reviews/LOG.md)
records the full cost review. No job, cache, concurrency, or resource limit
was changed. The unchanged hosted collector remains limited to authored hello
evidence; no external diagnostic material or derived private result is
uploaded.

## Evidence limits

This report establishes authored native test behavior for the named software
profile and the tested source/build identities. It does not establish full
IPL3 completion, broad game compatibility, cycle accuracy, physical-console
behavior, or completion of the separate private SC4 qualification. No external
ROM, source, capture, tool, or diagnostic result was used for this verification.


## Authored pipeline controls — 2026-11-13

Source content committed as `27d2177` adds the local pipeline, exact production
adapter binding, authenticated early completion and bounded command cleanup.
The owning all-target normal build and exact three diagnostic CTests passed in
14.88 seconds. The all-target ASan build and the same selection passed in
36.47 seconds with `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. These selections include
20 authored build controls and 11 authored routing/aggregation controls, plus
the existing qualification, overlay and C protocol controls. No CTest name or
inventory changed.

The native core and profile policies are unchanged, so earlier full native
and admission-fuzz evidence is retained. The current normal and instrumented
adapter binaries embed source annotation `ce87869`; their semantic core build
identities remain those recorded above. The final timeout fix affects the host
build helper and its authored control, not those core binaries. This record
preserves the actual tested identities and does not relabel a binary following
a documentation or source-annotation change.

The configured source review is clean, and the incremental security audit
verifies the registered pipeline qualification and private-output mitigations.
Those verdicts and authored checks do not establish SC4 or external diagnostic
completion. External inputs, receipts, captures and derived results remain
private and are not part of this public evidence record.
