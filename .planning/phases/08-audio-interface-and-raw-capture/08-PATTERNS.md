# Phase 08: Audio Interface and Raw Capture - Pattern Map

**Mapped:** 2026-11-18  
**Files/areas analyzed:** 17 proposed or modified areas  
**Closest analogs found:** 17 / 17 roles; no analog for safe no-replace bundle finalization

## File Classification

| New/Modified File or Area | Role | Data Flow | Closest Tracked Analog | Match Quality |
|---|---|---|---|---|
| `core/t64_ai.c` (new AI device module, if split from current files) | component | event-driven | `core/t64_vi.c` | exact device/timing/packet producer shape |
| `core/t64_scheduler.c` | scheduler | event-driven | same file, VI output admission | exact |
| `core/t64_internal.h` | model | event-driven | VI state and profile predicates in same file | role-match |
| `core/t64_bus.c`, `core/t64_warm_devices.c` | route/controller | request-response | existing AI warm-window routing and refusal | exact seam; retain old behavior |
| `core/t64_mi.c` | service | event-driven | `t64_mi_set_pending` / `t64_mi_clear_pending` | exact |
| `core/t64_machine.c` | config | request-response | v9 profile descriptor, validation and lookup | exact |
| `core/include/tendies64/t64_machine.h` | utility/API | request-response | video packet info/copy API | exact |
| `core/t64_digest.c` | utility | transform | VI packet canonical hashing | exact |
| `core/tests/test_ai.c` (new) | test | event-driven | `core/tests/test_vi.c`, `test_warm_devices.c` | role-match |
| Authored AI diagnostic assembly (new path TBD) | test fixture | transform | `fixtures/rdp/fill.S` | role-match: CPU-fetched MMIO and authored literals |
| AI fixture build script and target wiring | build/config | batch | `scripts/build-rdp-fill-fixture.sh`, `core/CMakeLists.txt` | role-match |
| `fixtures/manifest.json` | config | batch | existing `rdp-fill` entry | exact rights/identity schema |
| `runner/t64_real_fixture.c` | admission/route | request-response | fixture byte-hash/profile allowlist | exact |
| `runner/t64_real.c` | controller | streaming | actual bounded advance and observer-drain loop | role-match; present video drain is fixture-only |
| Runner-owned raw bundle writer (new helper, filename TBD) | service | file-I/O | `runner/t64_supervisor.c::atomic_write` | partial: single-file replace only |
| `runner/tests/test_audio_capture.c` (new) | test | file-I/O | `runner/tests/test_rdp_video.c` | role-match: real runner, literal comparator, negative control |
| `runner/CMakeLists.txt` | config | batch | existing `runner.rdp-fill` registrations | exact |

File names marked TBD are inferred from the existing VI/fixture naming patterns; CONTEXT.md and RESEARCH.md specify the seams but do not lock new source filenames.

## Pattern Assignments

### AI device, scheduler admission, and public packet copy

**Analogs:** `core/t64_vi.c:988-990, 1018-1024, 1271-1324`; `core/t64_scheduler.c:276-315`; public contract `core/include/tendies64/t64_machine.h:332-339`.

VI checks queue space before committing its phase/readback/interrupt state, and the scheduler checks capacity before popping an event. Extend that admission to the AI sample/descriptor event so a full output queue leaves the same event at the front and commits no sample cursor, phase, bytes, or AI/MI effects.

```c
static bool event_can_dispatch(const t64_machine *machine,
                               const t64_event *event) {
  return event->handler != NULL &&
         !event_counter_would_overflow(machine, event) &&
         !vi_event_waits_for_output_capacity(machine, event);
}
```

`core/t64_scheduler.c:295-314`: `dispatch_front` returns before `queue_pop_front` if admission fails. In `core/t64_vi.c:988-990`, a full packet queue returns `T64_STATUS_QUEUE_FULL` before VI state mutation.

For the new info/copy seam, follow `t64_video_info` and `t64_video_copy`. The copy validates output ranges and capacity first; short buffers receive the required length without touching destination bytes or dequeuing.

```c
if (capacity < required) {
  *length_out = required;
  return T64_STATUS_BUFFER_TOO_SMALL;
}
(void)memcpy(bytes_out,
             machine->video_packets +
                 (size_t)slot * T64_VIDEO_MAX_PACKET_BYTES,
             required);
*length_out = required;
machine->vi.packet_head =
    (machine->vi.packet_head + 1u) % T64_VIDEO_QUEUE_CAPACITY;
machine->vi.packet_count--;
```

Source: `core/t64_vi.c:1311-1324`. Keep the audio observer queue separate from VI and keep host drain from advancing guest time.

### Machine state, profile family, and digest boundary

**Analogs:** `core/t64_internal.h:879-907, 752-774`; `core/t64_machine.c:905-971, 1267-1320, 1873-1955`; `core/t64_digest.c:675-693`.

