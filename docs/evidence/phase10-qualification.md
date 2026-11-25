# Phase 10 carry-in closure qualification

The audited source is the tree of `76451024e148073804e162841c3d0a3543bd2567` (the Qualification
base recorded in the Phase 10 pin-impact record after the gap closure of plans 10-20 and 10-21).
The one final native qualification (`python3 -B scripts/qualify-phase10.py --native`, no
rehearsal flag) passed on the head `1fc0b1da70acf6c9b0f3baf0b46abf9d5a69468f` (named H2 below),
whose bound source inputs equal those of the audited tree (the one path between the two commits is
a planning record), on its first attempt, and the exact-binding verifier
(`python3 -B scripts/qualify-phase10.py --verify`) then accepted the record against that tree. The
head that was pushed and ran in a warm hosted pair is `85ba477bbf49a8fd1f2ca764c51433764c85bcab`
(named H3 below: H2 plus one planning-only commit, with the identical bound source snapshot). An
earlier qualification of plan 10-19 on the first audited tree (`ad39b6be...`) is superseded and
kept below as history: plan 10-20 then added the committed flip-every-member rows of the VI state
(a bound test source) and moved the qualifier text, so that record no longer verifies. The one
measured cold-cache hosted run of `gate-macos` (731 s of its 900-second job limit) was taken on
the earlier audited tree and was **not repeated** for the gap closure, which changed test source,
the mutant driver and qualifier text only. The bounded claims are the eight blocks of
[phase10](../claims/phase10.md).

Evidence tier: **unit-tested, authored scope** (every block of the claim file is tier `unit-tested`).
Every expected value is a hand-derived literal or comes from an independent oracle script kept
outside the code under test; the machine under test did not produce its own expectation. This is
not hardware-derived, not emulator-differential, and uses no commercial software, ROM, firmware,
save file or oracle material. Every Z mode rule, exponent decode, hidden-sidecar layout, tie rule,
scan-out expansion and timing figure is a named software-profile policy of this project, not a
measured hardware fact. No claim is made here about hardware accuracy, about agreement with any
other emulator, about broad title coverage, or about a stable interface; nothing was released or
published, and no tag was created (the commits are named by their 40-hex ids).

## Source and build identity

