# Phase 11: Installed Consumer and Freeze Candidate - Pattern Map

**Mapped:** 2026-11-25 (tree at commit a47bd8c, clean)
**Files analyzed:** 32 new or modified file groups (from 11-CONTEXT.md decisions and 11-RESEARCH.md "Recommended structure", "Public API shape plan", "Risks and ordering hazards")
**Analogs found:** 29 / 32 (rows 12, 20 and 31 have no in-repo analog: the CMake package config template, the standalone consumer CMake project, the optional `.pc` smoke; row 13 is a partial analog; inside row 6 the `T64_API` / `extern "C"` block also has none; row 31 has no assignment section because it is optional and has nothing to copy)
**Read-only:** no source file was touched; the nine oracle-bound test files and the oracle record are cited only as the objects of the orchestrator rebind.
**Tracked-source gate:** every analog path below was checked with `git ls-files` (all tracked; none is a gitignored mirror or a `build/` path).
**Verified by me this session (scratchpad only):** `sh tools/lint/core-lint.sh --files <header>` exits 0 on `core/include/tendies64/t64_machine.h`, also from a path with a space, and exits 1 with `L1-INCLUDE-BANNED` on a seeded header including `<stdio.h>` (see "Shared Patterns / Installed header scan").

## File Classification

Wave names follow RESEARCH "Risks and ordering hazards": W0 = preflight, pin map, decisions; SHAPE = production edit wave (types and handshake, poison/reset/unload, prefix and sha fold, relabel, codemod, rebind); INSTALL = package, consumer, controls, cheap-first checks; FREEZE = goldens, qualifier, claims.

| # | New / Modified file | Wave | Role | Data flow | Closest analog | Match |
|---|---------------------|------|------|-----------|----------------|-------|
| 1 | `.planning/phases/11-.../PIN-IMPACT.md` (NEW) | W0 | doc (pin map, wave bases, gate-growth ledger) | n/a | `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` | exact |
| 2 | `scripts/preflight-phase11.sh` (NEW) | W0 | script | batch | `scripts/preflight-phase10.sh` | exact (copy plus deltas) |
| 3 | `scripts/preflight-phase11-regexes.txt` (NEW) | W0 | config (data) | batch | `scripts/preflight-phase10-regexes.txt` | exact |
| 4 | `scripts/check-test-inventory.sh` (EDIT, `PHASE11_TESTS` + `ALL`) | W0 | script (exact name list) | transform | itself, `PHASE10_TESTS` 242-274 and 377 | exact (self) |
| 5 | `tools/digest-sweep/t64_digest_sweep.c` (EDIT, two-API shim) | W0 | tool (printer) | batch | itself 67-90, 193-217 | exact (self) |
| 6 | `core/include/tendies64/t64_machine.h` (EDIT) | SHAPE | public header | request-response | itself (audio info `struct_size` 327-342, status block 73-112, doc comments 640-651) | exact (self) |
| 7 | `core/t64_machine.c` (+ `t64_observe.c`, `t64_scheduler.c`, `t64_cp0.c`, `t64_state.c`, `t64_digest.c` defect sites) (EDIT) | SHAPE | service (public entry layer) | request-response | itself: `t64_machine_create` 2641-2650, `t64_machine_advance` 2988-2997, `reset_guest_state` 2292-2349, `t64_machine_admit_rom` 2964-2985 | exact (self) |
| 8 | `core/t64_internal.h` (EDIT, `poisoned`/`in_call`, prefix include, `t64i_test_*` hooks) | SHAPE | model | n/a | itself 1483-1491 (hook precedent), `docs/spec/machine-contract.md:957+` (member audit rows) | exact (self) |
| 9 | `core/softfloat/s_*.c` x9 (EDIT) + `provenance/copies.json` (EDIT) | SHAPE | vendored source + provenance record | n/a | `core/softfloat/s_countLeadingZeros32.c:42-46` and its `copies.json` row | exact |
| 10 | `core/t64_prefix.h` (NEW, generated) + its generator/regenerate-and-diff check | SHAPE | generated config header | batch | `scripts/list-membership-sites.py` (`--print`/`--check`) + `docs/spec/profile-membership-sites.txt` | role-match |
| 11 | `core/CMakeLists.txt` (EDIT: sha fold, install/export, alias, `T64_API` define, new test registrations) | SHAPE/INSTALL | config | n/a | itself 102-118, 815-832, 1018-1035, 1049-1058, 1139-1142 | exact (self) |
| 12 | `core/tendies64Config.cmake.in` (NEW) | INSTALL | config template | n/a | none in repo | no analog |
| 13 | Call-site codemod: ~157 `t64_machine_create(` lines, 145 positional + 35 compound-literal + 16 field-wise `t64_stop_request` sites, ~130 allocator initializers, over `core/tests`, `runner`, `runner/tests`, `tools/digest-sweep` | SHAPE | mechanical edit | transform | real call sites listed in assignment 13 | partial |
| 14 | Orchestrator rebind: nine bound test files (isolated commit), `docs/evidence/phase07-oracle-authority.json`, `.md`, `scripts/check-rdp-oracle-authority.py:23` | SHAPE | evidence record + checker pin | transform | commits `b64098f`, `15fd8d1` | exact |
| 15 | `core/tests/test_api_*.c` (NEW: handshake, struct_size, reset/unload, poison, output_status, class table) | SHAPE | test | request-response | `core/tests/test_lifecycle.c` + `core/tests/test_k1_refusal.c` (`--case` main) | exact |
| 16 | Layout pins that move with the public structs: `core/tests/test_digest_v26.c:830-926`, `runner/t64_audio_observation.h:80-100`, `runner/t64_raw_capture.c:829`, `runner/t64_real.c:742`, `core/t64_ai.c:536`, `core/t64_digest.c:1000`, `core/t64_state.c:2092` | SHAPE | test + serialization | transform | themselves | exact (self) |
| 17 | Stale-text relabel (20 production lines + `t64_facade.h:6` + banners + `docs/adr/0002...:55,91`) | SHAPE | doc/comment edit | transform | Phase 10 `PIN-IMPACT.md` section 12 (relabel table and gate) | exact |
| 18 | `docs/adr/0002-language-build-and-abi.md`, `docs/adr/0006-determinism-lint-mechanism.md` (amendments) | SHAPE/FREEZE | doc (ADR) | n/a | ADR-0002 "Amendment A-2/A-3" bullets (lines 71-77), ADR-0006 "Allowlist policy" (lines 91-98) | role-match |
| 19 | `docs/spec/machine-contract.md` (EDIT: member audit rows, `t64_machine_bus_write` carve-out) and new `docs/spec/api.md` | SHAPE/FREEZE | spec doc | n/a | `machine-contract.md` "Host allocator contract" 68-106, "Replacement reset snapshot" 693-722, "F0 identity record" 630-648 | role-match |
| 20 | `tests/installed-consumer/CMakeLists.txt` (NEW, separate top-level project) | INSTALL | config | n/a | none for a standalone project; `scripts/digest-sweep.py:288-292` for the configure-a-scratch-tree driver | no analog (partial) |
| 21 | `tests/installed-consumer/{main.c,hostloop.c,sequence.c}` (NEW) | INSTALL | host client / test | request-response + streaming drain | `runner/t64_real.c` (1218-1219, 1685-1814, 902-922), `core/tests/test_lifecycle.c` (allocator ledger, snapshot, two instances), `core/tests/test_k1_refusal.c` (`--case` main) | role-match |
| 22 | `tests/installed-consumer/controls.tsv` + `scripts/check-install.sh` (NEW: install, relocate, negative controls) | INSTALL | script + control matrix | batch | `tests/controls/matrix.tsv` + `tests/controls/CMakeLists.txt` (one CTest name per row) + `tests/controls/run-case.sh` | role-match |
| 23 | `tools/abi/CMakeLists.txt` (NEW, CTest registrations only) | INSTALL | config | n/a | `tools/repocheck/CMakeLists.txt` | exact |
| 24 | `scripts/check-abi.*` and `docs/abi/{public-symbols.txt,header.sha256,layout.c,status-classes.txt}` (NEW) | INSTALL/FREEZE | checker + golden data | transform | `scripts/list-membership-sites.py` (generate/`--check`), `scripts/check-state-field-use.py` + `docs/spec/state-field-rules.txt` (closed table + seeded cases), `scripts/check-docs.sh --selftest` | exact (composed) |
| 25 | `scripts/check-api04.sh`, `tools/lint/allowlist-installed.txt` (NEW; the source `tools/lint/allowlist.txt` stays unchanged), `tools/lint/lint-objects.sh` (EDIT, `--allow FILE` option), `tools/lint/installed-seeds/*` (NEW, outside the `bad_*.c` selftest glob) (refreshed 2026-11-25 to match plan 11-17, checker I-4) | INSTALL | checker + seeds | transform | `tools/lint/lint-objects.sh`, `tools/lint/core-lint.sh` selftest 399-455, seeds `bad_time.c`, `bad_entropy.c`, `bad_file_io.c`, `bad_thread.c` | exact |
| 26 | `repo.consumer-include-guard` grep script (NEW) | INSTALL | checker | transform | `scripts/check-docs.sh` (rule ids, `--selftest`) | role-match |
| 27 | `scripts/qualify-phase11.py` + `scripts/tests/test_phase11_qualification.py` (NEW) | FREEZE | qualifier + hermetic test | batch | `scripts/qualify-phase10.py`, `scripts/tests/test_phase10_qualification.py` | exact (copy plus deltas) |
| 28 | `runner/CMakeLists.txt` (EDIT: `harness.phase11-qualification`) | FREEZE | config | n/a | itself 796-800 | exact (self) |
| 29 | `docs/claims/phase11.md`, `docs/evidence/phase11-qualification.md` (NEW) | FREEZE | claim + evidence docs | n/a | `docs/claims/phase10.md`, `docs/evidence/phase10-qualification.md`, `docs/claims/TEMPLATE.md` | exact |
| 30 | `docs/ci-reviews/LOG.md` (APPEND rows/findings) | FREEZE | append-only log | n/a | rows F-124 to F-126 | exact |
| 31 | Optional: plain `cc` line + `.pc` smoke (only if under one task) | INSTALL | script | batch | none | no analog |
| 32 | Phase 10 carry-in test additions (W2-01 Z-product parity case, D-10-17-01 cap, W2-02 membership list refresh) | SHAPE | test + generated list | transform | `core/tests/test_state_import_v26.c:1295`, `runner/tests/test_cold_boot.c:124-139`, `scripts/list-membership-sites.py` | exact |

