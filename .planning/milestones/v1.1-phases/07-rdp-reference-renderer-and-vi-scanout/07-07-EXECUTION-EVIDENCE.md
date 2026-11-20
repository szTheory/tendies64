# Phase 07 Plan 07 Execution Evidence

## Result

Implemented the profile-bound v6 blender and logical coverage sidecar, integrated
coverage through raster output and raw VI, and added accepted CPU/cache/PI/SP
write invalidation. Existing profiles retain their prior identities and paths.
The 16-bit and 32-bit framebuffer paths share the documented logical-address
coverage mapping. No physical hidden-bit packing is claimed.

The software policy uses one coverage byte per two RDRAM bytes, with an allocated
length of `ceil(rdram_bytes / 2)`. Raster sample population maps to logical
coverage as `(samples + 1) / 2`. The sidecar starts cleared, resets on ROM
admission, is released with machine destruction, and participates in v20
canonical digests. Blender cost factors are normalized per cycle; the profile
names alpha/RGB dither selection and 5-bit quantization behavior. These are
versioned software choices where primary documentation does not specify the
physical encoding or tie policy.

There is no separate public ROM-unload or machine-reset operation in this
profile API. Successful admission exercises reset semantics; machine destruction
exercises storage release.

## Registered blender cases

`unit.core.rdp.blender` discovers and runs these ten cases through its real test
dispatcher:

1. `blender-fetched-literal-packet-to-raw-vi`
2. `blender-rgba8888-alias-to-raw-vi`
3. `blender-all-selectors-both-cycles`
4. `blender-alpha-threshold-and-blend-enable`
5. `blender-fetched-dither-quantization-to-raw-vi`
6. `blender-coverage-alpha-and-destinations`
7. `blender-fetched-overlap-and-sidecar-edges`
8. `blender-shared-edge-complementary-coverage`
9. `blender-cpu-pi-sp-write-ownership`
10. `blender-fill-copy-full-coverage`

The fetched literal case checks packet dispatch through raster, RDRAM, and raw VI.
The matrix asserts all selector slots in both cycles, image-read and force-blend
paths, alpha compare ties and dither, coverage/alpha interactions, all coverage
destinations, RGBA5551/RGBA8888 aliasing, overlapping and complementary shared
edges, sidecar bounds and split/interleaved digest behavior, fill/copy full
coverage, and accepted/refused write effects. The write case covers scalar and
masked CPU writes, cache writeback, PI completion, SP register DMA, and functional
RSP DMA preflight/commit.

## Verification

All commands ran from the repository root. The exact case checker archives the
owning build's `LastTest.log` before CTest discovery and retest.

| Command | Outcome | Retained evidence |
|---|---|---|
| `cmake --preset gate && cmake --build --preset gate` | Pass | Gate build; the final registered case check also rebuilt the changed normal test target. |
| `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite blender --run --artifact-prefix 07-07-task2-normal` | Pass, ten cases reached, 1/1 CTest | `build/gate/07-07-task2-normal-blender-LastTest.log` |
| `python3 -B scripts/check-rdp-vi-cases.py --build build/asan --suite blender --run --artifact-prefix 07-07-task2-asan` | Pass, ten cases reached, 1/1 CTest | `build/asan/07-07-task2-asan-blender-LastTest.log` |
| `ctest --test-dir build/gate --output-on-failure -R '^(unit\.core\.rdp\.(fill|copy|triangles|texture|combiner|blender)|unit\.core\.vi\.fill|unit\.core\.dp\.transport|unit\.core\.(pi-dma|sp-dma|rsp-tracer|rdram-protocol|profile|isviewer)|runner\.rdp-fill|controls\.real\.rdp-wrong-pixel)$'` | Pass, 16/16 | Run after the final production edits; the later shared-edge addition changed only the test and case inventory. |
| `ctest --test-dir build/asan --output-on-failure -R '^(unit\.core\.rdp\.(fill|copy|triangles|texture|combiner|blender)|unit\.core\.vi\.fill|unit\.core\.dp\.transport|unit\.core\.(pi-dma|sp-dma|rsp-tracer|rdram-protocol|profile|isviewer))$'` | Pass, 14/14 | Run after the final production edits; the later shared-edge addition changed only the test and case inventory. |
| `python3 -B scripts/check-rsp-no-hle.py --build build/gate` | Pass, 79 reviewed sources and expected RDP/write routes | Guard output recorded in the execution session. |
| `python3 -B scripts/tests/test_rsp_no_hle.py` | Pass, 48 mutation seeds and refusal controls | Mutation output recorded in the execution session. |
| `sh scripts/check-test-inventory.sh --preset gate` | Pass, 151 CTest entries and 84 CPU opcodes; rerun after shared-edge case registration | Output recorded in the execution session. |
| `sh scripts/rights-scan.sh --tree` | Pass, zero findings | Output recorded in the execution session. |
| gcc:13, `--network none`, `/src:ro`, core changed C units with gate warning flags and `-Wframe-larger-than=2048`; test target compiled with its own flags and no core-only frame limit | Pass | `build/gate/07-07-gcc13-warning-compile-final-v2.log`; `build/gate/07-07-gcc13-blender-test-final.log` |

