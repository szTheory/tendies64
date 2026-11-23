# Phase 10: Carry-in Closure Before the Freeze - Pattern Map

**Mapped:** 2026-11-23
**Files analyzed:** 36 new/modified files (by wave, from 10-CONTEXT.md and 10-RESEARCH.md "Recommended project structure")
**Analogs found:** 34 / 36 (2 have no code analog: PIN-IMPACT.md table, doc relabels)
**Read-only:** no source file was touched; the 14 oracle-bound files are cited as read-only analogs only and must not be edited (C1).
**Tracked-source gate:** every analog path below was checked with `git ls-files` (all tracked, none is a gitignored mirror).

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match |
|---|---|---|---|---|
| `scripts/preflight-phase10.sh` (W0 NEW) | script | batch | `scripts/gate.sh` (step helper) | role-match |
| `core/tests/test_k1_refusal.c` (W0 NEW) | test | request-response (command list -> refusal) | `core/tests/test_rdp_depth.c` (`case_refusal_truncation_and_recovery`) + `test_si_profile.c` (`create_machine` over ids) | role-match |
| `scripts/check-rdp-oracle-authority.py` (W0 EDIT, orchestrator only) | checker | transform | itself, `_verify_corpus` lines 296-397 | exact (self) |
| `scripts/tests/test_rdp_oracle_authority.py` (W0 EDIT) | test | request-response (CLI) | itself (`invoke`, `invoke_forged_approval`) | exact (self) |
| `.github/workflows/ci.yml` (W0 EDIT) | config | n/a | itself lines 15-22, 70-76 | exact (self) |
| `scripts/check-ci-yml.sh` (W0 EDIT) | scanner | transform | itself lines 27, 286-288, 538, 681 | exact (self) |
| `core/CMakeLists.txt` COST (W0 EDIT) | config | n/a | itself lines 841-846, 1100-1106 | exact (self) |
| `docs/ci-reviews/LOG.md` (W0 EDIT) | doc | n/a | F-106 / F-114 rows (lines 272, 280) | exact (self) |
| `scripts/gate.sh` (W0 optional EDIT) | script | batch | itself lines 98-110 | exact (self) |
| `.planning/phases/10-.../PIN-IMPACT.md` (W0 NEW) | doc | n/a | none (table seed is in 10-RESEARCH.md "Pin-impact map") | no analog |
| `core/tests/test_vg_gaps.c` (W1 NEW) | test | transform (vector tables) | `core/tests/test_rdp_depth.c` + `core/tests/test_vi.c` (case table) | exact |
| `scripts/p10-mutants.py` (W1 NEW) | tooling | batch | `scripts/k5-mutants.py` | exact |
| `scripts/check-test-inventory.sh` (W1 EDIT) | script | transform | itself, `PHASE09_TESTS` block + line 343 | exact (self) |
| `core/t64_internal.h` predicates (W2 EDIT) | model | n/a | `t64_has_si_profile` (1168-1174), `t64_has_rdp_depth_profile` (1290-1297) | exact (self) |
| `core/t64_machine.c` + `core/include/tendies64/t64_machine.h` ids (W2 EDIT) | model/registry | n/a | `T64_SI_FAMILY_COMMON_FIELDS` + `has_si_*_profile_id` + `t64_profile_lookup` | exact |
| `core/t64_digest.c` v26 walker (W2 EDIT) | service | transform (streaming hash) | itself: domain chain 270-360, depth block 705-720 | exact (self) |
| `core/t64_state.c` v26 export/import (W2 EDIT) | service | file-I/O (validate-then-apply) | itself: 1670-1700, 2712-2725, 2995-3030 | exact (self) |
| `docs/spec/profile-membership-sites.txt` (W2 regenerate) | generated data | batch | `scripts/list-membership-sites.py --print` | generated |
| `docs/spec/state-field-rules.txt`, `state-mutants.txt` (W2 EDIT) | data | n/a | their own row formats (headers) | exact (self) |
| `core/tests/test_digest_v26.c` (W2 NEW) | test | transform | `core/tests/test_digest_v25.c` | exact |
| `core/tests/test_rdp_depth_composite.c` (W2 NEW) | test | request-response | `core/tests/test_rdp_depth.c` | exact |
| `core/tests/test_state_import_v26.c` (W2 NEW) | test | file-I/O | `core/tests/test_state_import.c` B17 (3104-3200) + `state_import_helpers.h` | role-match |
| `core/tests/fuzz_state_import.c` (W2 EDIT) | test | file-I/O | itself (no main; called from `test_state_import.c`) | exact (self) |
| `core/tests/test_si_profile.c` matrix rows (W2, see Warning C7) | test | request-response | itself 50-243 | exact (self) |
| `fixtures/state/decode_container.py` (W2 EDIT) | tooling | transform | itself | exact (self) |
| `docs/spec/{rdp,vi,machine-contract,state-container}.md`, `.planning/PROJECT.md` (W2 EDIT) | doc | n/a | state-container.md lines 25-45, 160-180 | exact (self) |
| 7 provisional-name docs (W2 sweep) | doc | n/a | none (mechanical relabel list in C6) | no analog |
| `fixtures/composite/*.S, *.ld` (W3 NEW) | guest | event-driven | `fixtures/rdp/fill.S` + `fixtures/audio/pcm.S` + `fixtures/save/sram.S` | composed |
| `scripts/build-composite-fixture.sh` (W3 NEW) | script | batch | `scripts/build-save-fixtures.sh` | exact |
| `fixtures/manifest.json` rows (W3 EDIT) | config | n/a | row `phase09-save-roundtrip-s32k` | exact |
| `runner/t64_real_fixture.c` rows (W3 EDIT) | model | n/a | rows `phase09-save-roundtrip-s32k`, `phase09-sram-rw-wrong-s32k-v1` (lines 142-153) | exact |
| `runner/tests/test_composite_two_run.c` (W3 NEW) | test | request-response (two supervised runs) | `runner/tests/test_save_roundtrip.c` (`run_spec`) + `test_audio_capture.c` (`run_capture_resume_case`) | exact |
| `core/CMakeLists.txt` fixture target, `runner/CMakeLists.txt` test (W3 EDIT) | config | n/a | `t64-phase09-save-fixture` (305-338), `runner.save-roundtrip` (692-713) | exact |
| Phase 10 thin-delta qualifier (W4, optional) | tooling | batch | `scripts/qualify-phase09.py` header | role-match |

## Pattern Assignments

### Shared CTest registration for a NEW core unit-test binary (use for every new `core/tests/*.c` in W0-W2)

**Analog:** `core/CMakeLists.txt:763-780` (`test_digest_v25`, needs the openIPL3 asset)

```cmake
add_executable(test_digest_v25 tests/test_digest_v25.c)
target_include_directories(test_digest_v25 PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_digest_v25 PRIVATE t64_core t64_sha256)
t64_target_warnings(test_digest_v25)
add_dependencies(test_digest_v25 t64-boot-fixtures)
add_test(NAME unit.core.digest.sink-legacy-identity
  COMMAND test_digest_v25 --case sink-legacy-identity)
set_tests_properties(unit.core.digest.sink-legacy-identity
  unit.core.digest.v25-reference
  unit.core.digest.v25-strip-differential PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
```