## Pattern Assignments

### 1. `PIN-IMPACT.md` (doc, W0)

**Analog:** `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md`

Copy the section skeleton: `## 1. Pin-impact map` (table `Edit | Pin or check it moves | Cheap check`, lines 8-30), `## 5. Wave bases` (machine-read by the qualifier, lines 157-177), `## 6. Gate-growth ledger` (lines 179-220), `## 12. Provisional-name sweep` (relabel table and a one-line gate, lines 692-753). Seed section 1 from RESEARCH "Pin-impact propagation" (its table has the same three columns).

**Wave-base lines are parsed by `read_bases` and must stay in exactly this shape** (`scripts/qualify-phase10.py:198-232`; PIN-IMPACT lines 172-177):
```
- Wave 0 base: 4ac633e880e0c26005cb03092e8d8485e721034c
- Wave 1 base: f6ab8c26d008b8301b729b5aae2715e1b1079318
```
One line per label, 40 lowercase hex, never `HEAD`, a ref or a short hash (qualifier regex `- {label}: ([0-9a-f]{40})`). Phase 11 picks its own labels (suggest `Wave 0 base`, `Shape base`, `Install base`, `Qualification base`) and the qualifier's `BASE_LABELS` tuple must match them.

**Gate-growth row format** (lines 190-196): wave, new CTest names, measured added native seconds (from `build/gate/Testing/Temporary/CTestCostData.txt`), hosted macOS ctest delta, offset or G4-exception reason, margin to 900 s. Phase 11 starts from the measured cold run `731 s, margin 169 s` (ledger row "Cold run (plan 10-19...)"). ROADMAP line 76: "The exception covers Phase 10 only; Phases 11 and 12 follow G4 as written", so a Phase 11 row must name an offset or a recorded Phase 11 G4 exception (CONTEXT "Gate budget").

**Stale-text gate** (Phase 10 section 12 used a one-line `python3 -c` gate that prints `stale N`). Phase 11 equivalent, measured by RESEARCH: `git grep -n -i -E 'until (the )?Phase 10' -- core runner tools` must print nothing, plus the broader `git grep -n -E 'Phase[ -]?10' -- core runner tools` with an allowlist for the Phase 10 composite names (`phase10-composite-*`, `T64_PHASE10_*`), because the stated grep misses `t64_facade.h:6` and ADR-0002 lines 55 and 91.

---

### 2. `scripts/preflight-phase11.sh` (script, batch)

**Analog:** `scripts/preflight-phase10.sh` (512 lines). Copy whole, then apply the delta list below. Keep POSIX sh, `set -eu`, `LC_ALL=C`.

**Step runner with whole-second accounting** (lines 97-115, keep verbatim):
```sh
step() {
  _pf_name=$1
  shift
  if "$@"; then
    :
  else
    echo "[preflight] $_pf_name FAILED"
    exit 1
  fi
  _pf_now=$(date +%s)
  _pf_dt=$((_pf_now - PF_PREV))
  PF_PREV=$_pf_now
  PF_SUM=$((PF_SUM + _pf_dt))
  echo "[preflight] $_pf_name ok (${_pf_dt}s)"
  PF_STEPS="${PF_STEPS}${_pf_name} ${_pf_dt}s
"
}
```

**Label count against the inventory block** (lines 174-200): the awk reads the `PHASE10_TESTS='...'` block of `scripts/check-test-inventory.sh` and compares its name count with `ctest -N -L phase10`. Phase 11 renames to `PHASE11_TESTS` / label `phase11` and the message texts.

**Step order** (lines 491-498, then timing consistency 500-512). Cheap first; Phase 11 inserts one step after `ctest-counts`:
```sh
step oracle-authority check_oracle_authority
step ctest-counts check_ctest_counts
step host-load check_host_load
step head-ci check_head_ci
step timeout-audit check_timeout_audit
step pii check_pii
step linux-compile check_linux_compile
step post-gate-replay check_post_gate_replay
```
Deltas for Phase 11:
- `PF_WORK=build/preflight-phase11`; `REGEXES` default `scripts/preflight-phase11-regexes.txt` (line 55; keep the `T64_PREFLIGHT_REGEXES` override, it is the mismatch control).
- New step `abi-cheap` (CONTEXT "Order of work": header compile C99 pedantic and C++17, layout asserts, `cmake --install` from the existing tree, `nm` golden, object lint, absolute-path scan). Implement as the CTest set `ctest --test-dir build/gate -R '^(repo\.abi|install\.|api04\.)'`, target 15 s.
- `check_oracle_authority` (lines 126-128, `--expect-blocked`) stays: it must still pass after the Phase 11 rebind.
- `check_linux_compile` (lines 343-412): it rewrites each ninja compile command to `-o /dev/null` (line 395) and runs in the act image (`PF_ACT_IMAGE=ghcr.io/catthehacker/ubuntu:act-24.04`). The act image has no cmake or ninja (RESEARCH), so keep it compile-only; to also run `nm` and `lint-objects.sh --objects` on Linux objects, change that one `sed` line to write objects into `$PF_WORK/linux-compile/obj/` and run the lint inside the container (RESEARCH measured 17.6 s for 86 units).
- `check_post_gate_replay` anchors (`PF_REPLAY_FIRST`, `PF_REPLAY_LAST`, lines 58-60) read ci.yml run lines; Phase 11 does not edit ci.yml (CONTEXT), so the anchors stay.

---

### 3. `scripts/preflight-phase11-regexes.txt` (data)

**Analog:** `scripts/preflight-phase10-regexes.txt` lines 1-12.
```
# Row format: <expected count><TAB><regex>. Lines starting with # are comments.
# A zero row proves absence until the plan that owns the names adds them; the
# owning plan then raises the count in the same commit (AUTONOMY Phase 9
# lesson: plan regexes matched 10 names, not 11, more than once).
1	^harness\.rdp-oracle-authority$
2	^repo\.ci-yml(-selftest)?$
1	^repo\.si-membership-sites$
```
Phase 11 rows: carry the guards from RESEARCH "Sampling Rate" (`harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `repo.rsp-no-hle`) and add one row per new family: `^repo\.abi`, `^install\.`, `^installed\.consumer\.`, `^installed\.controls\.`, `^api04\.`, `^unit\.core\.api\.`, `^repo\.consumer-include-guard$`, `^harness\.phase11-qualification$`. Start new rows at 0 (the file's own "zero proves absence" rule) and raise them in the plan that registers the names.

---

### 4. `scripts/check-test-inventory.sh` (EDIT)

**Analog:** itself. The block shape (lines 242-245, 274) and the concatenation (line 377):
```sh
# Phase 10 names (carry-in closure before the freeze): every name also carries
# the extra CTest label phase10, which sh scripts/preflight-phase10.sh compares
# with this block.
PHASE10_TESTS='repo.p10-mutant-anchors
unit.core.k1.dispatch-refusal
...
harness.phase10-qualification'
```
```sh
printf '%s\n' "$BASELINE_CORE" "$BASELINE_CONTROLS" "$PHASE02_TESTS" ... "$PHASE09_TESTS" "$PHASE10_TESTS" "$_extra"
```
Add `PHASE11_TESTS='...'` (single-quoted, one name per line, closing quote on the last name line, because both `scripts/preflight-phase10.sh:176-186` and `scripts/qualify-phase10.py:297-308` parse it with `^PHASE10_TESTS='([^']*)'`) and append `"$PHASE11_TESTS"` to line 377. Every name in the block needs `LABELS "...;phase11"` at registration, and the exact count enters the selftest (about 0.2 s per name, PIN-IMPACT section 1). Verify with `sh scripts/check-test-inventory.sh --preset gate` and `--selftest`.

---

### 5. `tools/digest-sweep/t64_digest_sweep.c` (EDIT, one source for two API generations)

**Analog:** itself. The API sites to shim are three (RESEARCH counted a few; measured here):
```c
/* lines 67-68, 89-90: allocator callbacks and a positional initializer */
static bool sweep_allocate(void *context, size_t size, size_t alignment,
                           t64_host_storage *storage_out) {
...
static const t64_host_allocator allocator = {sweep_allocate, sweep_release,
                                             NULL};
/* line 200 */
  status = t64_machine_create(&allocator, profile, &machine);
/* line 216 */
      t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
```
**Constraints from the driver** (`scripts/digest-sweep.py`), all of which the shim must respect:
- `SWEEP_FLAGS` (lines 107-111) contain `-Wundef -Werror`: key the shim with `#if defined(T64_ABI_VERSION_MAJOR)` (head tree) `#else` (base tree). `#if T64_ABI_VERSION_MAJOR >= 0` fails on the base tree.
- The same source is compiled against both trees' `core/include` and `tools/common` (lines 326-334) and linked with `libt64_core.a` plus `libt64_sha256.a`, so the printer keeps using the unprefixed `t64_sha256_*` names (the fold renames them only inside `t64_core`).
- `force_build_id` (lines 176-185) requires the literal line `set(T64_BUILD_ID "t64-build-v1-${T64_BUILD_FINGERPRINT}")` exactly once in `core/CMakeLists.txt` (`core/CMakeLists.txt:66`); no install edit may duplicate or reword it.
- The driver builds only `t64_core t64_sha256` (lines 290-292): install rules and the alias must configure without other targets built.
- Selftest: `python3 -B scripts/digest-sweep.py --selftest` (91 s).

---

### 6. `core/include/tendies64/t64_machine.h` (public header, request-response)

**Analog:** itself. Four in-file precedents to copy.

**Status constants: append, never renumber** (lines 73-76 and 112; convention `UINT32_C`, not an enum):
```c
#define T64_STATUS_OK UINT32_C(0)
#define T64_STATUS_INVALID_ARGUMENT UINT32_C(1)
...
#define T64_STATUS_STATE_INVARIANT UINT32_C(33)
```
New statuses (ABI mismatch, poisoned, the defect status for AMBIG-B, any new refusal) go after 33 with the same spelling; stop reasons stay 1 to 8 (lines 146-154). The class table (`docs/abi/status-classes.txt`, assignment 24) lists each constant once; values 6 (`UNSUPPORTED_ROM_ORDER`, never returned) and stop reason 4 (`HALT`, unreachable through the public API) stay reserved and classified with a note.

**The only existing `struct_size` (callee-filled) and what depends on it** (lines 327-342):
```c
typedef struct {
  uint32_t struct_size;
  uint64_t sequence;
  ...
  uint32_t encoding;
  size_t byte_length;
  char profile_id[T64_PROFILE_TEXT_CAPACITY];
  char build_id[128];
} t64_audio_packet_info;
```
Phase 11 reverses the direction (caller-filled `struct_size` on in/out structs). Before touching this struct read Hazard H1 below: its `struct_size` is hashed into the machine digest and serialized in the state container.

