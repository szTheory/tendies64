# Phase 9: Controller, Save Media and Replay - Pattern Map

**Mapped:** 2026-11-19
**Files analyzed:** 77 file rows across the eleven plans (about 100 individual paths; header/source pairs, fixture triples and doc sets are one row each)
**Analogs found:** 71 exact or role-match, 4 partial, 2 none (see "No Analog Found")
**Basis:** `09-CONTEXT.md` (D-01..D-46, S-01..S-11, K1-K16 amendments) and the per-plan file lists in `09-RESEARCH.md`. This file extends RESEARCH patterns P1-P8 to per-file analogs; it does not restate them. Every analog path below was checked with `git ls-files` and is tracked source (no `build/`, no installed or synced mirror). Paths are repo-relative. Line numbers are against the commit at the start of this session; re-check before editing.

## Pattern-mapper findings beyond K1-K16

Eight additional hidden dependencies surfaced while reading the analogs. Each is verified against code with file:line. They are labeled N1-N8 so plans can cite them the way they cite K-ids. N1 and N4 change plan content materially.

| ID | Finding | Evidence | Handling (owner plan) |
|----|---------|----------|-----------------------|
| N1 | `scripts/check-rsp-no-hle.py` (CTest `repo.rsp-no-hle`, also run by `scripts/check-audio-cases.py --run` and bound by `scripts/qualify-audio.py` `SOURCE_PATHS`) hardcodes (a) the exact set of `core/t64_*.c` files (`CANONICAL_CORE_SOURCES`, `:18-51`), (b) per-file reviewed function inventories (`REVIEWED_AI_FUNCTIONS` `:224-234`), and (c) a SHA-256 of the whitespace-stripped, comment/literal-blanked body of about 245 functions (`REVIEWED_BODY_DIGESTS` `:239-`; comparison at `:815-825` fails with "executable body changed; fetched-path review is required"). Pinned functions that Phase 9 edits: all 19 `core/t64_ai.c` functions (the seven `t64_has_ai_profile` gates at `core/t64_ai.c:259,303,380,403,472,632,660`), `core/t64_bus.c :: decode_region` (`:136` AI gate; SI/SRAM carve-outs), `t64_bus_write*`, `core/t64_scheduler.c :: event_can_dispatch` and `t64_schedule_cancel`, `core/t64_pi.c :: t64_pi_complete`. A new `core/t64_si.c`, `t64_joybus.c`, `t64_save.c` or `t64_state.c` fails `_source_inventory` (`:585-601`, "unreviewed t64_*.c production source(s)") until added to the set. `scripts/tests/test_rsp_no_hle.py:104,137` iterate that set. | `scripts/check-rsp-no-hle.py:18-51,224-265,585-601,815-825`; `runner/CMakeLists.txt:170-176` (`repo.rsp-no-hle`) | In the plan that edits a pinned body or adds a core source, update the set/digest rows in the same commit. 09-01 refreshes the AI and `decode_region` digests; 09-03 adds `t64_si.c`/`t64_joybus.c` and any scheduler/bus body digests; 09-04 adds `t64_save.c` and the `t64_pi_complete` digest; 09-08 adds `t64_state.c`. Structure is FH-frozen scanner logic, rows are data (D-06). Run `python3 -B scripts/check-rsp-no-hle.py --build build/gate` as a cheap-first check after every core edit. |
| N2 | `scripts/gate.sh:100-104` accepts `T64_GATE_JUNIT_OUT` only under `build/gate/08-05-audio-private/`; a Phase 9 native record needs its own private-directory arm or the qualifier's `--native` step is refused. `scripts/gate.sh` is in the Phase 8 bound set. | `scripts/gate.sh:100-104` | Add the Phase 9 private arm in 09-02 (not 09-11) so it is frozen before FH. |
| N3 | The rights scanner's denied-extension table and container-magic table are C, not data: `denied_ext` assumes a 3-letter extension (`end - dot - 1u != 3u`) from `{z64,n64,v64,sav,eep,sra,fla,mpk,ndd,d64}`, and `container_magic` matches magic at offset 0. `.sav`/`.eep`/`.sra` already cover EEPROM and SRAM files. A state or replay extension that is not 3 letters, and a `T64STATE` magic row, need a source change plus selftest controls. | `tools/rights/t64_blobscan.c:533-578` (extensions), `:488-501` (magic), `scripts/rights-scan.sh:564-599` (near-miss and trailing-byte controls) | 09-02 owns it (D-38). Pick 3-letter state/replay extensions where possible so the change stays a table row; add the magic row for `T64STATE` and the replay magic with a control each. `tools/rights` is in `qualify-audio.py` `SOURCE_PATHS`, so a late edit costs a requalification cycle. |
| N4 | `scripts/check-ci-yml.sh` is an exact-match contract, not an allowlist you append to: per-job `job_runs != 9`, `step_count != 12`, total `nruns != 18`, the run-line `else if (val == ...)` ladder, the per-step counters and order checks, `expected_upload_path()`, and a long selftest mutation list. Adding Phase 9 collector/PII/upload steps touches all of them in both jobs. | `scripts/check-ci-yml.sh:79-84,105-129,195-206,243-262,292-294,379-381,495-549` | Land once in 09-03 with its selftest controls (D-06 FH). Mirror the audio triple (case run, emit, PII scan) and the upload-path list. |
| N5 | The runner's allowed-profile set is derived from fixture rows: `t64_real_fixture_profile_allowed` walks the static `fixtures[]`, so a profile id is runnable only when some row names it (K12 explains the row requirement; this is the mechanism). The array is `static`, so `runner.profile-table-id-length` (D-43) cannot iterate it without an accessor. The supervisor recomputes each row's `profile_sha256` through `t64_profile_lookup`/`t64_profile_hash` (`real_profile_identity`). | `runner/t64_real_fixture.c:8-59,88-95`; `runner/t64_real.c:1585-1588`; `runner/t64_supervisor.c:844-864` | In 09-02 add a small exported iteration accessor to `t64_real_fixture.[ch]` (or test via `t64_real_fixture_profile_allowed` plus a literal id list) so the 63-character test reads the real table. |
| N6 | Composite profile ids are hash-pinned at create: `is_rdram_b_t1_profile` compares the candidate's profile hash with the canonical const, so any serialized-field change (including `event_queue_capacity` and `sequence_width_bits`) is `INVALID_ARGUMENT` for the new family (the mutation loop at `core/tests/test_profile.c:882-884` proves it). The "small queue, small sequence width" tests in D-44/K16 therefore cannot use a mutated descriptor with `t64_machine_create`. | `core/t64_machine.c:1352-1411`; `core/tests/test_profile.c:882-884` | Use the two in-repo workarounds: fill the queue with synthetic events and set `next_sequence` near the limit (`core/tests/test_scheduler.c:346-360,373`; `core/tests/test_cpu_time.c:1025-1026,1051`), or build a direct-state machine copy and edit `machine.profile.event_queue_capacity` in memory (`core/tests/test_cpu_time.c:81-96` `setup_t1_machine`, which bypasses create). |
| N7 | The "sink seam" (D-05) already has a precedent: `profile_writer` is a three-way writer (`bytes` copy-out, `hash` context, running `offset` for size query) used by `write_profile`/`profile_serialized_size`/`t64_profile_hash`. `core/t64_digest.c` has no such seam today: its `hash_u32/hash_u64/hash_event` are file-static and 49 direct `t64_sha256_update` lines bypass them; `core/t64_ai.c:34-52` carries a second private copy of the helpers. | `core/t64_machine.c:1494-1499,1583-1609,1748-1753`; `core/t64_digest.c:8-36`; `core/t64_ai.c:34-52` | Copy the `profile_writer` shape for the digest sink (09-01). The size-then-copy-out API shape for state export (09-08) is `t64_profile_serialize` (`core/t64_machine.c:2145-2175`: disjoint-range checks, then `if (bytes_out == NULL || capacity < required) { *length_out = required; return T64_STATUS_BUFFER_TOO_SMALL; }`) plus `t64_audio_copy`'s `BUFFER_TOO_SMALL` convention. |
| N8 | Guest output is bounded three times at 4096 bytes: ISViewer retained capture, the real runner's `T64_REAL_OUTPUT_CAPACITY`, and the strict checker's `max_real_log_bytes`. D-16's one-ASCII-line-per-poll transcript plus any Count readings must fit all three, and the hello transcript is also the Phase 8 audio ROMs' contract (K4). | `core/t64_internal.h:66`; `runner/t64_real.c:26`; `runner/t64_result_check.c:139` | Size the pad-poll transcript (polls x line length) before authoring; the K4 per-fixture pin carries `expected_log_bytes` up to 4096. |

## File Classification

Plan tags: `09-0N` creates or first modifies the file; later plans that modify it again are noted in the assignment. "M" modify, "C" create. Match quality: exact = same role and data flow; role-match = same role, different flow or scale; partial = analogous mechanism only; none = see "No Analog Found".