| Item | Value |
| --- | --- |
| audited source (Qualification base) | `76451024e148073804e162841c3d0a3543bd2567` |
| qualified commit **H2** (`commit_sha` of the record of this document) | `1fc0b1da70acf6c9b0f3baf0b46abf9d5a69468f`; the one path between the base and H2 is the pin-impact record |
| pushed head **H3** (the head of the warm hosted pair of this document) | `85ba477bbf49a8fd1f2ca764c51433764c85bcab`, H2 plus one planning-only commit (the record of the re-qualification); `git diff --name-only H2 H3` lists planning records only, so H3 has the identical bound source snapshot of H2 |
| earlier audited source (superseded, history) | `ad39b6be2f803af45af15d4ed511b72a89460339`, qualified by the plan 10-19 record on `b9d957c391b128622912481a99ffe24e6c84e609`; the warm pair and the one cold run of the earlier tree ran on `f081d392763ad384c3d50ae1b24b709d5af3b7d4`, an ancestor of that earlier base (the paths between the two are planning records and `docs/ci-reviews/LOG.md` only) |
| wave bases bound by the record | Wave 0 `4ac633e880e0c26005cb03092e8d8485e721034c`, Wave 1 `f6ab8c26d008b8301b729b5aae2715e1b1079318`, Wave 2 `6dd785986f6aa78c6da5573a558b15b386a4edee`, Wave 3 `1cded8de34b0c2f5c5bea903095ffc7f1e55d6fc`, Qualification base `76451024e148073804e162841c3d0a3543bd2567` |
| core build id (the claim `build_id` values carry the prefix `0.0.0+`; all four build ids equal those of the superseded plan 10-19 record because no production source changed) | `t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884` |
| runner build id | `t64-runner-v1-684d67fa59f416d1549be743c93cfd451fc8b1ab849ac31fdd643ef2e7811f55` |
| runner test twin build id | `t64-runner-v1-2d3ac6f8be3b8203341f9238c3d2c07d52a3f3363ee6a078ef870d9b29f60051` |
| ASan and UBSan core build id | `t64-build-v1-63fe5be296b2b623822ce9f111b6474eb0c4462e1de642718c31f9bc31b7ad5a` |
| fixture manifest `fixtures/manifest.json` (bound by each claim block) | `sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88` (the manifest moved once in Phase 10, by the two composite rows of plan 10-14; the ten Phase 9 claim blocks keep the Phase 9 manifest digest `b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0` and a per-row view of the 39 rows that manifest held) |
| composite oracle script `fixtures/composite/composite_oracle.py` | `c6b26a37b34bd623fd15af696370764eb962777d26e2e1f71305bf5ad101d587` |
| committed expectation `fixtures/composite/composite.expected.json` | `32de79ec5162a37697ef828b1761cf32e6976bd5efffc6c8c21c9dba6729cd19` |
| composite transcript (188 bytes) | `da07d916f0314db05e8d609326242d7f944098e96ce19a96339ef1cd6a7aad39` |
| composite ROM `phase10-composite-s32k` (5,496 bytes, authored, build-only) | `7a0426b49eb8f4288c70414e9b34cc6e4fdfb8052335976f8fc43d886e954945` |
| registered ids of the new family | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2` (profile SHA-256 `3bc028bdb4c6f9622e20ecfff828a26245adc04ea69ddeeefdbb08d5b27dbc47`, 1,358 bytes) and its three save ids `-e4k`, `-e16k` and `-s32k` (state serialization 26, machine model 21, descriptor 13, digest domain `t64.state.si.26`, container version word 2) |

## Native record

The record is private build output kept on the machine that ran it (`build/gate/10-qualification-private/20261125T155456703622Z-1fc0b1da/record.json`,
SHA-256 `1099e3971e0cca245844641ad4977b3dbc3ce523e452449efb551d75a9d487fc`); this document holds
digests, counts and seconds only. The record states `rehearsal: false`, `status: complete`, a clean
tree, an empty `qualification_drift_paths`, `tier: unit-tested, authored scope`, and
`claims.hardware_accuracy: false` and `claims.oracle_comparison: false`. It binds 62 source inputs
(manifest SHA-256 `eb28495da4fb2a271c65f01d3a375b6b386d36160b55c792fd6cacc7c144aa15`; the two
inputs added against the superseded record are the claim and evidence documents of plan 10-19, and
three inputs have new content: `core/tests/test_digest_v26.c`, `scripts/p10-mutants.py` and
`scripts/qualify-phase10.py`) and the 30 `PHASE10_TESTS` names (names SHA-256
`c0e7d637b3f1c181b4f7cb08f032bf3aa5a84a04241005114459b08eff156f28`, equal to the 30 names of
`ctest -N -L phase10`). Total 410.78 s on the qualifier clock (6 min 51 s wall); host 1-minute
load 9.10 at the start and 7.28 at the end, after a wait for the load to fall to 8 or below, so the
seconds are close to a quiet host and still upper bounds. The record passed on its first attempt:
there is no failed attempt of this re-qualification, and no limit, backstop or source changed.

| Stage | Seconds | Limit | Observed |
| --- | ---: | ---: | --- |
| ci-scanner | 2.98 | 90 | `check-ci-yml.sh` ok, selftest 138 cases, 0 failures |
| inventory | 1.10 | 60 | exact test inventory of the gate preset passes, count 254 |
| oracle | 0.22 | 60 | `--expect-blocked` exit 0 (blocked: no oracle operation is authorized); `--require-approved` exit 2 (no pinned competent owner or legal decision) |
| gate (`scripts/gate.sh`) | 128.13 | 600 | PASS, total 128 s; `ctest --preset gate` 254 of 254 passed, wall 76.18 s |
| phase10-junit | 7.02 | 120 | 30 testcases equal the block, none failed or skipped (gate-preset JUnit SHA-256 `4d4e9987113ae7e3743681502ecc150d720d801f5d66e1a0cd6947e8b2c2214d`, 23.82 s summed; `unit.core.digest.v26-completeness` 1.13 s) |
| asan (configure, build, selection) | 125.10 | 1,320 | configure 0.14 s, build 7.84 s, `ctest --preset asan -L phase10` 117.12 s wall: 30 testcases equal the block, none failed or skipped (JUnit SHA-256 `db0cb3fb19eb490224cbf72052d4a446bdaecfe8dc4fcc9e65412d4f2a7977cc`, 274.81 s summed; `unit.core.digest.v26-completeness` 8.67 s), no sanitizer report |
| mutants | 105.00 | 600 | 28 rows equal the driver's `--list`: 26 KILLED, 1 equivalent control SURVIVED, 1 non-compiling control INVALID, each as expected |
| sweep | 40.77 | 300 | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` |

Each limit is a kill deadline of the qualifier, derived from measured seconds in its own source. The
tightest ratios of limit to observed seconds are 4.7 (the gate), 5.1 (the ASan selection wall), 5.7
(the mutants) and 7.4 (the sweep); against the serial sum of the ASan names (274.81 s) the 600 s
CTest limit is 2.18 times, not the rule of the qualifier (the four-job preset overlaps the names).

### Superseded qualification and its attempts (history, plan 10-19)

Plan 10-19 qualified the first audited tree (`ad39b6be...`, record
`20261125T134440416529Z-b9d957c3`, SHA-256 `82ef8c2e8aa45db3c938fb3b2ed736299a90026ac3850c7d16ca90e503985c96`,
`commit_sha` `b9d957c391b128622912481a99ffe24e6c84e609`, 60 inputs, a mutant stage without the
VI row, gate 129.41 s, asan 136.95 s, total 418.6 s, host load 16.1 at the start and 6.8 at the
end). **That record is superseded, not withdrawn**: plan 10-20 changed a bound test source and the
qualifier text, so it no longer verifies against the tree of this document, and it stays listed
with its four attempts because they are part of the evidence. Its four-attempt history follows.

