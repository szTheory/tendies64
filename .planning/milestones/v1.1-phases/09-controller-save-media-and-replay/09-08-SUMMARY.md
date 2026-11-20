---
phase: 09-controller-save-media-and-replay
plan: "08"
subsystem: state-export
tags: [core-04, t64-state-container, size-then-copy-out, section-recorder, observer-continuity, independent-decoders, runner-state-export, artifact-publisher, fsize-k9, mutation-controls]

requires:
  - phase: 09-controller-save-media-and-replay
    provides: "09-01 the digest sink seam and the v25 walker; 09-02 the private no-clobber artifact publisher, the artifact pair with the exact FSIZE rule, the checked child-argument helper and the admit ROMs; 09-03 to 09-05 sections 10 and 11; 09-06 the F1 state content freeze; 09-07 the replay flag slot-sum precedent"
provides:
  - "core/t64_state.c: t64_machine_state_export (size query then copy-out, planning pass plus emit pass over the one canonical walker), t64_state_container_length (checked 64-bit size arithmetic) and the versioned container with the observer-continuity section outside the digest"
  - "t64_digest_sink section recorder (section_ids, section_offsets, section_count, section_overflow) fed by the single v25 marker writer; no byte of any stream changes"
  - "an independent literal-offset reference decoder in core/tests/test_state_export.c and an independent stdlib decoder fixtures/state/decode_container.py with twelve refusal kinds"
  - "runner --state-export and --state-export-format through the 09-02 artifact publisher at authored completion, the closed state-export-* detail vocabulary, one-artifact-producer refusals, and real-runner state-size and file-size (K9) controls"
  - "the isolated N1 roster row for core/t64_state.c with hidden-source and missing-source controls"
  - "docs/spec/state-container.md and the state-export inventory suite (unit.core.state.export, runner.state-export)"
affects: [09-09, 09-09.1, 09-10, 09-11]

actuals:
  tokens: 45307
  tasks: 5
  commits: 12
plan_head_before: cec0446e1e78dad5d20053266cddd43e29ed48de
plan_head_after: 474fd97b9a534fe46493b69c6dc0f1526bbe9c39

tech-stack:
  added: []
  patterns:
    - "plan then emit over one walker: a counting sink with a section recorder fixes every offset before a single byte is written, the emit pass checks the stream against the plan"
    - "all non-bulk writes through three bounds-checked put helpers that fail closed; a defect clears the output and returns BAD_STORAGE instead of publishing a partial container"
    - "two independent decoders (C with literal offsets, Python stdlib with its own table) plus decoder surgery controls that must each reach their own failure kind"
    - "out-of-tree mutants: throwaway worktree, unmutated baseline first, one textual edit, -Werror compile of the production unit, named comparator required"
    - "record shape derived from a real record before the feature exists (RED), replaced by the real record once it does"

key-files:
  created:
    - core/t64_state.c
    - core/tests/test_state_export.c
    - fixtures/state/decode_container.py
    - runner/tests/test_state_runner.c
    - docs/spec/state-container.md
  modified:
    - core/include/tendies64/t64_machine.h
    - core/t64_internal.h
    - core/t64_digest.c
    - core/CMakeLists.txt
    - runner/t64_real.c
    - runner/t64_supervisor.c
    - runner/t64_result_check.c
    - runner/t64_real_result.h
    - runner/CMakeLists.txt
    - scripts/check-rsp-no-hle.py
    - scripts/tests/test_rsp_no_hle.py
    - scripts/check-test-inventory.sh
    - docs/spec/profile-membership-sites.txt
    - docs/spec/phase09-cases.json
    - docs/spec/result-protocol.md

key-decisions:
  - "The observer body is exactly five u64 values (output drain cursor, video head and count, audio head and count); write_generation never travels in the container and no export byte reads it, so 09-09 owns its rule"
  - "Export is defined for state serialization version 25 only; a legacy profile is INVALID_ARGUMENT and a pre-admission machine NOT_READY, both with every output unchanged"
  - "The runner opens the .sta publisher before the guest runs and maps every open failure to state-export-open; at authored completion only the outcome, the stop kind and the detail of the guest record are replaced, so no t64.result key is added"
  - "The result detail field grows from 32 to 48 bytes because the registered name state-export-incomplete-terminal is 32 characters"
  - "The artifact preflight stays first: a nonexistent directory is the existing prelaunch refusal, not state-export-open"

