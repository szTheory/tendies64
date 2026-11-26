# Phase 11 installed consumer and freeze candidate qualification

The audited source is the tree of `9b17d8106ce6d1479fd29587931c06136bae66c4` (the Qualification base
recorded in the Phase 11 pin-impact record; it is the head that the final independent audit read in a
scratch clone, `2c53513c0ddf2b1e31e0bbc261f7629ce2268658`, plus the commit of the audit record, a planning
file). The one final native qualification (`python3 -B scripts/qualify-phase11.py --native`, no
rehearsal flag) passed on the head `aa393c2287aaebf817170dfc54394beeabec8ca5` (named H below: the base
plus the commit of the `Qualification base` line, which changes only the pin-impact record) on its first
attempt, and the exact-binding verifier (`python3 -B scripts/qualify-phase11.py --verify RECORD`) accepted
the record against that tree, again after the record's planning commit, and again after this document
and the claim file were committed: the qualifier excludes these two documents from its bound inputs, as
it excludes `.planning/` and `docs/ci-reviews/LOG.md`, so writing them after the record does not
invalidate it. The commit of the two documents is the head pushed to `main` by plan 11-25; its warm
hosted pair is recorded in `docs/ci-reviews/LOG.md` and in the pin-impact Gate-growth ledger. The
bounded claims are the four blocks of [phase11](../claims/phase11.md).

Evidence tier: **unit-tested, authored scope** (every block of the claim file is tier `unit-tested`),
and a **software-interface claim only**. It says what the public C API of the installed package did for
an independent consumer through a clean relocated install, at ABI 0.1, a freeze candidate. Every
expected value is a hand-written literal of the consumer, the independent expectation of the Phase 10
composite fixture (committed from the independent twin oracle script) or the status-class table; the
machine under test did not produce its own expectation. This is not hardware-derived, not
emulator-differential, and uses no commercial software, ROM, firmware, save file or oracle material.
No claim is made here about hardware behavior, about agreement with any console, title or other
emulator, about a final or stable interface (the final ABI bind is the last plan of Phase 12), or
about a release: nothing was released or published as a package, no package channel exists, and no tag
was created (the commits are named by their 40-hex ids).

## Source and build identity

