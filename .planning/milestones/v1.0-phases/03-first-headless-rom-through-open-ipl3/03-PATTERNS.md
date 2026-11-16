# Phase 3: First Headless ROM Through Open IPL3 — Pattern Map

**Mapped:** 2026-11-10  
**Files classified:** 15 logical implementation units (module splits remain discretionary)  
**Analog assignments:** 6 exact/role matches, 8 partial matches, 1 without an analog; CPU execution has no working analog.

Paths marked “suggested” are module boundaries inferred from CONTEXT.md and RESEARCH.md, not existing files.

## File Classification

| New/Modified Unit | Role | Data Flow | Closest Tracked Analog | Match |
|---|---|---|---|---|
| `core/t64_internal.h`, `core/t64_machine.c` | model, boot, profile lifecycle | request-response | same | role-match |
| `core/t64_cpu.c` (suggested) | CPU execution | event-driven | none | none |
| `core/t64_scheduler.c` | scheduler | event-driven | same | exact |
| `core/t64_bus.c`, virtual-bus helper (suggested) | bus / address transform | request-response | `core/t64_bus.c` | partial |
| reset / RI policy (suggested) | device model | request-response | `core/t64_mi.c`, `core/t64_pi.c`, `core/t64_sp.c` | partial |
| `core/t64_digest.c` | state identity | transform | same | exact |
| ISViewer capture (suggested) | bounded output buffer | streaming | `core/t64_trace.c` drain ring | partial |
| `core/t64_trace.c`, timed-input hook | trace / scheduled events | event-driven | same + `core/t64_scheduler.c` | partial |
| authored `.S`, linker script, ROM packer | fixture / transform | batch | manifest contract only | partial |
| `fixtures/manifest.json` and tool pin data | provenance config | batch | `fixtures/manifest.schema.json` | role-match |
| `runner/t64_supervisor.c` | process / descriptor supervisor | file-I/O, request-response | same | partial; current path reopen is unsafe for this contract |
| real runner and report writer (suggested) | runner / report | request-response | synthetic runner only | partial |
| `runner/t64_result_check.c` | strict real-result checker | request-response | same v1 checker | partial |
| `core/CMakeLists.txt`, `runner/CMakeLists.txt` | build and test config | batch | same | exact |
| `scripts/check-test-inventory.sh`, `.github/workflows/ci.yml` | test inventory / CI config | batch | same | exact |

## Pattern Assignments

### Core machine, boot, profile, and reset state

**Analogs:** `core/t64_machine.c:384-431,449-499`; `core/t64_internal.h:120-157`.

Machine data is per-instance. ROM admission prepares and validates the candidate before swapping it into the machine; failure returns before machine-state mutation. Keep boot seeding/reset in this owner and give every software policy an explicit profile identity.

```c
status = t64_rom_candidate_prepare(..., &candidate);
if (status != T64_STATUS_OK) return status;
previous = machine->image_storage;
machine->image_storage = candidate.storage;
machine->image_length = candidate.length;
machine->has_image = true;
candidate.owns_storage = false;
reset_guest_state(machine);
```

The current profile is named `ntsc-cart-4m-skeleton`, boot status is `unimplemented`, and internal machine state has no CPU/CP0 state (`core/t64_machine.c:18-63`, `core/t64_internal.h:120-157`). This is ownership/layout guidance only; it does not implement CPU execution or establish warm-entry values.

### Scheduler and future CPU events

**Analog:** `core/t64_scheduler.c:88-127,315-352`.

```c
if (tick < machine->tick) return T64_STATUS_TIME_REVERSED;
if (machine->queue_count >= queue_limit(machine)) return T64_STATUS_QUEUE_FULL;
reservation_out->expected_queue_count = machine->queue_count;
reservation_out->expected_tick = machine->tick;
reservation_out->event.tick = tick;
reservation_out->event.sequence = machine->next_sequence;
reservation_out->valid = true;
```

Reserve validates capacity, time, and sequence before commit; events are ordered by `(tick, sequence)`. Time-bound advance dispatches events with `tick <= bound`, enforces the profile event budget, then advances the guest tick. Add CPU retirement to this queue/order contract; define cost and same-tick ordering before execution. Do not introduce a host-time loop.

### Bus, reset devices, input trace, and output drain

**Analogs:** `core/t64_bus.c:48-101,195-254`; `core/t64_mi.c:84-114`; `core/t64_trace.c:40-99,112-136`.

The current bus accepts physical/debug addresses and big-endian byte/word accesses. RI and several device windows decode as unsupported (`core/t64_bus.c:83-93`); it has no KSEG translation. Reuse the range/alignment checks and device-specific read/write dispatch, but add a distinct guest virtual-to-physical step. MI/PI/SP register handling is the closest reset-register model; no RI reset/probe or explicit zero-source policy exists.

The trace seam is a bounded per-machine ring. `t64_machine_trace_drain` validates caller output ranges, copies records, and reports dropped-record state (`core/t64_trace.c:40-99`). It is a useful shape for drain-after-advance output/input observability, but it is not ISViewer bytes: define byte capacity, overflow refusal, flush lanes, and copy/drain semantics separately. Core advancement may call internal deterministic device handlers; it must not call host clocks, files, threads, or host callbacks.

### Profile digest and state digest