Variant when the test needs the authored RDP fill ROM (`core/CMakeLists.txt:420-430`, `test_rdp_depth`): replace the dependency with `add_dependencies(<t> t64-rdp-fill-fixture)` and the environment with `ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64"`. Pure-profile/matrix tests with no ROM (like `test_si_profile`) still link `t64_core t64_sha256` and depend on `t64-boot-fixtures` (`core/CMakeLists.txt:741-760`).

**Naming convention (copy):** `unit.core.<area>.<case>` for core cases (one CTest name per `--case`), `runner.<name>` and `controls.real.<name>` for supervised-runner tests and their wrong-value controls, `harness.<name>` for python checkers, `repo.<name>` for scanners. Labels: `unit`; `"runner;controls"`; `"harness;qualification"`.

**C-test main with `--list-cases` / `--case` dispatch** (`core/tests/test_digest_v25.c:1800-1836`; the same shape is in `test_si_profile.c:1180-1220`):

```c
int main(int argc, char **argv) {
  static const char *const cases[] = {"sink-legacy-identity", "v25-reference",
                                      "v25-strip-differential"};
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) { /* print each */ return 0; }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "v25-reference") == 0) {
      case_v25_reference();
      return T64_TEST_FINISH("unit.core.digest.v25-reference");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) { fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]); return 2; }
  /* no args: run every case, return T64_TEST_FINISH("unit.core.digest.v25"); */
}
```

Assertions come from `tests/t64_test.h`: `T64_CHECK`, `T64_CHECK_INT(got, want)`, `T64_CHECK_STR`, `T64_TEST_FINISH(name)` (nonzero exit when zero checks ran). Counters are file-static, so each test binary owns its counters.

**Test-file preamble to copy** (`core/tests/test_digest_v25.c:14-137`): the five includes (`t64_internal.h`, `t64_sha256.h`, `t64_test.h`), a `test_allocate`/`test_release`/`allocator` triple, `read_openipl3_asset()` (env `T64_TEST_IPL3_PATH`, exactly 4096 bytes), `fill_authored_rom()`, and `create_admitted(profile_id, ipl3)`:

```c
static t64_machine *create_admitted(const char *profile_id, bool ipl3) {
  ...
  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine), T64_STATUS_OK);
  if (ipl3) { read_openipl3_asset(asset); status = t64_machine_admit_rom(machine, asset, sizeof asset); }
  ...
}
```

---

### Shared: `scripts/check-test-inventory.sh` (every wave that adds a CTest name)

**Analog:** itself, `PHASE09_TESTS='...'` at line 196 and the join at line 343.

```sh
PHASE09_TESTS='unit.core.profile.predicate-matrix
unit.core.profile.v12-identity
...
harness.state-field-use-audit'
...
printf '%s\n' "$BASELINE_CORE" ... "$PHASE08_TESTS" "$PHASE09_TESTS" "$_extra"
```

Add a `PHASE10_TESTS='...'` block (one name per line, no trailing blank) and append `"$PHASE10_TESTS"` to the line-343 `printf` list. `--selftest` removes each name once (`run_mode_selftest`, lines 593-608), so each new name costs about 0.2 s natively (hosted macOS selftest already about 47 s). The inventory is exact: an unregistered name, an extra registered name and a duplicate all fail. Run `sh scripts/check-test-inventory.sh --preset gate` and `--selftest` after each wave.

---

### Wave 0: `core/tests/test_k1_refusal.c` (test, request-response) - characterize Z refusal on descriptors 11 and 12 BEFORE any predicate edit

**Analog A (iterate ids, create machines):** `core/tests/test_si_profile.c:172-243` (`create_machine`, `legacy_rows`, `family_rows`, `observed_row`).
**Analog B (command list through the real DPC path, refusal asserts, snapshot of state):** `core/tests/test_rdp_depth.c:96-131` (`dispatch_all`, `run_commands`) and `:899-1010` (`capture_depth_render_snapshot`, `case_refusal_truncation_and_recovery`).

```c
/* test_rdp_depth.c:113-131 */
static bool run_commands(t64_machine *machine, const uint64_t *words, size_t count) {
  ...
  for (i = 0u; i < count; i++) write_be64(machine->ram + COMMAND_ADDRESS + i * 8u, words[i]);
  end = COMMAND_ADDRESS + (uint32_t)(count * 8u);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u, COMMAND_ADDRESS), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, end), T64_STATUS_OK);
  return dispatch_all(machine);
}
```

Refusal sites to pin (source, not copy): SetZImage (0x3e) and SetPrimDepth (0x2e) are refused at dispatch by `supported_opcode` (`core/t64_rdp.c:863-869`, increments `dpc_clock_counter` and `dpc_buffer_busy_counter`, then `T64_RDP_REFUSAL_UNSUPPORTED_OPCODE`); the four Z triangles (0x09/0x0b/0x0d/0x0f) pass dispatch and are refused at apply (`core/t64_rdp.c:444-457`). Pin both paths, including the DPC counters, `rdp.refusal_reason`, `last_opcode_field` and the machine digest, for the id `...rdp-vi-filter-v9-ai-v1` and the four `...si-v1[-e4k|-e16k|-s32k]` ids. Use the SetZImage/SetPrimDepth/triangle command words already literal in `test_rdp_depth.c:178-195` and `:954-1010`. Register with the IPL3 env variant (the ids need `ipl3 = true`, unlike the descriptor-10 ids that admit `rdp-fill.z64`).

---

### Wave 1: `core/tests/test_vg_gaps.c` (test, transform) - VG-01..05 and the NTSC clock vector, in a NEW binary

**Analogs:** `core/tests/test_rdp_depth.c` (vector-table style, lines 603-661) and `core/tests/test_vi.c` (case table with suites, lines 2425-2640; clock case at 790-840). DO NOT edit either file (both are oracle-bound, C1).

Vector-table idiom with independent hex literals (`test_rdp_depth.c:634-661`):

```c
static void case_primitive_compression_exponent_transitions(void) {
  static const uint64_t modes = UINT64_C(0x2f00000000000034);
  static const uint64_t primitive_words[] = { UINT64_C(0x2e00000000000009), ... };
  static const uint16_t visible[] = { 0x0000u, 0x1ffcu, 0x2000u, ... };
  for (i = 0u; i < sizeof primitive_words / sizeof primitive_words[0]; i++) {
    primitive_result result;
    T64_CHECK(run_primitive_vector(modes, primitive_words[i], 0xfffcu, 0u, &result));
    T64_CHECK_INT(result.refusal, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT(result.depth, visible[i]);
    T64_CHECK_INT(result.hidden, 0x83u);
  }
}
```

Accepted-effect case with the comment that records the independent derivation (`test_rdp_depth.c:178-228`): command words, expected literal bytes, then `/* Independent software oracle: 0x1357*8=0x9ab8, exponent=0, floor(0x9ab8/64)=0x26a, visible=0x09a8, delta code=3. */`. Copy this comment-the-derivation habit; the literal tables for VG-01, exponent boundaries and the NTSC vector (`first deadline 1,042,709`, field `812,175` source cycles) are already in 10-RESEARCH.md "Depth semantics recheck". Never call `z_compress`/`z_decompress` from the test.