| Item | Value |
| --- | --- |
| audited source (Qualification base) | `9b17d8106ce6d1479fd29587931c06136bae66c4` |
| qualified commit **H** (`commit_sha` of the record) | `aa393c2287aaebf817170dfc54394beeabec8ca5`; the one path between the base and H is the pin-impact record |
| reviewed head of the final audit | `2c53513c0ddf2b1e31e0bbc261f7629ce2268658` (`.planning/phases/11-installed-consumer-and-freeze-candidate/11-REVIEW.md` "FINAL"); the base adds only that record |
| wave bases bound by the record | Wave 0 `f9124ae9fce134f31b645c6f2911a6f27fa41b9a`, Install base `06e31ecab5b84c08996aba79cb3ab6e29bb021cc`, Freeze base `189afa8ff454415870b5a703e9f728e4ae3dc5ea`, Qualification base `9b17d8106ce6d1479fd29587931c06136bae66c4` |
| core build id, equal for the gate archive, the installed archive (built by the fresh-archive lane from `git archive` of H outside the repository) and the installed consumer executable (the claim `build_id` values carry the prefix `0.0.0+`, the untouched `project(VERSION)`) | `t64-build-v1-35e6e1f1d4a1b5fd50b579c0cfb9dd36ac658099d0a207db610006433c1a22d5` |
| runner build id (the in-tree runner of the three-way comparison) | `t64-runner-v1-01828ba678774da36f2e835749585159381793f55904faef00ca8f04e263518c` |
| runner test twin build id | `t64-runner-v1-7868c7674d64ac919be5b1948e43c11182ecb515a0d354924d26584e50b035f1` |
| ASan and UBSan core build id (in-tree sanitizer tree) | `t64-build-v1-689e46bd1805cdede425ec6f8d12aab862680f6b2afb05e9c42da70caa8c8c5f` |
| ABI and package | ABI candidate **0.1** (`T64_ABI_VERSION_MAJOR` 0, `T64_ABI_VERSION_MINOR` 1); package version **0.1.0** with `SameMinorVersion` (the ABI candidate number, not a release version); 0 amendment rows |
| freeze-candidate record `docs/abi/FREEZE-CANDIDATE.md` | SHA-256 `ed04512d57a1ee629d4cfbb89ab8bc32b46870f4fbc2d479c9b47f4bbe8a1f05`; 35 public functions, 39 statuses, 8 stop reasons, 14 structs, 165 members, 190 constants, 8 padding holes |
| golden `header_sha256` (`docs/abi/header.sha256`) | `538a6a92105025adcef0f8ef08fcfd46a6bca9d9bc39297dc8a0949d90294fda` (one line: `include/tendies64/t64_machine.h` `46d07b8f7658a99b9ab32da2998995a8ec05c1a7015e6de580838e4df528840d`) |
| golden `public_symbols_sha256` (`docs/abi/public-symbols.txt`) | `c45ff33c495f8c0935dd2b0362b528c631037483fb6affb42b7d2d908a252d39` |
| golden `layout_sha256` (`docs/abi/layout.c`) | `9637d530d28435ee619452d290b19c4a217c964d5c6d9883c50e492fbfd01931` |
| golden `status_classes_sha256` (`docs/abi/status-classes.txt`) | `570548fc95fb3997a0f750bbe8cfdf443d6eadf4c0e5a38e5fe5d12ee50e6e5e` |
| fixture manifest `fixtures/manifest.json` (bound by each claim block; unchanged in Phase 11) | `sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88` |
| independent expectation `fixtures/composite/composite.expected.json` and its oracle script `fixtures/composite/composite_oracle.py` | `32de79ec5162a37697ef828b1761cf32e6976bd5efffc6c8c21c9dba6729cd19` and `c6b26a37b34bd623fd15af696370764eb962777d26e2e1f71305bf5ad101d587` |
| runner-recorded pins `fixtures/composite/composite.runner-pins.json` (label "runner-recorded pin, not independent") | `5307f77feb8ed0a0cf285ad6ab8e1484345fcf3a701b08ede8573e33a6daa577`: completion tick 599357, stop reason `GUEST_COMPLETION`, final state digest `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c`, 3 video packets |
| profile id of every installed-consumer case | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` |

Three-way scope (pin-impact DR-32, checker finding B-1). The installed consumer equals the in-tree
runner on every compared value and equals the independent literals on the video packets, the audio
packets, the save image, the transcript and the poll state. The completion tick, the stop reason, the
final state digest and the video packet count have no independent source: they are runner-recorded
pins (`fixtures/composite/composite.runner-pins.json`, label "runner-recorded pin, not independent",
citing the Phase 10 supervised runs), the consumer and the three-way script print that label next to
each of them, and no claim calls them independent.

## Native record

The record is private build output kept on the machine that ran it
(`build/gate/11-qualification-private/20261126T091014977323Z-aa393c22/record.json`, SHA-256
`8e2c8c7a7970d12cc60ecd65f93bcee71906dd28391ab271e504f5ffb451f10b`); this document holds digests,
counts and seconds only. The record states `rehearsal: false`, `status: complete`, a clean tree, an
empty `qualification_drift_paths`, `tier: unit-tested, authored scope`, and `hardware_accuracy`,
`oracle_comparison`, `final_abi` and `release` all the JSON value `false`. It binds 188 source inputs
(every file changed since the Wave 0 base outside the exclusions plus 21 stage tools; manifest SHA-256
`36d26b37307e6790c857326038b45128682b91e6468f89903eb9c0d919906dc3`), the 75 `PHASE11_TESTS` names
(names SHA-256 `9a7007747aabf0fe4e0b53c8a367d7a43e5a80d3c23c5a735d932063b19bc2ec`, equal to the 75
names of `ctest -N -L phase11`), the six artifact build ids above and the freeze record with the
SHA-256 of each of its four goldens, which equal its machine-read block. Total 689.49 s on the
qualifier clock (09:10:14Z to 09:21:44Z); host 1-minute load 7.11 at the start, after a wait for it to
fall below 8 (unrelated system processes had raised it to 15.8), and 18.37 at the end (the sanitizer
and sweep stages plus unrelated indexing processes), so the seconds are upper bounds. The record
passed on its first attempt: there is no failed attempt of this qualification, and no limit, backstop
or source changed. A rehearsal of the same table (record `20261126T084217919248Z-dedb2052`, plan 11-24,
614.07 s at `dedb205`) is labelled as one and refused as final evidence by the verifier.

| Stage | Seconds | Limit | Observed |
| --- | ---: | ---: | --- |
| ci-scanner | 1.83 | 90 | `check-ci-yml.sh` ok, selftest 138 cases, 0 failures |
| inventory | 0.56 | 60 | exact test inventory of the gate preset passes, count 329 |
| oracle | 0.17 | 60 | `--expect-blocked` exit 0 (blocked: no oracle operation is authorized); `--require-approved` exit 2 |
| abi-cheap | 6.58 | 720 | incremental gate build, then the cheap ABI and install names of the preflight |
| installed | 10.70 | 180 | `ctest -L phase11` with JUnit: 75 testcases equal the block, 0 failed, 0 skipped (JUnit SHA-256 `060df4b51d1ac64bec8896b64c2766bffe4dbc3c0cc1aee6cd5d51f812792047`, 42.5 s summed) |
| gate (`scripts/gate.sh`) | 174.75 | 600 | PASS, total 175 s; `ctest --preset gate` 329 of 329 passed, wall 98.43 s |
| sanitize-shipped | 85.30 | 300 | the 24 consumer cases under AddressSanitizer and UndefinedBehaviorSanitizer against the shipped (uninstrumented) installed library, 0 sanitizer references in the archive and 34 in the consumer; the call-sequence generator at 20,000 steps and digest stride 1: 21,186 calls (2,478 ok, 14,518 refused, 1,561 guest-stop, 1 defect, 2,628 calls on the poisoned instance), 1,275 refusal bursts, 1,285 machine digests verified, 0 refusals without a digest check, `checks=98461 failures=0`; no sanitizer report |
| sanitize-instrumented | 353.39 | 1,200 | the same 24 cases against an instrumented library (733 references in the archive, 36 in the consumer), sequence 20,000 steps at stride 8, `checks=97335 failures=0`; no sanitizer report |
| tsan | 5.56 | 120 | two threads, one instance each, both at tick 599357 with the digest `97f27027...1267c`, `checks=18 failures=0` (564 and 21 ThreadSanitizer references); no report |
| fresh-archive | 3.28 | 120 | `git archive` of H (1,435 files) built outside the repository as the gate preset configures it, six files installed into a prefix with a space, consumer smoke and composite pass, installed build id equal to the gate's |
| sweep | 46.19 | 300 | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` |
| freeze | 0.47 | 60 | `check-abi.py freeze` against the fresh-archive prefix: `abi=0.1 package=0.1.0 symbols=35 layout=14/165/190 holes=8 statuses=39 stop_reasons=8`, 0 findings |

