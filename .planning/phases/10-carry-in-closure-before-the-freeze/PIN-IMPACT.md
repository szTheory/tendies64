# Phase 10 pin-impact map, identity record and Wave 0 decisions

Written by plan 10-01 (phase-wave 0) at the Wave 0 base recorded below. Evidence
tier: code observations and measurements at that base; nothing here is a
compatibility or hardware claim. No oracle material (D5) was used. All paths are
repo-relative.

## 1. Pin-impact map

Every edit class of Phase 10 against the pin or check it moves, with the cheapest
command that detects the move. Run the cheap check in the commit that makes the edit.

| Edit | Pin or check it moves | Cheap check |
|------|-----------------------|-------------|
| any of the 13 files in `EXPECTED_CORPUS_PATHS` of `scripts/check-rdp-oracle-authority.py` (14 at the Wave 0 base; `fixtures/manifest.json` left the whole-file set in plan 10-03, see the next row): `core/tests/test_rdp.c`, `test_rdp_copy.c`, `test_rdp_triangles.c`, `test_rdp_texture.c`, `test_rdp_combiner.c`, `test_rdp_blender.c`, `test_rdp_depth.c`, `core/tests/test_vi.c`, `test_video_output.c`, `runner/tests/test_rdp_video.c`, `scripts/check-rdp-vi-cases.py`, `docs/spec/rdp-vi-cases.json`, `fixtures/rdp/fill.S` | whole-file SHA-256 and blob id of each file, hence the derived request hash `PINNED_REQUEST_SHA256` (line 23); refusal text "authored corpus SHA-256 changed". Edits belong to the orchestrator only | `ctest --test-dir build/gate -R '^harness\.rdp-oracle-authority$'` and `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` |
| `docs/spec/rdp-vi-cases.json` (needed to list new cases of a Phase 7 binary) | `CASE_INVENTORY_SHA256` at `scripts/qualify-rdp-vi.py:28`, enforced at `:561` by the hosted step "Emit authored raw RDP VI evidence", plus the oracle request | replay the hosted emit step (`sh scripts/preflight-phase10.sh`); `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite <name>` (not in the gate) |
| a `fixtures/manifest.json` row | per-row pin of every row present at the rebind (39 rows, each by its own canonical hash in `corpus.fixture_manifest.bound_rows`, looked up by id); appended rows need no rebind (verified by `append-unrelated-row`); changing, removing or duplicating any existing row still refuses (verified by `existing-row-changed`, `bound-row-changed` and `duplicate-bound-id`). The `phase09-*` rows are F2-pinned and are never touched. The Phase 9 claim blocks bind the manifest by the pinned Phase 9 identity plus a per-row view (section 9) | `ctest -R '^harness\.rdp-oracle-authority$'`; `ctest -R '^harness\.phase09-qualification$'` |
| any core `.c` or `.h` text | the build id fingerprint (`core/CMakeLists.txt:20-66`), hence packet-bearing digests of pre-v25 ids; the sealed function bodies of `REVIEWED_BODY_DIGESTS` when a sealed body changes (rows by file at the base: `t64_rdp_depth.c` 14, `t64_rdp.c` 24, `t64_rdp_pipeline.c` 20, `t64_rdp_raster.c` 26, `t64_ai.c` 19, `t64_bus.c` 4, `t64_si.c` 1, `t64_save.c` 1, `t64_pi.c` 1) | `python3 -B scripts/check-rsp-no-hle.py --build build/gate`; the two-tree digest sweep tool of plan 10-02 |
| `docs/spec/phase09-cases.json`, existing `phase09-*` rows, existing profile fields | the F2 identity pins of `harness.phase09-qualification` (`scripts/tests/test_phase09_qualification.py` F2 block) | `ctest -R '^harness\.phase09-qualification$'` |
| `docs/spec/audio-cases.json` or removal of `repo.rsp-no-hle` from its `qualification` suite | `EXPECTED_INVENTORY_SHA256` at `scripts/qualify-audio.py:26`; the native plan lines that require `PASS: rsp-no-hle` | `ctest -R '^harness\.audio-qualification$'`; the post-gate replay of `sh scripts/preflight-phase10.sh` |
| `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh`, `scripts/gate.sh` | bound source of Phase 9's closed record (tuples at their own commits; no re-verification of the old record); the CI-TIMEOUT rule and its selftest move together | `ctest -R '^repo\.ci-yml(-selftest)?$'` |
| a descriptor membership site | `docs/spec/profile-membership-sites.txt` (generated) | `ctest -R '^repo\.si-membership-sites$'` |
| a new used state member | `docs/spec/state-field-rules.txt` and `docs/spec/state-mutants.txt` | `ctest -R '^harness\.state-field-use-audit$'` |
| a new CTest name | the exact inventory lists of `scripts/check-test-inventory.sh` (`PHASE10_TESTS` block), the label count of `sh scripts/preflight-phase10.sh`, the regex rows of `scripts/preflight-phase10-regexes.txt` | `sh scripts/check-test-inventory.sh --preset gate` and `--selftest` (each new name costs about 0.2 s in the selftest) |

Standing consequence (research correction C1): new VG, depth, v26 and composite
tests go in new files with new CTest names. The 14 bound files are not edited and
no case is added to a binary listed in `docs/spec/phase09-cases.json` or
`docs/spec/rdp-vi-cases.json`.

## 2. Registered identity record (one-way, recorded before the first digest pin)

Decided by CONTEXT "Identity naming" (research A1, A4). Verified free at the Wave 0
base: the current maxima of `core/t64_machine.c` are descriptor_version 12,
machine_model_version 20 and state_serialization_version 25, so the new tuple does
not collide with any shipped profile.

| Registered id | save_kind | length (limit `T64_PROFILE_TEXT_CAPACITY` 64) |
|---------------|-----------|------------------------------------------------|
| ntsc-cart-4m-openipl3-rdram-b-t1-si-v2 | none | 38 |
| ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-e4k | EEPROM 4K | 42 |
| ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-e16k | EEPROM 16K | 43 |
| ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k | SRAM 32K | 43 |

Tuple and format record:

