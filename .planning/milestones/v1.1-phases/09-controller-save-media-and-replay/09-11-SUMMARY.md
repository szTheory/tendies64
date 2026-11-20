---
phase: 09-controller-save-media-and-replay
plan: "11"
subsystem: qualification
tags: [f2-identity-freeze, claims, exact-sha-qualification, native-record, hosted-pair, final-mode-verifier, ci-timing, evidence]

requires:
  - phase: 09-controller-save-media-and-replay
    provides: plans 09-01 to 09-10 (device and state code, harness, review fix cycle), the FH tag phase09-fh, the FR tag phase09-fr on 8dd6af1 and its freeze record, the rehearsals of 09-10
provides:
  - F2_PINS, the F2 identity as literals in one bound test table, compared with independent carriers by the case f2-identity-pins (13 same-setup controls)
  - ten real t64-claim blocks in docs/claims/phase09.md and the case claims-final-blocks (eight controls) replacing the skeleton assertion
  - the qualified commit d3211e9 (tag phase09-f2): a non-rehearsal native record, both hosted jobs green on the same commit, collect and verify in final mode
  - docs/evidence/phase09-qualification.md and the completed docs/evidence/phase09-success-criteria.md, bound to the SHA and the record digests
  - CI review rows for runs 44795894664 and 44801811117 and findings F-111 to F-114
affects: [phase-10, phase-completion-verification]

actuals:
  tokens: 36642   # chars/4 over git diff 081f5f2..c2a0957 (146,569 characters, 5 files, 1,409 insertions, 155 deletions)
  tasks: 6
  commits: 7      # MEASURED: git rev-list --count 081f5f2..c2a0957 at write time (the closeout commit follows)

plan_head_before: 081f5f221bcf81483fa9794b0e1f73ad87ec5682
plan_head_after: c2a09579f55532385b3ba696aef691d8c868fe89

tech-stack:
  added: []
  patterns:
    - "literal pin table with one same-setup control per pin class, each refused by its own comparator after an accepted baseline"
    - "claim blocks generated from the pins by a scratch script, statements rendered independently of scripts/check-claims.sh and compared with it"
    - "one candidate commit, qualification started in the background and the exact commit pushed at once, hosted run watched in parallel"

key-files:
  created:
    - docs/evidence/phase09-qualification.md
    - .planning/phases/09-controller-save-media-and-replay/09-11-SUMMARY.md
  modified:
    - scripts/tests/test_phase09_qualification.py
    - docs/claims/phase09.md
    - docs/evidence/phase09-success-criteria.md
    - docs/ci-reviews/LOG.md

key-decisions:
  - "Suite-level claim blocks bind the stem id only; the other family ids the state, scheduler and save suites exercise are listed in the evidence document and are not claimed"
  - "The macOS cold projection of 673 s (warm 487 s) is recorded as a deviation extending DEV-17 and is not repaired: no logic edit is allowed after the freeze and the pair is unchanged rehearsed code"
  - "repo.sched-k5-anchors is a supporting name of block 7, not counted: it is registered but no suite of the case inventory lists it"
  - "Block 3 states that no wrong-value row exists for the 16 Kbit ROM; the 4 Kbit wrong-value row is named and not counted there"

patterns-established:
  - "Post-Gate CI steps replayed locally before the push: all 11 run steps in 25 s; a replay with the same run id and prefix is refused by the retained-original rule, so remove only that prefix's own artifacts before a second replay"
  - "Use ${SHA}:refs/heads/main in zsh: $SHA:r is a history-style modifier that mangles the refspec"

requirements-completed: []   # SAVE-01..04, CORE-02, CORE-04, CORE-05 are left to the orchestrator's phase-complete step

coverage:
  - id: Q1
    description: "F2 identity literals agree with independent carriers; a changed literal in any pin class is refused by its own comparator"
    verification:
      - kind: unit
        ref: "scripts/tests/test_phase09_qualification.py --case f2-identity-pins (2.1 s, 13 controls)"
        status: pass
    human_judgment: false
  - id: Q2
    description: "Ten real claim blocks within the CONTEXT wording boundaries, counts reconciled with the retained originals"
    verification:
      - kind: unit
        ref: "--case claims-final-blocks (8 controls); sh scripts/check-claims.sh --files docs/claims/phase09.md (claims: files=1 blocks=10 errors=0); scratch reconciliation against the native JUnit and LastTest"
        status: pass
    human_judgment: false
  - id: Q3
    description: "One candidate commit qualified exactly: native normal gate, ASan and UBSan, gcc:13, full-matrix step, both hosted jobs, collect and verify in final mode"
    verification:
      - kind: other
        ref: "qualify-phase09.py --native (exit 0, 1,884 s), run 44795894664 attempt 1 (both jobs success), --collect-hosted and --verify-evidence printed the final pass line and no rehearsal statement"
        status: pass
    human_judgment: false
  - id: Q4
    description: "Warm and cold time against the 600-second limit"
    verification:
      - kind: other
        ref: "gate-linux warm 203 s cold 287 s; gate-macos warm 487 s cold 673 s"
        status: fail
    human_judgment: false
    rationale: "Warm is inside 540 s on both jobs; the macOS cold projection is 73 s over 600 s and is a recorded deviation (extension of DEV-17), not a gate failure of the pair"

