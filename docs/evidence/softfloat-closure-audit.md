# SoftFloat Release 3e selected closure audit

Status: pre-use source and license closure finalized before additional source incorporation.

## Pin and rights

- Official repository: [ucb-bar/berkeley-softfloat-3](https://github.com/ucb-bar/berkeley-softfloat-3).
- Selected commit: `f74b1e48110ac3a27dd49b787d164e55e42d81d1` (Release 3e, 2018-01-26); fetched commit identity matched all 40 hex digits.
- Current official `master` at audit time: `07ebd29db0cfb55723a79fbbffe99103ef38e305`, dated 2026-11-02; repository is public and has a newer official commit, so recorded upstream status is `active`. The selection remains pinned to Release 3e.
- Inspected the pinned `COPYING.txt` and all 50 selected C and five selected header files at the pinned commit. `COPYING.txt` explicitly applies the three BSD conditions and disclaimer to every source file individually. Every selected file contains the Regents' source notice; the exact per-file copyright line is recorded below.
- `LICENSES/BSD-3-Clause.txt` byte-compares equal to the pinned `COPYING.txt`. The file-specific SHA-256 values below are over raw upstream blob bytes; the full upstream commit SHA binds each path to the official tree.
- A persistent ignored checkout is kept at `build/private-source-audit/softfloat-3e`; its `FETCH_HEAD` is the complete 40-character pin above, its origin is the official repository, and its `COPYING.txt` SHA-256 is `145ea96b4a4a04a1a7738d2a2bf9e830f861971e69606187b018d9e8fc0b95c7`. The exact official `master` advertised by `git ls-remote` at this audit was `07ebd29db0cfb55723a79fbbffe99103ef38e305`.
- The first ignored scratch checkout was absent when work resumed; I fetched the same exact official commit again, verified the fetched object and license hash, and retained this checkout plus an ignored receipt for later checks. No additional upstream files were copied to production before that re-verification.
- No selected file is target-specific. `platform.h` and `specialize.h` are authored portable Tendies64 glue.

## Exact source, destination, notice, and direct dependency table

This table is the frozen pre-copy denominator: 50 generic C sources and 5 generic headers. “Calls” lists selected direct code dependencies; “Context” lists upstream ambient state references that will be threaded explicitly. Header includes and authored glue dependencies are listed per file.

| Upstream source → repository destination | Upstream SHA-256 | Verbatim copyright notice | Direct includes, calls, and context |
| --- | --- | --- | --- |
| `source/f32_add.c` → `core/softfloat/f32_add.c` | `540c8f2d95b30bc846fe5b747aa6aae1bb70c687b9429278678bb0a000f65dcc` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_addMagsF32, softfloat_subMagsF32 |
| `source/f32_div.c` → `core/softfloat/f32_div.c` | `ec341d5028f63f19230729ed8d8aca424b08f221adfdce4ebce3caa6d54ba0d8` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_approxRecip32_1, softfloat_normSubnormalF32Sig, softfloat_propagateNaNF32UI, softfloat_raiseFlags, softfloat_roundPackToF32 |
| `source/f32_eq.c` → `core/softfloat/f32_eq.c` | `2bb5a867872c4489c9d1363efeccc1d09af4f8c74914c4821b38171a3ad7acb2` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_raiseFlags |
| `source/f32_le.c` → `core/softfloat/f32_le.c` | `84d3e3211b5ad2870602fd00bbc43b14bd9caec33292b433b8537b132bce5ce9` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_raiseFlags |
| `source/f32_lt.c` → `core/softfloat/f32_lt.c` | `b992a2f1f5a678b12865e50f836ab499993009566d491c82ef521dedee5218ce` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_raiseFlags |
| `source/f32_mul.c` → `core/softfloat/f32_mul.c` | `bca46360750b343f24aafcfa7c36f24e3fc4fb6465d0c49f60de7980a86bc37b` | Copyright 2011, 2012, 2013, 2014, 2015 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_normSubnormalF32Sig, softfloat_propagateNaNF32UI, softfloat_raiseFlags, softfloat_roundPackToF32, softfloat_shortShiftRightJam64 |
| `source/f32_sqrt.c` → `core/softfloat/f32_sqrt.c` | `2eb2d1a5a35d2e839aa69e18bda3868ef5f86c214df6adad3ae50cb8a6195dc8` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_approxRecipSqrt32_1, softfloat_normSubnormalF32Sig, softfloat_propagateNaNF32UI, softfloat_raiseFlags, softfloat_roundPackToF32 |
| `source/f32_sub.c` → `core/softfloat/f32_sub.c` | `c4020ba5c2d5f10e647711bd7287c574ca498b0e469ee99f1f9c68c930d43311` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_addMagsF32, softfloat_subMagsF32 |
| `source/f32_to_f64.c` → `core/softfloat/f32_to_f64.c` | `d25caefe4a26b9e9e1fa1cc53c683d8a22468122b2d09c16deb0e833702ada10` | Copyright 2011, 2012, 2013, 2014, 2015 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_commonNaNToF64UI, softfloat_f32UIToCommonNaN, softfloat_normSubnormalF32Sig |
| `source/f32_to_i32.c` → `core/softfloat/f32_to_i32.c` | `f955cfbb69c337d4546c2cf5353654432fb3170cc9f4fb0f416e306fdb4ea73c` | Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_roundToI32, softfloat_shiftRightJam64 |
| `source/f32_to_i64.c` → `core/softfloat/f32_to_i64.c` | `d7a1083de841e474736b8357f2342879b23c155a1591475f95020c48228bcefc` | Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_raiseFlags, softfloat_roundToI64, softfloat_shiftRightJam64Extra |
| `source/f64_add.c` → `core/softfloat/f64_add.c` | `edb05c732053d1107456c95b7f9b18e58a25546925d3ffafac27b5f263db0b2e` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_addMagsF64, softfloat_subMagsF64 |
| `source/f64_div.c` → `core/softfloat/f64_div.c` | `c44985b8084d6407fd3cd4043dbe02c2651721ac03d1c3920798fc1b75ea2f3e` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_approxRecip32_1, softfloat_normSubnormalF64Sig, softfloat_propagateNaNF64UI, softfloat_raiseFlags, softfloat_roundPackToF64 |
| `source/f64_eq.c` → `core/softfloat/f64_eq.c` | `70f18d1accbd596ff4be01882ec05b2006aedc96d0e5bde63f8c95851eedccd8` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_raiseFlags |
| `source/f64_le.c` → `core/softfloat/f64_le.c` | `a5ebbcf9314360ef9ce8e064856129d0665db2371de4069ed2d16ecc79e4042b` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_raiseFlags |
| `source/f64_lt.c` → `core/softfloat/f64_lt.c` | `f3bd638772b93335cb693ccaa149e482c38135634e50217fd6b1f925db20df45` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_raiseFlags |
| `source/f64_mul.c` → `core/softfloat/f64_mul.c` | `335137bca17d29eddb451f562869ca7327c5fe472f347d2e36c55e4c2219e79b` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_mul64To128, softfloat_normSubnormalF64Sig, softfloat_propagateNaNF64UI, softfloat_raiseFlags, softfloat_roundPackToF64 |
| `source/f64_sqrt.c` → `core/softfloat/f64_sqrt.c` | `643a9abb4d30aff2cac9be281687055a1a677ae7555ca782fc3fb1932d000908` | Copyright 2011, 2012, 2013, 2014, 2017 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_approxRecipSqrt32_1, softfloat_normSubnormalF64Sig, softfloat_propagateNaNF64UI, softfloat_raiseFlags, softfloat_roundPackToF64 |
| `source/f64_sub.c` → `core/softfloat/f64_sub.c` | `6b581000e70e679e057d2ce17e0839aa5a7da6982685b9718b6540069441e689` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_addMagsF64, softfloat_subMagsF64 |
| `source/f64_to_f32.c` → `core/softfloat/f64_to_f32.c` | `48e998019d76613444c6f20e5d272464abd2657795830905dcf629bb18d90529` | Copyright 2011, 2012, 2013, 2014, 2015 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_commonNaNToF32UI, softfloat_f64UIToCommonNaN, softfloat_roundPackToF32, softfloat_shortShiftRightJam64 |
| `source/f64_to_i32.c` → `core/softfloat/f64_to_i32.c` | `8cb6390959e1800514601ceab2a2010bfc6189729357d212c377208951a3b286` | Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_roundToI32, softfloat_shiftRightJam64 |
| `source/f64_to_i64.c` → `core/softfloat/f64_to_i64.c` | `1502fb0ef9d22983b8f5040284f3a69a8479326109a7b82f14c7217687b90900` | Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_raiseFlags, softfloat_roundToI64, softfloat_shiftRightJam64Extra |
| `source/i32_to_f32.c` → `core/softfloat/i32_to_f32.c` | `e6ccd7ec5ca8ea2b64435e94ecb0962be000f31d36abb88e68f6e90b44c0c290` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_normRoundPackToF32 |
| `source/i32_to_f64.c` → `core/softfloat/i32_to_f64.c` | `de334caa22edfcfc2a35572a2e858f0c500ad5018fc4335b46dfb003e0076183` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_countLeadingZeros32 |
| `source/i64_to_f32.c` → `core/softfloat/i64_to_f32.c` | `15c028b6e784138754cd7af1e6c360416ea26d71e97f52fe2a5cb452eb6048c4` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_countLeadingZeros64, softfloat_roundPackToF32, softfloat_shortShiftRightJam64 |
| `source/i64_to_f64.c` → `core/softfloat/i64_to_f64.c` | `905170a7a36d108afa5fc43596cd4a29a5a6a8f82264ad890b45feeb5bad021c` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_normRoundPackToF64 |
| `source/s_addMagsF32.c` → `core/softfloat/s_addMagsF32.c` | `5b2d7992841c17e42a96e5aec865b4386cdc7e9030ac6f4d25ad74e4e73fb59b` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h; calls softfloat_propagateNaNF32UI, softfloat_roundPackToF32, softfloat_shiftRightJam32 |
| `source/s_addMagsF64.c` → `core/softfloat/s_addMagsF64.c` | `2b4f69206b4e8d3fc26bc64ca147663530b68b53b0b0e7817c724e2d99376ec5` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h; calls softfloat_propagateNaNF64UI, softfloat_roundPackToF64, softfloat_shiftRightJam64 |
| `source/s_approxRecip32_1.c` → `core/softfloat/s_approxRecip32_1.c` | `38e984f9c6ea5843216ba73d08fafd217b285badce557bc9eb58227acc3667ea` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h; calls softfloat_approxRecip_1k0s, softfloat_approxRecip_1k1s |
| `source/s_approxRecipSqrt32_1.c` → `core/softfloat/s_approxRecipSqrt32_1.c` | `dae5502facf520d0915403967e3436e1c2d2f5b848527accdb3b1d2a3654a47f` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h; calls softfloat_approxRecipSqrt_1k0s, softfloat_approxRecipSqrt_1k1s |
| `source/s_approxRecipSqrt_1Ks.c` → `core/softfloat/s_approxRecipSqrt_1Ks.c` | `d7c4b4833ce9f42e86d9933a550ad0d6e4b8bc4df4bb30518c8a747b12699bf8` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h, primitives.h; defines coefficient arrays read by their matching approximation helpers; arrays will be static const behind pure accessors |
| `source/s_approxRecip_1Ks.c` → `core/softfloat/s_approxRecip_1Ks.c` | `a073abc2d5e7a4dfd97f323027d1ef1d370225aa922ae2395398ff3c7c8e9633` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h, primitives.h; defines coefficient arrays read by their matching approximation helpers; arrays will be static const behind pure accessors |
| `source/s_countLeadingZeros8.c` → `core/softfloat/s_countLeadingZeros8.c` | `8d0076e681bc613b607d8ff9d1b72631a3bfe66dd75ecb7f4faed5ddc3b830ca` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | stdint.h, platform.h, primitives.h; calls softfloat_countLeadingZeros8 |
| `source/s_countLeadingZeros32.c` → `core/softfloat/s_countLeadingZeros32.c` | `4cc8579412c437c011f3d9795a2bbd3d4eb06349c69ce624aa169471c643f336` | Copyright 2011, 2012, 2013, 2014, 2015 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h, primitives.h; calls softfloat_countLeadingZeros8 |
| `source/s_countLeadingZeros64.c` → `core/softfloat/s_countLeadingZeros64.c` | `f8fc823d585542552de1295cc8181369799030e7c67d2fa21ae6f7be16bb6840` | Copyright 2011, 2012, 2013, 2014, 2015 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h, primitives.h; calls softfloat_countLeadingZeros8 |
| `source/s_mul64To128.c` → `core/softfloat/s_mul64To128.c` | `1cff12724898aaace69ba3d7301404ded67156ba13466aec4a25f1a3cbb9564d` | Copyright 2011, 2012, 2013, 2014, 2015 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h, primitiveTypes.h; calls no SoftFloat helper |
| `source/s_normRoundPackToF32.c` → `core/softfloat/s_normRoundPackToF32.c` | `2e473ba709740808cfd59931034be7c67c80c87ddb76a4765dceb23c9bb518d2` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h; calls softfloat_countLeadingZeros32, softfloat_roundPackToF32 |
| `source/s_normRoundPackToF64.c` → `core/softfloat/s_normRoundPackToF64.c` | `4e65ccecf8ee96e2c5815575df3a0e4495ac82987a841868cb1d130556a3dd0a` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h; calls softfloat_countLeadingZeros64, softfloat_roundPackToF64 |
| `source/s_normSubnormalF32Sig.c` → `core/softfloat/s_normSubnormalF32Sig.c` | `6063e5c0ee98527c1a5d728195d11ef5ccd9077515812b0380a2f1953244c861` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h, internals.h; calls softfloat_countLeadingZeros32 |
| `source/s_normSubnormalF64Sig.c` → `core/softfloat/s_normSubnormalF64Sig.c` | `be1efe0196eacce551f117e58f5d2bc053a4a2897f37fa5805b23e7d4e00de95` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h, internals.h; calls softfloat_countLeadingZeros64 |
| `source/s_roundPackToF32.c` → `core/softfloat/s_roundPackToF32.c` | `aac61a425bed4e973c9e0e1fb254f74cb7c04602131509ed98513269da83e45a` | Copyright 2011, 2012, 2013, 2014, 2015, 2017 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_raiseFlags, softfloat_shiftRightJam32; reads ambient softfloat_roundingMode, softfloat_detectTininess, softfloat_exceptionFlags (to be replaced with explicit context fields) |
| `source/s_roundPackToF64.c` → `core/softfloat/s_roundPackToF64.c` | `e549468a71417c8fefd927d582ec8481e2d6f595c406a4db5efff9b462f8e2c8` | Copyright 2011, 2012, 2013, 2014, 2015, 2017 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, softfloat.h; calls softfloat_raiseFlags, softfloat_shiftRightJam64; reads ambient softfloat_roundingMode, softfloat_detectTininess, softfloat_exceptionFlags (to be replaced with explicit context fields) |
| `source/s_roundToI32.c` → `core/softfloat/s_roundToI32.c` | `9a64993647fe9cd9031d5a34d0caf86f48ba4b86b6c9bdecf3602d318fbc6401` | Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_raiseFlags; reads ambient softfloat_exceptionFlags (to be replaced with explicit context fields) |
| `source/s_roundToI64.c` → `core/softfloat/s_roundToI64.c` | `19008822ceff3ef5e46a770e5ffccb1953a8377c588001ca5da9dc82d1c44d2a` | Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_raiseFlags; reads ambient softfloat_exceptionFlags (to be replaced with explicit context fields) |
| `source/s_shiftRightJam32.c` → `core/softfloat/s_shiftRightJam32.c` | `96dee4db4ed95ffe7bd44060f980f0a010003a11c20f5757992dcc1f447e0b62` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h; calls no SoftFloat helper |
| `source/s_shiftRightJam64.c` → `core/softfloat/s_shiftRightJam64.c` | `ea54abfce241315f78b69893f6d7b20ab767e5fe8ade6b107245d8dc0745630f` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h; calls no SoftFloat helper |
| `source/s_shiftRightJam64Extra.c` → `core/softfloat/s_shiftRightJam64Extra.c` | `5371094c6e887cde72b34336c0246bfac9984d567376a4e18e4b79b9ed303dff` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h, primitiveTypes.h; calls no SoftFloat helper |
| `source/s_shortShiftRightJam64.c` → `core/softfloat/s_shortShiftRightJam64.c` | `0fcadc566939b0155100929995ee5234fb40cc146cf7093d6c9c15184aaca00c` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdint.h, platform.h; calls no SoftFloat helper |
| `source/s_subMagsF32.c` → `core/softfloat/s_subMagsF32.c` | `53ca2e9f6f1edc7095133e7c98018d69d1e6cd0d5f5b5c0393c4e656ee9a965c` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_countLeadingZeros32, softfloat_normRoundPackToF32, softfloat_propagateNaNF32UI, softfloat_raiseFlags, softfloat_shiftRightJam32; reads ambient softfloat_roundingMode (to be replaced with explicit context fields) |
| `source/s_subMagsF64.c` → `core/softfloat/s_subMagsF64.c` | `733f17b90486602ff6c67fa3d99166a841b03276c0fcc7da66751d1e8631f01f` | Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of California.  All rights reserved. | stdbool.h, stdint.h, platform.h, internals.h, specialize.h, softfloat.h; calls softfloat_countLeadingZeros64, softfloat_normRoundPackToF64, softfloat_propagateNaNF64UI, softfloat_raiseFlags, softfloat_shiftRightJam64; reads ambient softfloat_roundingMode (to be replaced with explicit context fields) |
| `source/include/internals.h` → `core/softfloat/internals.h` | `3983c618d09b0745d9238997ce40f855944db3c7d8574cc3329b56983a004d86` | Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the University of California.  All rights reserved. | <stdbool.h>, <stdint.h>, primitives.h, softfloat_types.h |
| `source/include/primitiveTypes.h` → `core/softfloat/primitiveTypes.h` | `6b0841b1aa4a9c3a1c0a89e868d5d69592e54360dc8e4ba947f7853243a55dcc` | Copyright 2011, 2012, 2013, 2014 The Regents of the University of California. All rights reserved. | <stdint.h> |
| `source/include/primitives.h` → `core/softfloat/primitives.h` | `f416c1a7a3f84d863ddc089665147b3ea51c62451a49b849c5176cfce402cd1d` | Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the University of California.  All rights reserved. | <stdbool.h>, <stdint.h>, primitiveTypes.h |
| `source/include/softfloat.h` → `core/softfloat/softfloat.h` | `27c5f39c21e10e1e798457cdfada111cc2e440a17acba8804e6a2b0a1d7833fc` | Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the University of California.  All rights reserved. | <stdbool.h>, <stdint.h>, softfloat_types.h; context roundingMode, detectTininess, exceptionFlags, raiseFlags |
| `source/include/softfloat_types.h` → `core/softfloat/softfloat_types.h` | `ec3a7b1e314fcdfe6e0f558199a6f466f31d82e0a3ac9fa8287b0dd54e875cd2` | Copyright 2011, 2012, 2013, 2014, 2015, 2017 The Regents of the University of California.  All rights reserved. | <stdint.h> |

## Closure and adaptation contract

- Build only this selected set with `SOFTFLOAT_FAST_INT64=1`; use two-limb portable integer helpers, never `__int128`, target intrinsics, or host floating point.
- `softfloat_raiseFlags` becomes an authored context-taking helper in `core/t64_softfloat.c`. The listed rounding, tininess, flag, and subnormal helpers receive the same explicit `t64_softfloat_context *` through all selected call edges; no `softfloat_state.c`, globals, TLS, or hidden context.
- The raw seam keeps exception flags sticky in the caller context, and the result reports that accumulated context value. The authored integer NaN policy selects the first NaN operand, quiets signaling NaNs, and raises invalid for signaling inputs. Invalid float-to-integer conversions return the selected signed minimum-integer sentinel and raise invalid.
- These are raw IEEE layer decisions. Later VR4300 COP1 consumers own architectural prechecks, exception routing, destination writes, and FCR31 commit behavior; no COP1 decoding or architectural register effects are added by this closure.
- The two reciprocal-coefficient table files each define two arrays. All four arrays become immutable `static const` data behind authored pure accessors, and their only call paths are binary32/binary64 division and square root. The `primitives.h` copy is pruned to generic portable definitions only. `internals.h` and `softfloat.h` are pruned to selected binary32/binary64 and signed integer conversions. All inactive non-FAST_INT64 branches and unrelated formats are removed and recorded in per-file provenance.
- Each C file and each header has one destination and one `BSD-3-Clause` entry in `provenance/copies.json`, with this upstream path, full commit, per-file notice, adaptation, independent test, and `szTheory` maintenance owner. The earlier tracer provenance path is replaced by the final standalone `core/softfloat/i32_to_f64.c` path before that file is copied.
- Selected-branch preprocessing of all 50 generic C files with `SOFTFLOAT_FAST_INT64=1` found no missing upstream C dependency. The selected functions and `softfloat_countLeadingZeros8` table close over the listed files. Authored `specialize.h` supplies integer-only classification, propagation, and common-NaN conversion for binary32/binary64; `core/t64_softfloat.c` supplies context-owned flag raising. Ambient rounding, tininess, and exception-flag references are rewritten to explicit context accesses. Includes `platform.h` and `specialize.h` are authored. Build and undefined-symbol audits will re-check this closure after adaptation.
- Independent result tests derive expected bits from integer/rational arithmetic and the pinned NEC manual; no SoftFloat-generated vector or other emulator output is used.
- Production integration checks execute every selected wrapper with positive and negative operands, exercise all four rounding modes and all five IEEE flag classes in both formats, distinguish tininess-before/after at the normal/subnormal boundary, and interleave contexts with different modes and flags.
- A build-identity mutation check appended a temporary comment to the selected `f32_add.c` input and reconfigured CMake. The identity changed from `t64-build-v1-3134b7a9ddac6d990413ad9a0a399d6ab4dae7ae576b124258396105a9d2a8ab` to `t64-build-v1-21f7ddc2eb494557f86ff781312e3c56b7145f59b67be8d645aa105378e470ee`; restoring the exact source restored the original identity. The temporary edit and backup were removed.
- For the strict compiler and source/object lint, the authored platform header now requires the build's explicit `SOFTFLOAT_FAST_INT64=1` define. This keeps CMake and lint compilation on the same portable limb implementation. The two-limb multiply and jam-shift files also include the selected primitive prototype header, and the copied arithmetic files use explicit narrowing/sign casts only where range checks or bit extraction establish the destination width. Nonsemantic trailing spaces and redundant blank lines at EOF were removed from copied files to satisfy the repository's normal whitespace check.

## Independently Derived Edge Matrix

### Task 1: signed W/L upper-bound slice

These expectations were written down before running the production wrapper. The
inputs are exact powers of two, so their bit decomposition gives an exact
integer independent of any floating-point implementation:

| Format / operation | Input bits and decomposition | Integer interval and result | Expected flags |
|---|---|---|---|
| binary32 `F32_TO_I32` | `0x4f000000`: sign 0, exponent field 158, unbiased exponent 31, significand 1; value `1 × 2^31 = 2147483648` | `INT32_MAX = 2147483647 < 2147483648`; outside the signed W interval. The raw invalid-conversion contract returns `INT32_MIN`, sign-extended to `0xffffffff80000000`. | invalid; no inexact because the input is an exact integer |
| binary64 `F64_TO_I64` | `0x43e0000000000000`: sign 0, exponent field 1086, unbiased exponent 63, significand 1; value `1 × 2^63 = 9223372036854775808` | `INT64_MAX = 9223372036854775807 < 9223372036854775808`; outside the signed L interval. The raw invalid-conversion contract returns `INT64_MIN`, `0x8000000000000000`. | invalid; no inexact because the input is an exact integer |

The same boundary is evaluated for RM0–RM3. Since the values are exact
integers, rounding does not change the range classification or result. Each
boundary operation gets a fresh flag state. An exact binary64 `1 + 2 = 3`
operation is interleaved on a second context configured with a different
rounding mode, before-tininess policy, and preexisting divide-by-zero flag;
the expected result and sticky flags are derived from those inputs, while the
first context's mode, tininess, and invalid flag are checked unchanged.

### Task 2: complete raw edge matrix

The raw test table has 126 uniquely named vector families; each family is run
through `t64_softfloat_execute` under RM0 through RM3 with a fresh context and
zero initial flags. The table stores format, operation, operands, category,
expected-source class, four expected result bit patterns, and four expected
flag bytes. The audit table below is generated from those authored constants
and maps every family to its categories, source method, and per-mode result.
No result was harvested from the wrapper, host floating point, or another
emulator. A standard-library integer square-root check was used only for the
integer midpoint comparison documented below.

The test-side inventory checks the exact 126-family count, unique family IDs,
all 26 operation IDs in each rounding mode, every required category in each
format and rounding mode, all three expected-source classes in each format
and mode, the five IEEE flag bits in each format, and rounding-sensitive
families for every add/subtract/multiply/divide/square-root kernel in both
formats. Removing a required row or its category/source metadata therefore
fails the existing raw test selection. The explicit context-isolation test
still interleaves different rounding/tininess settings and sticky flags; the
before/after tininess discriminator below remains unchanged.

#### Independent derivation rules

- **Exact baseline rows:** Decode the hex sign/exponent/fraction fields and
  use exact integer or dyadic arithmetic: `1+2=3`, `3−1=2`, `1.5×2=3`,
  `6/2=3`, `sqrt(4)=2`; comparisons use exact equality/order. `1.5` converts
  exactly between the two binary formats; integer 2/3 encode exactly in the
  listed formats. The result arrays repeat under all RMs because none of
  these exact operations has a discarded bit.
- **Addition tie and neighbors:** At 1, binary32 ULP is `2^-23`; the
  halfway addend is `2^-24` (`0x33800000`). Its predecessor and successor
  (`0x337fffff`, `0x33800001`) are on opposite sides of the exact midpoint.
  For binary64 the corresponding addend is `2^-53` (`0x3ca0000000000000`),
  bracketed by `0x3c9fffffffffffff` and `0x3ca0000000000001`. The rounding
  outputs follow comparison of the exact sum with 1 and its adjacent
  encodings; nearest-even selects the even significand at the exact tie.
- **Cross-format tie and neighbors:** `1+2^-24` lies exactly halfway between
  binary32 1 and its successor. The binary64 encodings immediately below and
  above that value are one integer bit-pattern step away. Negating the exact
  rational reverses directed-rounding choices while preserving nearest-even.
- **Integer-to-float ties:** `2^24+1` is midway between adjacent binary32
  values `2^24` and `2^24+2`; `2^53+1` is midway between adjacent binary64
  values `2^53` and `2^53+2`. For negative inputs, toward-negative selects
  the more-negative endpoint and toward-positive selects toward zero. The
  parity of the lower significand establishes the nearest-even endpoint.
- **Signed zeros:** The sign bit alone distinguishes the zero inputs. Adding
  like-signed zeros preserves that sign. Adding opposite-signed zeros gives
  negative zero only under round-toward-negative; the other modes give
  positive zero.
- **Subnormal/normal boundaries:** Binary32 min-subnormal is `2^-149`,
  max-subnormal is `(2^23−1)×2^-149`, and min-normal is `2^-126`.
  Binary64 uses `2^-1074`, `(2^52−1)×2^-1074`, and `2^-1022`. Thus
  min-subnormal + min-subnormal is the next subnormal; max-subnormal +
  min-subnormal is min-normal; and min-normal − max-subnormal is
  min-subnormal. Multiplication by exactly 1 preserves the min-normal and
  max-normal encodings.
- **Infinity and NaN rows:** Exponent/fraction fields classify the special
  operands. Infinity signs follow the sign xor for multiplication. The
  authored raw NaN policy already audited above selects and quiets the first
  NaN payload; signaling inputs raise invalid. Quiet-NaN equality is false
  without invalid; ordered comparisons with NaNs return false and raise
  invalid under the selected raw wrapper contract.
- **Signed W/L limits:** Compare each decoded exact integer with
  `[-2^31,2^31−1]` or `[-2^63,2^63−1]`. Binary32 W uses exact `−2^31`,
  predecessor `2^31−128`, and the next representable value `2^31`; binary32
  L uses `−2^63`, predecessor `2^63−2^39` (`0x5effffff`), and `2^63`.
  The predecessor is `(2^24−1)×2^39 = 2^63−2^39`; this is the corrected
  expectation recorded after an initial exponent-step miscalculation.
  Binary64 W includes exact endpoints and the adjacent out-of-range
  integers; binary64 L's largest in-range value is `2^63−1024`, followed by
  `2^63`. Values below the signed minimum are also tested. Invalid exact
  conversions return the raw signed-minimum sentinel and set invalid only.
- **Overflow/underflow and flags:** Largest finite times 2 exceeds the
  finite range; the output arrays select infinity or largest finite by sign
  and directed mode and set overflow|inexact. Min-subnormal times `1/2` is
  exactly halfway between zero and min-subnormal; nearest-even/toward-zero
  choose signed zero, while directed modes choose the adjacent value as
  required by sign, setting underflow|inexact. The existing distinct
  before/after tininess vectors continue to check tininess independently of
  the final encoded result. `±1/0` gives signed infinity with divide-by-zero;
  `0/0`, infinity cancellation, infinity×zero, and sqrt of a negative finite
  value are invalid and return the authored default quiet NaN.
- **Non-exact arithmetic kernels:** For subtract, `1−2^-25` in binary32
  (and `1−2^-54` in binary64) is the midpoint between 1 and its predecessor;
  neighboring encoded subtrahends bracket that midpoint. For multiply,
  `(1+2^-23)^2 = 1+2^-22+2^-46` (binary32) and
  `(1+2^-52)^2 = 1+2^-51+2^-104` (binary64), so the exact product is just
  above the second successor and below the third; negative operands reverse
  directed choices. Division uses exact `1/3`; its adjacent encodings are
  selected by integer cross-multiplication against 1, with sign reversing
  directed bounds. Square root uses integer midpoint comparisons: for
  binary32, `n=floor(sqrt(2×2^47))=0xb504f3` and
  `4×2^47−(2n+1)^2=−28184177`, so sqrt(2) is below the midpoint and nearest
  selects `0x3fb504f3`; for binary64, `n=floor(sqrt(2×2^105))=
  0x16a09e667f3bcc` and `4×2^105−(2n+1)^2=3292739303401103`, so it is
  above the midpoint and nearest selects `0x3ff6a09e667f3bcd`. Directed
  modes select the corresponding adjacent lower/upper encoding. These
  integer comparisons, not the library result, establish the expected root
  rows. An initial guessed sqrt table and a misread of its first mismatch
  output were discarded after this derivation; the corrected table passed,
  and no production sqrt defect was observed.

#### Per-family category, source, and expected-result inventory

Every table cell lists the result bits and accumulated flags for that RM.
`bits` means direct bit decomposition/classification; `rational` means exact
dyadic/rational arithmetic or interval comparison; `integer` means signed
integer midpoint or range comparison. The named families are also the
stable omission-control IDs checked by the test inventory.

<!-- PLAN17-MATRIX-INVENTORY -->
| Family ID | Format / operation | Category | Source | RM0 bits / flags | RM1 bits / flags | RM2 bits / flags | RM3 bits / flags |
|---|---|---|---|---|---|---|---|
| `base-i32-f64` | binary64 `I32_TO_F64` | BASE | bits | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u |
| `base-f32-add` | binary32 `F32_ADD` | BASE | bits | 0x40400000 / 0u | 0x40400000 / 0u | 0x40400000 / 0u | 0x40400000 / 0u |
| `base-f32-sub` | binary32 `F32_SUB` | BASE | bits | 0x40000000 / 0u | 0x40000000 / 0u | 0x40000000 / 0u | 0x40000000 / 0u |
| `base-f32-mul` | binary32 `F32_MUL` | BASE | bits | 0x40400000 / 0u | 0x40400000 / 0u | 0x40400000 / 0u | 0x40400000 / 0u |
| `base-f32-div` | binary32 `F32_DIV` | BASE | rational | 0x40400000 / 0u | 0x40400000 / 0u | 0x40400000 / 0u | 0x40400000 / 0u |
| `base-f32-sqrt` | binary32 `F32_SQRT` | BASE | bits | 0x40000000 / 0u | 0x40000000 / 0u | 0x40000000 / 0u | 0x40000000 / 0u |
| `base-f32-eq` | binary32 `F32_EQ` | BASE | bits | 1u / 0u | 1u / 0u | 1u / 0u | 1u / 0u |
| `base-f32-le` | binary32 `F32_LE` | BASE | bits | 1u / 0u | 1u / 0u | 1u / 0u | 1u / 0u |
| `base-f32-lt` | binary32 `F32_LT` | BASE | bits | 1u / 0u | 1u / 0u | 1u / 0u | 1u / 0u |
| `base-f32-f64` | binary32 `F32_TO_F64` | BASE | bits | 0x3ff8000000000000 / 0u | 0x3ff8000000000000 / 0u | 0x3ff8000000000000 / 0u | 0x3ff8000000000000 / 0u |
| `base-f32-i32` | binary32 `F32_TO_I32` | BASE | bits | 2u / 0u | 2u / 0u | 2u / 0u | 2u / 0u |
| `base-f32-i64` | binary32 `F32_TO_I64` | BASE | bits | 2u / 0u | 2u / 0u | 2u / 0u | 2u / 0u |
| `base-f64-add` | binary64 `F64_ADD` | BASE | bits | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u |
| `base-f64-sub` | binary64 `F64_SUB` | BASE | bits | 0x4000000000000000 / 0u | 0x4000000000000000 / 0u | 0x4000000000000000 / 0u | 0x4000000000000000 / 0u |
| `base-f64-mul` | binary64 `F64_MUL` | BASE | bits | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u |
| `base-f64-div` | binary64 `F64_DIV` | BASE | rational | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u |
| `base-f64-sqrt` | binary64 `F64_SQRT` | BASE | bits | 0x4000000000000000 / 0u | 0x4000000000000000 / 0u | 0x4000000000000000 / 0u | 0x4000000000000000 / 0u |
| `base-f64-eq` | binary64 `F64_EQ` | BASE | bits | 1u / 0u | 1u / 0u | 1u / 0u | 1u / 0u |
| `base-f64-le` | binary64 `F64_LE` | BASE | bits | 1u / 0u | 1u / 0u | 1u / 0u | 1u / 0u |
| `base-f64-lt` | binary64 `F64_LT` | BASE | bits | 1u / 0u | 1u / 0u | 1u / 0u | 1u / 0u |
| `base-f64-f32` | binary64 `F64_TO_F32` | BASE | bits | 0x3fc00000 / 0u | 0x3fc00000 / 0u | 0x3fc00000 / 0u | 0x3fc00000 / 0u |
| `base-f64-i32` | binary64 `F64_TO_I32` | BASE | bits | 2u / 0u | 2u / 0u | 2u / 0u | 2u / 0u |
| `base-f64-i64` | binary64 `F64_TO_I64` | BASE | bits | 2u / 0u | 2u / 0u | 2u / 0u | 2u / 0u |
| `base-i32-f32` | binary32 `I32_TO_F32` | BASE | bits | 0x40400000 / 0u | 0x40400000 / 0u | 0x40400000 / 0u | 0x40400000 / 0u |
| `base-i64-f32` | binary32 `I64_TO_F32` | BASE | bits | 0x40400000 / 0u | 0x40400000 / 0u | 0x40400000 / 0u | 0x40400000 / 0u |
| `base-i64-f64` | binary64 `I64_TO_F64` | BASE | bits | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u | 0x4008000000000000 / 0u |
| `f32-add-half-ulp-positive` | binary32 `F32_ADD` | TIE, INEXACT, ROUNDING_SENSITIVE | rational | 0x3f800000 / 1u | 0x3f800000 / 1u | 0x3f800000 / 1u | 0x3f800001 / 1u |
| `f32-add-half-ulp-below` | binary32 `F32_ADD` | NEIGHBOR, INEXACT | rational | 0x3f800000 / 1u | 0x3f800000 / 1u | 0x3f800000 / 1u | 0x3f800001 / 1u |
| `f32-add-half-ulp-above` | binary32 `F32_ADD` | NEIGHBOR, INEXACT | rational | 0x3f800001 / 1u | 0x3f800000 / 1u | 0x3f800000 / 1u | 0x3f800001 / 1u |
| `f64-add-half-ulp-positive` | binary64 `F64_ADD` | TIE, INEXACT, ROUNDING_SENSITIVE | rational | 0x3ff0000000000000 / 1u | 0x3ff0000000000000 / 1u | 0x3ff0000000000000 / 1u | 0x3ff0000000000001 / 1u |
| `f64-add-half-ulp-below` | binary64 `F64_ADD` | NEIGHBOR, INEXACT | rational | 0x3ff0000000000000 / 1u | 0x3ff0000000000000 / 1u | 0x3ff0000000000000 / 1u | 0x3ff0000000000001 / 1u |
| `f64-add-half-ulp-above` | binary64 `F64_ADD` | NEIGHBOR, INEXACT | rational | 0x3ff0000000000001 / 1u | 0x3ff0000000000000 / 1u | 0x3ff0000000000000 / 1u | 0x3ff0000000000001 / 1u |
| `f64-to-f32-halfway-positive` | binary64 `F64_TO_F32` | TIE, INEXACT | rational | 0x3f800000 / 1u | 0x3f800000 / 1u | 0x3f800000 / 1u | 0x3f800001 / 1u |
| `f64-to-f32-neighbor-below` | binary64 `F64_TO_F32` | NEIGHBOR, INEXACT | rational | 0x3f800000 / 1u | 0x3f800000 / 1u | 0x3f800000 / 1u | 0x3f800001 / 1u |
| `f64-to-f32-neighbor-above` | binary64 `F64_TO_F32` | NEIGHBOR, INEXACT | rational | 0x3f800001 / 1u | 0x3f800000 / 1u | 0x3f800000 / 1u | 0x3f800001 / 1u |
| `f64-to-f32-halfway-negative` | binary64 `F64_TO_F32` | TIE, INEXACT | rational | 0xbf800000 / 1u | 0xbf800000 / 1u | 0xbf800001 / 1u | 0xbf800000 / 1u |
| `f64-to-f32-neighbor-negative-below` | binary64 `F64_TO_F32` | NEIGHBOR, INEXACT | rational | 0xbf800000 / 1u | 0xbf800000 / 1u | 0xbf800001 / 1u | 0xbf800000 / 1u |
| `f64-to-f32-neighbor-negative-above` | binary64 `F64_TO_F32` | NEIGHBOR, INEXACT | rational | 0xbf800001 / 1u | 0xbf800000 / 1u | 0xbf800001 / 1u | 0xbf800000 / 1u |
| `i32-to-f32-integer-tie-positive` | binary32 `I32_TO_F32` | TIE, INEXACT | integer | 0x4b800000 / 1u | 0x4b800000 / 1u | 0x4b800000 / 1u | 0x4b800001 / 1u |
| `i32-to-f32-integer-tie-negative` | binary32 `I32_TO_F32` | TIE, INEXACT | integer | 0xcb800000 / 1u | 0xcb800000 / 1u | 0xcb800001 / 1u | 0xcb800000 / 1u |
| `i64-to-f32-integer-tie-positive` | binary32 `I64_TO_F32` | TIE, INEXACT | integer | 0x4b800000 / 1u | 0x4b800000 / 1u | 0x4b800000 / 1u | 0x4b800001 / 1u |
| `i64-to-f32-integer-tie-negative` | binary32 `I64_TO_F32` | TIE, INEXACT | integer | 0xcb800000 / 1u | 0xcb800000 / 1u | 0xcb800001 / 1u | 0xcb800000 / 1u |
| `i64-to-f64-integer-tie-positive` | binary64 `I64_TO_F64` | TIE, INEXACT | integer | 0x4340000000000000 / 1u | 0x4340000000000000 / 1u | 0x4340000000000000 / 1u | 0x4340000000000001 / 1u |
| `i64-to-f64-integer-tie-negative` | binary64 `I64_TO_F64` | TIE, INEXACT | integer | 0xc340000000000000 / 1u | 0xc340000000000000 / 1u | 0xc340000000000001 / 1u | 0xc340000000000000 / 1u |
| `f32-zero-positive` | binary32 `F32_ADD` | ZERO_SIGN | bits | 0u / 0u | 0u / 0u | 0u / 0u | 0u / 0u |
| `f32-zero-negative` | binary32 `F32_ADD` | ZERO_SIGN | bits | 0x80000000 / 0u | 0x80000000 / 0u | 0x80000000 / 0u | 0x80000000 / 0u |
| `f32-zero-opposite` | binary32 `F32_ADD` | ZERO_SIGN | bits | 0u / 0u | 0u / 0u | 0x80000000 / 0u | 0u / 0u |
| `f64-zero-positive` | binary64 `F64_ADD` | ZERO_SIGN | bits | 0u / 0u | 0u / 0u | 0u / 0u | 0u / 0u |
| `f64-zero-negative` | binary64 `F64_ADD` | ZERO_SIGN | bits | 0x8000000000000000 / 0u | 0x8000000000000000 / 0u | 0x8000000000000000 / 0u | 0x8000000000000000 / 0u |
| `f64-zero-opposite` | binary64 `F64_ADD` | ZERO_SIGN | bits | 0u / 0u | 0u / 0u | 0x8000000000000000 / 0u | 0u / 0u |
| `f32-minsub-plus-minsub` | binary32 `F32_ADD` | SUBNORMAL | rational | 2u / 0u | 2u / 0u | 2u / 0u | 2u / 0u |
| `f32-maxsub-plus-minsub` | binary32 `F32_ADD` | SUBNORMAL | rational | 0x00800000 / 0u | 0x00800000 / 0u | 0x00800000 / 0u | 0x00800000 / 0u |
| `f32-minnormal-minus-maxsub` | binary32 `F32_SUB` | SUBNORMAL | rational | 1u / 0u | 1u / 0u | 1u / 0u | 1u / 0u |
| `f32-minnormal-times-one` | binary32 `F32_MUL` | SUBNORMAL | bits | 0x00800000 / 0u | 0x00800000 / 0u | 0x00800000 / 0u | 0x00800000 / 0u |
| `f32-maxnormal-times-one` | binary32 `F32_MUL` | SUBNORMAL | bits | 0x7f7fffff / 0u | 0x7f7fffff / 0u | 0x7f7fffff / 0u | 0x7f7fffff / 0u |
| `f64-minsub-plus-minsub` | binary64 `F64_ADD` | SUBNORMAL | rational | 2u / 0u | 2u / 0u | 2u / 0u | 2u / 0u |
| `f64-maxsub-plus-minsub` | binary64 `F64_ADD` | SUBNORMAL | rational | 0x0010000000000000 / 0u | 0x0010000000000000 / 0u | 0x0010000000000000 / 0u | 0x0010000000000000 / 0u |
| `f64-minnormal-minus-maxsub` | binary64 `F64_SUB` | SUBNORMAL | rational | 1u / 0u | 1u / 0u | 1u / 0u | 1u / 0u |
| `f64-minnormal-times-one` | binary64 `F64_MUL` | SUBNORMAL | bits | 0x0010000000000000 / 0u | 0x0010000000000000 / 0u | 0x0010000000000000 / 0u | 0x0010000000000000 / 0u |
| `f64-maxnormal-times-one` | binary64 `F64_MUL` | SUBNORMAL | bits | 0x7fefffffffffffff / 0u | 0x7fefffffffffffff / 0u | 0x7fefffffffffffff / 0u | 0x7fefffffffffffff / 0u |
| `f32-positive-infinity-plus-one` | binary32 `F32_ADD` | INFINITY | bits | 0x7f800000 / 0u | 0x7f800000 / 0u | 0x7f800000 / 0u | 0x7f800000 / 0u |
| `f32-negative-infinity-times-two` | binary32 `F32_MUL` | INFINITY | bits | 0xff800000 / 0u | 0xff800000 / 0u | 0xff800000 / 0u | 0xff800000 / 0u |
| `f64-positive-infinity-plus-one` | binary64 `F64_ADD` | INFINITY | bits | 0x7ff0000000000000 / 0u | 0x7ff0000000000000 / 0u | 0x7ff0000000000000 / 0u | 0x7ff0000000000000 / 0u |
| `f64-negative-infinity-times-two` | binary64 `F64_MUL` | INFINITY | bits | 0xfff0000000000000 / 0u | 0xfff0000000000000 / 0u | 0xfff0000000000000 / 0u | 0xfff0000000000000 / 0u |
| `f32-qnan-propagation` | binary32 `F32_ADD` | QNAN | bits | 0x7fc12345 / 0u | 0x7fc12345 / 0u | 0x7fc12345 / 0u | 0x7fc12345 / 0u |
| `f32-qnan-eq` | binary32 `F32_EQ` | QNAN | bits | 0u / 0u | 0u / 0u | 0u / 0u | 0u / 0u |
| `f32-qnan-le-invalid` | binary32 `F32_LE` | QNAN, INVALID | bits | 0u / T64_SOFTFLOAT_FLAG_INVALID | 0u / T64_SOFTFLOAT_FLAG_INVALID | 0u / T64_SOFTFLOAT_FLAG_INVALID | 0u / T64_SOFTFLOAT_FLAG_INVALID |
| `f32-snan-propagation` | binary32 `F32_ADD` | SNAN | bits | 0x7fc12345 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc12345 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc12345 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc12345 / T64_SOFTFLOAT_FLAG_INVALID |
| `f32-snan-lt-invalid` | binary32 `F32_LT` | SNAN, INVALID | bits | 0u / T64_SOFTFLOAT_FLAG_INVALID | 0u / T64_SOFTFLOAT_FLAG_INVALID | 0u / T64_SOFTFLOAT_FLAG_INVALID | 0u / T64_SOFTFLOAT_FLAG_INVALID |
| `f64-qnan-propagation` | binary64 `F64_ADD` | QNAN | bits | 0x7ff8123456789abc / 0u | 0x7ff8123456789abc / 0u | 0x7ff8123456789abc / 0u | 0x7ff8123456789abc / 0u |
| `f64-qnan-eq` | binary64 `F64_EQ` | QNAN | bits | 0u / 0u | 0u / 0u | 0u / 0u | 0u / 0u |
| `f64-qnan-le-invalid` | binary64 `F64_LE` | QNAN, INVALID | bits | 0u / T64_SOFTFLOAT_FLAG_INVALID | 0u / T64_SOFTFLOAT_FLAG_INVALID | 0u / T64_SOFTFLOAT_FLAG_INVALID | 0u / T64_SOFTFLOAT_FLAG_INVALID |
| `f64-snan-propagation` | binary64 `F64_ADD` | SNAN | bits | 0x7ff8123456789abc / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8123456789abc / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8123456789abc / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8123456789abc / T64_SOFTFLOAT_FLAG_INVALID |
| `f64-snan-lt-invalid` | binary64 `F64_LT` | SNAN, INVALID | bits | 0u / T64_SOFTFLOAT_FLAG_INVALID | 0u / T64_SOFTFLOAT_FLAG_INVALID | 0u / T64_SOFTFLOAT_FLAG_INVALID | 0u / T64_SOFTFLOAT_FLAG_INVALID |
| `f32-i32-min-endpoint` | binary32 `F32_TO_I32` | RANGE | integer | 0xffffffff80000000 / 0u | 0xffffffff80000000 / 0u | 0xffffffff80000000 / 0u | 0xffffffff80000000 / 0u |
| `f32-i32-below-min` | binary32 `F32_TO_I32` | RANGE, INVALID | integer | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f32-i32-max-in-range` | binary32 `F32_TO_I32` | RANGE | integer | 0x000000007fffff80 / 0u | 0x000000007fffff80 / 0u | 0x000000007fffff80 / 0u | 0x000000007fffff80 / 0u |
| `f32-i32-above-max` | binary32 `F32_TO_I32` | RANGE, INVALID | integer | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f32-i64-min-endpoint` | binary32 `F32_TO_I64` | RANGE | integer | 0x8000000000000000 / 0u | 0x8000000000000000 / 0u | 0x8000000000000000 / 0u | 0x8000000000000000 / 0u |
| `f32-i64-below-min` | binary32 `F32_TO_I64` | RANGE, INVALID | integer | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f32-i64-max-in-range` | binary32 `F32_TO_I64` | RANGE | integer | 0x7fffff8000000000 / 0u | 0x7fffff8000000000 / 0u | 0x7fffff8000000000 / 0u | 0x7fffff8000000000 / 0u |
| `f32-i64-above-max` | binary32 `F32_TO_I64` | RANGE, INVALID | integer | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f64-i32-min-endpoint` | binary64 `F64_TO_I32` | RANGE | integer | 0xffffffff80000000 / 0u | 0xffffffff80000000 / 0u | 0xffffffff80000000 / 0u | 0xffffffff80000000 / 0u |
| `f64-i32-below-min` | binary64 `F64_TO_I32` | RANGE, INVALID | integer | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f64-i32-max-endpoint` | binary64 `F64_TO_I32` | RANGE | integer | 0x000000007fffffff / 0u | 0x000000007fffffff / 0u | 0x000000007fffffff / 0u | 0x000000007fffffff / 0u |
| `f64-i32-above-max` | binary64 `F64_TO_I32` | RANGE, INVALID | integer | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID | 0xffffffff80000000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f64-i64-min-endpoint` | binary64 `F64_TO_I64` | RANGE | integer | 0x8000000000000000 / 0u | 0x8000000000000000 / 0u | 0x8000000000000000 / 0u | 0x8000000000000000 / 0u |
| `f64-i64-below-min` | binary64 `F64_TO_I64` | RANGE, INVALID | integer | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f64-i64-max-endpoint` | binary64 `F64_TO_I64` | RANGE | integer | 0x7ffffffffffffc00 / 0u | 0x7ffffffffffffc00 / 0u | 0x7ffffffffffffc00 / 0u | 0x7ffffffffffffc00 / 0u |
| `f64-i64-above-max` | binary64 `F64_TO_I64` | RANGE, INVALID | integer | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x8000000000000000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f32-overflow-positive` | binary32 `F32_MUL` | OVERFLOW, INEXACT | rational | 0x7f800000 / 5u | 0x7f7fffff / 5u | 0x7f7fffff / 5u | 0x7f800000 / 5u |
| `f32-overflow-negative` | binary32 `F32_MUL` | OVERFLOW, INEXACT | rational | 0xff800000 / 5u | 0xff7fffff / 5u | 0xff800000 / 5u | 0xff7fffff / 5u |
| `f64-overflow-positive` | binary64 `F64_MUL` | OVERFLOW, INEXACT | rational | 0x7ff0000000000000 / 5u | 0x7fefffffffffffff / 5u | 0x7fefffffffffffff / 5u | 0x7ff0000000000000 / 5u |
| `f64-overflow-negative` | binary64 `F64_MUL` | OVERFLOW, INEXACT | rational | 0xfff0000000000000 / 5u | 0xffefffffffffffff / 5u | 0xfff0000000000000 / 5u | 0xffefffffffffffff / 5u |
| `f32-underflow-positive` | binary32 `F32_MUL` | UNDERFLOW, INEXACT | rational | 0u / 3u | 0u / 3u | 0u / 3u | 1u / 3u |
| `f32-underflow-negative` | binary32 `F32_MUL` | UNDERFLOW, INEXACT | rational | 0x80000000 / 3u | 0x80000000 / 3u | 0x80000001 / 3u | 0x80000000 / 3u |
| `f64-underflow-positive` | binary64 `F64_MUL` | UNDERFLOW, INEXACT | rational | 0u / 3u | 0u / 3u | 0u / 3u | 1u / 3u |
| `f64-underflow-negative` | binary64 `F64_MUL` | UNDERFLOW, INEXACT | rational | 0x8000000000000000 / 3u | 0x8000000000000000 / 3u | 0x8000000000000001 / 3u | 0x8000000000000000 / 3u |
| `f32-divzero-positive` | binary32 `F32_DIV` | DIVZERO | rational | 0x7f800000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO | 0x7f800000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO | 0x7f800000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO | 0x7f800000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO |
| `f32-divzero-negative` | binary32 `F32_DIV` | DIVZERO | rational | 0xff800000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO | 0xff800000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO | 0xff800000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO | 0xff800000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO |
| `f64-divzero-positive` | binary64 `F64_DIV` | DIVZERO | rational | 0x7ff0000000000000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO | 0x7ff0000000000000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO | 0x7ff0000000000000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO | 0x7ff0000000000000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO |
| `f64-divzero-negative` | binary64 `F64_DIV` | DIVZERO | rational | 0xfff0000000000000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO | 0xfff0000000000000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO | 0xfff0000000000000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO | 0xfff0000000000000 / T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO |
| `f32-invalid-zero-over-zero` | binary32 `F32_DIV` | INVALID | rational | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f32-invalid-infinity-cancel` | binary32 `F32_SUB` | INVALID | bits | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f32-invalid-infinity-times-zero` | binary32 `F32_MUL` | INVALID | bits | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f32-invalid-negative-sqrt` | binary32 `F32_SQRT` | INVALID | bits | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7fc00000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f64-invalid-zero-over-zero` | binary64 `F64_DIV` | INVALID | rational | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f64-invalid-infinity-cancel` | binary64 `F64_SUB` | INVALID | bits | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f64-invalid-infinity-times-zero` | binary64 `F64_MUL` | INVALID | bits | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f64-invalid-negative-sqrt` | binary64 `F64_SQRT` | INVALID | bits | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID | 0x7ff8000000000000 / T64_SOFTFLOAT_FLAG_INVALID |
| `f32-sub-half-ulp-below-one` | binary32 `F32_SUB` | NEIGHBOR, INEXACT, ROUNDING_SENSITIVE | rational | 0x3f800000 / 1u | 0x3f7fffff / 1u | 0x3f7fffff / 1u | 0x3f800000 / 1u |
| `f32-sub-half-ulp-at-one` | binary32 `F32_SUB` | TIE, INEXACT, ROUNDING_SENSITIVE | rational | 0x3f800000 / 1u | 0x3f7fffff / 1u | 0x3f7fffff / 1u | 0x3f800000 / 1u |
| `f32-sub-half-ulp-above-one` | binary32 `F32_SUB` | NEIGHBOR, INEXACT, ROUNDING_SENSITIVE | rational | 0x3f7fffff / 1u | 0x3f7fffff / 1u | 0x3f7fffff / 1u | 0x3f800000 / 1u |
| `f64-sub-half-ulp-below-one` | binary64 `F64_SUB` | NEIGHBOR, INEXACT, ROUNDING_SENSITIVE | rational | 0x3ff0000000000000 / 1u | 0x3fefffffffffffff / 1u | 0x3fefffffffffffff / 1u | 0x3ff0000000000000 / 1u |
| `f64-sub-half-ulp-at-one` | binary64 `F64_SUB` | TIE, INEXACT, ROUNDING_SENSITIVE | rational | 0x3ff0000000000000 / 1u | 0x3fefffffffffffff / 1u | 0x3fefffffffffffff / 1u | 0x3ff0000000000000 / 1u |
| `f64-sub-half-ulp-above-one` | binary64 `F64_SUB` | NEIGHBOR, INEXACT, ROUNDING_SENSITIVE | rational | 0x3fefffffffffffff / 1u | 0x3fefffffffffffff / 1u | 0x3fefffffffffffff / 1u | 0x3ff0000000000000 / 1u |
| `f32-mul-near-one-positive` | binary32 `F32_MUL` | NEIGHBOR, INEXACT, ROUNDING_SENSITIVE | rational | 0x3f800002 / 1u | 0x3f800002 / 1u | 0x3f800002 / 1u | 0x3f800003 / 1u |
| `f32-mul-near-one-negative` | binary32 `F32_MUL` | NEIGHBOR, INEXACT, ROUNDING_SENSITIVE | rational | 0xbf800002 / 1u | 0xbf800002 / 1u | 0xbf800003 / 1u | 0xbf800002 / 1u |
| `f64-mul-near-one-positive` | binary64 `F64_MUL` | NEIGHBOR, INEXACT, ROUNDING_SENSITIVE | rational | 0x3ff0000000000002 / 1u | 0x3ff0000000000002 / 1u | 0x3ff0000000000002 / 1u | 0x3ff0000000000003 / 1u |
| `f64-mul-near-one-negative` | binary64 `F64_MUL` | NEIGHBOR, INEXACT, ROUNDING_SENSITIVE | rational | 0xbff0000000000002 / 1u | 0xbff0000000000002 / 1u | 0xbff0000000000003 / 1u | 0xbff0000000000002 / 1u |
| `f32-div-one-third-positive` | binary32 `F32_DIV` | INEXACT, ROUNDING_SENSITIVE | rational | 0x3eaaaaab / 1u | 0x3eaaaaaa / 1u | 0x3eaaaaaa / 1u | 0x3eaaaaab / 1u |
| `f32-div-one-third-negative` | binary32 `F32_DIV` | INEXACT, ROUNDING_SENSITIVE | rational | 0xbeaaaaab / 1u | 0xbeaaaaaa / 1u | 0xbeaaaaab / 1u | 0xbeaaaaaa / 1u |
| `f64-div-one-third-positive` | binary64 `F64_DIV` | INEXACT, ROUNDING_SENSITIVE | rational | 0x3fd5555555555555 / 1u | 0x3fd5555555555555 / 1u | 0x3fd5555555555555 / 1u | 0x3fd5555555555556 / 1u |
| `f64-div-one-third-negative` | binary64 `F64_DIV` | INEXACT, ROUNDING_SENSITIVE | rational | 0xbfd5555555555555 / 1u | 0xbfd5555555555555 / 1u | 0xbfd5555555555556 / 1u | 0xbfd5555555555555 / 1u |
| `f32-sqrt-two` | binary32 `F32_SQRT` | INEXACT, ROUNDING_SENSITIVE | rational | 0x3fb504f3 / 1u | 0x3fb504f3 / 1u | 0x3fb504f3 / 1u | 0x3fb504f4 / 1u |
| `f64-sqrt-two` | binary64 `F64_SQRT` | INEXACT, ROUNDING_SENSITIVE | rational | 0x3ff6a09e667f3bcd / 1u | 0x3ff6a09e667f3bcc / 1u | 0x3ff6a09e667f3bcc / 1u | 0x3ff6a09e667f3bcd / 1u |


The source paths and hashes above were calculated from the official pinned git objects before copying any additional source into the repository.