| # | New/Modified File | Role | Data Flow | Closest Analog | Match |
|---|-------------------|------|-----------|----------------|-------|
| **09-01 Core identity and walker** |||||
| 1 | `core/include/tendies64/t64_machine.h` (M) | config/API header | request-response | same file: `ai_policy` field `:189-190`, `T64_SAVE_NONE` `:95` | exact |
| 2 | `core/t64_internal.h` (M) | model/predicates | transform | same file: `t64_has_ai_profile` `:927-939`, `t64_ai_state` `:785-826` | exact |
| 3 | `core/t64_machine.c` (M) | config/validator/writer | CRUD | same file: AI descriptor `:974-1039`, ladders `:2215-2415` | exact |
| 4 | `core/t64_isviewer.c` (M) | middleware/gate | request-response | same file: `ai_profile` arm `:118-122` | exact |
| 5 | `core/t64_digest.c` (M) | utility/walker | transform | `core/t64_machine.c` `profile_writer` `:1494-1609` (sink); v24 section `core/t64_digest.c:746-816` | role-match |
| 6 | `core/t64_ai.c`, `core/t64_bus.c` (M, gate switch) | device/route | event-driven | same files: seven gates `core/t64_ai.c:259..660`, `core/t64_bus.c:136` | exact |
| 7 | `core/tests/test_profile.c` (M) | test | transform | same file: `test_rdp_profile_metadata_binding` `:772-914` | exact |
| 8 | `core/tests/test_scheduler.c`, `core/tests/test_determinism.c` (M) | test | event-driven | `core/tests/test_ai.c:1411-1475` (`run_vi_ai_tie_order`), `core/tests/test_determinism.c:198-218` | exact |
| 9 | `core/tests/test_digest_v25.c` (C) | test | transform | `core/tests/test_boot_entry.c:595-804` (independent reference encoder), `core/tests/test_digest_trace.c:260-386` | exact |
| 10 | membership sweep script / predicate golden matrix (C) | utility | batch | `scripts/check-rsp-no-hle.py:470-549` (C source scanner helpers) | partial |
| 11 | `core/CMakeLists.txt` (M, every core plan) | config | batch | same file `:567-606`, `:626-651`, `:758-786` | exact |
| 12 | `docs/spec/machine-contract.md` (M) | doc/spec | n/a | same file `:246-305`, `:388-410` | exact |
| 13 | `scripts/check-test-inventory.sh` (M, every plan) | config/gate | batch | same file `PHASE08_TESTS` `:174-194`, `:281-282`, `:297` | exact |
| 14 | `scripts/check-rsp-no-hle.py`, `scripts/tests/test_rsp_no_hle.py` (M, N1) | gate | batch | same files | exact |
| **09-02 Runner, supervisor, publisher, harness mechanism** |||||
| 15 | `runner/t64_artifact_publisher.[ch]` (C) | service | file-I/O | `runner/t64_raw_capture.c/.h` (copy with provenance) | exact |
| 16 | `runner/tests/test_artifact_publisher.c` (C) | test | file-I/O | `runner/tests/test_audio_capture.c:1821-3366` (substitution/race/target cases) | role-match |
| 17 | `fixtures/phase09/admit.S`, `admit.ld` (C) | fixture | transform | `fixtures/hello/hello.S` (`VARIANT` defsym) and `fixtures/audio/pcm.ld` | exact |
| 18 | `scripts/build-phase09-admit-fixture.sh` (C) | build script | batch | `scripts/build-audio-fixture.sh` | exact |
| 19 | `scripts/qualify-phase09.py` (C stub; real logic 09-03) | qualifier | batch | `scripts/qualify-audio.py` | role-match (stub is a reduced copy) |
| 20 | `scripts/tests/test_phase09_qualification.py` (C) | test | batch | `scripts/tests/test_audio_qualification.py` | exact |
| 21 | `scripts/check-phase09-cases.py`, `docs/spec/phase09-cases.json` (C) | gate | batch | `scripts/check-audio-cases.py`, `docs/spec/audio-cases.json` | exact |
| 22 | `docs/claims/phase09.md` (C) | doc/claim | n/a | `docs/claims/TEMPLATE.md`, `docs/claims/phase08.md` | exact |
| 23 | `runner/t64_supervisor.c` (M) | controller | request-response | same file: raw-capture pair `:474-475,509-514,592-603,1496-1528,1534-1536` | exact |
| 24 | `runner/t64_real.c` (M) | controller | streaming | same file: audio profile branches `:40-41,488,681,747,754-769`, caps `:1591` | exact |
| 25 | `runner/t64_real_fixture.c/.h` (M) | config/route | request-response | same files: rows `:8-59`, struct `.h:11-19` | exact |
| 26 | `runner/t64_result_check.c` (M, K4) | validator | request-response | same file `:134-139,838-856,1177-1190` | exact |
| 27 | `runner/CMakeLists.txt` (M, every runner plan) | config | batch | same file `:12-18`, `:282-311`, `:422-530` | exact |
| 28 | `scripts/rights-scan.sh`, `tools/rights/t64_blobscan.c` (M, N3) | gate | batch | same files `:564-599`, `:488-578` | exact |
| 29 | `docs/spec/raw-capture.md` (M) | doc/spec | n/a | same file: known-limit paragraph `:122-123` | exact |
| 30 | `fixtures/manifest.json` (M, every fixture plan) | config | batch | same file: rows `audio-pcm-literal`, `audio-pcm-wrong-sample` | exact |
| 31 | `scripts/gate.sh` (M, N2) | gate | batch | same file `:100-104` | exact |
| **09-03 SI/PIF/controller tracer and first hosted data** |||||
| 32 | `core/t64_cpu.c` (M) | controller | request-response | same file `:1483-1550` | exact |
| 33 | `core/t64_scheduler.c` (M) | service | event-driven | `core/t64_cpu.c:1531-1546` (capacity rule to extract); same file `:88-190` | role-match |
| 34 | `core/t64_si.c` (C) | device | event-driven | `core/t64_pi.c` (reserve/commit, sticky error, complete) and `core/t64_ai.c:251-374` (profile-gated registers) | exact |
| 35 | `core/t64_joybus.c` (C) | utility/parser | transform | `core/t64_rom.c:56-95` (bounded byte decode/normalize) | partial |
| 36 | `core/tests/test_si.c` (C) | test | event-driven | `core/tests/test_ai.c` (dispatch `:2792-2976`, helpers `:25-218`), `core/tests/test_pi_dma.c:651-742`, `core/tests/test_cpu_time.c:982-1093` | exact |
| 37 | `core/tests/test_joybus.c` (C) | test | transform | `core/tests/test_boot_entry.c:595-804` (literal field-table preflight), `core/tests/test_ai.c` dispatch | role-match |
| 38 | `core/tests/fuzz_joybus.c` (C, ASan-only) | test/fuzz | batch | `core/tests/fuzz_rom_admission.c:420-474`; registration `core/CMakeLists.txt:758-786` | exact |
| 39 | `core/t64_internal.h`, `core/t64_warm_devices.c`, `core/t64_mi.c` (M) SI id/bit constants | model | event-driven | `core/t64_internal.h:85-92` (AI ids/bit), `core/t64_pi.c:16` (local MI bit) | exact |
| 40 | `core/t64_observe.c` (M) input latch | service | event-driven | same file `:86-122` | exact |
| 41 | `fixtures/pad/poll.S`, `poll.ld` (C) | fixture | transform | `fixtures/audio/pcm.S` | exact |
| 42 | `scripts/build-pad-fixture.sh` (C) | build script | batch | `scripts/build-audio-fixture.sh` | exact |
| 43 | `runner/tests/test_pad_poll.c` (C) | test | request-response | `runner/tests/test_rdp_video.c` (accepted / same-setup wrong value / replacement refused) | exact |
| 44 | `docs/spec/si-pif-controller.md` (C) | doc/spec | n/a | `docs/spec/audio-interface.md` (heading structure) | exact |
| 45 | `scripts/qualify-phase09.py` real collector/verifier (M) | qualifier | batch | `scripts/qualify-audio.py:946-991,2504-2606` | exact |
| 46 | `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh` (M, N4) | config/gate | batch | same files | exact |
| 47 | `docs/ci-reviews/LOG.md` (M via `scripts/ci-review.sh --append`) | doc/log | batch | same file rows | exact |
| **09-04 Save devices** |||||
| 48 | `core/t64_save.c` (C) | device/storage | CRUD | `core/t64_pi.c` (DMA effect at completion), `core/t64_ai.c` (profile-keyed storage) | role-match |
| 49 | `core/tests/test_save.c` (C) | test | CRUD | `core/tests/test_ai.c` (dispatch, helpers), `core/tests/test_pi_dma.c` (duration/atomic effect) | exact |
| 50 | `fixtures/save/eeprom.S`, `sram.S`, `.ld` (C) | fixture | transform | `fixtures/audio/pcm.S` | exact |
| 51 | `scripts/build-save-fixtures.sh` (C) | build script | batch | `scripts/build-audio-fixture.sh` | exact |
| 52 | `runner/tests/test_save_fixtures.c` (C) | test | request-response | `runner/tests/test_rdp_video.c` | exact |
| 53 | `docs/spec/save-media.md` (C) | doc/spec | n/a | `docs/spec/audio-interface.md` | exact |
| 54 | `core/t64_pi.c` (M) SRAM start path | device | event-driven | same file `:48-118,250-295` | exact |
| **09-05 Save import/export and persistence** |||||
| 55 | `fixtures/save/roundtrip.S` (C) | fixture | transform | `fixtures/audio/pcm.S` | exact |
| 56 | `runner/tests/test_save_roundtrip.c` (C) | test | file-I/O | `runner/tests/test_audio_capture.c` `actual-av-bundle` | role-match |
| 57 | `core/t64_save.c` import/export, header API (M) | service | CRUD | `core/t64_ai.c:623-679` (info/copy, `BUFFER_TOO_SMALL`), `core/t64_rom.c:76-95` (format transforms) | exact |
| **09-06 CORE-02 ordering matrix** |||||
| 58 | `core/tests/test_sched_matrix.c` (C) | test | event-driven | `core/tests/test_ai.c:2303-2462` (whole vs split, interleave), `core/tests/test_determinism.c:285-356`, `core/tests/test_cpu_time.c:315-361` (Count wrap) | exact |
| 59 | out-of-tree mutant procedure and list (C, in `docs/spec/phase09-cases.json`) | test infrastructure | batch | none (RESEARCH Open Question 6); structure from `scripts/tests/test_rsp_no_hle.py` `MUTATION_SEEDS` | partial |
| **09-07 Replay** |||||
| 60 | `runner/t64_replay.c/.h` (C) | service | transform/file-I/O | `runner/t64_input.c/.h` (closed parser + canonical writer), `runner/t64_real_result.c:38-125` (canonical sorted-key writer) | exact |
| 61 | `runner/tests/test_replay.c` (C) | test | event-driven | `runner/tests/test_input.c:127-366` (parser), `:446-758` (partition runs) | exact |
| 62 | `fixtures/replay/mix.S`, `.ld`; `scripts/build-replay-fixture.sh` (C) | fixture/build | transform | `fixtures/audio/pcm.S`; `scripts/build-audio-fixture.sh` | exact |
| 63 | build-independent replay goldens (C) | test data | batch | `docs/spec/audio-cases.json` data discipline; literal pins in `core/tests/test_profile.c:784-795` | role-match |
| 64 | `docs/spec/replay.md` (C) | doc/spec | n/a | `docs/spec/audio-interface.md` | exact |
| 65 | `runner/t64_real.c`, `runner/t64_supervisor.c` replay flags (M) | controller | request-response | raw-capture pair (rows 23-24) | exact |
| **09-08 State export and container** |||||
| 66 | `core/t64_state.c` (C) | service | transform | `core/t64_machine.c:1494-1753` (`profile_writer`, `t64_profile_serialize`), `core/t64_digest.c` walker | role-match |
| 67 | `core/tests/test_state_export.c` (C) | test | transform | `core/tests/test_boot_entry.c:595-804` (independent decoder/encoder), `core/tests/test_digest_trace.c:260-386` | exact |
| 68 | `runner/tests/test_state_runner.c` (C) | test | file-I/O | `runner/tests/test_audio_capture.c` | role-match |
| 69 | `docs/spec/state-container.md` (C) | doc/spec | n/a | `docs/spec/machine-contract.md:246-305` (digest text), `docs/spec/audio-interface.md` | role-match |
| **09-09 State import, invariants, boundary matrix** |||||
| 70 | `core/tests/test_state_import.c` (C) | test | batch | `core/tests/test_ai.c` dispatch; `core/tests/test_pi_dma.c:651-742` (refusal = bit-identical digest) | role-match |
| 71 | `core/tests/fuzz_state_import.c` (C, ASan-only) | test/fuzz | batch | `core/tests/fuzz_rom_admission.c` | exact |
| 72 | `scripts/check-state-field-use.py` (C) | gate | batch | `scripts/check-rsp-no-hle.py:470-549,815-825` | role-match |
| **09-10 Review, security audit, fix loop** |||||
| 73 | `09-REVIEW.md`, `09-SECURITY.md` (C) | doc | n/a | `.planning/phases/08-audio-interface-and-raw-capture/08-REVIEW.md`, `08-SECURITY.md` | exact |
| **09-11 Identity freeze and qualification** |||||
| 74 | `docs/evidence/phase09-*.md` (C) | doc/evidence | n/a | `docs/evidence/phase08-audio.md` | exact |
| 75 | `docs/claims/phase09.md` (M, real blocks) | doc/claim | n/a | `docs/claims/phase08.md` | exact |
| 76 | test pins and `scripts/qualify-phase09.py` data pins (M) | test/qualifier | batch | `scripts/qualify-audio.py:24-31`, `core/tests/test_profile.c:784-795` | exact |
| 77 | `docs/spec/*` finalization (M) | doc/spec | n/a | the same spec files | exact |

## Pattern Assignments

### Shared registration conventions are in "Shared Patterns" below. Per-file sections give analog, what to copy, and the file-specific delta.

---

### 09-01: core identity and walker

#### `core/include/tendies64/t64_machine.h` (config/API, request-response)

**Analog:** same file. Add `si_policy` and `save_policy` after `ai_policy` and before `descriptor_version`, so the end-of-text-block ordering the offset table in `core/tests/test_profile.c:802-818` relies on stays "texts, then versions, then boot hashes".

```c
  /* Descriptor v11 field: deterministic AI DMA/sample policy. */
  char ai_policy[T64_PROFILE_TEXT_CAPACITY];
  uint32_t descriptor_version;
```
(`core/include/tendies64/t64_machine.h:189-191`.) Add `T64_SAVE_EEPROM_4K 1`, `_16K 2`, `_SRAM_32K 3` beside `#define T64_SAVE_NONE UINT32_C(0)` (`:95`); `T64_PROFILE_TEXT_CAPACITY` is 64 (`:15`) and `T64_PROFILE_SERIALIZED_MAX_BYTES` 2048 (`:16`), so two more labels fit (RESEARCH: 1,319 bytes today). Trace device constants follow `T64_TRACE_DEVICE_VI UINT32_C(8)` (`:29`); the AI device id is internal-only (`T64_AI_DEVICE` at `core/t64_internal.h:87`), so SI id 10 (D-02) can follow that internal-only convention. New API statuses extend the list ending `T64_STATUS_BUS_DEVICE_REFUSED UINT32_C(19)` (`:88`).

#### `core/t64_internal.h` (model, transform)

**Analog:** same file; add the new family beside the AI roots.

```c
static inline bool t64_has_ai_profile(const t64_machine *machine) {
  return machine != NULL && machine->profile.descriptor_version == 11u &&
         machine->profile.machine_model_version == 19u &&
         machine->profile.state_serialization_version == 24u;
}

static inline bool t64_has_vi_filter_v9_profile(
    const t64_machine *machine) {
  return (machine != NULL && machine->profile.descriptor_version == 10u &&
          machine->profile.machine_model_version == 18u &&
          machine->profile.state_serialization_version == 23u) ||
         t64_has_ai_profile(machine);
}
```
(`core/t64_internal.h:927-939`.) Copy this shape for `t64_has_si_profile` (12/20/25) and OR it into the second function. K1: `t64_has_rdp_depth_profile` is guarded by `descriptor_version == 10u` (`:1028-1035`), so depth stays inactive on descriptor 12 with no extra edit; capture the golden matrix first. State structs follow `t64_pi_state` (`:187-209`, plain fields, `active_valid`/`next_token`) and `t64_ai_state` (`:785-826`, including the comment `/* Host observer cursor and occupancy are excluded from guest identity. */` at `:815` as the convention for observer-only fields such as `write_generation`). Device id/kind constants follow `:85-92` (`T64_AI_DEVICE UINT32_C(9)`, `T64_AI_EVENT_SAMPLE UINT32_C(1)`, `T64_MI_INTERRUPT_AI UINT32_C(0x04)`); add `T64_SI_DEVICE UINT32_C(10)` and an SI MI bit constant here rather than the local define in `core/t64_warm_devices.c:22`.

#### `core/t64_machine.c` (config/validator/writer, CRUD)

**Analog:** same file, every AI site. Copy the AI arm at each:

- Canonical const descriptors (one per id, four total): `ntsc_cart_4m_openipl3_rdram_b_t1_rdp_vi_filter_v9_ai_v1_profile` `:974-1039`; the new ones differ only in `profile_id`, `peripheral_flags`, `save_kind`, the two new text fields, and the 12/20/25 triple.
- Id and tuple helpers `:1178-1192` (`has_..._profile_id` then `is_ai_profile`).
- Canonical-descriptor selection and fail-closed compare `:1352-1411` (add one `else if` arm per id before the final `else`, which silently compares against the v2 base profile and so fails closed for an unlisted id).
- `profile_fields_are_valid` `:1507-1579`: the chain `(profile->descriptor_version != 11u || T64_PROFILE_TEXT_OK(ai_policy))` at `:1559-1560` is the pattern for `si_policy`/`save_policy` (descriptor 12 only). Legacy versions must keep ignoring the new fields (hash and serialize paths unchanged; `core/tests/test_profile.c:152-153` hashes `save_kind = 7`). Add range checks: `save_kind` 0..3, `peripheral_flags` bit 0 only, `synchronization_quantum == 1`, ratio 1:1 for descriptor 12; refuse non-zero `save_kind`/`peripheral_flags` for versions below 12 at create (D-01, D-04).
- Writer: `write_profile` `:1623-1746`; the AI arm is `:1741-1744` (`if (profile->descriptor_version == 11u) { profile_writer_text(writer, profile->ai_policy, sizeof profile->ai_policy); }`). Add the descriptor-12 arm after it, and extend the `5u..11u` group at `:1702-1709` and the `7u..11u`, `8u..11u`, `10u..11u` groups so 12 is included in each.
- Lookup `:1978-2124`: one `static const char ..._profile_id[]` per id plus one `bounded_text_equal` branch (the AI branch is `:2121-2124`).
- Create-time storage ladders: the eight `is_ai_profile(profile)` uses at `:2229, 2250, 2265, 2308, 2332, 2385, 2398, 2410` become "AI or SI family" (video/audio rings and RDP coverage are inherited). Per-profile trailing regions for EEPROM/SRAM go in the same measure-then-assign style as `coverage_offset`/`memory_coverage_offset`/`depth_hidden_offset` (`:2230-2272`, `:2386-2415`). Reset: SI/save state clear belongs in `t64_machine_reset_execution_state` (`:1793-1804`) or `reset_guest_state` (`:1931-1971`) next to `t64_ai_reset_machine_state` (`:1959`).

#### `core/t64_isviewer.c` (gate, request-response)

**Analog:** same function. K2: without these arms the new ids have no ISViewer window and pad-poll prints nothing.

```c
  ai_profile = machine->profile.descriptor_version == 11u &&
               machine->profile.machine_model_version == 19u &&
               machine->profile.state_serialization_version == 24u &&
               memcmp(machine->profile.profile_id, ai_profile_id,
                      sizeof ai_profile_id) == 0;
```
(`core/t64_isviewer.c:118-122`; OR it into the return at `:123-130`.) Add four id arms, or one tuple-12/20/25 arm plus a `memcmp` against each of the four ids, in this same style. Test with the analog `core/tests/test_isviewer.c:134-157` (`test_ai_profile_isviewer`: `t64_isviewer_active`, magic write/read through the bus).

#### `core/t64_digest.c` (walker, transform)

**Analog (sink seam):** `profile_writer` in `core/t64_machine.c:1494-1609`, the in-repo three-way writer (N7).

```c
typedef struct {
  uint8_t *bytes;
  size_t offset;
  t64_sha256_ctx *hash;
} profile_writer;
...
static void profile_writer_bytes(profile_writer *writer, const uint8_t *bytes,
                                 size_t length) {
  if (writer->bytes != NULL) {
    (void)memcpy(writer->bytes + writer->offset, bytes, length);
  }
  if (writer->hash != NULL) {
    t64_sha256_update(writer->hash, bytes, length);
  }
  writer->offset += length;
}
```
(`:1494-1499`, `:1583-1592`.) Route `hash_u32/hash_u64/hash_event` (`core/t64_digest.c:8-36`) and the direct `t64_sha256_update` calls through one such sink. Size query = `writer = {NULL, 0u, NULL}` then read `offset` (`profile_serialized_size` `:1748-1753`).

**Analog (v25 branch placement):** new domain constant after `domain_v24` (`core/t64_digest.c:197`), a `has_v25_state` boolean after `has_v24_state` (`:231`), and the branch first in the domain chain (`:251-253`). The v24 device section at `:746-816` is the template for a device section (fixed-width big-endian fields, ring entries iterated by logical index, observer cursors excluded).

```c
  if (has_v24_state) {
    t64_sha256_update(&context, domain_v24, sizeof domain_v24 - 1u);
  } else if (has_v23_state) {
```
(`:251-253`.) The v25 stream must reuse the v24 field order for inherited devices so the strip-and-compare differential holds (K1: depth block absent because `has_v21_state = t64_has_rdp_depth_profile(machine)` is false, `:228`).

#### `core/t64_ai.c`, `core/t64_bus.c` (device/route, event-driven)

**Analog:** same lines. Replace `t64_has_ai_profile(machine)` with `t64_has_ai_device(machine)` at `core/t64_ai.c:259,303,380,403,472,632,660` and `core/t64_bus.c:136` (the last is `return t64_has_ai_profile(machine) ? T64_BUS_REGION_AI : T64_BUS_REGION_WARM;`). Refresh the pinned digests for all of these functions (N1). Old-profile pins for these gates already exist: `core/tests/test_ai.c:397-442` (`case_old_profile_ai_refusals`), keep passing unchanged.

#### `core/tests/test_profile.c` (test, transform)

**Analog:** same file, the Phase 8 "new-descriptor check" (CONTEXT process rule). Extend `test_rdp_profile_metadata_binding` (`:772-914`) rather than copying it:

```c
  static const char *const ids[] = {
      ...
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1",
  };
  /* Earlier-profile bytes are pinned to the retained pre-repair artifact. */
  static const char *const legacy_hashes[] = { ... };
  static const size_t legacy_lengths[] = {1116u, 1132u, 1172u, 1155u,
                                          1231u, 1243u, 1256u, 1319u};
```
(`:773-795`.) The mutation loops are the template: flip one bit in every text and boot field (`:863-886`), then blank/unterminated each text (`:887-909`), requiring hash change, serialized-length constancy, and `t64_machine_create` -> `INVALID_ARGUMENT`. The new descriptor-12 cases add `si_policy`/`save_policy` to `text_offsets[]` (`:802-812`) and the per-index `text_count` ladder (`:826-827`). During the unfrozen period (D-06) the four new ids' literal hashes must come from a symbolic accessor, not literals. Case dispatch: append names to `--list-cases` and `--case` (`:937-958`) and register one CTest per case (`core/CMakeLists.txt:596-599`, the existing `unit.core.ai.profile-pins`/`profile-mutations` pair). Add the id-length (<= 63) and quantum cases here (the contrast already exists at `:450-470`).

#### `core/tests/test_scheduler.c`, `core/tests/test_determinism.c` (test, event-driven; E0 ledger and FIFO characterization)

**Analog (both insertion orders, independent literals):** `run_vi_ai_tie_order(ai_first)` in `core/tests/test_ai.c:1411-1475`. Copy the structure: strip the boot CPU event (`cancel_boot_cpu_event`, prototyped `core/tests/test_ai.c:23`), arrange two events at one tick in each insertion order, enable trace, advance to the tick in slices, drain the trace, assert the index order equals insertion order.

```c
  if (ai_first) {
    T64_CHECK(ai_index < vi_index);
  } else {
    T64_CHECK(vi_index < ai_index);
  }
```
(`:1469-1473`.) Existing PI/SP tie check to keep: `check_same_tick_trace_order` (`core/tests/test_determinism.c:198-218`). The closed `(device, kind)` ledger (D-07) is asserted against a hand-written literal table of the reservation sites listed in RESEARCH P2 plus SI 10/{execute,complete}; the transposed-table mutation control must build and fail the named test (Mutant-must-compile).

#### `core/tests/test_digest_v25.c` (test, transform)

**Analogs:** (1) independent reference encoder in the test, compared with the production digest and a literal hex: `core/tests/test_boot_entry.c:20-21` (prototype), `:595-632` (`reference_hash_u32/u64`, `reference_profile_text`), `:804-` (`reference_machine_digest`), usage `:395-412` (literal `expected_v9_prepared_state_sha256`, with a "RED observation" message when production and reference differ). (2) field-mutation matrix: `core/tests/test_digest_trace.c:260-386` (`check_digest_differs_after_*` helpers over every field, and a relocation case proving the queue ring head is not hashed, `:369-383`).

```c
  /* Independently assembled SHA-256 of the documented state.v5 byte stream. */
  uint8_t expected[T64_DIGEST_SIZE] = { 0x68u, 0x13u, ... };
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  T64_CHECK(memcmp(digest, expected, sizeof expected) == 0);
```
(`core/tests/test_digest_trace.c:263-282`.) The strip-and-compare differential has no committed analog (see "No Analog Found"); build it from the reference encoder: emit the v25 stream with a marker list, strip markers/new sections/identity spans, and compare with the v24 stream for idle, AI mid-DMA, VI frame, RDP state machines. Legacy pins (`test_digest_trace.c` golden and `core/tests/test_boot_entry.c` v9 prepared state) must pass unchanged through the sink.

#### Membership sweep script and predicate golden matrix (utility, batch)

**Partial analog:** `scripts/check-rsp-no-hle.py:470-549` (`strip_c_comments_and_literals`, `function_body`, `file_function_names`) for a script that lists predicate call sites by scanning C source with comments/literals blanked. The golden matrix itself is a test, not a script: loop over every registered id and print every `t64_has_*` result (RESEARCH K1 probe), then compare the new ids with the AI row except the new predicate. Capture before the first ladder edit.

#### `core/CMakeLists.txt`, `scripts/check-test-inventory.sh`, `docs/spec/machine-contract.md`

Shared conventions S2/S4 below. File-specific: `docs/spec/machine-contract.md:246-305` is the section to rewrite (stale v5/v8 text); keep the replacement table `:388-410` in lockstep (it demands "New future-affecting state must be added here and to the reset, digest, and refusal tests when it is introduced", `:390-392`).

---

### 09-02: runner, supervisor, publisher, harness mechanism

#### `runner/t64_artifact_publisher.[ch]` (service, file-I/O)

**Analog:** `runner/t64_raw_capture.c/.h`, copied with a provenance comment; D-38 says the original is left untouched (CR-01 locus). Copy these pieces:

- Parent-by-fd walk with `O_NOFOLLOW`, no `.`/`..` components: `open_directory_nofollow` `:196-210`, `open_directory_path` `:212-265`, `split_target` `:267-311`.
- Atomic no-replace rename with per-platform primitive:

```c
static int rename_noreplace(int old_directory_fd, const char *old_name,
                            int new_directory_fd, const char *new_name) {
#if defined(__APPLE__)
  return renameatx_np(old_directory_fd, old_name, new_directory_fd, new_name,
                      RENAME_EXCL);
#elif defined(__linux__) && defined(SYS_renameat2) && defined(RENAME_NOREPLACE)
  return (int)syscall(SYS_renameat2, old_directory_fd, old_name,
                      new_directory_fd, new_name, RENAME_NOREPLACE);
```
(`runner/t64_raw_capture.c:313-320`.)
- 0700 private stage, identity-checked: `create_stage` `:370-427` (records dev/ino before later checks so cleanup can remove its own empty directory), `same_directory_entry` `:352-358`, probe of real no-replace support before guest execution `probe_noreplace` `:429-471`.
- 0600 files, `O_CREAT|O_EXCL|O_NOFOLLOW`, `fchmod`, `st_nlink == 1`: `write_packet_file` `:558-` (`:572-578`), `write_manifest_file` `:844-` (`:869-892`).
- Stable short error names (`capture-stage-name`, `capture-no-replace-unsupported`, ...) and the `abort`/`destroy` pair `:981-1050`.

Delta: the new module publishes single files (state, save, replay, report), not a directory bundle; keep the transaction (stage, write, fsync, no-replace rename) and drop bundle layout and packet counting. The public API mirrors `t64_raw_capture_open/finalize/abort/destroy/error` (`runner/t64_raw_capture.h:36-68`) with the `#ifdef T64_REAL_RUNNER_TESTING` hook shape for the interrupted-stage witness (`:66-70`). Add it to `T64_RUNNER_BUILD_INPUTS` and both runner targets (K15; see row 27).

#### `runner/tests/test_artifact_publisher.c` (test, file-I/O)

**Analog:** `runner/tests/test_audio_capture.c` cases, as a reference for what to cover (not to copy wholesale; the file is 3,481 lines): `run_existing_target_case` `:1824-1917`, `run_unsafe_target_parent_case` `:1918-1989`, `run_group_writable_parent_case` `:2026-2117`, `run_capture_io_errors` `:2176-2262`, `run_capture_interrupted_stage` `:3008-3134`, `run_stage_entry_replacement` `:3165-3256`, `run_capture_commit_race` `:3257-3365`. These are the CR-01 substitution/no-clobber controls to repeat against the new leaf names (`controls.artifact-substitution`). Dispatch convention: `--list-cases` list then positional-argument subcommands (`:3419-3481`). Registration: `runner/CMakeLists.txt:422-514` (one `add_test` per subcommand with the supervisor, checker and fixtures as `$<TARGET_FILE:...>` arguments).

#### `fixtures/phase09/admit.S`, `admit.ld`, `scripts/build-phase09-admit-fixture.sh` (fixture, K12)