**Positional-initializable structs that the leading `struct_size` breaks** (lines 175-180 allocator, 251-254 stop request):
```c
typedef struct {
  t64_host_allocate_fn allocate;
  t64_host_release_fn release;
  void *context;
} t64_host_allocator;
typedef struct {
  uint32_t kind;
  uint64_t value;
} t64_stop_request;
```
Public initializer macros (CONTEXT "Public init macros") land first, e.g. `T64_STOP_REQUEST_INIT(kind, value)` and `T64_HOST_ALLOCATOR_INIT(allocate, release, context)`, so the codemod is a regex rewrite (assignment 13).

**Postcondition comment style for refusals** (lines 640-651, queue_input):
```c
 * serialized with advance. Refusals do not change the queue or sequence.
 * Submission is accepted only for an admitted, nonterminal machine and a tick
 * at or after the current guest tick.
```
and the bus-write carve-out text that AMBIG-D must make explicit (lines 655-665, `t64_machine_bus_write`: "a write that a device refuses can leave the device's own record of the refusal").

**`bool` and `size_t` inside public structs** (members at lines 166-167, 276-277, 280-281, 318-319, 339; allocator typedef 170). `size_t` to `uint64_t` is layout-neutral on LP64, which is why it is safe for the pins in Hazard H2; `bool` to a fixed-width integer must keep struct sizes where Hazard H2 pins them.

**No analog (new):** `extern "C"` guards (`git grep 'extern "C"'` and `__cplusplus` find nothing in the repo), the `T64_API` export macro, `T64_ABI_VERSION_*` constants, `t64_abi_version()`, create-info. Use RESEARCH "Pattern: struct_size prologue" (marked ASSUMED there) and the SQLite pattern named in CONTEXT.

---

### 7. `core/t64_machine.c` and neighbouring defect sites (service, request-response)

**Analog:** itself. Public entries have one uniform shape; the new handshake, poison guard and in-call guard slot in at the top of each of the 24 non-destroy machine entries (RESEARCH "Reset/unload/poison today").

**Create: clear the out parameter first, then validate** (`core/t64_machine.c:2641-2650`):
```c
  if (machine_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  *machine_out = NULL;
  if (allocator == NULL || allocator->allocate == NULL ||
      allocator->release == NULL || !profile_fields_are_valid(profile) ||
      profile->event_queue_capacity > T64_EVENT_QUEUE_CAPACITY ||
      t64_profile_hash(profile, profile_digest) != T64_STATUS_OK) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
```
The create-info check (`struct_size` floor, `abi_version` mismatch giving the new distinct status) belongs right after `*machine_out = NULL;` and before any allocation.

**A thin public wrapper is the cheapest guard site** (`core/t64_machine.c:2988-2997`, not a sealed body):
```c
t64_status t64_machine_advance(t64_machine *machine,
                               const t64_stop_request *request,
                               t64_stop_result *result_out) {
  if (machine == NULL || request == NULL || result_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  return t64_scheduler_advance(machine, request, result_out);
}
```
The `struct_size` prologue (RESEARCH `take_stop_request`) goes between the NULL test and the `has_image` test; poisoned and in-call checks go before the NULL-pointer dereference of `machine`. Every other `has_image` test (`t64_machine_content_digest`, `rom_size`, `rom_header`, `cpu_stop_detail`, `queue_input`) gives `NOT_READY` after unload for free; audit `video_info`, `audio_info`, `save_info`, `output_drain`, `bus_*`, `trace_*` in one table test.

**Destroy must work on a poisoned instance** (`core/t64_machine.c:2902-2925`): it reads only `machine->allocator` and the three storage descriptors, then releases image, video, audio and machine storage. Do not add a poison guard to it.

**Public `reset` = `reset_guest_state` on the admitted image** (`core/t64_machine.c:2292-2349`). The function clears RAM, RDP coverage and depth tables, SP memories, `cpu`, `rsp` and its observation fields, `boot`, `cache`, `guest_devices`, `output`, then the device reset calls, `output_drain_cursor`, `mi_*`, `t64_trace_reset`, and ends with
```c
  if (is_openipl3_profile(&machine->profile)) {
    return prepare_openipl3_warm_entry(machine);
  }
  if (is_vr4300_cpu_profile(&machine->profile)) {
    return prepare_openipl3_vr4300_entry(machine);
  }
  return T64_STATUS_OK;
```
Test: digest after `reset` equals digest after the first admit (RESEARCH). **Defect path to classify:** in `t64_machine_admit_rom` the image swap commits at lines 2964-2972 (`previous = machine->image_storage; ... machine->has_image = true;`) and `reset_guest_state` runs after it (`status = reset_guest_state(machine);`, then `release(previous)` at 2983): a failing `reset_guest_state` leaves a replaced image with partial state and is a defect-class path.

**Public `unload`** is the admit swap run backwards: release `image_storage` through `machine->allocator.release(machine->allocator.context, ...)` (as at line 2983), clear `has_image`, then the same reset state.

**Defect sites to reclassify** (AMBIG-A, AMBIG-B; each currently answers a caller-facing status for an internal fault):
- `core/t64_machine.c:2935-2944` admit: stored `profile_digest` no longer matches the profile bytes returns `INVALID_ARGUMENT`.
- `core/t64_cp0.c:558-580` `t64_cp0_synchronize_guest_time`: a corrupt `count_half_clock_phase` returns `INVALID_ARGUMENT`.
- `core/t64_digest.c:345,350` and `core/t64_state.c:169,214,221,335` (line numbers re-checked with `grep -n T64_STATUS_BAD_STORAGE`): `BAD_STORAGE` for internal inconsistency, against the allocator-contract uses at `core/t64_machine.c:2766,2804,2836` and `core/t64_rom.c:184` that stay refused; state export clears the first `total` buffer bytes on that status (header comment at `t64_machine.h` lines 405-409).

**Handler errors folded into `RESOURCE_LIMIT`** (`core/t64_scheduler.c:442-451` and `:508-517`, identical in both bounds):
```c
    status = dispatch_front(machine);
    dispatched++;
    if (status != T64_STATUS_OK) {
      set_stop_result(result_out,
                      machine->cpu.stopped
                          ? cpu_stop_result_reason(machine)
                          : T64_STOP_REASON_RESOURCE_LIMIT,
                      machine->tick);
      return T64_STATUS_OK;
    }
```
The poison hook intercepts only statuses outside the whitelist (CONTEXT AMBIG-C/E); proof is zero new poisons across the 1,599-line digest sweep.

**Sealed bodies:** `scripts/check-rsp-no-hle.py:244` `REVIEWED_BODY_DIGESTS` contains `t64_audio_copy`, `t64_audio_info` (`core/t64_ai.c`), `t64_video_copy`, `t64_video_info` (`core/t64_vi.c`), `t64_bus_write`, `t64_bus_write_masked`. An entry guard inside any of them re-pins; run `python3 -B scripts/check-rsp-no-hle.py --build build/gate` before and after (RESEARCH Pitfall 6, Phase 9 plan 09-01 precedent).

---

### 8. `core/t64_internal.h` (model)

**Analog:** itself, hook precedent (`core/t64_internal.h:1483-1491`):
```c
t64_status t64_test_schedule_synthetic(t64_machine *machine, uint64_t tick,
                                       uint32_t kind, uint64_t value);
t64_status t64_test_schedule_synthetic_after(t64_machine *machine,
                                             uint64_t delay, uint32_t kind,
                                             uint64_t value);
t64_status t64_test_reserve_synthetic(t64_machine *machine, uint64_t tick,
                                      uint32_t kind, uint64_t value,
                                      t64_event_reservation *reservation_out);
bool t64_test_event_handler_matches(const t64_machine *machine,
                                    size_t logical_offset);
```
A poison trigger for in-tree white-box tests follows this shape (becomes `t64i_test_*` through the prefix header). New members `poisoned` and `in_call` need the audit rows described in assignment 19 and must not appear in the digest walkers or state writer (those emit named members, never struct bytes). `scripts/check-state-field-use.py` only maps members used as index/length/shift/loop bound, so a plain flag needs no `state-field-rules.txt` row.

---

### 9. SoftFloat nine-file edit + provenance (vendored source)

**Analog:** `core/softfloat/s_countLeadingZeros32.c:42-47`. The trap the prefix header hits:
```c
#ifndef softfloat_countLeadingZeros32

#define softfloat_countLeadingZeros32 softfloat_countLeadingZeros32
#include "primitives.h"

uint_fast8_t softfloat_countLeadingZeros32(t64_softfloat_context *context, uint32_t a)
```
In each of the nine files (`softfloat_approxRecip32_1`, `_approxRecipSqrt32_1`, `_countLeadingZeros32`, `_countLeadingZeros64`, `_mul64To128`, `_shiftRightJam32`, `_shiftRightJam64`, `_shiftRightJam64Extra`, `_shortShiftRightJam64`) delete the `#ifndef`, the self-`#define` and the trailing `#endif` (27 lines; RESEARCH verified exactly 252 = 30 public + 222 `t64i_*` on Apple clang and gcc 13.3). Provenance: each file is a `mode: adapted` row of `provenance/copies.json` (57 rows); extend its `modifications` string, keep `revision` and the upstream SHA-256 text, then `sh scripts/check-provenance.sh`:
```json
 "mode": "adapted",
 "modifications": "adapt the selected helper to integer bit-carrier types, ... upstream source SHA-256: 4cc8579412c437c011f3d9795a2bbd3d4eb06349c69ce624aa169471c643f336.",
 "verification_test": "unit.core.softfloat32 and unit.core.softfloat64 (conversion/normalization)",
```
Verify with `unit.core.softfloat32`, `unit.core.softfloat64`, `unit.core.softfloat-isolation`, `unit.core.cop1-state`, `unit.core.lifecycle` (RESEARCH ran them: failures=0).

---

### 10. `core/t64_prefix.h` generated rename header + regenerate-and-diff check

**Analog:** `scripts/list-membership-sites.py` and its golden `docs/spec/profile-membership-sites.txt`. Same contract: a generator with `--print` and `--check FILE`, a generated-file banner, and a CTest that diffs.