The one qualification command was run four times on the same unchanged source; no limit was widened
and no source, test, script or pin changed between attempts. Attempts 1 to 3 ended in the gate stage
without a record: eight different Phase 8 and 9 supervised-run tests ended at their supervisor wall
backstops (none is a Phase 10 name) while unrelated host workloads loaded the local machine. The
attempt that passed ran after those workloads were reduced. Host load as the cause is an inference
from timing and from the passes of the unchanged source (plain `sh scripts/gate.sh` 196 s and
135 s, the 10-17 rehearsal, the 10-18 clone gate and every hosted run), not a reproduced cause.

| Attempt | Private record directory (repo-relative, not committed) | Source head | Result | `ctest --preset gate` wall |
| ---: | --- | --- | --- | ---: |
| 1 | `build/gate/10-qualification-private/20261125T051452604175Z-1f862a0f` | `1f862a0f7a25` | gate stage failed, no record: `runner.save-roundtrip`, `runner.state-export`, `controls.real.rdp-drain-cadence` | 389.31 s |
| 2 | `build/gate/10-qualification-private/20261125T054642787825Z-b9d957c3` | `b9d957c391b1` | gate stage failed, no record: `runner.cpu-diagnostic`, `controls.real.raw-capture-caps`, `controls.real.raw-capture-target-errors` | 242.25 s |
| 3 | `build/gate/10-qualification-private/20261125T055712530841Z-b9d957c3` | `b9d957c391b1` | gate stage failed, no record: `controls.real.audio-wrong-sample`, `harness.fsize-report-capture-still-bounded` | 187.38 s |
| 4 | `build/gate/10-qualification-private/20261125T134440416529Z-b9d957c3` | `b9d957c391b1` | **passed; record written; `--verify` exit 0** | 77.30 s |

The verifier ran once for each record: for the superseded record at the head
`b9d957c391b128622912481a99ffe24e6c84e609`, and for the record of this document (`20261125T155456703622Z-1fc0b1da`)
at H2, `1fc0b1da70acf6c9b0f3baf0b46abf9d5a69468f`, in both cases before this document and the claim
file were committed. It cannot be repeated at a later head: this document and
`docs/claims/phase10.md` are bound source inputs newer than the Qualification base (the qualifier
excludes only `.planning/` and `docs/ci-reviews/LOG.md`), so a later `--verify` is expected to
refuse with a changed-path message (reproduction below). It was deliberately not re-run.

## Mutant classification

The out-of-tree mutant driver `scripts/p10-mutants.py --run` edits scratch copies of the sources and
classifies each row by the named test and case that fails. The table lists the 28 rows of the
driver: 26 mutants, all KILLED, and 2 controls, the equivalent control that must SURVIVE and the
non-compiling control that must be INVALID. Every row of this table is as expected. The row
`v26-digest-omit-vi-phase-cycles` (plan 10-20, finding W2-03) removes the hash of one VI member
(`phase_cycles`) from the digest walker; it SURVIVED the committed case before the VI rows existed
(a behavioral RED, attributed by a stream-equality probe) and is KILLED by the case after them.

| Row | Kind | Result | Failing test and case |
| --- | --- | --- | --- |
| `control-equivalent-depth-cap` | control | SURVIVED | equivalent mutant, must SURVIVE |
| `k1-predicate-widened` | mutant | KILLED | `test_k1_refusal` case `apply-refusal` |
| `vg01-decal-strict` | mutant | KILLED | `test_vg_gaps` case `decal-equality` |
| `vg01-zcap-7ffe` | mutant | KILLED | `test_vg_gaps` case `primitive-z-saturation` |
| `vg01-delta-floor` | mutant | KILLED | `test_vg_gaps` case `delta-ceiling` |
| `vg-zexp-decode-top` | mutant | KILLED | `test_vg_gaps` case `z-exponent-boundaries` |
| `vg-zexp-compress-round` | mutant | KILLED | `test_vg_gaps` case `z-exponent-boundaries` |
| `vi-clock-48600000` | mutant | KILLED | `test_vg_gaps` case `ntsc-clock` |
| `vg03-alpha255-v9` | mutant | KILLED | `test_vg_gaps` case `rgba5551-alpha-clear` |
| `vg03-alpha255-v8` | mutant | KILLED | `test_vg_gaps` case `rgba5551-alpha-clear` |
| `vg02-parity-ignored` | mutant | KILLED | `test_vg_gaps` case `serrate-filter-parity` |
| `vg02-next-row` | mutant | KILLED | `test_vg_gaps` case `serrate-filter-parity` |
| `vg04-floor-off-s` | mutant | KILLED | `test_vg_gaps` case `perspective-floor` |
| `vg04-floor-off-t` | mutant | KILLED | `test_vg_gaps` case `perspective-floor` |
| `vg05-shade-trunc` | mutant | KILLED | `test_vg_gaps` case `shade-floor-bayer` |
| `vg05-bayer-shift2` | mutant | KILLED | `test_vg_gaps` case `shade-floor-bayer` |
| `v26-sidecar-invalidation-removed` | mutant | KILLED | `test_rdp_depth_composite` case `depth-composite-sidecar` |
| `v26-digest-omit-scalar` | mutant | KILLED | `test_digest_v26` case `v26-completeness` |
| `v26-digest-omit-hidden` | mutant | KILLED | `test_digest_v26` case `v26-completeness` |
| `v26-export-omit-hidden` | mutant | KILLED | `test_state_export_v26` case `export-v26-idle` |
| `v26-export-version-word-1` | mutant | KILLED | `test_state_export_v26` case `export-v26-idle` |
| `v26-reader-pairing-dropped` | mutant | KILLED | `test_state_export_v26` case `export-v26-idle` |
| `v26-import-skip-hidden` | mutant | KILLED | `test_state_import_v26` case `roundtrip` |
| `v26-import-depth-after-proof` | mutant | KILLED | `test_state_import_v26` case `roundtrip` |
| `v26-import-addr-unchecked` | mutant | KILLED | `test_state_import_v26` case `hostile` |
| `v26-import-z-products-unchecked` | mutant | KILLED | `test_state_import_v26` case `hostile` |
| `v26-digest-omit-vi-phase-cycles` | mutant | KILLED | `test_digest_v26` case `v26-completeness` |
| `control-no-compile` | control | INVALID | does not compile, must be INVALID |