requirements-completed: []

coverage:
  - id: E1
    description: "One authored stem-id machine exports by size query then copy-out to a container whose every field is confirmed by an independent literal-offset decoder against values read through the public API"
    requirement: "CORE-04"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.state\\.export$' (case export-layout)"
        status: pass
    human_judgment: false
  - id: E2
    description: "The declared canonical digest equals t64_machine_digest and the SHA-256 of the canonical region on five authored states and the four ids; no digest literal changed"
    requirement: "CORE-04"
    verification:
      - kind: unit
        ref: "unit.core.state.export (case states-and-ids) with unit.core.digest.* unchanged"
        status: pass
    human_judgment: false
  - id: E3
    description: "Export is non-interfering and idempotent and keeps the size-then-copy-out convention; every refusal leaves bytes, length and digest unchanged; size arithmetic refuses the ceiling, UINT64_MAX and the section limits"
    requirement: "CORE-04"
    verification:
      - kind: unit
        ref: "unit.core.state.export (cases non-interference, idempotence-shape, refusals, size-arithmetic, observer-continuity)"
        status: pass
    human_judgment: false
  - id: E4
    description: "Each decoder surgery reaches its own failure kind and six non-equivalent mutants each fail their named comparator"
    requirement: "CORE-04"
    verification:
      - kind: unit
        ref: "unit.core.state.export (case decoder-controls) and the out-of-tree mutant record in this summary"
        status: pass
    human_judgment: false
  - id: E5
    description: "The real supervised runner publishes a private 0600 blob larger than the default child limit at authored completion, refuses one byte below the size with state-export-cap, survives the exact file-size fault at the state size and is signalled one byte over; an independent Python decoder confirms the real blob and refuses mutated copies"
    requirement: "CORE-04"
    verification:
      - kind: integration
        ref: "ctest -R '^runner\\.state-export$' (cases export-accepted, export-size-cap, export-fsize-at-state-size, export-decode)"
        status: pass
    human_judgment: false
  - id: E6
    description: "State export is refused with raw capture, with every save and replay flag and in synthetic mode, and allowed with the input triple; the closed state-export-* vocabulary is accepted only as a host-error harness-error"
    requirement: "CORE-04"
    verification:
      - kind: integration
        ref: "runner.state-export (cases export-refusals, export-nonterminal, checker-vocabulary)"
        status: pass
    human_judgment: false
  - id: E7
    description: "The guard roster gains core/t64_state.c in its own commit (numstat 1 0) and the controls still reject a hidden or missing state source"
    verification:
      - kind: unit
        ref: "python3 -B scripts/check-rsp-no-hle.py --build build/gate; python3 -B scripts/tests/test_rsp_no_hle.py; ctest -R '^repo\\.rsp-no-hle$'"
        status: pass
    human_judgment: false

duration: 40min
completed: 2026-11-20
status: complete
---

# Phase 9 Plan 08: State Container Export Summary

**A versioned 6.35 MB state container is exported by size query then copy-out from the canonical walker (planning pass plus emit pass, observer cursors outside the digest), confirmed field by field by two independent decoders, published as a private 0600 file by the real supervised runner under the raised file-size limit, and guarded by six mutants and an isolated reviewed-source row.**

## Performance

- **Duration:** about 40 min (2026-11-20 02:09Z to 02:49Z) for five tasks.
- **Commits:** 12 measured from the ledger (`cec0446..474fd97`, before this summary).
- **Estimate:** 210,000 tokens (low confidence); actual 45,307 (chars/4 over the added lines of the diff).

## OPEN ISSUES FOR THE ORCHESTRATOR (read first)

