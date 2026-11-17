# Plan 07-08 Execution Evidence

Plan 07-08 adds a versioned RDP depth path and tests its command, RDRAM, hidden
sidecar, digest, and VI effects. It records a software policy for fields whose
exact runtime choices were left open; it does not claim hardware accuracy.

## Red and implementation

The initial depth attempt refused at setup (`refusal_reason=5`), so that run was
not treated as a meaningful RED. After fixing the authored packet setup, the
accepted fetched triangle rendered the expected green color while only the
visible depth and hidden-delta assertions failed (`0x09a8` and `0x83`). That
valid RED is retained in `build/gate/07-08-red-retest-LastTest.log`,
`build/gate/07-08-red-junit2-LastTest.log`, and
`build/gate/07-08-red-record.json`. The production implementation made those
assertions pass through the registered dispatcher and raw VI capture.

The v7 composite profile binds descriptor 10, model 16, and serialization 21.
V6 profile identity and serialized bytes are checked by a fixed digest and
length. V7 owns the aligned byte-addressed Z image, visible 16-bit depth,
per-halfword logical hidden sidecar, signed Q16.16 interpolation, named
`0,15.3` conversion and saturation, primary eight-row compression, delta
priority, compare tolerance, and the selected opaque, interpenetrating,
transparent, and decal rules. The sidecar is not a claim about physical hidden
bit packing.

The registered depth suite has 11 reached cases. It covers all eight raw
triangle opcodes and reconciled packet lengths (including the final Z words
W20/W21 for opcode 0f), signed fractional interpolation, compression rows and
transitions, zero and maximum delta, less/equal/greater comparisons, decal
tolerance, coverage and alpha rejection, all incomplete Z packet prefixes,
invalid image alignment and bounds, old-profile refusal, reset, digest
sensitivity, split/interleaved instance output, FullSync, and raw VI output in
both 16-bit and 32-bit color modes. External CPU, PI, SP, fill, and copy writes
also exercise depth-sidecar invalidation ownership.

## Verification

| Check | Result | Retained evidence |
|---|---|---|
| `cmake --preset gate && cmake --build --preset gate` | Pass | Build in `build/gate` |
| `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite fill --run --artifact-prefix 07-08-fill-preflight` | Pass, 5/5; includes accepted runner, wrong-pixel control, and replacement refusal | `build/gate/07-08-fill-preflight-fill-LastTest.log` |
| `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite depth --run --artifact-prefix 07-08-depth-final` | Pass, 11 cases registered, 1/1 CTest | `build/gate/07-08-depth-final-depth-LastTest.log`, command JSON, and exit status |
| Focused normal owner CTest for RDP depth, triangle, texture, blender, fill, copy, combiner, DP, VI, profile, digest, bus, cache, PI, and SP | Pass, 16/16 | CTest output confirmed during execution; the later final depth-only run replaced `LastTest.log`, and its owning log is retained separately |
| Focused ASAN/UBSan CTest for 15 affected owners | Pass, 15/15; final depth case rerun also passes | `build/asan/07-08-pre-depth-digest-fields-LastTest.log` and `build/asan/07-08-final-depth-LastTest.log` |
| GCC 13.5 read-only warning compile, `--network none`, `/src:ro` | Pass | `build/gate/07-08-gcc13-warning-compile.log`, `build/gate/07-08-gcc13-test-warning-compile.log` |
| `python3 -B scripts/tests/test_rsp_no_hle.py` | Pass, 80 reviewed sources and 50 mutation seeds | `build/gate/07-08-final-mutation-guard.log` |
| `python3 -B scripts/check-rsp-no-hle.py --build build/gate` | Pass, 80 source-object edges and depth routes | `build/gate/07-08-final-source-guard.log` |
| `sh scripts/check-test-inventory.sh --preset gate` | Pass, 152 CTest entries and 84 CPU opcodes | `build/gate/07-08-final-inventory.log` |
| `sh scripts/rights-scan.sh --tree` and `sh scripts/check-provenance.sh` | Pass, zero rights findings; 1,014 files and zero provenance errors | `build/gate/07-08-final-rights.log`, `build/gate/07-08-final-provenance.log` |

The sanitizer build emitted existing instrumented frame-size warnings in core
files and completed. No full ASAN inventory or combined phase gate was run;
those are outside this plan's focused owner checks, and the parent owns the
combined gate after independent review.

## Failures retained without relabeling

- `build/gate/07-08-red-LastTest.log` is the earlier setup refusal, not RED.
- `build/gate/07-08-red-retest-LastTest.log` and
  `build/gate/07-08-red-junit2-LastTest.log` retain the valid depth RED.
- `build/gate/07-08-owner-final-LastTest.log` retains the v6 owner-fixture
  failure; the fixture was corrected to use the v7 sidecar profile.
- `build/gate/07-08-oob-address-fix-LastTest.log` retains the bounds-vector
  refusal mismatch; its literal was corrected. The subsequent diagnostic
  opcode expectation was corrected and the owning depth test passed.
- The first ASAN target build named nonexistent `test_cp0_cache`. CMake's
  target list identified `test_cpu_path`; the corrected focused ASAN build and
  tests passed. No package or tool was installed.
- An early production compile rejected the first profile-policy string length;
  the policy label was shortened before the valid RED. Its compiler output was
  not retained as a standalone file.

## Source and artifact identity

The source commits are `d4717ef89374ee715ddeb489b9e63302bbb480f8`
(accepted-path RED), `a7bdfc43bcaf3e3058333d83eb9efc0c1649324a`
(profile and renderer), and `203b2079bfed5bd1cb0c44d98cd867c59819ab77`
(test matrix and strict guards). The recorded plan base is
`084ccc4ef4056e79ec81ce7114d6d28e618ed862`; three plan task commits are
measured from that ledger. This execution evidence is a separate closeout
documentation commit.

Exact changed-source hashes, test binaries, core archives, LastTest logs,
command records, and build identities are retained in
`build/gate/07-08-retained-artifacts/`. The normal build identity is
`t64-build-v1-34ac4ee923191a6b41d735210b980c21c1c881806b7abf24d1a11dff5886bb60`;
the ASAN build identity is
`t64-build-v1-b1f1005c296c387baa80980deca02e529a1786abd2c6a4444bfcb35e7f3d3df2`.
Both artifacts record source revision
`d4717ef89374ee715ddeb489b9e63302bbb480f8`; this is the compiled revision
annotation, while the content-derived build IDs and hashes bind the source
artifacts. See `build/gate/07-08-retained-artifacts/binding.json` for the
per-file SHA-256 values.

Authored test success remains software-contract evidence only. It does not
complete VID-04, and no private guest, external oracle, silicon, paid, or
publication activity was used.