## Digest sweep, K1 and oracle authority

- **Sweep.** `python3 -B scripts/digest-sweep.py --base 4ac633e880e0c26005cb03092e8d8485e721034c --head HEAD`
  compares the canonical digests of the 21 shipped ids at every ROM and checkpoint between the
  Wave 0 base and the head: `sweep: 1599 shared lines, 0 differences, 0 head-only lines`, so the
  shipped ids (the 17 earlier ids and the four si-v1 ids) are byte
  identical across the one identity and state bump (the four new ids add digest lines of their own
  and change none of the shared ones).
- **K1.** `unit.core.k1.dispatch-refusal` and `unit.core.k1.apply-refusal` passed natively and under
  ASan: the five shipped composite ids still refuse SetZImage, SetPrimDepth and the four Z triangle
  opcodes (depth stays inactive there), and the mutant `k1-predicate-widened` is KILLED by the
  apply-refusal case.
- **Oracle authority.** `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` exits 0 and
  `--require-approved` exits 2: the Phase 7 oracle request stays blocked and no oracle operation
  is authorized. `PINNED_REQUEST_SHA256` is `e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab`
  and unchanged since the rebind recorded below; the 13 oracle-bound files, `decode_container.py`
  and the case lists of the binaries inventoried by `docs/spec/phase09-cases.json` were not edited,
  and adding the two composite manifest rows needed no rebind (per-row pins, GATE-02).

## Hosted records

Run 45650890027 is the warm pair of the pushed head H3 (`85ba477bbf49a8fd1f2ca764c51433764c85bcab`,
workflow `ci`, event `push`, both jobs success, Binutils cache hit on both, no cache saved or
changed): the head of this document's re-qualification, whose bound source snapshot is that of the
qualified commit H2. Run 45548987145 is the warm pair of the earlier audited head (workflow `ci`,
source `f081d39`, both jobs success, Binutils cache hit on both). Run 45550277510 is the cold-cache
run of that earlier head: the orchestrator deleted the macOS Binutils cache entry, then dispatched
the workflow on the same head (`workflow_dispatch`); the executor only read the completed run.
**The cold run was measured on the earlier audited tree and was not repeated for the gap closure**,
which changed test source, the mutant driver and qualifier text and added no CTest name; the
measured hosted cost of the gap closure is test time only: `unit.core.digest.v26-completeness` ran
3.13 s on macOS and 3.85 s on Linux against 1.67 s and 1.02 s before the VI rows, and the ctest
wall of 254 names moved by -2 s on macOS and +2 s on Linux against the earlier warm run, inside the
52 s spread of F-119. The recorded rows are F-123 to F-126 and the Runs rows of
`docs/ci-reviews/LOG.md`; the measurements of the earlier waves are F-120 to F-122.

| Run | Job | Seconds | Gate step | `ctest` | Binutils cache |
| --- | --- | ---: | ---: | ---: | --- |
| 45650890027 (warm, H3, the head of this document) | `gate-macos` | 368 | 315 s | 194 s on 254 names | hit |
| 45650890027 (warm, H3, the head of this document) | `gate-linux` | 254 | 202 s | 128 s on 254 names | hit |
| 45548987145 (warm, earlier head) | `gate-macos` | 385 | 337 s | 196 s on 254 names | hit |
| 45548987145 (warm, earlier head) | `gate-linux` | 233 | 191 s | 126 s on 254 names | hit |
| 45550277510 (cold, earlier head) | `gate-macos` | **731** | 653 s | 265 s on 254 names | **miss on restore, saved after the job** |
| 45550277510 (cold, earlier head) | `gate-linux` | 250 | 200 s | not read | hit |

### Cold run against the 900-second limit