1. **CORE-04 is only half done and was not marked complete.** The plan lists `requirements: [CORE-04]`, but this plan is the export half; import and the boundary matrix are 09-09. `requirements-completed` is therefore empty and no REQUIREMENTS.md checkbox was changed.
2. **The plan's state-size arithmetic was wrong** (see Deviation 1): the real exported size is 6,351,511 to 6,384,399 bytes, not 8.4 to 11.5 MiB. The 09-09 and later plans that quote "9 to 11 MiB" or the bounds 8,400,916 and 11,534,336 should use 6,303,756 (strictly above) and 11,534,336 (strictly below). Nothing else depends on the figure.
3. **`runner/t64_real_result.h` changed** (detail field 32 to 48 bytes, Deviation 7). `t64_real_result.c` and the result schema keys are unchanged. If the 09-10 drift sweep or 09-11 pins list the header, record this one-line change.
4. **`docs/spec/result-protocol.md` still omits the raw-capture detail names** (the older gap flagged in the plan): this plan added the six `state-export-*` names beside the nine `artifact-*` names and left the raw-capture gap for 09-10.
5. **Not pushed.** The plan forbids a push. `repo.rsp-no-hle` was red only between `be6a8d4` and `a5045d0` as planned; HEAD is green. `harness.rdp-oracle-authority` was not re-run because this plan changes no `fixtures/manifest.json` row.
6. K13: cumulative Phase 9 gate names are 43 of about 45 (41 through 09-07 plus `unit.core.state.export` and `runner.state-export`); two remain for 09-09 to 09-11.

## Commits

| Hash | Message |
| --- | --- |
| `be6a8d4` | test: RED scaffold, stub export, constants, literal-offset decoder, case export-layout |
| `caa81cc` | feat: section recorder on the digest sink (no stream byte changes) |
| `e34740e` | feat: export implementation, membership list +2 sites |
| `a5045d0` | chore: isolated roster row for `core/t64_state.c` (numstat `1 0`) with two controls |
| `5ce5b84` | test: breadth cases, surgery controls, observer, non-interference, refusals, size arithmetic |
| `2efbd6b` | test: export-layout reads decoder fields only for an accepted blob (found by mutant m6) |
| `97d00e2` | test: RED checker-vocabulary and the `runner.state-export` registration |
| `ad1a9f6` | feat: closed state-export detail vocabulary at the three checker sites |
| `79596ea` | test: RED runner cases before the flags exist |
| `929294f` | feat: runner flags, validation, forwarding, publisher open, export at authored completion |
| `37a15ec` | test: Python decoder, decode comparisons, export-decode, real cap record |
| `474fd97` | docs: state container specification, result-protocol names, inventory suite |

## Accomplishments

- **Core.** `t64_machine_state_export(const t64_machine *, uint8_t *, size_t, size_t *)` plans over the walker with a counting sink (the section recorder yields the canonical length and each marker offset), refuses a recorder overflow, a build id of 128 bytes or longer and a total above 64 MiB, then streams the same bytes into the caller buffer while hashing them, and writes the observer body, section hashes, table, header and trailer through three bounds-checked put helpers. No allocation, no host call, no mutable static, no frame over 2,048 bytes (gate build under `-Werror`, the gcc:13 closure and the Ubuntu cc pre-flight are clean).
- **Layout.** Header 0x120, 56-byte table entries, canonical region equal to the v25 digest preimage, 40-byte observer body, 32-byte trailer; the declared digest equals `t64_machine_digest`. The stem id carries ids 1 to 10 and 16, the three save ids 1 to 11 and 16.
- **Independent evidence.** The C decoder reads literal offsets only and returns one of fourteen kinds; the Python decoder has its own table and twelve refusal kinds. Both agree with the spec table (all 14 header rows compared programmatically against `--print-layout` and the C constants).
- **Runner.** `--state-export LEAF` with `--state-export-format t64.state-container.v1` through the artifact publisher: both required together, artifact pair required, refused with raw capture, synthetic mode and every save and replay flag, allowed with the input triple; slot sum 39 to 43 (44 with the terminating NULL, capacity 64).

## RED records (before the implementation commits)