Wrong-value control (`core/tests/test_vi.c:643-683`, `case_vi_filter_wrong_expected`): accepted setup in the same case, copy the expected buffer, change one byte (`wrong[28u] = (uint8_t)(wrong[28u] - 1u)`), and require the first difference at that exact offset (`T64_CHECK_INT(first_difference(packet, wrong, length), 28u)`).

Case table with named cases and `--suite`/`--list-cases` (test_vi.c:2449-2455, 2557-2562):

```c
typedef struct { const char *name; void (*run)(void); const char *suite; } test_case;
static const test_case cases[] = {
    {"vi-ntsc-rational-field-timing", case_vi_ntsc_rational_field_timing, "vi-timing"}, ...
};
```

**Registration:** one CTest name per case, `add_test(NAME unit.core.vg.<case> COMMAND test_vg_gaps --case <case>)`, env `T64_TEST_RDP_FILL_PATH` (copy `core/CMakeLists.txt:445-460`, the `unit.core.vi.*` block, for the multi-name-per-binary form). NTSC clock vector: `vi_clock_policy` is `"rsp62500000-ntsc48681812-halfline-rational-v2"`; machine setup helper to copy is `create_vi_v8_machine()` + `advance_guest_to_tick()` in `test_vi.c`.

---

### Wave 1: `scripts/p10-mutants.py` (tooling, batch) - mutant driver

**Analog:** `scripts/k5-mutants.py` (613 lines). Copy structure, change ids/edits/checks.

Mutant table entry shape (`k5-mutants.py:90-105`):

```python
{
    "id": "latch-visible-from-wrong-tick",
    "tag": "K5-1",
    "expect": "KILLED",
    "title": "...",
    "edits": [ ("core/t64_si.c", "<unique anchor text>\n", "<replacement>\n") ],
    "kill": [matrix_check("cross-device", ["si-sample"])],   # check that must fail
    "record": [other_check("test_si", "latch-strictly-after")],
},
```

Two controls the driver needs (`k5-mutants.py:214-246`): an equivalent edit with `"expect": "SURVIVED"` and `"kill": []` plus an `"all_pass"` list of checks, and a non-compiling edit (`#error`) with `"expect": "INVALID"`. Classes: KILLED / SURVIVED / INVALID. Kill checks for the new tests should name `test_vg_gaps --case <name>` and use the non-matrix form `OWN_CHECK_LINE = re.compile(r"test_[a-z_]+\.c:\d+: CHECK(_INT|_STR)? failed")` (line 59), updating `TEST_SOURCES` (line 55-60) with the new test sources.

Modes (`--list`, `--check-anchors`, `--run [ID...] [--no-controls]`) and exit codes (0 ok, 1 survivor/unexpected control, 2 usage/dirty tree, 3 anchor missing/not unique, 4 baseline failure) are in the docstring lines 1-34. `--run` builds a throwaway `git worktree` under `build/<id>-mutants` (constant `WORKTREE`, line 49) and writes `RESULTS` to `build/gate/...`; point both at `build/10-mutants/wt` and `build/gate/10-p10-results.txt`.

**Cheap in-gate CTest for anchors** (`runner/CMakeLists.txt:213-220`):

```cmake
add_test(NAME repo.sched-k5-anchors
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/k5-mutants.py --check-anchors)
set_tests_properties(repo.sched-k5-anchors PROPERTIES
  LABELS runner TIMEOUT 60 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
```

Mutants named in 10-RESEARCH.md Validation (decal `<=` to `<`, primitive Z cap `0x7fff` to `0x7ffe`, delta ceiling to floor in `t64_rdp_depth_evaluate_pixel`, VG-02..05, VI clock `48681812` to `48600000`, depth sidecar omitted from the digest, sidecar invalidation loop removed `core/t64_rdp_pipeline.c:269`, predicate widened to descriptors 11/12) take their anchors from the committed source via `--check-anchors`.

---

### Wave 0 (GATE-02, orchestrator task only): per-row oracle pin

**Analog:** `scripts/check-rdp-oracle-authority.py` and `scripts/tests/test_rdp_oracle_authority.py` (both edit themselves). Executors never edit the checker or `PINNED_REQUEST_SHA256`.

Sites to change (verified):
- `check-rdp-oracle-authority.py:23` `PINNED_REQUEST_SHA256`; `:37-52` `EXPECTED_CORPUS_PATHS` (14 paths, delete `fixtures/manifest.json` from it).
- `_verify_corpus`, whole-file manifest hash (`:308-314`):

```python
fixture_manifest = corpus.get("fixture_manifest")
fixture_path = _safe_repo_path(fixture_manifest.get("path"))
fixture_bytes = fixture_path.read_bytes()
if hashlib.sha256(fixture_bytes).hexdigest() != fixture_manifest.get("sha256"):
    raise Refusal("fixture manifest SHA-256 changed")
```

- Per-row checks already exist and are the model for the new binding (`:380-396`):

```python
rows = {entry.get("id"): entry for entry in fixture_rows if isinstance(entry, dict)}
if fixture_ids != ["rdp-fill", "rdp-fill-payload"]: raise Refusal(...)
for expected in corpus.get("fixture_rights", []):
    row = rows.get(expected.get("id"))
    if row.get("source_rights") != "authored-here" or row.get("binary_rights") != "cleared" or \
       row.get("distribution") != ["build-only"] or row.get("sha256") != expected.get("built_fixture_sha256") or \
       row.get("size") != expected.get("built_fixture_size_bytes"):
        raise Refusal("authored fixture rights or identity changed")
```

