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


### Clean-workspace fixture correction

Hosted run `43391557506` exposed an authored-control setup dependency on an
existing local diagnostic directory; both jobs stopped before the evidence
emission/upload tail. The setup now creates its own owner-only temporary
fixture under the ignored build tree. A minimal clean workspace containing
only owned scripts, Git ignore metadata and the built C adapter reaches all
11 routing controls in both normal and instrumented configurations without
creating or consuming a private diagnostic directory. Production code, core
policy, limits and CTest inventories are unchanged. The bounded source review
is clean; the failed run and remedy are retained in the CI review log.


Hosted run [43398862644](https://github.com/szTheory/tendies64/actions/runs/43398862644)
verifies source `81430005903cdc8e58100b5e5a6c29ee60f6d9bd` on Ubuntu 24.04 and
macOS 26. Both jobs pass all 111 registered checks, core lint over 83 files with
zero violations, authored evidence emission, rights/PII scans and sanitized
artifact upload. The newly reached publication tail is verified separately
from the preceding failed run. External diagnostic inputs and results were
not used or uploaded. The CI log records measured timings and the fixture
correction; these checks do not close SC4.

## Approved software compatibility profile

The owner approved the separate
`ntsc-cart-4m-openipl3-vr4300-compat-v1` target, descriptor3/model4/state10 and
semantic policy2. The strict target retains its descriptor, state bytes,
fixed vectors and refusals. [The contract](../spec/cpu-compatibility.md)
defines the separate software evidence boundary, including the v10-only
branch origin for displaced delay-slot exceptions.

The local production artifacts were built from the compatibility source delta
with source annotation `5028c4ffa58055242c73218c3e09b251208f962a`; that annotation
precedes committing the delta and is not a claim that the old commit contains
it. Actual production content and compiler/build policy are bound separately:

| Artifact | Core build SHA-256 |
|---|---|
| Normal | `af340176499a331f6da9de852e7aee52cdcc8ac3d1e71262ff3cd4a2693d2005` |
| ASan/UBSan | `d1d07f7a04340736e76cf4206e01da1c732394499a45fa5c2e9b316f36ee9dc5` |

Both report compatibility profile digest
`5955f3556c0653602c7c2da9faaee49dd2a647f0692d46f823ccb152011fa8d7`.
The complete native gate passed all 111 tests, inventory controls and core
lint. All instrumented targets built and the 39-test owning core/profile,
legacy determinism, protocol/inventory/adapter, admission fuzz and sanitizer
negative-control selection passed. Review additions passed their owning normal
and instrumented checks; the final annulled-register/retirement assertions
also passed both CPU-control runs. No broad retest was repeated for those
test-only follow-ups.

The [portable validation record](../../.planning/phases/04-complete-vr4300-cpu/04-19-VALIDATION.md)
preserves failed fixture attempts, original RED limits and follow-up scope.
The configured review findings are dispositioned, and the incremental security
audit closes both new mitigation threats. These are authored software checks.
Private diagnostic outcomes do not enter this evidence record, hosted inputs
or public uploads. The earlier hosted pair above covers its named earlier
source; new-source hosted verification is separate and remains to be recorded.

Hosted run [43452732705](https://github.com/szTheory/tendies64/actions/runs/43452732705)
verifies the exact compatibility foundation source
`61c9c174428e55594f9d8cb45e4ef8de4f63507c` on Ubuntu24.04 and macOS26.
Both jobs pass all111 checks, core lint83/0, authored evidence emission,
publication scans and sanitized upload. No private diagnostic input or result
is used or published. This pair closes the hosted part of Plan19, while
Plan16 remains incomplete and Plan20 isolates the additional v2 policies.

## Compatibility v2 implementation

The separate `ntsc-cart-4m-openipl3-vr4300-compat-v2` profile is
model5/state11/semantic3. It retains strict and published compat-v1 behavior,
adds an explicitly serialized COP2 latch and selects the documented software
policies in the [contract](../spec/cpu-compatibility.md). No physical-console
qualification or external diagnostic outcome is claimed here.

The full local native gate passed all111 tests, inventory and lint in139s;
the all-target instrumented build and39-test owning selection passed, including
fuzz and the sanitizer negative control. Those broad runs preceded a review
correction and retain these actual build identities:

| Pre-correction artifact | Core build SHA-256 |
|---|---|
| Normal | `7d884052d5d84a396fe4db1e443695796857aa3de23441470ee41d081ef2d315` |
| Instrumented | `dbf18f85f9572316d3443bcc3674b3bc5b843b6c056e0356508f59b477fc67ed` |

Review found that scalar conversion results could overwrite an even neighbor
instead of the requested odd destination. A fetched-instruction regression
fails before the correction and passes afterward. Conversion source aliases
remain intentional software policy; destination normalization now requires a
64-bit pair result. Complete target builds and the six owning COP1/three-
diagnostic controls pass in both configurations after that change. Unchanged
subsystem evidence is reused; the earlier broad runs are not relabeled.

| Corrected artifact | Core build SHA-256 |
|---|---|
| Normal | `14cc6c38ae714fa1c92596f51b917b1a4e17e63746ece058150a7174ad7a5f9a` |
| Instrumented | `fdd7246d62002cb9f776514cbba2f312971576928e2c452e483bbbf0a206fe77` |

Both corrected artifacts carry source annotation
`c3856d1e7613ad28a59ff5fd61c0ce415e8f6ebc` with the then-uncommitted v2 delta;
actual content is bound by the build identities above. The fixed profile hash is
`1086516cd466bf48133533a8edf33a12cc8fd9e27dcd2b493e92b2bcf21a7634`.
The independent prepared-state reference agrees with the state11 production
serializer and fixed vector, including old-version exclusion of the new latch.

The configured source review and narrow follow-up resolve the conversion
finding; the security auditor closes all three new mitigation threats and
confirms the correction. [Validation](../../.planning/phases/04-complete-vr4300-cpu/04-20-VALIDATION.md),
[RED limits](../../.planning/phases/04-complete-vr4300-cpu/04-20-RED-evidence.md)
and [correction proof](../../.planning/phases/04-complete-vr4300-cpu/04-20-REVIEW-FIX.md)
retain the actual scopes and fixture mistakes. Exact-source hosted verification
and private Plan16 completion remain separate and are not asserted by these
native results. No protected input/result enters this record or hosted uploads.
