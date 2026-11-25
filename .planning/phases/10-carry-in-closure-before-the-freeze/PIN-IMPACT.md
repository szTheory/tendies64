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

### 2.3 State 26 import (plan 10-12)

Written by plan 10-12 (phase-wave 2). Evidence tier: code observations and unit-test
results on the gate preset at the commits named below; nothing here is a compatibility or
hardware claim. The importer half of the state bump: the domain text follows the machine
(`t64.state.si.25` for the v25 tuple, `t64.state.si.26` for the v26 tuple), the five depth
scalars are read inside the RDP block before the triangle-render proof, the hidden table
ends section 8, and the commit writes both from the blob for state 26 only.

| Item | Value |
|------|-------|
| tracer RED (`5e4aa1e`) | idle v26 blob imports (`idle-v26-precheck`, 10,545,971 bytes) and the mid Z-triangle blob is refused with status 32 (`STATE_RANGE`), reason 26 (`FIELD_RANGE`), section 8, rule 11 (`LEN_LE`): the triangle-render proof of `state_check_triangle_render` saw `depth_valid` false; no header, version or layout refusal |
| GREEN (`1083257`) | the same case passes: 119 checks, the Z triangle in flight imports at tick 17 with 2 hidden entries, both machines hold 10 after 2,000 ticks and agree on digest, color and depth bytes and the 2,097,152 hidden bytes |
| `unit.core.state.import-v26-roundtrip` native seconds | 2.03, 2.00, 2.00, 1.88 and 1.87 s (five `ctest` runs at 1-minute host load 8 to 12.7, above the quiet-host threshold of 8, so an upper bound); 19,760 checks |
| `unit.core.state.import-v26-hostile` native seconds | 6.82, 6.80 and 6.80 s (7.92 s next to three other tests at `-j4`; the 16 rows of `docs/spec/state-mutants-v26.txt` plus two v26 fuzz campaigns of 24 iterations on the idle blob and the mid Z-triangle blob); 16,689 checks |
| mid scan-out | a Z-tested 4x3 frame drawn into the VI origin, one retained video packet and two PCM DMAs in flight at export; on `-s32k` (one SRAM word written before the export) and `-e4k` (one EEPROM block written, busy window open): 3 packet-bearing field boundaries compared with digests, 1,096 field events run, 138 video packets and 2 audio packets drained equal on both machines, save bytes and busy flag equal |
| ASan seconds (the only sanitizer run of the plan, `cmake --build --preset asan`, 1-minute host load 8 to 13, an upper bound) | `import-v26-roundtrip` 39.48 and 30.82 s; `import-v26-hostile` 119.44 and 111.90 s (about 20 and 16 times native) |
| final TIMEOUTs (F-108) | gate preset: 300 s for both (150 and about 40 times native, 7.6 and 2.5 times the larger ASan second); sanitizer preset (`core/CMakeLists.txt`, the asan block): 600 s and 900 s (15 and 7.5 times the larger ASan second). `unit.core.state.import` keeps 600 s and 2400 s; the largest registered TIMEOUT, which `scripts/qualify-rdp-vi.py` must stay above, is unchanged |
| importer mutants (`python3 -B scripts/p10-mutants.py --run`, committed tree `4ac1621`) | `v26-import-skip-hidden` and `v26-import-depth-after-proof` KILLED by `import-v26-roundtrip`; `v26-import-addr-unchecked` and `v26-import-z-products-unchecked` KILLED by `import-v26-hostile`; the other 21 mutants KILLED, the equivalent control SURVIVED, the non-compiling control INVALID |
| digest sweep against the Wave 2 base (`--head WORKTREE`) | 1599 shared lines, 0 differences, 0 head-only lines; with the four `--head-extra-id` ids 296 head-only lines (the importer does not enter any digest) |
| `v25-hidden-zero` | on `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k` every one of the 2,097,152 hidden bytes of the peer was set before a state 25 import and all 2,097,152 are zero after it, depth address 0, depth flags clear |

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
record below is the only line of this form written by plan 10-01. The gap closure of
plans 10-20 and 10-21 moved the bound sources (a test source and the qualifier) after the first
qualification, so plan 10-21 re-recorded the Qualification base after the independent review of
that closure and relabelled the plan 10-18 line as historical, so that exactly one record line
remains (the base extraction idiom and `read_bases` of `scripts/qualify-phase10.py` match
only lines that start with the label and a colon).

