---
phase: 10-carry-in-closure-before-the-freeze
plan: 11
subsystem: core
tags: [state-container, container-v2, state-26, version-marker, decoder, export-bounds, build-identity, mutants, docs, core-06]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: the four si-v2 ids, the t64.state.si.26 walker with the depth scalars and the hidden table in section 8 (plan 10-09), the digest sweep and the Wave 2 base 6dd7859 (plans 10-02 and 10-08), the mutant driver (plan 10-01), the preflight (plan 10-01)
  - phase: 09-controller-save-media-and-replay
    provides: the state container export and importer of state 25, the version 1 decoder fixtures/state/decode_container.py (a pinned oracle), the import helper header and the frozen hostile rows
affects: [10-12, 10-13, 10-14, 10-15, phase-11]

actuals:
  tokens: 17200
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "one pairing function used by writer and reader: state_container_version_for(state word) gives 2 for state 26 and 1 otherwise, so only (1, 25) and (2, 26) can pass the header and the registry"
    - "an independent decoder for a new container version lives in a new script when the old one is a hash-pinned oracle, and a parity case compares both decoders byte for byte on the old version"
    - "sizes derived as the same builder on the matching older id plus the hand-derived additions (20 + 4 + 2,097,152), and the container length re-derived from the blob's own table"

key-files:
  created:
    - core/tests/test_state_export_v26.c
    - fixtures/state/decode_container_v2.py
  modified:
    - core/t64_internal.h
    - core/t64_state.c
    - core/CMakeLists.txt
    - tools/digest-sweep/t64_digest_sweep.c
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase10-regexes.txt
    - scripts/p10-mutants.py
    - docs/spec/state-container.md
    - docs/spec/machine-contract.md
    - docs/spec/profile-membership-sites.txt

key-decisions:
  - "The version marker is one function, state_container_version_for, used by write_header and by the reader's header check; T64_STATE_CONTAINER_VERSION stays 1 and T64_STATE_CONTAINER_VERSION_V2 is 2. The frozen v25 hostile rows keep their reasons (word 2 on a v25 blob refuses CONTAINER_VERSION, state word 24 with word 1 reaches the registry and refuses STATE_VERSION)"
  - "The v26 section 8 layout needs no new writer in t64_state.c: the exporter streams t64_digest_stream, which already emits the five depth scalars and the hidden table (plan 10-09). The exporter change is the widened predicate (t64_has_si_profile), the version word and the registry row"
  - "A state 26 machine passes the registry but stays refused at the domain bytes (state_parse_table) for every blob until plan 10-12, so a blob whose domain was forged to t64.state.si.25 and fully re-signed cannot be applied through the v25 readers to a v26 machine"
  - "fixtures/state/decode_container.py is a pinned Phase 9 oracle (F2 oracle_sha256 of harness.phase09-qualification and the state-decoder-sha256 revision of claim block 8), so the v2 decoder is the new script fixtures/state/decode_container_v2.py and the pinned file is byte for byte unchanged"
  - "The v26 upper bound is derived, not measured: the largest idle blob 10,578,819 plus 2 x 307,200 plus 2 x 262,136 is 11,717,491, rounded up to 12,582,912 (12 MiB); the v25 bound 11,534,336 stays in the document for state 25"

patterns-established:
  - "A pairing check on a header word is proved three ways: the writer's word, the reader's refusal at the word's offset with the machine allocation byte-identical, and a mutant that drops the pairing"
  - "A packet-retaining state with both rings full must fill the audio ring first (the AI then has no more samples) and the video ring second, because a full ring stalls the machine at the next event of its device"

requirements-completed: [CORE-06]