The core gcc:13 check used the repository's gate warning set:
`-std=c17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes
-Wmissing-prototypes -Wundef -Wvla -Wcast-qual -Wformat=2 -Wwrite-strings
-Werror -Wdouble-promotion`, with the 2048-byte frame limit only on core units.
The test compile used the CMake test-target flags. Both compile logs are empty
because both commands succeeded without diagnostics.

The successful ASAN build emitted frame-size warnings from instrumented core
units and duplicate-library linker warnings in runner targets; it completed, and
the focused ASAN case suite passed. The gcc:13 gate-flags compile passed with
`-Werror`. No full ASAN inventory or full phase gate was run here; the parent
owns the final combined gate after independent review.

## Preserved initial failures

These original failure artifacts remain unchanged and are not counted as passes:

- `build/gate/07-07-gcc13-warning-compile-final-v1-failed.log`: the first manual
  test compile omitted the test target's `core/` include root and failed to find
  `t64_internal.h`. The corrected command added that include root and passed.
- `build/gate/07-07-shared-edge-normal-blender-LastTest.log`: the first shared
  edge assertion used framebuffer width for the fixed-stride sample plane.
- `build/gate/07-07-shared-edge-normal-v2-blender-LastTest.log`: after correcting
  the plane index, the independently expected sample population was adjusted
  from nine to the literal vector's ten covered samples. The final normal and
  ASAN runs pass with the corrected expected values.
- `build/gate/07-07-final-normal-v2-blender-LastTest.log`: an earlier digest
  equality assertion captured raw VI on only one of two instances. The case was
  corrected to capture both instances; the subsequent v3 and final suites pass.

Both default-sandbox Git commit attempts could not create `.git/index.lock`;
inspection found no lock file. Each exact staged-path commit succeeded after the
required root guard was rerun with the task-authorized escalation.

## Artifact identity

At final case build time, HEAD was the first plan commit
`547b231437c424e6139472c565d27295a7e32b8f`; CMake embedded that revision in the
core objects. `T64_BUILD_ID` was
`t64-build-v1-4e5abaf31402cdda7348fb4be9a82cdcc5774896e78d1b6ae64192ccb4050a98`.
It is content-derived; the test-only follow-up commit does not change the core
artifact identity. The hashes below bind the final source contents, binaries,
and owning logs used for this evidence.

### Final changed-file SHA-256

