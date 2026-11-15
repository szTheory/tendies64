# Phase 06: Cold Boot, CPU Timing, and Full BASE Suite — Pattern Map

**Mapped:** 2026-11-14  
**Analogs found:** 6 / 6 (two are near matches; see gaps below)

## File Classification

| New/Modified File or Area | Role | Data Flow | Closest Analog | Match |
|---|---|---|---|---|
| `core/t64_machine.c`, `core/t64_internal.h` | model/config | transform | same profile and capability seams | exact |
| `core/t64_digest.c` | utility | transform | existing state-domain hashing | exact |
| `core/t64_warm_devices.c`, `core/t64_bus.c`, `core/t64_mi.c` | service/model | request-response, event-driven | bounded warm MMIO and MI IRQ handling | near |
| `core/t64_cpu.c`, `core/t64_exception.c`, `core/t64_scheduler.c` | controller/scheduler | event-driven | fixed CPU event plus exception reservation | near |
| Phase-owned whole-BASE C inventory/aggregate (path to be named by planner) | utility | file-I/O, batch | `runner/t64_cpu_diag_protocol.c` | near |
| Phase-owned qualification context/entry (path to be named by planner) | utility | file-I/O, request-response | `scripts/verify-cpu-diagnostic-qualification.py` | near |
| Owning tests: `core/tests/test_profile.c`, `test_warm_devices.c`, `test_cpu_time.c`, `test_cpu_cp0.c`, `test_boot_entry.c`; `runner/tests/test_cpu_diag_protocol.c`; qualification controls below | test | transform, batch | corresponding tests | exact |

## Pattern Assignments

### Profile capabilities and state digest

**Analogs:** `core/t64_machine.c`; `core/t64_internal.h`; `core/t64_digest.c`.

The compat-v2 profile records separate descriptor, machine-model, and state versions (`core/t64_machine.c:285-303`). Add new profiles as explicit descriptors and registry entries; keep existing descriptors and state-domain bytes stable. Gate new behavior through a named profile predicate, following the paired model/state version checks in `core/t64_internal.h:597-629`.

**State digest pattern** (`core/t64_digest.c:169-191`):

```c
t64_sha256_init(&context);
if (has_v12_state) {
  t64_sha256_update(&context, domain_v12, sizeof domain_v12 - 1u);
} else if (has_v11_state) {
  t64_sha256_update(&context, domain_v11, sizeof domain_v11);
}
hash_u32(&context, machine->profile.state_serialization_version);
t64_sha256_update(&context, machine->profile_digest,
                  sizeof machine->profile_digest);
t64_sha256_update(&context, machine->content_digest,
                  sizeof machine->content_digest);
```

`core/tests/test_profile.c:713-738` preserves prior profile identity while asserting a new profile hashes differently. `core/tests/test_boot_entry.c:395-432` is the state-digest sensitivity/reference pattern; extend it for every new future-affecting protocol/timing field.

### Cold RI/RDRAM register state

**Analogs:** `core/t64_warm_devices.c:28-43,45-103,106-178`; `core/t64_bus.c:52-76,79-100,277-315`; `core/t64_mi.c:9-35,37-52`.

```c
status = validate_warm_access(machine, width);
if (status != T64_STATUS_OK) return status;
switch (address) {
case T64_RI_SELECT:
  if (!machine->guest_devices.ri_select_known)
    return T64_STATUS_BUS_UNSUPPORTED;
  value = machine->guest_devices.ri_select;
  break;
default:
  return T64_STATUS_BUS_UNSUPPORTED;
}
*value_out = value;
```

`core/t64_internal.h:493-499` shows the existing versioned `ri_select`/known-state fields. Bus decoding owns range and address classification; the register owner handles values/refusals; MI owns interrupt pending/mask transitions. Extend only the new cold profiles and serialize all new future state. Existing warm MMIO is a near match, not an RDRAM protocol implementation.