coverage:
  - id: D1
    description: "One version marker: an idle export of each si-v2 id has container version word 2 at 0x008 and state word 26 at 0x00C, the header and table by literal offset, the section ids ascending as v25 plus section 11 for the save ids; the si-v1 export keeps word 1 and state 25"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "ctest unit.core.state.export-v26 (544 checks, 0 failures; ASan pass)"
        status: pass
    human_judgment: false
  - id: D2
    description: "The header check pairs the version word with the state word: (1, 26) and (2, 26 rewritten to 25) refuse CONTAINER_VERSION at offset 0x008 on all four ids with the machine allocation byte-identical, (2, 25) and (1, 26) refuse on the v25 machine, and the frozen v25 hostile row 'container version' and the registry row 'state version' keep their reasons"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "ctest unit.core.state.export-v26 prints status 26 reason 7 position 0x8 for the rewritten copy on all four ids; ctest unit.core.state.import passes unchanged (frozen rows)"
        status: pass
      - kind: other
        ref: "python3 -B scripts/p10-mutants.py --run v26-reader-pairing-dropped v26-export-version-word-1 --no-controls: both KILLED by test_state_export_v26:export-v26-idle"
        status: pass
    human_judgment: false
  - id: D3
    description: "Section 8 of a v26 blob carries the five depth scalars at 228 + 8n + 2,097,152 and the 2,097,152-byte hidden table after the coverage table; the independent decoder decodes version 2 and prints byte-identical output for a version 1 blob, and refuses (1, 26) and (2, 25) with the kind container-version"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "ctest unit.core.state.export-v26 (decoder acceptance, parity of decode_container.py and decode_container_v2.py on the v25 blob, both refusals); ctest unit.core.state.export-v26-bounds (scalars read at the decoded offset, hidden table byte for byte)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Idle sizes equal the hand-derived values on the four ids (v25 idle plus 2,097,176: 10,545,971, 10,546,563, 10,548,099, 10,578,819); mid Z-triangle and retained video plus audio packet exports stay below the derived bound 12,582,912; a blob one byte short and one byte long are refused by the decoder"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "ctest unit.core.state.export-v26 and unit.core.state.export-v26-bounds (565 checks, 0 failures; ASan pass, 32.9 s for both)"
        status: pass
    human_judgment: false
  - id: D5
    description: "CORE-06 precision edge: depth_image_address, primitive_z and primitive_dz travel as exact u32 words and the hidden table byte for byte, on a Z triangle in flight with a nonzero sidecar"
    requirement: "CORE-06"
    verification:
      - kind: unit
        ref: "ctest unit.core.state.export-v26-bounds (words at the independently computed offset equal the machine values; memcmp of the 2,097,152 hidden bytes; SHA-256 of the table equals the decoder's)"
        status: pass
    human_judgment: false
  - id: D6
    description: "State 25 exports and the shipped ids are unchanged: the digest sweep against the Wave 2 base shows 0 differences, including the four si-v1 export lines"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "python3 -B scripts/digest-sweep.py --base 6dd785986f6aa78c6da5573a558b15b386a4edee --head WORKTREE: 1599 shared lines, 0 differences, 0 head-only; with four --head-extra-id: 0 differences, 296 head-only lines (292 as before plus one export line per si-v2 id)"
        status: pass
    human_judgment: false
  - id: D7
    description: "The export mutant v26-export-omit-hidden and two pairing mutants are killed by export-v26-idle"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "python3 -B scripts/p10-mutants.py --run v26-export-omit-hidden v26-export-version-word-1 v26-reader-pairing-dropped --no-controls: KILLED, KILLED, KILLED (baseline passed in the same worktree); --check-anchors exit 0; ctest repo.p10-mutant-anchors passes"
        status: pass
    human_judgment: false
  - id: D8
    description: "state-container.md documents container v2, the derived v26 bound next to the v25 bound, depth members excluded for state 25 and carried for state 26, and the build-identity-bound and outside-the-ABI statement; machine-contract.md carries the same statement"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "grep -c build-identity-bound gives 1 in each file; grep finds '2 for state 26' (2), '12,582,912' (3) and '11,534,336' (2) in state-container.md; sh scripts/check-docs.sh ok; sh scripts/pii-scan.sh --tree findings=0"
        status: pass
    human_judgment: false
  - id: D9
    description: "Whether the build-identity statement states the right scope (accepted only by the build identity and profile that wrote it, no cross-build portability) and whether the plan 10-15 runner pairing can keep using the pinned version 1 decoder name"
    requirement: "CORE-06"
    verification: []
    human_judgment: true
    rationale: "The wording is a scope statement for the Phase 11 freeze, and the decoder file split is a pin decision (see Deviations 1); both are for the orchestrator or the owner to confirm"