Each limit is a kill deadline of the qualifier, derived from measured seconds in its own source with a
ratio of at least two. The tightest ratios of limit to observed seconds are 3.4 (the gate and the
instrumented sanitizer lane), 3.5 (the shipped sanitizer lane) and 6.5 (the sweep). Three stages ran
above the measured constants the table's derivation text cites (installed 8.8, sanitize-shipped 59,
sanitize-instrumented 299, all taken at lower load); the constants are derivation text, not limits, and
were not refreshed, because an edit of the qualifier after the Qualification base would be a
bound-input change and none of them is a defect.

## Local lanes

The sanitizer, TSan and fresh-archive runs are local lanes of `scripts/check-install.sh`, run only by
the qualifier and by hand; they are not gate names and are not in `ci.yml`. Their seconds and results
in the final record are the `sanitize-shipped` (85.30 s), `sanitize-instrumented` (353.39 s), `tsan`
(5.56 s) and `fresh-archive` (3.28 s) rows above, all on H and dated 2026-11-26 by the record. The
ThreadSanitizer run is the evidence of the threading statement (distinct instances are independent; one
instance needs external serialization) for two instances on two threads only; it is not counted in a
claim block.

## Final independent audit

Before the Qualification base, the whole phase was audited in a scratch clone of the head
`2c53513c0ddf2b1e31e0bbc261f7629ce2268658` (`.planning/phases/11-installed-consumer-and-freeze-candidate/11-REVIEW.md`,
section "FINAL"): the full gate 329 of 329, the sweep against the Wave 0 base with 0 differences, the
freeze record recomputed from a fresh install with 0 findings, the oracle authority still blocked with
an identity-only rebind, `docs/spec/api.md` section 12 read against the tests, no tag on a Phase 11
commit, no workstation path or personal data in the tree or the history, no edit of `ci.yml`,
`check-ci-yml.sh` or `gate.sh` since the Wave 0 base, and no pushed head inside the DEV-11-01 window.
Every requirement (VER-05, API-01 to API-04), every success criterion and every CONTEXT decision has a
row with its evidence and verdict. Result: zero blocking findings, no warning, four info rows (FN-01 to
FN-04, accepted). The earlier wave reviews are the SHAPE and INSTALL sections of the same record; every
blocking or warning finding of them was resolved by a numbered plan (11-26 to 11-29) before the freeze
record was bound.