duration: "about 1 h 25 min of executor time: tasks 1 to 3 about 55 min, native qualification 31 min and hosted run 8 min in parallel, tasks 5 and 6 about 25 min"
completed: 2026-11-20
status: complete
---

# Phase 09 Plan 11: F2 identity freeze, claims, one qualifying native record and hosted pair, evidence

**The F2 identity is pinned as literals with independent carriers and 13 controls, ten real claim blocks are in place with counts reconciled to the retained originals, and one candidate commit (`d3211e9`, tag `phase09-f2`) passed the non-rehearsal native qualification (normal gate 224 of 224, ASan and UBSan 226 of 226, gcc:13 over 64 units, full continuation matrix step) and both hosted jobs; the final-mode collector and verifier accepted the pair. One deviation is recorded: the macOS cold projection is 673 s against 600 s (warm 487 s).**

## What the orchestrator needs first

1. **Deviation (DEV-17 extension).** gate-macos ran 487 s warm (inside the 540 s limit), so its cold projection (warm plus the 186 s Binutils build delta, projected, never spent) is 673 s, 73 s over the 600 s job limit (619 s at the rehearsal). gate-linux is 203 s warm and 287 s cold. The identical source ran 349, 420, 433, 435 and 487 s on macOS (the docs-only run of D measured 435 s), so the spread is as large as the overrun. Both Binutils cache entries were checked fresh before the push and hit on both jobs; a cold macOS run needs a key change or an eviction and would be cancelled at the 10 minute limit. The plan text calls this a stop; your instruction for this dispatch said to record a deviation, so the plan ran to completion. The decision whether to act (levers measured, not tried: start `unit.core.state.import` first by CTest cost, drop the audio records step that re-runs names the Gate ran, raise `timeout-minutes` of gate-macos to 15) is yours; each is a bound-source edit that costs a new candidate cycle (LOG F-111, F-114).
2. **Verification and requirements are not done here.** No requirement was marked complete and phase completion was not run. `09-VERIFICATION.md`'s digest is stale as 09-10 stated; the confirmatory verification still has to run.
3. **Claim blocks:** ten, counts below. Final qualified SHA `d3211e957315afbb6bf83c91c49a111d0adbeb58`.

## Performance

- **Duration:** about 1 h 25 min of executor time. Native qualification 1,884 s from start to record (31 minutes), hosted run 44795894664 487 s (macOS), the hosted wait overlapped the native run.
- **Commits:** 7 measured from the ledger (see frontmatter), the closeout commit follows.
- **Files:** 5 changed (1,409 insertions, 155 deletions); one created evidence document.

## Final report block

| Item | Value |
| --- | --- |
| Final qualified SHA (X) | `d3211e957315afbb6bf83c91c49a111d0adbeb58`, local and pushed tag `phase09-f2` |
| FR (rehearsed commit, tag `phase09-fr`, pushed with the first push) | `8dd6af1cda44fe021fcc4b42c4103ffd5148c986` |
| Native record | `build/gate/09-11-phase09-private/native-d3211e957315afbb6bf83c91c49a111d0adbeb58/native-record.json`, SHA-256 `4310bc4c0667a1d26c03d3de7e0754225ee4853872b228f76a6484ebad6fa483`, `rehearsal: false`, status complete, empty absent-pending list, 288 bound inputs (manifest `bc627e75...0dca`) |
| Hosted record | `build/gate/09-11-phase09-private/hosted-d3211e957315-44795894664-1-1795202282515508000/hosted-record.json`, SHA-256 `a91c5d545a36ef95602cc477615034bbfa059b03f06d1eb8db9d21de26c5c97f` |
| Hosted run | 44795894664, attempt 1, source X, public URL `https://github.com/szTheory/tendies64/actions/runs/44795894664` |
| Artifacts | gate-linux id 14659700422, 10,883 bytes, `sha256:edaead89654dd5721101c0c62fcd3d37cab0d904e3771320e897e214cd18155b`; gate-macos id 14658583889, 10,885 bytes, `sha256:2381952de432c9db7f0659f1684f4f78ddbcd4bccc6a026057969460b2ae0cdc` |
| Public manifests (SHA-256) | Linux `a34e9839463c48cb4b16c4ec1505ae6fd3bf574196aa0eeddf1d8eb7562fdaa9`, macOS `724bde6495d44e93b73122859b94c562791e8d91bad1f255e5f2c11cd02fd16b` |
| Build identities | native normal `t64-build-v1-b58d1ab6...2de22` (claim build id, prefix `0.0.0+`); hosted core Linux `t64-build-v1-287b26a3...db7a`, macOS `t64-build-v1-2032f8e2...3691` (they differ by design) |
| F2 freeze and qualification verify | printed their final (non-rehearsal) statements: `qualify-phase09: complete native, ASan, GCC13, and both hosted platform records verified`, no rehearsal statement (zero matches of the word in the verifier output) |
| Evidence commit D | `928c9d8` (docs only); D's hosted run 44801811117 green on both jobs |

