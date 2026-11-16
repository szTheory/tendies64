# Phase 2: Deterministic Machine Skeleton - Pattern Map

**Mapped:** 2026-11-10  
**Files analyzed:** 15 inferred files (exact core names are discretionary)  
**Analogs found:** 7 / 15; new machine behavior has no existing analog

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `core/CMakeLists.txt` | config | build | `tools/common/CMakeLists.txt` | role-match |
| `core/t64_core_stub.c` (replace behavior) | service | request-response | same file | placeholder only |
| `core/include/tendies64/t64_facade.h` (preserve/extend if needed) | facade | request-response | same file | provisional seam |
| `core/include/tendies64/t64_machine.h` (candidate) | provider/API | request-response | facade header | role-match |
| `core/t64_machine.c` (candidate) | service | request-response | none | no analog |
| `core/t64_profile.c` (candidate) | utility | transform | none | no analog |
| `core/t64_scheduler.c` (candidate) | service | event-driven | none | no analog |
| `core/t64_bus.c` (candidate) | service | request-response | none | no analog |
| `core/t64_dma.c` (candidate) | service | event-driven | none | no analog |
| `core/t64_rom.c` (candidate) | service | transform | none | no analog |
| `core/t64_digest.c` (candidate) | utility | transform | SHA-256 primitive | partial |
| `core/t64_trace.c` (candidate) | service | event-driven | none | no analog |
| `core/t64_sha256.c`, `core/include/tendies64/t64_sha256.h` | utility | transform | `tools/common/t64_sha256.c/.h` | exact implementation |
| `core/tests/test_*.c`, `fuzz_rom_admission.c` | test | request-response / transform | common unit/fuzz tests | role-match |
| `docs/profile-limits.md` (candidate) | documentation | transform | none | no analog |

## Pattern Assignments

### Build and CTest

**Analogs:** `CMakeLists.txt` lines 15-22; `tools/common/CMakeLists.txt` lines 10-17; `CMakePresets.json` lines 17-25.

Move the core target from the root into the new `core/CMakeLists.txt`; retain
the shared C17 warning function and gate/asan presets. Register focused tests
with CTest; do not add a framework.

```cmake
function(t64_target_warnings tgt)
  target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic -Wshadow
    -Wconversion -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wvla ...)
  if(T64_WERROR)
    target_compile_options(${tgt} PRIVATE -Werror)
  endif()
endfunction()

add_executable(test_common tests/test_common.c)
target_link_libraries(test_common PRIVATE t64_common)
t64_target_warnings(test_common)
add_test(NAME unit.common COMMAND test_common)
set_tests_properties(unit.common PROPERTIES LABELS unit)
```

### Facade compatibility

**Analogs:** `core/include/tendies64/t64_facade.h` lines 11-16, 41-52;
`runner/t64_synthetic.c` lines 110-140, 169.

Keep the synthetic runner's behavior and Phase 1 result identity. Add machine
calls alongside the provisional facade or extend it deliberately; the API is
not an ABI. The supervisor serializes `t64_core_identity()` at
`runner/t64_supervisor.c:221`, so keep that consumer compatible.

```c
typedef struct {
  t64_stop_reason reason;
  uint64_t tick;
} t64_advance_result;
typedef struct t64_facade_ops {
  t64_advance_result (*advance)(void *inst, uint64_t bound_tick);
  size_t (*drain_guest_log)(void *inst, uint64_t *tick_out, uint8_t *buf,
                            size_t cap);
} t64_facade_ops;
```

### SHA-256 relocation and digest

**Analogs:** `tools/common/t64_sha256.h` lines 16-28;
`tools/common/t64_sha256.c` lines 104-115;
`tools/common/tests/test_common.c` lines 21-60.

Relocate the authored implementation into the linted core boundary; do not link
host `t64_common` (it also contains JSON/UTF-8 helpers). Its streaming API is:

```c
void t64_sha256_init(t64_sha256_ctx *ctx);
void t64_sha256_update(t64_sha256_ctx *ctx, const void *data, size_t len);
void t64_sha256_final(t64_sha256_ctx *ctx, uint8_t out[T64_SHA256_DIGEST_SIZE]);
```

The implementation writes encoded length/output big-endian. Serialize fixed-
width state fields explicitly before hashing; never hash structs, padding,
pointers, build identity, unused queue slots, or trace settings. Preserve the
known-vector and streamed-chunk `T64_CHECK_STR` tests after relocation.

### Assertions and admission fuzz

**Analogs:** `tests/t64_test.h` lines 21-28, 57-60;
`tools/common/tests/test_json_fuzz.c` lines 21, 35-44.

Every component test must assert; `T64_TEST_FINISH` also rejects a zero-check
test. The existing harness uses a pinned PRNG seed and bounded campaign. Fuzz
the production ROM admission/normalization/commit path with authored spans,
all three orders, short prefixes, header-field boundaries, allocation denial,
and refusal-state equality. Record seed, case inventory and reached fields;
run under the existing ASan+UBSan preset. The JSON mutations are a harness
style reference, not a ROM oracle.

```c
#define T64_TEST_FINISH(name) \
  (printf("%s: checks=%d failures=%d\n", name, t64_test_checks, \
          t64_test_failures), \
   (t64_test_failures == 0 && t64_test_checks > 0) ? 0 : 1)
```

### Core source/object lint

**Analogs:** `tools/lint/core-lint.sh` lines 347-359;
`tools/lint/lint-objects.sh` lines 1-11, 169-197; `tools/lint/rules.txt` lines 5-20.

Include every new core `.c`/`.h`, including relocated SHA, in the lint file
set. Layer 1 scans source; Layer 2 compiles each C file and checks sections,
constructors and unresolved symbols. Rules forbid host time/files/stdio/heap,
threads, entropy and mutable globals. Keep state per instance; lifecycle and
admission allocations use host callbacks.

## Shared Patterns

- Strict C17, extensions off, shared warning flags and `-Werror`; use CTest
  `gate` and ASan+UBSan `asan` presets.
- Host reads files; core accepts raw spans and has no host clocks, I/O,
  allocation, threads, entropy, floating point or mutable globals.
- Refuse before commit: failed admission preserves prior digest and observable
  state. Tests establish model behavior, not hardware timing or broad support.

## No Analog Found

No scheduler, N64 bus/device, DMA, profile, state serializer, trace ring or
ROM admission implementation exists. The only core C file is a seven-line
placeholder; use CONTEXT/RESEARCH for these behavioral contracts.

## Metadata

**Search scope:** `core/`, `runner/`, `tools/common/`, `tools/lint/`, `tests/`,
root CMake, phase context/research. All cited source analogs passed
`git ls-files`; no ignored mirror paths are named.  
**Pattern extraction date:** 2026-11-10
