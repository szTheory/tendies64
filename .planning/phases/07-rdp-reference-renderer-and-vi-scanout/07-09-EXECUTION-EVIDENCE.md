# Plan 07-09 execution evidence

## Delivered behavior

The new composite VI profile is `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8`.
Its VI timing, active/pending register state, rational phase, line/current,
field and event state are versioned, serialized and included in the canonical
digest. Its raw video metadata reports the selected geometry, source pitch and
encoding, field, filter and timing/output policy. Existing VI v2 and other
profile identities retain their prior semantics and policy strings.

Primary register facts remain separate from software choices. The selected
profile uses 62,500,000 guest ticks/second and 48,681,812 source cycles/second;
H_SYNC is low 12 bits plus one, V_SYNC is the low 10-bit half-line count, and
the period is `(H + 1) * V / 2` source cycles. Software policy is named
`rsp62500000-ntsc48681812-halfline-rational-v2`: bounded quotient/remainder
conversion, CURRENT once per full line, INTR at the selected half-line,
next-field latching, end-exclusive geometry, Q10 source step/offset, and
serrate row/parity mapping. Zero V_SYNC disables the standard timer. These are
reproducible software-contract choices, not claims about undocumented silicon
behavior. PAL/MPAL profile expansion and VID-04 qualification remain out of
scope.

## Verification

| Command | Result | Retained evidence |
|---|---|---|
| `cmake --preset gate` | Pass | `build/gate/` |
| `cmake --build --preset gate --parallel 4` | Pass, no pending work | `build/gate/` |
| `ctest --test-dir build/gate --no-tests=error --output-on-failure -R '^(runner.rdp-fill|controls.real.rdp-wrong-pixel)$'` | Pass, 2/2: accepted runner and wrong-pixel comparator control | `build/gate/07-09-accepted-controls.log`, `build/gate/07-09-accepted-controls-LastTest.log`, `build/gate/07-09-accepted-controls-exit.txt` |
| `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite vi-timing --run --artifact-prefix 07-09-final` | Pass; 22 registered cases, 1/1 CTest, 1,177 checks and zero failures | `build/gate/07-09-final-vi-timing-LastTest.log`, `build/gate/07-09-final-vi-timing-command.json`, `build/gate/07-09-final-vi-timing-exit-status.txt` |
| `ctest --test-dir build/gate --no-tests=error --output-on-failure -R '^(unit.core.profile|unit.core.vi.fill|unit.core.rdp.fill)$' --output-junit $PWD/build/gate/07-09-final-baseline-owners.xml` | Pass, 3/3 | `build/gate/07-09-final-baseline-owners-LastTest.log`, `build/gate/07-09-final-baseline-owners.xml` |
| `cmake --preset asan` and `cmake --build --preset asan --target test_vi test_profile --parallel 4` | Pass | `build/asan/core/test_vi`, `build/asan/core/test_profile` |
| `ctest --test-dir build/asan --no-tests=error --output-on-failure -R '^(unit.core.vi.timing|unit.core.profile|unit.core.vi.fill)$' --output-junit $PWD/build/asan/07-09-focused-owners.xml` | Pass, 3/3; ASan/UBSan | `build/asan/07-09-focused-owners-LastTest.log`, `build/asan/07-09-focused-owners.xml` |
| GCC 13.5 container compile with `--pull=never --platform linux/amd64 --network none --read-only`, source mounted `/src:ro`, C17 gate warnings and `-Werror` | Pass, all five changed C units | Core: `t64_machine.c`, `t64_vi.c`, `t64_digest.c` also used `-Wframe-larger-than=2048`; tests `test_vi.c` and `test_profile.c` used their target warning flags without the core-only frame limit. No diagnostics. |
| `python3 -B scripts/check-rsp-no-hle.py --build build/gate` | Pass, 80 sources and object edges | Guard completed against the final source and normal archive. |
| `python3 -B scripts/tests/test_rsp_no_hle.py` | Pass, 51 mutation seeds | Injected-effect and unknown-source refusal controls pass. |
| `sh scripts/check-test-inventory.sh --preset gate` | Pass, 153 CTest entries and 84 CPU opcodes | Exact phase inventory is nonempty. |
| `sh scripts/rights-scan.sh --tree` | Pass, zero findings | Tree scan completed; history scan remains skipped by the approved scope. |
| `sh scripts/check-provenance.sh` | Pass, 1,024 files, 57 copied-looking entries, zero errors | Provenance inventory accepted. |
| `git diff --check` | Pass | No whitespace errors. |

The ASan/UBSan target build emitted frame-size warnings in instrumented core
functions; the build completed and all three focused sanitizer owners passed.
No full ASan inventory or combined phase gate was run; the parent owns the
combined gate after independent review.

The GCC invocation used the already available `gcc:13` image, with no pull or
network access:

```sh
docker run --rm --pull=never --platform linux/amd64 --network none --read-only \
  --tmpfs /tmp:size=64m \
  -v $PWD:/src:ro -w /src gcc:13 sh -ec '
COMMON="-std=c17 -O3 -DNDEBUG -DSOFTFLOAT_FAST_INT64=1 -Icore/include -Itools/common -Icore/softfloat -Icore -Itests -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wvla -Wcast-qual -Wformat=2 -Wwrite-strings -Werror -Wdouble-promotion"
gcc $COMMON -Wframe-larger-than=2048 -c core/t64_machine.c -o /tmp/machine.o
gcc $COMMON -Wframe-larger-than=2048 -c core/t64_vi.c -o /tmp/vi.o
gcc $COMMON -Wframe-larger-than=2048 -c core/t64_digest.c -o /tmp/digest.o
gcc $COMMON -c core/tests/test_vi.c -o /tmp/test_vi.o
gcc $COMMON -c core/tests/test_profile.c -o /tmp/test_profile.o
'
```