duration: 25min
completed: 2026-11-24
status: complete
commits: 3
plan_head_before: 21b27b4a4ba59a67ca36c7eb65a99c92db5f1ff1
plan_head_after: ab4754905a9e0262d763ead6ef2c823db2ca7266
---

# Phase 10 Plan 11: State 26 export, container version word 2 and build-identity documentation Summary

**A state 26 machine exports a container with version word 2 beside state word 26 (state 25 keeps word 1 and stays byte-identical, sweep 1599 lines and 0 differences), the header check pairs the two words through one function so only (1, 25) and (2, 26) pass, the section 8 depth scalars and the 2,097,152-byte hidden table are decoded by a new independent decoder (the pinned version 1 decoder is untouched), sizes are hand-derived and stay below the derived bound 12,582,912, and the docs state that blobs are build-identity-bound and outside the ABI promise.**

## Performance

- **Duration:** 25 min (about 04:45Z to 05:10Z on 2026-11-24); host 1-minute load 9 to 12, so seconds are upper bounds
- **Tasks:** 3 (Task 1 tracer, Tasks 2 and 3 auto), 3 commits
- **Files:** 12 (11 planned minus `fixtures/state/decode_container.py` plus `fixtures/state/decode_container_v2.py` and `docs/spec/profile-membership-sites.txt`, see Deviations); 1 new test binary, 2 new CTest names, 1 new decoder script

## Accomplishments

- **Task 1 (tracer, `9c51eb4`).** `T64_STATE_CONTAINER_VERSION_V2` (2) sits beside the v1 constant (1); `state_container_version_for` returns 2 for state 26 and 1 otherwise; `write_header` writes it and the reader compares the blob's word with it (`state_be32(bytes + HDR_STATE_VERSION)`) at the existing check, so a rewritten (1, 26) or (2, 25) refuses `T64_STATE_REASON_CONTAINER_VERSION` at 0x008. `t64_machine_state_export` now takes `t64_has_si_profile`; the walker already streams the v26 depth layout, so section 8 carries the five scalars and the hidden table. `state_version_registered` has the exact v26 row. `unit.core.state.export-v26` (`export-v26-idle`, 544 checks) checks, on the four ids: header words by literal offset, ascending section ids, every section length equal to the v25 export of the same builder except section 8 (+2,097,176), the hand-derived total sizes (10,545,971 / 10,546,563 / 10,548,099 / 10,578,819), the container length re-derived from the blob's own table, decoder acceptance with the section id list, the pairing refusals in the decoder and the importer (the importer prints `status 26 reason 7 position 0x8` for the rewritten copy on all four ids, machine allocation byte-identical), parity of the two decoders on the v25 blob, and a forged-domain, fully re-signed copy refused untouched. Tracer gate: both `<verify>` commands re-run on the committed tree (auto mode off, `end-of-phase` mode, automated verify only) and passed, so expansion went ahead.
- **Task 2 (`43feb09`).** `unit.core.state.export-v26-bounds` (`export-v26-bounds`, 565 checks): a Z triangle in flight (SetZImage 0x6000, SetPrimDepth, opcode 0x09, stopped after the first row, nonzero sidecar) and a machine with both video slots and both audio slots occupied, on the stem and `s32k` ids. Section 8 of the mid-render blob equals idle plus 8 bytes per pending triangle word; the scalars are read at their independently computed offset (`body + 228 + 8n + 2,097,152`) and equal the machine's words; the 2,097,152 hidden bytes are compared with `memcmp` and by SHA-256 against the decoder; the decoder refuses one byte short and one byte long (kind `total-length`). The bound is asserted as derived: idle maximum 10,578,819 + 614,400 + 524,272 = 11,717,491, rounded up to 12,582,912. Sweep, mutants and registration in this commit.
- **Task 3 (`ab47549`).** `state-container.md`: header rows `1 for state 25, 2 for state 26` and `25 or 26`, the domain per state, a "Container v2 (state 26)" section (pairing, domain, section 8 offsets, tail, sizes, import status, evidence), the v26 bound next to the unchanged v25 bound 11,534,336, depth members "excluded for state 25, carried for state 26", and the Stability paragraph; `machine-contract.md` carries the ABI note. The option matrix is untouched.
- **Independent decoder.** `fixtures/state/decode_container_v2.py` (stdlib only, own literal offsets) keeps the version 1 paths and messages and adds the version 2 pairing, the `t64.state.si.26` domain and the depth keys (`depth_scalars_offset`, the five scalars, `hidden_table_length`, `hidden_table_offset`, `hidden_table_sha256`) to the accepted line only for version 2.