## Claim blocks (counted names)

| Block | Pair | Tier | Counted |
| ---: | --- | --- | --- |
| 1 | stem / `phase09-pad-poll` | unit-tested | 1 of 1: `phase09-pad-poll-si-v1` (completion tick 626,225) |
| 2 | `-e4k` / `phase09-eeprom-rw` | unit-tested | 1 of 1: `phase09-eeprom-rw-e4k-v1` (1,567,331) |
| 3 | `-e16k` / `phase09-eeprom-rw` | unit-tested | 1 of 1: `phase09-eeprom-rw-e16k-v1` (1,567,338) |
| 4 | `-s32k` / `phase09-sram-rw` | unit-tested | 1 of 1: `phase09-sram-rw-s32k-v1` (539,986) |
| 5 | `-s32k` / `phase09-save-roundtrip` | unit-tested | 1 of 1: `phase09-save-roundtrip-s32k` (540,634) |
| 6 | stem / `phase09-save-refusals` | unit-tested | 2 of 2: `unit.core.save.device`, `unit.core.save.import-export` |
| 7 | stem / `phase09-scheduler-order` | unit-tested | 4 of 4: `unit.core.sched.matrix`, `unit.core.sched.ledger`, `unit.core.sched.characterize-fifo`, `unit.core.profile.quantum-identity`; supporting not counted: `repo.sched-k5-anchors` (registered, listed by no suite of the inventory) |
| 8 | stem / `phase09-state-roundtrip` | unit-tested | 7 of 7: `unit.core.digest.sink-legacy-identity`, `unit.core.digest.v25-reference`, `unit.core.digest.v25-strip-differential`, `unit.core.state.export`, `unit.core.state.import`, `runner.state-export`, `harness.state-field-use-audit` |
| 9 | stem / `phase09-replay-mix-goldens` | regression-pinned | 1 of 1: `runner.replay-mix` |
| 10 | stem / `phase09-replay-format` | unit-tested | 1 of 1: `unit.replay.suite` |

Block 1 also names `unit.core.cpu.si-store-budget`, `unit.core.si.atomic-queue-full`, `unit.core.si.device`, `unit.core.input.latch-partition`, `unit.core.joybus.parser` and `controls.real.pad-wrong-value` as supporting tests. Block 6 names `controls.real.save-wrong-byte` and `runner.save-roundtrip` as supporting. All counted names were reconciled against the retained originals (every one a passed testcase in the retained normal JUnit and in the ASan JUnit; blocks 1 to 5 additionally by the case marker line of the owning LastTest section). `build_id` equals the native normal build (`t64-build-v1-b58d1ab6...`): no claim correction was needed. The wording limits of K20 and the FR record's open findings are in the prose and `not_claimed` lists (RDP state for the four family ids and authored fixtures only; 17 matrix rows with 4 full and 13 sampled and the full set only in the native step; resume mechanism only, never a runner pass; the publication contract; the first-pair state digests superseded; the step records unsealed; the gate-default campaign reaching reason 40 only; same-user swap not claimed).

## Task 1: freeze verification, cheap-first sweep, tracer