### Variable CPU interval and exception preflight

**Analogs:** `core/t64_cpu.c:1960-1974,2404-2422`; `core/t64_exception.c:76-101`; `core/t64_scheduler.c:88-146`.

```c
status = t64_schedule_reserve_after(
    machine, UINT64_C(1), T64_DEVICE_CPU, T64_CPU_EVENT_STEP, UINT64_C(0),
    UINT64_C(0), t64_cpu_step, &reservation);
if (status != T64_STATUS_OK) return status;
return t64_schedule_commit(machine, &reservation);
```

The current CPU interval is fixed at one tick. Preserve the reserve/commit seam, but compute T1 cost and validate the successor tick, queue capacity, and resource limits before instruction state changes. Exception entry (`core/t64_exception.c:90-101`) reserves and commits its next boundary before writing CP0/vector state; use that ordering for failure atomicity. Keep delay-slot retirement and CP0 timer/Random ownership in `core/t64_cp0.c`; owning test analogs are `core/tests/test_cpu_time.c:304-343,351-415` and `core/tests/test_cpu_cp0.c:697-710`.

### C inventory and aggregation

**Analogs:** `runner/t64_cpu_diag_protocol.h:84-137`; `runner/t64_cpu_diag_protocol.c:703-763,1021-1115`; `runner/tests/test_cpu_diag_protocol.c:494-535,720-735`.

```c
status = parse_batch(aggregate->inventory, bytes, length,
                     &registration_index, &value_index, &outcome);
if (status != T64_CPU_DIAG_OK) {
  aggregate->invalid = true;
  return status;
}
if (!valid_member(aggregate->inventory, registration_index, value_index,
                  &flat_index)) return T64_CPU_DIAG_INVALID_BATCH;
```

The C protocol owns canonical inventory parsing, member bindings, duplicate rejection, and completeness. Reuse it as the member protocol and test style. It covers CPU inventory only; the complete BASE membership join is new, so create one phase-wide C completeness authority over independently sealed full-source membership rather than repurposing the CPU projection as the denominator. Preserve original member records and identity checks.

### Qualification contexts

**Closest analog:** `scripts/verify-cpu-diagnostic-qualification.py:532-584,767-822,885-911`; test pattern: `scripts/tests/test_cpu_diagnostic_qualification_controls.py:41-63,222-256`.

```python
receipt_path = lexical_path(receipt_path, repository)
reject_symlink_chain(receipt_path, private_root, "qualification receipt")
...
if data["source_revision"] != SOURCE_REVISION:
    refuse("pinned source revision does not match the qualification")
```

The verifier checks a bounded private receipt, source seal, and qualified file closure before tool use. Controls create isolated authored fixtures and exercise the production entry point. There is no exact analog for immutable per-attempt context records: add a phase-owned context seam, bind it to independent source authority and the actual tested artifact, preserve failed contexts, and make accepted/rejected fixtures call the complete verifier and normal dispatch—not helper checks alone. A repeated digest is not authorization.

## Shared Patterns

- Version machine policies explicitly while freezing old profile identities; reserve events and preflight arithmetic/resources before mutation, preserving FIFO and device ownership.
- Keep register reads side-effect-free until success; return unsupported/refused effects at the owning layer and preserve unrelated state.
- Keep one C membership/completeness authority. Python may prepare commands and retain records, but does not recalculate the denominator.
- Run the complete pre-use verifier before creating a guest run directory. Keep private diagnostic identities/results out of public receipts and this map.

**No exact analog:** the cold RI/RDRAM protocol has no current cold-state implementation; whole-BASE inventory exceeds the CPU-scoped C inventory; immutable per-attempt contexts extend beyond the current receipt/source-closure verifier.

**Analog search scope:** exact CONTEXT/RESEARCH seams and their named owning tests only; no broad repository scan. **Pattern extraction date:** 2026-11-14