| File | SHA-256 |
|---|---|
| `core/CMakeLists.txt` | `6f92e291ec71878a372071198c99448af925a9533f0fd81acf32c275166a4e37` |
| `core/include/tendies64/t64_machine.h` | `d8fb46e3492b477098a79c73ec2fcfcb32af30ea907aec3e2b440a8dbfb18c60` |
| `core/t64_bus.c` | `54f7f02e621bc34c00d2c1fa978234f763b9f060f965e137db309a0370d0d737` |
| `core/t64_digest.c` | `11526df5b21499508b31357d2e7c97c8238b90b7739d9422d1924ffed8de045d` |
| `core/t64_internal.h` | `19731bef1215a8b2d89ac57a62c88e81d88ac7fedd69e2dcade2e0da14e1fa78` |
| `core/t64_isviewer.c` | `2c36fb4263c921ac6e11d42cce4c85a187382906cb7b7e3a67b77b5feb7a9b4a` |
| `core/t64_machine.c` | `ccd8eddf48bd2f74c90d3cdb1294a4465237a3c73719025891660130518e1205` |
| `core/t64_pi.c` | `eae3ec86f63a3a606d8ba1bb67fd31519efdd560ab922f54b28cc8aa2707150a` |
| `core/t64_rdp.c` | `ec19684b91d698872a8faa2367a63ffaf1995453ea7b3d9c6e0ef806c1978fb2` |
| `core/t64_rdp_pipeline.c` | `4cefdcb226b4bd5db05d7a48a23052add3c5db54c5d30133d3a4b9129cc6e5bd` |
| `core/t64_rdp_raster.c` | `ef4e6eb1ded8958ce1dd922d310cc620ebba1b37e65abede95355aca251937ee` |
| `core/t64_rdp_tmem.c` | `0220f77b48520368f590ec2a7862a59aaa55d5123b6e10df9047ec78edc8cc11` |
| `core/t64_sp.c` | `a86fe55e6301bf753fcae882e4bf191db4eb2d9d2f14a78e633c7789052c7cca` |
| `core/t64_vi.c` | `4fe41fd86fe2c44185972ebe63fc42226ff408905689b23392b2f597e232d388` |
| `core/tests/test_rdp_blender.c` | `0431bfa0f55e9e2b2fc2ff042fa859dc55babd89ff591a4e4efc8e276ba4691e` |
| `docs/spec/rdp-vi-cases.json` | `4bc19973dbbca4f1f4e926a7443ae70035684d1b6ece0fd7540b2e5104588857` |
| `docs/spec/rdp.md` | `ee4aa6318569b705d3b1f502b0e2c3429158bc1d0f06b5e6ce748bdaa73adfc4` |
| `scripts/check-rsp-no-hle.py` | `989dcbcda167c9858bd3dcadae4e66c29636c1584876734b6dae6594b794d11b` |
| `scripts/check-test-inventory.sh` | `e6723d7d29b10285fa56ede1f32a9c6a0f984a522a3b94bd107c2589d0d07c24` |
| `scripts/tests/test_rsp_no_hle.py` | `f45416c906fce3d0434bd2368d4b13ea1f4bf7534d8273ddcd0490714131296f` |

### Tested build and log SHA-256

| Artifact | SHA-256 |
|---|---|
| `build/gate/core/libt64_core.a` | `2e42d423d50e4d381d62bfc10c3d71daaf375b7ebfd54713959d3da0fe02c0e3` |
| `build/gate/core/test_rdp_blender` | `df79482dd4a33701292693572b0ee64dfff6547c1ff9270320803f7e0b91f73f` |
| `build/asan/core/libt64_core.a` | `48d62447c90c573034285a74e130c8db9c20435323c2147bccdd90a8666edf71` |
| `build/asan/core/test_rdp_blender` | `a644c10343c69a82fe9e42d3f45c0cefa983842e8ae269616a5e3d5bdb3c7e65` |
| `build/gate/07-07-task2-normal-blender-LastTest.log` | `70f2ee99e747cd628ff991369a13433d0e26969c31c3d88a936c51fe244f4422` |
| `build/asan/07-07-task2-asan-blender-LastTest.log` | `80e3faca9ec3954d08ee05bc76cb0632e8b9437cbf8a364c1586125c3cc877df` |
| `build/gate/07-07-gcc13-warning-compile-final-v2.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `build/gate/07-07-gcc13-blender-test-final.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `build/gate/07-07-literal-check.json` | `6b1ef2a47f145df093e224ca4e496f2289ef516db175796f66a134440211315b` |

## Evidence boundary

These are deterministic authored software checks for this profile and artifact.
No private guest image, external emulator oracle, hardware test, paid operation,
or publication was used. D2/D5/D6 stops remain in force. These results do not
establish VID-04 hardware comparison or broad compatibility; those remain for
the planned later qualification and authority steps.