## Hosted records

Every hosted run of Phase 11 is a `ci.yml` run on `main` with the two jobs `gate-macos` and
`gate-linux`; the rows of `docs/ci-reviews/LOG.md` (Runs table and findings F-127 to F-134) hold
the measured seconds. Seconds are job run times; "warm" means the Binutils cache was hit on both
jobs.

| Run | Plan | Head | Event | Result | `gate-macos` s | `gate-linux` s | LOG finding |
| --- | --- | --- | --- | --- | ---: | ---: | --- |
| 45736981784 | 11-15 (SHAPE review) | `43b31e2c0511` | push | success, warm | 445 | 251 | F-127 |
| 45742744889 | 11-16 (INSTALL tracer) | `e5a2e3c8ad11` | push | success, warm | 496 | 279 | F-128 |
| 45750585142 | orchestrator push after 11-18 | `c6f28f679af6` | push | success after one rerun of the failed `gate-macos` job (flake F-129, fixed by 11-28) | 500 | 216 | F-129 |
| 45766004142 | 11-21 (INSTALL review) | `463bd09d472c` | push | failure: `gate-linux` failed the two version-mismatch controls on cmake 3.31.6 (F-130, review finding I-11, fixed by 11-28) | 487 | 194 | F-130 |
| 45770631197 | 11-28 (defect fix) | `0e6b96dd8c24` | push | success, warm | 538 | 362 | F-131, F-132 |
| 45772586364 | cold dispatch (orchestrator) | `0e6b96dd8c24` | workflow_dispatch | success, **cold** (macOS cache entry deleted first) | **731** | 263 | F-133 |
| 45775107983 | 11-21 (resumed) | `4b13a9032dab` | push | success, warm | 592 | 240 | F-134 |

The first hosted runs of the five names added after `4b13a90` (`unit.core.state.v26-z-product-bound`,
`repo.installed-case-map`, `repo.abi-freeze-record`, `repo.abi-freeze-selftest` and
`harness.phase11-qualification`, the last three for the first time on Linux with gcc 13.3 and cmake
3.31) are the hosted pair of the qualified head, pushed by plan 11-25 after this document; that run,
its seconds and the final Gate-growth ledger row are recorded in `docs/ci-reviews/LOG.md` and in
PIN-IMPACT "Gate-growth ledger" (FREEZE row) by the same plan.

### Gate growth against the 900-second limit

The one measured cold `gate-macos` run of the phase took **731 s** of the 900 s job limit (run
45772586364 at 324 names; margin 169 s, 18.8 percent; Gate 669 s with the Binutils build 204 s and
ctest 284 s), which supersedes every earlier cold projection of the phase. The FREEZE wave added five
names with 9.98 s of COST after it (329 in all). The reversal rule of the Phase 11 G4 exception has
not triggered. The cold run with the final 329-name set is an orchestrator step after the push of
this plan and is not claimed here; no projection is presented as a measurement.

## Roadmap success criteria