- **Preconditions:** tag `phase09-fr` on `8dd6af1` (local, not on origin), `phase09-fh` on `2065efc`; `git diff --quiet phase09-fr HEAD -- core runner tools scripts .github fixtures` exit 0; tree clean; `harness.rdp-oracle-authority` passes (0.6 s); caches checked (`gh cache list`: `binutils-macOS-ARM64-69d0bfd4...` 5.03 MiB and `binutils-Linux-X64-69d0bfd4...` 31.10 MiB, key equals sha256 over the raw sha256 of `scripts/build-boot-fixtures.sh`, last used 2026-11-20T18:11Z); the last CI head 8dd6af1 (run 44790405712) was green.
- **Freeze check** (`build/gate/09-11-scratch/freeze-check.py`, stdlib, `python3 -I -B`; output saved in `build/gate/09-11-freeze-fh.txt`): `--range FR..HEAD --product-zero` printed 0 production paths changed; `--frozen phase09-fh` classified 19 files: 11 identical, 2 DATA, 6 LOGIC all matching the declared set (fingerprints of the changed-name lists): FD-05 `scripts/qualify-phase09.py` (added `_state_import_checks`, `_verify_full_matrix_step`, `_verify_native_step`; changed `_native_command_plan`, `_native_qualify`, `_verify_native_record`, `_verify_rdp_native_receipt`; assignments `FULL_MATRIX_*` and the two shared stage limits), FD-06 `scripts/qualify-rdp-vi.py` (five limits and `_run_native`), FD-04 and FD-02 `scripts/check-rsp-no-hle.py` (`_check_source_route`; data `CANONICAL_CORE_SOURCES`, `REVIEWED_BODY_DIGESTS`, `REVIEWED_RDP_RASTER_FUNCTIONS`), FD-01 `scripts/tests/test_rsp_no_hle.py`, FD-03 `runner/t64_artifact_publisher.c` and `.h`. DATA hunks: `scripts/rights-scan.sh` (five selftest cases added beside existing ones, additions only) and `scripts/check-test-inventory.sh` (15 bare test-name rows). Identical: `scripts/gate.sh`, `scripts/check-ci-yml.sh`, `.github/workflows/ci.yml`, `scripts/collect-boot-evidence.sh`, `scripts/build-boot-fixtures.sh`, `scripts/check-phase09-cases.py`, `scripts/qualify-audio.py`, `scripts/check-audio-cases.py` and the three `tools/rights` files. `--frozen phase09-fr`: all 19 identical. `--controls`: against both tags a copy of the qualifier with one `==` flipped in `_host_platform` was rejected (`NOT DECLARED: changed:_host_platform`) and a copy of `scripts/gate.sh` with `-z` flipped to `-n` was rejected (`unclassified diff`), each after an accepted baseline of the real file in the same setup. Re-run after every task and on X with the same results.
- **Cheap-first sweep (seconds):** `ast.parse` of 60 tracked Python files; `pii-scan --tree` 0.3; `rights-scan --tree` 0.2; claims on the skeleton (blocks=0) and later on the tree; `check-ci-yml.sh` 0.02 and its selftest 1.9 (`cases=133 failures=0`); `check-test-inventory.sh --preset gate` 0.6 (count 224); `check-rsp-no-hle.py --build build/gate` 0.25; `qualify-phase09.py --selftest` 6.2.
- **Gate-name figure:** 45 Phase 9 names in `PHASE09_TESTS` (45 of the K13 budget), `ASAN_ONLY` holds only the two names of plan 02-08 (`fuzz.rom-admission`, `sanitizer.negative-control`), gate inventory 224, ASan 226, nothing added by this plan.
- **Qualifier pin constants:** none empty; the module holds two non-empty helper constants for `HOSTED_ROWS` (`_STEM_PROFILE_HASH`, `_BASELINE_INPUT`) and no inventory digest constant, so the qualifier is unchanged (no data edit).
- **Verifier-tail replay (F-098):** `harness.phase09-collect-real` passed (the cross-platform controls from the two committed real records). The final-mode `--verify-evidence` (no rehearsal flag) refused both retained rehearsal records with exit 1 and `refused original evidence: hosted record rehearsal label differs from the requested verification` (the 09-03.1 pair of run 44632478137 and the 09-10 pair of run 44790405712). Stated limit: the final collect and verify first met real artifacts of X in Task 5.
- **Tracer:** `runner.pad-poll`, `runner.phase09-admit`, `runner.profile-table-id-length` and `harness.phase09-collect-real` passed 4 of 4 (0.6 s); the retained `result.json` reported `build_id` `t64-build-v1-b58d1ab6...2de22` and the stem profile digest `661b161d...`; `test_phase09_runner --print-profile-hashes` printed the four profile hashes. The probe block (stem, `phase09-pad-poll`) was accepted (`claims: files=1 blocks=1 errors=0`); the three mutated copies were each refused with their own rule after the accepted baseline: a reserved word in `not_claimed` (C-RESERVED-WORD), one statement byte (C-STATEMENT), pass 2 of total 1 (C-COUNTS). The probe stayed in the ignored scratch directory.

## Task 2: F2 identity pins (RED, GREEN, values, controls)