Golden banner (`docs/spec/profile-membership-sites.txt:1-3`):
```
# Version-membership and scheduler-reservation sites.
# Generated by scripts/list-membership-sites.py --print; do not edit by hand.
# Format: path :: function :: detail
```
Mode contract (`scripts/list-membership-sites.py:11-16` docstring, `:212-242` main):
```python
    if len(args) == 2 and args[0] == "--check":
        expected_path = pathlib.Path(args[1])
        ...
        regenerated = render(root)
        if regenerated == committed:
            return 0
        sys.stdout.writelines(difflib.unified_diff(...fromfile="committed", tofile="regenerated"))
        return 1
    return usage()
```
CTest registration of such a check (`runner/CMakeLists.txt:214-218`):
```cmake
add_test(NAME repo.si-membership-sites
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/list-membership-sites.py
          --check ${CMAKE_SOURCE_DIR}/docs/spec/profile-membership-sites.txt)
set_tests_properties(repo.si-membership-sites PROPERTIES
  LABELS runner TIMEOUT 60 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
```
Generation input is the `nm` set (defined globals of the archive minus `docs/abi/public-symbols.txt`), emitted as `#define name t64i_name`. The header is `core/t64_*.h`, so the `core/CMakeLists.txt` glob at lines 12-13 hashes it into `T64_BUILD_ID`; generate it only after the last production symbol is added (RESEARCH ordering constraint). Host tools keep `t64_sha256_*`; only `t64_core` renames them (private compile definitions, RESEARCH recipe).

---

### 11. `core/CMakeLists.txt` (config, EDIT)

**Analog:** itself.

**The target that changes** (lines 102-118):
```cmake
add_library(t64_core STATIC
  ${T64_CORE_SOURCES}
  ${T64_SOFTFLOAT_SOURCES}
  ${T64_SOFTFLOAT_HEADERS})
target_include_directories(t64_core
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_SOURCE_DIR}/tools/common"
          "${CMAKE_CURRENT_SOURCE_DIR}/softfloat")
t64_target_warnings(t64_core)
...
target_link_libraries(t64_core PRIVATE t64_sha256)
target_compile_definitions(t64_core PRIVATE
  "SOFTFLOAT_FAST_INT64=1"
  "T64_BUILD_ID=\"${T64_BUILD_ID}\""
  "T64_SOURCE_REVISION=\"${T64_SOURCE_REVISION}\"")
```
Edits (RESEARCH "CMake install recipe", scratch-verified, copy that block): add `tools/common/t64_sha256.c` to the library sources, delete line 114, wrap the PUBLIC include in `$<BUILD_INTERFACE:...>`/`$<INSTALL_INTERFACE:include>`, add the four private `t64_sha256_*=t64i_sha256_*` definitions, `FILE_SET HEADERS` for `t64_machine.h`, `install(TARGETS ... EXPORT)`, `install(EXPORT ...)`, `write_basic_package_version_file(... VERSION 0.1.0 COMPATIBILITY SameMinorVersion)` and `configure_package_config_file`. **`project(VERSION)` stays `0.0.0` (`CMakeLists.txt:2`) and `PROJECT_VERSION` is part of the build manifest (line 45), so the package version must be a separate literal.** The single-config guard (lines 3-6) stays.

**New test binaries** copy the K1 registration (lines 815-832), including the `LABELS "unit;phase11"` style:
```cmake
add_executable(test_k1_refusal tests/test_k1_refusal.c)
target_include_directories(test_k1_refusal PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_k1_refusal PRIVATE t64_core t64_sha256)
t64_target_warnings(test_k1_refusal)
add_dependencies(test_k1_refusal t64-boot-fixtures)
add_test(NAME unit.core.k1.dispatch-refusal
  COMMAND test_k1_refusal --case dispatch-refusal)
set_tests_properties(unit.core.k1.dispatch-refusal
  unit.core.k1.apply-refusal PROPERTIES
  LABELS "unit;phase10"
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
```
A slow case adds `TIMEOUT` (lines 1031-1035, `TIMEOUT 300`), and the hosted-ordering `COST` (lines 1053-1058: "COST is plain decimal seconds, it orders parallel starts only"; the timeout audit in `preflight-phase10.sh:267-330` holds a name with `COST` to 2 times that COST).

**Hazard (not in RESEARCH):** `add_custom_target(...)` fixture targets are not part of `all` (`core/CMakeLists.txt:155,178,205,238,269,299,337,368`); they build only through `add_dependencies(<executable>, <fixture target>)` (e.g. `runner/CMakeLists.txt:737-739` for `test_composite_two_run`). A script-only CTest name that needs `build/fixtures/phase10-composite-s32k.z64` (the installed consumer) must hang off an executable or an `ALL` custom target that depends on `t64-phase10-composite-fixture`, otherwise `cmake --build --preset gate` does not make the ROM.

---

### 12. `core/tendies64Config.cmake.in` (NEW, no analog)

No in-repo package config exists (`git grep -l 'install(' -- '*CMakeLists.txt'` finds nothing). Use the RESEARCH recipe: `configure_package_config_file` output with `include("${CMAKE_CURRENT_LIST_DIR}/tendies64Targets.cmake")`, `set(tendies64_BUILD_ID ...)` and `set(tendies64_BUILD_TYPE ...)` for the consumer to compare with `t64_build_identity()`. `_IMPORT_PREFIX` is derived from the config file location (relocation verified including a prefix with spaces and after `mv`).

---

### 13. Call-site codemod (mechanical)

**No code analog, but the real call sites to convert, by shape** (measured at this commit with `git grep` over `core runner tools tests`):
- `t64_machine_create(`: 157 lines. 59 files in `core/tests`, 7 in `runner/tests`, one each in `tools/digest-sweep/t64_digest_sweep.c`, `runner/t64_skeleton_probe.c`, `t64_rsp_smoke.c`, `t64_rsp_diagnostic.c`, `t64_real.c`, `t64_cpu_diagnostic.c`, `t64_cold_boot.c`, `t64_base_diagnostic.c`.
- `t64_stop_request NAME = {` declaration initializers: 145, e.g. `tools/digest-sweep/t64_digest_sweep.c:216` `t64_stop_request request = {T64_STOP_GUEST_TICK, tick};`.
- `(t64_stop_request){` compound literals: 35, e.g. `runner/t64_real.c:1709-1711`:
```c
      status = t64_machine_advance(
          machine,
          &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);
```
- Uninitialized declarations filled field-wise or by assignment: 16 in 9 files (`core/tests/test_ai.c` 5, `test_rsp_fuzz.c` 3, `test_sched_matrix.c` 2, `fuzz_state_import.c`, `test_determinism.c`, `test_sched_ledger.c`, `test_si_profile.c`, `runner/tests/test_input.c`, `runner/tests/test_state_runner.c`), e.g. `core/tests/test_ai.c:352` `t64_stop_request request;`. **These compile cleanly after `struct_size` is added and then fail at run time with `struct_size == 0`.** Add a grep guard that fails on `t64_stop_request[[:space:]]+[A-Za-z_0-9]+;` outside the init-macro form.
- `t64_host_allocator NAME = {` initializers: 130 (24 of them multi-line), plus one compound literal; positional forms: `core/tests/test_k1_refusal.c:67-69`, `core/tests/test_lifecycle.c:643-646` (with a context pointer), `runner/t64_real.c:1218-1219`.
- `t64_stop_result NAME = {...}`: 215 initializers; the result struct is not in the locked `struct_size` set unless the planner adds it, so decide it before the codemod.

Order (CONTEXT): init macros first, then a codemod split by directory, compiling commits only. `-Werror` with `-Wextra` (`CMakeLists.txt:15-22`) catches positional misses but not the field-wise sites above.

---

### 14. Orchestrator-only oracle rebind (evidence record + checker pin)

**Analogs:** commit `b64098f` ("fix(10-03): per-row oracle manifest binding and identity-only rebind (orchestrator)") and `15fd8d1`. Files touched there: `docs/evidence/phase07-oracle-authority.json`, `docs/evidence/phase07-oracle-authority.md`, `scripts/check-rdp-oracle-authority.py`.

Commit-message shape to reuse (b64098f body): reproduce the old pinned hash before editing, name old and new hash, state "`local_decision` stays blocked, both trusted pins stay None, no oracle/rights/publication authority".

What changes (verified): `scripts/check-rdp-oracle-authority.py:23` `PINNED_REQUEST_SHA256 = "e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab"`; the record's `request_sha256` and `request.corpus.bound_authored_inputs[]` (13 paths, each `{path, sha256, git_blob_id_sha1}`, `EXPECTED_CORPUS_PATHS` at line 37); the verifier `_verify_corpus` (lines 295-397) refuses with `authored corpus SHA-256 changed: <path>` and `Git blob ID changed: <path>`. Ten C files are bound and nine of them call the changing API (measured here with `grep -c`; create/stop_request/allocator): `test_rdp.c` 3/1/3, `test_rdp_copy.c` 1/0/1, `test_rdp_triangles.c` 2/0/2, `test_rdp_texture.c` 1/0/1, `test_rdp_combiner.c` 1/0/1, `test_rdp_blender.c` 2/0/2, `test_rdp_depth.c` 1/0/1, `test_vi.c` 4/12/4 (and 46 uses of `t64_video_packet_info`), `test_video_output.c` 1/3/1. `runner/tests/test_rdp_video.c` uses none of the changing types, so it needs no edit (CONTEXT lists ten paths under the label "nine"; edit only the nine that use the API). The 46 `t64_video_packet_info` uses in `test_vi.c` make Hazard H2 a bound-file question too: a leading `struct_size` on that struct is a call-site edit of a bound file. Cheap check: `ctest -R '^harness\.rdp-oracle-authority$'` (0.2 s) and `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked`. CONTEXT scope: owner approval 2026-11-25 "Approve call-site-only rebind"; the bound-file edits sit in one commit placed last in the shape wave; no other bound-file edit.

---

### 15. New API unit tests `core/tests/test_api_*.c`

**Analog A (structure, allocator ledger, refusal snapshot, two instances):** `core/tests/test_lifecycle.c`.