The current profile uses version-pair predicates for VI/RDP and validates known profile IDs against a canonical descriptor hash. Add the AI profile as a new composite member through descriptor lookup, canonical validation, and the relevant existing CPU/RSP/PI/RDP/VI family predicates. Otherwise the profile may be constructible while one of its inherited devices remains disabled. Preserve the v9 descriptor and all older IDs, hashes, and refusal behavior.

```c
} else if (has_rdp_vi_filter_v9_profile(profile)) {
  canonical =
      &ntsc_cart_4m_openipl3_rdram_b_t1_rdp_vi_filter_v9_profile;
}
```

Source: `core/t64_machine.c:1294-1319`. Also extend the version-pair predicates in `core/t64_internal.h:879-972` where needed for the inherited RDP/VI capabilities.

The digest already separates producer state from host observer drain state:

```c
/* Packet bytes and metadata retain produced-output history ... packet_head/count
 * are observer state and intentionally stay out of the digest. */
for (logical = 0u; logical < T64_VIDEO_QUEUE_CAPACITY; logical++) {
  ...
  hash_u64(&context, length);
  t64_sha256_update(&context, packet_bytes, length);
  ...
}
```

Source: `core/t64_digest.c:675-693`. Hash all future-affecting AI state and packet contents/metadata; omit host drain cursor and caller/file buffers, matching this VI boundary.

### Bus and MI compatibility

**Analogs:** `core/t64_bus.c:133-139, 410-418`; `core/t64_warm_devices.c:98-101, 158-167`; `core/t64_mi.c:28-59`.

AI currently routes through the warm-device window. Warm-profile AI status reads zero; only zero control writes are accepted, and AI status acknowledgement clears only the MI AI source. Route real AI behavior only for the new composite profile and leave these warm-profile semantics intact. Use `t64_mi_set_pending` / `t64_mi_clear_pending` for the AI source rather than changing MI mask behavior.

```c
if (address == T64_SP_PC_ADDRESS ||
    (address >= T64_AI_BASE &&
     address < T64_AI_BASE + T64_DEVICE_WINDOW_BYTES)) {
  return T64_BUS_REGION_WARM;
}
```

`core/t64_bus.c:133-139` is the current decode seam; dispatch ordering must preserve the old fallback.

### Authored fixture, manifest, and actual runner test

**Analogs:** `fixtures/rdp/fill.S:18-33, 39-45`; `scripts/build-rdp-fill-fixture.sh:94-120, 123-151`; `fixtures/manifest.json:593-610`; `runner/t64_real_fixture.c:8-49, 51-84`; `runner/tests/test_rdp_video.c:240-388, 620-654`; `runner/CMakeLists.txt:349-381`.

Reuse the authored fixture path’s literal inputs and CPU stores to ordinary MMIO; its build script pins tool versions, validates packed payload bytes, and records fixture/build/tool identities. Add the new generated image as local/build-only with its documented rights row, then pin exact size/hash/profile in the real-runner allowlist and wire actual-runner plus wrong-sample control tests in CMake. `test_rdp_video` is the closest integration-test pattern: it starts the supervisor, reaches the raw packet comparator, and has a same-setup changed-literal control.

### Real runner and raw files

**Analogs:** `runner/t64_real.c:380-445, 841-856`; partial file helper `runner/t64_supervisor.c:89-115`.

The actual runner advances to bounded guest ticks and drains observers after each advance. Its current `drain_video` is intentionally limited to a 4×3 observation and fixed observation record. Extend the runner to consume the general video and audio info/copy APIs at each yield, but do not treat that fixed record or supervisor stdout capture as the raw bundle.

`atomic_write` in the supervisor uses an exclusive temporary file, `fsync`, then `rename(tmp, path)`. The final rename replaces an existing path, so it does not satisfy the new bundle’s no-clobber finalization contract. A repository search found no shared cross-platform no-replace file/directory finalizer. Treat platform support and fail-closed behavior as a planning risk; do not copy `atomic_write` as the bundle commit operation.

## Shared Patterns

- Keep new device state per machine; reuse canonical guest ticks and the scheduler’s tick/sequence order.
- Preflight bounded output capacity before dispatch, then make observer copies all-or-nothing.
- Extend the versioned profile family and canonical hash checks while preserving all previous profile behavior.
- Keep file and manifest failures in the runner; they must not change guest time or canonical core policy.
- Keep fixture source, generated identity, rights, build recipe, and expected literal evidence linked through `fixtures/manifest.json` and the real-runner allowlist.

## No Analog Found

| Operation | Role | Data Flow | Reason |
|---|---|---|---|
| Finalize a staged multi-file bundle atomically without replacing an existing target across supported hosts | utility/service | file-I/O | Existing helpers cover exclusive temporary files or atomic replacement, but none provides the required cross-platform no-replace directory finalization. |

## Metadata

**Analog search scope:** tracked `core/`, `runner/`, `fixtures/`, `scripts/`, and their CMake wiring. All named existing analog paths were confirmed by `git ls-files`; no ignored runtime mirror paths are used.  
**Root guard:** `bash build/gate/07-root-pin.sh` passed before this write.  
**Pattern extraction date:** 2026-11-18.