- **RED (commit `f1a44f3`, unpushed at the time):** the case existed with only the four hand-typed ids and lengths. After proving the other 29 cases of the file still passed (30 ran, 1 failed, 5.5 s) the first failure was the `profile_sha256` class: `profile_sha256: pinned ids [] differ from the profile_ids table`, then `no pinned hash for <id>; the runner table holds [...]` for each id with the observed runner literals `661b161d...`, `e8dcb8dc...`, `22837f61...`, `52c47d60...`; the fixtures class listed all 15 manifest and runner rows as not pinned.
- **GREEN (commit `08ec831`):** the table was generated by a scratch script from independent sources and printed beside the carriers: profile hashes from `test_phase09_runner --print-profile-hashes` (equal to the four runner row literals; limit stated: they are regression pins of the production hash, the runner test `profile-table` ties them to `t64_profile_hash`); ROM SHA-256 and size of the 15 `phase09-` rows from `shasum -a 256` of each built ROM under `build/fixtures`, equal to the manifest and runner row in all 15 (15 of 15 ALL-EQUAL); transcripts: `--bytes`, `--sha256` and `shasum` over `--print` for pad (baseline, shifted, neutral, wrong-rom: 68 bytes each), save (eeprom-4k 191, eeprom-16k 192, eeprom-wrong-4k 191, sram-32k 101, sram-wrong-32k 101), roundtrip (baseline and swap16-as-raw 144) and replay-mix (72), each triple equal and equal to the runner log pin of every row that names it; the four admit transcripts by `printf` and `shasum` (20 bytes, `5fc4d0d3...`, `bd084623...`, `c8ba0710...`, `6a0845a8...`) equal to the runner pins; inputs: the retained baseline `input.json` hashed `e70838a9...d1ba` (equal to `HOSTED_ROWS`), the other three pad inputs and the replay input carried by `HOSTED_ROWS` and the file bytes (`mix.input.json` `e61dcd21...`, accepted by `mix_oracle.py --check-input`); golden header `runner/tests/replay_mix_golden.h` `3004debb...`; five oracle script digests; inventory digest `3f490463...` (also the snapshot the native record reports). No difference was found, nothing was adjusted.
- **Controls (13, each reached its own comparator after the accepted baseline of the same setup):** one hex digit changed in a profile hash (`profile_sha256: ... pinned 061b161d... but the runner table holds ...`), a fixture hash (`fixtures: ... pinned (...) but the manifest row holds ...`), a manifest row dropped (`fixtures: manifest row ... is not pinned`), an id added that is not a manifest row (`fixtures: pinned id phase09-added-row is not a manifest row`), a transcript hash (`transcripts: admit-0 pinned (20, 0fc4...) but the oracle prints ...`), a pad input and the replay input (`inputs: ... but the carrier holds ...`), the golden (`replay_golden: ...`), an oracle script digest (`oracle_sha256: ...`), the inventory digest (`inventory_sha256: ...`), one id dropped (`profile_ids: expected four distinct ids, got 3`), one length changed (`id_lengths: pinned (38, 42, 43, 42) but the ids measure (38, 42, 43, 43)`), and a counted name replaced by the registered name that no suite lists (`claim_counted: block 7 counts repo.sched-k5-anchors, which no suite of docs/spec/phase09-cases.json lists`). The case runs in 2.1 s.
- **Symbolic accessors:** none found. `grep -rn -i 'symbolic\|\bF2\b' core/tests runner/tests scripts/tests` printed only two comments in `core/tests/test_sched_matrix.c` ("no literal profile hash exists before F2") that describe the absence of a dependence and no test expression; summaries 09-01 to 09-09.1 name none. No test file was edited beyond the one sanctioned file.
- **Verify:** focused CTests `harness.phase09-qualification|runner.profile-table-id-length|gov.manifest.*` 6 of 6 (8.7 s); `--range FR..HEAD --allow scripts/tests/test_phase09_qualification.py` passed (planning and review-log paths allowed implicitly, see Deviations).

## Task 3: claims-final-blocks, claim blocks, candidate X

- **RED (commit `b6f435d`):** the case `claims-final-blocks` with the skeleton case removed from `CASE_NAMES` and its body deleted (the planned consequence of K11): `claims-final: 0 real blocks but 10 expected`, `6 example fence(s) remain`, `the pairs [] differ from the pinned pairs`.
- **GREEN (commit `d3211e9`, the candidate):** ten blocks generated from the pins by a scratch script, each statement rendered independently of `check-claims.sh`; `sh scripts/check-claims.sh --files docs/claims/phase09.md` printed `claims: files=1 blocks=10 errors=0`, the tree scan `files=68 blocks=14 errors=0`, the selftest `cases=49 failures=0`; no reserved word and no project-name line (the case checks the whole file).
- **Controls of claims-final-blocks (8, each after the accepted baseline):** a reserved word in a `not_claimed` (`claims-final: reserved word 'accurate' on line 82`, plus `C-RESERVED-WORD`), one statement byte (`block 2 statement differs from the template rendering`, plus `C-STATEMENT`), a duplicated pair (`a (profile id, fixture set) pair is duplicated`), tier smoke (`block 4 tier smoke is outside the pinned tiers`), pass different from total (`block 1 pass 0 differs from total 1`, plus `C-COUNTS`), total different from the counted list length (`block 1 total 2 differs from the counted list length 1`), a counted name no suite lists (`block 7 counts repo.sched-k5-anchors, which no suite of docs/spec/phase09-cases.json lists`), a block removed (`9 real blocks but 10 expected`). The case also pins oracle kind and revision from the F2 pins, the manifest digest, the closed key list, the `not_claimed` phrases (`other builds`, `other profiles and fixture sets`, `unexercised behavior`, `commercial software`) and that each counted name appears in the prose before its block.
- **The declared edit to the bound test file:** removed check `claim-skeleton-has-no-real-block`; new checks `f2-identity-pins` and `claims-final-blocks` (suite `f2`), each with the controls above. `git diff --name-only phase09-fr HEAD -- core runner tools scripts .github fixtures` lists exactly `scripts/tests/test_phase09_qualification.py`. No CTest name was added.
- **Verify:** `ast.parse`; `--suite f2` 2.7 s; claims tree and selftest; PII and rights scans; focused CTests `harness.phase09-qualification|gov.claims.repo|gov.claims.seeds|gov.rights.repo|gov.manifest.*` 8 of 8 (12.3 s with the build); freeze check `--product-zero` and `--frozen phase09-fh` as above; tree clean.

## Task 4: exact-SHA qualification