The check is an integer comparison made by the plan's own command: the `gate-macos` job of run
45550277510 took **731 s**, and `test 731 -le 900` holds. The margin is 169 s, 18.8 percent of the
limit; the 600-second limit of Phase 9 would have been exceeded by 131 s, so the 15-minute limit
set in plan 10-04 (with its scanner rule, `CI-TIMEOUT` `gate-macos` 1 to 15 and other gate jobs 1
to 10) was needed. The reversal rule of the gate-growth exception (a cold run above 900 s or
cancelled at the limit returns a new numbered plan) is **not triggered**.

| `gate-macos` cold step | Seconds |
| --- | ---: |
| Set up job, Checkout | 2, 6 |
| Restore pinned Binutils (actions/cache miss) | 1 |
| Gate | 653 |
| of which fixture-qualification (the Binutils build; about 1 to 2 s warm) | 219 |
| of which ctest (254 of 254) | 265 |
| of which test-inventory-selftest, boot-evidence-selftest, build, core-lint, lint-selftest | 57, 31, 31, 28, 12 |
| audio qualification records, Phase 9 qualification records | 40, 5 |
| Post Restore pinned Binutils (cache save) | 3 |

Against the other numbers, one sample each and read against the run-to-run spread of up to 52 s
that F-119 measured on identical source: warm before Phase 10 (F-111) 487 s; warm after (F-120 to
F-123) 346, 326, 496 and 385 s; the cold projection of F-111 before Phase 10 was 673 s, so the
measured 731 s is 58 s higher than that projection and still inside the limit; the plan 10-18
cold projection (warm 385 s plus the 186 s Binutils delta) was 571 s, so the measurement is 160 s
higher; the plan 10-17 projections were 696 to 735 s (primary) and 709 to 806 s (cross-check), and
the measurement lies at the top of the first and inside the second. The Binutils build cost 219 s
here against 179 and 186 s measured earlier (F-096, F-111), ctest 69 s more than the warm run on the
same names (a runner-speed difference, 17 s beyond the spread). The macOS cache entry was recreated
after the run (5.03 MiB); the Linux entry was untouched; a timed-out job saving no cache was not
exercised. Measured, not projected, is the success criterion of GATE-01, and this measurement meets it.
It was taken on the earlier audited tree (`f081d39`) and is not repeated for the gap closure, whose
only hosted cost is the added test time of one existing case (+1.46 s on macOS in run 45650890027);
a cold figure for H3 would be about 733 s, which is a projection and not a measurement, with a
margin of about 167 s.

## Phase 9 carry-in triage

Every item the Phase 9 review tagged for Phase 10 ends in done, deferred or dropped, with its
reason, in `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` section 13: T3
(the dead publisher link of `test_replay`) is done; T1 (splitting `unit.core.state.import`), T2
(replay-driver duplication), T4 (stale comments), T5 (closed detail names for recorder failures), T6
(`T64_REAL_KEEP_WORKSET`), T7 (the third SI trace kind name and dead scratch facts) and T8 (the
case-to-CTest mapping checker follow-up) are deferred to Phase 11. The provisional-name sweep of
seven live spec docs is section 12 of the same record.

## Roadmap success criteria

| Criterion | Evidence |
| --- | --- |
| 1. depth fixture on the new ids equal to independent literals, wrong-depth control fails, shipped ids unchanged | claim block 1 (`unit.core.rdp.depth-composite-*`), the mutant rows of the VID-07 group, the K1 tests and the sweep above (1599 lines, 0 differences) |
| 2. state mid-primitive and mid-scan-out with depth round trips, one version marker, flip-every-member test, build-identity-bound documentation | claim block 2 (`unit.core.state.import-v26-*`, `unit.core.digest.v26-*`); the flip-every-member case `v26-completeness` covers the RDP half (every RDP member, the coverage table, strided TMEM and the hidden depth table) and the VI half (120 rows over the 1,208 bytes of `t64_vi_state`: on the stem id every byte is flipped and exactly 417 of 1,208 are inert, namely 400 bytes of six named exclusions with reasons and 17 padding bytes, 791 member bytes change the stream; a representative sweep on the three save ids and the state 25 stem id; five ids in all), with the mutant `v26-digest-omit-vi-phase-cycles` KILLED (finding W2-03 closed by plan 10-20); `docs/spec/state-container.md` and `docs/spec/machine-contract.md` state that blobs are build-identity-bound and outside the interface promise |
| 3. VG-01 to VG-05 and the standard-NTSC clock each have independent expectations and a failing mutant | claim blocks 4 to 8 (nine `unit.core.vg.*` names, 14 mutants KILLED) |
| 4. one composite guest: raw video and audio from a capture run, exact save bytes from a save-out run, same final digest, each output against its own independent expectation with a wrong-value control | claim block 3 (`phase10-composite-s32k` through `runner.composite-two-run`; `controls.real.composite-wrong-rom` and `harness.composite-oracle` as supporting tests) |
| 5. measured cold-cache hosted `gate-macos` run inside its limit with before and after timings, the 15-minute limit with its scanner rule and the pinned limit relation; adding a manifest row needs no oracle-request rebind | the cold run above (731 s of 900 s, measured on the earlier audited tree and not repeated), F-111 and F-120 to F-126, F-115 and F-117; the composite rows were appended in one commit with `PINNED_REQUEST_SHA256` unchanged |

