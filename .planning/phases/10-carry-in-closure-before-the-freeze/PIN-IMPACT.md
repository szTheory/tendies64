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
| any of the 14 files in `EXPECTED_CORPUS_PATHS` of `scripts/check-rdp-oracle-authority.py`: `core/tests/test_rdp.c`, `test_rdp_copy.c`, `test_rdp_triangles.c`, `test_rdp_texture.c`, `test_rdp_combiner.c`, `test_rdp_blender.c`, `test_rdp_depth.c`, `core/tests/test_vi.c`, `test_video_output.c`, `runner/tests/test_rdp_video.c`, `scripts/check-rdp-vi-cases.py`, `docs/spec/rdp-vi-cases.json`, `fixtures/rdp/fill.S`, `fixtures/manifest.json` | whole-file SHA-256 and blob id of each file, hence the derived request hash `PINNED_REQUEST_SHA256` (line 23); refusal text "authored corpus SHA-256 changed". Edits belong to the orchestrator only | `ctest --test-dir build/gate -R '^harness\.rdp-oracle-authority$'` and `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` |
| `docs/spec/rdp-vi-cases.json` (needed to list new cases of a Phase 7 binary) | `CASE_INVENTORY_SHA256` at `scripts/qualify-rdp-vi.py:28`, enforced at `:561` by the hosted step "Emit authored raw RDP VI evidence", plus the oracle request | replay the hosted emit step (`sh scripts/preflight-phase10.sh`); `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite <name>` (not in the gate) |
| a `fixtures/manifest.json` row | the oracle request until GATE-02 lands (per-row pins afterwards); the `phase09-*` rows are F2-pinned and are never touched | `ctest -R '^harness\.rdp-oracle-authority$'`; `ctest -R '^harness\.phase09-qualification$'` |
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