- Hash helper to reuse: `_canonical_sha256` (the test file's twin is `canonical_sha256`, `test_rdp_oracle_authority.py:59-64`).

Test-side patterns to extend (`scripts/tests/test_rdp_oracle_authority.py`):
- `BOUND_FILES` (lines 22-37) must track `EXPECTED_CORPUS_PATHS`.
- `invoke(flag, checker=..., cwd=...)` (40-56) runs the real CLI; `private_temporary_directory()` (86-88) builds a scratch mirror under `build/gate/07-12-task1/...`; `invoke_forged_approval()` (110-173) shows the mirror recipe: copy `BOUND_FILES` plus `docs/evidence/phase07-oracle-authority.json` and the checker into the mirror, mutate, run `--require-approved` with `cwd=mirror`; `require(cond, msg, result)` (176-180) is the assertion helper; `main()` (217-294) holds the exact-set `expected_controls` tuple that the checker's `--selftest` must print (add any new control name there and in the checker).
- New cases: (a) append an unrelated authored row to a scratch manifest copy -> `--expect-blocked` still exits 0 with "blocked: no oracle operation is authorized"; (b) change a bound row's `sha256`/`size`/rights -> exit nonzero; (c) the old whole-file mutation no longer refuses; (d) `--require-approved` still exits 2 with "no pinned competent owner/legal decision".

CTest: `harness.rdp-oracle-authority` (`runner/CMakeLists.txt:780`); run `ctest -R '^harness\.rdp-oracle-authority$'` (about 0.2-1 s) after every manifest or bound-file edit.

---

### Wave 0 (GATE-01): CI timeout rule, selftest mutations, COST

**Analog:** `scripts/check-ci-yml.sh`, `.github/workflows/ci.yml`, `core/CMakeLists.txt`.

Current rule (`check-ci-yml.sh:27`, `:286-288`):

```sh
#   CI-TIMEOUT           required gate jobs set timeout-minutes from 1 to 10
...
if (field == "timeout-minutes") {
  if (val ~ /^[0-9]+$/ && val + 0 >= 1 && val + 0 <= 10) hastimeout = 1
  else viol("CI-TIMEOUT", "timeout-minutes must be an integer from 1 to 10")
}
```

Trap (verified): selftest mutations inject after the FIRST line matching `timeout-minutes: 10` (`check-ci-yml.sh:538` and `:681`):

```sh
awk '/timeout-minutes: 10/ && !done { print; print "    env:"; print "      TOKEN: ${{ secrets.TOKEN }}"; done = 1; next } { print }' "$REAL" > "$W/secrets.yml"
expect CI-SECRETS "a secrets reference" "$W/secrets.yml"
```

`ci.yml` has `timeout-minutes: 10` at line 20 (gate-linux) and 74 (gate-macos). Keep gate-linux at 10 so the first-match awk still binds; set only gate-macos to 15; make the scanner bound per job (`job == gate-macos` -> max 15, others max 10) and add two selftest mutations using the repo's `mut <label> '<sed>'` + `expect <RULE> "<desc>" "$W/<label>.yml"` pair (visible at `check-ci-yml.sh:545-548`): macos 16 must fail `CI-TIMEOUT`, linux 11 must fail `CI-TIMEOUT`. The scanner comment line 27 changes in the same commit. The existing stress-workflow mutation at line 470 is unrelated.

COST (no existing COST anywhere; `core/CMakeLists.txt` and `runner/CMakeLists.txt` grep returned none). Place beside the registration:

```cmake
# core/CMakeLists.txt:841-846 (existing)
add_test(NAME unit.core.state.import COMMAND test_state_import)
set_tests_properties(unit.core.state.import PROPERTIES LABELS unit TIMEOUT 600 ENVIRONMENT "...")
# add (plain double, order only):
set_tests_properties(unit.core.state.import PROPERTIES COST 1000)
```

The sanitizer block (`core/CMakeLists.txt:1100-1106`) already does `set_tests_properties(unit.core.state.import PROPERTIES TIMEOUT 2400)` inside `if(...)`; put `COST` outside that conditional. `harness.state-field-use-audit` and `harness.phase09-qualification` (`runner/CMakeLists.txt:731`) are the other candidates; `repo.rsp-no-hle` is the guard test (C3: schedule last, measure first).

`docs/ci-reviews/LOG.md`: append rows to the `## Findings` table (`| id | run id | finding | taken/declined | reason |`, line 135+) for the next free `F-NNN` and amend dispositions of F-106 (line 272, currently `declined`), F-111 and F-114 by NEW rows (rows are append-only; `scripts/ci-review.sh` refuses duplicates and edits: `--append FILE --finding "id|finding|taken/declined|reason"`). Run rows: `ci-review.sh RUN_ID --append docs/ci-reviews/LOG.md`.

---

### Wave 0: `scripts/preflight-phase10.sh` (script, batch, under about 90 s)

**Analog:** `scripts/gate.sh` (122 lines): POSIX sh, `step <name> <command>` helper printing `[gate] ...` lines with a per-step seconds summary, `sh_n` over tracked `*.sh`, and a final `GATE_STEPS` summary. Copy the `step`/`banner`/summary frame (gate.sh lines 1-60 and 98-122) and replace the step list with the Wave 0 list in 10-RESEARCH.md "Wave 0 Gaps": act-image Linux compile, post-Gate hosted-step replay (F-107), `ctest -N -R` count check, TIMEOUT audit (F-108), `ctest -R '^harness\.rdp-oracle-authority$'`, `scripts/pii-scan.sh`, `gh run list` head status, host load. Private JUnit roots are an allow-list at `gate.sh:102-105` (`08-05-audio-private`, `09-11-phase09-private`); add a Phase 10 root only if a Phase 10 qualifier needs one.

---

### Wave 2: identity bump (descriptor 13 / model 21 / state 26) - registry and predicates

**Analog (predicates):** `core/t64_internal.h:1168-1174` and `:1290-1297`:

```c
static inline bool t64_has_si_profile(const t64_machine *machine) {
  return machine != NULL && machine->profile.descriptor_version == 12u &&
         machine->profile.machine_model_version == 20u &&
         machine->profile.state_serialization_version == 25u;
}
...
static inline bool t64_has_rdp_depth_profile(const t64_machine *machine) {
  return machine != NULL && machine->profile.descriptor_version == 10u &&
         ((machine->profile.machine_model_version == 16u && ... == 21u) ||
          (... == 17u && ... == 22u) ||
          t64_has_vi_filter_v9_profile(machine));
}
```

Recommended edit (10-RESEARCH.md Pattern 1): `t64_has_si_v25_profile` (exact 12/20/25) and `t64_has_si_v26_profile` (exact 13/21/26) new; `t64_has_si_profile` becomes `v25 || v26` ("SI device family"); `t64_has_rdp_depth_profile` gains `|| t64_has_si_v26_profile(machine)`. Note the existing `descriptor_version == 10u &&` guard would exclude the v26 tuple, so the new term must sit outside that conjunction. Digest, `state_version_registered`, exporter and `T64_STATE_CONTAINER_VERSION` selection use the exact predicates. Cheap verification after the header edit: `python3 -B scripts/check-rsp-no-hle.py --build build/gate` (sealed bodies must not need re-pin) and `ctest -R '^repo\.si-membership-sites$'`.

**Analog (descriptors and registry):** `core/t64_machine.c:1047-1133` (macro body `T64_SI_FAMILY_COMMON_FIELDS` ending `.descriptor_version = 12u, .machine_model_version = 20u, .state_serialization_version = 25u, ...`, then four `static const t64_profile_descriptor ..._profile = { .profile_id = "...", .save_kind = T64_SAVE_NONE|T64_SAVE_EEPROM_4K|T64_SAVE_EEPROM_16K|T64_SAVE_SRAM_32K, T64_SI_FAMILY_COMMON_FIELDS };`), then lookup by `bounded_text_equal` (`:2210-2260` pattern) and id helpers (`:1285-1345`):

```c
static bool has_si_family_profile_id(const t64_profile_descriptor *profile) {
  return has_si_profile_id(profile) || has_si_e4k_profile_id(profile) ||
         has_si_e16k_profile_id(profile) || has_si_s32k_profile_id(profile);
}
static bool is_si_profile(const t64_profile_descriptor *profile) {
  return profile != NULL && profile->descriptor_version == 12u &&
         has_si_family_profile_id(profile);
}
```

`is_si_profile` hardcodes `descriptor_version == 12u`: it (and `is_ai_family_profile`, the create-time sidecar allocation at `t64_machine.c:2672-2681`, `has_pinned_openipl3_profile_id`, `is_rdram_b_t1_profile`, `profile_fields_are_valid`, `write_profile`) must learn the new family. The authoritative site list is `docs/spec/profile-membership-sites.txt` (82 lines; regenerate with `python3 -B scripts/list-membership-sites.py --print`, verified by `repo.si-membership-sites`, `runner/CMakeLists.txt:208-212`). Id length cap is `T64_PROFILE_TEXT_CAPACITY` 64 (`t64_machine.h:15`). The doc comment above `T64_SI_FAMILY_COMMON_FIELDS` ("Provisional until Phase 10; no API/ABI freeze") is a stale label to relabel in the same commit (C6 says code comments go to Phase 11 because they move the build id; the descriptor macro is in `t64_machine.c`, so leave it).

---

### Wave 2: `core/t64_digest.c` v26 walker (service, streaming)

**Analog:** itself. Domain-constant + exact predicate chain (`:270-360`):

```c
static const uint8_t domain_v25[] = "t64.state.si.25";           /* add domain_v26 next to it */
const bool has_v21_state = t64_has_rdp_depth_profile(machine);
const bool has_v25_state = t64_has_si_profile(machine);          /* becomes the EXACT v25 predicate */
...
if (has_v25_state && machine->rdp_memory_coverage == NULL) { return T64_STATUS_BAD_STORAGE; }
if (has_v25_state) {
  digest_bytes_class(sink, domain_v25, sizeof domain_v25 - 1u, T64_DIGEST_CLASS_IDENTITY);
} else if (has_v24_state) { ... }
```

Add `has_v26_state` first in the chain. Existing depth block at `:705-719`, already inside the RDP section and before `work_policy_version` (this position is required: the importer's triangle proof needs the scalars before the tail):

```c
if (has_v21_state) {
  hash_u32(sink, rdp->depth_image_address);
  hash_u32(sink, rdp->depth_image_valid ? 1u : 0u);
  hash_u32(sink, rdp->primitive_z);
  hash_u32(sink, rdp->primitive_dz);
  hash_u32(sink, rdp->primitive_depth_valid ? 1u : 0u);
  hash_u32(sink, (uint32_t)T64_RDP_DEPTH_HIDDEN_BYTES(machine->profile.rdram_bytes));
  digest_bytes(sink, machine->rdp_depth_hidden, T64_RDP_DEPTH_HIDDEN_BYTES(machine->profile.rdram_bytes));
}
```

For v26 the five scalars stay here; the hidden table moves to the tail class (research Pattern 2, A3). Use `digest_bytes_class(..., T64_DIGEST_CLASS_NEW)` (sink API at `t64_digest.c:19-50`, `hash_u32_new`/`hash_u64_new` at `:84`/`:88`) so the strip-and-compare against v25 stays possible.

---

### Wave 2: `core/t64_state.c` importer/exporter for v26

**Analog:** itself.
- Registry (`:2721-2725`): `state_version_registered` returns `version == STATE_VERSION_REGISTERED && t64_has_si_profile(machine) && ...state_serialization_version == version`; make it a per-version row (25 with the exact v25 predicate, 26 with the exact v26 predicate), so an unregistered version reports `T64_STATE_REASON_STATE_VERSION`.
- Render-proof hazard (`:1686-1688`): `triangle.view.depth_address = 0u; triangle.view.depth_valid = false; triangle.view.primitive_depth_valid = false;` then `state_check_triangle_render(r, &triangle)`. For v26 read the depth scalars first and feed them in.
- Commit zeroing (`:2995-3030`): `memset(machine->rdp_depth_hidden, 0, T64_RDP_DEPTH_HIDDEN_BYTES(...))` and `machine->rdp.depth_image_address = 0u;` become blob-driven for v26 and stay zeroed for v25.
- Rule-reader idiom: each new member gets a `STATE_CHECK(r, cond, T64_STATE_REASON_*, T64_STATE_RULE_*)` with named rules (`ADDR_RANGE`, `ENUM_CLOSED`, `BOOL_01`), a row in `docs/spec/state-field-rules.txt`, and a structure-aware mutant row in `docs/spec/state-mutants.txt`.

`docs/spec/state-field-rules.txt` row format (header lines 1-14): `member :: RULE :: reader_function`, e.g. existing `depth_image_address :: PROFILE_DERIVED :: state_commit` (line 92); `docs/spec/state-mutants.txt` format `name :: status :: reason :: section id :: rule` (header lines 1-20; the C mutant table of `test_state_import.c` must match it name by name). `harness.state-field-use-audit` fails on any unmapped used member.

Container version: `T64_STATE_CONTAINER_VERSION UINT32_C(1)` (`t64_internal.h:1873`), public string `T64_STATE_CONTAINER_FORMAT "t64.state-container.v1"` (`t64_machine.h:410`). Size doc to rewrite in `docs/spec/state-container.md:25-45` (header table row `0x008 | 4 | container version, 1`) and `:160-180` (measured sizes, "below 11,534,336": invalid for v26 with retained packets, new bound about 12,582,912; the same literal asserted at `core/tests/test_state_export.c:720` and `runner/tests/test_state_runner.c:855`).

---

### Wave 2: `core/tests/test_digest_v26.c` (test, transform) - completeness flip test written FIRST (RED)

**Analog:** `core/tests/test_digest_v25.c:1055-1328`. Copy-paste is the accepted dependency philosophy (decided in the research).

Member table, size assertion and flip helper to reuse verbatim:

```c
typedef struct { const char *name; size_t offset; size_t size; const char *excluded; } rdp_member;
#define M(name)  {#name, offsetof(t64_rdp_state, name), sizeof(((t64_rdp_state *)0)->name), NULL}
#define MX(name, reason) {#name, offsetof(t64_rdp_state, name), sizeof(((t64_rdp_state *)0)->name), reason}
...
M(color_image_address), MX(depth_image_address, RDP_DEPTH_REASON),     /* v25: excluded (K1) */
MX(primitive_z, RDP_DEPTH_REASON), MX(primitive_dz, RDP_DEPTH_REASON),
...
MX(depth_image_valid, RDP_DEPTH_REASON), MX(primitive_depth_valid, RDP_DEPTH_REASON),
#define RDP_STATE_SIZE 5144u
_Static_assert(sizeof(t64_rdp_state) == RDP_STATE_SIZE, "t64_rdp_state changed: add the new member to rdp_members");
```

For v26: the five `MX(..., RDP_DEPTH_REASON)` rows become `M(...)` (in-stream), ids list becomes the four new ids, and add flips of `machine->rdp_depth_hidden` bytes (first, a middle, last) next to the existing `machine->rdp_memory_coverage + at` flips (`:1278-1290`). `check_byte_flip` (`:1194-1212`) flips one bit, compares the full stream via `capture_into`/`stream_equals` (`t64_digest_sink` with `buffer/capacity`), restores the byte, and prints `digest completeness: <name> is not in the canonical stream`. Keep the final "one flip tied to the SHA-256 digest" block (`:1315-1322`). RED-first means committing this test against a build whose walker still treats depth as excluded, and showing the failure lines, before the walker change.

Other Phase 9 patterns in the same file to reuse: `case_sink_legacy_identity` literal-golden table (`idle_goldens[]`, `:145-150`, hex digests captured BEFORE the edit; idle states are build-independent), `check_digest_refuses_missing_coverage_table` (`:1336-1360`, accepted baseline then refusal then restore), and the three-case `cases[]`/`main` (`:1800-1836`). Anti-pattern (C4): do not capture non-idle digests literals from one tree (build id leaks into packet-bearing digests); only idle literals are stable.

---

### Wave 2: `core/tests/test_rdp_depth_composite.c` (test) - depth on the four new ids, independent literals

**Analog:** `core/tests/test_rdp_depth.c` lines 11-131 (helpers), 178-228 (accepted effect incl. raw VI capture at `capture_raw_vi`, 133-176), 315-452 (all Z triangle opcodes), 663-764 (compare/update modes), 940-1120 (refusal, truncation, recovery), 1122-1210 (sidecar invalidation and reset). Difference: `create_machine_for_profile(profile_id)` there loads `rdp-fill.z64` via `T64_TEST_RDP_FILL_PATH`; the new ids require the openIPL3 asset path (`T64_TEST_IPL3_PATH`, as `create_admitted` in `test_digest_v25.c`) or an admitted authored ROM accepted by `ROM_min/max` of the family (`rom_min_bytes = 4096u`). Copy the accepted-effect + wrong-value-control form: the same setup is run twice, expected literals changed in one byte of the second, and the divergence is required at that byte (`test_vi.c:643-683` idiom). Include the `same-setup-wrong-pixel` control name pattern from the research.

---

### Wave 2: `core/tests/test_state_import_v26.c` (test, file-I/O) and `fuzz_state_import.c`

**Analog:** `core/tests/test_state_import.c:3104-3275` (B17 and carried RDP members), reusing `core/tests/state_import_helpers.h` (static-inline allocator with counters, `read_openipl3_asset`, container offsets `D_TABLE`, `D_ENTRY`, `E_OFF`, `E_LEN`, `E_ID`, `rd32`/`rd64`, re-signing helper; includes `SI_STEM_ID`).

Setup block to copy (a command list through the real DPC path, then advance one tick at a time until the machine is mid-render):

```c
static const uint64_t rdp_carry_setup[] = {
    UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
    UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
    ... fill, one-cycle mode, primitive/env/fog/blend/key colors ...,
    UINT64_C(0xfc55566c8877ffff)};/* combine mode */
static const uint64_t rdp_carry_triangle[] = { UINT64_C(0x08000014000c0004), ... };
static t64_machine *build_b17(void) {
  ...
  rdp_submit(machine, words, RDP_CARRY_COUNT + 4u);
  for (i = 0u; i < 3000u && !(machine->rdp.render_active && machine->rdp.render_triangle &&
                              machine->rdp.render_y > machine->rdp.render_y_begin); i++) {
    T64_CHECK(advance_to(machine, machine->tick + 1u));
  }
  return machine;
}
```

For v26 add a Z-triangle variant (opcodes `0x09/0x0b/0x0d/0x0f`, packet literals in `test_rdp_depth.c:975-1010`) preceded by `SetZImage 0x3e00000000006000` / `SetPrimDepth 0x2e...` / mode `0x2f...34`, and a mid-scan-out case (video packets: `capture_raw_vi` shape from `test_rdp_depth.c:133-176`). Round trip = export, import into a fresh machine, compare `t64_machine_digest`, then advance both and compare again (see `case_roundtrip` and `case_continuation_roundtrip`, `test_state_import.c:107, 277`). Hostile rows per new member go through the mutant table (`state-mutants.txt`). CTest cost: unit.core.state.import is 48 s native, 210 s hosted macOS and 1,149-1,441 s under ASan (Pitfall 6), so register the v26 import coverage as its own CTest name with a sampled matrix, with env like `unit.core.state.import` (`core/CMakeLists.txt:841-846`: `LABELS unit`, `TIMEOUT`, `ENVIRONMENT "T64_TEST_IPL3_PATH=...;T64_TEST_RDP_FILL_PATH=...;T64_STATE_MUTANT_INVENTORY=${CMAKE_SOURCE_DIR}/docs/spec/state-mutants.txt"`) and compare TIMEOUT against max(native, ASan, hosted) before the first push (F-108).

`fuzz_state_import.c` has no `main` (header comment, lines 1-15); it exposes `state_fuzz_set_windows(...)` and the campaign function that `test_state_import.c` calls with an iteration count from env (`T64_STATE_FUZZ_ITERATIONS`, set for ASan in `core/CMakeLists.txt:1100-1101`). Extend its window list (`section_position`, `fuzz_section`) for the new members; do not add a `main`.

---

### Wave 3: authored composite guest `fixtures/composite/*.S` + `*.ld` (guest, event-driven)

**Analogs (compose from these, same pinned conventions):**
- `fixtures/rdp/fill.S`: MI mask clear (`mfc0/mtc0 $12`), RDP command copy to uncached RDRAM, DP mask `0x0800`, DPC START/END, 4x3 VI window, wait loop, ISViewer transcript idiom (`lui $t0, 0xb3ff` ... `sh $t6, 0x16($t0)`), final `break`. `.rodata` holds `rdp_commands` and `hello_text`.
- `fixtures/audio/pcm.S`: AI enable + DACRATE 1103 + BITRATE 15 + two 16-byte DMAs and the `wait_for_audio` poll on `AI_STATUS`; note this guest uses a 12x7 window which the runner admits only for `T64_REAL_AUDIO_PROFILE_ID` (C2); the composite guest must use the legacy 4x3 / 48-byte video shape and two 16-byte audio packets, or `runner/t64_real.c` must change (avoid).
- `fixtures/save/sram.S` / `roundtrip.S`: PI DMA macro against the SRAM window (`PI_DMA dram, length_reg`, lines 85-100), `.globl t64_save_completion` followed by `break` (the builder looks up this symbol, `sram.S:189-193`), `empty_break` fallback.
- SI poll: `fixtures/pad/poll.S` / `fixtures/replay/mix.S` (six polls, input script `fixtures/replay/mix.input.json`).
- Linker script (identical across fixtures, `fixtures/audio/pcm.ld`):

```ld
OUTPUT_ARCH(mips)
ENTRY(_start)
__t64_rom_clock = 0x0000000f;
__t64_rom_crc2 = 0x00000000;
SECTIONS { . = 0x80000400; .text : { KEEP(*(.text.start)) *(.text*) } .rodata : { *(.rodata*) } }
```

Wrong-value variant convention: `.ifdef T64_SAVE_WRONG` / `T64_WRONG_SAMPLE` via `--defsym NAME=1` (`build_variant` passes defsyms). Header-comment habit of the save fixtures: name the printed lines, state "Editing this file re-pins every row", and state values are software-profile policy, not hardware measurement.

---

### Wave 3: `scripts/build-composite-fixture.sh` (script, batch)

**Analog:** `scripts/build-save-fixtures.sh` (221 lines; itself a copy of the pad, admit and audio builders). Copy whole, change only: the provenance comment (lines 2-8), `SAVE_STEMS` (line 33), `source_content_sha256()` file list (lines 87-94), the `build_variant` invocations in `run_build` (lines 167-190), the metadata schema string (`t64.phase09-save-fixture-build.v1`, line ~143), the staging dir name, and the completion symbol if different. Keep verbatim: `ASSET_SHA256`, `PAYLOAD_SHA256`, `BINUTILS_SHA256` pins, `verify_prerequisites()` (tool version pins, packer source hashes), `--build|--qualify` modes, `die()`, `sha256_stream`. `scripts/check-test-inventory.sh --selftest` and `sh -n` in `gate.sh` (`sh_n` over `scripts/*.sh`) apply to the new script automatically.

**CMake fixture target:** `core/CMakeLists.txt:305-338`:

```cmake
set(T64_PHASE09_SAVE_STEMS ... phase09-save-roundtrip-s32k)
set(T64_PHASE09_SAVE_OUTPUTS)
foreach(T64_SAVE_STEM IN LISTS T64_PHASE09_SAVE_STEMS)
  list(APPEND T64_PHASE09_SAVE_OUTPUTS
    "${T64_BOOT_FIXTURE_DIR}/${T64_SAVE_STEM}.z64" ".elf" ".payload.bin" ".json")
endforeach()
add_custom_command(OUTPUT ${T64_PHASE09_SAVE_OUTPUTS}
  COMMAND "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}"
    sh "${CMAKE_SOURCE_DIR}/scripts/build-save-fixtures.sh" --build
  DEPENDS "${CMAKE_SOURCE_DIR}/fixtures/manifest.json" ".../fixtures/save/*.S|.ld" ".../scripts/build-save-fixtures.sh"
    ".../tools/fixture/t64_rom_pack.c" ".../tools/common/t64_sha256.c" ".../tools/common/t64_sha256.h"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" COMMENT "..." VERBATIM)
add_custom_target(t64-phase09-save-fixture DEPENDS ${T64_PHASE09_SAVE_OUTPUTS})
add_dependencies(t64-phase09-save-fixture t64-boot-fixtures)
```

Runner side adds `add_dependencies(<fixture target> t64-rdp-fill-fixture)` lines (`runner/CMakeLists.txt:617-621`), the pattern for build ordering.

---

### Wave 3: `fixtures/manifest.json` rows (config) and `runner/t64_real_fixture.c` rows (model)

**Analog row (manifest):** `phase09-save-roundtrip-s32k` (id, `source{repo,revision,path}`, `build{recipe,toolchain}`, `sha256`, `size`, `profile`, `source_rights: "authored-here"`, `binary_rights: "cleared"`, `distribution: ["build-only"]`, `rights_evidence`, `notices[]`, `expected{kind: "self-checking", source, revision: "source-content-sha256:<64 hex>"}`). Add a `wrong-` control row beside it (`phase09-sram-rw-wrong-s32k-v1` shows the "same source, one defsym" recipe text and the `Wrong-byte ROM exists only to prove the real comparator is reached.` notice). `phase09-*` rows are F2-pinned by `scripts/tests/test_phase09_qualification.py:516-672`; name the new rows `phase10-*` and never touch existing ones. After editing the manifest run `ctest -R '^harness\.rdp-oracle-authority$'` at once (a rebind is an orchestrator task until GATE-02 lands).

**Analog rows (runner admission table, `runner/t64_real_fixture.c:142-153`; columns per `runner/t64_real_fixture.h:21-32`: id, sha256, size, profile_id, profile_sha256, completion_pc, pass_eligible, guest_tick_cap, expected_log_bytes, expected_log_sha256):**

```c
    {"phase09-save-roundtrip-s32k",
     "ec744b26449cafbc73fb5c0e8ebaceb58573cfd224bb8e30adae361927f977eb",
     5000u, "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k",
     "52c47d606e2a0b4dce7a743327082c505ea6f062648cf37349f6e50ca398e87a",
     UINT32_C(0x800006a4), true, 0u, 144u,
     "649b62940ace25d9e4c31e12381f38df779a5bbef90dcf0baf1be1458e6b75c4"},
```

A long guest sets `T64_REAL_GUEST_TICKS_LONG_CAP` in the cap column (rows 94-141). The profile hash literal for each new id comes from `t64_profile_hash` after Wave 2 registration. `t64_real_fixture_profile_allowed()` (`:183-190`) admits a profile only if some row names it, so a row is mandatory for the new id.

---

### Wave 3: `runner/tests/test_composite_two_run.c` (test, request-response) - two supervised runs, equal `state_digest`

**Analog 1 (argv builder, both run kinds):** `runner/tests/test_save_roundtrip.c:223-345`: `run_spec` already carries `raw_capture_dir`, `artifact_dir`, `artifact_max`, `leaf`, import trio and `video_fd/audio_fd`; `arg_add_pair(&list, "--raw-capture-dir", spec->raw_capture_dir)`, `arg_add_pair(&list, "--artifact-dir", ...)`, `"--artifact-max-bytes"`, `"--save-export-leaf"`, `"--save-kind"`, `"--save-format"`; fixed head `--real --result --meta --work-dir --wall-ms 30000 [--bound] --fixture --input --capture-out`. Case table and `main` (`:1860-1915`): `--list-cases`, `--case NAME SUPERVISOR TEST_SUPERVISOR CHECKER ROOT RUNNER`, per-case `g_failures` reset, `case %s: ok|FAILED` stderr line. Evidence dir: `prepare_evidence()` (`:82-100`, env `T64_PHASE09_EVIDENCE_DIR`, `mkdtemp`, `t64_test_scope_open`), `make_run_dir`, `fixture_path(stem)` -> `<root>/build/fixtures/<stem>.z64`.

**Analog 2 (two runs with equal digest, bundle equality):** `runner/tests/test_audio_capture.c:1689-1722` (`run_capture_resume_case`): loop over `run`, call `run_supervised(...)` with a per-run `digest[run]`, then `strcmp(digest[0], digest[1]) != 0` -> failure message; `result_is_pass` (`:377-416`) parses `outcome == "pass"`, `profile_id`, `profile_digest`, `fixture_id`, `fixture_sha256`, and copies the 64-hex `state_digest` out with `t64_json_get`. RUN-07 flow: run 1 = `--raw-capture-dir` (no artifact options), run 2 = `--artifact-dir/--artifact-max-bytes/--save-export-leaf` (RAW with PAIR or SAVE-OUT is refused by design, so these are two separate supervisor invocations); then compare `state_digest` of the two result records and each output against its own independent literal (4x3 video packet 48 bytes, two 16-byte S16BE audio packets, exact SRAM bytes), with a wrong-value control in `controls.real.*` style (test_audio_capture.c:477-486 `controls.real.audio-wrong-sample`, a separate ROM variant through the same comparator).

**CMake registration (copy `runner/CMakeLists.txt:692-713`):**

```cmake
add_executable(test_save_roundtrip tests/test_save_roundtrip.c)
target_compile_definitions(test_save_roundtrip PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_save_roundtrip PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core/include)
target_link_libraries(test_save_roundtrip PRIVATE t64_common t64_core)
t64_target_warnings(test_save_roundtrip)
add_dependencies(test_save_roundtrip t64-supervisor t64-supervisor-test
  t64-result-check t64-runner-real t64-boot-fixtures t64-phase09-save-fixture)
add_test(NAME runner.save-roundtrip
  COMMAND test_save_roundtrip
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
    $<TARGET_FILE:t64-result-check> ${CMAKE_SOURCE_DIR}
    $<TARGET_FILE:t64-runner-real>)
set_tests_properties(runner.save-roundtrip PROPERTIES
  LABELS "runner;controls" TIMEOUT 300
  ENVIRONMENT "T64_PHASE09_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/09-05-evidence")
```

Use `t64-composite-fixture` (new) in place of `t64-phase09-save-fixture` and a `10-NN-evidence` directory. For per-case CTest names on one binary, use the `t64_phase09_runner_case(TEST_NAME CASE_NAME)` function at `runner/CMakeLists.txt:574-582`.

Do NOT touch `runner/t64_real.c` unless the guest shape cannot match the legacy observation constants (`drain_video` requires 4x3/48/24 bytes for non-AI ids, `:594-603`; `drain_audio` requires exactly the 16-byte packet and at most two packets, `:742-748`). `scripts/check-rdp-oracle-authority.py` does not bind `runner/tests/*` other than `test_rdp_video.c`; this new file is free.

---

### Docs (W2): K1 / depth-silent text, state-container v2, build-identity statement, provisional-name sweep

No code analog. Concrete sites already located:
- State blobs "build-identity-bound and outside the ABI promise": `docs/spec/state-container.md` privacy section (after line 177) and header table row 25-45; grep target for the preflight: `grep -n "build-identity-bound" docs/spec/state-container.md`.
- K1 text: `docs/spec/machine-contract.md:322`, `:402`, and the table row `:965`; `docs/spec/rdp.md` V7 (lines 443-507; add the Z exponent table and the primary citations here so the repo doc is self-contained).
- "Provisional until Phase 10" labels (live spec docs only; leave closed-phase records `docs/claims/phase09.md:32` and `docs/evidence/phase09-qualification.md:276`, and leave code/header comments to Phase 11): `docs/spec/raw-capture.md:132`, `docs/spec/result-protocol.md:546,554`, `docs/spec/machine-contract.md:609,701,829` (plus 487, 529 per C6), `docs/spec/replay.md:11`, `docs/spec/state-container.md:15,258,626`, `docs/spec/si-pif-controller.md:250`, `docs/spec/save-media.md:30`.

---

## Shared Patterns

### Pin-impact checks after every wave (cheap, seconds)
**Source:** 10-RESEARCH.md "Pin-impact map"; commands verified against `runner/CMakeLists.txt`.
**Apply to:** every task commit.
```sh
ctest --test-dir build/gate -R '^harness\.rdp-(oracle-authority|vi-qualification)$' --output-on-failure
ctest --test-dir build/gate -R '^repo\.si-membership-sites$|^repo\.ci-yml(-selftest)?$'
python3 -B scripts/check-rsp-no-hle.py --build build/gate
sh scripts/check-test-inventory.sh --preset gate
```

### Do not add cases to Phase 7 or Phase 9 test binaries (extends research C1)
**Source:** `scripts/check-phase09-cases.py:236-258`, `docs/spec/phase09-cases.json`, `scripts/check-rdp-oracle-authority.py:37-52`, `scripts/qualify-rdp-vi.py:28`.
**Apply to:** all wave 1-3 tests.
`check-phase09-cases.py` runs each listed binary with `--list-cases` and requires an exact list match with `docs/spec/phase09-cases.json` (F2-pinned `inventory_sha256` in `scripts/tests/test_phase09_qualification.py`). The inventory names these binaries: `core/test_si_profile` (4 cases), `core/test_digest_v25` (3), `core/test_sched_ledger`, `core/test_si`, `core/test_joybus`, `core/test_save`, `core/test_save_transfer`, `core/test_sched_matrix`, `core/test_state_export`, `core/test_state_import` (28), `runner/test_phase09_runner`, `runner/test_artifact_publisher`, `runner/test_pad_poll`, `runner/test_save_fixtures`, `runner/test_save_roundtrip`, `runner/test_replay`, `runner/test_state_runner`, `runner/check_state_field_use`. The hosted CI step runs `--suite hosted` (only `runner/test_pad_poll`, 5 cases; `ci.yml:37,91`), so a new case there fails the hosted job; other suites feed the closed native qualification. Therefore the wave 2 predicate-matrix rows for the four new ids, the v26 digest cases and the v26 import cases belong in NEW binaries (`test_digest_v26.c`, `test_state_import_v26.c`, a new matrix case file), not in `test_si_profile.c`, `test_digest_v25.c` or `test_state_import.c`, unless the planner records an inventory edit and its pin consequences. (`test_si_profile.c` `family_rows`/`FAMILY_ROW_COUNT == 4` and `T64_CHECK((family_rows[index].expected & P_DEPTH) == 0u)` also stay true for the v25 ids and must stay so: that is the K1 characterization.)

### Authored-evidence discipline
**Source:** `core/tests/test_rdp_depth.c:178-228`, `runner/tests/test_save_roundtrip.c:1-18`.
**Apply to:** all new tests and fixtures.
Expected values are hex literals written from the primary documents or an independent oracle script (`fixtures/save/roundtrip_oracle.py`, `fixtures/replay/mix_oracle.py`, `fixtures/pad/poll_oracle.py` are the committed twin-oracle precedents), never taken from a production helper; every accepted case has a same-setup wrong-value control; policy-only claims are labeled "software policy, not silicon".

### Registered-id conventions and one-way pins
**Source:** `core/t64_machine.c:1047-1133`, `core/tests/test_si_profile.c:244-259` (`si_ids`, `si_save_kinds`, `si_id_lengths = {38u, 42u, 43u, 43u}`), `runner/t64_real_fixture.c`.
**Apply to:** wave 2 and wave 3.
Four ids share one body and differ only in `profile_id` and `save_kind` (`T64_SAVE_NONE`, `T64_SAVE_EEPROM_4K`, `T64_SAVE_EEPROM_16K`, `T64_SAVE_SRAM_32K`). The research A1 proposes the stem `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2` with the same `-e4k`, `-e16k`, `-s32k` suffixes (lengths 38, 42, 43, 43 fit the 64-character cap). Record the chosen ids in the plan BEFORE the first digest pin; they are one-way.

### Error/refusal-leaves-state-unchanged convention
**Source:** `core/tests/test_si_profile.c:261-300` (`counting_allocate`/`counting_release` with `allocations`, `outstanding`, `release_errors`), `fuzz_state_import.c` (refusal leaves the whole machine allocation byte identical, allocator counters unchanged).
**Apply to:** any new create/import refusal case (for example `t64_machine_create` of descriptor 13 under an unregistered id must refuse without allocating).

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `.planning/phases/10-.../PIN-IMPACT.md` | doc | n/a | A planning table; the seed table is in 10-RESEARCH.md "Pin-impact map" (11 rows). |
| 7 provisional-name docs | doc | n/a | Mechanical relabel; site list in the Docs block above. |
| Per-row oracle pin shape (`bound_rows: [{id, row_sha256}]`) | checker | transform | Closest is the existing per-row rights check (`check-rdp-oracle-authority.py:380-396`); the new request JSON schema has no precedent and is an orchestrator-only design. |
| Cold-cache measured run (`gh cache delete`, `gh workflow run ci.yml`) | procedure | n/a | `scripts/ci-review.sh RUN_ID --append docs/ci-reviews/LOG.md` records results, but the cold-run procedure itself is a manual orchestrator step. |

## Metadata

**Analog search scope:** `core/`, `core/tests/`, `core/include/`, `runner/`, `runner/tests/`, `scripts/`, `scripts/tests/`, `fixtures/`, `docs/spec/`, `docs/ci-reviews/`, `.github/workflows/`, `tests/`.
**Files scanned/read:** about 30 (full or targeted ranges; large files located with grep first).
**Tracked-source check:** `git ls-files` returned a tracked entry for every named analog (core, runner, scripts, fixtures, docs, CMake files); no `.gsd`/capability mirror path is used.
**Pattern extraction date:** 2026-11-23