- **Cheap-first on X before anything heavy:** `ast.parse` 60 files, both scanners, claims tree, `check-ci-yml.sh` and selftest (`cases=133 failures=0`), inventory (224), the reviewed-source guard, the qualifier selftest, the freeze checks; `act -l -W .github/workflows/ci.yml --container-architecture linux/amd64` listed exactly `gate-linux` and `gate-macos`; the cache entries re-listed before the push. The Linux compile pre-flight and the gcc:13 closure over changed units were not run: no C or header file changed between FR and X (the FR..X diff over `core runner tools scripts .github fixtures` is one Python test file) and FR passed hosted Linux Gate.
- **Local replay of the post-Gate CI steps** (`build/gate/09-11-scratch/replay-ci-tail-full.py`, F-107): the three Phase 9 emitter steps with `GITHUB_JOB=gate-macos`, `GITHUB_SHA=X`, `GITHUB_RUN_ID=1`, `GITHUB_RUN_ATTEMPT=1` after a reconfigure (case run 1.4 s, emit 0.2 s, PII 0.1 s, exactly one public `manifest.json`, `rights-scan --staging build/evidence` 1 file 0 findings), then all 11 run steps after Gate in 25 s: audio case run 20.7 s, Phase 9 case run 1.8, five emitters, rights and PII scans. A second replay with the same run id and prefix was refused three times by the retained-original rule (the audio and Phase 9 emitters need exactly one retained log per prefix, and the audio private record destination must not exist); I removed only that prefix's own artifacts and private records and replayed once cleanly.
- **Native start and push:** the qualifying run was started in the background at 18:46:01Z (`python3 -B scripts/qualify-phase09.py --native --sha d3211e9... --out build/gate/09-11-phase09-private/native-d3211e9...`, no `--rehearsal`) and X plus the tag `phase09-fr` were pushed 12 seconds later (`git push origin "${SHA}:refs/heads/main" phase09-fr`; the first attempt with `"$SHA:refs/heads/main"` failed locally with a refspec error in zsh, nothing reached the remote). `git ls-remote --tags origin phase09-fr` then printed `8dd6af1...`. No tracked file changed and no untracked non-ignored file was created from the native start to the end of the verifier.
- **Hosted:** run 44795894664 (created 18:46:15Z) both jobs success, head X, attempt 1; Binutils cache hit on both.
- **Native record (exit 0 at 19:17:25Z, 1,884 s):** record SHA-256 above. Steps: shared native stage 1,816.5 s (normal gate 219.5, ASan configure 0.2, ASan build 11.1, ASan and UBSan full CTest 1,476.5, gcc13 closure over 64 units 54.0), `normal-state-import-full-matrix` 53.6 s, 26 inventory checks and 2 source-body checks 0.1 to 0.3 s each. The run was about 28 percent slower than the rehearsal on 8dd6af1 (stage 1,421.6 s): slowest sanitizer test `unit.core.state.import` 1,441 s against its TIMEOUT of 2,400 s (1.7 times), `runner.state-export` 226 s against 480 s; the limits (3,000 s wrapper, 4,800 s outer) held. Normal JUnit 224 testcases, 0 failures; ASan JUnit 226, 0 failures; matrix JUnit passed.
- **Candidate and attempt history:** one candidate (X), no failed attempt, no second candidate; the push was exactly X.

## Task 5: collect, verify, timing, log

- **Commands (outputs in `build/gate/09-11-collect.out` and `build/gate/09-11-verify.out`):** `python3 -B scripts/qualify-phase09.py --collect-hosted --repo szTheory/tendies64 --sha X --run 44795894664 --attempt 1 --native-record NATIVE --out build/gate/09-11-public-d3211e957315` (10.2 s wall, collector process 5 s, exit 0; printed `qualify-phase09: exact hosted artifacts collected; private originals retained at .../hosted-record.json`), then `--verify-evidence` with HEAD equal to X and a clean tree (4.5 s, exit 0): `qualify-phase09: complete native, ASan, GCC13, and both hosted platform records verified` and no other line. The tag `phase09-f2` was created on X afterwards. No tail defect met real data; the two platform manifests agree on every row (state digest, guest tick) and inventory field (`snapshot_sha256` `3f490463...`) while the build identities differ.
- **Timing table** (`gh api .../runs/44795894664/attempts/1/jobs`):

| | gate-linux | gate-macos |
| --- | ---: | ---: |
| queue / run (warm) | 1 s / 203 s | 2 s / 487 s |
| Gate step | 159 s (ctest 101, test-inventory-selftest 13, build 19, core-lint 12, boot-evidence-selftest 9) | 410 s (ctest 251, test-inventory-selftest 47, boot-evidence-selftest 35, build 30, core-lint 26, lint-selftest 11) |
| audio qualification records | 30 s | 42 s |
| Phase 9 case run, emit, PII scan | 2, 0, 0 s | 5, 0, 1 s |
| upload | 2 s | 1 s |
| Binutils cache | hit | hit |
| increment over F-100 (129 s, 275 s) | +74 s | +212 s |
| rehearsal of 09-10 (warm) | 242 s | 433 s |
| warm at most 540 s | pass | pass (53 s margin) |
| cold projection (warm + 84 s or 186 s) at most 600 s | 287 s pass | **673 s, 73 s over** |