| Step | Observation |
| --- | --- |
| Task 1 step 2 (`be6a8d4`) | size query returned status 1 (INVALID_ARGUMENT) where 9 (BUFFER_TOO_SMALL) is required and the length stayed 0: `CHECK_INT failed: status got 1 want 9`, `checks=11 failures=2` (`build/gate/09-08-t1-red.LastTest.log`) |
| Task 1 guard | `repo.rsp-no-hle` red with exactly the t64_state.c findings (five FAIL lines: unreviewed source, unreviewed ninja object edge, unreviewed archive object, hidden object file, archive member inventory) and no `executable body changed` line, until `a5045d0` |
| Task 2 | before the row the test script's own guard run failed with those five lines; an isolated run of the new seeds raised `the source-only fixture has no core/t64_state.c to delete` |
| Task 4 step 2 (`97d00e2`) | the accepted cap-shaped record was refused with `R-ENUM ... outcome or detail is outside the closed v2 enum` (test line 428), and the outcome-pass variant also reported R-ENUM where R-DETAIL-OUTCOME is required (line 441) |
| Task 4 step 3 (`79596ea`) | with `--state-export` unknown the accepted run reported `harness-error`, detail `usage`, stop `not-started`, guest tick 0 (exit 2), no file, where `pass` and one published file are required |

Task 3's breadth cases started green by design (the Task 1 implementation was complete); their proof is the mutant record below, and the one behavioral failure it exposed was a test defect (Deviation 5).

## Measured state sizes (bytes)

| State | Size |
| --- | --- |
| stem id, idle (tick 64) | 6,351,551 |
| stem id, AI mid-DMA / first packet | 6,351,643 / 6,351,631 |
| stem id, VI retained packet / RDP pending | 6,351,607 / 6,351,591 |
| `e4k` / `e16k` / `s32k`, idle | 6,352,143 / 6,353,679 / 6,384,399 |
| authored admit ROM at authored completion (guest tick 538,781) | **6,351,511** (S) |

Idle stem section lengths: 168, 32, 4,194,308, 8,200, 276, 56, 43,724, 2,103,016, 336, 404 for ids 1 to 10 and 40 for id 16 (the id 8 body holds one 2 MiB coverage array, not two); the save ids add 532, 2,068 and 32,788 for id 11. S exceeds the default child limit of 69,632 and fits the raised limit (artifact maximum plus 65,536). The observer values of the real blob are `20,0,0,0,0`: the runner's output drain cursor equals the record's 20 log bytes.

## Out-of-tree mutant results (nothing committed, worktree removed)

Driver: a throwaway `git worktree add --detach build/09-08-mutants` at `5ce5b84`; for each mutant the unmutated source was compiled, linked and run first (accepted baseline, pass), then one anchored textual edit of `core/t64_state.c` was compiled with the gate ninja command (`-Werror`, `-Wframe-larger-than=2048`, 0.1 s), linked with the main-tree libraries and test object, and the named case run. `git worktree remove --force` followed; `git worktree list` shows the main tree only.

| Mutant | Edit | Case and observed comparator |
| --- | --- | --- |
| m1 | table body offsets of canonical entries point at the marker (offset minus 4) | `export-layout`: decoder kind `marker` |
| m2 | section hash computed from the marker start | `export-layout`: decoder kind `section-hash` |
| m3 | the declared digest also hashes the drain cursor (observer inside the digest) | `export-layout`: decoder kind `canonical-digest` and declared digest not equal to the machine digest; `observer-continuity`: decoder `canonical-digest` and the declared-equality check fail |
| m4 | trailer over total minus 33 bytes | `export-layout`: decoder kind `trailer` |
| m5 | export zeroes the drain cursor through a const cast via `uintptr_t` | `non-interference`: whole-structure snapshot mismatch (twice) |
| m6 | the size query reports the total minus the 40-byte observer and accepts that capacity | `export-layout`: canary `buffer[required + k] got 0 want 195` |

## Decoder surgery controls (case `decoder-controls`, accepted baseline first)