- Wave 0 base: 4ac633e880e0c26005cb03092e8d8485e721034c
- Wave 1 base: f6ab8c26d008b8301b729b5aae2715e1b1079318
- Wave 2 base: 6dd785986f6aa78c6da5573a558b15b386a4edee
- Wave 3 base: 1cded8de34b0c2f5c5bea903095ffc7f1e55d6fc
- Historical qualification base (plan 10-18, superseded by plan 10-21): ad39b6be2f803af45af15d4ed511b72a89460339
- Qualification base: 76451024e148073804e162841c3d0a3543bd2567

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
| 2 (plans 10-09 to 10-12, head `f57b019`, run 45530265365) | `unit.core.profile.v26-identity`, `unit.core.profile.v26-isviewer`, `unit.core.profile.v26-predicate-matrix`, `unit.core.digest.v26-completeness`, `unit.core.digest.v26-strip-differential`, `unit.core.rdp.depth-composite-accepted`, `unit.core.rdp.depth-composite-wrong-depth`, `unit.core.rdp.depth-composite-sidecar`, `unit.core.state.export-v26`, `unit.core.state.export-v26-bounds`, `unit.core.state.import-v26-roundtrip`, `unit.core.state.import-v26-hostile` (236 to 248 names) | 15.13 s (one fresh run in the review scratch clone from `CTestCostData.txt` at 1-minute load 12 to 15, so an upper bound: import-v26-hostile 8.63, export-v26 2.20, import-v26-roundtrip 2.20, export-v26-bounds 1.57, digest.v26-completeness 0.40, strip-differential 0.05, depth-composite-sidecar 0.03 and the other five 0.006 to 0.016 s; none carries `COST`; the shared tree's running means are lower, 3.99 s, because they include the RED and early runs of the same names, and are not used). The `check-test-inventory.sh` selftest cost of about 0.2 s per name (section 1) was 21 s in the scratch gate against 20 s at the Wave 1 review, which host load hides | ctest wall 260 s against 187 s (F-121) and 184 s (F-120): +73 s and +76 s, above the 52 s spread of F-119 by 21 to 24 s. Not read as growth of the new names alone: the same sample is slow in every step no Wave 2 edit touches (build 34 against 17 s, boot-evidence-selftest 39 against 19 s, core-lint 23 against 15 s) and the unchanged critical-path test `unit.core.state.import` ran 216.18 s against 172.55 s. The 12 names run 53.91 s in total on macOS and 29.47 s on Linux (Linux ctest 125 s against 115 s, +10 s, inside the spread); that sum is the upper bound of their share. F-122 | G4 exception (ROADMAP Phase 10): the 12 names are the required pre-freeze evidence of VID-07 and CORE-06 that the Phase 11 runner and API freeze build on (the v26 digest completeness flip and strip differential, the registered family and its predicate matrix, depth accepted with an independent wrong-depth control, the state 26 export with container version 2 and the state 26 import with depth-inclusive round trips, 16 hostile rows and fuzz windows). No offset is claimed: the rsp-no-hle guard removal stays declined (F-116) | warm: gate-macos run 496 s (Gate 424 s), the slowest of eight warm samples, margin 404 s. Cold projection: warm plus the Binutils build delta of 186 s is 682 s, margin 218 s (673 s in F-111); plan 10-17 re-reads the total before the measured cold run of plan 10-19 |
| Total before the cold run (plan 10-17; rehearsal head `da6af0d`, record `20261125T041245798491Z-da6af0d6`, 1-minute host load 12.0 to 13.6, an upper bound) | all 30 `PHASE10_TESTS` names (224 names at the Wave 0 baseline, 254 now); of them 6 are the Wave 3 and 4 names (`runner.composite-two-run`, `harness.composite-oracle`, `controls.real.composite-wrong-rom`, `runner.state-format-v26`, `runner.composite-state-v2`, `harness.phase10-qualification`) | 30.61 s: one fresh `ctest -L phase10` run, the JUnit `time` of the 30 names at 4 jobs (`import-v26-hostile` 9.88, the qualifier's own hermetic test 8.99, `import-v26-roundtrip` 2.58, `export-v26` 2.54, `state-format-v26` 1.81, `export-v26-bounds` 1.78, the other 24 names 0.003 to 0.7 s each); the Wave 3 and 4 names are 12.20 s of it. The mean of the `CTestCostData.txt` history (8 to 22 runs per name at mixed load) is 18.83 s. Under the sanitizer the selection took 163.52 s wall and 387.25 s summed (not a hosted lane) | Not a hosted measurement. Native: `ctest --preset gate` wall 103.34 s at 254 names against 99.7 s at 224 names in the baseline row (+3.6 s, +3.7 percent, both at load 12 to 16), and the whole gate stage 155.88 s (the same gate took 143 s with ctest 84.94 s at load 7.4 to 9 on commit `48c928e`). Projected hosted (native sum times the F-111 hosted over native ratio: 2.8 for the Python harness test of F-111 and 4.34 for `unit.core.state.import`, 210.19 s over 48.4 s): the 30 names add 85.7 to 132.9 hosted seconds of summed test time, which is 23 to 36 s of ctest wall packed on 4 jobs (wall about 1.08 times a quarter of the sum, F-109) and at most the whole 133 s if every second landed on the critical path; the six Wave 3 and 4 names add 34.2 to 53.0 summed seconds, 9 to 14 s packed. The one hosted measurement of Waves 0 to 2 (24 names, 18.41 s native) was +9 s of ctest wall against F-111 (260 against 251 s), inside the F-119 spread of 52 s | G4 exception (ROADMAP Phase 10), carried forward unchanged per wave and with no offset claimed anywhere: Wave 0, the K1 refusal pins and the mutant anchor test (required pre-freeze evidence before any depth predicate edit); Wave 1, the nine VG and NTSC-clock tests of VER-06 that Phase 7 left open; Wave 2, the 12 v26 identity, digest, depth and state 26 export and import tests of VID-07 and CORE-06 that the Phase 11 runner and API freeze build on; Wave 3 and 4, the composite two-run, wrong-ROM, oracle and state-container-format tests of RUN-07 and CORE-06 and the qualifier's hermetic test (about 5 s natively by design, F-113). The rsp-no-hle guard removal stays declined (F-116), so no time is removed elsewhere. Reversal rule: restated below the table and in section 6.1 | Projected cold gate-macos 696 s (packed) to 735 s (all six Wave 3 and 4 names serialized): the Wave 2 cold projection 682 s plus their 9 to 53 s; margin 204 s to 165 s. Cross-check from the F-111 baseline cold projection 673 s plus all 30 names: 709 s (packed) to 806 s (serialized), margin 191 s to 94 s; the 94 s end is below 120 s and is recorded as a warning for plans 10-18 and 10-19 (section 6.1); the measured cold run is plan 10-19 |
| Waves 3 and 4 (plans 10-14 to 10-18, head `f081d39`, run 45548987145) | `runner.composite-two-run`, `harness.composite-oracle`, `controls.real.composite-wrong-rom`, `runner.state-format-v26`, `runner.composite-state-v2`, `harness.phase10-qualification` (248 to 254 names) | 9.50 s (one fresh run in the review scratch gate from `CTestCostData.txt` at 1-minute load 6 to 10: the qualifier's hermetic test 6.954, `state-format-v26` 1.441, `composite-two-run` 0.543, `composite-wrong-rom` 0.271, `composite-state-v2` 0.249, `composite-oracle` 0.046 s; none carries `COST`). All 30 Phase 10 names ran 23.23 s in the same gate (30.61 s at load 12 to 13.6 in the 10-17 row, 18.83 s the mean of the history) | ctest wall 196 s against 260 s (F-122), 187 s (F-121) and 184 s (F-120): 64 s lower and 9 s and 12 s higher, inside or below the 52 s spread of F-119 with 27 more names than F-120, so no growth is read. The six names ran 19.32 s summed on macOS (13.39 s of it the qualifier's hermetic test) and 9.67 s on Linux; the 30 names 63.07 s and 41.25 s, below the 85.7 s lower end of the 10-17 hosted projection of the 30 names and the six names below its 34.2 to 53.0 s; Linux ctest 126 s against 125 s (F-122). F-123 | G4 exception (ROADMAP Phase 10), carried forward unchanged: the six names are the required evidence of RUN-07 (two runs of one composite guest, the wrong-ROM control, the independent oracle equality), of the CORE-06 container format string pairing and of the qualifier; no offset is claimed anywhere and the rsp-no-hle guard removal stays declined (F-116) | warm: gate-macos run 385 s (Gate 337 s), margin 515 s; a warm run above 700 s is a warning and this one is 315 s below it. Cold projection: warm plus the Binutils build delta of 186 s is 571 s, margin 329 s, below the 696 to 735 s primary and 709 to 806 s cross-check projections of the 10-17 row; the 94 s worst-case margin warning is not approached by this sample, which is one sample (the Wave 2 sample was 496 s). The measured cold run is plan 10-19 |
| Cold run (plan 10-19, measured; head `f081d39`, run 45550277510, workflow_dispatch after the orchestrator deleted the macOS Binutils entry) | none new: the same 254 names of the Wave 3 and 4 row | not a new-name measurement: the cold run executed the same 254 names; ctest 265 s cold against 196 s warm (F-123) | ctest wall 265 s on 254 names against 196 s warm on the same names (+69 s, 17 s above the 52 s spread of F-119, so runner speed and not a cold-cache cost; the Binutils build is outside ctest). Against F-111 (251 s on 224 names) +14 s. F-124 | G4 exception (ROADMAP Phase 10), carried forward unchanged: no offset is claimed anywhere and the rsp-no-hle guard removal stays declined (F-116). The reversal rule is NOT triggered: 731 s is not above 900 s and the job was not cancelled | **measured cold gate-macos 731 s (Restore 1 s miss, Gate 653 s of which fixture-qualification 219 s, ctest 265 s; audio records 40 s), margin 169 s** (18.8 percent of the 900 s limit). Against the projections: 58 s above the F-111 cold projection of 673 s, 160 s above the F-123 cold projection of 571 s, at the top of the 10-17 primary range of 696 to 735 s and inside the cross-check range of 709 to 806 s; the 94 s worst-case warning margin was not approached (the measured margin is 169 s). The old 600 s limit would have been exceeded by 131 s. One sample |
| Gap closure (plans 10-20 and 10-21; qualified commit `1fc0b1d`, pushed head `85ba477`, run 45650890027) | none (254 names, 30 `PHASE10_TESTS` names): the VI rows live inside the existing case `unit.core.digest.v26-completeness` | about 0.7 s: that case ran 0.40 s before the VI rows (Wave 2 row above) and 1.13 s in the JUnit of the plan 10-21 record (1.08 to 1.10 s in the three runs of section 16.1; 8.67 s under the sanitizers, 7.92 s in section 16.1). The 30 names sum 23.82 s in the record against 23.35 s in the plan 10-19 record (+0.47 s at different loads); native `ctest --preset gate` wall 76.18 s against 77.30 s; the mutant stage has one row more (105.00 s against 100.72 s). Section 17 | ctest wall 194 s against 196 s in the Waves 3 and 4 row on the same 254 names: -2 s (Linux 128 s against 126 s, +2 s), inside the 52 s spread of F-119, so no gate growth is read. The case itself ran 3.13 s on macOS and 3.85 s on Linux against 1.67 s and 1.02 s before the VI rows (F-122): +1.46 s and +2.83 s; the 30 names ran 64.16 s and 44.07 s against 63.07 s and 41.25 s (+1.09 s and +2.82 s). F-126 | G4 exception (ROADMAP Phase 10), carried forward unchanged: the VI rows are the required evidence of CORE-06 and of ROADMAP Phase 10 success criterion 2 (a committed flip-every-member test sees each RDP and VI member; the VI half rested on an uncommitted probe, finding W2-03, until plan 10-20). No offset is claimed anywhere (no CTest name was added, so none was available to trade) and the `rsp-no-hle` guard removal stays declined (F-116) | warm: gate-macos run 368 s (Gate 315 s), margin 532 s; a warm run above 700 s is a warning and this one is 332 s below it. Cold: the measured cold run of F-124 (731 s) was taken on the earlier audited tree and was **not repeated**, because the gap closure added test time only; adding the case's hosted growth gives a cold figure of about 733 s, margin 167 s, and the warning line of about 850 s (section 15.2) is not approached. The warm plus 186 s Binutils projection (554 s) understated the measured cold run by 177 s, as F-123 did (F-124), so the measured number is the one that counts |

Reversal rule of the G4 exception (ROADMAP Phase 10): if the measured cold-cache
gate-macos run of plan 10-19 exceeds 900 s or is cancelled at the limit, a new numbered
plan moves non-essential Phase 10 cases (sampled sanitizer runs, duplicate-path round
trips) to a non-gating lane and the cold run is repeated. The exception covers Phase 10
only; Phases 11 and 12 follow G4 as written. The redundant `rsp-no-hle` guard is a
stop-loss that is not counted as an offset unless its removal is measured.

### 6.1 Total before the cold run (plan 10-17)

Written by plan 10-17 (phase-wave 4) from the rehearsal of section 14 and the earlier
ledger rows. Evidence tier: native measurements at host load 12.0 to 13.6 (above the
quiet-host threshold of 8, so every native second is an upper bound) and projections
from recorded hosted measurements; nothing here is a hosted measurement of the Phase 10
tree, and the measured cold run is plan 10-19.

| Input | Value |
|-------|-------|
| native `ctest --preset gate` wall, 254 names | 103.34 s (rehearsal) against 99.7 s for 224 names in the Wave 0 baseline: +3.6 s, +3.7 percent, at comparable load; 84.94 s at load 7.4 to 9 (commit `48c928e`) |
| native gate stage (`scripts/gate.sh`, whole script) | 155.88 s (steps: sh-n 0, lint-selftest 4, fixture-qualification 1, configure 0, build 0, boot-evidence-selftest 11, test-inventory-selftest 23, test-inventory 1, ctest 103, core-lint 13); 143 s at load 7.4 to 9 |
| summed native seconds of all 30 `PHASE10_TESTS` names | 30.61 s (JUnit of one fresh run) and 18.83 s (mean of the `CTestCostData.txt` history); the sum of the per-wave ledger rows is 0.36 + 0.14 + 15.13 = 15.63 s for Waves 0 to 2 (taken at different loads) plus the Wave 3 and 4 names |
| hosted over native ratio of F-111 | 2.8 (the Python harness test) as the central value and 4.34 (`unit.core.state.import`, 210.19 s over 48.4 s) as the upper value |
| projected hosted summed seconds of the 30 names | 85.7 (central) to 132.9 (upper) |
| projected hosted ctest wall delta of the 30 names | 23 to 36 s packed on 4 jobs; 86 to 133 s only if the whole sum landed on the critical path (it does not: `unit.core.state.import` starts first by `COST` and is the longest test, F-115, F-120) |
| projected hosted ctest wall delta of the Wave 3 and 4 names alone | 9 to 14 s packed, 34 to 53 s serialized (12.20 s native) |
| primary cold projection | Wave 2 cold projection 682 s (warm 496 s, the slowest of eight warm samples, plus the Binutils build delta 186 s, F-122) plus the Wave 3 and 4 delta: 696 s (packed) to 735 s (serialized), margin 204 s to 165 s of the 900 s limit |
| cross-check projection | F-111 cold projection 673 s plus all 30 names: 709 s (packed) to 806 s (serialized), margin 191 s to 94 s |

Warning for plans 10-18 and 10-19: the serialized end of the cross-check leaves 94 s, less
than the 120 s the plan sets as the warning margin. The primary projection leaves at least
165 s and the one hosted measurement of 24 of the 30 names added only 9 s of ctest wall, so
the serialized bound is a ceiling, not a forecast. Plan 10-18 compares its first warm
hosted run of the Wave 3 and 4 head with these numbers (a warm gate-macos run above 700 s
is already a warning in that plan); plan 10-19 measures the cold run.

Offsets and exception reasons carried forward from the ledger rows: no row claims an
offset. Waves 0 to 2 and 3 to 4 each rest on the G4 exception reason of their row. The
redundant `rsp-no-hle` guard stays a declined stop-loss (F-116) and counts for nothing.

Reversal rule of the G4 exception (restated, ROADMAP Phase 10): if the measured cold-cache
gate-macos run of plan 10-19 exceeds 900 s or is cancelled at the limit, a new numbered
plan moves non-essential Phase 10 cases (sampled sanitizer runs, duplicate-path round
trips) to a non-gating lane and the cold run is repeated. The exception covers Phase 10
only; Phases 11 and 12 follow G4 as written.

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

## 11. Manifest-moving commit (plan 10-14, RUN-07 and the GATE-02 real-row proof)

Written by plan 10-14 (phase-wave 3). Evidence tier: code observations and supervised-run
results on the gate preset at the commits named below; the guest is authored here (D2, D5),
embeds only the pinned openIPL3 asset the earlier builders use, and nothing here is a
compatibility or hardware claim. The 1-minute host load was 12 to 15 (above the quiet-host
threshold of 8), so seconds are upper bounds.

**Commits.** The guest sources, builder, oracle and input script are commit `419275d`
(no manifest row and no runner table row). The manifest rows and runner rows are the one
commit of this phase that changes `fixtures/manifest.json` (subject `feat(10-14): composite
manifest and runner rows`; its hash is in the after-record of section 11.4). The two are
separate so that `source.revision` of the new rows names a commit that already holds
`fixtures/composite/composite.S` (the Phase 9 rows do the same); the guest source is frozen
before the rows are computed from it.

### 11.1 Rows

| Row id | ROM SHA-256 | ROM size | Payload SHA-256 (size) | Completion PC | Runner cap |
|--------|-------------|----------|------------------------|---------------|------------|
| `phase10-composite-s32k` | `7a0426b49eb8f4288c70414e9b34cc6e4fdfb8052335976f8fc43d886e954945` | 5,496 | `a908ce5882787f107335a7f45a671d02df6ae7b77ab665239e33dadbfcc3146b` (1,400) | `0x800007a4` | long cap |
| `phase10-composite-wrong-s32k` | `dbb52ad154e77c41d148eaef9ce704c425aad68ca49f4726708c3e0b13da7514` | 5,496 | `c9a40d0c07dde87e4455a6a68561291cc0a97898dc57edbb8045f88ae0e6b86e` (1,400) | `0x800007a8` | long cap |

Both rows: profile `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` (profile hash
`9dfb90d8055b822a1767be1c5594d1b2d904f042ef6fe82f2e7a3243e5756578`, section 2.1),
`authored-here`, `cleared`, `build-only`, source-content SHA-256
`311a8dbc86f6624948b14320d25bc24ecabf3e3735f5a27f829b94589a5e4c6f` (the builder's file list
leaves out the oracle and the input script, so plan 10-15 can add oracle modes without moving
the rows). Runner pin of both rows: 188 bytes, SHA-256
`da07d916f0314db05e8d609326242d7f944098e96ce19a96339ef1cd6a7aad39` (the transcript of the
accepted ROM with `fixtures/composite/composite.input.json`; the wrong ROM is judged by the
same pin and fails it). The long cap (4,000,000 guest ticks) is used instead of the default
600,000: IPL3 hands over at about tick 539,200 and the guest completes at tick 599,357, 643
ticks below the default cap, because each SI DMA models thousands of ticks.

### 11.2 Pins that did not move

| Item | Value |
|------|-------|
| `PINNED_REQUEST_SHA256` (`scripts/check-rdp-oracle-authority.py`, line 23) | `e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab`, unchanged; the file, `scripts/tests/test_phase09_qualification.py`, `runner/t64_real.c` and `runner/t64_supervisor.c` are byte-identical to the Wave 3 base |
| manifest rows of the Wave 3 base | 39, equal in canonical JSON and in position; two rows appended (41) |
| closed Phase 9 records | `docs/claims/phase09.md` and `docs/evidence/phase09-qualification.md` are byte-identical to both the Wave 0 and the Wave 3 base |
| `harness.rdp-oracle-authority`, `harness.phase09-qualification` | pass on the appended rows with no oracle rebind (per-row binding of plan 10-03, GATE-02 on a real row) |
| one consumer the plan did not name | `runner.fixture-transcript-pin` compares the pass-rule sentence of `docs/spec/result-protocol.md` with the runner table (drift control of plan 09-10). The two new pinned rows made it say "17 rows" and add the 188-byte length; the sentence was updated (the only text change outside the plan list) and the control passes. `check-rsp-evidence.py` hashes that document too, but already exits 1 (section 9, row e) |

### 11.3 Supervised runs (accepted ROM with `composite.input.json`)

| Item | Value |
|------|-------|
| raw-capture run (`--raw-capture-dir`) | outcome `pass`, `positive-match`, guest tick 599,357, state digest `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c` |
| save-out run (`--artifact-dir`, `--artifact-max-bytes 32768`, `--save-export-leaf composite.sra`, `--save-kind sram32k`, `--save-format raw-v1`) | outcome `pass`, guest tick 599,357, the same state digest; one exported file of 32,768 bytes, SHA-256 `f0b7fe880aeed453233a0b07ab62301420967a959bd1d43e9dd49e23d3e6926d` |
| raw capture sizes (limit 69,632 per file) | 3 video frames of 48 bytes (4x3 RGBA8), 2 audio packets of 16 bytes (S16BE), `manifest.json` 2,273 bytes, data 176 bytes, total 2,449 bytes: the largest file is 3.3 percent of the limit |
| oracle agreement (`fixtures/composite/composite_oracle.py --input fixtures/composite/composite.input.json`) | video bytes, both audio packets, the save SHA-256 and the 188-byte transcript (SHA-256 above) are equal to the run outputs; the wrong ROM prints the SR line the oracle gives for `--variant wrong` (last digit 6, not 7) and fails the pin with `wrong-transcript`; the empty input script completes and prints the oracle's empty transcript (SHA-256 `a99af82e3f0ec8df350822f9170034f1797adc902fc5392a2b3808695c4ed04e`, save image `a7762195a09b78d930db2bd627429c7c6c2ea4389dc658c23df8d8b3124a3a34`) |
| controller poll ticks (measured once from the neutral run with 116 probe stamps, 400 ticks apart, strictly-after rule) | poll 1 in (555,400, 555,800], poll 2 in (573,400, 573,800], poll 3 in (591,400, 591,800]; the input script stamps at 564,000 and 582,000, at least 8,200 ticks from every poll; the oracle refuses a stamp within 2,000 ticks of a nominal poll tick (exit 2) |
| raw run before audio existed | with no AI the raw capture ends `harness-error` / `capture-incomplete-streams` (a bundle needs both streams), so the tracer carried the audio phase |

### 11.4 Consumers of `fixtures/manifest.json` that no gate test exercises

Exit statuses of the two scripts of the section 9 audit (rows e and f), before the rows
commit. "Before" is the main tree with the Wave 3 base manifest (equal to the committed
manifest at the plan start): `check-rsp-evidence.py --check` 1 and `check-base-evidence.py
--check-current` 0, the statuses the 10-03 audit recorded. A throwaway worktree of the Wave 3
base gives 1 and 2: the second is environmental (the private BASE artifacts of
`build/private-base-diagnostic` are not part of a worktree; a clean clone also exits 2, section 9
row f), so it is read beside the worktree after-value, not against the main tree.

| Script | Before (main tree) | Before (worktree of the Wave 3 base) | After (main tree, rows commit) | After (worktree, rows commit) |
|--------|--------------------|--------------------------------------|--------------------------------|-------------------------------|
| `check-rsp-evidence.py --check` | 1 | 1 | 1 | 1 |
| `check-base-evidence.py --check-current` | 0 | 2 | 0 | 2 |

After-record. The rows commit is `f027b05` (full hash
`f027b055a1cd14e7cb84e0fd9f9a31b243f175cb`). Every status is unchanged by the commit, so no
consumer outside the gate moved. Right after the commit: the four manifest-adjacent CTests
(`harness.rdp-oracle-authority` and `harness.phase09-qualification` exercise the manifest
binding; `harness.audio-qualification` and `harness.rsp-evidence-ownership` are regression runs
only, section 9) pass 4 of 4; `sh scripts/preflight-phase10.sh` on the clean committed tree
passes in 76 s (227 units compiled with the act image cc 13.3.0, 11 post-Gate steps replayed
with the real `qualify-audio.py` and `qualify-phase09.py` emit steps over the new manifest);
`git log` since the Wave 3 base lists exactly this one commit for `fixtures/manifest.json`.

## 12. Provisional-name sweep

Written by plan 10-16 (phase-wave 4). Evidence tier: code observation of the seven live spec
docs at the commits named below; docs only, nothing here moves a build id, a pin or a
compatibility or hardware claim. The freeze is a Phase 11 candidate and the final ABI bind
is at the end of Phase 12 (ROADMAP Phase 11 and 12); the sweep adds no API or ABI freeze
statement. A relabel changes the name of the milestone a statement waits for, never the
statement.

Gate (whitespace-normalized, so a phrase that wraps over two lines counts):

    python3 -c "import re,pathlib,sys; fs='raw-capture result-protocol machine-contract replay state-container si-pif-controller save-media'.split(); rx=re.compile(r'(provisional|revisable)\s+until\s+(the\s+)?Phase\s+10|Phase\s+10\s+(interface\s+review|item|follow-up)'); n=sum(len(rx.findall(pathlib.Path('docs/spec/'+f+'.md').read_text())) for f in fs); print('stale', n); sys.exit(1 if n else 0)"

Baseline re-measured at the plan start (commit `9c60de0`): stale 15 (raw-capture 1,
result-protocol 2, machine-contract 5, replay 2, state-container 3, si-pif-controller 0,
save-media 2), equal to the planning baseline. The replacement phrase used for every
relabel is the one literal "provisional until the Phase 11 freeze candidate (final ABI bind
at the end of Phase 12)" (and "revisable until" in the one site that said revisable), so
Phase 11 and 12 can find every site with one search.

| # | Site at the plan start (file:line) | Old phrase | New phrase or settlement |
|---|-----------------------------------|------------|--------------------------|
| 1 | `docs/spec/raw-capture.md:132` (artifact pair, Phase 9) | provisional until Phase 10 | provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) |
| 2 | `docs/spec/result-protocol.md:546` (artifact publisher detail names, plan 09-05) | provisional until Phase 10 | provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) |
| 3 | `docs/spec/result-protocol.md:554` (state export detail names, plan 09-08) | provisional until Phase 10 | provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12); Phase 10 added no detail name (plan 10-15 pairing refusals are the existing prelaunch refusal), so nothing here was decided |
| 4 | `docs/spec/machine-contract.md:507` (Composite SI profile family: ids, descriptor fields, constants, section numbers) | revisable until the Phase 10 interface review | revisable until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) |
| 5 | `docs/spec/machine-contract.md:548-549` (save device statuses 20 to 22 and `t64_machine_save_info`) | provisional until the Phase 10 interface review | provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) |
| 6 | `docs/spec/machine-contract.md:632` (F0 identity record) | provisional until Phase 10 | provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) |
| 7 | `docs/spec/machine-contract.md:724` (CORE-02 ordering evidence) | provisional until Phase 10 | provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) |
| 8 | `docs/spec/machine-contract.md:852` (F1 state content record) | provisional until Phase 10 | provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) |
| 9 | `docs/spec/replay.md:11` (format keys, flag names, module API, fixture row fields) | provisional until Phase 10 | provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) |
| 10 | `docs/spec/replay.md:198` (naming the recorder failures in the closed result vocabulary) | a Phase 10 item | a Phase 11 item, same sentence and content (the 09-REVIEW-FIX L3 IN-01 owner item) |
| 11 | `docs/spec/state-container.md:24-25` (stability list: name, layout, format string, flag names, detail names) | provisional until Phase 10 | settled in Phase 10 (plans 10-11 and 10-15): the container format strings `t64.state-container.v1` (state 25) and `t64.state-container.v2` (state 26) and their pairing with the state versions, as state-format constants outside the ABI promise; name, layout, flag names and detail names are provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) |
| 12 | `docs/spec/state-container.md:412` (`t64_machine_state_import`: name, flag, report record, statuses 26 to 33, reason and rule names) | provisional until Phase 10 | provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) |
| 13 | `docs/spec/state-container.md:786` (runner descriptor option names `--input-fd`, `--save-import-fd`, `--replay-fd`, `--state-import-fd`) | provisional until the Phase 10 interface review | provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) |
| 14 | `docs/spec/save-media.md:29-30` (statuses, kind and format constants, `t64_save_info`, import and export calls, flag and detail names) | provisional until the Phase 10 interface review | provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12); the clause "only the kind numbering above and the `save_policy` label stay fixed for the rest of Phase 9" is unchanged |
| 15 | `docs/spec/save-media.md:336` (guest writes do not increment `write_generation`) | a Phase 10 follow-up | a Phase 11 item, same content |