| Item | Value |
|------|-------|
| profile tuple of the four ids | descriptor_version 13, machine_model_version 21, state_serialization_version 26 (the si-v1 family keeps 12, 20, 25) |
| state digest domain | `t64.state.si.26` (si-v1 keeps `t64.state.si.25`, `core/t64_digest.c:278`, `core/t64_state.c:389`) |
| state container header version word | 2 for state 26, 1 for state 25 (`T64_STATE_CONTAINER_VERSION`, `docs/spec/state-container.md:36`) |
| public container format string | `t64.state-container.v2`, accepted only with the state-26 ids; `t64.state-container.v1` (`T64_STATE_CONTAINER_FORMAT`, `t64_machine.h:410`) only with the state-25 ids |
| RUN-07 profile | the `-s32k` id (SRAM 32K); all four ids are registered |
| depth state placement | depth scalars inside the RDP block (the importer's render-in-progress proof needs them before the tail); the hidden depth table in the section 8 tail after the coverage table |
| predicate design | `t64_has_si_v25_profile` (exact 12, 20, 25); `t64_has_si_v26_profile` (exact 13, 21, 26); `t64_has_si_profile` widened to the union so no sealed `.c` body text changes; `t64_has_rdp_depth_profile` gains the v26 term outside the descriptor 10 conjunction, so ids 11 and 12 keep their Z refusal (K1) |

One-way: the four ids, the tuple, the digest domain, the container version word and
the format string feed every later pin, replay binding and the Phase 11 consumer.
They are recorded here before any digest pin exists and are frozen once plan 10-09
pins a digest; a later change needs a new identity bump, not an edit.

### 2.1 Pins captured by plan 10-09 (the identity record is now frozen)

Written by plan 10-09 (phase-wave 2). Evidence tier: code observations and unit-test
results at commit `286ccbb` (registration) and `831c947` (depth activation); nothing here
is a compatibility or hardware claim. The profile hashes below are the one-way pins of
`unit.core.profile.v26-identity` (`core/tests/test_profile_v26.c`); each equals a SHA-256
the test computes over the serialized descriptor bytes outside the core.

| Registered id | Profile SHA-256 | Serialized bytes |
|---------------|-----------------|------------------|
| ntsc-cart-4m-openipl3-rdram-b-t1-si-v2 | `3bc028bdb4c6f9622e20ecfff828a26245adc04ea69ddeeefdbb08d5b27dbc47` | 1358 |
| ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-e4k | `0a4bfc455adc8b66c4666087c71ff5c0ce3b2f46ab6c83784a37d862956c1fe8` | 1362 |
| ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-e16k | `34f767b0a9495d604719288515951c2e2ae726ef661f6a7c5fc378397718536c` | 1363 |
| ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k | `9dfb90d8055b822a1767be1c5594d1b2d904f042ef6fe82f2e7a3243e5756578` | 1363 |

| Item | Value |
|------|-------|
| descriptor body | equals the si-v1 body except the id digit, the three version words (12 to 13, 20 to 21, 25 to 26) and the `boot_status` text, which is `prepared-rdram-b-t1-si-v2` (si-v1 keeps `prepared-rdram-b-t1-rdp-vi-filter-v9-ai-v1`); the test proves exactly four differing bytes outside the removed `boot_status` field; `rdp_depth_policy` keeps the shared `z-q16-floor8192-cfloor-p15s3-dzmax-eq-icvg-xlu-decal-h2-v1` text |
| v26 stream layout (domain `t64.state.si.26`) | framing exactly as v25 (section ids 1 to 9, plus 10 and 11 for the three save ids); the five depth scalars are a 20-byte new-span block inside section 8 immediately before `work_policy_version`; the section 8 tail is the 88-byte combiner block, the 4-byte coverage length and table, then the 4-byte hidden depth length and the hidden table, ending at the section 9 marker |
| exact versus family predicates | `t64_has_si_v25_profile` and `t64_has_si_v26_profile` select the digest domain and the state version registry; `t64_machine_state_export` uses the v25 predicate until plan 10-11; `t64_has_si_profile` is the union and widens the ISViewer window (`core/t64_isviewer.c`, eight ids) |
| pins that did not move | the four si-v1 F2 profile hashes (`harness.phase09-qualification`), the five K1 refusal digests, and the digest sweep of the 21 shipped ids against the Wave 2 base (1599 shared lines, 0 differences; the four si-v2 ids add 64 head-only digest lines each) |
| sealed bodies | none changed; `scripts/check-rsp-no-hle.py --build build/gate` passes with no re-pin |

### 2.2 Wave 2 shipped-id proof (plan 10-10)

Written by plan 10-10 (phase-wave 2). Evidence tier: code observations and unit-test
results at the working tree of commit `9a5dfd8` plus the Task 3 test edit; no core
source (`core/t64_*.c`, `core/t64_internal.h`, `core/include`) differs from the plan 10-09
head `3d5b3bd`, so the sweep head is the same machine as that plan's. Nothing here is a
compatibility or hardware claim.

| Item | Value |
|------|-------|
| base | the Wave 2 base of section 5 (`6dd785986f6aa78c6da5573a558b15b386a4edee`, read with the 40-hex sed, `grep -cEx`, `git cat-file -e`; never `HEAD`) |
| command, as the plan prints it | `python3 -B scripts/digest-sweep.py --base "$base" --head WORKTREE` gives `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (the 21 shipped ids) |
| command, with the four head-only ids | the same plus four `--head-extra-id` (the si-v2 ids) gives `sweep: 1599 shared lines, 0 differences, 292 head-only lines`: per si-v2 id 64 digest lines and 9 stop or skip lines (4 x 73) |
| shipped-id byte identity | 21 of 21 shipped ids at every ROM and checkpoint of the sweep (section 7), 0 changed lines and 0 lines present only in the base |
| K1 refusal | `unit.core.k1.dispatch-refusal` and `unit.core.k1.apply-refusal` pass (the five shipped composite ids still refuse SetZImage, SetPrimDepth and the four Z triangles) |
| predicate scope | `unit.core.profile.v26-predicate-matrix` prints `rows=25` (17 earlier ids, four si-v1, four si-v2): `t64_has_rdp_depth_profile` is true on exactly seven ids (rdp-depth-v7, rdp-vi-v8, rdp-vi-filter-v9 and the four si-v2 ids) and false on the descriptor 11 id and the four descriptor 12 ids |

## 3. Wave 0 decisions

| Decision | Choice | Reason |
|----------|--------|--------|
| Hosted footprint of the Phase 10 qualification | none new: no new `ci.yml` step; the new CTests run inside the existing Gate step | the new names are ordinary CTests and the post-Gate steps are Phase 8 and 9 retained-evidence producers that this phase does not change, so `ci.yml` is edited once, by GATE-01 |
| `scripts/gate.sh` | no Phase 10 private JUnit root; the Phase 10 qualifier writes its own JUnit with `ctest --output-junit` under `build/gate/10-qualification-private`, outside `gate.sh` | the allow-list at `gate.sh:102-105` names only the Phase 8 and 9 roots and `gate.sh` is bound source of Phase 9's record |
| GATE-02 scope | per-row oracle pins replace only the whole-file `fixtures/manifest.json` hash; the other 13 bound files keep whole-file hashes and every Phase 10 test goes in a new file; an unavoidable edit of a bound file is one batched orchestrator identity-only rebind, recorded in this file | new files are cheap to move, while per-row pins for all 14 files would change the authority checker's binding contract again (costly, orchestrator-only) |
| Redundant `rsp-no-hle` guard removal | measured last in plan 10-04 and declined if it needs a `scripts/qualify-audio.py` edit | `docs/spec/audio-cases.json` lists `repo.rsp-no-hle` in its qualification suite and is pinned by `EXPECTED_INVENTORY_SHA256`; F-106 keeps the Phase 8 qualifier frozen; the 15-minute limit alone already fits the 673 s cold projection |
| Optional `make -j2` in `scripts/build-boot-fixtures.sh` | declined (CONTEXT discretion item 3) | it changes the Binutils cache key (`hashFiles('scripts/build-boot-fixtures.sh')` in `ci.yml`) and a bound script for a cold-only benefit |

## 4. Preflight measurements

`sh scripts/preflight-phase10.sh` at the commit that added the full step list
(base `612a3ad`, the merge base with `origin/main`). Whole seconds from one
`date +%s` source; the total equals the step sum. The 1-minute host load stayed above
the script's WARN threshold of 8 in every run, so these totals are an upper bound for
a quiet host.

| Configuration | Host load (1 min) | Steps in seconds | Total |
|---------------|-------------------|------------------|-------|
| no C change since BASE (linux-compile skipped), post-gate replay on | 9.5 to 12.7 | oracle-authority 0, ctest-counts 0 to 1, host-load 0, head-ci 1 to 2, timeout-audit 0, pii 0 to 1, linux-compile 0 to 1, post-gate-replay 20 to 22 | 23 s, 23 s and 24 s in three runs (target of 90 s met) |
| forced `--base 7de85af` (C change since BASE, first path `core/t64_digest.c`) | 12.7 | as above, linux-compile 47 (219 compile units with the act image cc 13.3.0), post-gate-replay 21 | 70 s |
| `--no-replay` | about 10 | post-gate-replay 0 (skipped with its reason printed) | 2 s |

Observations: at the base, `origin/main` (`612a3ad`) has no hosted `ci.yml` run
(the newest run on main is `928c9d8`, completed, success); the timeout audit checked
96 registered TIMEOUTs against the recorded native seconds with no value below 3.5 times
(lowest ratio 14.3, `harness.state-field-use-audit` TIMEOUT 240 s against 16.79 s) and
no unmeasured test; two consecutive runs pass because each replay takes a fresh
`GITHUB_RUN_ID`, and the replay removes only the artifacts, private records and
`build/evidence` files that the invocation created. The first run after a commit can
include a rebuild, because the revision annotation of the core units follows HEAD.

## 5. Wave bases

Format and rule (standing rule "Base records" of the Phase 10 plans): a base is the
40-hex output of `git rev-parse HEAD` taken after the previous wave's last commit and
before the next wave's first commit, written here as exactly one line per label and
committed in a commit that changes only this file. Later labels are appended by plan
10-05 for Wave 1, plan 10-08 for Wave 2, plan 10-13 for Wave 3 and plan 10-18 for the
Qualification base, each after its plan's last non-recording commit. The Wave 0
record below is the only line of this form written by plan 10-01.

- Wave 0 base: 4ac633e880e0c26005cb03092e8d8485e721034c
- Wave 1 base: f6ab8c26d008b8301b729b5aae2715e1b1079318
- Wave 2 base: 6dd785986f6aa78c6da5573a558b15b386a4edee

## 6. Gate-growth ledger

ROADMAP guardrail G4 says the required gate may not grow without removing equal time;
the ROADMAP Phase 10 "G4 exception" records that it grows here because the new tests
are the required evidence of this phase and no offset removes measured gate time.
This ledger measures that growth instead of projecting it.

Baseline, measured at the Wave 0 base:

| Measure | Value |
|---------|-------|
| native `ctest --preset gate`, 224 names, 4 jobs | 99.7 s wall, 100 percent passed, at 1-minute host load 13 to 16 (above the quiet-host threshold of 8; the preflight prints WARN there) |
| hosted macOS ctest, F-111 (run 44795894664, source `d3211e9`) | 251 s |
| hosted macOS ctest, F-114 (run 44801811117, docs-only head `928c9d8`) | 215 s; F-114 records a run-to-run spread of up to 52 s on identical bound source (gate-macos 349, 420, 433, 435 and 487 s) |
| hosted gate-macos cold projection before Phase 10 | 673 s (F-111) and 621 s (F-114) against the 900 s limit, margin 227 s and 279 s |

One row per wave is appended by the review plans (10-05 Wave 0, 10-08 Wave 1,
10-13 Wave 2, 10-18 Waves 3 and 4); plan 10-17 records the measured total before the
cold run. Row format: the wave, the new CTest names, the measured added native seconds
(the sum of the new names' seconds from `build/gate/Testing/Temporary/CTestCostData.txt`),
the hosted macOS ctest delta against the baseline above, the offset (seconds removed
elsewhere) or the G4-exception reason (why the time is required Phase 10 gate evidence,
for example a pre-freeze contract test that Phase 11 builds on), and the remaining
margin to the 900 s gate-macos limit.

| Wave | New CTest names | Measured added native seconds | Hosted macOS ctest delta | Offset or G4-exception reason | Margin to 900 s |
|------|-----------------|-------------------------------|--------------------------|-------------------------------|-----------------|
| 0 (plans 10-01 to 10-04, head `33f6e9c`, run 45311817943) | `repo.p10-mutant-anchors`, `unit.core.k1.dispatch-refusal`, `unit.core.k1.apply-refusal` (224 to 227 names) | 0.36 s (0.03, 0.10 and 0.23 s from `CTestCostData.txt`; none carries `COST`); the `check-test-inventory.sh` selftest cost of about 0.2 s per name is not separately measured | ctest wall 184 s against 251 s (F-111) and 215 s (F-114): 67 s and 31 s lower. Not read as a saving or growth: the 52 s run-to-run spread of F-119 covers the second delta, and the first coincides with the `COST` start order (`unit.core.state.import` 2nd of 227, 162.57 s against 210.19 s), which one sample cannot separate from runner spread | G4 exception (ROADMAP Phase 10): the three names are required pre-freeze evidence (K1 pins Z refusal of the five shipped composite ids before any depth predicate edit that plans 10-09 to 10-12 and Phase 11 build on; the anchor test keeps the mutant table honest). No offset is claimed: `COST` reorders starts and removes no test time, and the rsp-no-hle guard removal stays declined (F-116) | warm: gate-macos run 346 s (Gate 298 s), margin 554 s. Cold projection: warm plus the Binutils build delta of 186 s that F-111 and F-114 both measured is 532 s, margin 368 s; the measured cold run is plan 10-19 |
| 1 (plans 10-06 and 10-07, head `a0c0a38`, run 45321391540) | `unit.core.vg.decal-equality`, `unit.core.vg.primitive-z-saturation`, `unit.core.vg.delta-ceiling`, `unit.core.vg.z-exponent-boundaries`, `unit.core.vg.ntsc-clock`, `unit.core.vg.rgba5551-alpha-clear`, `unit.core.vg.serrate-filter-parity`, `unit.core.vg.perspective-floor`, `unit.core.vg.shade-floor-bayer` (227 to 236 names) | 0.14 s (one fresh run in the review scratch clone, from `CTestCostData.txt`: 0.004, 0.003, 0.004, 0.021, 0.098, 0.003, 0.002, 0.002 and 0.003 s; none carries `COST`; 0.07 s as the running mean of 4 to 11 runs in the shared tree). The `check-test-inventory.sh` selftest cost of about 0.2 s per name (section 1) was 20 s in the scratch gate against 19 s at the Wave 0 review, which host load hides | ctest wall 187 s against 184 s in the F-120 row: +3 s, inside the 52 s run-to-run spread of F-119, so no growth is read. The nine names ran in 0.29 s on macOS and 0.51 s on Linux, and the macOS selftest step took 38 s against 42 s | G4 exception (ROADMAP Phase 10): the nine names are the required independent VG-01 to VG-05 and standard-NTSC clock evidence of VER-06 that the Phase 7 verification left open, each with a killed mutant, and the Phase 11 freeze builds on them. No offset is claimed: the rsp-no-hle guard removal stays declined (F-116) | warm: gate-macos run 326 s (Gate 288 s), margin 574 s. Cold projection: warm plus the Binutils build delta of 186 s is 512 s, margin 388 s; the measured cold run is plan 10-19 |

Reversal rule of the G4 exception (ROADMAP Phase 10): if the measured cold-cache
gate-macos run of plan 10-19 exceeds 900 s or is cancelled at the limit, a new numbered
plan moves non-essential Phase 10 cases (sampled sanitizer runs, duplicate-path round
trips) to a non-gating lane and the cold run is repeated. The exception covers Phase 10
only; Phases 11 and 12 follow G4 as written. The redundant `rsp-no-hle` guard is a
stop-loss that is not counted as an offset unless its removal is measured.

## 7. Sweep baseline

Written by plan 10-02 (phase-wave 0) before any identity edit. Evidence tier: a
measurement of the tree at the commit below; it states that the sweep tool works
and what it covers, not that any later tree is unchanged.

Tool: `scripts/digest-sweep.py` (driver) and `tools/digest-sweep/t64_digest_sweep.c`
(per-tree printer). Command, with a 40-hex base (the driver refuses `HEAD`, a
ref name or a short hash as `--base` with exit 2):

    base=$(git rev-parse HEAD) && python3 -B scripts/digest-sweep.py --base "$base" --head HEAD

It extracts the base commit and the head tree with `git archive` into
`build/10-sweep/<role>-<label>` (the main `build/gate` tree and `build/fixtures`
are never written), rewrites the `set(T64_BUILD_ID ...)` line of `core/CMakeLists.txt`
in each scratch copy to `t64-build-v1-sweep` (the build identity enters the
packet-bearing digests; the 09-10 comparison with a different identity differed in
30 of 792 lines), builds only `t64_core` and `t64_sha256` (Ninja, Release,
`T64_WERROR=ON`), runs the printer on both with the same `build/fixtures`, sorts
the lines by id, ROM and checkpoint and compares them. The head list may carry
extra ids (`--head-extra-id`) for the four ids of plans 10-09 onward; the base list
is always the 21 shipped ids. Checkpoints: guest ticks 0, 1, 16, 256, 4096, 65536,
262144, 1048576, 2097152 and 4194304; a resource-limit stop is continued as a yield
(the replay player rule, at most 64 calls without progress: the digest of a stalled
machine depends on how many advance calls were made) and a terminal stop (guest completion, CPU unsupported or a
stalled resource limit) is printed as a `stop` line and ends that ROM. Candidate ROMs:
the eight authored or cleared fixtures `ipl3_compat`, `hello`, `rdp-fill`,
`audio-pcm` and the four `phase09-admit-*` images, plus one generated authored 4 KiB
image (`authored-4k`) that only the skeleton id admits (status 5, invalid ROM, for the
other 20). The report file records the SHA-256 of every candidate fixture.

Baseline commit (informational; later sweeps use the Wave 0 and Wave 2 base records
of section 5, never this line and never `HEAD`): `18e41b47c173e6cdcccda8805a715b57742f9b90`

| Measure | Value |
|---------|-------|
| shared lines between two trees of this commit | 1599 (0 differences, 0 head-only lines) |
| line kinds | 1460 digest lines, 115 stop lines (reasons: guest completion 55, CPU unsupported 42, stalled resource limit 18), 20 skip lines (all `authored-4k`, status 5), 4 export lines |
| distinct ids with at least one digest line | 21 of 21 |
| export lines | the four si-v1 ids (state container hash of the first admitted ROM at its last checkpoint) |
| seconds per run (one tree, printer only) | 40.9 to 60.4 (38.1 to 38.8 with the earlier bound of 8 yields) |
| seconds per tree build (t64_core and t64_sha256 only) | 0.1 to 3.7 |
| wall seconds for a two-tree comparison (builds and sweeps in parallel) | 60.6 at 1-minute load 8.5 |
| wall seconds for `--selftest` (three sweeps, base reused) | 91.2 |
| 1-minute host load during the runs | 7.9 to 10 (other sessions on the same machine; an upper bound for a quiet host) |

Per-id coverage at the baseline commit:

| Registered id | ROMs admitted | digest lines | stop lines | export line |
|---------------|---------------|--------------|------------|-------------|
| ntsc-cart-4m-skeleton | 9 | 90 | 0 | no |
| ntsc-cart-4m-openipl3-warm-v1 | 8 | 64 | 8 | no |
| ntsc-cart-4m-openipl3-vr4300-v1 | 8 | 74 | 3 | no |
| ntsc-cart-4m-openipl3-vr4300-compat-v1 | 8 | 74 | 3 | no |
| ntsc-cart-4m-openipl3-vr4300-compat-v2 | 8 | 74 | 3 | no |
| ntsc-cart-4m-openipl3-rsp-functional-v1 | 8 | 74 | 3 | no |
| ntsc-cart-4m-openipl3-rdram-a-t1-v1 | 8 | 74 | 3 | no |
| ntsc-cart-4m-openipl3-rdram-b-t1-v1 | 8 | 74 | 3 | no |
| ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2 | 8 | 64 | 8 | no |
| ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3 | 8 | 64 | 8 | no |
| ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4 | 8 | 64 | 8 | no |
| ntsc-cart-4m-openipl3-rdram-b-t1-rdp-combiner-v5 | 8 | 64 | 8 | no |
| ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6 | 8 | 64 | 8 | no |
| ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7 | 8 | 74 | 3 | no |
| ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8 | 8 | 74 | 3 | no |
| ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9 | 8 | 74 | 3 | no |
| ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1 | 8 | 64 | 8 | no |
| ntsc-cart-4m-openipl3-rdram-b-t1-si-v1 | 8 | 64 | 8 | yes |
| ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k | 8 | 64 | 8 | yes |
| ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k | 8 | 64 | 8 | yes |
| ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k | 8 | 64 | 8 | yes |

Proof that the sweep can fail (`python3 -B scripts/digest-sweep.py --selftest`, exit 0):
identical trees (base and head both the commit at selftest start, each extracted and
built on its own) report 0 differences in 1599 shared lines; a head scratch copy whose
`core/t64_digest.c` has one character of the state version 20 domain string changed
(`t64.state.rdp.vi.blender.20` to `...2X`, scratch copy only) reports 64 differences,
all in the `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6` id; `--base HEAD` is
refused with exit 2.

## 8. GATE-02 per-row oracle pin record

Written by plan 10-03 (phase-wave 0). Evidence tier: code observation and test
results at the commit that records it. The change is an identity-only rebind of the
Phase 7 oracle request: it grants no oracle, rights or publication authority, and
`local_decision` stays blocked.

| Item | Value |
|------|-------|
| what changed | `corpus.fixture_manifest` of `docs/evidence/phase07-oracle-authority.json` went from `{path, sha256, git_blob_id_sha1}` (whole file) to `{path, bound_rows: [{id, row_sha256}]}`; `fixtures/manifest.json` left `EXPECTED_CORPUS_PATHS` and `bound_authored_inputs` |
| bound rows | 39 (every manifest row at the rebind, manifest order, `rdp-fill` and `rdp-fill-payload` included); `row_sha256` is `_canonical_sha256` of the row |
| whole-file bound set | 13 paths (14 at the Wave 0 base) |
| old `PINNED_REQUEST_SHA256` | `a4bb7774b72321261e37a44bbb518161b9b564e4beb076dfebe06881aab67733` (reproduced from the committed record before the edit) |
| new `PINNED_REQUEST_SHA256` | `e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab` |
| who | the orchestrator, commit `b64098f` (checker edit, record, `phase07-oracle-authority.md`); the executor wrote only the tests (`ff341a0`, then the BOUND_FILES alignment of this plan) |
| unchanged | `TRUSTED_LOCAL_DECISION_SHA256` and `TRUSTED_PUBLICATION_DECISION_SHA256` stay `None`; `--require-approved` exits 2 with "no pinned competent owner/legal decision"; `--expect-blocked` exits 0 with "blocked: no oracle operation is authorized"; VID-04 stays deferred |
| selftest | 26 controls (22 plus `fixture-manifest-empty-bound-rows`, `fixture-manifest-duplicate-bound-id`, `fixture-manifest-bound-row-changed`, `fixture-manifest-bound-row-missing`) |

Behavior proved by `scripts/tests/test_rdp_oracle_authority.py` against scratch
mirrors (RED report of Task 1 in `build/gate/10-03-task1/red.txt`: exactly
`append-unrelated-row`, `old-whole-file-mutation` and `reorder-bound-rows` failed
against the whole-file checker; all nine cases pass after the rebind): an appended
row and a whitespace-only reformat leave the record valid and blocked; a reorder is
accepted (lookup by id); a changed bound row (`rdp-fill` sha256, size, source_rights,
binary_rights, distribution, `rdp-fill-payload` sha256), a changed or removed row the
request does not consume (`hello-openipl3`), a duplicated bound id, a row cloned under
another id, a missing manifest, an empty or absent row list refuse with the checker's
own exit 2.

Consequence for later plans: appending a manifest row (plan 10-14) needs no oracle
rebind. Changing, removing or duplicating any of the 39 bound rows is still an
orchestrator-only identity-only rebind.

## 9. Phase 9 claim binding (recorded exception)

Written by plan 10-03 Task 4 (checker B1). Evidence tier: code observation and test
results at the commit that records it; no claim, tier, count or oracle revision changes.

**What changed.** `claims_final_problems` in `scripts/tests/test_phase09_qualification.py`
compared each of the ten claim blocks' `manifest_digest` with the SHA-256 of the live
`fixtures/manifest.json`. That is a second whole-file manifest binding in the tree (the
first was the Phase 7 oracle request, section 8): the plan 10-14 manifest commit would
have moved it and failed `harness.phase09-qualification` with "block N manifest_digest ...
differs". It now compares each block with `sha256:` plus the pinned constant
`PHASE09_CLAIM_MANIFEST_SHA256`, and calls the new `claim_manifest_problems(manifest_bytes)`
once. That function looks the 39 pinned ids (`PHASE09_CLAIM_MANIFEST_IDS`) up by id, never by
position, requires each exactly once, and compares the canonical digest of the view
`{schema, schema_version, entries}` (the 39 rows in pinned order, encoded as
`_canonical_sha256` of `scripts/check-rdp-oracle-authority.py` encodes) with
`PHASE09_CLAIM_MANIFEST_VIEW_SHA256`. Rows whose id is not pinned (later phases) are ignored.
The three constants sit outside `F2_PINS` and `F2_PIN_KEYS`.

| Before | After |
|--------|-------|
| block `manifest_digest` equals sha256 of the live manifest file | block `manifest_digest` equals the pinned Phase 9 qualification manifest digest; the 39 rows that manifest held are bound by a canonical per-row view; appended or moved rows do not move either |

**Values reproduced on the unchanged tree before the edit.**

| Value | Observed |
|-------|----------|
| sha256 of `fixtures/manifest.json` (the `manifest_digest` of all ten blocks) | `b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0` |
| manifest entries | 39 (39 distinct ids) |
| canonical view digest | `beb2b1c8b209480199ce53d0ce30c51887bfb0769d7a6d522fd7bbe7bf2aec5f` |

**Unchanged.** `docs/claims/phase09.md`, `docs/evidence/phase09-qualification.md`,
`fixtures/manifest.json`, the `F2_PINS` literal, tiers, counts and oracle revisions
(proved against the Wave 0 base `4ac633e880e0c26005cb03092e8d8485e721034c` by the Task 4
verify: `git diff --quiet` on the three files and an equal `ast.unparse` of the `F2_PINS`
literal). No Phase 9 claim content, tier or authority changes. No closed digest is
re-pinned.

**Evidence.** RED (`build/gate/10-03-task4/red.txt`, unchanged comparator): the appended
row and both reorder variants (`rdp-fill` moved to the end, every row reversed) failed
with "manifest_digest ... differs"; all fifteen refused controls and the block-digest
control already passed on the common needle `claims-final:`; no needle-setup error.
GREEN (`build/gate/10-03-task4/green.txt`): the same controls pass and every refused
control's line begins `claims-final: manifest` (changed, removed, duplicated or
re-id'd row, schema change, empty or malformed manifest). Mutation check in a scratch copy:
without the view-digest comparison the three changed-row controls fail; with a count test
that allows duplicates the duplicated-row control fails; with the live-file digest back in
the block comparison the three accepted controls fail.

**Consumer audit** (every live-tree consumer of `fixtures/manifest.json`; observed exit
codes are for the Wave 0 base, a clean local clone of `4ac633e`, and for this checkout):

| | Consumer | Exercised by | Effect of an appended row |
|---|----------|--------------|---------------------------|
| (a) | `claims_final_problems` in `scripts/tests/test_phase09_qualification.py` | `harness.phase09-qualification` (this task) | none after this task; changed, removed or duplicated Phase-9-era rows refuse |
| (b) | `scripts/check-rdp-oracle-authority.py` | `harness.rdp-oracle-authority` | none; per-row since Tasks 2 and 3 (section 8) |
| (c) | `scripts/qualify-phase09.py` `SOURCE_PATHS` (the whole `fixtures/` directory, `scripts/tests/test_phase09_qualification.py`, `runner/`) | no gate consumer: every snapshot mode requires the checked-out HEAD to equal the record's commit, so a closed Phase 9 record is verified only at its own commit; `--emit-hosted` (ci.yml Phase 9 emit steps and the preflight post-gate replay) snapshots the current commit fresh and cannot refuse on an appended row; `--verify-evidence` is run by no CTest, `gate.sh` or `ci.yml` step | none |
| (d) | `scripts/qualify-audio.py` `SOURCE_PATHS` | no gate consumer of the binding: `harness.audio-qualification` loads the module but never calls `_source_snapshot` or computes `input_manifest_sha256`; the only live use is the ci.yml audio emit step and its preflight replay, which snapshot the current commit | none |
| (e) | `scripts/check-rsp-evidence.py` with `docs/evidence/phase05-final-rsp-evidence.json` (`verification_sha256` over all of `scripts/` plus `fixtures/manifest.json`) | no gate consumer: only `check-rsp-evidence.py --check` reads it, no CTest runs it (`harness.rsp-evidence-ownership` runs the single archive test `test_owned_tables_survive_milestone_archive`). Observed: `--check` exits 1 ("final RSP evidence is incomplete or inconsistent") at the Wave 0 base and in this checkout; recorded `verification_sha256` `be5b9776...` against computed `97d76933...` at the base. Pre-existing failure, neither caused nor fixed here | none that matters (already failing) |
| (f) | `scripts/check-base-evidence.py` with `docs/evidence/phase06-final.json` (pins manifest sha256 `9ed5d63f4928b340e8ceb8e5dd965c860b8adb80eb2f5bbe42a8322b6a2e0ad1`; the live manifest is `b75864fd...`, a pre-existing mismatch) | `harness.base-evidence` uses its own scratch inputs: unaffected. Observed in this checkout: `--check-current` exits 0 and `--check-artifacts` exits 2 ("BASE evidence refused", the private artifacts are absent). In a clean clone of the base both exit 2 because `build/private-base-diagnostic` does not exist there | none; pre-existing pin mismatch recorded |
| (g) | the prose digest in `docs/evidence/phase09-qualification.md` | no checker reads it (a repo-wide search for the digest finds only that file, `docs/claims/phase09.md`, the test constants and planning records) | none; it names the qualification-time manifest |

**Authority basis, stated honestly.** Delegated autonomy for routine reversible
engineering (`.planning/AUTONOMY.md`); ROADMAP Phase 10 success criterion 5 ("adding a
manifest row no longer requires an oracle-request rebind"); CONTEXT locked decision GATE-02
("per-row oracle pins replace the whole-file hash"); the 2026-11-23 owner approval of
per-row binding. This is not an owner approval of this specific comparator change. The
change is reversible test code (reverting restores whole-file binding), touches no closed
record and no authority pin. The plan 10-19 summary and the claim documents must surface it
to the owner as a recorded exception.

**Rejected alternative.** Re-pinning the ten closed claim blocks to a moved manifest digest.
That edits a closed record and is not taken. If a later phase ever needs it, it is an
orchestrator-only reproduce-then-change-identities-only task, never an executor edit.

## 10. GATE-01 measurements

Written by plan 10-04 (phase-wave 0). Evidence tier: measurements on the local gate
preset at the commit that records them, at a 1-minute host load of 8.4 to 9.6 (above
the quiet-host threshold of 8, so seconds are upper bounds). Nothing here is a hosted
measurement: the hosted warm run is plan 10-05 and the cold run is plan 10-19.

### 10.1 COST properties and fresh-runner start order (Task 1)

`COST` is set beside each registration, outside the sanitizer conditional, as plain
decimal seconds taken from the hosted macOS run of F-111 (run 44795894664):

| Test | COST | File |
|------|------|------|
| `unit.core.state.import` | 210 | `core/CMakeLists.txt` |
| `harness.state-field-use-audit` | 71 | `runner/CMakeLists.txt` |
| `repo.rsp-no-hle` | 48 | `runner/CMakeLists.txt` |
| `harness.phase09-qualification` | 35 | `runner/CMakeLists.txt` |

Read back with `ctest --show-only=json-v1`: the four values appear on the gate preset
(227 names) and on the asan preset (229 names, four `COST` entries, one per test), and
`sh scripts/check-test-inventory.sh --preset gate` and `--preset asan` pass with no name
change.

Fresh-runner simulation: `build/gate/Testing/Temporary/CTestCostData.txt` moved aside
before each run (a hosted runner has none), `ctest --preset gate` (4 jobs), start order
read from the `Start N: name` lines, the original cost file restored afterwards.

| Run | Start position of `unit.core.state.import` (test number 66) | Start positions of the other three | ctest wall (`Total Test time (real)`) | Longest test |
|-----|---|---|---|---|
| without the COST lines (scratch revert of the two files, reconfigured, then re-applied) | 66th of 227 (registration order) | `repo.rsp-no-hle` 125th, `harness.state-field-use-audit` 126th, `harness.phase09-qualification` 174th | 70.1 s | not recorded |
| with COST, run 1 | 2nd | `harness.state-field-use-audit` 3rd, `repo.rsp-no-hle` 4th, `harness.phase09-qualification` 5th | 68.9 s | `unit.core.state.import` 47.7 s |
| with COST, run 2 | 2nd | the same three in the same order | 72.9 s | `unit.core.state.import` 51.4 s |

Reading: with COST the four tests are the second to fifth starts of the run. The test
that starts first is `runner.fixture-descriptor` (0.01 s, no COST), with and without
the COST lines, because `controls.real.source-replaced` declares `DEPENDS` on it
(`runner/CMakeLists.txt:386-387`, the only `DEPENDS` prerequisite in the tree); it frees
its slot at once, so `unit.core.state.import` effectively starts first. Without COST the
same test starts 66th, behind 65 mostly sub-second tests. Locally the wall time is the same inside the
noise (68.9, 72.9 and 70.1 s at load 8.4 to 9.6): the local run has a cheap prefix and a
longest test of 47.7 to 51.4 s, so the ordering saves little here. The F-111 estimate of
up to about 40 s hosted (macOS ctest 251 s against a 210 s longest test that started late)
is not measured by this plan; plan 10-05 reads the hosted ctest wall and the start order
of the four tests against this band and the F-114 spread (up to 52 s macOS).

### 10.2 The repeated rsp-no-hle work in the audio-records step (Task 2)

Measured before `ci.yml` was edited. Evidence tier: local measurements on the gate
preset at the Task 1 commit, and the hosted macOS job log of the F-111 reference run
44795894664 (source `d3211e9`); no source file was edited by this task.

The step "Run authored audio qualification records" runs
`python3 -B scripts/check-audio-cases.py --build build/gate --suite qualification --run`,
which does two rsp-no-hle things, and they have different costs:

| Work in the step | What it is | Local seconds | Hosted macOS seconds (F-111 run, job log) |
|------------------|------------|---------------|-------------------------------------------|
| direct source-body guard | `scripts/check-rsp-no-hle.py --build build/gate` (`scripts/check-audio-cases.py:240-262`), whose command and stdout the emit step requires | 0.19, 0.19, 0.20 (three runs alone) | below 1 (not separately logged; the step is 42 s and its ctest part 41.07 s) |
| CTest name `repo.rsp-no-hle` | `scripts/tests/test_rsp_no_hle.py` (mutation seeds and fetched controls), one of the four names the step's `ctest -R` regex runs | 14.12 (of a 22 s step; the other three names 0.3, 0.7 and 1.2 s) | 41.06 (the step's whole ctest part is 41.07 s of the 42 s step) |
| the same CTest name in the full Gate ctest | the first run of it | 15.0 and 15.3 in the two fresh runs of section 10.1 | 48.34 |

The seconds the plan estimated for the guard (about 25 to 32 s of 42 s macOS) are
not what the F-111 log shows: the repeated cost is the CTest name, not the direct guard
(F-097 and F-105 measured the older, smaller step totals of 25 s macOS and 32 s Linux).
The removable duplicate, if any, is therefore the CTest name `repo.rsp-no-hle` in the
audio-records step: up to about 41 s of the 42 s hosted macOS step. The direct guard
cannot go: the hosted emit step requires its command and a `PASS: rsp-no-hle` stdout.

Dependency evidence (why removing the CTest name needs a closed Phase 8 qualifier edit):

| File and line | Evidence |
|---------------|----------|
| `docs/spec/audio-cases.json:182` | `repo.rsp-no-hle` is listed in the `qualification` suite `ctest` array (`grep -c` counts 1) |
| `scripts/qualify-audio.py:26` | `EXPECTED_INVENTORY_SHA256 = "2f68e71e..."` is the SHA-256 of that whole file (`shasum -a 256 docs/spec/audio-cases.json` reproduces it today); lines 475, 1223, 1399, 1449, 2158 and 2467 refuse any other digest |
| `scripts/tests/test_audio_qualification.py:92, 734, 945` | the closed Phase 8 test of the qualifier reads the same constant |
| `scripts/qualify-audio.py:979-990, 1367, 1510, 2197-2201` | the native plan and the hosted emit require the direct guard's command and its `PASS: rsp-no-hle` stdout (not the CTest name) |

Deleting the array entry changes the inventory bytes, hence its SHA-256, hence
`EXPECTED_INVENTORY_SHA256` in `scripts/qualify-audio.py`, which F-106 keeps closed
together with `docs/evidence/phase08-audio.md`.

**Disposition: declined.** Removal needs a closed Phase 8 qualifier edit (F-106 keeps
it untouched); the 15-minute limit already fits the 673 s cold projection with 227 s
margin (F-111; 279 s margin on the 621 s of F-114). The foregone saving (about 41 s hosted
macOS, about 30 s Linux step) is not counted as an offset in the section 6 ledger. The
decision is reversible: it can be taken at the next Phase 8 requalification, together
with the F-106 literals. `git diff --quiet` over `scripts/qualify-audio.py`,
`docs/spec/audio-cases.json` and `scripts/check-audio-cases.py` exits 0 after the
measurement. The measurement run used a unique artifact prefix and removed only the 18
files it created under `build/gate`.

### 10.3 The single bound-CI edit (Task 3, commit `4ca1054`)

`.github/workflows/ci.yml` and `scripts/check-ci-yml.sh` changed in one commit, the only
change of either file (and of `scripts/gate.sh`, which is unchanged) in Phase 10.

| Item | Before | After |
|------|--------|-------|
| `gate-macos` `timeout-minutes` (`ci.yml` line 74) | 10 | 15 |
| `gate-linux` `timeout-minutes` (`ci.yml` line 20) | 10 | 10 (kept, so the two first-match `awk '/timeout-minutes: 10/ ...'` secrets injections of the selftest still bind to the gate-linux line; `grep -c` counts 1 for `timeout-minutes: 15` and 1 for `timeout-minutes: 10`) |
| CI-TIMEOUT rule | 1 to 10 for every gate job | 1 to 15 when `job == "gate-macos"`, 1 to 10 otherwise; the comment (line 27) and the message carry the same text |
| selftest | 133 cases | 138 cases: the same 133 plus five `mut_timeout` mutations (`gate-macos` 16, 0, 15.5 and 14.5, `gate-linux` 11); the 14.5 case is the only one that tells an integer check from a bound check, because 15.5 is also above the limit |

RED, captured in a scratch root (`build/gate/10-04-task3/red.txt`, local and ignored: the
new cases, the old rule and a copy of `ci.yml` with `gate-macos` at 15): exactly two cases
fail, `clean` first ("the unmodified copy was rejected: ... line 74: timeout-minutes must
be an integer from 1 to 10") and `dir-clean`; the five new mutation cases pass against the
old rule for the wrong reason (it refuses every value above 10), so they are not counted
as RED evidence. GREEN (`build/gate/10-04-task3/green.txt`): 138 cases, 0 failures.

Proof that each new case can fail, six scanner mutants in scratch copies of the final
scanner (each run with `--selftest`, exit 1, listed failures only):

| Scanner mutant | Failing case |
|----------------|--------------|
| bound 10 for every job | `clean`, `dir-clean` (gate-macos 15 refused) |
| bound 15 for every job | `gate-linux timeout-minutes 11` |
| gate-macos bound 16 | `gate-macos timeout-minutes 16` |
| lower bound 0 instead of 1 | `gate-macos timeout-minutes 0` |
| decimals accepted (`[0-9.]+`) | `gate-macos timeout-minutes 14.5` |
| the wide bound given to `gate-linux` | `clean`, `dir-clean` |

Other checks at the commit: `sh scripts/check-ci-yml.sh` clean; `ctest -R
'^repo\.ci-yml(-selftest)?$'` 2 of 2; `actionlint .github/workflows/ci.yml` exit 0;
`act -l` lists `gate-linux` and `gate-macos`; `harness.phase09-qualification`,
`harness.audio-qualification` and `harness.rdp-oracle-authority` pass; `git diff --quiet`
over `scripts/gate.sh` exits 0. Every other scanner rule, and every other existing
selftest case, is unchanged and still passes.

### 10.4 Records of this plan (Task 4)

`docs/ci-reviews/LOG.md` gets F-115 to F-119 (append-only, run column 44795894664, the
reference run of F-111): F-115 taken (COST and timeout 15), F-116 declined (guard), F-117
declined (F-106 outer limits), F-118 declined (Binutils build parallelism), F-119 taken
(the run-to-run noise band, which also answers the open question of F-114). Correction of
the section 3 row "Optional `make -j2`": `scripts/build-boot-fixtures.sh` already runs
`make -j2 all-binutils all-gas all-ld` (line 259, since commit `e6f0655`), so F-118 records
the real remaining lever (a higher job count) and declines it for the cache-key reason of
section 3. `.planning/REQUIREMENTS.md` carries the one-line GATE-01 annotation. GATE-01
stays unchecked: its success criterion is the measured cold-cache hosted run of plan 10-19.

### 10.5 COST weights `CTestCostData.txt` (found by the Task 4 preflight, commit `df5442f`)

CTest keeps the explicit `COST` property as the running mean of its cost history: the
stored value after a run is `(runs * COST + this run) / (runs + 1)` (checked on
`harness.phase09-qualification`, 55 prior runs, COST 35, a run of 8.07 s stored 34.52; and
on `repo.rsp-no-hle`, 112 prior runs, COST 48, a run of 14.1 s stored 47.70). For the four
COST tests, `build/gate/Testing/Temporary/CTestCostData.txt` therefore stops holding native
seconds after the first local run. Two consequences:

- The timeout audit of `sh scripts/preflight-phase10.sh` read that file as native seconds
  and failed `harness.phase09-qualification` (TIMEOUT 120 s against 3.5 times 34.52 s). A
  test with an explicit COST is now audited against 2 times its COST (hosted seconds, the
  "below 2x" line of F-108): `unit.core.state.import` 600/210 = 2.86,
  `harness.state-field-use-audit` 240/71 = 3.38, `repo.rsp-no-hle` 240/48 = 5.0,
  `harness.phase09-qualification` 120/35 = 3.43, all above 2. Every other test keeps 3.5
  times its recorded native seconds. The full preflight passes at the fix commit (63 s,
  replay of 11 steps).
- The section 6 ledger rule "the sum of the new names' seconds from `CTestCostData.txt`"
  stays valid for new Phase 10 names (none carries COST); for the four COST tests read
  native seconds from a `ctest` run log, not from that file. The native seconds of this
  plan's fresh runs are in section 10.1 (state.import 47.7 and 51.4 s, field-use audit
  17.1 and 17.6 s, rsp-no-hle 15.0 and 15.3 s, phase09-qualification 8.6 and 8.8 s).
