# Phase 5: RSP Low-Level Emulation - Pattern Map

**Mapped:** 2026-11-13  
**Files analyzed:** 14 proposed file groups  
**Analogs found:** 14 / 14 groups (role/data-flow analogs; no VU semantic oracle)

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `core/t64_internal.h` | model/state | stateful | `core/t64_internal.h` | role-match |
| `core/t64_rsp.c` | interpreter | fetch/execute, transform | `core/t64_cpu.c` | role-match for dispatch/refusal only |
| `core/t64_sp.c` | device/controller | register request, scheduled DMA | `core/t64_sp.c` | exact for existing SP contract; extend by profile |
| `core/t64_bus.c` | bus | request-response, memory I/O | `core/t64_bus.c` | exact |
| `core/t64_machine.c` | profile/config | request-response, transform | `core/t64_machine.c` | exact |
| `core/t64_digest.c` | serializer/utility | transform | `core/t64_digest.c` | exact |
| `core/CMakeLists.txt` | config | build/test registration | same | exact |
| `core/tests/test_rsp_*.c` | test | request-response, state transition | `core/tests/test_sp_dma.c`, `core/tests/test_profile.c` | role-match |
| `scripts/check-test-inventory.sh` | utility | batch/inventory | same | exact |
| `tools/lint/core-lint.sh` | utility | batch/source scan | same | exact |
| `runner/t64_rsp_diagnostic.c` | runner/controller | request-response, bounded guest run | `runner/t64_cpu_diagnostic.c` | role-match |
| `runner/t64_rsp_diag_protocol.[ch]` | protocol/utility | batch/transform | `runner/t64_cpu_diag_protocol.[ch]` | role-match; keep RSP schema sealed separately |
| `runner/tests/test_rsp_diag_protocol.c` | test | batch/transform | `runner/tests/test_cpu_diag_protocol.c` | role-match |
| `scripts/*rsp-diagnostic*.py` (qualification/build/run) | utility | file I/O, batch | matching `scripts/*cpu-diagnostic*.py` | role-match |

## Pattern Assignments

### RSP state, profile, digest, and events

**Analogs:** `core/t64_internal.h`, `core/t64_machine.c`, `core/t64_digest.c`, `core/t64_sp.c`

- State is machine-owned: `core/t64_internal.h:413-429,431-467` defines tick/sequence events, reservation state, machine memory, device and CPU state. Add RSP state beside existing device/CPU state, and keep host observer cursors out of guest identity.
- Profiles are designated immutable records: `core/t64_machine.c:151-195` lists CPU/SP ratios, scheduler and SP policies, queue limits, and descriptor/model/state versions. `core/t64_machine.c:896-930` is the lookup seam. Add an explicit RSP-capability profile and preserve prior descriptors/IDs.
- Digest versions are explicit, not ordered capabilities: `core/t64_digest.c:123-181` selects a domain by state serialization version; `:233-250` hashes SP latches and queued events; `:292-300` shows version-gated CPU state. Add a new domain and serialize every future-affecting RSP field/event; do not change old encodings.
- Event work follows reserve/commit before state mutation: `core/t64_sp.c:192-229`; stale completion tokens are ignored at `:306-313`. Reuse scheduler reservation and stable sequence ordering for issue/DMA events.

### Interpreter and semantic boundary

**Analog:** `core/t64_cpu.c:2054-2110,2175-2203,2290-2304`

The existing CPU path fetches through the bus, records the fetched opcode, dispatches by family, and stops unsupported operations explicitly. Copy that production-route/refusal structure for fetched IMEM instructions. `core/t64_cpu.c` COP1/COP2 code is **not** a VU arithmetic oracle; there is no existing VU semantic core. Implement readable byte/lane-based VU semantics in the new RSP module, with independently derived vectors and explicit transactional refusal for unsupported cases.

### SP registers, bus routing, and DMA

**Analogs:** `core/t64_sp.c:57-121,168-229,232-295,297-369`; `core/t64_bus.c:79-111,358-377,502-518,533-541`

SP validates and decodes the full DMA descriptor before reserving an event, distinguishes active/pending/full, validates exact completion token/tick, then transfers through bounded bank offsets and promotes the queued descriptor. Register reads/writes validate address/width and use explicit offset switches. Extend these seams for RSP status/PC/semaphore and profile-specific beat timing; keep IMEM/DMEM backing and the bus's notification behavior connected.

### Native tests, inventory, and lint

**Analogs:** `core/tests/test_sp_dma.c:127-180,365-414,889-948`; `core/tests/test_profile.c:539-573,607-623`; `core/CMakeLists.txt:439-446`; `scripts/check-test-inventory.sh:361-444`; `tools/lint/core-lint.sh:314-390,597-603`

Use machine/bus-visible setup and assert bytes, tick/event order, refusals, and digest changes. Profile tests pin the new descriptor and hash sensitivity. Register every native test in CMake and the exact gate/ASan inventory; inventory checking rejects missing, extra, duplicate, disabled, or empty registrations. The production lint scan includes `core/**/*.c` and headers while excluding `core/tests`; place the interpreter in `core` and keep host-only test helpers in tests.

### Runner protocol and qualified component scripts

**Analogs:** `runner/t64_cpu_diag_protocol.h:84-137`; `runner/t64_cpu_diag_protocol.c:178-205,423-455,1111-1178`; `runner/t64_cpu_diagnostic.c:1107-1140,1247-1278`; `scripts/run-cpu-diagnostic.py:711-775,778-805,860-878`; `scripts/verify-cpu-diagnostic-qualification.py:532-584,885-910`

Reuse shared JSON parse/canonicalization/encoding and aggregation primitives while giving RSP its own versioned schema and exact inventory. The runner refuses non-local execution and non-private outputs before use. The host runner creates a fresh per-member variant, records a bounded result, and delegates completeness to the production adapter. The qualifier binds a closed receipt to the pinned source, tool, asset, and dependency evidence. Keep RSP member outcomes distinct (pass/fail/incomplete/refused/harness error) and preserve local-only/private storage boundaries.

## No Analog Found

| File/area | Role | Data Flow | Reason |
|---|---|---|---|
| VU instruction semantics in `core/t64_rsp.c` | interpreter | transform | No VU implementation exists. CPU COP1 is not a vector arithmetic oracle; use the phase's primary guide and independent vectors. |

## Metadata

**Analog search scope:** named `core`, `runner`, `scripts`, and `tools/lint` files identified by phase context/research; targeted reads only.  
**Tracked-source gate:** every named analog is git-tracked; no runtime/install mirror path is used.  
**Pattern extraction date:** 2026-11-13