Allocator with an outstanding/release ledger the consumer also needs (lines 137-177, trimmed):
```c
static void test_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;
  ...
      slot->active = false;
      state->release_count++;
      state->outstanding_count--;
      ...
  state->bad_release_attempts++;
}
```
**Refusal is a no-op** (lines 203-258, `check_machine_snapshot`): state digest, content digest and profile hash before and after, then allocator counters:
```c
  T64_CHECK_INT(t64_machine_digest(machine, state_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_content_digest(machine, content_before),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_profile_hash(profile, profile_before), T64_STATUS_OK);
  allocator->next_mode = next_mode;
  T64_CHECK_INT(t64_machine_admit_rom(machine, input, input_length),
                expected_status);
  T64_CHECK_INT(t64_machine_digest(machine, state_after), T64_STATUS_OK);
  T64_CHECK(memcmp(state_before, state_after, T64_DIGEST_SIZE) == 0);
  ...
  T64_CHECK_INT(allocator->outstanding_count,
                outstanding_before + expected_allocation_delta -
                    expected_release_delta);
```
**Two instances** (`test_two_instances_have_disjoint_storage`, lines 654-713) and **teardown assertions** (lines 646-651):
```c
  t64_machine_destroy(machine);
  T64_CHECK_INT(allocator.outstanding_count, 0u);
  T64_CHECK_INT(allocator.release_count, allocator.allocation_count);
  T64_CHECK_INT(allocator.bad_release_attempts, 0u);
```
**Analog B (CTest names via `--case`, summary line):** `core/tests/test_k1_refusal.c:462-499`:
```c
int main(int argc, char **argv) {
  static const char *const cases[] = {"dispatch-refusal", "apply-refusal"};
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) { ... return 0; }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "dispatch-refusal") == 0) {
      case_dispatch_refusal();
      return T64_TEST_FINISH("unit.core.k1.dispatch-refusal");
    }
    ...
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
```
and the harness header `tests/t64_test.h` (`T64_CHECK`, `T64_CHECK_INT`, `T64_TEST_FINISH(name)` prints `name: checks=N failures=M`, exits nonzero when `N == 0`).

**Includes:** public-API cases use `#include "tendies64/t64_machine.h"` only (`test_lifecycle.c:1-10`); white-box poison cases include `"t64_internal.h"` as `test_k1_refusal.c` does. Put new tests in new files with new CTest names; do not add cases to a binary listed in `docs/spec/phase09-cases.json` or `docs/spec/rdp-vi-cases.json` (Phase 10 standing consequence, PIN-IMPACT section 1).

---

### 16. Layout pins that move with the public structs

Read these before changing any public struct (see Hazards H1, H2):
- `core/t64_ai.c:536` `info.struct_size = (uint32_t)sizeof info;`
- `core/t64_digest.c:1000` `hash_u32(sink, info->struct_size);` (inside the audio packet loop)
- `core/t64_state.c:2092` `info.struct_size = state_rd32(r);`
- `runner/t64_audio_observation.h:80-100`:
```c
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
_Static_assert(sizeof(t64_audio_packet_info) == 280u,
               "audio packet info layout is part of observation version 2");
_Static_assert(sizeof(t64_audio_observation) == 3976u, ...
```
- `runner/t64_raw_capture.c:829` and `runner/t64_real.c:742`: `info->struct_size != (uint32_t)sizeof *info` refuses a packet.
- `core/tests/test_digest_v26.c:921-926`:
```c
#define VI_STATE_SIZE 1208u
_Static_assert(sizeof(t64_vi_state) == VI_STATE_SIZE, ...
#define VIDEO_PACKET_INFO_SIZE 488u
_Static_assert(sizeof(t64_video_packet_info) == VIDEO_PACKET_INFO_SIZE, ...
```
with the member-row macros `VMT`/`VMTX` (lines 860-874: "All 37 members of one retained packet slot") that a new member must join.

---

### 17. Stale-text relabel

**Analog:** Phase 10 plan 10-16, `PIN-IMPACT.md` section 12 (table of site, old phrase, new phrase, plus a measured gate). Replacement phrase used there: "provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12)". Phase 11 sites (RESEARCH "Verified tree facts", measured): `t64_machine.h` 12 lines (30, 93, 98, 102, 120, 135, 238, 389, 428, 564, 599, 646), `t64_internal.h:1891`, `t64_machine.c:1045`, `t64_state.c:10,391`, `runner/t64_artifact_publisher.h:13`, `runner/t64_replay.h:6`, `runner/t64_save_flags.h:11`, `runner/tests/test_state_runner.c:2211`, plus `t64_facade.h:6` and the banners at `t64_machine.h:1-4` and `t64_facade.h:1-34`, plus `docs/adr/0002-language-build-and-abi.md:55,91`. Stays untouched by rule: `docs/claims/phase09.md:32`, `docs/evidence/phase09-qualification.md:276`, `fixtures/state/decode_container.py`. All of these edit production text, so they ride in the shape wave (they move `T64_BUILD_ID`).

---

### 18. ADR amendments (`0002`, `0006`)

**Analog:** the bullet-style amendment already used inside ADR-0002's Decision Outcome (lines 71-77, "Amendment A-2 ...", "Amendment A-3 ...") and the ADR-0006 allowlist policy list (lines 91-98):
```
Allowlist policy (`tools/lint/allowlist.txt`, rows `name|why|ADR/date|os`):

- It starts with `memcpy memmove memset memcmp` plus the Darwin-only `bzero`.
- `malloc` is never allowed: core memory comes from host callbacks.
- Every new row needs an ADR reference and the operating system it applies to.
- A compiler-runtime helper needs an ADR-backed per-architecture row, and the preferred fix is to remove the construct.
```
ADR-0006 amendment content (CONTEXT "Stack protector"): allowlist exactly `__stack_chk_fail` and `__stack_chk_guard` for the installed-archive check (compiler runtime, no host service, abort only); the lint's own compile stays `-fno-stack-protector`; Linux needs `_GLOBAL_OFFSET_TABLE_` and aarch64 `__stack_chk_guard` (RESEARCH). ADR-0002 amendment: lines 54-55 ("The facade header is provisional ... the API freeze is Phase 10") and line 91 ("Bad: the facade is provisional ...before Phase 10") relabeled; the reversal-window sentence at line 163 ("effectively closed once the C API is frozen and consumed in v1.2") gets a dated note that the candidate is bound. `scripts/check-docs.sh` (rules `DOC-ADR-STRUCT`, `DOC-STATUS-MISMATCH`) requires the heading order and a non-empty "Reversal evidence"; run `sh scripts/check-docs.sh` and `--selftest` after the edit. Front matter and the `**Status:**` line must keep agreeing.

---

### 19. `docs/spec/api.md` (NEW) and `docs/spec/machine-contract.md` (EDIT)

**Analogs inside `docs/spec/machine-contract.md`:**
- Postcondition prose with a "refused leaves X unchanged" contract: "Host allocator contract" (lines 68-106) and the state-import paragraph ("a refused import leaves the machine bit-identical", lines 100-105).
- Audit tables that a new member or operation must be added to: "Replacement reset snapshot" (lines 693-722; `| State | Present in this slice? | Replacement result |`, closing sentence "Refusal assertions compare every present machine observation..."), and the "State member audit" (line 957+; `| Member | Section | Reset by replacement | Reason |`, member list "has 68 members"). Add `poisoned` and `in_call` rows with section "excluded" and reason class "flag outside digest and state container"; update the member count sentence.
- A frozen-record voice: "F0 identity record" (lines 630-648): "This record freezes ... A later change is a recorded deviation with its cost, not a silent edit." Use that voice for the additive rule text of `api.md`.

`api.md` content (RESEARCH "Freeze-candidate record"): additive rule (new trailing struct fields guarded by `struct_size`, new statuses appended after the last value, stop-reason and status numbers never reused), the status-class table pointer, per-function NULL-machine behavior (`INVALID_ARGUMENT`, outputs untouched), the `t64_machine_bus_write` carve-out (AMBIG-D), `RESOURCE_LIMIT` wording (AMBIG-C/E), threading statement, `docs/abi/` golden list and the re-bind command. `scripts/check-docs.sh` does not constrain `docs/spec`.

---

### 20. `tests/installed-consumer/CMakeLists.txt` (standalone project, no analog)

**Closest in-repo analog for the driver, not the project:** `scripts/digest-sweep.py:288-292`, which configures and builds a scratch tree with explicit generator and build type:
```python
    done = run(["cmake", "-S", source, "-B", build, "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", "-DT64_WERROR=ON"], ROOT)
    if done.returncode == 0:
        done = run(["cmake", "--build", build, "--target", "t64_core",
                    "t64_sha256", "--parallel", "4"], ROOT)
```
The consumer project itself follows RESEARCH: `find_package(tendies64 0.1 REQUIRED CONFIG)` and `target_link_libraries(consumer PRIVATE tendies64::core)`, `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` for the visible-source-tree control. **It must live outside the `tools/*/CMakeLists.txt` glob** (`CMakeLists.txt:28-36` adds every `tools/*/CMakeLists.txt` except `common`) and must not be `add_subdirectory`'d (RESEARCH "Recommended structure"). The consumer must not include `t64_test.h`, `t64_sha256.h` or any core or runner file (CONTEXT: grep guard), so it carries its own tiny check macros and its own SHA-256-free comparisons (compare bytes to hard-coded literals).

---

### 21. `tests/installed-consumer/{main.c,hostloop.c,sequence.c}` (host client)

**Analog (public-header-only client, the host loop):** `runner/t64_real.c`. Allocator callbacks (lines 304-325):
```c
static bool allocator_allocate(void *context, size_t size, size_t alignment,
                               t64_host_storage *storage_out) {
  void *data = NULL;
  size_t host_alignment = alignment;
  (void)context;
  if (alignment < sizeof(void *)) {
    host_alignment = sizeof(void *);
  }
  if (storage_out == NULL || size == 0u || alignment == 0u ||
      posix_memalign(&data, host_alignment, size) != 0) {
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = host_alignment;
  return true;
}
```
(For the consumer use the counting allocator of `core/tests/test_lifecycle.c:79-177` with a per-instance context, so two instances stay disjoint and release counts are checked.)

**Bounded advance with drain in host order and a resumable `RESOURCE_LIMIT`** (`runner/t64_real.c:1685-1814`, trimmed to the control flow):
```c
  while (current_tick < guest_ticks) {
    uint64_t target = guest_ticks - current_tick > advance_step
                          ? current_tick + advance_step : guest_ticks;
    for (;;) {
      status = t64_machine_advance(machine,
          &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);
      if (status != T64_STATUS_OK || stop.tick < current_tick ||
          stop.tick > target) { /* harness error */ }
      if (!drain_trace(machine, &observations)) ...
      if (!drain_output(machine, output, &output_length, ...)) ...
      if (!drain_audio(machine, &observations, raw_capture)) ...
      if (!drain_video(machine, &observations, video_scratch, ...)) ...
      current_tick = stop.tick;
      if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
        const bool made_progress = stop.tick > before_tick || ...;
        resource_yields++;
        if (resource_yields > T64_REAL_MAX_RESOURCE_YIELDS || !made_progress) { /* harness error */ }
        continue;
      }
      break;
    }
```
The "stall with no progress after a full drain is permanent" wording (AMBIG-C/E) is exactly the `!made_progress` bound. Drain primitives: `t64_video_info` then `t64_video_copy` (lines 585-624), `t64_audio_copy` (726, 749), `t64_machine_output_drain` (lines 885-922, with the `capacity`/`copied_bytes` accounting). The loop must also queue the two composite input events at the stamped ticks (`runner/t64_real.c:1653-1669`; literals in `fixtures/composite/composite.input.json`: ticks `564000` and `582000`).