The plan's second verify block (`awk` over the jobs) printed `gate-linux warm=203 cold_projection=287`, `gate-macos warm=487 cold_projection=673` and exited 1. A per-job difference above 30 s against the rehearsal is a finding: macOS +54 s and Linux -39 s on essentially identical code (macOS Gate 361 to 410 s, ctest wall 218 to 251 s, `unit.core.state.import` 190 to 210 s, `harness.state-field-use-audit` 65 to 71 s, `harness.phase09-qualification` 19 to 35 s of which about 6 s are the two new cases at the measured hosted ratio of 2.8 times native); logged in F-111. The duplicated guard run (F-097) costs the Phase 9 case step 2 s (Linux) and 5 s (macOS).
- **Log:** `scripts/ci-review.sh 44795894664 --append docs/ci-reviews/LOG.md --finding ...` appended the Runs row and F-111 (timing, cache, spread, levers), F-112 (final-mode verifier tail and native cost), F-113 (increment, duplicated guard, local replay); one `[skip ci]` commit `8be91d8` touching only the log. A second commit `c2a0957` (also `[skip ci]`, log only) appended run 44801811117 and F-114 (the docs-only run spread: macOS 435 s, Linux 305 s on identical source). No verifier command was run after the log commit.

## Task 6: evidence documents and commit D

- **Claim reconciliation:** the scratch reconciliation (`build/gate/09-11-scratch/reconcile-claims.py`) read the retained originals: `build_id` of every block equals the native normal build; blocks 1 to 5 owner sections of the normal LastTest log show `Test Passed.` and the case marker lines with the completion ticks above; every CTest name of blocks 6 to 10 is a passed testcase in the normal JUnit and in the ASan JUnit with its listed seconds (for example `unit.core.state.import` 69.1 s normal and 1,441.2 s ASan); result `reconcile: OK`. No claim edit was needed.
- **Documents:** `docs/evidence/phase09-qualification.md` (321 lines: native record and step table, hosted record and build identities, timing, F2 identity tables, claims index, attempt and deviation history, review and security references, family ids not claimed, limitations and preserved stops, reproduction) and the completed `docs/evidence/phase09-success-criteria.md`. Success criteria map changes, old to new: the draft banner and the pre-F2 status paragraph replaced by a status and bindings paragraph (SHA, tag, native and hosted record digests, run, attempt); the checker description now names the 09-11 scratch copy and its two added checks; the column heading `Planned claim blocks` became `Claim blocks`; each of the four Pass evidence cells changed from `pending 09-11: filled from the retained native originals` to the list of passed tests plus the claim blocks and counts; the section `Planned claim blocks` became `Claim blocks` with the counts; `Limits carried into the independent review` became `Limits` with one sentence reworded. No named test, source, control or claim pair of any row changed.
- **Checks:** `check-criteria-map.py` (copy of the 09-10 checker extended with: named tests passed in the normal and ASan JUnit, the pass-evidence cell bound to the SHA and naming every test, claim pairs read from `docs/claims/phase09.md`) printed `criteria map ok: rows=4 named_ctest=23 claim_pairs=10`; four mutated drafts (wrong SHA, unregistered name, missing claim pair, pending marker) were each rejected with their own `MAP` line. `sh scripts/pii-scan.sh --tree`, `sh scripts/rights-scan.sh --tree`, `sh scripts/check-claims.sh` (tree and the Phase 9 file), `sh scripts/check-docs.sh` and the 13 `gov.*` CTests plus the two qualification CTests passed; a scratch copy of the claim file with a reserved word in a `not_claimed` was refused with C-RESERVED-WORD; all 60 distinct digests of the two documents were found in the native record, hosted record, collector output, verifier output, run file or `F2_PINS`, except the hosted record's own SHA-256, recomputed with `shasum -a 256` (it cannot appear inside itself).
- **Commit D `928c9d8` (docs only):** `git diff --name-only phase09-f2..HEAD` listed only `docs/ci-reviews/LOG.md`, `docs/evidence/phase09-qualification.md` and `docs/evidence/phase09-success-criteria.md`; freeze check `--allow` passed; the tag and branch were pushed (`git push origin main phase09-f2`). D's hosted run 44801811117 succeeded on both jobs (gate-macos 435 s, gate-linux 305 s); its Runs row and F-114 are in the log.

## Deviations from Plan

