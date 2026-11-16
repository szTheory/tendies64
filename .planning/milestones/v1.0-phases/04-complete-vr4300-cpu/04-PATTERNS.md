# Phase 4: Complete VR4300 CPU - Pattern Map

**Mapped:** 2026-11-11  
**Files analyzed:** 23 candidate paths/roles (some new filenames remain TBD)  
**Analogs found:** 5 strong pattern families; no arithmetic or private CPU-suite equivalent

Only repository-tracked analogs are named below. “Path TBD” marks a planned role where CONTEXT/RESEARCH intentionally do not prescribe a filename. All new files are creations; no new API is implied by this map.

## File Classification

| New/Modified File or Role | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `core/include/tendies64/t64_machine.h` | model | request-response | `core/include/tendies64/t64_machine.h` | exact for public descriptor/API style |
| `core/t64_machine.c` | model | transform | `core/t64_machine.c` | exact for descriptor lookup/validation/serialization; partial for v3 seed |
| `core/t64_internal.h` | model | transform | `core/t64_internal.h` | exact for per-machine state and internal seams |
| `core/t64_digest.c` | utility | transform | `core/t64_digest.c` | exact for canonical state digest pattern |
| `core/t64_cpu.c` | controller | event-driven | `core/t64_cpu.c` | role match; current semantics are warm-v1 only |
| `core/t64_scheduler.c` | service | event-driven | `core/t64_scheduler.c` | exact for FIFO event ownership; partial for CPU-boundary clock order |
| `core/t64_bus.c` | service | request-response | `core/t64_bus.c` | exact for validated physical bus dispatch |
| `core/t64_pi.c`, `core/t64_sp.c` | service | event-driven | same files | exact for transactional DMA scheduling and device ownership |
| `core/CMakeLists.txt` | config | batch | `core/CMakeLists.txt` | role match; current globs cover top-level files only |
| New descriptor/spec and executable v9 seed (path TBD) | model | transform | `core/t64_machine.c` | role match; existing warm seed is not architectural reset state |
| `core/t64_exception.c` | service | event-driven | `core/t64_cpu.c`, `core/t64_scheduler.c` | partial; no precise guest-exception path exists |
| `core/t64_cp0.c` | model | request-response | `core/t64_cpu.c` | partial; existing CP0 behavior is warm-v1 policy |
| `core/t64_mmu.c` | service | request-response | `core/t64_cpu.c`, `core/t64_bus.c` | partial; current translation is KSEG direct mapping only |
| `core/t64_cache.c` | model | request-response | `core/t64_internal.h`, `core/t64_cpu.c` | partial; current cache state is not a functional data cache |
| `core/t64_cop1.c` | service | transform | `core/t64_cpu.c` | partial dispatcher style only; no COP1 behavior analog |
| `core/t64_softfloat.*` and any copied helper subtree | utility | transform | none | no analog; exact closure and paths depend on the pre-copy audit |
| Focused native CPU/profile/COP1 tests (`core/tests/test_*.c`, exact names TBD) | test | batch | `core/tests/test_profile.c`, `test_cpu_path.c`, `test_scheduler.c`, `test_digest_trace.c` | exact harness and assertion patterns |
| `runner/t64_verified_input.c` integration | utility | file-I/O | `runner/t64_verified_input.c` | exact snapshot boundary; do not admit unqualified diagnostic inputs |
| Separate private CPU diagnostic adapter (path TBD) | controller | file-I/O, request-response | `runner/t64_verified_input.c`, `runner/t64_real_result.c` | partial; no CPU diagnostic adapter exists |
| Private exact suite inventory/protocol (path TBD) | test | transform | `scripts/check-test-inventory.sh`, `tests/controls/INVENTORY` | partial; these inventory CTest controls, not external CPU membership |
| Private audited build/run script (path TBD) | config | batch, file-I/O | `scripts/check-test-inventory.sh` | partial; no diagnostic package lifecycle analog |
| Authored adapter controls and exact per-group statuses (path TBD) | test | batch | `runner/tests/test_input.c`, `runner/tests/test_real_runner.c` | partial; no external CPU result groups today |

## Pattern Assignments

### Profile descriptor, validation, seed, and digest