Settle rows (a statement that waited for the "state content freeze at the end of plan
09-06"; the regex of the gate does not count them):

| # | Site at the plan start | Old sentence | New sentence |
|---|------------------------|--------------|--------------|
| S1 | `docs/spec/si-pif-controller.md:250-252` (SI and PIF section content, the plan's settle row) | The section content is provisional until the state content freeze at the end of plan 09-06; nothing here is an API or ABI freeze. | The section content is the state content frozen by state versions 25 and 26 (the F1 record of machine-contract.md; state 26 keeps this section and its framing); nothing here is an API or ABI freeze. |
| S2 | `docs/spec/save-media.md:208-209` (save-media section content; the identical sentence for section 11, not named by the plan, found by the same search over the 7 docs and settled with the same rule) | The section content is provisional until the state content freeze at the end of plan 09-06. | The same sentence as S1 (state versions 25 and 26, F1 record, state 26 keeps this section and its framing, no API or ABI freeze). |

Result. Every one of the 15 stale sites is relabeled or settled and S1 and S2 are settled:
the gate prints `stale 0` (raw-capture 0, result-protocol 0, machine-contract 0, replay 0,
state-container 0, si-pif-controller 0, save-media 0); `grep "Phase 10"` over the seven docs
finds only the one settlement sentence of row 11. `sh scripts/check-docs.sh` prints
`check-docs: ok` and `sh scripts/check-claims.sh` prints `errors=0`.
`git diff --quiet -- docs/claims docs/evidence` exits 0, so no closed-phase record moved
(`docs/claims/phase09.md:32` and `docs/evidence/phase09-qualification.md:276` each still
carry one "until the Phase 10 freeze" statement and are left untouched by rule). The edit is a
relabel: no sentence changes its claim, no API or ABI freeze statement is added, and no
build id, sealed body or pin moves (no `core/`, `runner/` or `tools/` file is touched).

Where Phase 10 decided an item (the settled set): only the container format strings and
their pairing with the state versions (row 11). The other names the lens list R8 names
(state flag names, closed recorder-failure detail names, the third SI trace kind name,
`T64_REAL_KEEP_WORKSET`, the info call) were not decided by Phase 10 and are provisional
until the Phase 11 freeze candidate.

## 13. Phase 9 carry-in triage

Written by plan 10-16 (phase-wave 4). Evidence tier: code observations and measurements at
the commits named; the one build edit is a CMake source list, nothing here is a
compatibility or hardware claim. Every item tagged "owner Phase 10" by
`09-REVIEW-FIX.md` ends in one of done, deferred (to Phase 11) or dropped with a reason, so
Phase 11 inherits nothing silently. The first four rows are the four items of the plan;
the rows after them are items the same review file tagged for Phase 10 and that the plan
did not name.

| # | Item (origin) | Disposition | Reason and evidence |
|---|---------------|-------------|---------------------|
| T1 | Split `unit.core.state.import` (09-REVIEW-FIX "Margin to watch" and L2 IN-05: ASan 1,049 to 1,339 s, 97 percent of the sanitizer wall; the 45-of-45 name bound) | deferred to Phase 11 | The v26 coverage was added as its own CTest names (`unit.core.state.import-v26-roundtrip`, `-v26-hostile`, plan 10-12), so the Phase 9 binary did not grow. A split changes the case list or the name set of a binary that `docs/spec/phase09-cases.json` inventories, and that file's digest is an F2 pin of `harness.phase09-qualification` (`inventory_sha256`, a closed Phase 9 record). `core/tests/test_state_import.c` and `docs/spec/phase09-cases.json` have an empty `git diff` against the Wave 0 base, so the ASan cost is the Phase 9 measurement and is not re-measured here. The hosted cost fits the 15-minute limit with the `COST` start order of plan 10-04: `unit.core.state.import` starts 2nd (F-120), warm gate-macos at most 496 s and a cold projection of 682 s against 900 s (section 6, F-122); the reversal rule of section 6 (a new numbered plan if plan 10-19 measures more than 900 s) is the trigger, not a split |
| T2 | Replay-driver duplication (L3 IN-02: the in-process driver `t64_replay_play`, `t64_replay_report_check` and `t64_replay_player_end_at` duplicates the runner play loop with different terminal logic) | deferred to Phase 11 | No state or digest effect. The unit cases validate the driver and the real controls validate the runner loop (both exist today), so unifying them needs a change to `runner/t64_replay.c`, which is the host loop Phase 11 reshapes with the API (facade, poison, reset), and it changes the source closure of the Phase 9 inventoried binary `runner/test_replay` plus the runner build identity. Doing it before the API shape exists would be redone |
| T3 | Dead `t64_artifact_publisher.c` link in `test_replay` (R2 RC-04) | done | Commit `3deee7d` removes the source from the `test_replay` target of `runner/CMakeLists.txt` only (`t64-runner-real`, `t64-runner-test` and `test_artifact_publisher` use the publisher and keep it). Proof it was dead: `nm -u` of `t64_replay.c.o` and `tests/test_replay.c.o` (the objects of the target before the edit) lists none of the 8 symbols `t64_artifact_publisher.c.o` defines, and the relinked `test_replay` defines 0 `t64_artifact` symbols. After: `unit.replay.suite` and `runner.replay-mix` pass (2 of 2, 4.1 and 3.9 s; `ctest -N -R` on the two-name regex reports 2); `python3 -B scripts/check-phase09-cases.py --build build/gate --suite replay` validates the 15-case inventory with no "Phase09 case inventory error", so the case list of the Phase 9 binary did not change; `harness.phase09-qualification`, `harness.rdp-oracle-authority`, `harness.state-field-use-audit`, `gov.rights.selftest` and the test inventory (253 names) pass. The act-image Linux compile of the preflight drops from 229 to 228 units, the one object the target no longer builds |
| T4 | Stale code and header comments ("provisional until Phase 10" in production code; Research C6) | deferred to Phase 11 | Editing a `core/` comment moves `T64_BUILD_ID` (the fingerprint of `core/CMakeLists.txt`), and ROADMAP Phase 11 "Order" already assigns them to the Phase 11 API-shape plan. Re-measured at this plan with a wrap-aware search: 20 lines in 8 files (`core/include/tendies64/t64_machine.h` 12, `core/t64_state.c` 2, `core/t64_internal.h` 1, `core/t64_machine.c` 1, `runner/t64_artifact_publisher.h` 1, `runner/t64_replay.h` 1, `runner/t64_save_flags.h` 1, `runner/tests/test_state_runner.c` 1; `tools/` has none), against 19 at planning: one phrase wraps over a comment star (`t64_machine.h:598-599`) and one reads "until the Phase 10 interface review" (`t64_machine.h:120`), which a one-line search for "provisional until Phase 10" misses. Phase 11 verifies with `git grep -n -i -E 'until (the )?Phase 10' -- core runner tools` printing nothing. `fixtures/state/decode_container.py` carries one more and is a pinned Phase 9 oracle that is never edited. The closed records `docs/claims/phase09.md:32` and `docs/evidence/phase09-qualification.md:276` ("until the Phase 10 freeze") stay untouched; `git diff --quiet -- docs/claims docs/evidence` exits 0 |
| T5 | Closed detail names for the recorder failures (L3 IN-01: checkpoint, op and byte caps end in the generic `host-error`) | deferred to Phase 11 | Needs a result-protocol vocabulary change and a checker change; `docs/spec/replay.md` states the limit and now calls the naming "a Phase 11 item" (row 10 of section 12) |
| T6 | `T64_REAL_KEEP_WORKSET` production retention switch (L3 IN-06) | deferred to Phase 11 | The 09-REVIEW-FIX FX-18 text records that gating it broke two Phase 8 tests; it is a production supervisor name, so it belongs to the Phase 11 API and name review and moves the runner build identity |
| T7 | Public name of the third SI trace kind (L1 IN-04), dead scratch facts and packet metadata (L2 IN-02 and IN-04) | deferred to Phase 11 | Core names and comments: any edit moves the build id, and the names are part of the provisional set until the freeze candidate (section 12 settles only the container format strings) |
| T8 | Case-to-CTest mapping checker follow-up (L4 IN-02: the case inventory proves the case list, not a CTest mapping) | deferred to Phase 11 | A checker change with no state or digest effect; Phase 11 builds new installed-consumer case lists and is the natural place to add the mapping once; every listed case was traced as reached in the Phase 9 review |

### 13.1 Phase 10 findings deferred to the Phase 11 freeze-candidate review

Recorded at the direction of the plan 10-16 dispatch; these are carry-ins, not edits of this
plan. None blocks Phase 10, none changes a pin, and each is test, generator or
documentation work that does not change a shipped digest. Counted against guardrail G3
(at most 5 carried warning-level gaps): two warnings (W2-01, W2-02) and two info items
(DEF-W1-01, W1-03).

| Item | Source | What is open | Why it waits |
|------|--------|--------------|--------------|
| W2-01 | `10-REVIEW.md` Wave 2, `10-13-SUMMARY.md` | The Z product bound `state_z_products_bounded` (`core/t64_state.c:1490`) and its parity case `check_preflight_parity` (`core/tests/test_state_import_v26.c:1295`) are not pinned by a committed boundary test: two perturbed bounds (limit `INT64_MAX / 2`, divisor `y_bound + 1`) survive every committed test | The code equals the live preflight today (D10: 6,000,000-packet differential, 22,140 boundary comparisons); the fix is a boundary parity (bisection of each derivative against the live preflight) plus two mutant rows, a new numbered plan outside the 19 of this phase; the freeze-candidate review judges pin strength of the exposed contract |
| W2-02 | `10-REVIEW.md` Wave 2, `10-13-SUMMARY.md` | `t64_has_si_v25_profile` and `t64_has_si_v26_profile` are not tokens of `scripts/list-membership-sites.py`; the committed `docs/spec/profile-membership-sites.txt` lacks six site rows (`t64_has_si_profile` and the importer sites `state_commit`, `state_domain_for`, `state_parse_table`, `state_read_rdp`, `state_read_rdp_carried`) and three details | The script is a Phase 9 bound path (`SOURCE_PATHS` of `scripts/qualify-phase09.py`) and the list is a discovery aid, not behavior; the exact-tuple registry, the domain check and `unit.core.profile.v26-predicate-matrix` (25 ids) are independent guards. A future family author greps for `t64_has_si_v26_profile` until the two tokens and a regenerated list land in one commit |
| DEF-W1-01 | `10-REVIEW.md` Wave 1 (W1-01), `10-08-SUMMARY.md` | Three sibling copies of the perspective and major-edge floor are unpinned behaviorally: `t64_rdp_tmem_triangle_pixel_rgba8` (combiner-profile texel path), `project_texture_q16` (LOD projection) and `texture_channel_q16` (major edge), at `core/t64_rdp_tmem.c:1218-1221`, `core/t64_rdp_raster.c:495` and `:556-557`; five scratch mutants pass every behavioral test | Not required by VER-06 (VG-01 to VG-05 and the NTSC vector are closed); a new numbered plan with its own mutant rows if the freeze-candidate review judges the cost worth it |
| W1-03 | `10-REVIEW.md` Wave 1 | One sentence stating the `ceil` rule in the v8 clock section of `docs/spec/vi.md` (the NTSC comment cites a section that does not state the rounding) | `docs/spec/vi.md` is not one of the seven docs of the plan and a new sentence is not a relabel; docs only |

W2-03 (the VI state had no committed completeness rows) was not carried: it is closed by plan 10-20 (section 16), so the G3 count stays at five (W2-01, W2-02, DEF-W1-01, W1-03 and D-10-17-01).

ROADMAP Phase 11 carries these in its "Carry-ins" line (docs only, added by plan 10-16).

### 13.2 Checks of the plan end

| Check | Result |
|-------|--------|
| relabel gate (section 12) | `stale 0` |
| `grep -c "build-identity-bound"` in `docs/spec/state-container.md` and `docs/spec/machine-contract.md` | 2 and 1 (written in plan 10-11, checked here and not rewritten: `machine-contract.md:9` and `state-container.md:17, 217`) |
| `sh scripts/preflight-phase10.sh` after commit `3deee7d` (clean committed tree, host load 9.5, above the quiet-host threshold of 8, so seconds are an upper bound) | PASS in 86 s: oracle-authority, ctest-counts (45 rows, 0 mismatches), host-load WARN only, head-ci green on `f57b019`, timeout-audit (106 TIMEOUTs, lowest ratio 2.9), pii, linux-compile 46 s (228 units), post-gate-replay 38 s (11 steps) |
| `PINNED_REQUEST_SHA256` | unchanged (`e131cbe7...`); `harness.rdp-oracle-authority` passes |

## 14. Qualification rehearsal (plan 10-17)

Written by plan 10-17 (phase-wave 4). Evidence tier: a native rehearsal of the Phase 10
qualifier on the head `da6af0d` (plan 10-17 Tasks 1 and 2 committed, tree clean). It shows
that the qualifier runs every stage and verifies its own record on this tree. It is a
rehearsal, not the qualification: plan 10-19 runs it once on the audited head. Nothing here
claims hardware accuracy or an oracle comparison; the recorded tier is "unit-tested, authored
scope". The private record and its raw logs stay under
`build/gate/10-qualification-private/20261125T041245798491Z-da6af0d6/` and are not committed.

Commands: `python3 -B scripts/qualify-phase10.py --native --rehearsal` (exit 0) and
`python3 -B scripts/qualify-phase10.py --verify build/gate/10-qualification-private/20261125T041245798491Z-da6af0d6/record.json --rehearsal`
(exit 0, prints "this result is a rehearsal and not final evidence"). The same record
presented without `--rehearsal` exits 1: "a rehearsal record is not final evidence; it is
refused as final".

| Item | Value |
|------|-------|
| source snapshot | 60 bound inputs (changed since the Wave 0 base `4ac633e880e0c26005cb03092e8d8485e721034c` plus the 14 tools of `TOOL_PATHS`), no deleted path, clean tree |
| wave bases bound | Wave 0 `4ac633e8`, Wave 1 `f6ab8c26`, Wave 2 `6dd78598`, Wave 3 `1cded8de` (40-hex literals read from section 5; the Qualification base line does not exist yet, plan 10-18 records it, and a final record requires it) |
| test inventory | 30 `PHASE10_TESTS` names equal the 30 names of `ctest -N -L phase10` |
| tested artifact identity | core `t64-build-v1-b6c298e66f1c...`, runner `t64-runner-v1-684d67...`, runner test twin `t64-runner-v1-2d3ac6...`, ASan core `t64-build-v1-63fe5be...` (the ids embedded in `build/gate` and `build/asan`) |
| host | macOS, 1-minute load 12.03 at the start and 13.64 at the end (above the quiet-host threshold of 8, so every second below is an upper bound) |
| record total | 506.35 s on the qualifier clock (about 511 s wall with interpreter start) |

| Stage | Seconds | Limit (sum of its commands) | Largest measured before (Task 2 table) | What was observed |
|-------|---------|-----------------------------|----------------------------------------|-------------------|
| ci-scanner | 1.98 | 90 | 0.03 and 2.0 | `check-ci-yml.sh` ok; selftest 138 cases, 0 failures |
| inventory | 0.62 | 60 | 1.0 | `check-test-inventory.sh --preset gate` PASS, count 254 |
| oracle | 0.14 | 60 | 0.09 each | `--expect-blocked` exit 0 ("blocked: no oracle operation is authorized"), `--require-approved` exit 2 ("no pinned competent owner/legal decision") |
| gate | 155.88 | 600 | 143 | `gate.sh` PASS; steps sh-n 0, lint-selftest 4, fixture-qualification 1, configure 0, build 0, boot-evidence-selftest 11, test-inventory-selftest 23, test-inventory 1, ctest 103, core-lint 13, total 156; `ctest --preset gate` wall 103.34 s, 254 of 254 passed |
| phase10-junit | 9.90 | 120 | 7.2 | `ctest --preset gate -L phase10`: 30 testcases equal the block, none failed or skipped, 30.61 s summed |
| asan | 175.12 | 1,320 (120 + 600 + 600) | 1, 16 and 121 | configure 0.21, build 11.39, `ctest --preset asan -L phase10` 163.52 s wall: 30 testcases equal the block, 387.25 s summed (`import-v26-hostile` 163.51, `import-v26-roundtrip` 50.42, `state-format-v26` 47.93, `export-v26` 34.00, `export-v26-bounds` 21.27, `composite-two-run` 19.47) |
| mutants | 110.72 | 600 | 97 | 27 rows equal the driver's `--list`: 25 mutants KILLED, the equivalent control SURVIVED and the non-compiling control INVALID, each as expected |
| sweep | 51.54 | 300 | 60.6 (section 7) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` against the Wave 0 literal |

Every stage ended inside its limit; the smallest ratios of limit to observed seconds are 3.7
(the ASan selection, 600 s against 163.52 s) and 3.8 (the gate, 600 s against 155.88 s), so
every limit is still at least twice the largest second observed in the rehearsal and none
needed a change (the hermetic test requires each limit to be at least twice its recorded
measurement; the rehearsal seconds are above the recorded measurements where the host load
was higher). Limits were derived from the Task 2
measurements at commit `48c928e` (host load 7.4 to 13.7): gate 143 s, ctest -L phase10 7.2 s,
ASan configure 1 s, build 16 s and selection 120.9 s, mutants 97 s, sweep 41 s (60.6 s in
section 7), scanner selftest 2 s, inventory 1 s, oracle 0.09 s.

Findings of the rehearsal:

- First attempt (record directory `20261125T040923048566Z-da6af0d6`, no record written): the
  gate stage failed its ctest step because `integration.cold-hello` printed `not ok cold
  startup evidence directory prepared`; the Phase 6 test creates `06-05-cold-hello-attempt-NN`
  for NN 01 to 99 and all 99 existed. Environmental and pre-existing, not a Phase 10 defect;
  recorded in `deferred-items.md` as D-10-17-01 with the cause, the workaround (the 193 attempt
  directories of the two cold tests were moved, not deleted, into
  `build/gate/archived-closed-phase-runs/`) and a suggested owner. The second attempt above
  is the rehearsal.
- The AUTONOMY v1.2 prune of closed-phase run directories under `build/gate` was only done
  in that narrow, non-destructive form. The broader prune of `build/gate/08-04-evidence`
  (3,404 entries, 467 MB) and `build/gate/09-05-evidence` (2,044 entries) was refused by the
  permission system and was not performed; no test depends on it.
- The derivation text of the `mutants` stage in `scripts/qualify-phase10.py` and the plan
  commit message count "26 mutants and 2 controls"; the driver lists 25 mutants and 2
  controls (27 rows). The check itself is exact (rows equal `--list`); only the sentence is
  off by one. It is not edited here so that the rehearsal stays the run of the committed
  code; plan 10-18 corrects it with the other audit edits, if any. Corrected by plan 10-18
  in commit `4dd936a` (the stage derivation now reads "25 mutants and 2 controls, 27 rows";
  text only: the limit, the command and the exact row check are unchanged, and
  `harness.phase10-qualification` passes). The rehearsal record above stays bound to
  `da6af0d`; the one qualification of plan 10-19 runs fresh on the audited head.
- The hermetic test `harness.phase10-qualification` took 5.2 to 6.4 s natively at load 8 to 13
  (8.99 s inside the loaded JUnit run), under the 10 s of the plan; its `TIMEOUT` is 120 s.

## 15. Qualification (plan 10-19)

Written by plan 10-19 Task 1 (phase-wave 4). Evidence tier: a native attempt of the Phase 10
qualifier on the audited tree, recorded as it happened. **Attempt 1 failed in the gate stage and
produced no record; the plan stopped at the tracer gate and was returned to the orchestrator.**
No source, test, script or pin was edited after the failure, and the executor did not retry the
qualification (the plan says a failed stage stops the plan). There is no qualification record
yet, so nothing below is a pass.

Update after the repeats (section 15.1): a fourth native attempt on the same unchanged source
passed and its record verifies; the three failed attempts, this one first, stay in the evidence
beside it. This section up to section 15.1 records attempt 1 as it happened.

Command: `python3 -B scripts/qualify-phase10.py --native` (no `--rehearsal`), exit 1, last line
`qualify-phase10: refused original evidence: stage gate command 0 exited 1, expected 0`. The
private step logs of the attempt are under
`build/gate/10-qualification-private/20261125T051452604175Z-1f862a0f/steps/` and are not
committed; no `record.json` exists in that directory.

| Item | Value |
|------|-------|
| source head at the attempt | `1f862a0f7a255c5f0dd1dabb488acf44dc0a5259`, tree clean (`git status --short` empty) before and after |
| Qualification base | `ad39b6be2f803af45af15d4ed511b72a89460339` (section 5) |
| changed paths, base to head | `.planning/ROADMAP.md`, `.planning/STATE.md`, `10-18-SUMMARY.md` and `PIN-IMPACT.md` only (the Task 1 changed-path check passed), so every bound source input equals that of the base |
| preflight at the start | `sh scripts/preflight-phase10.sh` PASS in 41 s (oracle-authority 0, ctest-counts 1, host-load 0, head-ci 1, timeout-audit 0, pii 0, linux-compile skipped, post-gate-replay 39) |
| host | macOS, 18 logical cores; 1-minute load 6.19 at the start of the attempt and 21.56 at its end (the preflight read 7.26 and the changed-path check 6.47 shortly before) |
| cold-boot attempt counters | 2 and 2 before the attempt (cap 99), so the D-10-17-01 cause was not reached |

| Stage | Result | Seconds (limit) | Observed |
|-------|--------|-----------------|----------|
| ci-scanner | ok | 1.79 (90) | `check-ci-yml.sh` ok, selftest 138 cases, 0 failures |
| inventory | ok | 0.58 (60) | count 254 |
| oracle | ok | 0.14 (60) | blocked as expected, `--require-approved` exit 2 |
| gate | **failed** | 441 wall (600) | `scripts/gate.sh` steps before ctest passed (lint-selftest 3, boot-evidence-selftest 11, test-inventory-selftest 35); `ctest --preset gate`: 251 of 254 passed, 3 failed, `Total Test time (real) = 389.31 sec` against 103.34 s in the 10-17 rehearsal |
| phase10-junit, asan, mutants, sweep | not run | | the qualifier stops at the first failed stage |

The three failures are supervised-run tests of Phases 8 and 9 (labels `controls runner`); none is
a Phase 10 name:

| Test | Seconds in the attempt | Observed | Expected |
|------|------------------------|----------|----------|
| `runner.save-roundtrip` | 45.73 | the baseline run passed (tick 540,634) and every case up to `import-refused` was ok; in the `export-publication` case `run->supervisor_exit == 0`, `outcome == "pass"`, `detail == "positive-match"`, `stop_kind == "authored-completion"`, `log_bytes == 144` and the log SHA-256 checks failed (lines 384 to 387, 766, 767, 681 of `runner/tests/test_save_roundtrip.c`) | all cases ok |
| `runner.state-export` | 97.34 | `case import-option-refusals: FAILED`, `FAIL line 1813: run->guest_tick > 0u` (every case before it was ok) | all cases ok |
| `controls.real.rdp-drain-cadence` | 42.74 | failed; its own output was not read before it was overwritten (see below) | pass |

What the data say. The slowdown was machine-wide and bursty, not specific to the failed tests:
the slowest passing tests ran far above their usual seconds in the same window
(`harness.phase10-qualification` 91.64 s against 5 to 9 s, `unit.core.state.import` 221.65 s,
`runner.cpu-diagnostic` 71.19 s, `gov.pii.repo` 71.10 s, `unit.core.state.import-v26-hostile`
62.19 s), and the host load rose from 6 to 21. The supervised runs of these tests carry wall
backstops (`--wall-ms 30000` in the save round trip harness, shorter ones in the controls suite);
a supervised child that is starved past its backstop ends non-zero and fails exactly the
assertions above. That is the most likely cause but it is an inference: the attempt's own
`LastTest.log` was overwritten by the first diagnostic below before the third test's output was
read (lesson: copy `build/gate/Testing/Temporary/LastTest.log` before any further ctest run), and
the cause of the load spike on the shared host was not identified. The persistent load before the
attempt (about three busy cores) comes from unrelated long-running host workloads, and an
unrelated virtual machine held about 10.8 GB resident.

Diagnostics run after the failure. None is a qualification record and none changed a tracked file:

| Diagnostic | Result |
|------------|--------|
| `ctest --test-dir build/gate -R` the three names, `-j1`, load 13.6 | 3 of 3 passed: `controls.real.rdp-drain-cadence` 0.29 s (42.74 s in the attempt), `runner.state-export` 6.43 s (97.34 s), `runner.save-roundtrip` 46.46 s (45.73 s) |
| `sh scripts/gate.sh` once more on the same tree, load 7.7 at the start and 9.3 at the end | failed again in ctest with three other names: 251 of 254 passed, `Total Test time (real) = 213.23 sec` (103.34 s in the rehearsal), test-inventory-selftest 80 s (23 to 35 s before). `runner.cpu-diagnostic` (112.79 s): `guest record control exit=4 output=classification=harness-error status=child-or-wall-bound`; `integration.open-ipl3-hello` (45.19 s): TAP `not ok 22 - wall kill after correct guest text takes harness-error precedence`; `controls.suite-dropped` (11.84 s): `case truncated-verdict failed: t64-supervisor: outcome=harness-error detail=wall-backstop`. No name is in both runs, and every one reports a supervisor wall limit |

Reading. The tree did not change between the 10-17 rehearsal (254 of 254 at load 12 to 14), the
10-18 clone gate (254 of 254, 123 s, load 6 to 10.7) and these runs, while the same gate now fails
three different supervised-run tests per run with ctest wall 213 to 389 s, so the evidence points
at the host state, not at a Phase 10 edit; it does not prove it. The qualification must not be
repeated until a plain `sh scripts/gate.sh` passes on this host, and a repeat that passes is
recorded together with this failed attempt, not instead of it.

Disposition. Stopped at the tracer gate per the plan; no expansion task (the recording of the cold
run, the evidence and the claims) was started. The choice between a repeat on a quiet host and a
numbered fix plan belongs to the orchestrator. A repeat creates a new record directory and
consumes one cold-boot attempt number per gate run (the counters stand at 4 and 4 after the gate
stage of the attempt and the diagnostic gate run).

### 15.1 Repeats and the passing final record

Written by plan 10-19 when it was resumed (Tasks 1 recording, 3 and 4). The orchestrator
repeated the one qualification command three more times and the executor read the results;
nothing was edited between attempts (`git status --short` empty, `git diff` of every bound
path against the Qualification base empty, the head stayed `b9d957c391b128622912481a99ffe24e6c84e609`
for attempts 2 to 4), no limit was widened and no wall backstop was changed. Evidence tier: native
measurements; the record is `tier: unit-tested, authored scope` and says
`claims.hardware_accuracy: false` and `claims.oracle_comparison: false`. The four attempts, all
on the same command `python3 -B scripts/qualify-phase10.py --native` (no `--rehearsal`), in order:

| Attempt | Record directory (private, repo-relative, not committed) | Source head | Result | Failed names (supervisor wall backstop) | `ctest --preset gate` wall |
|---------|-----------------------------------------------------------|-------------|--------|------------------------------------------|----------------------------|
| 1 | `build/gate/10-qualification-private/20261125T051452604175Z-1f862a0f` | `1f862a0f7a25` | gate stage failed, no record | `runner.save-roundtrip` 45.73 s, `runner.state-export` 97.34 s, `controls.real.rdp-drain-cadence` 42.74 s | 389.31 s |
| 2 | `build/gate/10-qualification-private/20261125T054642787825Z-b9d957c3` | `b9d957c391b1` | gate stage failed, no record | `runner.cpu-diagnostic` 134.68 s, `controls.real.raw-capture-caps` 43.91 s, `controls.real.raw-capture-target-errors` 44.07 s | 242.25 s |
| 3 | `build/gate/10-qualification-private/20261125T055712530841Z-b9d957c3` | `b9d957c391b1` | gate stage failed, no record | `controls.real.audio-wrong-sample` 55.44 s, `harness.fsize-report-capture-still-bounded` 55.80 s | 187.38 s |
| 4 | `build/gate/10-qualification-private/20261125T134440416529Z-b9d957c3` | `b9d957c391b1` | **passed, `record.json` written, `--verify` exit 0** | none (254 of 254) | 77.30 s |

Reading of the three failures, with its limit. Every failed name is a Phase 8 or 9 supervised-run
test (labels `controls` or `runner`; none is a Phase 10 name) that ended at a wall backstop of the
supervisor under heavy unrelated host workloads (1-minute load 10 to 12 on a multi-core macOS host
while other CPU-heavy work ran); no name failed twice, and the names move with the host speed. The
same source passed plain `sh scripts/gate.sh` (196 s and 135 s), the 10-17 rehearsal, the 10-18
clone gate and every hosted run (two warm pairs and the cold run of section 15.2). The passing
attempt ran after the unrelated workloads were reduced. That the cause is host load is an
inference from timing and from the passes of the unchanged source, not a proven cause, and the
supervisor backstops of those tests stay as they are (they live in bound or frozen source).

The passing record (all figures are the record's own):

| Item | Value |
|------|-------|
| record id and directory | `20261125T134440416529Z-b9d957c3`, `build/gate/10-qualification-private/20261125T134440416529Z-b9d957c3/record.json` |
| record SHA-256 | `82ef8c2e8aa45db3c938fb3b2ed736299a90026ac3850c7d16ca90e503985c96` |
| `commit_sha`, rehearsal | `b9d957c391b128622912481a99ffe24e6c84e609`, `rehearsal: false`, `status: complete`, tree clean (`dirty_status_sha256` is the SHA-256 of the empty string) |
| Qualification base | `ad39b6be2f803af45af15d4ed511b72a89460339`; `qualification_drift_paths` is empty, so every bound input equals the audited tree |
| source snapshot | 60 bound inputs, `input_manifest_sha256` `35bee73db0e88da29f771481d3307c99dada8e7343ea94d2e9ee73a69cd6bceb`, no deleted path, excluded `.planning/` and `docs/ci-reviews/LOG.md` |
| wave bases bound | Wave 0 `4ac633e8`, Wave 1 `f6ab8c26`, Wave 2 `6dd78598`, Wave 3 `1cded8de`, Qualification base `ad39b6be` |
| `--verify` | `python3 -B scripts/qualify-phase10.py --verify <record>` exit 0, "record verified against the current tree" (run at head `b9d957c`, one-shot: it cannot be repeated after the evidence and claim documents are committed, because those paths are bound inputs newer than the Qualification base) |
| inventory | 30 `PHASE10_TESTS` names equal the 30 names of `ctest -N -L phase10`; names SHA-256 `c0e7d637b3f1c181b4f7cb08f032bf3aa5a84a04241005114459b08eff156f28`; 254 names in the gate |
| tested build ids | core `t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884`, runner `t64-runner-v1-684d67fa59f416d1549be743c93cfd451fc8b1ab849ac31fdd643ef2e7811f55`, runner test twin `t64-runner-v1-2d3ac6f8be3b8203341f9238c3d2c07d52a3f3363ee6a078ef870d9b29f60051`, ASan core `t64-build-v1-63fe5be296b2b623822ce9f111b6474eb0c4462e1de642718c31f9bc31b7ad5a` (equal to the 10-17 rehearsal ids) |
| host | macOS; 1-minute load 16.1 at the start and 6.8 at the end (the start is above the quiet-host threshold of 8, so the seconds below are upper bounds); total 418.6 s on the qualifier clock, about 6 min 59 s wall |

| Stage | Seconds | Limit | Observed |
|-------|---------|-------|----------|
| ci-scanner | 2.16 | 90 | `check-ci-yml.sh` ok, selftest 138 cases, 0 failures |
| inventory | 0.65 | 60 | `check-test-inventory.sh --preset gate` PASS, count 254 |
| oracle | 0.15 | 60 | `--expect-blocked` exit 0 ("authority record valid; blocked: no oracle operation is authorized"), `--require-approved` exit 2 |
| gate | 129.41 | 600 | `gate.sh` PASS, total 129 s (sh-n 0, lint-selftest 4, fixture-qualification 1, configure 0, build 0, boot-evidence-selftest 13, test-inventory-selftest 24, test-inventory 0, ctest 78, core-lint 9); 254 of 254 passed, ctest wall 77.30 s |
| phase10-junit | 7.16 | 120 | 30 testcases equal the block, none failed or skipped; JUnit SHA-256 `7f53de882887238a1b1cac4c858b4f532d917d44f4a5f6b62135680b95b5e923`; the qualifier hermetic test 7.13 s, `import-v26-hostile` 7.15 s, `import-v26-roundtrip` 2.03 s, `export-v26` 1.87 s, `state-format-v26` 1.45 s, the other 25 names 0.003 to 1.31 s |
| asan | 136.95 | 1,320 (120 + 600 + 600) | configure 0.19, build 7.71, `ctest --preset asan -L phase10` 129.05 s wall (305.08 s summed): 30 testcases equal the block, none failed or skipped; JUnit SHA-256 `d4e0144cc049173066c11d3b02924e51b3ada8897fafce59cbbd83fe9c1520d8`; `import-v26-hostile` 129.03 s, `import-v26-roundtrip` 37.44 s, `state-format-v26` 33.99 s |
| mutants | 100.72 | 600 | 27 rows equal the driver's `--list`: 25 mutants KILLED, the equivalent control `control-equivalent-depth-cap` SURVIVED, the non-compiling control `control-no-compile` INVALID, each as expected |
| sweep | 41.02 | 300 | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (21 shipped ids, Wave 0 base to head) |

Every stage ended inside its limit; the three smallest ratios of limit to seconds are 4.6 (the
gate, 600 s against 129.41 s), 6.0 (the mutants, 600 s against 100.72 s) and 7.3 (the sweep, 300 s
against 41.02 s), so every limit is still more than four times the second of this run. The
Qualification base check of the
plan (one-shot) passed before any plan 10-19 document was committed: the base is the 40-hex
literal of section 5, and `git diff --name-only` base to head lists only `.planning/` paths.
`sh scripts/preflight-phase10.sh` was run at the start of the first attempt (PASS in 41 s,
section 15 above). The cold-boot attempt counters (`06-05-cold-hello-attempt-NN` and
`06-05-cold-startup-attempt-NN`, cap 99, D-10-17-01) stand at 9 and 9 after the four attempts, so
the cap was not reached.

### 15.2 Cold-cache hosted run (plan 10-19 Tasks 2 and 3) and the GATE-01 closeout

Written by plan 10-19 Task 3. Evidence tier: a hosted measurement read with `gh run view` and
`gh cache list` (read only; the cache deletion and the workflow dispatch were done by the
orchestrator, never by the executor), nothing else. COLD_RUN_ID is `45550277510`.

| Item | Value |
|------|-------|
| run | workflow `ci`, event `workflow_dispatch`, head `f081d392763ad384c3d50ae1b24b709d5af3b7d4` (the audited source tree: `git diff --name-only ad39b6be2f803af45af15d4ed511b72a89460339 f081d392763ad384c3d50ae1b24b709d5af3b7d4` lists no path outside `.planning/` and `docs/ci-reviews/LOG.md`, and the run head is an ancestor of the Qualification base), created 04:57:47Z, updated 05:10:04Z, conclusion `success` |
| Task 2 automated checks (re-run by the executor with the planned wording) | conclusion `success`; `Restore pinned Binutils=success` and `Gate=success` on gate-macos; exactly one `Cache not found for input keys: binutils-macOS-ARM64-69d0bfd4...` line and one `Cache saved with key: binutils-macOS-ARM64-69d0bfd4...` line, so the run was cold and the Binutils build ran inside the Gate step; no match was loosened |
| Task 3 verify (integer comparison) | `gate-macos seconds: 731`, `test "$s" -le 900` true; `COLD_RUN_ID` set; exactly one `gate-macos` job concluded `success` |
| gate-macos job | 04:57:52Z to 05:10:03Z = **731 s** inside the **900 s** limit by **169 s** (18.8 percent of the limit); the 600 s limit of Phase 9 would have been exceeded by 131 s |
| gate-macos steps (s) | Set up job 2, Checkout 6, Restore pinned Binutils 1 (miss), **Gate 653**, audio qualification records 40, Phase 9 qualification records 5, emit steps 1 + 0 + 0 + 1, rights scan 0, PII scans 3 + 4 + 0 + 0, upload 2, Post Restore pinned Binutils 3 (save), Post Checkout 1, Complete job 4 |
| Gate step inside (`[gate]` lines of the log) | sh-n 0, lint-selftest 12, **fixture-qualification 219 (the Binutils build)**, configure 4, build 31, boot-evidence-selftest 31, test-inventory-selftest 57, test-inventory 3, **ctest 265** (254 of 254, `Total Test time (real) = 265.39 sec`), core-lint 28, total 653 |
| gate-linux job | 04:57:50Z to 05:02:00Z = 250 s; Restore pinned Binutils 3 s (hit), Gate 200 s, audio records 28 s; queue 2 s |
| queue | gate-macos 4 s, gate-linux 2 s; run wall 737 s, critical path 735 s (gate-macos) |
| cache state after the run (`gh cache list`) | macOS entry recreated (id 11078086756, 5.03 MiB, created 05:09:56Z, key `binutils-macOS-ARM64-69d0bfd4279dc48f436582c76ec0e08c98eef6e7e27d969d562c2db17485b384`); Linux entry untouched (id 10796791825, 31.10 MiB, created 2026-11-19). Assumption A5 (a timed-out job saves no cache) was not exercised: the job did not time out |
| scanner rule and pinned relations | `scripts/check-ci-yml.sh` CI-TIMEOUT: `gate-macos` 1 to 15, other gate jobs 1 to 10 (138 selftest cases, 0 failures in the final qualification); `ci.yml` `gate-macos` `timeout-minutes: 15`, `gate-linux` `timeout-minutes: 10`; the F-106 relation (the closed Phase 8 qualifier keeps its own outer limits) unchanged (F-117) |
| LOG rows | Runs row for 45550277510, findings F-124 (the measurement against the 900 s limit) and F-125 (the failed native attempts and why no limit moved) in `docs/ci-reviews/LOG.md` |

Cold against the projections and the warm samples (one sample each; F-119 gives a run-to-run
spread of up to 52 s on the same source):

| Reference | Seconds | Cold 731 s against it |
|-----------|---------|-----------------------|
| warm before, F-111 (run 44795894664, source `d3211e9`) | 487 | +244 |
| warm after, F-120 (Wave 0), F-121, F-122, F-123 (this head) | 346, 326, 496, 385 | +385, +405, +235, +346 |
| cold projection before Phase 10, F-111 | 673 | +58 (higher than the old projection; still inside 900 s) |
| cold projection of plan 10-18 (F-123 warm 385 plus 186) | 571 | +160 |
| plan 10-17 primary projection (packed to serialized) | 696 to 735 | inside, at the top end |
| plan 10-17 cross-check projection | 709 to 806 | inside |
| 900 s limit (plan 10-04, F-115) | 900 | margin 169 |

Decomposition of the 346 s that the cold run adds to the warm F-123 run (385 s): fixture-qualification
+217 s (219 s against about 2 s warm; F-096 and F-111 measured 179 and 186 s for the same build),
ctest +69 s on the same 254 names (a runner-speed difference of 17 s beyond the F-119 spread, not
a cold-cache cost), test-inventory-selftest +6 s, build +9 s, core-lint +11 s, lint-selftest +4 s,
boot-evidence-selftest -6 s, audio records +12 s and the remaining small steps. The F-123 cold
projection of 571 s understated the measured run by 160 s: it added a fixed 186 s Binutils delta
to a warm sample, and the cold run saw 31 s more Binutils time and about 129 s more elsewhere on
the same code (ctest +69 s, the other steps, the audio records and the job set-up about +60 s), which reads as
a slower runner or a slower start of the same steps, an inference from the per-step seconds and
not a proven cause. The Wave 2 projection of 682 s (warm 496 s, the slowest warm sample) was
closer. The measured number is the one that counts for GATE-01.

**GATE-01 closeout.** The success criterion is a measured cold-cache hosted run inside the
15-minute limit, not a projection; run 45550277510 measured 731 s on the audited source tree, so
the criterion is met with 169 s of margin on one sample. The G4 exception reversal rule (cold run
above 900 s or cancelled at the limit, which would return a new numbered plan that moves
non-essential Phase 10 cases to a non-gating lane) is not triggered. The exception stays as
recorded in the ledger: no offset is claimed, the redundant `rsp-no-hle` guard stays declined (F-116:
its removal needs `scripts/qualify-audio.py` and `docs/spec/audio-cases.json` edits that F-106
freezes) and Phases 11 and 12 follow G4 as written. If a later cold run is spent, it would have to
be read against the 169 s margin and the 52 s spread: a run above about 850 s is a warning.

## 16. VI completeness rows (plan 10-20, gap closure of W2-03)

Written by plan 10-20 Task 3 (gap closure after `10-VERIFICATION.md`, status `gaps_found`, success criterion 2, VI
half). Evidence tier: code observations and native measurements on this host at the commits named; test source, one
mutant-driver row and qualifier text only. No production source changed (nothing under `core/` outside `core/tests/`,
nothing under `runner/` or `tools/`), so every build id and shipped digest stays as qualified; no CTest name was added
(254 gate names, 30 `phase10` names, two cases of `test_digest_v26`); the gate, K1, sweep and preflight results of Task 4 are recorded in section 16.3.

| Item | Value |
|------|-------|
| rows | `vi_members` in `core/tests/test_digest_v26.c`: 120 rows over `t64_vi_state` (1,208 bytes, 17 padding bytes unclaimed): 114 member rows (14 `registers`, 14 `active_registers`, 8 64-bit counters, `field`, `current_readback`, `refusal_reason`, 2 `packet_lengths`, the 37 members of each of the 2 `t64_video_packet_info` slots less the 2 excluded strings, `enabled`, `sequence_exhausted`, `latch_pending`) and 6 named exclusion rows |
| byte tallies (checked on every id) | 791 member bytes, 400 excluded bytes (8 + 8 + 2 x (64 + 128)), 17 padding bytes (4 after `refusal_reason`, 4 after `filter` in each slot, 5 after `latch_pending`); 791 + 400 + 17 = 1,208 |
| size pins | `_Static_assert(sizeof(t64_vi_state) == 1208)` and `_Static_assert(sizeof(t64_video_packet_info) == 488)`; a new member fails the build until it has a row. The layout was read from the compiler (`offsetof` and `sizeof` of every member) before the literals were written and equals the facts of the plan |
| exclusion reason 1 (`packet_head`, `packet_count`) | host observer cursor and occupancy, not guest identity: `core/t64_internal.h` ("Host observer cursor and occupancy; neither is part of guest identity") and the packet comment of the VI section of `core/t64_digest.c` ("packet_head/count are observer state and intentionally stay out of the digest"), so the host drain cadence cannot move the digest |
| exclusion reason 2 (per-slot `profile_id`, `build_id`) | `core/t64_digest.c` hashes them only under `!has_v25_framing`; the v25 and v26 families (the si-v1 and si-v2 ids) bind identity through the domain string and the profile digest of the identity section instead |
| ids | the descriptor 13 stem `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2`: **every byte** (1,208 flips, exactly 417 inert, 791 change the stream); the three save ids `-e4k`, `-e16k`, `-s32k` and the state 25 stem `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1`: representative (first byte of every row, last byte of every excluded row, every padding byte). One flip of `vi.phase_cycles` is tied to the SHA-256 digest on each id |
| W2-03 | CLOSED. The finding was written at the Wave 2 head (`10-REVIEW.md`); the closing note and the section "Gap closure (plans 10-20 and 10-21)" there record the closure, and the Final audit SC2 row no longer says the VI state was covered by committed rows at the audited head |

### 16.0 RED and GREEN by mutant

The mutant row `v26-digest-omit-vi-phase-cycles` removes the `hash_u64(sink, vi->phase_cycles)` call of the VI section of
`t64_digest_stream` (anchor: that one line, exactly one occurrence; `--check-anchors` exits 0).

| Step | Commit | Command | Result |
|------|--------|---------|--------|
| RED | `be683bf` (row only, tree clean) | `python3 -B scripts/p10-mutants.py --run v26-digest-omit-vi-phase-cycles --no-controls` | exit 1, row **SURVIVED** against the expected KILLED, 3.2 s (`build/gate/10-20-red-mutant.log`, `build/gate/10-20-red-results.txt`, private, not committed) |
| attribution of the RED | scratch probe, not committed | the canonical stream of the retained-packet state of the stem id (`build_vi_frame`), before and after flipping bit 0 of `vi.phase_cycles`, on a copy of the digest walker with the row's one-line edit and on the main tree | `existing-case-under-mutant: exit 0` (1,556 checks, 0 failures), `stream-equal-under-mutant: yes` (10,545,043 bytes), `stream-equal-unmutated: no` (10,545,051 bytes, the 8-byte difference is the removed call) |
| GREEN | `0ed0d75` (rows on the stem id), `49c935a` (four more ids) | the same command on the clean committed tree | exit 0, row **KILLED** by `test_digest_v26:v26-completeness`, 5.9 s (`build/gate/10-20-green-mutant.log`) |
| what the case reports under the mutant | scratch control | the committed case linked with the mutated digest object | all 8 bytes of `phase_cycles` are "not in the canonical stream", inert count 425 against 417, the digest tie fails; after `49c935a` one more failure on each of the other four ids (12 byte failures, 5 tie failures in total) |

Full driver run on the clean committed tree at `49c935a` (`python3 -B scripts/p10-mutants.py --run`,
`build/gate/10-20-full-mutants.log`): 28 rows (`--list` prints 28, 26 expected KILLED), 26 KILLED,
`control-equivalent-depth-cap` SURVIVED, `control-no-compile` INVALID, every row as expected. **147 s wall** at 1-minute
host load 10.7 at the start and 13.5 at the end (above the quiet-host threshold of 8, so an upper bound); the new row 5.9 s,
the slowest rows `v26-import-z-products-unchecked` 14.8 s and `v26-import-addr-unchecked` 12.7 s.

### 16.1 Qualifier derivation figures and case cost

Commit `7e9f3b8` changes only derivation text and `measured_seconds` arguments of the `mutants`, `gate` and `asan`
stages of `scripts/qualify-phase10.py` (and one sentence of the `_stage_table` docstring pointing here); no limit literal,
regex, marker, check or other stage moved, and `scripts/tests/test_phase10_qualification.py` is unchanged. The hermetic
test (`harness.phase10-qualification`) and `repo.p10-mutant-anchors` pass.

| Stage | Text before | Text now (source) | `measured_seconds` | Limit (unchanged) |
|-------|-------------|-------------------|--------------------|-------------------|
| mutants | 25 mutants and 2 controls, 27 rows, 97 s at load 8 | 26 mutants and 2 controls, 28 rows, 147 s (this section) | 97 to 147 | 600 (4.1 times) |
| gate | 143 s on a warm build at load 7.4 (plan 10-17) | 129.41 s, the passing plan 10-19 record (section 15.1, load 16.1 at the start and 6.8 at the end) | 143 to 129 | 600 (4.6 times) |
| asan | 1 s, 16 s and 120.9 s wall, 278.5 s summed (plan 10-17) | 0.19 s, 7.71 s and 129.05 s wall, 305.08 s summed (section 15.1, same loads) | 1, 16, 121 to 0.2, 7.7, 129 | 120, 600, 600 (the CTest limit is 4.6 times the wall and 1.97 times the serial sum) |

No newer measurement of the whole gate stage or of the whole ASan selection existed when the text was refreshed (the
Task 4 gate run of this plan is recorded in section 16.3, after this commit), so the derivations say so instead of inventing a figure.
Reading, with its limit: the 600 s CTest limit of the asan stage is 1.97 times the serial sum of 305.08 s, not 2 times;
the hermetic factor rule is taken on the wall seconds (129, 4.65 times), the four-job preset overlaps the names and the
longest name alone is 129.03 s. It is a margin for plan 10-21 to watch, not a limit that was widened or changed here.

| Measure of `unit.core.digest.v26-completeness` | Seconds | Host load (1-minute) |
|-----------------------------------------------|---------|----------------------|
| native, Wave 2 row of the Gate-growth ledger (section 6, scratch clone, before any VI row) | 0.40 | 12 to 15 |
| native, after the VI rows (three runs, `ctest --preset gate -R`) | 1.10, 1.10, 1.08 | 15.8, 14.9, 14.9 |
| native, one run in the first build after the stem rows | 0.81 | 13.0 |
| ASan and UBSan (`ctest --preset asan -R`, configure 0.14 s, build 1.3 s) | 7.92 | 13.9, no sanitizer report |

The case grew by about 0.7 s natively at comparable load (the stem sweep is 1,208 stream captures of 10.5 MB each); the
budget of the plan (5 s native at load 8 or below) holds, so the representative mode was not narrowed and no row, id or
sweep was dropped. The case carries no `COST` and no `TIMEOUT` property, so the timeout audit has nothing to compare.

### 16.2 The record of plan 10-19 is superseded by design

The qualifier source (`scripts/qualify-phase10.py`, a bound input) and a bound test source
(`core/tests/test_digest_v26.c`) changed after the passing record of section 15.1, and the record binds the stage
table text, so it no longer verifies against this tree. Plan 10-21 re-qualifies and records the new evidence; nothing in
section 15 was rewritten.

### 16.3 Local gate, K1, sweep, protected paths and preflight (plan 10-20 Task 4)

Run on the clean committed tree at head `e705ce1` (the docs commit that follows `7e9f3b8` and `49c935a`); every
command ran once, nothing was retried and no limit was widened. Evidence tier: native measurements; the 1-minute host
load is given for each, and a start above the quiet-host threshold of 8 makes the seconds an upper bound.

| Check | Result | Seconds and load |
|-------|--------|------------------|
| cold-boot attempt counters (D-10-17-01, directories `06-05-cold-hello-attempt-NN` and `06-05-cold-startup-attempt-NN` under `build/gate`) | 9 and 9 (cap 99, workaround threshold 90), nothing moved | |
| preflight at the start of the plan (`sh scripts/preflight-phase10.sh --no-replay`, clean tree before the first commit) | PASS, total 3 s | load 6.68 |
| `sh scripts/gate.sh` | `[gate] PASS`, 254 of 254 passed, `Total Test time (real) = 81.69 sec` | total 136 s (sh-n 0, lint-selftest 4, fixture-qualification 1, configure 0, build 2, boot-evidence-selftest 13, test-inventory-selftest 22, test-inventory 1, ctest 81, core-lint 12); load 5.9 at the start and 11.5 at the end |
| `ctest --preset gate -R '^unit\.core\.k1\.'` | 2 of 2 passed (`k1.dispatch-refusal` 0.19 s, `k1.apply-refusal` 0.39 s) | |
| `python3 -B scripts/digest-sweep.py --base <Wave 0 base> --head HEAD` (Wave 0 base `4ac633e880e0c26005cb03092e8d8485e721034c` read by the base extraction idiom) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines`, as expected because no production source changed | 45 s wall (two builds of 2.5 and 2.6 s, two sweeps of 42.3 s in parallel) |
| production paths since the Qualification base `ad39b6be2f803af45af15d4ed511b72a89460339` | `git diff --name-only <base> HEAD -- core runner tools` lists `core/tests/test_digest_v26.c` only | |
| protected paths against the Qualification base | `git diff --quiet` exits 0 for `scripts/check-rdp-oracle-authority.py`, `fixtures/manifest.json`, `fixtures/state/decode_container.py`, `core/CMakeLists.txt`, `scripts/check-test-inventory.sh`, `docs/claims/phase09.md` and `docs/evidence/phase09-qualification.md`; `PINNED_REQUEST_SHA256` is still `e131cbe7...` | |
| `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` | exit 0, "authority record valid; blocked: no oracle operation is authorized" | |
| full `sh scripts/preflight-phase10.sh` (no `--no-replay`) | `[preflight] PASS`, total 73 s: oracle-authority 1, ctest-counts 0 (46 regex rows, 0 mismatches; 30 tests carry the label `phase10`), host-load 0 (WARN at 10.93), head-ci 2 (`origin/main` `f081d39` completed with success), timeout-audit 0 (107 TIMEOUTs, 0 too low, 0 unmeasured, lowest ratio 2.9), pii 0, linux-compile 49 (C changed since `f081d39`, first path `core/tests/test_digest_v26.c`; 228 units compiled with the act image cc 13.3.0), post-gate-replay 21 (11 `ci.yml` steps, outputs removed) | load 10.93 at the start and 11.77 at the end |

The Linux compile of the changed test unit is the first hosted-toolchain check of the VI rows; nothing was pushed (plan
10-21 pushes) and no tag was created. Plan 10-21 carries the re-qualification, the claim and evidence documents and the
independent review of this closure.

## 17. Re-qualification after the gap closure (plan 10-21)

Written by plan 10-21 Task 2. Evidence tier: one native qualification (`python3 -B scripts/qualify-phase10.py --native`, no
`--rehearsal`) on the tree that contains the VI rows, recorded as it happened; the record is private build output and this
section holds digests, counts and seconds only. **It passed on the first attempt**: no failed attempt, no retry, no limit
widened, no source, test, script or pin edited between the base and the record.

Why it exists: plan 10-20 changed a bound test source (`core/tests/test_digest_v26.c`), the mutant driver
(`scripts/p10-mutants.py`) and the qualifier (`scripts/qualify-phase10.py`), so the plan 10-19 record of section 15.1 no
longer verifies against this tree (section 16.2). **That record is superseded and kept as history**: section 15 was not
rewritten, its record directory stays under `build/gate/10-qualification-private`, and its four attempts stay in the
evidence. The independent review of the 10-20 wave (`10-REVIEW.md`, "Wave 20 independent review") ran first and its
reviewed head is `2a840a9885879b6306a51991fe7676249920605c`.

| Item | Value |
|------|-------|
| record id and directory | `20261125T155456703622Z-1fc0b1da`, `build/gate/10-qualification-private/20261125T155456703622Z-1fc0b1da/record.json` (private, not committed) |
| record SHA-256 | `1099e3971e0cca245844641ad4977b3dbc3ce523e452449efb551d75a9d487fc` |
| `commit_sha`, rehearsal, status | `1fc0b1da70acf6c9b0f3baf0b46abf9d5a69468f` (**H2**, the qualified commit: the commit that recorded the new base), `rehearsal: false`, `status: complete`, tree clean (`dirty_status_sha256` is the SHA-256 of the empty string) |
| pushed head | **H3** is the head that Task 3 pushes: H2 plus this section's one planning-only commit. Both are named in the evidence document. `git diff --name-only H2 H3` lists only `.planning/` paths, so H3 has the identical bound source snapshot of H2 |
| Qualification base | `76451024e148073804e162841c3d0a3543bd2567` (section 5; recorded after the review commit `7645102` in the planning-only commit `1fc0b1d`; the plan 10-18 value `ad39b6be...` is the historical line); `qualification_drift_paths` is `[]`, so every bound input equals the tree of the base. `git diff --name-only <base> H2` lists `PIN-IMPACT.md` only |
| origin/main hosted status checked before the run | `git fetch origin`, `origin/main` `f081d392763ad384c3d50ae1b24b709d5af3b7d4`; the newest `ci.yml` push run, 45548987145, has `headSha` equal to it, `status` completed and `conclusion` success |
| `--verify` | `python3 -B scripts/qualify-phase10.py --verify <record>` exit 0, "record verified against the current tree", run at H2 before any other commit (one-shot: it refuses once a bound document changes). The record check preceding it (newest record, `commit_sha` an ancestor of `HEAD`, not a rehearsal, complete, empty drift, `wave_bases` names the new base) passed |
| source snapshot | 62 bound inputs, `input_manifest_sha256` `eb28495da4fb2a271c65f01d3a375b6b386d36160b55c792fd6cacc7c144aa15`, no deleted path, excluded `.planning/` and `docs/ci-reviews/LOG.md`. Against the 60 inputs of the plan 10-19 record: two added (`docs/claims/phase10.md` and `docs/evidence/phase10-qualification.md`, the plan 10-19 documents, which the earlier base preceded) and three with new content (`core/tests/test_digest_v26.c`, `scripts/p10-mutants.py`, `scripts/qualify-phase10.py`); no path removed |
| inventory | 30 `PHASE10_TESTS` names equal the 30 names of `ctest -N -L phase10`; names SHA-256 `c0e7d637b3f1c181b4f7cb08f032bf3aa5a84a04241005114459b08eff156f28` (unchanged from section 15.1); 254 names in the gate |
| tested build ids (compared with the plan 10-19 record) | core `t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884`, runner `t64-runner-v1-684d67fa59f416d1549be743c93cfd451fc8b1ab849ac31fdd643ef2e7811f55`, runner test twin `t64-runner-v1-2d3ac6f8be3b8203341f9238c3d2c07d52a3f3363ee6a078ef870d9b29f60051`, ASan core `t64-build-v1-63fe5be296b2b623822ce9f111b6474eb0c4462e1de642718c31f9bc31b7ad5a`: **all four equal the plan 10-19 ids**, as expected, because no production source changed (the only core path since the base is `core/tests/test_digest_v26.c`, and the build id hashes production content and the compiler policy), so the claim `build_id` values do not move |
| preconditions | tree clean; cold-boot attempt counters (D-10-17-01) 10 and 10 before the plain gate run (cap 99, workaround threshold 90), nothing moved; plain `sh scripts/gate.sh` `[gate] PASS` in 139 s (254 of 254, `Total Test time (real) = 77.94 sec`) at 1-minute load 6.91 at its start and 11.09 at its end; then the load wait (below) |
| host load | 1-minute load 7.54 after a 50 s wait (the limit of the plan is 8 and the wait budget 15 minutes), 9.10 when the qualifier started (the record's `load_start`) and 7.28 at its end (`load_end`); the start is slightly above the quiet-host threshold of 8, so the seconds below are upper bounds |
| total | 410.78 s on the qualifier clock, 2026-11-25T15:54:56Z to 16:01:47Z (6 min 51 s wall) |

| Stage | Seconds | Limit | Observed |
|-------|---------|-------|----------|
| ci-scanner | 2.98 | 90 | `check-ci-yml.sh` ok, selftest 138 cases, 0 failures |
| inventory | 1.10 | 60 | `check-test-inventory.sh --preset gate` PASS, count 254 |
| oracle | 0.22 | 60 | `--expect-blocked` exit 0, `--require-approved` exit 2 (blocked, no oracle operation authorized) |
| gate | 128.13 | 600 | `gate.sh` PASS, total 128 s (sh-n 0, lint-selftest 5, fixture-qualification 1, configure 0, build 0, boot-evidence-selftest 12, test-inventory-selftest 23, test-inventory 1, ctest 76, core-lint 10); 254 of 254 passed, ctest wall 76.18 s |
| phase10-junit | 7.02 | 120 | 30 testcases equal the block, none failed or skipped, 23.82 s summed; JUnit SHA-256 `4d4e9987113ae7e3743681502ecc150d720d801f5d66e1a0cd6947e8b2c2214d`; the qualifier hermetic test 7.00 s, `import-v26-hostile` 7.01 s, `import-v26-roundtrip` 1.94 s, `export-v26` 1.94 s, `state-format-v26` 1.43 s, **`unit.core.digest.v26-completeness` 1.13 s** (0.40 s before the VI rows, section 16.1), the other 24 names 0.002 to 1.45 s |
| asan | 125.10 | 1,320 (120 + 600 + 600) | configure 0.14 s, build 7.84 s, `ctest --preset asan -L phase10` 117.12 s wall (274.81 s summed): 30 testcases equal the block, none failed or skipped; JUnit SHA-256 `db0cb3fb19eb490224cbf72052d4a446bdaecfe8dc4fcc9e65412d4f2a7977cc`; `import-v26-hostile` 117.10 s, `import-v26-roundtrip` 34.11 s, `state-format-v26` 30.95 s, `export-v26` 22.73 s, **`unit.core.digest.v26-completeness` 8.67 s**, no sanitizer report |
| mutants | 105.00 | 600 | 28 rows equal the driver's `--list`: **26 KILLED** (the new `v26-digest-omit-vi-phase-cycles` among them), the equivalent control `control-equivalent-depth-cap` SURVIVED and the non-compiling control `control-no-compile` INVALID, each as expected |
| sweep | 40.77 | 300 | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (21 shipped ids, Wave 0 base to head) |

Every stage ended inside its limit. The smallest ratios of limit to seconds are 4.7 (the gate, 600 s against 128.13 s, the
rule the hermetic test enforces for the whole stage), 5.1 (the ASan selection wall, 600 s against 117.12 s), 5.7 (the
mutants), 7.4 (the sweep) and 17.1 (the JUnit stage). The watch item of plan 10-20, the ASan CTest limit against the
serial sum, reads 600 s against 274.81 s, 2.18 times (1.97 times against the 305.08 s of the plan 10-19 record); the serial
sum is not the rule (four jobs overlap the names and the longest name alone is 117.10 s), and no limit moved. The qualifier
derivation text still carries the plan 10-19 and 10-20 figures (129.41 s, 129.05 s, 147 s); the measurements above are
all inside them, which is why no text or limit changes here.

Comparison with the plan 10-19 record (section 15.1): same four build ids, same 30 names, same 254 gate names, same sweep
(1599 shared lines, 0 differences); gate 128.13 against 129.41 s, ASan wall 117.12 against 129.05 s, mutants 105.00
against 100.72 s for 28 rows against 27, JUnit 7.02 against 7.16 s, sweep 40.77 against 41.02 s. The one measurable cost of
the VI rows is the case itself: 1.13 s natively (0.7 s more than before) and 8.67 s under ASan and UBSan (7.92 s in section
16.1). The plan 10-19 host-load failures did not recur: the plain gate precondition ran at 1-minute load 6.91 and the native run
started after the load wait.