Body byte `section-hash`; length plus one `section-range`; swapped ids `table-order`; header flags `flags`; trailer byte `trailer`; total plus one `total-length`; marker byte `marker`; observer flags cleared `observer`; body flipped with the entry hash and trailer recomputed `canonical-digest`; plus magic, container version, section count 0 and 33, header tick and profile digest. A body flipped with every hash, the declared digest and the trailer recomputed is **accepted** by the decoder, recorded as the integrity-not-authentication limit (the declared digest then differs from the machine digest).

## Task 1 step 1 findings (sibling outputs, recorded as the plan requires)

- `t64_digest_sink` fields before this plan: `buffer`, `capacity`, `offset`, `hash`, `spans`, `span_count`, `span_capacity`, `overflow`. Every marker write already went through one function (`section_marker` in `core/t64_digest.c`); the recorder fields were added to the sink and fed from that function. All seven existing sink declarations zero-initialize with `memset`.
- Sections emitted: stem id 1 to 10; the three save ids 1 to 11 (section 11 exists only when `profile.save_kind != 0`, 09-04). Literal id lists are in the test and the spec.
- Runner flags after 09-05 and 09-07: save `--save-import`, `--save-import-size`, `--save-import-sha256`, `--save-kind`, `--save-format`, `--save-export-leaf` (child: `--save-import-fd`, `--save-kind`, `--save-format`, `--save-export-leaf`); replay `--replay-record`, `--replay-checkpoint-interval`, `--replay-play`, `--replay-play-size`, `--replay-play-sha256`, `--replay-report` (child: `--replay-record`, `--replay-checkpoint-interval`, `--replay-fd`, `--replay-report`). Slot sum before 39, after 43.
- **Observer-field review.** The host observer fields in `struct t64_machine` are `output_drain_cursor`, `vi.packet_head`, `vi.packet_count`, `ai.packet_head`, `ai.packet_count` (the five observer values) and `save.write_generation` (observer only, declared by 09-05 and never travelling). No other host cursor or occupancy comment exists in `core/t64_internal.h`, so `T64_STATE_OBSERVER_BYTES` stays 40 and no deviation to the layout was needed.

## Task 2 roster record

Pin: `CANONICAL_CORE_SOURCES` in `scripts/check-rsp-no-hle.py`. Old content: the 34-row roster at the plan base (including the 09-03 and 09-04 rows `t64_si.c`, `t64_joybus.c` and `t64_save.c`). New row: `"t64_state.c"` between `t64_sp.c` and `t64_trace.c`. Causing change: the new core source of Task 1. Control: seeds `hidden state source` (a `t64_state_unreviewed.c`, finding `core: unreviewed t64_*.c production source(s): t64_state_unreviewed.c`) and `missing state source` (finding `core: canonical production source(s) missing: t64_state.c`), each after an accepted baseline in the same setup, plus `t64_state.c` in the source-name deletion loop (deleting the row is reported naming the source). `core/t64_digest.c` has no `REVIEWED_BODY_DIGESTS` row, which is why the recorder edit needed no digest refresh. Numstat of the commit for the guard file: `1 0`.

## Checks and measured seconds