**Composite expectation to compare against (independent literals):** `fixtures/composite/composite.expected.json` (video 4x3 RGBA8 48 bytes `00ff00ff...`; audio two 16-byte packets `000040002000e0007fff80010123fedd` and `1111eeef800000015a5aa5a6ffff0000`; save SHA-256 `f0b7fe880aeed453233a0b07ab62301420967a959bd1d43e9dd49e23d3e6926d`; transcript 188 bytes SHA-256 `da07d916f0314db05e8d609326242d7f944098e96ce19a96339ef1cd6a7aad39`; polled `00000000`, `080114ec`, `44109c64`). The in-tree analog that loads and compares these values is `runner/tests/test_composite_two_run.c` (lines 418-470: "This file never derives an expected value from a runner or core output"); the consumer hard-codes the literals in C and a separate CTest (or a Python check in the controls script) proves the C literals equal the JSON file (the `harness.composite-oracle` precedent, `runner/CMakeLists.txt:758-762`). Final state digest from PIN-IMPACT 11.3 / RESEARCH: `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c`, completion at tick 599,357.

**Call-sequence generator (`sequence.c`):** the snapshot-before/after contract is `check_machine_snapshot` (assignment 15); the observation set is RESEARCH "Observations that exist" (digest, content digest, state export hash, `t64_video_info`, `t64_audio_info`, `t64_machine_save_info`, `t64_machine_cpu_stop_detail`, `t64_machine_rom_size`, `t64_machine_rom_header`, the new `output_status`, consumer allocator counters). Constant-seed LCG lives in the consumer only (the core owns no PRNG). Entry style of `main` with `--case` selection as in `test_k1_refusal.c:462-499`.

---

### 22. `tests/installed-consumer/controls.tsv` + `scripts/check-install.sh` (negative controls)

**Analog:** `tests/controls/` (the negative-control matrix of the result protocol). One TSV row per control with the *exact* expected outcome (never only "not pass"), and one CTest name per row generated by a CMake `foreach` over the file.

Matrix header and a row (`tests/controls/matrix.tsv:1-2`):
```
#id	script	expect_pass	bound	wall_ms	slice_a	slice_b	fixture_mode	fixture_sha256	outcome	detail	exit	runner_flags	log_limit
positive	positive.t64s	2	1000	5000	1	7	ok	05e9...25a	pass	positive-match	0	-	65536
```
Registration loop (`tests/controls/CMakeLists.txt:1-17`):
```cmake
file(STRINGS "${T64_MATRIX}" T64_MATRIX_LINES)
foreach(t64_line IN LISTS T64_MATRIX_LINES)
  if(t64_line STREQUAL "" OR t64_line MATCHES "^#")
    continue()
  endif()
  string(REPLACE "\t" ";" t64_cols "${t64_line}")
  list(GET t64_cols 0 t64_id)
  add_test(NAME controls.${t64_id}
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/run-case.sh
            --bin-dir $<TARGET_FILE_DIR:t64-supervisor> --case ${t64_id})
  set_tests_properties(controls.${t64_id} PROPERTIES LABELS controls)
endforeach()
```
Phase 11 rows (`installed.controls.<id>`), each asserting a specific failure text or status: `missing-package`, `version-mismatch` (`find_package(tendies64 0.2)` gives "The version found is not compatible with the version requested"), `visible-source-tree`, `doctored-header` (wrong `T64_ABI_VERSION_*` or shrunk struct, expects the ABI-mismatch status or the v1-minimum refusal at create), `symbol-collision` (consumer defines `f32_add` and `t64_sha256_init`; link succeeds, digest and results unchanged; also run once against an unprefixed build to show it fails), `relocated-prefix-with-spaces`, `seeded-object` (assignment 25). Driver structure from `tests/controls/run-case.sh:1-40` (usage line, `field()` helper reading the row, exit codes 0/1/2). Scratch-tree handling: use a `mktemp -d` work dir with `trap 'rm -rf "$TMP"' EXIT` as in `tools/lint/lint-objects.sh:79-81`.

---

### 23. `tools/abi/CMakeLists.txt` (CTest registrations only)

**Analog:** `tools/repocheck/CMakeLists.txt` (picked up by the root glob at `CMakeLists.txt:28-36`; adds no target). It also does not move `T64_BUILD_ID` (the manifest hashes `core/` inputs, the two root files and `tools/common/*`, `core/CMakeLists.txt:20-32`):
```cmake
add_test(NAME repo.docs
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-docs.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME repo.docs-selftest
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-docs.sh --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
...
set_tests_properties(repo.docs repo.docs-selftest repo.ci-yml repo.ci-yml-selftest
  repo.spike-decision-selftest
  PROPERTIES LABELS repo)
```
Phase 11 names follow RESEARCH "Phase Requirements to Test Map": `repo.abi-header`, `repo.abi-layout`, `repo.abi-status-classes`, `repo.abi-freeze-record`, `repo.consumer-include-guard`, `install.*`, `api04.*`, `installed.consumer.{ops,composite,hostloop,sequence}`, `installed.controls.*`. Labels `"repo;phase11"` etc. The CTest set needs a built `build/gate` tree (`cmake --install` and `ar x` read `libt64_core.a`), so give them `DEPENDS`-free CTest names but ensure `t64_core` is in the default build (it is).

---

### 24. ABI goldens and their checker (`docs/abi/*`, `scripts/check-abi.*`)

**Analog 1 (generate, `--print`, `--check`, committed golden):** `scripts/list-membership-sites.py` (assignment 10).

**Analog 2 (closed table + seeded self-test cases on temporary copies):** `scripts/check-state-field-use.py` with `docs/spec/state-field-rules.txt`. The table is parsed from a row format declared in the file header (`docs/spec/state-field-rules.txt:1-14`: `member :: RULE :: reader_function`). A Phase 11 `docs/abi/status-classes.txt` row is `NAME :: VALUE :: CLASS :: note` (proposed; classes `ok`, `refused`, `guest-stop`, `defect`, `reserved`), and the checker proves: every `#define T64_STATUS_*` and `T64_STOP_REASON_*` of the installed header appears exactly once; no row without a constant; an unclassified constant fails. Entry-point and case structure to copy (`scripts/check-state-field-use.py:681-690, 860-916`):
```python
SELFTEST_CASES = (
    "baseline-clean",
    "new-indexed-member-unmapped",
    ...
)
...
    if args == ["--list-cases"]:
        for name in SELFTEST_CASES:
            print(name)
        return 0
    ...
    if len(args) == 2 and args[0] == "--case":
        ...
        problem = run_case(args[1], ROOT)
        if problem is None:
            print(f"selftest {args[1]}: ok")
            return 0
        print(f"selftest {args[1]}: FAILED: {problem}")
        return 1
```
Each case copies the needed files into `build/<tmp>/`, applies one seeded breakage (`run_case`, lines 744-849) and expects its own finding text. Phase 11 seeded cases: unclassified new status constant, duplicated row, renumbered value, removed row, golden symbol list drift, header hash drift.

**Analog 3 (shell checker with rule ids and `--selftest`/`--root`):** `scripts/check-docs.sh:1-20` (header with rule-id table, exit codes 0/1/2) and its helpers `expect_fail RULE DESCRIPTION`, `expect_absent`, `expect_clean` (lines 243-275); use it for the `nm` and header-compile pieces if they stay in sh.

**Golden `nm` command** (RESEARCH, verified on macOS and Linux):
```sh
nm -P -g --defined-only "$LIB" | awk -v os="$(uname -s)" 'NF>=3 && $2 ~ /^[A-Z]$/ {n=$1; if (os=="Darwin") sub(/^_/,"",n); print n}' | LC_ALL=C sort -u
```
compare with `docs/abi/public-symbols.txt` and assert every other name starts with `t64i_`. The same `nm -P` and Darwin underscore strip is already in `tools/lint/lint-objects.sh:172-180`.

**`layout.c`:** `_Static_assert` precedents for size/offset pins in tests: `core/tests/test_digest_v26.c:830-926`, `runner/t64_audio_observation.h:84-100`. Compile it three ways (source headers, installed headers, `-std=c99 -pedantic -Werror`; and as C++17 for the guard check).

---

### 25. API-04 object checks (`scripts/check-api04.sh`, lint edits, seeds)

**Analog:** `tools/lint/lint-objects.sh` (202 lines). `--objects OBJ...` already computes undefined minus defined across the whole set; the usage contract (lines 24-27):
```
#   lint-objects.sh [-I DIR|-D MACRO]... [--] FILE.c...   compile, then check
#   lint-objects.sh --objects OBJ...             check precompiled objects only
# Output: "RULE-ID path: message (hint: ...)" per violation. Exit 0 clean,
# 1 violations, 2 usage or tool error. Include paths must not contain spaces.
```
The set subtraction (lines 190-199):
```sh
  comm -23 "$TMP/und.$_i.s" "$TMP/def.all.s" > "$TMP/miss1.$_i"
  comm -23 "$TMP/miss1.$_i" "$TMP/allow.sorted" > "$TMP/miss2.$_i"
  ...
    viol L2-UNDEF-SYMBOL "$_name" "undefined symbol '$_sym' is neither defined by the core nor on tools/lint/allowlist.txt"
```
**Chosen by plan 11-17 (refresh 2026-11-25):** the `--allow FILE` option with a separate `tools/lint/allowlist-installed.txt` of exactly three rows (`__stack_chk_fail` and `__stack_chk_guard` on every platform, `_GLOBAL_OFFSET_TABLE_` on ELF/Linux only, each with platform and reason, per the revised CONTEXT "Stack protector" decision), and seeds in `tools/lint/installed-seeds/`; the options below are the original analysis. **Gap:** `ALLOW="$LINT_DIR/allowlist.txt"` is hard-coded at line 35. Either append the two `__stack_chk_*` rows (and the ELF `_GLOBAL_OFFSET_TABLE_`, aarch64 `__stack_chk_guard` rows with the OS column, line format `name|why|ADR/date|os`) to the shared file, or add a one-line `--allow FILE` option to the argument loop (lines 57-66) for a separate `installed` allowlist. The first widens the allowlist for the per-file source lint too; the lint compile stays `-fno-stack-protector` so those names never appear there. The post-allowlist undefined set must also be compared to a recorded expected set (RESEARCH "Flags") so a new import fails even when allowlisted names change.

