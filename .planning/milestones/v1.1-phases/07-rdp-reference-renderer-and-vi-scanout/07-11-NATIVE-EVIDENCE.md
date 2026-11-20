# Phase 07 Plan 11: Native Qualification Evidence

**Result:** Local authored qualification passed for source commit `2c8e9ef487792970b588b14ea253f4a706d0a861`. This is native, sanitizer, and GCC evidence only. Hosted artifact collection and publication are still pending; VID-04 remains incomplete.

## Source and preflight binding

The actual CLI entry was run once:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -B scripts/qualify-rdp-vi.py --native
```

The command completed with exit 0. Its preflight reached the accepted real producer, the wrong-pixel comparator, the parent/grandchild deadline control, the native source-binding control, and the missing, swapped, zero-test, missing-v9, and forged-record controls. It reported all controls passed before starting the full gates.

The qualification bound 505 relevant source/build/test/fixture/spec inputs. The canonical SHA-256 manifest is `2bc31c3e67291a9068ba94cd3bddb15dd55533c9ca588a056c6ad1aac3ac4eee`; each input also records its Git blob ID and Git mode separately. The post-command source recheck passed. The explicitly permitted owner files `AGENTS.md`, `.planning/config.json`, and `.planning/AUTONOMY.md` were excluded from this gate input set and left untouched by this work.

Detailed source binding and per-stage original records are in `build/gate/07-qualification/native-2c8e9ef48779-zh4cdygy/`. The relevant source identity fields are in `source-binding.json`; each stage directory has its original `command.json`, `process.json`, `stdout.log`, and `stderr.log`.

## Native and compiler results

| Stage | Actual result | Duration |
| --- | --- | ---: |
| `sh scripts/gate.sh` | Exit 0; all 157 normal CTests passed. Full inventory and core lint passed; 93 linted files, zero violations. | 77.894 s |
| `cmake --preset asan` | Exit 0. | 0.259 s |
| `cmake --build --preset asan --parallel 1` | Exit 0. | 8.320 s |
| Full ASan/UBSan CTest | Exit 0; 159 JUnit cases, zero failures, errors, or skips. | 238.655 s |
| Read-only GCC 13 route | Exit 0; compiled all 30 Phase 07 changed C files plus the real-runner and supervisor test-macro variants. | 23.579 s |

The sanitizer command used exactly `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Its JUnit file is `build/gate/07-qualification/native-2c8e9ef48779-zh4cdygy/asan/full-ctest-junit.xml`; the owning normal and sanitizer `LastTest.log` files were retained in the same directory before and after their respective CTest runs.

The host build used AppleClang 21.0.0.21000101 on arm64. The actual runner binaries embed `t64-build-v1-4f10a216af1f0564c70eeb58590dbb08fe534486f86afb7c64e06ff602daf5b9` for normal and `t64-build-v1-4e93628787d5e272fad1cbbbad8a36798c8bb6680820331b69255559a5c0a975` for ASan/UBSan.

GCC 13 used the already-local image `sha256:ad13696c6b2464e813639c0479ec2f66877516ec18a183c85282378720376786` (`linux/amd64`) with pull disabled, network disabled, and a read-only source mount. Its exact command, including source list and warning flags, is retained in `gcc13/command.json`; no image was pulled.

## Retained build artifacts

Before the first all-target gate build, the current binary closures were copied and verified in `build/gate/07-11-native-prebuild-retained/binding.json`: 93 normal and 95 ASan executable/library artifacts. This is a pre-run snapshot, not a claim that the old binaries formed a coherent build of the current commit. The older normal binding disagreed with nine listed binary hashes; that historical record and the fresh snapshot remain available.

After qualification, the exact final normal and ASan/UBSan closures were copied and checked again. `build/gate/07-qualification/native-2c8e9ef48779-zh4cdygy/final-binary-binding.json` binds 93 normal and 95 sanitizer artifacts to the post-run source input manifest and stage receipts. All 188 retained copies passed both SHA-256 and Git blob-ID verification. The detailed file sizes, modes, hashes, compiler configuration, and embedded build annotations are in that binding.

## Limits and next step

These results establish the authored local software checks for this exact source and build set. They do not establish hosted CI collection, ROM compatibility, silicon behavior, oracle agreement, or VID-04 completion. No private guest, oracle, hardware, or external asset was used. The existing D2/D5/D6 and publication stops remain in effect, and future oracle threats remain open until their authorized phase.

The remaining qualification action is the separately authorized hosted run and exact artifact collection on this tested source snapshot. Only that later evidence can support the hosted result; local mock-transport controls are not a substitute.