| Criterion | Evidence |
| --- | --- |
| 1. a consumer built against a relocated clean install runs the Phase 10 composite fixture and agrees with the in-tree runner and independent literals; missing-package, version-mismatch, visible-source-tree, doctored-header and symbol-collision controls fail | claim block 1 (`installed.consumer.composite`, `.literals`, `.three-way`; the pinned values labeled as runner-recorded pins); the ten `installed.controls.*` names, each observed failing (or, for the positive rows, passing) for its own stated reason in the final audit; the fresh-archive stage (an exported tree, not the work tree) |
| 2. through the installed consumer every API-01 operation has an accepted case and a refusal, including output rings with forced backpressure and two instances in one process, in a host-shaped loop | claim block 2 (17 `installed.consumer.ops-*` names) and claim block 3 (`hostloop` with a forced `RESOURCE_LIMIT` stop resumed after a drain, `hostloop-cadence`, `hostloop-delay`, `two-instance`) |
| 3. a call-sequence generator proves every refused status leaves observations bit-identical, a guest-stop resumes and a defect poisons the instance, under ASan and UBSan against the shipped library; every status and stop reason appears once in the class table | claim block 4 (`installed.consumer.sequence`, 400 steps in the gate); the `sanitize-shipped` stage (20,000 steps, every burst digest-verified, no sanitizer report) and the `sanitize-instrumented` stage; `repo.abi-status-classes` (39 statuses and 8 stop reasons, each once) |
| 4. the installed archive's members import no file, clock, entropy or thread symbol, and a seeded violating object fails the check | `api04.members` (archive-level undefined set equal to `docs/abi/installed-undefined.txt` in both directions on Darwin and, by hosted measurement, on Linux), `api04.disasm`, `api04.headers` and `api04.seeded` (seeded `clock_gettime`, `fopen`, `getentropy` and `pthread_create` each reported), all passed in the record's installed stage; supporting, not counted in a block |
| 5. a bound freeze-candidate record exists and an incompatible change fails a check; an additive-extension rule is documented | `docs/abi/FREEZE-CANDIDATE.md` and its four goldens (hashes above), `repo.abi-freeze-record` and `repo.abi-freeze-selftest` (21 seeded cases), the `freeze` stage against the fresh install; `docs/spec/api.md` section 13 (the additive-extension rule) and section 14 (candidate, final bind and amendments); supporting, not counted in a block |

## Claims index

| Block | Profile id | Fixture set | Tier | Counting unit | Counted |
| ---: | --- | --- | --- | --- | --- |
| 1 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` | `phase11-installed-composite` | unit-tested | registered CTest names | 3 of 3: `installed.consumer.composite`, `.literals`, `.three-way` |
| 2 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` | `phase11-installed-ops` | unit-tested | registered CTest names | 17 of 17: `installed.consumer.ops-lifetime`, `-handshake`, `-struct-size`, `-profile`, `-admission`, `-input`, `-advance`, `-reset`, `-unload`, `-rings`, `-text`, `-save`, `-state`, `-queries`, `-trace`, `-bus`, `-poison-query` |
| 3 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` | `phase11-installed-hostloop` | unit-tested | registered CTest names | 4 of 4: `installed.consumer.hostloop`, `-cadence`, `-delay`, `.two-instance` |
| 4 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` | `phase11-installed-sequence` | unit-tested | registered CTest names | 1 of 1: `installed.consumer.sequence` |

The claim blocks count 25 passed registered names of the 75 `PHASE11_TESTS` names; each passed in the
retained JUnit of the record's installed stage (0.016 to 1.177 s each) and again in its full gate
stage. The other 50 names are supporting tests and are not counted: the 14 in-tree `unit.core.*`
names, the 14 `repo.*` names, the five `install.*` names, the four `api04.*` names, the ten
`installed.controls.*` names, `installed.consumer.build`, `installed.consumer.smoke` and
`harness.phase11-qualification`. The negative controls are named in the claim prose and not counted.

## Recorded exceptions for owner review

These items were decided under the delegated autonomy of the project policy, or are deviations
the phase recorded, and are listed here so the owner can review them. **None of them is an owner
approval**, and none changes a Phase 10 or earlier claim, tier or authority. The one owner decision
of the phase is the call-site-only rebind approval of 2026-11-25 and its answer A (item 2); it is
cited, not widened.