**Analogs:** `core/t64_machine.c:161-231,331-382`; `core/t64_digest.c:57-100`.

Profile serialization uses ordered, explicit-width fields and SHA-256. State digest starts with a domain tag and `state_serialization_version`, then hashes profile/content identity and canonical machine state. Extend the canonical field order to every future-affecting CPU, CP0, boot, reset, output, and device field; bump profile/state versions and update independent goldens deliberately. Exclude paths, wall time, and compiler build identity.

### Fixture bytes and provenance

**Analog:** `fixtures/manifest.schema.json:17-66` (structural schema).

The manifest separates source rights, binary rights, recipe/toolchain, digest/size, distribution, notices, and expected-output provenance. Carry those fields to the authored `.S` fixture and pinned assembler/linker. No existing packer, MIPS assembly fixture, or tool-download recipe was found. Keep the packer narrow and deterministic; do not copy the existing `nemu64-test-base` manifest entry, which is a different fixture and has pending rights.

### Supervisor, real runner, and report

**Analog:** `runner/t64_supervisor.c:447-495,580-648,662-730,764-797`.

The supervisor has bounded regular-file checks (`lstat`, `O_NOFOLLOW`, `fstat`, capped reads), private work directories, process-group cleanup, and atomic result writes. Its present fixture path flow is not reusable as-is for descriptor attestation: `check_fixture` closes the verified descriptor at line 491, and launch later passes the original `--fixture` path at lines 628-647. Copy/hash into private storage, verify it, unlink it, then pass the already-open descriptor; never re-open the caller path. Apply the same transaction and cleanup discipline to schedule bytes and every failure path. The existing runner is synthetic; it does not exercise a guest or provide the real report contract.

### Strict result v2

**Analogs:** `runner/t64_result_check.c:43-55,428-445`; `runner/t64_model_result_check.c:23-28,66-70,126-176`.

The real checker already has a closed v1 key set, field types, enums, and consistency rules. The model checker shows exact-key checking and bounded JSON parse limits:

```c
if (root == NULL || root->type != T64_JSON_OBJECT ||
    !exact_keys(root, model_keys, ARRAY_LEN(model_keys))) return 0;
if (!str_is(t64_json_get(root, "schema"), "t64.model-result")) return 0;
node = t64_json_get(root, "schema_version");
if (node == NULL || node->type != T64_JSON_INT || node->num != 1) return 0;
```

Add real v2 with closed version-specific fields/validation and dual-read v1 while retaining current v1 bytes and goldens. Keep `t64.model-result` in its own executable/schema; do not use its checker as a shared result format. V1 currently requires `state_digest` null (`runner/t64_result_check.c:547-550`), so v2 must define its own strict rules.

### Build, test inventory, and hosted lanes

**Analogs:** `core/CMakeLists.txt:8-13,98-109,143-177`; `runner/CMakeLists.txt:50-69,71-78`; `scripts/check-test-inventory.sh:71-82,92-101`; `.github/workflows/ci.yml:15-42`.

Core production sources are `t64_*.c`, and build identity hashes source names/content. Existing CTest targets declare their executable dependencies; for example `test_skeleton_probe` depends on the producer and both checkers before it invokes them (`runner/CMakeLists.txt:50-67`). Add equivalent producer/checker dependencies for all consumer tests. Append phase IDs to the inventory without changing prior IDs. Both hosted jobs already call the single `./scripts/gate.sh`; extend that path for the pinned authorized fixture/toolchain, then compare guest identity/state separately from host build identity. Add new core modules to the established source/object lint input.

## Shared Patterns

- **Prepare, then commit:** `core/t64_rom.c:122-174` creates an owned candidate; `core/t64_machine.c:458-480` mutates the machine only after preparation succeeds. `core/t64_scheduler.c:88-127,149-166` similarly reserves then commits against captured queue/time state.
- **Bounded and closed inputs:** preserve access/length checks, strict JSON keys, canonical hashing, and explicit refusal outcomes; overflow, unsupported opcodes, and failed descriptor admission cannot become pass.
- **Core/host boundary:** use per-instance guest state, guest ticks, and a drain/copy seam. Host process, file, and deadline policy belongs in runner/supervisor.
- **Consumer build edges:** every CTest consumer that invokes a producer or checker must depend on those executable targets.

## No Analog Found

| Capability | Why no analog |
|---|---|
| CPU decode/retirement, CP0 and CACHE | No CPU/CP0 state or instruction execution exists in the core. |
| Guest KSEG virtual mapping | Existing bus is a physical/debug bus only. |
| RI reset/probe, prepared-RDRAM zero source | No RI model or such profile policy exists. |
| ISViewer byte protocol and timed-input parser | Trace ring is records-only; no byte capture or schedule parser exists. |
| Authored MIPS fixture packer/tool provisioning | No assembly fixture, linker script, packer, or cross-toolchain recipe exists. |
| Real guest runner/report and open-descriptor handoff | Current supervisor/runner are synthetic; supervisor reopens via the caller path. |

## Metadata

**Analog search scope:** tracked `core/`, `runner/`, `fixtures/`, `tools/lint/`, `scripts/`, `docs/spec/`, and `.github/workflows/` sources/configuration.  
**Pattern extraction date:** 2026-11-10. No build, test, or application was launched for this read-only mapping.