| Block | Result | Seconds |
| --- | --- | --- |
| Preflight: gate build (cached), `check-rsp-no-hle.py`, inventory, membership check | all pass | 3.0, 0.2, 0.8 |
| Task 1 verify block 1 (configure, build, 8 names, lint, inventory) | 8 of 8, lint `violations=0` | 14.7 |
| Task 1 gcc:13 `-Werror` closure over three units (twice, final tree) | compiled, no warning | 3.4 |
| Linux pre-flight (Ubuntu 24.04 cc 13.3.0 from the act image, `-Werror`, eight units incl. runner and test twins; amd64 image emulated on an arm64 host) | all compiled | 3.0 (3 units), 7.6 (8 units) |
| Task 2 guard + `test_rsp_no_hle.py` | pass, `sources_scanned=85` | 13.3 |
| Task 2 `ctest -R repo.rsp-no-hle` | pass | 12.5 |
| Task 3 verify block 1 (4 names + guard + inventory) | 4 of 4 | 3.7 |
| `unit.core.state.export` native / ASan | pass | 2.7 to 3.1 / 47.0 to 49.9 |
| Six mutants plus baselines | all fail their comparator | 4.2 |
| Task 4 regression group (33 legacy names plus the new name) | 34 of 34 | 4.8 |
| `runner.state-export` native / ASan (alone in a three-name batch 52.0; five-name batch wall 56.7) | pass | 2.2 to 2.6 / 52 to 57 |
| ASan build (cached tree, test and runner units rebuilt) | no error | 4.9 (Task 3), not separately timed (Task 4) |
| Task 5 case checker, suite `state-export` | validated, nonempty | 11.7 |
| Case checker over all 11 Phase 9 suites (sweep) | validated, nonempty | about 24 of ctest time in total |
| `repo.docs` and the four gov names (6 tests) | 6 of 6 | 8.4 |
| ASan selection of five names | 5 of 5, guard clean, inventory asan 224 | 56.7 wall |
| `check-test-inventory.sh --selftest` | PASS `removal_controls=446 malformed_controls=5` | 21.9 |
| inventory gate / asan | PASS counts 222 / 224 | under 1 |
| `core-lint.sh` | `files=98 violations=0` | not timed (14.1 in 09-07) |
| `check-claims.sh --files docs/claims/phase09.md` | `claims: files=1 blocks=0 errors=0` | 0.02 |

The full `sh scripts/gate.sh` was not run (D-40: reserved for 09-11). **K13 count:** this plan adds exactly 2 gate names (`unit.core.state.export`, `runner.state-export`); cumulative Phase 9 names are 43 of about 45.

**Native seconds per added name:** `unit.core.state.export` 2.7 to 3.1, `runner.state-export` 2.2 to 2.6; **ASan:** 47.0 to 49.9 and 52 to 57 (the budget `TIMEOUT 300` and `240` leave a margin of about 4 times).

**Mapping note for 09-VALIDATION row 9-V21:** `unit.core.state.export-layout`, `.idempotence-shape` and `.non-interference` are the cases of the same names inside `unit.core.state.export`; `controls.real.state-export-fsize` is the pair of cases `export-size-cap` and `export-fsize-at-state-size` inside `runner.state-export` (K13 consolidation). It is a mapping label and not a registered CTest name.

## Prohibition checks (flagged-unverified in the plan; clean scanner output is evidence, not a dismissal)

- Rights and fixtures: `sh scripts/rights-scan.sh --tree` reports `tree=1211 history=skipped findings=0`; `git ls-files '*.sta' '*.sav' '*.eep' '*.sra' '*.rpl'` prints nothing; every blob lives under `build/` (ignored).
- Paths, user names, emails: `sh scripts/pii-scan.sh --tree` reports `tree_files=1211 commits=skipped findings=0`; the header build id of a real blob contains no path character (asserted).
- Guard: `python3 -B scripts/check-rsp-no-hle.py --build build/gate` and `build/asan` pass; the guard file changed only in `a5045d0` by 1 insertion and 0 deletions.
- Frozen files: `git log --oneline cec0446..HEAD -- scripts/qualify-phase09.py .github/workflows/ci.yml scripts/check-ci-yml.sh scripts/gate.sh scripts/rights-scan.sh tools/rights scripts/check-phase09-cases.py runner/t64_artifact_publisher.c runner/t64_artifact_publisher.h runner/t64_raw_capture.c scripts/build-boot-fixtures.sh runner/t64_real_result.c` prints nothing; `git diff cec0446..HEAD -- core/tests/test_digest_v25.c core/tests/test_si_profile.c` is empty.
- Claim wording: `check-claims.sh --files docs/claims/phase09.md` clean; `docs/spec/state-container.md` contains none of the reserved words and states that hashes are integrity checks, not authentication.

## Handoff