1. **DEV-11-01, the non-compiling window of the nine oracle-bound test executables (PIN-IMPACT
   DR-22, corrected after the plan check).** From the flip commit `0338380171fc198c175b78e70e499c1cee554fac`
   (plan 11-12) to the parent of the bound commit `e66625a6494863ba6921da575fdb86e379a1c76d` (plan 11-14),
   12 commits, the nine bound test executables (and `test_dp_transport`) did not build in the shared
   tree, because the owner approval requires their call-site edits as one isolated commit placed last
   in the shape wave and no header could accept both the old positional forms and the new layouts.
   The commits of the window are not consecutive code commits (refresh, prologue and planning commits
   sit between). Before each code commit in it, the full suite including the nine tests passed in a
   marked scratch clone with the proven bound patch (identity-block wording of amendment `36fc5ac`:
   exactly the two identity-block names failed until the rebind). Nothing inside the window was
   pushed: no `ci.yml` run head lies in it, and every pushed head of the phase descends from
   `e66625a` (final audit check (i)). `git bisect` across the window is unsupported for the nine tests.
   Deviation from the CONTEXT rule "compiling commits only", which holds for every other commit and
   target.
2. **The orchestrator's identity-only rebind of the Phase 7 oracle request (plan 11-14).** Commit
   `e66625a` changed the nine bound files at call sites only (17 category (a), 92 category (b) and the
   one category (c) hunk `core/tests/test_rdp.c` line 486 of owner answer A), their derived hashes in
   the authority record and `PINNED_REQUEST_SHA256` (`e131cbe7...` to
   `75f2b651394825ade3a2661950b889522a59b03e2fbc21c65c70f271adda2779`); `local_decision` stays blocked,
   `--expect-blocked` exits 0 and `--require-approved` exits 2. This is the owner approval of
   2026-11-25 as granted; it grants no oracle, rights or publication authority.
3. **File-count and plan-count exceptions (ROADMAP Phase 11 G5 note).** Plan 11-12 changed 81 files,
   67 of them the output of one codemod invocation over every non-bound caller, because the create and
   trace-drain signatures and their callers had to change in one compile state; plans 11-03 (24 files),
   11-04 (18) and 11-11 (33) were accepted by orchestrator ruling (the project rules cap tasks and time
   per plan, not files). The phase took 29 plans against a guideline of about 10 to 15: 25 planned
   (the shape wave split for the 4-task and 15-file caps, three mandated review plans) and the four
   numbered defect-fix plans 11-26 to 11-29 that the reviews required.
4. **Phase 11 G4 gate-growth exception (PIN-IMPACT "Gate-growth ledger").** The required gate grew
   from 254 to 329 CTest names (75 `phase11` names, COST sum 65.83) without removing equal time; the
   names are the required evidence of SC1 to SC5 and no offset is claimed. Sanitizer, TSan and
   fresh-archive runs stay local; no `ci.yml`, `check-ci-yml.sh` or `gate.sh` edit was made in the
   phase. Measured: cold `gate-macos` 731 s of 900 s at 324 names (run 45772586364, margin 169 s);
   warm `gate-macos` 445 to 592 s across the phase's pairs (a 105 s spread on identical names). The
   reversal rule (a cold run above 900 s, or a cancelled run, moves non-essential names to a local
   lane) has not triggered. The cold run with the final 329-name set is the orchestrator's, after the
   push of plan 11-25.
5. **DR-07, the parameter reading of "`bool` and `size_t` leave public structs".** Every `size_t`
   struct member became `uint64_t` and every `bool` struct member `uint32_t`; function parameters and
   the allocator callback typedef keep `size_t` and `bool` (fixed by each platform ABI; the locked
   decision names public structs; libretro's `retro_serialize(void *, size_t)` is the precedent).
6. **INFO-03, the one exception to "caller-filled `struct_size`" (DR-06, kept by owner answer A).**
   `t64_host_storage.struct_size` is filled by the core before each allocate call, because the core is
   the caller of the host allocator callback for that out struct.
7. **The CONTEXT amendment of the stack-protector decision (DR-31).** The orchestrator revised the
   decision on 2026-11-25 after the plan check to add the ELF-only `_GLOBAL_OFFSET_TABLE_` row to the
   installed-archive allowlist (three rows in all, each with its platform and reason, ADR-0006
   amendment); the Linux rows are confirmed by hosted `gate-linux` measurements.
8. **Accepted limits of the candidate (review items I-05, S-05, I-06).** The `value_out` span of
   `t64_machine_bus_read` is the caller's responsibility (stated in the header; a check would narrow
   accepted inputs and edit a sealed RSP path, so it is an open amendment); eight implicit padding
   holes are bound as such and explicit reserved members are a Phase 12 final-bind question; the gate
   samples the machine digest over every second refusal burst while the local lanes verify every
   burst. The final audit's four info rows (FN-01 to FN-04) are accepted the same way.