**Analogs:** `core/t64_machine.c`, `core/t64_internal.h`, `core/t64_digest.c`, `core/tests/test_profile.c`, `core/tests/test_digest_trace.c`.

The descriptor table uses immutable `static const` values, while each `t64_machine` owns a descriptor copy and its mutable CPU/device state. Keep new TLB/cache/FPR/timer/reservation fields per machine; share only immutable policy tables. `output_drain_cursor` is explicitly a host observer cursor and is excluded from guest identity (`core/t64_internal.h:202-238`).

**Validation and serializer pattern** (`core/t64_machine.c:251-295, 297-324`): validate bounded text and numeric invariants before exposing the descriptor; serialize integers in explicit big-endian bytes and prefix text with lengths.

```c
return profile != NULL && T64_PROFILE_TEXT_OK(profile_id) &&
       T64_PROFILE_TEXT_OK(console_target) &&
       profile->cpu_rsp_ratio_numerator > 0u &&
       profile->state_serialization_version > 0u;
```

```c
bytes[0] = (uint8_t)(value >> 24);
bytes[1] = (uint8_t)(value >> 16);
bytes[2] = (uint8_t)(value >> 8);
bytes[3] = (uint8_t)value;
profile_writer_bytes(writer, bytes, sizeof bytes);
```

`write_profile` is the concrete serialization order (`core/t64_machine.c:326-406`); `test_serialization_golden` compares independently authored bytes and a fixed hash (`core/tests/test_profile.c:190-220`). `test_each_descriptor_field_changes_hash` checks that individual fields affect identity (`core/tests/test_profile.c:259-336`). Reuse this fixed-golden/field-sensitivity test shape for profile v3 and the executable seed vectors.

**Compatibility hazard:** the current serializer appends asset/payload/policy digests and HLE seed version only under `descriptor_version == 2` (`core/t64_machine.c:397-405`). The new v3 descriptor must encode all boot metadata under an explicit v3 rule; do not copy a v2-only fallthrough. Likewise, `prepare_openipl3_warm_entry` and `reset_guest_state` are the qualified warm-v1 reset path (`core/t64_machine.c:467-527`), not a source for zero-CP0 architectural state. Keep descriptor v2/state v8 and the seven qualified hello fixtures byte-stable.

The digest starts with a serialization-version domain, then hashes profile/content identity and canonical machine state (`core/t64_digest.c:57-116`). Current warm-state hashing covers CPU/CP0/delay state and cache policy fields (`core/t64_digest.c:181-221`). Add every future-affecting v9 field explicitly and update the version domain/branches; keep observer cursors and host pointers out. `test_partitioned_and_isolated_advances_have_same_digest` is a useful per-instance isolation test shape (`core/tests/test_digest_trace.c:439-485`); `test_cp0_cache_state_is_in_digest` shows field mutation checks (`core/tests/test_cpu_path.c:737-762`).

### CPU attempt, exception, and scheduler boundaries

**Analogs:** `core/t64_cpu.c:891-1037`, `core/t64_scheduler.c:88-190, 282-294`, `core/tests/test_scheduler.c`.

CPU steps are scheduled through the central FIFO reservation/commit API; dispatch advances the machine tick and calls the selected device handler:

```c
status = t64_schedule_reserve_after(
    machine, UINT64_C(1), T64_DEVICE_CPU, T64_CPU_EVENT_STEP, UINT64_C(0),
    UINT64_C(0), t64_cpu_step, &reservation);
if (status != T64_STATUS_OK) return status;
return t64_schedule_commit(machine, &reservation);
```

```c
queue_pop_front(machine);
machine->tick = event.tick;
t64_trace_emit(machine, event.tick, event.device, event.kind, event.value);
status = event.handler(machine, &event);
```

The current CPU path retires then calls `cp0_retirement_tick` and schedules the next step (`core/t64_cpu.c:1024-1036`). That is only an old warm-v1 pattern: for the new profile synchronize Count/Compare and pending interrupts at each CPU boundary, commit CP0 effects after instruction execution, and schedule handler progress after a guest exception even when the instruction did not retire. Stage destination/memory effects until access and higher-priority-fault checks pass. Do not copy retirement-only timer behavior, nested-BD capture, old Status masking, or warm-v1 BREAK completion into the new profile. The corrected context owns these policies: Status writable mask `0xF757FFFF` after RP selection; capture EPC and BD together only when old EXL is clear; preserve both under old EXL; new-profile BREAK is architectural.