- **09-09 (import).** Container layout: header 0x120 (fields and offsets in the spec and in `python3 fixtures/state/decode_container.py --print-layout`), table at 0x120 with 56-byte entries, canonical offset `0x120 + 56 x count`, canonical region = domain `t64.state.si.25` then (id, body) pairs, observer body 40 bytes (five u64: drain cursor, video head and count, audio head and count), trailer 32 bytes. Closed id lists: stem 1 to 10 and 16, save ids 1 to 11 and 16. The planning-pass layout can be reused by the reader: `t64_digest_sink` recorder, `t64_state_container_length`, `T64_STATE_*` constants in `core/t64_internal.h`. The decoder kinds to mirror in mutants: `magic`, `container-version`, `total-length`, `section-count`, `flags`, `table-order`, `section-range`, `marker`, `section-hash`, `canonical-digest`, `observer`, `trailer` (the C decoder adds `header-identity`). `write_generation` rule is 09-09's (one increment per successful import into a profile with a save device, none on refusal, reset by replacement). The real continuation sources: the authored admit blob S = 6,351,511 and the 09-07 replay-mix checkpoint digests.
- **09-09 option matrix.** `--state-export` is refused with raw capture, synthetic mode and every save and replay flag (`usage`, guest tick 0, publisher never opens) and allowed with the input triple; the full matrix is in the spec.
- **09-10 review.** Cite the six mutants above (the driver was a scratchpad script, not committed); the raw-capture detail names are still missing from `result-protocol.md`.
- **09-11.** Pin nothing from this plan: the blob size and digests are regression observations of one build.

## CI/CD efficiency notes (AUTONOMY section 2)

- **Cheap checks that paid off.** Running the export once through a probe before writing any bound showed the plan's arithmetic was wrong (6.35 MB, not 8 to 11 MiB) before a test depended on it; core lint found the `strlen` allowlist miss in 14 s before any hosted run; the Linux pre-flight (Ubuntu cc, eight units, 7.6 s) and the gcc:13 closure (3.4 s) passed first time; a one-line direct run of the supervisor with the flag exposed the 32-character detail truncation immediately.
- **Mutant driver.** Compiling only the mutated production object with the gate ninja command and linking the main-tree libraries and test object costs 0.1 s per mutant (a worktree CMake configure would rebuild the fixture toolchain).
- **ASan cost.** About 20 times the native time (47 to 57 s against 2 to 3 s): each export hashes the 6.35 MB region three times and the C suite runs 25 full exports; the decoder hashing is the other half. If hosted ASan time matters, the two largest candidates are the six decoder surgeries that hash the whole blob and the three exports of the continuation schedule.
- **Tooling.** Each Bash call is a fresh shell, so the plan commit ledger, the scratchpad notes and `SP=` variables must be re-established per call; `ctest` runs and the case checker share `LastTest.log`, so batches were run one at a time.

## Deviations from Plan

**1. [Plan text - size bounds] The state is 6.35 MB, not 8.4 to 11.5 MiB.** The plan's bounds 8,400,916 and 11,534,336 assumed two 2 MiB coverage arrays; section 8 holds one (2,103,016 bytes idle). The measured idle sizes are in the table above. The tests use the literal arithmetic lower bound 4,194,308 + 2,097,152 + 4,096 + 8,200 = 6,303,756 (strictly above) and 11,534,336 (strictly below); the plan's K9 requirement that S exceed the default 69,632 limit holds. Files: `core/tests/test_state_export.c`, `runner/tests/test_state_runner.c`, spec.

**2. [Rule 1 - lint] `strlen` is not on the core allowlist** (`L2-UNDEF-SYMBOL`); a bounded `text_length` helper replaced it before the implementation commit.

**3. [Plan text - decoder order and m1] The decoder checks the marker before contiguity, and m1 edits canonical entries only.** With the plan's literal order a table pointing at the marker reached `section-range`; checking the id at offset minus 4 first gives the plan's expected kind `marker`. Applying m1 to the observer entry as well reaches `observer` first (the observer entry shape check precedes the canonical loop), so the mutant is conditional on the entry index.

**4. [Plan text - non-interference] The non-interference machine drains 12 ISViewer bytes first.** Without it the drain cursor was 0 and m5 (zeroing it) was equivalent to the original.

**5. [Test defect] `export-layout` indexed decoder fields with `count - 1` after a failed decode** (the first run of m6 segfaulted, exit -11); fixed in `2efbd6b`, the fields are now read only for an accepted blob.