Allowlist row format (`tools/lint/allowlist.txt:6-11`):
```
memcpy|compilers emit calls to the mem* block functions|ADR-0006 2026-11-09|any
bzero|Apple clang rewrites memset(p,0,n) to bzero; Darwin only|ADR-0006 2026-11-09|Darwin
```
**Seeded failing objects:** copy the seed shape (`tools/lint/selftest/bad_time.c`, `bad_entropy.c`, `bad_file_io.c`, `bad_thread.c`):
```c
/* EXPECT: L1-INCLUDE-BANNED L1-NAME-TIME L2-UNDEF-SYMBOL */
#include <time.h>

long t64_seed_now(void) { return (long)time(0); }
```
**Hazard:** `tools/lint/core-lint.sh:411` runs every `tools/lint/selftest/bad_*.c` and demands a first-line `/* EXPECT: ... */`; a new `bad_installed_*.c` is therefore also a source-lint seed (it must report the L1 ids it contains, and the coverage gate at lines 457-485 checks every EXPECT id is a rule in `rules.txt`). A seed that must only exist as an *object* appended to the extracted members (four imports `clock_gettime`, `fopen`, `getentropy`, `pthread_create`, compiled with `-fno-stack-protector`) should either carry a valid EXPECT line or live outside the `bad_*.c` glob.

**Installed header include scan:** see Shared Patterns, "Installed header scan".

**Disassembly scan** (no in-repo analog; RESEARCH ran it): `objdump -d --no-show-raw-insn` over the extracted members, grep for `mrs xN, CNTVCT/CNTPCT/RNDR`, `rdtsc[p]`, `rdrand`, `rdseed`, `rdpmc`; seeded `mrs x0, cntvct_el0` object must print `mrs x0, CNTVCT_EL0`. Mirror the Layer-1 ban of host-state intrinsics (ADR-0006, "Host-state intrinsics ... Layer 1 bans them by name prefix").

**Member extraction:** `ar x libt64_core.a` into a space-free temp dir (RESEARCH open question 6: `lint-objects.sh` documents that `-I` paths must not contain spaces; `--objects` paths were only tried without spaces).

---

### 26. `repo.consumer-include-guard`

**Analog:** the rule-id plus `--selftest` structure of `scripts/check-docs.sh` (lines 1-20, 243-275). The rule: every `#include` in `tests/installed-consumer/**` of a repo path must be `tendies64/...` (or a system header); fail on `core/`, `runner/`, `tools/`, `t64_internal.h`, `t64_test.h`, `t64_sha256.h`, quoted relative includes that leave the directory. Seed: a temp copy with one offending include must exit 1 with the rule id.

---

### 27. `scripts/qualify-phase11.py` and `scripts/tests/test_phase11_qualification.py`

**Analog:** `scripts/qualify-phase10.py` (968 lines) and `scripts/tests/test_phase10_qualification.py`. Copy, then rename and re-derive the tables. Skeleton to keep:

Constants block (`qualify-phase10.py:62-72`):
```python
ROOT = Path(__file__).resolve().parents[1]
PRIVATE_SUBPATH = "build/gate/10-qualification-private"
PIN_IMPACT_SUBPATH = ".planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md"
INVENTORY_SUBPATH = "scripts/check-test-inventory.sh"
SCHEMA = "t64.phase10-qualification"
SCHEMA_VERSION = 1
RECORD_NAME = "record.json"
TIER = "unit-tested, authored scope"
CLAIMS = {"hardware_accuracy": False, "oracle_comparison": False}
REHEARSAL_STATEMENT = "this result is a rehearsal and not final evidence"
```
Bound tool list and artifact identities (lines 94-112): `TOOL_PATHS` lists the stage entry points bound even when unchanged; `ARTIFACTS` pairs a name, a path and a build-id regex (`CORE_BUILD_RE = rb"t64-build-v1-[0-9a-f]{64}"`, line 74), read from the binary with `artifact_identity` (lines 344-356). Phase 11 adds the installed archive and the installed consumer as artifacts, so the record binds the identity of what was actually installed and run.

Inventory binding (lines 297-339): `phase10_tests()` parses `^PHASE10_TESTS='([^']*)'`; `label_names()` runs `ctest -N -L phase10`; `inventory_snapshot()` requires `sorted(names) == labelled`. Rename to `phase11`.

Stage table (lines 361-476): `_command(argv, limit, measured, *, expect, marker, check, env)` and `_stage(name, derivation, commands, artifacts)`; each stage states its measured seconds and a limit of at least 2x (kill deadline), enforced by the hermetic test (`test_stage_table_shape`). Phase 11 stages: ci-scanner, inventory, oracle (still blocked: `--expect-blocked` exit 0 and `--require-approved` exit 2, lines 432-442), abi-cheap, install-and-relocate, installed consumer (composite, hostloop, sequence), controls, api04, gate, asan consumer (library and consumer instrumented), TSan two-thread (local), sweep against the Phase 11 Wave 0 base with `check="sweep"` and marker `^sweep: [0-9]+ shared lines` (lines 470-475; `SWEEP_SHARED_LINES = 1599`, line 122).

Evidence tier stays `unit-tested, authored scope`; CLAIMS keep `hardware_accuracy` and `oracle_comparison` false (a software-interface claim only, CONTEXT "Evidence tier").

Entry points (`main`, lines 937-967): `--native [--rehearsal]`, `--verify RECORD [--rehearsal]`, errors as `qualify-phase10: refused original evidence: ...` exit 1.

Hermetic test (`test_phase10_qualification.py:836-857`):
```python
def main() -> int:
    args = sys.argv[1:]
    if args == ["--list-cases"]:
        print("\n".join(CASE_NAMES))
        return 0
    if args == ["--selftest"]:
        ...
    if len(args) == 2 and args[0] == "--case" and args[1] in CASE_NAMES:
```
Its 22 `CASE_NAMES` (lines 38-61: bases refuse head/short/duplicate/missing, snapshot binds delta and tools, dirty tree refuses, native and verify accepted, verify refuses changed input / rehearsal as final / failed stage / missing stage, tier and claims pinned, stage table shape, real stage commands start, no undefined global names) are the cases to keep. It runs in scratch git repos with stub stage tables, never a nested CTest (F-113).

---

### 28. `runner/CMakeLists.txt` (EDIT, qualifier hermetic test)

**Analog:** itself, lines 796-800:
```cmake
add_test(NAME harness.phase10-qualification
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_phase10_qualification.py
    --selftest)
set_tests_properties(harness.phase10-qualification PROPERTIES
  LABELS "harness;qualification;phase10" TIMEOUT 120)
```
The comment above it (lines 790-795) is the TIMEOUT derivation style the preflight audit enforces (F-108: TIMEOUT at least 3.5 times the recorded native seconds, or 2 times a declared `COST`). Other script-test registrations to copy: `repo.p10-mutant-anchors` (lines 231-234, `LABELS "runner;phase10" TIMEOUT 60 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"`), `harness.composite-oracle` (lines 758-762), `repo.rsp-no-hle` with `COST 48` (lines 182-190).

---

### 29. `docs/claims/phase11.md`, `docs/evidence/phase11-qualification.md`

**Analogs:** `docs/claims/phase10.md` (330 lines) and `docs/evidence/phase10-qualification.md` (438 lines); format authority `docs/claims/TEMPLATE.md`.

Claim block (checked by `sh scripts/check-claims.sh`, tier from the closed list in TEMPLATE.md; Phase 10 used `unit-tested`):
````
```t64-claim
build_id: 0.0.0+t64-build-v1-b6c298e6...36884
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v2
tier: unit-tested
fixture_set: phase10-depth-composite
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 3
total: 3
guest_fail: 0
boot_fail: 0
timeout: 0
...
```
````
Key points to copy from the Phase 10 file: the paragraph before each block states the counting unit and names the CTest names counted; `pass` plus the other counts equal `total`; each block binds one profile id and one fixture set; "Limits that apply to every block" paragraph; the "Wording rule" (lines 43-44): the three reserved words (`accurate`, `compatible`, `supports`) stay out of the file, and no prose line starts with the project name followed by a space. The Phase 11 claims are software-interface claims (candidate bound record, installed consumer agreement), tier `unit-tested`, with `not_claimed` covering compatibility, accuracy, a final ABI, a release, a tag, any package channel, any oracle comparison. The statement line is generated byte for byte from the other keys (TEMPLATE.md "The rendered sentence").

Evidence document skeleton (`phase10-qualification.md:1-60`): opening paragraph naming the audited source commit and the head, an "Evidence tier" paragraph, a "Source and build identity" table (`Item | Value` with the 40-hex commits, build ids, manifest digest, composite oracle and expectation hashes), then "Native record" with the stage table (`Stage | Seconds | Limit | Observed`). Add the freeze-candidate record hashes (header SHA-256, public symbol count, layout table hash, status-class table hash) and the installed-archive build id. Closed records `docs/claims/phase10.md` and `docs/evidence/phase10-qualification.md` stay as history; they cite `b6c298e6...`, which will no longer be the build id.

---

### 30. `docs/ci-reviews/LOG.md` (append-only)

**Analog:** rows `F-124` (cold run), `F-125` (flaky attempts), `F-126` (warm pair) at the end of the Runs/findings tables. Policy (file lines 1-35): append-only (`scripts/ci-review.sh RUN_ID --append docs/ci-reviews/LOG.md`), never edit an old row; a docs-only commit that changes only this file carries `[skip ci]`; each completed run of any pushed SHA gets a row; a finding resolves as `taken` or `declined` with a reason. Phase 11 uses it for the first hosted install-from-build-tree run on Linux (UNVERIFIED until then) and for any G4 measurement.

---

### 32. Carry-in test additions (shape wave, optional per triage)

Each is "done, deferred or dropped with a recorded reason" (CONTEXT). Existing sites to extend:
- W2-01: `core/tests/test_state_import_v26.c:1295` parity case; the `state_z_products_bounded` source at `core/t64_state.c:1490`; two survivor mutants (limit `INT64_MAX / 2`, divisor `y_bound + 1`) become rows in `scripts/p10-mutants.py` style (`--check-anchors`, `--run`, KILLED/SURVIVED/INVALID, lines 1-60 docstring).
- D-10-17-01: `runner/tests/test_cold_boot.c:124-139` `make_evidence_dir` 99-attempt cap (moves a runner test identity only).
- W2-02: `python3 -B scripts/list-membership-sites.py --print > docs/spec/profile-membership-sites.txt`, then `ctest -R '^repo\.si-membership-sites$'` and `harness.phase09-qualification`.