**Analog (source variants):** `fixtures/hello/hello.S` selects variants with a `VARIANT` symbol (`.if VARIANT == 1`, `:16,33,65,76,89`) and the build script assembles each with `--defsym="VARIANT=$variant"` (`scripts/build-boot-fixtures.sh:378`). That script is not to be touched (D-38, F-100), so the new script copies the `build_variant` loop. **Analog (script):** `scripts/build-audio-fixture.sh`:

```sh
build_variant() {
    ...
    if [ "$wrong_sample" = yes ]; then
        "$AS" -32 -mips3 -EB --defsym T64_WRONG_SAMPLE=1 \
            -o "$object" fixtures/audio/pcm.S
    else
        "$AS" -32 -mips3 -EB -o "$object" fixtures/audio/pcm.S
    fi
    "$LD" -m elf32ebmip -EB -T fixtures/audio/pcm.ld -o "$elf" "$object"
    "$OBJCOPY" -O binary "$elf" "$flat"
    ...
    "$PACKER" --pack --boot "$BOOT" --payload "$flat" --output "$rom" \
        --entry "$entry" --clock "$clock" --crc2 "$crc2"
```
(`scripts/build-audio-fixture.sh:84-108`.) Copy: pinned `ASSET_SHA256`/`PAYLOAD_SHA256`/`BINUTILS_SHA256` (`:17-19`), `verify_prerequisites` receipt check and packer source pins (`:35-68`), `source_content_sha256` listing every input (`:70-74`), `write_metadata` JSON (`:119-139`), staging then `mv` with a `trap` (`:141-158`), `--build|--qualify` modes (`:160-174`). Linker script copy of `fixtures/audio/pcm.ld` (entry `0x80000400`, `__t64_rom_clock = 0x0000000f`, `__t64_rom_crc2 = 0x00000000`). Entry assertions at `:104-106` must be copied verbatim. The admit ROM is a tiny BREAK-at-completion program like the `hello-empty-break` row (`runner/t64_real_fixture.c:14-17`, `completion_pc` `0x80000400`) built four times with a one-byte `--defsym` variant so each of the four ids has one ROM.

#### `runner/t64_supervisor.c` (controller, request-response)

**Analog:** the existing raw-capture option pair is the template for `--artifact-dir`/`--artifact-max-bytes`.

- Struct fields `:414,419,428-429`; option table rows `:474-475` (`{"--raw-capture-dir", &a->raw_capture_dir, NULL, 0, 0}` and the `np` form for the numeric option); the "seen" flags `:509-514`; cross-validation (dir required with max, caps, default) `:592-603`:

```c
    if (!bad && a->raw_capture_dir_seen) {
      if (a->raw_capture_dir == NULL || a->raw_capture_dir[0] == '\0' ||
          (a->raw_capture_max_bytes_seen &&
           (a->raw_capture_max_bytes == 0u ||
            a->raw_capture_max_bytes > T64_RAW_CAPTURE_MAX_BYTES))) {
        bad = 1;
      } else if (!a->raw_capture_max_bytes_seen) {
        a->raw_capture_max_bytes = T64_RAW_CAPTURE_DEFAULT_MAX_BYTES;
      }
    } else if (!bad && a->raw_capture_max_bytes_seen) {
      bad = 1;
    }
```
- Synthetic mode refuses the new options: the generic branch lists them in the refusal at `:651-663` (add the pair there).
- Child argv and forwarding rows `:1496-1529`; K6: `char *child_args[24];` at `:1498` has no bound check. Replace with a larger array and one checked push helper plus a `_Static_assert` on the maximum option set (nothing in `:1500-1528` checks `child_argc`).
- FSIZE: today `limit.rlim_cur = (rlim_t)(T64_REAL_RESULT_MAX_BYTES + FSIZE_SLACK);` at `:1534`; the exact-`artifact_max_bytes + slack`-only-when-present rule is a new branch here. `FSIZE_SLACK` is `(64ull << 10)` at `:44`.
- K3 cap: `a->bound > UINT64_C(600000)` at `:606` and default `REAL_GUEST_TICKS` `:47`; change to a named per-fixture cap read from the fixture row (the supervisor already has the row via `fixture_identity`, `:1386-1388`).
- Test faults: the `T64_SUPERVISOR_TESTING` allowlist `:623-645` and the runner's twin allowlist `runner/t64_real.c:1648-1670` must both gain each new fault name (K9). The existing fault `report-oversize` (`runner/t64_real.c:1689-1693`: writes `T64_REAL_RESULT_MAX_BYTES` of `x` to stdout) is the pattern for an artifact-writing fault that writes N bytes to the artifact path.

#### `runner/t64_real.c` (controller, streaming)

**Analog:** same file; K3/K4 and profile-branch edits.

- K3 cap in the runner: `guest_ticks <= UINT64_C(600000)` at `:1591` (and the supervisor site above). Keep the default 530,500 unchanged.
- K4 pin: today the pass branch compares the transcript with the compile-time `expected_log` (`:61-62`) and `T64_REAL_EXPECTED_LOG_SHA256` (`:44-45`, used `:1371-1378`):

```c
      } else if (fixture_identity->pass_eligible &&
                 profile_observations_valid(profile, &observations) &&
                 fixture.sha256_hex[0] != '\0' &&
                 output_length == sizeof expected_log - 1u &&
                 memcmp(output, expected_log, sizeof expected_log - 1u) == 0 &&
                 bytes_equal_hex(output, output_length,
                                 T64_REAL_EXPECTED_LOG_SHA256)) {
```
Replace the compile-time constants with row-supplied `expected_log_bytes`/`expected_log_sha256`/bytes pointer, with 0/NULL meaning the legacy hello pin (Phase 8 rows keep passing; K4). The strict checker twin sites are in row 26.
- Profile/observation branches: `T64_REAL_AUDIO_PROFILE_ID` is a literal id (`:40-41`) used at `:488, 681, 747, 758`; `profile_observations_valid` (`:754-769`) falls through to `!rdp_vi_profile || ...` and so already treats model 20 as "no observation required" (RESEARCH P1). Add the family's id-based branch only where the observation rule differs.
- Artifact option: parse like `--raw-capture-dir`/`--raw-capture-max-bytes` (`:1615-1622`, validation `:1637-1645`); pre-queue loop for schedule events `:1139-1155` (K7: port 0 only comes from the schedule parser).

#### `runner/t64_real_fixture.c/.h` (config, request-response)

**Analog:** same file. Rows are positional initializers; struct is `runner/t64_real_fixture.h:11-19`:

```c
typedef struct {
  const char *id;
  const char *sha256;
  size_t size;
  const char *profile_id;
  const char *profile_sha256;
  uint32_t completion_pc;
  bool pass_eligible;
} t64_real_fixture;
```
Row shape to copy (`runner/t64_real_fixture.c:49-58`): `{"audio-pcm-literal", "<rom sha256>", 4728u, "<profile id>", "<profile sha256>", UINT32_C(0x800005e8), true}`. Append new fields (tick cap, transcript pin) at the end and give every existing row an explicit default (0/NULL = legacy), so old rows are byte-for-byte unchanged in behavior. Lookups: `t64_real_fixture_find_bytes` `:61-73`, `_find_identity` `:75-86`, `_profile_allowed` `:88-95` (N5).

#### `runner/t64_result_check.c` (validator, request-response; K4)

**Analog:** same file. The checker already links `t64_real_fixture.c` (`runner/CMakeLists.txt:331`), so it can read the row's pin. Sites: constants `:134-139` (`expected_log_sha256`, `expected_log_bytes = 30u`, `max_real_log_bytes = 4096u`); the pass-match function using `t64_real_fixture_find_identity` and `pass_eligible` `:838-856`; the pass branch `:1177-1190`. Keep the "closed branch" shape: every outcome has exactly one admitted combination of detail/outcome, and a mismatch is a `R-CONSISTENCY` diagnostic.

#### `runner/CMakeLists.txt` (config, batch)

**Analog:** same file. K15: add every new runner module to `T64_RUNNER_BUILD_INPUTS` (`:12-18`) and to the executable source lists of `t64-runner-real` (`:293-294`), `t64-runner-test` (`:303-304`) and, if linked there, the supervisor targets (`:282-289`, `:339-345`); otherwise edits do not change the runner identity. New static libraries follow `t64_input` (`:43-46`: `add_library(... STATIC ...)`, `target_include_directories(... PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})`, `target_link_libraries(... PUBLIC t64_common)`, `t64_target_warnings`). New runner tests follow the `test_audio_capture` block (`:422-514`): executable, `target_compile_definitions(... ${T64_HOST_DEFS})`, `add_dependencies` on supervisor/checker/fixture targets, one `add_test` per subcommand, `set_tests_properties(... LABELS "runner;controls" TIMEOUT 120 ENVIRONMENT ...)`. Python harness tests: `harness.audio-qualification` `:516-530` (`add_test(NAME ... COMMAND python3 -B ... --selftest)`, `LABELS "harness;qualification" TIMEOUT 120`).

#### `scripts/qualify-phase09.py` (stub, qualifier, batch), `scripts/tests/test_phase09_qualification.py`

**Analog:** `scripts/qualify-audio.py` and `scripts/tests/test_audio_qualification.py`. The 09-02 stub is a reduced copy that must fail closed (zero cases fails). Copy:

- Header constants: `ROOT`, `INVENTORY`, `EXPECTED_INVENTORY_SHA256`, profile/fixture pins (`qualify-audio.py:24-31`), the `Refusal` class (`:84`), `SOURCE_PATHS` bound-set tuple (`:68-80`; the new tuple must list every Phase 9 bound file), `OWNER_METADATA_PATHS` (`:81`).
- Mode structure: mutually exclusive `--selftest/--emit-hosted/--native/--collect-hosted/--verify-evidence/--verify-record/--scan-pii` (`:2504-2527`) and the shared refusal handler (`:2601-2606`); `--selftest` delegates to the test file with a timeout (`:2529-2536`).
- GCC13: import the helpers by file path (`_rdp_helpers` `:1003-1010`) and require the pinned image id (`EXPECTED_GCC13_IMAGE`, `:1000`) plus a required-units set (`PHASE08_GCC_REQUIRED_UNITS` `:994-999`; Phase 9 needs its own for all changed C units, with `-Wframe-larger-than=2048` coverage as in `_validate_gcc13_closure` `:1060-1078`).
- Native plan: `_native_command_plan` `:946-991` builds explicit normal and ASan command lists with timeouts; keep this exact shape.
- Test file: `CASE_NAMES` tuple `:30-52`, qualifier loaded with `importlib.util` `:54-57`, `main` with `--list-cases`/`--selftest` `:1054-1066`.

#### `scripts/check-phase09-cases.py`, `docs/spec/phase09-cases.json` (gate, batch)

**Analog:** `scripts/check-audio-cases.py` and `docs/spec/audio-cases.json` (copy; do not parameterize the Phase 8 checker: it is bound). Behavior to preserve exactly:

```python
        actual = listed.stdout.splitlines()
        if not actual or len(actual) != len(set(actual)):
            return fail(f"{relative} case list is empty or duplicated")
        expected = all_cases.get(relative)
        if actual != expected:
            return fail(...)
```
(`check-audio-cases.py:231-239`.) Also: `load_inventory` `:84-137` (schema id and version, suites with `ctest` and `binaries[{path,cases}]`), `ctest --show-only=json-v1` discovery `:200-221`, `LastTest.log` retention before/after discovery `:192-202`, exact `-R` regex, `--parallel 4`, `--output-junit` unique path and "passed without its JUnit archive" refusal `:266-298`. JSON shape: `{"schema":"t64.audio-case-inventory","schema_version":1,"suites":{"<suite>":{"ctest":[...],"binaries":[{"path":"core/test_ai","cases":[...]}]}}}` (`docs/spec/audio-cases.json:1-30`); new schema id e.g. `t64.phase09-case-inventory`. The `--list-cases` order in each test binary must equal the JSON order (the audio list in `core/tests/test_ai.c:2793-2826` mirrors suite order). Note `check-audio-cases.py:245-264` also runs `scripts/check-rsp-no-hle.py` (N1) in `--run` mode; keep that call in the copy.

#### `docs/claims/phase09.md` (doc/claim; K11)

**Analog:** `docs/claims/TEMPLATE.md` for rules and `docs/claims/phase08.md` for prose shape. For 09-02, the skeleton uses ```` ```t64-claim-example ```` fences only (the checker ignores them; a ```` ```t64-claim ```` block with placeholder counts fails `gov.claims.repo`). Prose first, then the block:

```text
```t64-claim
build_id: 0.0.0+t64-build-v1-<64 hex>
profile_id: <registered id>
tier: unit-tested
fixture_set: <set name>
manifest_digest: sha256:<64 hex>
pass: 1 / total: 1 / guest_fail ... skipped: 0   (seven counts sum to total)
oracle_kind: ... / oracle_revision: ...
not_claimed: other builds, profiles, fixtures ...; commercial software
statement: Tendies64 <build_id> on profile <id>: unit-tested for <set> @ sha256:... (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: <kind> <revision>. Not claimed: <not_claimed>.
```
```
(`docs/claims/phase08.md` block; format and the byte-exact rendered sentence are in `docs/claims/TEMPLATE.md`.) Wording rules (CONTEXT): the words "accurate", "compatible" and "supports" never appear in a Phase 9 claim block or in a prose line that starts with the project name. One block per profile id and fixture set (RESEARCH 09-11).

#### `scripts/rights-scan.sh`, `tools/rights/t64_blobscan.c` (gate, batch; N3)

**Analog:** same files. Mutation-control shape to copy for each new extension/magic (`scripts/rights-scan.sh:564-599`): `mk NAME` makes a one-file repo, `printf 'plain\n' > "$ST/ext/x.sav"`, then `case_run denied-ext-text 1 R-DENIED-EXT "$ST/ext" --tree`; near-miss controls that must stay clean (`notes.save`, `x.sav1`, `sav`, expected exit 0, `:595-599`); trailing-byte and case controls (`a.sav `, `C.SAV.`, `d.Eep<TAB>`). Container-magic controls follow `:666-691` (`bzh-text` clean control after the positive). The table row for a 3-letter extension is one string in `exts[]` (`tools/rights/t64_blobscan.c:534-535`); a magic row is one `has_prefix` line in `container_magic` (`:489-501`), e.g. `if (has_prefix(h, n, "T64STATE", 8u)) return 1; /* state */`. Upload allowlist mutation control: the `upload-extra.yml` mutation in `scripts/check-ci-yml.sh:549` (append an extra path to the audio upload list and expect `CI-UPLOAD-ARTIFACT`).

#### `docs/spec/raw-capture.md`, `fixtures/manifest.json`, `scripts/gate.sh`

- `docs/spec/raw-capture.md:122-123` holds the "Known limit" paragraph (`RLIMIT_FSIZE` of result bound plus 64 KiB, about 68 KiB); record the artifact-pair backstop change there.
- `fixtures/manifest.json` row shape (entries array, schema `fixtures/manifest.schema.json`, ids `^[a-z0-9][a-z0-9-]*$` max 64, `source.revision` 40-hex): copy the `audio-pcm-literal` and `audio-pcm-wrong-sample` rows (`:658-710`): `source{repo,revision,path}`, `build{recipe,toolchain:["GNU Binutils 2.45"]}` with the recipe quoting source-content SHA-256, ROM SHA-256 and size, `profile`, `source_rights:"authored-here"`, `binary_rights:"cleared"`, `distribution:["build-only"]`, `rights_evidence`, `notices[]`, `expected{kind:"self-checking",source,revision:"source-content-sha256:<hex>"}`. The wrong-value row describes the single defsym change and cites the harness oracle. Because `source.revision` must be a commit that already contains the source, the manifest row lands in a commit after the `.S` (RESEARCH P6).
- `scripts/gate.sh:100-104`: add a `"$ROOT"/build/gate/09-<tag>-private/*) ;;` arm next to the Phase 08 arm (N2).

---

### 09-03: SI/PIF/controller tracer and first hosted data

#### `core/t64_cpu.c` (controller, request-response; D-44, first task)

**Analog:** same file. The budget today:

```c
  return translation.physical_address < machine->profile.rdram_bytes ? 0u
                                                                     : 1u;
}
```
(`core/t64_cpu.c:1509-1511`.) Add a profile-gated branch for `t64_has_si_profile(machine)` when the physical address is in the SI window: 2 for the RD64B register and 1 for WR64B (D-44); legacy budgets stay exactly as above (pinned by old-profile controls). The caller `cpu_preflight_successor` (`:1513-1550`) already enforces queue room and sequence headroom atomically:

```c
  needed_events = 1u + cpu_successor_extra_event_budget(machine, opcode);
  ...
  if (machine->queue_count > queue_limit ||
      needed_events > queue_limit - machine->queue_count) {
    return T64_STATUS_QUEUE_FULL;
  }
  ...
  if (machine->sequence_exhausted ||
      machine->next_sequence > sequence_limit ||
      (uint64_t)(needed_events - 1u) >
          sequence_limit - machine->next_sequence) {
    return T64_STATUS_SEQUENCE_EXHAUSTED;
  }
```
(`:1526-1546`.) Test with the exact analog `core/tests/test_cpu_time.c:982-1093` (`test_cold_t1_preflight_before_mutation`): `reset_t1_cpu`, `fill_t1_cpu_queue(&machine, capacity - 1u)` or `machine.next_sequence = UINT64_MAX`, `t64_cpu_step`, then assert status, `retired_instructions == 0`, unchanged device latches and I-cache line. Use the direct-state setup (`:81-96`, N6) for the small queue and narrow sequence width.

#### `core/t64_scheduler.c` (service, event-driven; K16 capacity check)

**Analog:** extract the capacity rule from `cpu_preflight_successor` (`core/t64_cpu.c:1531-1546`, above) into an internal scheduler function (queue room for n events, sequence headroom, tick-add overflow), declared in `core/t64_internal.h` next to `t64_schedule_reserve` and used by both the CPU preflight and the SI start. Constraints to preserve from the existing API:

```c
  if (machine == NULL || reservation == NULL || !reservation->valid ||
      reservation->machine != machine ||
      reservation->expected_tick != machine->tick ||
      reservation->expected_queue_count != machine->queue_count ||
      reservation->event.sequence != machine->next_sequence ||
```
(`core/t64_scheduler.c:157-161`.) Any commit invalidates other outstanding reservations and `t64_schedule_cancel` (`:192-224`) does not restore `next_sequence`, so: check capacity, then reserve and commit sequentially. Pinned bodies: `t64_schedule_cancel` and `event_can_dispatch` have digest rows (N1); a new function does not, but any edit to those two does.

#### `core/t64_si.c` (device, event-driven)

**Analog:** `core/t64_pi.c` for the DMA state machine and `core/t64_ai.c` for profile-gated register dispatch and per-device event ids.

- Start sequence, sticky error on busy start, token and active descriptor (`core/t64_pi.c:48-118`):

```c
  if (pi->active_valid) {
    /* The sticky busy-start ERROR effect is the selected profile policy. */
    pi->write_length = raw_length & T64_PI_LENGTH_MASK;
    pi->error = true;
    trace_refusal(machine, UINT32_C(0x0c), T64_STATUS_BUS_DEVICE_REFUSED);
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  ...
  status = t64_schedule_reserve_after(machine, delay, T64_DEVICE_PI,
                                      T64_PI_EVENT_COMPLETE,
                                      (uint64_t)length, token,
                                      t64_pi_complete, &reservation);
  ...
  status = t64_schedule_commit(machine, &reservation);
```
  SI differs: two events (execute at E, complete at C) via the K16 capacity check, then reserve+commit twice; completion tick derives from the event's own tick, never the per-advance budget (D-36 mutant list).
- Complete handler with stale-token tolerance and atomic effect plus cache/link invalidation (`:250-295`): copy the stale-token guard (`:259-263`, returns `T64_STATUS_OK` on a canceled/old token), the effect-at-event-tick structure (use `event->tick` for trace, `:292-293`), and for RD64B (PIF to RDRAM) the invalidation calls `t64_rdp_memory_coverage_invalidate` and `t64_cpu_link_invalidate_range` (`:280-283`). MI pending via `t64_mi_set_pending(machine, T64_MI_PI_INTERRUPT)` (`:291`); STATUS write clears via `t64_mi_clear_pending` (`:218`).
- Register dispatch by offset with 4-byte-only bus width and `BUS_UNSUPPORTED` default: `t64_pi_read_register` `:131-189`, `t64_pi_write_register` `:191-248`; profile gate at the top of each AI function (`core/t64_ai.c:259`, `:303`: `if (!machine->has_image || !t64_has_ai_profile(machine)) { return T64_STATUS_NOT_READY; }`), and "write-only registers return `T64_STATUS_BUS_UNSUPPORTED` on read" (`:264-267`).
- Reset: `memset(&machine->pi, 0, ...)` then re-init tokens (`:297-303`) and AI's event cancellation loop (`core/t64_ai.c:597-621`) show the two reset idioms; SI cancels its own events like AI.
- Refusal tracing: `trace_refusal` packs `(reason << 32) | offset` into one trace value (`core/t64_pi.c:18-24`).
- Lint (S1 below): no allocation names, no mutable file-scope statics.

#### `core/t64_joybus.c` (parser, transform)

**Partial analog:** `core/t64_rom.c:56-95` for pure bounded byte decoding and normalization with explicit literal tables and `memcmp` against `static const` arrays:

```c
static bool detect_order(const uint8_t *bytes, t64_rom_order *order_out) {
  static const uint8_t big_endian[4] = {0x80u, 0x37u, 0x12u, 0x40u};
  ...
  if (memcmp(bytes, big_endian, sizeof big_endian) == 0) {
```
(`core/t64_rom.c:56-74`.) The parser itself (escape bytes, TX/RX counts, five channels, bounded loop over 64 bytes, guest-visible outcomes instead of refusals) has no analog: see "No Analog Found". Keep it a pure function over a 64-byte buffer plus a small input/state view so the fuzz target and vector tests need no machine.

#### `core/tests/test_si.c`, `core/tests/test_joybus.c` (test, event-driven/transform)

**Analog:** `core/tests/test_ai.c` is the template for a device test file.

- Setup helpers: `read_openipl3_asset` via `T64_TEST_IPL3_PATH` (`:25-42`), counting-free `test_allocate/test_release` (`:44-67`), `create_admitted_profile` (`:69-86`), `advance_guest_to` and `advance_guest_to_in_slices` (`:128-147`), `submit_*` register helpers (`:165-199`).
- Authored guest code written directly into RDRAM (no assembled ROM needed for in-process cases): `start_ai_spin_loop` (`:155-163`) writes a branch-to-self at `0x1000` and sets `cpu.pc`; use the same for CPU-store cases.
- Case dispatch (copy exactly): `--list-cases` prints every case name; `--suite NAME` runs a named group and finishes with `T64_TEST_FINISH("unit.core.<area>.<suite>")`; `--case NAME` for single cases (`:2792-2976`). A suite function prints `case <name>` before each case body (`:2771-2790`); the inventory checker relies on the `--list-cases` list, not these lines.
- RD64B/WR64B direction control: assert both directions literally and a swapped-direction mutant (D-09). Independent timing literals: tests compute E and C from the D-14 formula written out in the test (n=1, m=4 gives 10,876 ticks, RESEARCH validation row), never from a production helper.
- In-flight mutation witness and "effect atomic at the completion event": `case_inflight_source_mutation` (`core/tests/test_ai.c:1010`) is the AI analog for WR64B-reads-at-start versus RD64B-copies-at-completion.
- Atomic refusal assertions: `core/tests/test_pi_dma.c:651-742` (fill queue with `t64_test_schedule_synthetic`, digest before/after equal, latch untouched, then `sequence_exhausted = true`, then `tick = UINT64_MAX - 8`):

```c
  status = t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                 UINT32_C(15));
  check_status("PI start refuses when completion queue is full", status,
               T64_STATUS_QUEUE_FULL);
  ...
  check_u64("Queue-full refusal leaves canonical machine state unchanged",
            memcmp(digest_before, digest_after, sizeof digest_before) == 0,
            UINT64_C(1));
```
- Input latch partition tests: legacy encoding pins to keep green: `core/tests/test_isviewer.c:631-740` (refusals; non-zero port refused, `:675-678`) and `:742-829` (value `0x01ff1234` ordering, `:807-811`); the strictly-after latch is new behavior under the family only. Same-tick store/sample order cases: `case_same_tick_store_sample` and `run_cpu_store_sample_order` (`core/tests/test_ai.c:1477-1482`).

`core/tests/test_joybus.c` follows `test_boot_entry.c`'s "independent literal first" discipline: author the 64-byte PIF images from the field table before decoder breadth (AUTONOMY raw-command preflight), 16 single-button vectors as literals.

#### `core/tests/fuzz_joybus.c` (fuzz, batch, ASan-only)

**Analog:** `core/tests/fuzz_rom_admission.c`: a deterministic campaign with a fixed xorshift32 seed and an independent oracle, never `rand()`:

```c
static uint32_t next_random(uint32_t *state) {
  uint32_t x = *state;

  x ^= x << 13u;
  x ^= x >> 17u;
  x ^= x << 5u;
  *state = x;
  return x;
}
```
(`:420-428`; `uint32_t random = UINT32_C(0x7a64c0de);` `:445`; sampled expensive checks `i % 1000u == 0u` `:464`; independent offset oracle `encoded_offset` `:430-439`; bounds `FUZZ_MUTATIONS 250000u` `:10`.) Registration: only in the ASan block, with `TIMEOUT 120 LABELS fuzz`:

```cmake
  add_executable(test_fuzz_rom_admission tests/fuzz_rom_admission.c)
  ...
  add_test(NAME fuzz.rom-admission COMMAND test_fuzz_rom_admission)
  set_tests_properties(fuzz.rom-admission PROPERTIES TIMEOUT 120 LABELS fuzz)
```
(`core/CMakeLists.txt:766-778`.) Add the new fuzz name to `ASAN_ONLY` in `scripts/check-test-inventory.sh:281-282`, not to a `PHASE09_TESTS` variable (the gate preset must not expect it).

#### `core/t64_warm_devices.c`, `core/t64_mi.c`, `core/t64_bus.c` (SI window gating)

**Analog:** the AI window routing. The bus decode and the warm stub:

```c
  if (address >= T64_AI_BASE &&
      address < T64_AI_BASE + T64_DEVICE_WINDOW_BYTES) {
    return t64_has_ai_profile(machine) ? T64_BUS_REGION_AI
                                       : T64_BUS_REGION_WARM;
  }
  if (address == T64_SP_PC_ADDRESS ||
      (address >= T64_SI_BASE &&
       address < T64_SI_BASE + T64_DEVICE_WINDOW_BYTES)) {
    return T64_BUS_REGION_WARM;
  }
```
(`core/t64_bus.c:134-143`.) Add a `T64_BUS_REGION_SI` (like `T64_BUS_REGION_AI`, declared at `:48`) selected for the family and keep the old line for every other profile; touch every `switch`/`if` that names `T64_BUS_REGION_AI`: `:306` (region_bytes list), `:355` (probe), `:453-460` (read), `:542`, `:636`, `:699`, `:788`, `:883-884`. The PIF window (`T64_PIF_BASE 0x1fc00000`..`0x1fc00800`, `:34-35`) stays unsupported (D-08); `:159-163` is where it is classified. The legacy SI_STATUS stub lives in `core/t64_warm_devices.c:97-99` (read 0) and `:167-174` (zero-only write clears `T64_MI_INTERRUPT_SI`); leave it for old profiles. Old-profile pins that must stay: `core/tests/test_bus.c:238-245,267-270` (SI window and PIF reads) and `core/tests/test_warm_devices.c:552-555`.

#### `core/t64_observe.c` (service, event-driven; latch)

**Analog:** same file. The input event today is observation-only:

```c
t64_status t64_input_event_handler(t64_machine *machine,
                                   const t64_event *event) {
  if (machine == NULL || event == NULL ||
      event->device != T64_TRACE_DEVICE_INPUT ||
      event->kind != T64_TRACE_KIND_INPUT_SUBMITTED) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  return T64_STATUS_OK;
}
```
(`core/t64_observe.c:86-94`.) For the family the handler promotes `next` to `current` when `event->tick` reaches `visible_from` (use `event->tick`, K5), and `t64_machine_queue_input` (`:96-122`) keeps its checks and its value packing (`buttons | x<<16 | y<<24`, `:113-114`); the port goes in value bits 32-33 under the family only (D-03; legacy value `0x01ff1234` pinned at `core/tests/test_isviewer.c:807-811`). The refusal order (`port != 0u`, axis range, `NOT_READY`, reserve, commit, `:103-121`) must remain observable for legacy profiles.

#### `fixtures/pad/poll.S`, `poll.ld`, `scripts/build-pad-fixture.sh` (fixture)

**Analog:** `fixtures/audio/pcm.S` (194 lines) and `scripts/build-audio-fixture.sh`. Copy section by section:

- Header and mode: `.set mips3 / noreorder / noat` (`:2-4`), `.section .text.start`, `_start` (`:7-9`).
- Mask CP0 interrupts so the guest does not divert into IPL3's vector (`:10-16`): `mfc0 $t2, $12; addiu $t3, $zero, -2; and ...; mtc0 $t2, $12`. For the Count witness read CP0 Count with the same `mfc0` form using register 9.
- Copy authored literal words from `.rodata` into uncached RDRAM with CPU loads/stores (`copy_commands` loop `:18-32`, `rdp_commands` data `:169-179`, uncached base `lui $t1, 0xa000; ori $t1, $t1, 0x3100`); this is the template for building the literal joybus block at the PIF image address.
- Device register writes through `lui $t0, 0xa450`-style uncached addresses (`:49-64`): SI base is `0xa4800000`.
- Poll loop on a status register (`wait_for_audio` `:101-105`) and a bounded wait loop (`:116-120`).
- ISViewer report: magic handshake, copy bytes into the buffer window, flush with a halfword count store (`:134-165`); `hello_text_length` guard with `.error` on length drift (`:5,181-185`). For multi-line output, the count-lane behavior is exercised by `core/tests/test_isviewer.c:988-1040` (`test_count_lanes_and_repeat`). N8: keep the total under 4096 bytes.
- Completion: `break` at the final retired PC (`:164-165`) matches the row's `completion_pc`.
- Wrong-value variant: `.ifdef T64_WRONG_SAMPLE` ... `.else` ... `.endif` with `--defsym` (`:187-194`); the pad-poll variants (one wrong byte, wrong port-1 flag, tick-shifted script is host-side) use the same mechanism. Same source, one ROM per variant, one manifest row each.
- Linker script: copy `fixtures/audio/pcm.ld`.

#### `runner/tests/test_pad_poll.c` (test, request-response)

**Analog:** `runner/tests/test_rdp_video.c` (686 lines), the smaller of the two real-runner test templates: `run_supervised` `:221-405`, `result_is_pass` `:114-151`, `checker_accepts` `:92-113`, fixture-replacement refusal `:152-220`, `run_case` `:548-645`, `main` with `--list-cases` `:646-686`. Three-case structure to copy: accepted run, same-setup wrong value reaching the same comparator, replacement-fixture refused (`accepted-cpu-fill-to-raw-frame`, `same-setup-wrong-pixel`, `replacement-fixture-refused`; registrations `runner/CMakeLists.txt:394-418`). The supervisor command-line shape to reproduce is the argument list in `runner/tests/test_audio_capture.c:636-705` (`--real --result --meta --work-dir --wall-ms --bound --log-limit --fixture-id --fixture --fixture-sha256 --input --input-size --input-sha256 --expect-pass ...`). Expected transcript is a Python-oracle literal held outside the ROM (K4), passed to the test.

#### `scripts/qualify-phase09.py` real collector/verifier, `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh`

**Analog:** the Phase 8 hosted path. Collector/verifier functions to copy in shape: `_collect_hosted` `scripts/qualify-audio.py:716-831`, `_verify_hosted_originals` `:548-646`, `_compare_hosted_pair` `:647-678` (platform-invariant fields only; build ids differ per platform by design), `_build_pii_upload_layout` `:679-692`, `_verify_public_pair` `:693-707`, `_emit_hosted` `:2406-2501`, `_public_projection` `:380-500` (closed key set `PUBLIC_KEYS` `:63-67`), `_scan_public` `:1537-1564`. Early verifier-tail replay against real retained hosted artifacts is a CONTEXT process rule (F-098).

CI shape: three steps per job (case run, emit, PII scan) mirrored in both jobs, in the audio order, before the upload step:

```yaml
      - name: Run authored audio qualification records
        run: python3 -B scripts/check-audio-cases.py --build build/gate --suite qualification --run --artifact-prefix "08-05-${GITHUB_JOB}-${GITHUB_RUN_ID}-${GITHUB_RUN_ATTEMPT}-${GITHUB_SHA}"
      - name: Emit authored audio evidence
        run: python3 -B scripts/qualify-audio.py --emit-hosted --job "$GITHUB_JOB" --sha "$GITHUB_SHA" ...
```
(`.github/workflows/ci.yml:34-37` and `:81-84`; PII step `:48-49`; upload `:50-` with a `path: |` list of exact files.) `check-ci-yml.sh` edits (N4): the counters at `:243-262` and step-name recognition at `:292-294`, the exact-string `else if (val == "...")` rungs at `:198-205`, count constants (`job_runs != 9`, `step_count != 12`, `nruns != 18`), ordering conditions `:109-129`, `expected_upload_path` `:79-84`, and selftest mutations modeled on `:495-549` (removed step, reordered step, altered command, altered prefix, extra upload file, block-scalar run line). Both hosted jobs and ten-minute limits are unchanged (CONTEXT).

#### `docs/ci-reviews/LOG.md`, `docs/spec/si-pif-controller.md`

- Log rows are appended only through `scripts/ci-review.sh RUN_ID --append docs/ci-reviews/LOG.md [--finding "id|finding|taken/declined|reason"]` (header `scripts/ci-review.sh:1-24`; rows are never edited). Warm and cold timing rows (`LOG.md` runs table, cold macOS 461-507 s per CONTEXT).
- Spec structure: `docs/spec/audio-interface.md` headings (`Scope and evidence`, `Modeled registers`, `Timing and source policy`, `Raw packet API`, `Determinism checks`, `Authored tracer`). Source URLs and fetch dates go in the spec, facts restated in authored words (D-42).

---

### 09-04: save devices

#### `core/t64_save.c` (device/storage, CRUD) and `core/t64_pi.c` SRAM start path

**Analogs:** storage and gating from the AI device; DMA from PI.

- EEPROM `busy_until` compare at handshake execute ticks and no scheduler event (D-20): there is no analog for a deadline-compare device. The nearest is the VI/CP0 deadline fields (`count_compare_deadline` in `core/t64_digest.c:138`), used as data compared at a known tick. Compute `busy_until` from the execute tick E (K5 mutant: store tick would be wrong).
- SRAM DMA reuses PI: today `case UINT32_C(0x08)` in `t64_pi_write_register` always refuses (`core/t64_pi.c:208-210`: `pi->read_length = value & T64_PI_LENGTH_MASK; return refuse_semantic(machine, offset);`). For SRAM profiles only, add the read-length start path (`start_rom_dma` is the template, `:48-118`), storing direction and target kind in new `t64_pi_state` fields (`core/t64_internal.h:187-209`) and applying the effect atomically at the completion event (`t64_pi_complete` `:250-295`: copy into RDRAM or into the save buffer, then invalidate coverage and link). Duration equation `base + length/divisor` (`:79-87`) holds for both directions.
- Domain 2 carve-out: `decode_region` lists `T64_DOMAIN2_B_BASE` in the unsupported arm (`core/t64_bus.c:104-108`); the SRAM window `[0x08000000, 0x08008000)` must be matched before that arm and only for the SRAM id, with the old-profile pin `core/tests/test_bus.c:261-262` (`0x08000000` -> `BUS_UNSUPPORTED`) unchanged. Window size/width semantics (32-bit aligned word CPU/host access only) follow the `width != 4u -> T64_STATUS_BUS_UNSUPPORTED` convention for PI/AI registers (`core/t64_bus.c:441-460`).
- Storage: EEPROM 512/2,048 bytes and SRAM 32,768 bytes as trailing regions keyed by `save_kind`, measured and assigned in the same style as the coverage regions (`core/t64_machine.c:2230-2272, 2386-2415`); initial contents EEPROM `0xFF`, SRAM `0x00`; cleared on replacement and cleared of `busy_until` (D-25, replacement table `docs/spec/machine-contract.md:388-410`).
- Digest `save-media` section: kind, size, `busy_until`, all bytes (D-27), emitted like the audio packet buffers in the v24 section (`core/t64_digest.c:787-815`, bytes hashed with length prefixes).
- Channel-4 responder in `core/t64_joybus.c`: exact tx/rx lengths per command and `0x40`/`0x80` flags are literal tables (see joybus above).
- Refusal statuses `SAVE_UNKNOWN_DEVICE`, `SAVE_UNSUPPORTED_DEVICE`, `SAVE_DEVICE_MISMATCH`: add to the header status list (`core/include/tendies64/t64_machine.h:69-88`); every refusal asserts a bit-identical digest (S3).

#### `core/tests/test_save.c`, `runner/tests/test_save_fixtures.c`, `fixtures/save/*.S`, `scripts/build-save-fixtures.sh`, `docs/spec/save-media.md`

Same analogs as row 36/43/41/42/44 respectively: `core/tests/test_ai.c` + `core/tests/test_pi_dma.c` (duration, atomic effect: `core/tests/test_pi_dma.c:221-277` `test_profile_pi_duration_controls_completion_tick`, `:509-579` range boundaries); `runner/tests/test_rdp_video.c`; `fixtures/audio/pcm.S`; `scripts/build-audio-fixture.sh` (one script per family, `--defsym` for the 4 Kbit/16 Kbit variants and the wrong-byte variant); `docs/spec/audio-interface.md`. Block patterns embed the block index (RESEARCH validation: `0x41 & 0x3F = 1`). K3 applies: busy-window expiry (375,000 ticks, `eeprom-busy-ceiling-6ms-v1`) can only be observed in a real-runner guest if the per-fixture tick cap was raised in 09-02; otherwise assert it in-process (`unit.core.save.eeprom-busy-triple`).

---

### 09-05: save import/export and persistence

#### `core/t64_save.c` import/export (service, CRUD)

**Analogs:** (1) size-query/copy-out with `BUFFER_TOO_SMALL` reporting the required length and no mutation: `t64_audio_copy`.

```c
  if (capacity < required) {
    *length_out = required;
    return T64_STATUS_BUFFER_TOO_SMALL;
  }
  if (bytes_out == NULL && required != 0u) return T64_STATUS_INVALID_ARGUMENT;
```
(`core/t64_ai.c:666-670`; argument range/disjointness preamble `:647-659` using `t64_internal_make_range`, `t64_internal_observation_range_is_disjoint`, `t64_internal_ranges_overlap`, the same in `core/t64_observe.c:7-15,27-42`.) The D-24 check order (admitted, pointer/length, device present, kind known, kind supported, kind equals profile, format known, length equals size, nothing in flight) is tested row by row; every row before the first caller-byte read.

(2) format transforms RAW/SWAP16/WORDREV32: the repo already defines these conventions for ROM input; the save transforms must follow the same byte permutations:

```c
  } else if (order == T64_ROM_ORDER_BYTE_SWAPPED) {
    for (i = 0u; i < length; i += 2u) {
      canonical[i] = input[i + 1u];
      canonical[i + 1u] = input[i];
    }
  } else {
    for (i = 0u; i < length; i += 4u) {
      canonical[i] = input[i + 3u];
      ...
```
(`core/t64_rom.c:76-95`.) This supports RESEARCH A1 (SWAP16 swaps bytes inside each 16-bit half-word, WORDREV32 reverses each 32-bit word); the 8-byte literal vectors must still be authored by hand. Independent oracle for a transform: `encoded_offset()` in `core/tests/fuzz_rom_admission.c:430-439` (an offset-mapping function that does not call the production transform).

Replacement semantics (D-25): extend `t64_machine_reset_execution_state` / `reset_guest_state` (`core/t64_machine.c:1793-1804, 1931-1971`) and the replacement table.

#### `runner/t64_real.c` save import/export wiring; `runner/tests/test_save_roundtrip.c`; `fixtures/save/roundtrip.S`

**Analogs:** import through the verified-input fd path: `t64_verified_input_prepare(source_path, expected_size, expected_sha256_hex, max_bytes, &output)` (`runner/t64_verified_input.h:35-38`; supervisor use at `runner/t64_supervisor.c:1386-1402`; `T64_VERIFIED_HARNESS_MAX_BYTES 64 MiB` `:12`). Export through the new publisher (row 15). Result JSON and logs carry kind/length/format/flags only, never save bytes (D-26): `t64_real_result` has no free-form field (`runner/t64_real_result.h:23-47`), so add nothing to the closed `t64.result` v2 record; use detail strings from the closed vocabulary or a separate record. Test shape: `runner/tests/test_audio_capture.c` `actual-av-bundle` (accepted run then byte-for-byte comparison of the published files, `:1129-1388`) for round-trip, plus an altered length/format refused before guest start (`controls.real.save-import-refused`).

---

### 09-06: CORE-02 ordering matrix

#### `core/tests/test_sched_matrix.c` (test, event-driven)

**Analogs:** whole-versus-split and interleaved-machine isolation from `core/tests/test_ai.c`:

```c
  T64_CHECK(advance_guest_to(split, first_tick - 1u));
  T64_CHECK(advance_guest_to(split, first_tick));
  T64_CHECK(advance_guest_to(split, second_packet_first_tick));
  ...
  T64_CHECK(memcmp(digest_whole, digest_split, sizeof digest_whole) == 0);
```
(`case_whole_versus_split` `:2303-2373`; `case_interleaved_machine_isolation` `:2375-2462` uses four machines, two interleaved and two isolated references, comparing digests). Cross-device PI+SP version with trace modes and a drain-less/drain comparison: `core/tests/test_determinism.c:162-283` and `:285-356`, which also asserts no host allocator calls during advance (`advance_without_host_calls` `:147-160`). The resource-limit stop then resume case needs `advance_event_budget` (4096) exhaustion; `T64_STOP_REASON_RESOURCE_LIMIT` paths are in `core/t64_scheduler.c:399-419`. Count-wrap case (K14): direct-state setup of `core/tests/test_cpu_time.c:60-96` and the whole-versus-split pattern at `:315-361`, calling `t64_cp0_synchronize_guest_time` directly; do not drive a 2^33-tick span through `advance` (a stopped CPU returns before time moves, `core/t64_scheduler.c:390-394`). MI is sampled at CPU step start (`core/t64_cpu.c:2175-2180`); the matrix claims "MI ordered through its producers' events" only.

**Required mutants (K5 replacements):** latch `visible_from` from `machine->tick` at queue time instead of the stamped tick; EEPROM `busy_until` from the RD64B store tick instead of E; SI completion from the per-advance budget; sequence taken at commit instead of reserve; promote/sample order reversed. Mutation mechanism: out-of-tree patched copy that builds and fails the named test (Phase 8 practice; in-tree `#ifdef` mutants in core are forbidden by build-id hashing and lint). Structure of a mutation-control inventory with named seeds: `scripts/tests/test_rsp_no_hle.py` `MUTATION_SEEDS` (`:31-`) and its per-seed (file, symbol, description) tuples (`:542-543`).

---

### 09-07: replay

#### `runner/t64_replay.c/.h` (service, transform/file-I/O)

**Analogs:** the `t64.input` v1 module is the structural template; `t64_real_result.c` is the canonical-writer template.

- Public API shape (`runner/t64_input.h`): status enum with `T64_INPUT_OK`, `INVALID_JSON`, `INVALID_SCHEMA`, `EVENT_LIMIT`, `INVALID_EVENT`, `TIME_REVERSED`, `CANONICALIZATION_FAILED`; `t64_input_error{status, input_offset}`; parse into an owned schedule with the caller's output unchanged on refusal; borrowed canonical bytes plus SHA-256 until destroy (`:15-62`).
- Strict parse: bounded `t64_json_limits`, closed keys with duplicate detection, `node_int` range checks, u64 as decimal string via `t64_json_u64_string`:

```c
static bool closed_keys(const t64_json_node *object,
                        const char *const *keys, size_t key_count) {
  ...
  for (node = object->first; node != NULL; node = node->next) {
    ...
        if (seen[i]) return false;
```
(`runner/t64_input.c:46-74`; `parse_event` `:87-124`; limits literal `t64_json_limits limits = {T64_INPUT_MAX_BYTES, 3u, 64u, 1024u};` `:200`.) Replay needs caller-raised limits (default max nodes 4,096, `tools/common/t64_json.h:33`, so 65,536 ops cannot parse with defaults; A4).
- Canonical writer: sorted keys written manually in order, every u64 as a decimal string, finished with `t64_jsonw_finish`, identity = SHA-256 of canonical bytes (`runner/t64_input.c:144-194`; same idiom with sorted keys `runner/t64_real_result.c:38-125`).
- Port field restricted to 0 (K7): `node_int(port_node, 0, 0, &value)` at `runner/t64_input.c:106-107`.
- Recorder wraps the actual accepted `t64_machine_queue_input` call: today the real runner pre-queues the schedule before the first advance (`runner/t64_real.c:1139-1155`) and advances in 16-tick slices with inclusive bounds (`:28`, `:1157-1172`); the recorder records `at = 0` for pre-queued events and `at = completed tick` for later submissions; absolute stop targets (Pitfall 5).

#### `runner/tests/test_replay.c`, `fixtures/replay/mix.S`, `scripts/build-replay-fixture.sh`, goldens

**Analog (tests):** `runner/tests/test_input.c`: parser contract with mutation of every key and bound (`:127-366`), then actual-machine partition runs on fresh instances with trace-enabled 16-tick slices versus 4096-tick untraced slices and equal final digests (`execute_case`, `prepare_machine` `:446-483`, results asserted at `:728-747`): the "`at` must diverge when mutated" and "equal digests across partitions" controls extend this. Original run driven from an independently authored `t64.input` file (the format at `runner/t64_input.c`), built like `build_schedule_json` (`runner/tests/test_input.c`, used `:692-706`). TAP `check(...)` helpers (`:20-60`) rather than `t64_test.h`.

**Goldens:** build-independent literals (ops and checkpoint digests as literals, header composed at test time) follow the discipline of `core/tests/test_profile.c:784-795` (literal hashes for pinned artifacts) and `docs/spec/audio-cases.json`; sanitizer and gcc:13 builds record fresh (Pitfall 7: `T64_BUILD_ID` hashes every core source and the compiler flags, `core/CMakeLists.txt:20-66`).

**Analog (fixture/spec):** as rows 41/42/44.

#### `runner/t64_real.c`, `runner/t64_supervisor.c` replay flags

Same raw-capture-pair template as row 23/24. Separate closed `t64.replay-report` record (not `t64.result` v2): copy the `t64_real_result` writer idiom (closed keys written in sorted order, `schema`/`schema_version` last-but-sorted) and add a checker case in the spirit of `runner/t64_result_check.c`; the supervisor already validates and re-emits the real result (`real_report_decode` `runner/t64_supervisor.c:958-1046`).

---

### 09-08 and 09-09: state export, container, import

#### `core/t64_state.c` (service, transform)

**Analogs:** writer = the `profile_writer` triple (N7); size-then-copy-out = `t64_profile_serialize` (`core/t64_machine.c:2145-2175`; call shape `t64_profile_serialize(profile, bytes, capacity, &length)` as used by `core/tests/test_profile.c:845-847, 877-879`) and the hash-mode use of the same writer in `t64_profile_hash` (`:2130-2142`) and `t64_audio_copy` (S3). Section ids and per-section SHA-256 use `tools/common/t64_sha256.h` (`t64_sha256_init/update/final/hex`); the core is lint-admitted for it (`core/CMakeLists.txt:114`, `tools/lint/rules.txt` L1-INCLUDE-UNLISTED allows `string.h`, `stdint.h` etc. and `tendies64/*.h`). No allocation: the 09-09 reader runs under `L1-NAME-ALLOC`, `L1-STATIC-MUTABLE` (`tools/lint/rules.txt`) and `-Wframe-larger-than=2048` (`core/CMakeLists.txt:111-113`, K8: use bitsets or a machine-owned scratch field, not a 4 KiB stack array). In the ASan preset the frame warning is non-fatal (`core/CMakeLists.txt:766-770`), so a frame violation shows only in the normal gate: check the normal preset first.

Closed handler table for import (D-07/D-30): the same `(device, kind)` ledger as 09-01.

#### `core/tests/test_state_export.c`, `core/tests/test_state_import.c`, `core/tests/fuzz_state_import.c`

- Export tests: independent reference decoder with literal offsets: `core/tests/test_boot_entry.c:595-804` is the in-repo template for "re-implement the byte stream in the test, never call the production helper", used with a literal expected value (`:395-412`).
- Import tests: refusals leave the live machine bit-identical (digest before/after, `core/tests/test_pi_dma.c:677-707`); boundary construction sources named in D-32 with a "boundary reached" assertion on the machine state before export (builders to confirm: `core/tests/test_ai.c` two-descriptor streams `:2261-2287`, PI active/canceled `core/tests/test_pi_dma.c:785-943`, pending/dirty CPU states `core/tests/test_cpu_time.c`, near-exhaustion `core/tests/test_scheduler.c:331-479`).
- Fuzz: copy `core/tests/fuzz_rom_admission.c` (campaign stats struct `:43-61`, deterministic xorshift, sampled expensive checks, allocator with failure modes `:8-14,82-127`) and register ASan-only (S4). Structure-aware mutants keep magic and checksums valid and each reach their specific refusal (Mutant-must-compile).
- `runner/tests/test_state_runner.c` follows `runner/tests/test_audio_capture.c` with a real-runner state-size control under the raised FSIZE (`controls.real.state-export-fsize`).

#### `scripts/check-state-field-use.py` (gate, batch; D-45)

**Analog:** `scripts/check-rsp-no-hle.py` (comment/literal-blanking scanner and function-body extraction `:470-549`, a table keyed `"core/<file>.c :: <function>"` and a refusal on any unmapped/changed entry `:815-825`) with its test harness `scripts/tests/test_rsp_no_hle.py` (mutation seeds, `CHECKER` module loaded via `importlib` `:20-30`). The state-field audit maps every state field used as an index, length, shift, loop bound or array subscript to its import range rule and fails on an unmapped field; register it as a CTest like `repo.rsp-no-hle` (`runner/CMakeLists.txt:170-176`: `add_test(... COMMAND python3 -B ... )`, `LABELS runner TIMEOUT 60`, `PYTHONDONTWRITEBYTECODE=1`) named `harness.state-field-use-audit`.

#### `docs/spec/state-container.md`

Heading structure from `docs/spec/audio-interface.md`; digest framing text cross-references the rewritten `docs/spec/machine-contract.md:246-305`.

---

### 09-10 and 09-11

- `09-REVIEW.md` and `09-SECURITY.md`: copy the heading structure of `.planning/phases/08-audio-interface-and-raw-capture/08-REVIEW.md` (Summary, Narrative Findings: Critical Issues `CR-nn`, Warnings `WR-nn`) and `08-SECURITY.md` (`Threat verification` per plan, `CI and command-construction checks`, `Findings`, `Unregistered flags`, `Review claims confirmed against code`, `Not verified here`). Review against real mutated blobs from the 09-09 sweep (D-41).
- `docs/evidence/phase09-*.md`: copy `docs/evidence/phase08-audio.md` structure (`Native record` table: complete gate CTest count, full ASan run, GCC13 unit count, inventory case/CTest counts with snapshot SHA-256, source inputs bound with manifest hash, native record SHA-256; `Hosted record` table per job with run id, duration, artifact id, archive digest, manifest SHA-256; `Authored case and independently expected facts`; `Review and repair history`; `Independent review results`; `Limitations and preserved stops`).
- `docs/claims/phase09.md` real blocks: copy the `docs/claims/phase08.md` block verbatim in shape; one block per profile id and fixture set; seven counts sum to total; `statement` is generated byte-exact from the keys (rule in `docs/claims/TEMPLATE.md`, selftest `sh scripts/check-claims.sh --selftest`).
- Literal pins replace the symbolic accessor: `core/tests/test_profile.c:784-795` style hashes/lengths for the four new ids; `scripts/qualify-phase09.py` pins like `qualify-audio.py:24-31` (`EXPECTED_INVENTORY_SHA256`, `PROFILE_SHA256`, `FIXTURE_SHA256`, `INPUT_SHA256`). Data pins only; no logic edits (D-06).

---

## Shared Patterns

### S1. Core determinism lint and build-identity coupling (all new `core/t64_*.c` and core test files)

**Source:** `tools/lint/rules.txt`, `core/CMakeLists.txt:8-13,20-66,102-118`.
**Apply to:** `core/t64_si.c`, `t64_joybus.c`, `t64_save.c`, `t64_state.c` and every core edit.

- New `core/t64_*.c` files are picked up by `file(GLOB T64_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/t64_*.c")` and enter `T64_BUILD_ID` automatically (`:8-10`, `:20-66`); no CMake edit is needed to add a source, but N1's canonical list must be updated.
- Forbidden in core (lint): allocation names (`L1-NAME-ALLOC`), mutable file-scope statics and globals, stdio, time/entropy/file/thread/env names, floats, `__int128`, inline asm, constructor attributes, host includes beyond `stdint.h stddef.h stdbool.h stdalign.h limits.h string.h` and `tendies64/*.h` (`tools/lint/rules.txt`; `include "t64_sha256.h"` is admitted via the include directory `core/CMakeLists.txt:108`).
- Core-only flags `-Wdouble-promotion -Wframe-larger-than=2048` (`:112-113`) with `T64_WERROR=ON` in the gate preset (`CMakePresets.json`).
- Constants that are file-local `#define`s at the top of the `.c` (`core/t64_pi.c:6-16`, `core/t64_ai.c:8-21`) are the house style for device registers; shared ids go in `core/t64_internal.h`.

### S2. Core test binary registration (CTest + case list + inventory)

**Source:** `core/CMakeLists.txt:567-606` (AI/profile block), `core/CMakeLists.txt:626-651` (plain tests), `scripts/check-test-inventory.sh:174-194,281-299`.
**Apply to:** every new core test binary and every new CTest name.

```cmake
add_executable(test_ai tests/test_ai.c)
target_include_directories(test_ai PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_ai PRIVATE t64_core)
t64_target_warnings(test_ai)
add_dependencies(test_ai t64-boot-fixtures)
add_test(NAME unit.core.ai.registers
  COMMAND test_ai --suite registers)
...
set_tests_properties(unit.core.ai.profile unit.core.ai.old-profile-refusals
  unit.core.ai.registers unit.core.ai.cadence unit.core.ai.output
  unit.core.ai.determinism
  PROPERTIES LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
```
(`core/CMakeLists.txt:574-604`.) Tests that read internal state add `${CMAKE_CURRENT_SOURCE_DIR}` to the include directories (`:627-629`) and include `t64_internal.h` (`core/tests/test_ai.c:7`, `core/tests/test_scheduler.c:8`); tests that only use the public API do not (`core/tests/test_profile.c`). Tests that admit the open IPL3 ROM need `add_dependencies(... t64-boot-fixtures)` and the `T64_TEST_IPL3_PATH` environment. A test that checked nothing is a failure (`tests/t64_test.h:70-79`, `T64_TEST_FINISH`).

Inventory registration is a data edit to `scripts/check-test-inventory.sh`: add

```sh
PHASE09_TESTS='unit.core.profile.v12-identity
...'
```
beside `PHASE08_TESTS` (`:174-194`) and the extra `"$PHASE09_TESTS"` argument in `expected_names` (`:297`); ASan-only names (fuzz) go in `ASAN_ONLY` (`:281-282`). K13: the selftest builds a removal control per name for both presets, so target at most about 45 new CTest names and group cases with `--suite`.

### S3. Refusal postconditions: bit-identical digest (every refusal test)

**Source:** `core/tests/test_pi_dma.c:651-742`, `core/tests/test_isviewer.c:631-740`, `core/tests/test_ai.c:397-442`.
**Apply to:** SI start/refusals, save selection/import/export refusals, state import refusals, replay refusals.

Pattern: capture `t64_machine_digest` before; perform the refusing call and assert the exact status; capture after and `memcmp` equal; also assert the specific side latches/queue/sequence/tick (`core/tests/test_ai.c:409-441` snapshots `ai_before`, `queue_count`, `next_sequence`, `tick`, `mi_pending`, `mi_mask` and compares). Each refusal case must first prove an accepted baseline in the same setup (AUTONOMY): see the accepted-then-refused rows in `core/tests/test_isviewer.c:662-731`, where each refusal uses a fresh machine so the baseline is independent. Fill the queue with `t64_test_schedule_synthetic(machine, tick, T64_TEST_EVENT_NOOP, 0u)` up to `machine->profile.event_queue_capacity` (`core/tests/test_scheduler.c:346-358`), force sequence exhaustion with `machine->sequence_exhausted = true` or `machine->next_sequence = UINT64_MAX` (`core/tests/test_pi_dma.c:696`, `core/tests/test_cpu_time.c:1051`), tick overflow with `machine->tick = UINT64_MAX - 8u` (`core/tests/test_pi_dma.c:710`).

### S4. ASan-only fuzz registration

**Source:** `core/CMakeLists.txt:758-786`.
**Apply to:** `fuzz_joybus`, `fuzz_state_import`.

Add the executable and `add_test(NAME fuzz.<name> ...)` with `set_tests_properties(... TIMEOUT 120 LABELS fuzz)` inside the `if(T64_HAS_ADDRESS_AND_UNDEFINED_SANITIZERS)` block, never in the gate preset; list the name in `ASAN_ONLY`. Long fuzz, ASan, gcc:13, state-hash-heavy loops and the replay soak stay native (D-40).

### S5. Authored-fixture chain (every fixture plan)

**Source:** `fixtures/audio/pcm.S`, `fixtures/audio/pcm.ld`, `scripts/build-audio-fixture.sh`, `core/CMakeLists.txt:181-206`, `fixtures/manifest.json` rows, `runner/t64_real_fixture.c:49-58`.
**Apply to:** admit (09-02), pad-poll (09-03), eeprom/sram (09-04), roundtrip (09-05), replay-mix (09-07).

Chain, in order: (1) `.S` and `.ld` under `fixtures/<family>/`; (2) `scripts/build-<family>-fixture.sh` (pinned IPL3 asset/payload hashes and qualified-tool receipt, `--build|--qualify`); (3) CMake custom command and target:

```cmake
set(T64_AUDIO_FIXTURE_OUTPUTS
  "${T64_BOOT_FIXTURE_DIR}/audio-pcm.z64"
  ...)
add_custom_command(
  OUTPUT ${T64_AUDIO_FIXTURE_OUTPUTS}
  COMMAND "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}"
    sh "${CMAKE_SOURCE_DIR}/scripts/build-audio-fixture.sh" --build
  DEPENDS
    "${CMAKE_SOURCE_DIR}/fixtures/manifest.json"
    "${CMAKE_SOURCE_DIR}/fixtures/audio/pcm.S"
    "${CMAKE_SOURCE_DIR}/fixtures/audio/pcm.ld"
    "${CMAKE_SOURCE_DIR}/scripts/build-audio-fixture.sh"
    "${CMAKE_SOURCE_DIR}/tools/fixture/t64_rom_pack.c"
    ...
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building authored CPU audio and VI fixtures"
  VERBATIM)
add_custom_target(t64-audio-fixture DEPENDS ${T64_AUDIO_FIXTURE_OUTPUTS})
add_dependencies(t64-audio-fixture t64-boot-fixtures)
```
(`core/CMakeLists.txt:181-206`); consuming tests `add_dependencies` on the target (`runner/CMakeLists.txt:428-429`); (4) manifest row (row 30); (5) runner table row with literal ROM and profile hashes, `completion_pc` equal to the authored `break` address, and `pass_eligible` (`runner/t64_real_fixture.c:49-58`; K4 adds the transcript pin). Build outputs under `build/fixtures` are build products, not analogs. `scripts/build-boot-fixtures.sh` is never edited (D-38; its hash is the Binutils cache key, `.github/workflows/ci.yml:27-31`). Wrong-value controls are `--defsym` variants of the same source (`T64_WRONG_SAMPLE`, `fixtures/audio/pcm.S:187-194`), each with a manifest row stating the single changed value, and each must reach the same comparator in the accepted setup.

### S6. Test-case dispatch convention (binary side) and JSON inventory (checker side)

**Source:** `core/tests/test_ai.c:2792-2976`, `core/tests/test_profile.c:937-967`, `runner/tests/test_audio_capture.c:3419-3481`, `scripts/check-audio-cases.py:84-298`, `docs/spec/audio-cases.json`.
**Apply to:** every new test binary that has named cases.

Binary: `--list-cases` prints one name per line and exits 0; `--case NAME` runs one case and finishes with `T64_TEST_FINISH("unit.core.<area>[.<suite>]")`; `--suite NAME` runs a group; no argument runs a default set; unknown name prints to stderr and returns 2. Checker: JSON `suites -> {ctest[], binaries[{path, cases[]}]}`; the checker runs every binary's `--list-cases` and requires equality (order included) with the JSON, so a green process alone cannot hide a case that did not run. Runner test binaries instead take positional subcommands plus the supervisor/checker/fixture paths (`runner/CMakeLists.txt:430-487`).

### S7. Runner option pair and forwarding rows (every runner flag)

**Source:** `runner/t64_supervisor.c:403-440` (args struct), `:450-515` (option table and seen flags), `:592-603` (pair validation), `:1496-1529` (child argv), `runner/t64_real.c:1576-1645` (child-side parse), and the two test-fault allowlists (`runner/t64_supervisor.c:623-645`, `runner/t64_real.c:1648-1670`).
**Apply to:** artifact pair, save import/export flags, state export/import flags, replay record/play flags.

Each flag: one `sup_opt` row, one `seen` bool, cross-validation before any guest starts (refuse undefined combinations), one forwarding push through the checked helper (K6), one child-side parse, and a refusal in the synthetic-mode branch (`:651-663`). Synthetic-mode flood control runs under the generic FSIZE line (`:1880`), not the real-mode line (`:1534`), so FSIZE evidence uses the real-mode test twin (K9).

### S8. Claim and evidence documents

**Source:** `docs/claims/TEMPLATE.md`, `docs/claims/phase08.md`, `docs/evidence/phase08-audio.md`, `tests/seeded/claims/` (seeded invalid claims), `scripts/check-claims.sh`.
**Apply to:** `docs/claims/phase09.md`, `docs/evidence/phase09-*.md`.

`sh scripts/check-claims.sh --files docs/claims/phase09.md` is the cheap-first check; `sh scripts/check-claims.sh --selftest` proves the checker. Tier names and phrases are fixed by the template table (`unit-tested`, `regression-pinned`, ...); the reserved words rule and the "line starting with the project name" rule apply to prose too.

### S9. Old-profile pins to keep green in every plan

- `core/tests/test_profile.c` `old-profile-pins` (the shipped Phase 8 id and earlier, `:772-914`).
- `core/tests/test_ai.c:397-442` (old profile AI refusals), `:348-395` (new profile inherited devices; template for "inherits the AI family's devices" under the new family).
- `core/tests/test_bus.c:225-274` (SI/PIF/SRAM-window reads on legacy profiles).
- `core/tests/test_warm_devices.c:552-555` (legacy SI_STATUS stub).
- `core/tests/test_isviewer.c:631-829` (legacy input refusal, value encoding, trace order, observer invariance).
- `core/tests/test_digest_trace.c:260-386`, `core/tests/test_boot_entry.c:395-412` (legacy digests through the sink).
- `core/tests/test_scheduler.c:331-479` (queue/sequence/tick atomicity for the base profile).

## No Analog Found

Files or mechanisms with no close committed match; the planner should use RESEARCH patterns and the cited partial analogs for structure only.

| File / Mechanism | Role | Data Flow | Reason |
|------------------|------|-----------|--------|
| v25 strip-and-compare differential (`core/tests/test_digest_v25.c`) | test | transform | No committed test compares two digest *streams* (only final digests). Build from the reference-encoder pattern; the v24 stream is the oracle. |
| Joybus frame parser (`core/t64_joybus.c`) | parser | transform | No existing escape-byte/length-prefixed bit-stream decoder; `core/t64_rom.c` supplies only the table-and-`memcmp` style. Author the field table and 64-byte vectors first (AUTONOMY raw-command preflight), then the decoder. |
| EEPROM busy-deadline device (`busy_until` compared at handshake ticks, no event) | device | request-response | Every existing timed device uses scheduler events; none compares a stored deadline at a host/guest access tick. |
| Out-of-tree mutant build harness (K5; RESEARCH Open Question 6) | test infrastructure | batch | Phase 8 practice is described but no committed driver exists; `scripts/tests/test_rsp_no_hle.py` mutates source copies in a temporary tree and is the nearest structure. |

## Metadata

**Analog search scope:** `core/` (all `t64_*.c`, `include/`, `tests/`, `CMakeLists.txt`), `runner/` (all modules, `tests/`, `CMakeLists.txt`), `scripts/` (qualifier, case checker, gates, scanners, build scripts, `tests/`), `tools/rights`, `tools/common`, `tools/lint`, `fixtures/`, `docs/spec`, `docs/claims`, `docs/evidence`, `docs/ci-reviews`, `.github/workflows/ci.yml`, `.planning/phases/08-audio-interface-and-raw-capture`.
**Files scanned:** about 90 read in whole or in targeted ranges; every analog path verified tracked with `git ls-files`.
**Pattern extraction date:** 2026-11-19
**Not verified here:** line numbers drift with any edit to `core/t64_internal.h`, `runner/t64_supervisor.c`, `runner/t64_real.c` or `scripts/check-test-inventory.sh` (RESEARCH validity note); no test or build was run by this mapper.