## Verification

| Check | Result |
|-------|--------|
| `cmake --build --preset gate && ctest -R '^unit\.core\.state\.export-v26$'` | 1 of 1 pass (2.4 s) |
| `ctest -R '^unit\.core\.state\.export-v26'` / `ctest -N` | 2 of 2 pass, 2 names (HEAD 0) |
| `ctest -R '^(unit\.core\.state\.export\|unit\.core\.state\.import\|runner\.state-export)$'` | 3 of 3 pass (import 56 to 92 s at host load 9 to 12) |
| greps (Task 1 acceptance) | `T64_STATE_CONTAINER_VERSION_V2` 1 in `t64_internal.h`; `state_container_version_for` 3 in `t64_state.c`; `grep -v '^ *[/*]' core/t64_state.c \| grep -c "!= T64_STATE_CONTAINER_VERSION)"` 0 (HEAD 1) |
| digest sweep, Wave 2 base `6dd785986f6aa78c6da5573a558b15b386a4edee` (40-hex checked, `git cat-file -e`), `--head WORKTREE` | 1599 shared lines, 0 differences; with four `--head-extra-id` 296 head-only lines |
| mutants (committed tree) | `v26-export-omit-hidden`, `v26-export-version-word-1`, `v26-reader-pairing-dropped` KILLED by `export-v26-idle`; `--check-anchors` exit 0 |
| `ctest --test-dir build/gate -j4` (whole gate preset, after Task 3) | 246 of 246 pass, 85 s; `sh scripts/check-test-inventory.sh --preset gate` PASS count 246 |
| ASan (`ctest --test-dir build/asan -R '^unit\.core\.state\.export-v26'`) | 2 of 2 pass, 32.9 s both; registered TIMEOUT 300 against 2.4 s and 1.3 s native |
| `harness.phase09-qualification`, `harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit` | pass (the pinned decoder is unchanged; membership list regenerated) |
| `sh scripts/check-docs.sh`, `sh scripts/pii-scan.sh --tree` | ok, findings 0 |
| `sh scripts/preflight-phase10.sh` (clean committed tree at `ab47549`) | PASS 75 s: 39 regex rows 0 mismatches, 22 phase10 labels, 225 units compiled with the act image cc 13.3.0, 11 post-Gate steps replayed, PII clean (host load 9.1, WARN) |
| new names, native seconds | 2.4 s and 1.3 s (the three decoder runs per state dominate) |

## Task Commits

1. **Task 1: export one idle v26 state end to end (tracer)** - `9c51eb4` (feat)
2. **Task 2: bounds, mid-render and retained-packet exports, mutants, sweep note** - `43feb09` (test)
3. **Task 3: container v2 and build-identity documentation** - `ab47549` (docs)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The v1 decoder is a pinned oracle, so the v2 decoder is a new script**
- **Found during:** Task 1 (read_first of the decoder)
- **Issue:** the plan edits `fixtures/state/decode_container.py`, but its SHA-256 is pinned by `oracle_sha256` in the F2 block of `scripts/tests/test_phase09_qualification.py:668` (compared with the live file at lines 865 to 868) and by the claim block 8 revision `state-decoder-sha256:e02a7551...` of `docs/claims/phase09.md` and `docs/evidence/phase09-qualification.md`. An edit would fail `harness.phase09-qualification` and would need a closed Phase 9 record re-pinned, which `PIN-IMPACT.md` section 9 names an orchestrator-only identity task, never an executor edit.
- **Fix:** `fixtures/state/decode_container_v2.py` carries the version 1 paths and messages forward and adds version 2; `decode_container.py` is byte for byte unchanged (`git diff` against the Wave 0 base is empty). The new test takes both scripts from the environment (`T64_STATE_DECODER` the v2 script, `T64_STATE_DECODER_V1` the pinned one) and compares their output on a v25 blob.
- **Files modified:** `fixtures/state/decode_container_v2.py` (new), `core/CMakeLists.txt`, `scripts/p10-mutants.py`
- **Commit:** `9c51eb4`, `43feb09`
- **Consequence for plan 10-15:** its `state-v2` case says "decodes with fixtures/state/decode_container.py (v2 decode)" and the runner test environment `T64_STATE_DECODER` (`runner/CMakeLists.txt:623`) names the pinned file; plan 10-15 must name `decode_container_v2.py` instead, unless the orchestrator decides an identity-only re-pin of the Phase 9 oracle and claim block 8.