---

## Shared Patterns

### Installed header scan (reuse the L1-INCLUDE allowlist; verified)
**Source:** `tools/lint/core-lint.sh` (`--files`, `okhdr` list at line 282: `stdint.h stddef.h stdbool.h stdalign.h limits.h string.h`; banned headers line 280).
**Apply to:** the installed headers under `$PREFIX/include/tendies64/`.
```sh
sh tools/lint/core-lint.sh --files "$PREFIX/include/tendies64/t64_machine.h"
# clean:    core-lint: files=1 violations=0            (exit 0)
# seeded <stdio.h>: L1-INCLUDE-BANNED ... banned header <stdio.h> ...  (exit 1)
```
Verified here with a prefix containing a space. No new scanner code is needed; the seeded negative is a header that includes `<stdio.h>`. Note it also applies the other Layer-1 token rules to the header text (a struct member named `time` is flagged, ADR-0006 "Consequences"), which is the existing behavior for the in-tree header.

### Script skeleton (POSIX sh) and exit codes
**Source:** `scripts/check-docs.sh:1-20`, `tools/lint/lint-objects.sh:28-42`, `scripts/gate.sh:6-8`.
```sh
set -eu
LC_ALL=C
export LC_ALL
...
TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64lint.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
trap 'exit 2' INT TERM HUP
```
**Apply to:** every new `scripts/*.sh`. Exit 0 clean, 1 violations, 2 usage or tool error. `scripts/gate.sh:60-83` (`sh_n`) runs `sh -n` over `find scripts tools tests -type f -name '*.sh'`, so every new shell script is syntax-checked by the gate with no registration.

### Python checker skeleton
**Source:** `scripts/list-membership-sites.py`, `scripts/check-state-field-use.py`, `scripts/p10-mutants.py` (docstring: modes, exit codes, "Standard library only; run with `python3 -B`").
**Apply to:** `check-abi.py`/generators, `qualify-phase11.py`. Run via `python3 -B`, CTest environment `PYTHONDONTWRITEBYTECODE=1`.

### CTest registration conventions
**Source:** `core/CMakeLists.txt:815-832, 1018-1035, 1053-1058, 1139-1142`; `runner/CMakeLists.txt:182-190, 231-234, 740-762, 796-800`; `tools/repocheck/CMakeLists.txt`; `tests/controls/CMakeLists.txt`.
**Apply to:** every new name. Rules: one binary or script, one name per case (`--case`); `LABELS "<kind>;phase11"` on every `PHASE11_TESTS` name; `TIMEOUT` by the F-108 rule (at least 3.5x the native seconds, with a comment recording the derivation); `COST` (hosted seconds) only to order starts; `ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64"` for tests that admit the openIPL3 asset; `add_dependencies(<exe> t64-boot-fixtures)` (and `t64-phase10-composite-fixture` for the composite). No nested `ctest` inside a CTest name (F-113); drive `cmake`/`ninja` and the consumer binary directly.

### Test helper header and summary line
**Source:** `tests/t64_test.h:12-62` (`T64_CHECK`, `T64_CHECK_STR`, `T64_CHECK_INT`, `T64_TEST_FINISH`).
**Apply to:** in-tree `core/tests/test_api_*.c` only. The installed consumer includes no repo file, so it reproduces the same `name: checks=N failures=M` line and the "zero checks is a failure" rule in its own few lines.

### Allocator and ownership discipline in tests
**Source:** `core/tests/test_lifecycle.c:137-177, 203-258, 646-651`; `core/tests/test_k1_refusal.c:52-69`.
**Apply to:** the consumer's allocator (per-instance context, outstanding and release counters, bad-release counter) and every refusal test. After `destroy`: `outstanding_count == 0`, `release_count == allocation_count`, `bad_release_attempts == 0`.

### Closed-set discipline for CTest names
**Source:** `scripts/check-test-inventory.sh` (exact lists, selftest rejects missing, extra, duplicate, disabled and empty inventories, lines 611-680).
**Apply to:** all Phase 11 names; add them to `PHASE11_TESTS` in the same commit that registers them, and raise the count rows of `preflight-phase11-regexes.txt` in that commit.

### Cheap-first order
**Source:** CONTEXT "Order of work" and `scripts/preflight-phase10.sh` step order.
**Apply to:** plan task order and the preflight: header compile (C99 pedantic and C++17), `_Static_assert` layout against source then installed headers, `cmake --install` from `build/gate` (`--component Development`), `nm` golden, `lint-objects.sh --objects`, absolute-path scan, then the consumer suite, then sanitizer and TSan locally.

## Hazards found in the analogs (not stated in RESEARCH or CONTEXT)

**H1. `t64_audio_packet_info.struct_size` is already part of the digest and the state container.** It is set to `sizeof` at `core/t64_ai.c:536`, hashed at `core/t64_digest.c:1000` (`hash_u32(sink, info->struct_size)`), restored at `core/t64_state.c:2092`, and checked against `sizeof` by `runner/t64_raw_capture.c:829` and `runner/t64_real.c:742`. Any change to `sizeof(t64_audio_packet_info)` moves every AI-family digest and breaks state import. The CONTEXT "before/after proof that every digest is identical" fails unless the struct keeps its size (the `size_t byte_length` to `uint64_t` change is size-neutral on LP64) or the digest hashes a named constant equal to today's value.

**H2. Size pins on public structs.** `runner/t64_audio_observation.h:84` pins `sizeof(t64_audio_packet_info) == 280u` and the 3,976-byte observation record that `scripts/qualify-audio.py` maps with ctypes (Phase 8 hosted evidence). `core/tests/test_digest_v26.c:921-926` pins `sizeof(t64_video_packet_info) == 488u` and `sizeof(t64_vi_state) == 1208u`; `t64_video_packet_info` is the retained packet slot inside `t64_vi_state`, so a leading `struct_size` on it changes `t64_vi_state` and the 37-row member sweep. Decide before the shape plans whether the retained slot or only the public copy-out struct carries `struct_size` (a separate public info struct keeps the retained layout and all pins unchanged).

**H3. Field-wise `t64_stop_request` sites are invisible to the compiler** (16 uninitialized declarations in 9 files, 35 compound literals): they compile after `struct_size` is added and then refuse at run time. A grep guard is required (assignment 13).

**H4. Sweep printer shim and `-Wundef`.** `scripts/digest-sweep.py:107-111` compiles the printer with `-Wundef -Werror` against both trees; the shim must test `defined(...)`, not a bare macro (assignment 5). The driver also forces the build id by a literal line match (`:60-61, 176-185`) and builds only `t64_core t64_sha256` (`:290-292`).

**H5. `lint-objects.sh` allowlist path is fixed** (`tools/lint/lint-objects.sh:35`), and `core-lint.sh --selftest` runs every `tools/lint/selftest/bad_*.c` (line 411) and requires an `EXPECT` first line. A new installed-object seed must satisfy both (assignment 25).

**H6. Fixture targets are not in `all`** (`core/CMakeLists.txt:155-368`): a script-only CTest name needing the composite ROM must hang off an executable or an `ALL` target (assignment 11). Without it, `cmake --build --preset gate` leaves the ROM unbuilt on a fresh runner.

**H7. Root glob.** `CMakeLists.txt:28-36` adds every `tools/*/CMakeLists.txt` (except `common`) and `tests/controls`; the consumer project must not sit under `tools/`. `tools/abi/CMakeLists.txt` is safe for registrations (assignment 23).

**H8. Public functions that mention profile tokens move `docs/spec/profile-membership-sites.txt`** (6 rows, 3 details by RESEARCH; the file has about 25 block-1 rows at this commit): regenerate and re-run `repo.si-membership-sites` in the commit that adds such a function.

**H9. Sealed public observers.** Four public functions are sealed bodies (`check-rsp-no-hle.py:244`); see assignment 7.

## No Analog Found

Files with no close match in the codebase (planner uses RESEARCH patterns instead):

| File | Role | Data flow | Reason |
|------|------|-----------|--------|
| `core/tendies64Config.cmake.in` | config template | n/a | No `install()` rule or package config exists anywhere (RESEARCH "Verified at this commit"); use the scratch-verified recipe in RESEARCH "CMake install recipe" |
| `tests/installed-consumer/CMakeLists.txt` | config (separate top-level project) | n/a | No standalone consumer project exists; `scripts/digest-sweep.py` drives scratch builds of the main project only |
| `T64_API`, `extern "C"`, `T64_ABI_VERSION_*`, create-info and `t64_abi_version()` in `t64_machine.h` | public header macros | n/a | No `__cplusplus` or `extern "C"` anywhere; no export macro; SQLite and mupen64plus patterns cited in CONTEXT and RESEARCH "Prior art re-check" |
| Optional `.pc` file and plain `cc` smoke line | script | batch | No pkg-config file in the repo; only if it costs under one task (CONTEXT "Package smoke") |

## Metadata

**Analog search scope:** `scripts/`, `tools/` (lint, digest-sweep, repocheck, claims, common), `core/` (CMakeLists, headers, `t64_machine.c`, `t64_scheduler.c`, `t64_cp0.c`, `t64_ai.c`, `t64_digest.c`, `t64_state.c`, softfloat, tests), `runner/` (CMakeLists, `t64_real.c`, tests), `tests/` (`t64_test.h`, `controls`, `seeded`), `fixtures/composite/`, `docs/` (adr, claims, evidence, spec, ci-reviews), `provenance/`, `.planning/phases/10-*`, `.github/workflows/ci.yml` (read only to confirm no edit is needed), `CMakeLists.txt`, `CMakePresets.json`.
**Files read for excerpts:** about 45; commands run read-only (`git ls-files`, `git grep`, `sed`, `nm`-free); the only executions were `sh tools/lint/core-lint.sh --files` on headers copied to the session scratchpad.
**Counts re-measured at this commit:** `t64_machine_create(` 157 lines; `t64_stop_request NAME = {` 145; `(t64_stop_request){` 35; uninitialized `t64_stop_request` declarations 16 in 9 files; `t64_host_allocator NAME = {` 130; `t64_stop_result NAME = {` 215. RESEARCH figures (155 create lines in 74 files, 248 archive symbols, 20 stale lines) were not re-measured here.
**Pattern extraction date:** 2026-11-25