## Claims index

| Block | Profile id | Fixture set | Tier | Counting unit | Counted |
| ---: | --- | --- | --- | --- | --- |
| 1 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2` | `phase10-depth-composite` | unit-tested | registered CTest names | 3 of 3: `unit.core.rdp.depth-composite-accepted`, `-wrong-depth`, `-sidecar` |
| 2 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2` | `phase10-state-26-roundtrip` | unit-tested | registered CTest names | 4 of 4: `unit.core.state.import-v26-roundtrip`, `-hostile`, `unit.core.digest.v26-completeness` (RDP and VI members), `-strip-differential` |
| 3 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` | `phase10-composite-run` | unit-tested | accepted ROM row | 1 of 1: `phase10-composite-s32k` through `runner.composite-two-run` |
| 4 | `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7` | `phase10-vg-depth` | unit-tested | registered CTest names | 4 of 4: `unit.core.vg.decal-equality`, `primitive-z-saturation`, `delta-ceiling`, `z-exponent-boundaries` |
| 5 | `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8` | `phase10-vg-vi-clock` | unit-tested | registered CTest names | 2 of 2: `unit.core.vg.ntsc-clock`, `rgba5551-alpha-clear` |
| 6 | `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9` | `phase10-vg-serrate` | unit-tested | registered CTest names | 1 of 1: `unit.core.vg.serrate-filter-parity` |
| 7 | `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4` | `phase10-vg-perspective` | unit-tested | registered CTest names | 1 of 1: `unit.core.vg.perspective-floor` |
| 8 | `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3` | `phase10-vg-shade-bayer` | unit-tested | registered CTest names | 1 of 1: `unit.core.vg.shade-floor-bayer` |

The claim blocks count 3 + 4 + 9 = 16 passed registered names of the 30 `PHASE10_TESTS` names plus
one accepted ROM row (block 3), which `runner.composite-two-run` carries; the other 13 names are
supporting tests and are not counted (the refusal and identity tests `unit.core.k1.*` and
`unit.core.profile.v26-*`, the export-side tests `unit.core.state.export-v26` and `-bounds`,
`runner.state-format-v26`, `runner.composite-state-v2`, `harness.composite-oracle`,
`controls.real.composite-wrong-rom`, `repo.p10-mutant-anchors` and `harness.phase10-qualification`).
Each counted name passed in both retained JUnit files of the record. The wrong-value controls are
named in the claim prose and not counted.

## Ids exercised and not claimed

The nine `unit.core.vg.*` names run on six registered ids: `rdp-depth-v7` (decal equality, primitive-Z
saturation, delta ceiling, Z exponent boundaries), `rdp-vi-v8` (NTSC clock and the first half of the
alpha clear), `rdp-vi-filter-v9` (serrate parity and the second half of the alpha clear),
`rdp-texture-v4` (perspective floor), `rdp-triangle-v3` and `rdp-blender-v6` (the shade floor and the
Bayer matrix). Each name is bound to one id in the claim file; the alpha clear on `rdp-vi-filter-v9`
and the Bayer matrix on `rdp-blender-v6` are exercised and not claimed. The four ids of the
descriptor 13 family are exercised by the depth, digest and import names (the import round trips add
the three save ids on a mid scan-out state with a save write); the claim blocks bind the stem id, and
the save ids `-e4k`, `-e16k` and `-s32k` are exercised and not claimed, except that block 3 binds
`-s32k` for the composite ROM. The representative VI sweep of `unit.core.digest.v26-completeness`
also runs on the state 25 stem id `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1`, which is exercised and
not claimed.

## Recorded exceptions for owner review

These items were decided under the delegated autonomy of the project policy and are recorded here
so the owner can review them. **None of them is an owner approval**, and none changes a Phase 9 or
earlier claim, tier or authority.

1. **Phase 9 claim-binding comparator (T-10-49).** `claims_final_problems` in
   `scripts/tests/test_phase09_qualification.py` compared the `manifest_digest` of the ten closed
   Phase 9 claim blocks with the SHA-256 of the live `fixtures/manifest.json`, which the plan 10-14
   manifest commit would have moved. It now compares each block with the pinned Phase 9 manifest
   identity (`sha256:b75864fd...`, the value the ten blocks always carried) and binds the 39 rows that
   manifest held by a canonical per-row view looked up by id, so a changed, removed or duplicated
   Phase 9 era row still refuses and an appended row does not move it. Every live consumer of the
   manifest was audited (PIN-IMPACT section 9), and the two earlier non-gate checks that already fail
   (`check-rsp-evidence.py --check`, a stale `phase06-final.json` pin) are recorded there and were
   neither caused nor fixed. Authority basis: delegated autonomy for routine reversible engineering,
   ROADMAP Phase 10 success criterion 5 ("adding a manifest row no longer requires an oracle-request
   rebind"), the locked GATE-02 decision and the owner approval of 2026-11-23 for per-row binding;
   this is **not an owner approval of this comparator change**. It is reversible test code
   (reverting restores whole-file binding). No Phase 9 claim content, tier or authority moved:
   `docs/claims/phase09.md`, `docs/evidence/phase09-qualification.md` and `fixtures/manifest.json`
   rows of Phase 9 are byte unchanged, and no closed digest was re-pinned.
2. **G4 gate-growth exception.** The required gate grew from 224 to 254 CTest names without removing
   equal time; the 30 Phase 10 names are the required evidence of this phase (K1 refusal pins, the VG
   vectors, the v26 identity, digest, export and import tests, the composite runs and the qualifier's
   own hermetic test) and no offset is claimed. Measured: the 30 names sum to 23.35 s natively (30.61 s
   at a higher host load in the rehearsal) and 305.08 s under the sanitizer selection (not a hosted
   lane); native `ctest --preset gate` wall 77.30 s at 254 names in the final record against 99.7 s
   at 224 names in the Wave 0 baseline (different host loads, so no saving or growth is read); hosted
   macOS ctest 265 s cold and 196 s warm on 254 names against 251 s on 224 names (F-111): +14 s
   against F-111, inside the 52 s spread, and +69 s against the warm run on the same names, 17 s
   beyond it. The measured cold `gate-macos` job is 731 s against 900 s (margin 169 s, one
   sample; the Gate-growth ledger is PIN-IMPACT section 6). The reversal rule did not trigger. The
   gap closure of plans 10-20 and 10-21 added no CTest name (254 still) and no offset; it grew the
   existing case `unit.core.digest.v26-completeness` by about 0.7 s natively (1.13 s in the final
   record) and, hosted, by 1.46 s on macOS and 2.83 s on Linux (run 45650890027, F-126), with the
   ctest wall of the 254 names at 194 s macOS and 128 s Linux; the warm `gate-macos` job of that
   run is 368 s. The cold run was not repeated.
3. **G5 plan-count exception.** Phase 10 took 21 plans (the 19 plans of the first pass plus the two
   gap-closure plans, 10-20 and 10-21) against a guideline of about 10 to 15. The four
   per-wave review plans (10-05, 10-08, 10-13, 10-18) are mandated by the autonomy policy
   ("review per wave in scratch clones") and the qualifier (10-17) is split from the one
   qualification (10-19); folding them would break the 4-task and about 45-minute plan cap. The two
   gap-closure plans exist because the verification of the first pass found the VI half of the
   flip-every-member criterion resting on an uncommitted probe (plan 10-20 commits the rows) and
   the record of plan 10-19 then no longer bound the tree (plan 10-21 re-qualifies).
4. **Two accepted wave 3 deviations (plan 10-15, reviewed in 10-18 as W3-01 and W3-02).** (a) One
   literal of `runner/tests/test_state_runner.c`, a file of the Phase 9 inventoried binary, moved from
   the `t64.state-container.v2` string to the v3 string, because v2 joined the closed set of format
   strings and the old "wrong format string" case would no longer reach the supervisor's usage
   refusal; the case, the case list, the inventory and every pin are unchanged (no pin hashes the
   file). (b) The wrong-ROM control compares its save through the 16-byte record the guest read back
   from the save device, a weaker comparison than a published save image, because a failing run
   publishes no save; the 188-byte transcript is compared whole against the oracle's wrong
   transcript, and the offset of the first difference (271) is the oracle's own.
5. **GATE-01 guard-removal decline.** The redundant `repo.rsp-no-hle` run in the audio records step
   stays (F-116): removing it needs edits of `scripts/qualify-audio.py` (the inventory digest literal)
   and `docs/spec/audio-cases.json`, which the closed Phase 8 qualifier freezes (F-106). The saving
   forgone is up to about 41 s on hosted macOS. The 15-minute limit alone fits the measured cold run,
   so the guard stays a stop-loss that is not counted as an offset. Owner: the next Phase 8
   requalification.
6. **One orchestrator rebind of `PINNED_REQUEST_SHA256`.** In plan 10-03 (commit `b64098f`, 2026-11-23)
   the orchestrator rebound the Phase 7 oracle request from
   `a4bb7774b72321261e37a44bbb518161b9b564e4beb076dfebe06881aab67733` to
   `e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab`, identity only: the whole-file
   manifest hash became 39 per-row pins, the 13 whole-file hashes of the other bound files are
   unchanged, `local_decision` stays blocked and no oracle, rights or publication authority was
   granted. The script has not changed since; every later manifest row was appended without a rebind.
7. **Native qualification retries.** Three failed attempts of the superseded plan 10-19 qualification
   (a supervisor wall backstop in eight Phase 8 and 9 tests under unrelated host load) preceded its
   passing attempt; all four are listed above with their record directories. No limit was widened
   and no source changed. The re-qualification of plan 10-21 (item 9) passed on its first attempt
   after a wait for the host load to fall to 8 or below. Whether a slow local host should be able to
   refuse a native qualification (a test hygiene question for Phase 11) is left for review; the
   evidence does not depend on it because the passing record verifies and the hosted runs of Phase
   10 passed.
8. **Deferred carry-ins at the G3 limit and the Phase 9 deferrals.** Five items are carried, the limit
   of five warning-level gaps of guardrail G3: W2-01 (the Z product bound `state_z_products_bounded` and
   its parity case are not pinned by a committed boundary test), W2-02 (`t64_has_si_v25_profile` and
   `t64_has_si_v26_profile` are not tokens of `scripts/list-membership-sites.py`, six site rows are
   missing from the generated list), DEF-W1-01 (three sibling copies of the perspective floor are
   not pinned behaviorally), W1-03 (one sentence of the `ceil` rule in `docs/spec/vi.md`) and D-10-17-01
   (the cold-boot tests stop working after 99 gate runs in one build tree; the counters stood at 10
   and 10 before the re-qualification, cap 99). W2-03 (the VI state had no committed completeness
   rows) is **closed, not carried**: plan 10-20 committed them and the record above ran them. The five
   items are listed in `ROADMAP.md` for the Phase 11 freeze-candidate review together with the Phase 9
   deferrals T1, T2 and T4 to T8 above. VID-04 stays deferred and unclaimed.
9. **Re-qualification after the gap closure (plans 10-20 and 10-21).** The first qualification (plan
   10-19) was taken before the VI half of the flip-every-member criterion was a committed test, so
   plan 10-20 changed a bound test source, the mutant driver and the qualifier text, and plan 10-21
   qualified the resulting tree again: an independent review of the 10-20 wave in a detached clone
   first (gate 254 of 254, the sanitizer run of the case, the full mutant run), then the new
   Qualification base, then one native qualification (record `20261125T155456703622Z-1fc0b1da`,
   passed on the first attempt, `--verify` exit 0), then the push of the verified head and its warm
   hosted pair. The cost of this exception is the 21-plan count of item 3, two more bound inputs
   in the snapshot (62 instead of 60) and one more mutant row; the plan 10-19 record, its four
   attempts and the whole cold-cache measurement of the earlier tree are kept, not rewritten. No
   limit, backstop or production source was changed by the gap closure, and the cold-cache hosted
   run (731 s of 900 s, F-124) was not repeated: it was measured on the earlier audited tree and the
   gap closure added test time only.

## Limitations and preserved stops

- Authored scope only: authored literals, authored ROM rows and independent oracle scripts. No hardware
  measurement, other-emulator comparison, oracle operation or commercial title was used or is claimed.
- The state 26, digest, container v2 and replay formats and the runner flags and records are
  build-identity-bound and provisional until the Phase 11 freeze candidate; no application or binary
  interface is frozen, and a state blob is not promised to import into another build.
- The depth sidecar is a logical software model (one byte per RDRAM halfword, with no claim about
  physical hidden bits) and the Z mode precedence and update override are software policy, not
  confirmed hardware behavior.
- A round trip compares a machine with its own export; the layout is pinned by separate export-side
  tests and an independent decoder (supporting tests, not counted).
- Hosted time: the measured cold `gate-macos` job is 731 s of 900 s on one sample; run-to-run spread
  on identical source is up to 52 s (F-119). Native qualification costs about 7 minutes on a quiet
  host and just over half of that is the sanitizer selection and the mutant driver (230 of 411 s); a loaded local host
  can refuse it at the supervised-run wall backstops of closed Phase 8 and 9 tests.
- Raw state, save, replay, capture and log files stay private and are never uploaded; this document
  holds digests, sizes, counts and public run numbers only.
- D5 (no oracle material), D6 (no tag, no release, no channel or versioning decision) and D7 (no
  branding, logo or store listing; the name is not trademark-cleared) stay in force. Phase 7 VID-04 and the oracle request stay blocked on oracle authority and the
  parallel-rdp rights audit. No ROM, firmware, PIF or IPL image of a commercial product, 64DD
  media or commercial save file was used; the only embedded IPL3 is the separately rights-cleared
  Unlicense payload recorded in the fixture manifest.

## Reproduction

The private records are untracked build outputs kept on the machine that ran the qualification; the
check below needs them and the public repository checkout at the qualified head.

```sh
git switch --detach 1fc0b1da70acf6c9b0f3baf0b46abf9d5a69468f
python3 -B scripts/qualify-phase10.py --verify build/gate/10-qualification-private/20261125T155456703622Z-1fc0b1da/record.json
git switch main
```

It must print `qualify-phase10: record verified against the current tree` and no rehearsal
statement. At a later head it is expected to refuse, because the evidence and claim documents are
newer bound inputs than the Qualification base (this was read from the qualifier and not re-run). The claim file check is `sh scripts/check-claims.sh --files docs/claims/phase10.md`
(`claims: files=1 blocks=8 errors=0`). The closed Phase 9 records are unchanged:
`git diff --quiet 4ac633e880e0c26005cb03092e8d8485e721034c HEAD -- docs/claims/phase09.md docs/evidence/phase09-qualification.md`
exits 0. The commits after the pushed head H3 change documents only. The superseded record of
plan 10-19 (`20261125T134440416529Z-b9d957c3`, head `b9d957c391b128622912481a99ffe24e6c84e609`)
stays under the same private directory and no longer verifies against this tree, by design.