## Test chronology and retained failures

The first literal run that reached the valid fetched-command consumer was
**green** (45 checks). The earlier `build/gate/07-09-literal-red.log` was a
fixture/environment setup refusal and is not RED evidence. A valid
pre-implementation semantic RED was therefore not established; no earlier
failure is being reclassified to satisfy the plan's TDD label.

The first expanded, already-implemented suite had 14 failures, retained in
`build/gate/07-09-timing-initial.log`; a later expanded suite had six failures,
retained in `build/gate/07-09-timing-expanded.log`. These were reached
post-implementation failures. Investigation corrected independently checked
Q10 literal placement and pixel expectations, tick/trace-order expectations,
and the legacy v7 output-policy expectation. The boundary work also corrected
the v8 low-24-bit origin normalization and progressive parity behavior, and
made blanking take effect at the documented next-field latch. Each original
log is preserved; subsequent reached cases pass in the final run. Intermediate
compiler/setup failures are retained under `build/gate/07-09-*-compile*.log`
and are not represented as behavioral failures.

## Tested artifact identity

Both build artifacts carry source revision annotation
`c1f391d85aa4999275439397335480721e7c9c65`. Their content-derived build IDs
and principal executable/archive/log hashes are:

| Artifact | SHA-256 / build ID |
|---|---|
| Normal build ID | `t64-build-v1-ea6bb3376502ad40d5cb9e9beab5f29d021ccb3d7e3bf41de7ebb226ff41926e` |
| ASan build ID | `t64-build-v1-4b15944dbe4292a9ef03f9c33a8b72d8ec7266a9624d59d829fb2f5cfcead887` |
| `build/gate/core/test_vi` | `05c74ba99e320ff9213ee4fdf4871fbbe4264c395b976b020453ee8a51e0f881` |
| `build/gate/core/test_profile` | `aa8f07b127fc8444bcb586a740a9d186e4f83c61f3dd50e15e155672a794d870` |
| `build/gate/core/libt64_core.a` | `457c35c88fde586d8bd4c26967596db13d02d196044442e91683eb200342a068` |
| `build/asan/core/test_vi` | `985df9cecf8b3fd5dd078b43dbf5fc95ca50da47c167becef8fe5aeed50c09fd` |
| `build/asan/core/test_profile` | `a9b2825f409883f315807dad93081e94d069f23006e3d5b0c64e6d6bb434fd48` |
| `build/asan/core/libt64_core.a` | `4f485347b00b9676e107564c6a7207351dc50879c4d3ab5f012957fa1b64febe` |
| Final normal VI `LastTest.log` | `9b882a9991f0b957d772a79dc87a3ac87285fd99cb48ab412739faf62ab1d8fa` |
| Final normal baseline owners `LastTest.log` | `1133fc2254a69001fe88ef105dc57a833012104625f33f1091d03c0f17e01705` |
| Focused ASan owners `LastTest.log` | `f24617b8d591e6ac82bcadd56917df9b811388c3a1aa34d6039032a123b45c38` |

Changed source hashes at the tested point:

| File | SHA-256 |
|---|---|
| `core/t64_vi.c` | `67f3bbb4d4d64b9c7f1881c10f8adb7347df2d1e76cd6f3fa8709a72cc8c400c` |
| `core/t64_internal.h` | `5eda29e7700c927e7c399df2ae69f270d2161ef3d777267aa67fa9ff1ee23791` |
| `core/t64_digest.c` | `f3094ddad8c0458e572e41f95f62130beb1dde3976e8e53a510fdee2e57441a2` |
| `core/t64_machine.c` | `415db3e82cd64217fa46fb8bcf7357d94c2d6c1045e967fd032685264839df51` |
| `core/include/tendies64/t64_machine.h` | `1539132e44a9b71090068bf7da97ccf01413091fb8a5a8be75e12e48bc246774` |
| `core/tests/test_vi.c` | `09bb1f7e26bec4ed36b3f05dd82b6bcdacc8515b87b1f8c65327e79a1cad0d1a` |
| `core/tests/test_profile.c` | `4bd75b066eedf01a7b955570e5d2ebd3ec63b2f7faf834f40d3a5cbe2a6d6324` |
| `core/CMakeLists.txt` | `1e563aa169c1dd697850bae4e045119868552087c4925922220d5c593681d40e` |
| `docs/spec/vi.md` | `03f85a17c18d4d0ad40a49cca8cbb2fe42249230844587e182b3521f7f82d47a` |
| `docs/spec/rdp-vi-cases.json` | `78aab3243ac9e441f111a348319109fd2610b2db8047f720cb144a800d40e972` |
| `scripts/check-test-inventory.sh` | `5ca9805d45125f19904b55ad7bf58d0e11dc0e1b757ea0b061cda5bdeb7dbfb1` |
| `scripts/check-rsp-no-hle.py` | `25ee7b4e5abccfe421e31eea8556132f55b3431f5833ea23d0c4fe823545bf9b` |
| `scripts/tests/test_rsp_no_hle.py` | `9d0b657a830cff4fcf1a07ce61d141106b42dedf00f11bc6329dae9fbbf4663a` |

Authored tests establish the named deterministic software contract only. No
private guest, external emulator oracle, physical hardware, paid service or
publication was used; VID-04 remains incomplete.