**6. [Plan text - nonexistent directory] A nonexistent artifact directory is the existing artifact preflight refusal**, before the state publisher is reached: the runner binary reports detail `refusal` and the supervised record is `report-error` / `capture-invalid`, guest tick 0 (the 09-02 rule), not `state-export-open`. `state-export-open` is asserted for a leaf with another extension and for an existing target (original bytes unchanged, directory holds only that file).

**7. [Rule 1/3 - blocking] `runner/t64_real_result.h` detail field widened from 32 to 48 bytes.** `state-export-incomplete-terminal` is 32 characters; the 32-byte field truncated it to 31, the checker answered R-ENUM and the supervisor `report-invalid`. Header only; `t64_real_result.c`, the keys and the schema are unchanged; the result-protocol note about the 32-byte field was updated in the same passage.

**8. [Plan text - ceiling check] The runner checks only the artifact maximum.** `T64_STATE_MAX_BYTES` is core-internal; the artifact maximum (at most 32 MiB) is the stricter bound and the core refuses a larger container with RESOURCE_EXHAUSTED (mapped to `state-export-core`), so the "above the ceiling" check is subsumed and noted in a comment.

**9. [Plan text - non-version-25 profile] The supervised record of a legacy profile with the state flags is the report-error classification of the runner's prelaunch refusal** (the runner binary reports `refusal`); both are asserted.

**10. [Plan text - requirements] `requirements-completed` is empty**: CORE-04 completes with the import half (09-09); no checkbox was changed (Open issue 1).

**11. [Plan text - mutants at the Task 1 head] The worktree was created at the Task 3 head (`5ce5b84`)** so the Task 3 cases exist; only `core/t64_state.c` was edited. m6 was expressed as a reported-size mutant: with the bounds-checked put helpers a total that omits the observer fails closed (BAD_STORAGE, buffer cleared) and cannot write past the buffer, which is the intended property, so the mutant makes the size query lie instead and the canary check catches the overrun.

**12. [Plan text - RED order] Task 4 step 2's RED used a record shape derived from a real pass record** (no real cap record can exist before the flag); step 4 replaced it with the real record of the supervised run one byte below the state size. The LEGACY_RUNNER passing count was not recorded before any edit; the final group is 34 passing names (33 legacy plus the new one).

**13. [Added controls] The decoder-controls case also covers magic, container version, section count 0 and 33, header tick and profile digest, the runner rows add complete save and replay families (each with a non-usage baseline) and direct runner rows, and a mutant of the supervisor's one-producer rule (removing it) made the three complete-family rows fail at their comparator.**

**Total deviations:** 13. **Impact:** no guard weakened (one roster row, controls added), no frozen file changed, no digest literal or F0 pin touched; the only core behavior change outside the new file is the recorder fields and their writes in the marker function.

## Assumptions of this plan

1. The blob of one build is a self-consistency artifact of that build; no cross-build, cross-version or other-emulator statement is made.
2. The observer drain cursor of the real blob equals the log bytes the runner drained (20) because the runner drains the output before export; this is asserted against the record, not claimed as hardware behavior.
3. The Python decoder shares no code with the C decoder, but both were written from the same layout text, so a misreading of that text would be shared; the spec table was compared with both programmatically.

## Authentication gates

None.

## Known Stubs

None. No placeholder text or empty wiring exists in the created or modified files (the RED stub export was replaced in `e34740e`).

## Threat Flags

None beyond the plan's register (T-09-59 to T-09-65): the new surface is two runner options on the existing artifact publisher path, one stdlib decoder that reads a file, and the container itself (mitigated by the two-pass plan, checked arithmetic, the put helpers, canary and surgery controls, six mutants and the private 0600 no-replace publication).

## Self-Check: PASSED

- `git ls-files` lists the five new files; all twelve commit hashes above are ancestors of HEAD.
- `git worktree list` shows the main checkout only; no `__pycache__` remains outside `build/`; no `.sta` file is tracked or present outside `build/`.
- No ROM, save, PIF or IPL image, 64DD media, hardware measurement or oracle was used; the only external input is the already cleared openIPL3 asset and the authored admit ROMs. No state blob is committed or uploaded. No push was made.