### Translation, cache, and bus/device writes

**Analogs:** `core/t64_cpu.c:115-250`, `core/t64_bus.c:324-440`, `core/t64_pi.c:191-248`, `core/t64_sp.c:166-228, 270-293`.

Current CPU translation returns only a physical address and accepts direct KSEG mappings. Keep its “translate before physical bus access” seam, but extend the result to carry cacheability/attributes before the bus path. CPU byte writes validate the full byte range before the first mutation (`core/t64_cpu.c:198-225`). The bus validates width, decodes the region, dispatches PI/SP registers, and traces only successful writes (`core/t64_bus.c:332-398`).

PI/SP DMA provide a useful transaction pattern: decode/validate, reserve the completion event, commit that reservation, then publish active/pending descriptor and token state (`core/t64_sp.c:187-227`). Adapt this staging discipline for CPU stores, SC decisions, cache writeback, and device writers; CPU virtual translation/cache rules are new and must not be inferred from PI/SP DMA semantics. The new DMA policy explicitly updates backing memory without snooping valid cache lines.

### Verified input, JSON, and local adapter protocol

**Analogs:** `runner/t64_verified_input.c:82-165, 205-264`, `runner/t64_input.c:43-68, 196-280`, `runner/t64_real_result.c:38-128`.

`t64_verified_input_prepare` copies a bounded input while hashing it, verifies a read-only snapshot against expected size/digest, then unlinks the temporary pathname before handing out the owned descriptor. The input parser rejects unknown/duplicate keys, bounds bytes/events, parses to a temporary schedule, and assigns `schedule_out` only after canonicalization succeeds. `t64_real_result_encode` uses the shared JSON writer, checks result invariants before emission, emits 64-bit counts as decimal strings, and writes schema/build/profile/source identity explicitly.

Use these seams for bounded local file input, canonical protocol bytes, explicit identity, and fail-closed result validation. The public real/hello adapter and its cleared-fixture collector remain narrowly scoped; do not widen them to the private diagnostic or point the new adapter at the old warm-v1 profile. Add adapter controls for missing/reduced/unreached members and separate guest failures from setup/harness failures.

### Build identity, lint, and exact inventories

**Analogs:** `core/CMakeLists.txt:8-32, 54-62, 98-109`, `tools/lint/core-lint.sh:335-345`, `scripts/check-test-inventory.sh:71-101, 111-180`, `tests/controls/INVENTORY`.

Current CMake globs only top-level `core/t64_*.c`, public headers under `core/include/tendies64`, and top-level internal `core/t64_*.h`; the build fingerprint hashes only `T64_BUILD_INPUTS`. A copied helper subtree therefore needs explicit production source/header and identity entries. The production core lint already recursively finds `.c/.h` under `core/`, excluding only `core/tests`, and runs source plus object lint; keep every copied helper inside that set or add explicit files if it is placed outside `core/`.

`check-test-inventory.sh` compares sorted registered CTest names with reviewed phase sets and rejects malformed/disabled/missing entries. Preserve old gate/ASan names and add exact new project-owned test names to the phase set. For the private CPU suite, create its own exact required CPU inventory and per-group value denominator/status record; `tests/controls/INVENTORY` is only the emulator-control inventory and is not a diagnostic denominator.

## No Analog Found

| File or role | Reason |
|---|---|
| Minimal generic binary32/binary64 SoftFloat closure | No existing IEEE arithmetic implementation; use only after the exact pinned closure, notices, dependencies, and context adaptation are audited. |
| Private nemu64 CPU overlay and adapter | No existing external CPU-suite runner or exact CPU registration/value inventory. The current runner handles a different, cleared hello fixture. |
| Functional TLB/cache/COP1 behavior | Existing direct mapping, cache metadata and integer dispatch offer integration seams only; they do not implement the requested behavior. |

## Metadata

**Analog search scope:** `core/`, `core/tests/`, `runner/`, `scripts/`, `tests/`, and `tools/lint/`; targeted source reads only.  
**Tracked-source gate:** every named analog path was confirmed by `git ls-files`.  
**External material:** no external emulator source, package, diagnostic ROM, or asset was opened or executed.