9. **The A5 assumption held; no fallback.** The black-box defect trigger (a flipped bit in the
   stored profile digest, found exactly once in the consumer's own allocator storage) works through the
   published API, so no test-only symbol is declared in the consumer and no fallback deviation exists.
10. **Carry-ins deferred, none dropped (PIN-IMPACT "Carry-in triage", 13 rows).** Done: W2-01, W2-02,
   W1-03, D-10-17-01, T3 (in Phase 10), T4, T7 and T8. Deferred with reasons: DEF-W1-01 (three unpinned
   sibling copies of the perspective floor in sealed RDP bodies, to v1.3), T1 (splitting
   `unit.core.state.import`, beyond v1.2: its case list is a closed Phase 9 pin), T2 (replay-driver
   duplication, Phase 12 host pump), T5 (closed recorder-failure detail names, Phase 12) and T6
   (`T64_REAL_KEEP_WORKSET`, Phase 12). Five carried items, the G3 limit.
11. **A defect fixed in the copied qualifier only.** The Phase 10 qualifier compares the claims map
   with `==`, which accepts `0` for `false`; the Phase 11 copy requires the JSON value `false` itself
   (case `claims-pinned`). The closed Phase 10 qualifier and record were not edited.

## Limitations and preserved stops

- Authored scope only: hand-written literals, the authored composite ROM and its independent oracle
  script. No hardware measurement, other-emulator comparison, oracle operation or commercial title was
  used or is claimed.
- A software-interface claim about ABI 0.1, a freeze candidate. Between the candidate and the final
  bind (the last plan of Phase 12) only defect-driven amendments with evidence, a re-run of the
  installed-consumer suite and a minor increment may change a bound element; the final bind may
  confirm the candidate or list amendments. No stable interface exists before it.
- The layout pins hold for targets with 64-bit pointers (macOS arm64 and Linux x86_64, the two hosted
  operating systems); on other data models the candidate binds no layout.
- The installed package is one configuration (Release) built by the project's own preset; the claims
  hold for the exact build id above. State blobs and container formats stay build-identity-bound and
  outside the interface promise.
- The gate samples the machine digest over every second refusal burst of the generator; the full
  comparison is the local `sanitize-shipped` stage. ThreadSanitizer covers two instances on two
  threads; concurrent calls on one instance stay undefined.
- Hosted time: the one measured cold `gate-macos` job of the phase is 731 s of 900 s at 324 names;
  warm samples of the same names spread by 105 s. The cold run with the final set is an orchestrator
  step after the push of plan 11-25.
- Raw logs, state, save, capture and record files stay private and are never uploaded; this document
  holds digests, sizes, counts and public run numbers only.
- D5 (no oracle material), D6 (no tag, no release, no channel; "mismatched version" means an ABI
  constant) and D7 (no branding; the name is not trademark-cleared) stay in force. Phase 7 VID-04 and
  the oracle request stay blocked on oracle authority and the parallel-rdp rights audit. No ROM,
  firmware, PIF or IPL image of a commercial product, 64DD media or commercial save file was used; the
  only embedded IPL3 is the separately rights-cleared Unlicense payload recorded in the fixture
  manifest. The closed Phase 6 to 10 claims and evidence documents are unchanged.

## Reproduction

The private record is an untracked build output kept on the machine that ran the qualification; the
check below needs it, the build trees the stages created and a checkout of the qualified history.

```sh
python3 -B scripts/qualify-phase11.py --verify build/gate/11-qualification-private/20261126T091014977323Z-aa393c22/record.json
sh scripts/check-claims.sh --files docs/claims/phase11.md
```

The first command must print `qualify-phase11: record verified against the current tree` at H and at
any later head whose changes after the Qualification base are limited to `.planning/`,
`docs/ci-reviews/LOG.md`, `docs/claims/phase11.md` and this document (it was run at H, after the
record's planning commit and after the commit of this document); any other changed path refuses it by
design. The second prints `claims: files=1 blocks=4 errors=0`. The closed Phase 10 records are
unchanged: `git diff --quiet f9124ae9fce134f31b645c6f2911a6f27fa41b9a HEAD -- docs/claims/phase10.md
docs/evidence/phase10-qualification.md` exits 0.