1. **[Plan arithmetic, recorded] macOS cold projection 673 s over 600 s.** See the first section. The pair itself passed; this is an extension of the accepted DEV-17, not repaired and reported. No logic edit was possible or made.
2. **[Plan wording] Freeze check allow mode.** The plan's `--range FR..HEAD --allow <test file>` over all paths would fail on the orchestrator's `.planning/` commits and the review-log commits that follow FR; the helper treats paths under `.planning/` and `docs/ci-reviews/LOG.md` as implicitly allowed (the FR record's data-only list) and prints them separately. Product-zero is exact.
3. **[Plan wording] Public output path.** `--out build/gate/09-11-phase09-private/public-SHA` is refused by the carried-over rule (public output stays outside the private store); `build/gate/09-11-public-d3211e957315` was used, as in 09-03.1 and 09-10.
4. **[Plan wording] Verify block 2 of Task 5** exits 1 on the macOS line by design under the deviation above; block 1 `test -d "$NATIVE"` style checks were run as file tests; the plan's push command needs `${SHA}` in zsh.
5. **[Declared plan edit] Bound test file.** One sanctioned file edited before the qualifying run: `claim-skeleton-has-no-real-block` replaced by `claims-final-blocks`, and `f2-identity-pins` added; the qualifier, ci.yml, scripts and every other frozen file are unchanged (the qualifier declares no empty pin constant to fill).
6. **[Skipped by reasoning] Linux compile pre-flight and gcc:13 closure over changed units** before the native run: no C or header file differs between FR and X.

**Total deviations:** 1 recorded timing deviation, 1 declared edit, 4 wording items, 1 reasoned skip; no guard weakened; no frozen file touched; no CTest name added.

## Auth gates

None (`gh` was already authenticated).

## Known Stubs

None. No placeholder text or empty value flows to a consumer in the files created or modified.

## Threat Flags

None new beyond the plan's register (T-09-83 to T-09-86): the new surface is the documents and the test cases only. Mitigations as built: literal pins with controls and the freeze check with rejected mutants (T-09-83); documents drafted in the ignored scratch directory and scanned before commit, digests, sizes, counts and public run links only, nothing private uploaded (T-09-84); only the two tiers, counts reconciled to the retained originals, byte-exact statements checked in every gate run, reserved words absent (T-09-85); final mode run without the rehearsal flag against the exact SHA, run, attempt and artifact ids, the retained rehearsal records refused first, the 600 s limit measured and its overrun recorded (T-09-86).

## Handoff for the orchestrator

1. **Confirmatory verification scope** (no review, no audit): the Phase 9 goal and success criteria 1 to 4 and the seven requirement ids against `docs/evidence/phase09-success-criteria.md`; `docs/claims/phase09.md` (ten blocks) and `docs/evidence/phase09-qualification.md`; the tested source at the tag `phase09-f2` (commit X); the inputs `09-REVIEW.md`, `09-SECURITY.md`, `09-VERIFICATION.md`, `09-REVIEW-FIX.md`, `09-10-SUMMARY.md` and this summary; the private records under `build/gate/09-11-phase09-private` (`native-d3211e957315afbb6bf83c91c49a111d0adbeb58`, `hosted-d3211e957315-44795894664-1-1795202282515508000`, `09-11-run.env`) to reproduce the exact-binding verification on a detached checkout of `phase09-f2`. The covered digest should differ from 09-10's by exactly the files this plan edited (the test file, the claim blocks, the two evidence documents and the review log). Probes belong in a scratch copy or a detached worktree.
2. **Bookkeeping left to phase complete:** `.planning/REQUIREMENTS.md` (SAVE-01 by block 1; SAVE-02 and SAVE-04 by blocks 2 to 6; SAVE-03 by blocks 5 and 6; CORE-02 by block 7; CORE-04 by block 8; CORE-05 by blocks 9 and 10, each with its tier), `.planning/ROADMAP.md` (this plan's progress row was updated through the SDK; the phase checkbox is yours), `.planning/STATE.md` follow-ups: depth stays inactive on descriptor 12 (K1); the per-id state claims for the three save ids are not made; the macOS cold margin decision (DEV-17 extension); the CI findings F-111 to F-114; the FR record, the FH tag and `phase09-f2`.
3. **Owner metadata:** `AGENTS.md`, `.planning/config.json` and `.planning/AUTONOMY.md` may be edited again now (the record binds their content at X; the reproduction checks out the tag detached).
4. **Open items carried in the claim wording:** the FR record's non-blocking findings (R1 RC-06, R2 RC-06, R3 RC-04, L4 IN-04 and IN-05, security F-1 to F-8) are reflected in the prose and `not_claimed` lists or the limitations section; none is closed here.
5. **Margins to watch:** `unit.core.state.import` under the sanitizer 1,441 s of 2,400 s (1.7 times, slower than the 1,150 to 1,257 s of the rehearsals, host load dependent), `runner.state-export` 226 s of 480 s, hosted macOS `unit.core.state.import` 186 to 210 s and `harness.state-field-use-audit` 61 to 71 s of 240 s.

## Self-Check: PASSED

- Created and modified files exist: `docs/evidence/phase09-qualification.md`, `docs/evidence/phase09-success-criteria.md`, `docs/claims/phase09.md`, `docs/ci-reviews/LOG.md`, `scripts/tests/test_phase09_qualification.py`, and this summary.
- Commits are ancestors of HEAD: `f1a44f3`, `08ec831`, `b6f435d`, `d3211e9`, `8be91d8`, `928c9d8`, `c2a0957`; the tag `phase09-f2` resolves to `d3211e957315afbb6bf83c91c49a111d0adbeb58`, the tag `phase09-fr` to `8dd6af1cda44fe021fcc4b42c4103ffd5148c986`, both on origin.
- No `__pycache__` directory outside `build`, no tracked file of the bound set changed after X, no ROM, save, state or replay file tracked (`git ls-files -- '*.eep' '*.sra' '*.sav' '*.sta' '*.rpl' '*.rpr'` prints nothing), PII and rights scans clean.