**2. [Rule 2 - Missing critical] A state 26 machine stays refused at the domain bytes**
- **Found during:** Task 1 (reading `state_parse_table` after adding the registry row)
- **Issue:** with the registry row alone, a blob with state word 26, container word 2, the domain forged to `t64.state.si.25` and every hash recomputed passes the header, the registry and the binding on a v26 machine and reaches the v25 section readers, which would apply a v25-shaped state to a v26 machine (the importer's hashes are integrity checks, not authentication). The orchestrator rule is that state 26 import stays refused until plan 10-12.
- **Fix:** `state_parse_table` takes the domain check only for a v25 machine (`!t64_has_si_v25_profile(machine) || memcmp(...)`), so a v26 machine refuses every blob at the domain bytes with the existing SECTION_BOUNDS reason; the importer test copies a v26 blob, forges the domain, re-signs it with `blob_resign(RS_ALL)` and requires a refusal with the machine allocation byte-identical. The genuine-blob assertion stays `reason is none of MAGIC, TOTAL_LENGTH, CONTAINER_VERSION`, so it also holds once plan 10-12 accepts state 26.
- **Files modified:** `core/t64_state.c`, `core/tests/test_state_export_v26.c`
- **Commit:** `9c51eb4`

**3. [Rule 2 - Added check] Two pairing mutants beyond the planned export mutant**
- **Issue:** the plan lists only `v26-export-omit-hidden`; the version-word pairing (T-10-52) had no mutant.
- **Fix:** `v26-export-version-word-1` (the pairing function returns 1 for state 26) and `v26-reader-pairing-dropped` (the header check accepts any word up to 2); both KILLED by `export-v26-idle`. `v26-export-omit-hidden` reuses the walker emission anchor of `v26-digest-omit-hidden` (the exporter's section 8 writer is the walker).
- **Files modified:** `scripts/p10-mutants.py`
- **Commit:** `43feb09`

**4. [Rule 3 - Blocking] Membership sites list**
- **Issue:** `repo.si-membership-sites` fails once `t64_machine_state_export` names `t64_has_si_profile`; the list is not in `files_modified`.
- **Fix:** regenerated with `python3 -B scripts/list-membership-sites.py --print` (one new row `core/t64_state.c :: t64_machine_state_export :: t64_has_si_profile`).
- **Files modified:** `docs/spec/profile-membership-sites.txt`
- **Commit:** `9c51eb4`

### Plan and code observations (no behavior deviation)

- **Section 8 writer.** The plan says to "emit, in the section 8 writer of `core/t64_state.c`, the five depth scalars and the hidden table". No such writer exists in `t64_state.c`: the exporter streams `t64_digest_stream`, which has emitted both since plan 10-09. The exporter change is the predicate, the version word and the registry row; section 8 is covered by the tests and the decoder.
- **Sweep tool.** The plan asks to teach `tools/digest-sweep/t64_digest_sweep.c` to print export lines for state 26. `export_hash` calls the public export API and printed a line as soon as the export succeeded, so the only edit is its header comment. The four new `ID export SHA256HEX` lines appear as head-only lines (292 to 296).
- **Task boundary.** The `export-v26-bounds` case code is in the Task 1 commit (compiled, reachable with `--case`, not registered); Task 2 registers it, adds the mutants and records the sweep. Both Task 1 verify commands and the case itself passed before the Task 1 commit.
- **Mutants need a committed tree** (as in plan 10-10): they ran on `43feb09` right after the commit and passed, so no fix commit was needed.
- **Regex rows.** `scripts/preflight-phase10-regexes.txt` also carries the row for the plan's second Task 1 verify regex (3 names), which the preflight counts.

**Total deviations:** 4 (2 Rule 3, 2 Rule 2). **Impact:** one new fixture script instead of an edit to a pinned one (plan 10-15 follows it), one extra membership-list row, no change to any pin, sealed body, oracle-bound file or phase09-inventoried binary.

## Review findings and observations (information only)

- **Info (importer boundary for plan 10-12):** plan 10-12 says an idle v26 export imports with `T64_STATUS_OK` after its layout step. That also needs (a) the domain check of `state_parse_table` (the `t64_has_si_v25_profile` guard of Deviation 2) and `STATE_DOMAIN` in `state_recompute_digest` to select `t64.state.si.26` for a state 26 machine, and (b) the state 26 readers. `state_import_helpers.h` `D_DOMAIN` and `blob_resign` use the v25 domain; a v26 re-sign needs the v26 text.
- **Info (decoder offset derivation):** the scalar offset `228 + 8n + 2,097,152` was derived from the walker order of `t64_digest_stream` (the section 8 fields before `work_policy_version`) and then proved by the test: a wrong offset would make the words read from the blob differ from the machine's words on the Z-triangle state, which has nonzero scalars (address 0x6000, nonzero Z and dZ).
- **Info (idle sizes):** the hand-derived sizes equal the Phase 9 measured idle sizes of the document (8,448,795 / 8,449,387 / 8,450,923 / 8,481,643) plus 2,097,176; the test asserts both the v25 side and the v26 side.
- **Info (retained packets):** the packet-retaining blob of the test holds 2 small video packets and 2 small audio packets (10,546,075), far below the arithmetic maximum; the bound is the derived worst case and the test asserts the derivation, not an occupied maximum.
- **Info (no windows ledger):** `.planning/WINDOWS.md` does not exist, so no entry was appended; the decoder split is recorded in Deviations instead.

## Issues Encountered

- The first packet-retaining builder started the VI and the AI together; the video ring filled at tick 246 and the machine stopped at the blocked field event (resource-limit stop), so no audio packet was produced. The builder fills the audio ring first and the video ring second.
- A background sweep run started from a wrapper shell left its output file empty until the sweep ended; the summary line was read after completion.

## Authentication Gates

None.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-52 mitigated (the pairing function at the reader check, the frozen v25 rows unchanged, the rewritten-word checks on four ids and two mutants killed). T-10-24 mitigated (blobs are written 0600 under `build/gate/10-11-evidence`, removed after each decoder run, nothing committed). T-10-25 mitigated (exact sizes and the derived bound asserted; the 64 MiB ceiling and the runner 32 MiB maximum are unchanged).

## Next Phase Readiness

Ready for plan 10-12 (importer half). It starts from: the registry row for state 26 (exact v26 predicate), the container version pairing at the header check, a v26 export of every si-v2 id, the section 8 offsets in `state-container.md` ("Container v2 (state 26)") and `decode_container_v2.py`, and the guarded domain check named in Deviation 2. `unit.core.state.export-v26` asserts only that a genuine v26 blob gets past the header, so it stays green when 10-12 accepts it. The runner format-string pairing is plan 10-15 and must use `decode_container_v2.py` (Deviation 1).

## Self-Check: PASSED

- Files found: `core/tests/test_state_export_v26.c`, `fixtures/state/decode_container_v2.py`, `docs/spec/state-container.md`, `docs/spec/machine-contract.md`.
- Commits found as ancestors of HEAD: `9c51eb4`, `43feb09`, `ab47549`; `git rev-list --count 21b27b4..HEAD` is 3 before this file.
- The pinned `fixtures/state/decode_container.py`, `core/tests/test_state_export.c`, `core/tests/test_state_import.c`, `runner/tests/test_state_runner.c` and `docs/spec/phase09-cases.json` are unchanged against `21b27b4`.
