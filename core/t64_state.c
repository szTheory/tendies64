#include "t64_internal.h"

#include "t64_sha256.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * State container export (plan 09-08, provisional until Phase 10).
 *
 * Export is a planning pass followed by an emit pass over the one canonical
 * walker (t64_digest_stream). The planning pass streams into a counting sink,
 * so the section recorder yields the canonical length and the offset of every
 * section marker without touching a byte. The emit pass streams the same
 * bytes into the caller buffer while hashing them, then the observer body,
 * the section hashes, the table, the header and the trailer are written
 * around it. The canonical region is therefore byte for byte the digest
 * preimage and the declared digest is the machine digest.
 *
 * Nothing here allocates, calls the host, or modifies the machine; all state
 * lives in the caller buffer and small stack records, and no function frame
 * holds both a digest sink and the layout record (frame limit, K8).
 */

typedef struct {
  uint32_t id;
  uint32_t flags;
  uint64_t offset; /* body offset from the blob start */
  uint64_t length; /* body length */
} state_entry;

typedef struct {
  state_entry entries[T64_STATE_MAX_SECTIONS];
  uint32_t count; /* table entries, the observer entry included */
  uint64_t canonical_offset;
  uint64_t canonical_length;
  uint64_t total;
} state_layout;

bool t64_state_container_length(uint64_t canonical_length,
                                uint32_t section_count,
                                uint64_t *length_out) {
  uint64_t fixed;
  uint64_t total;

  if (length_out == NULL || section_count == 0u ||
      section_count > T64_STATE_MAX_SECTIONS) {
    return false;
  }
  fixed = (uint64_t)T64_STATE_HEADER_BYTES +
          (uint64_t)T64_STATE_ENTRY_BYTES * section_count +
          (uint64_t)T64_STATE_OBSERVER_BYTES +
          (uint64_t)T64_STATE_TRAILER_BYTES;
  if (canonical_length > UINT64_MAX - fixed) {
    return false;
  }
  total = canonical_length + fixed;
  if (total > T64_STATE_MAX_BYTES) {
    return false;
  }
  *length_out = total;
  return true;
}

/* The C library string-length routine is not on the core lint allowlist: a
 * bounded length helper. It returns limit when no terminator occurs within
 * limit bytes. */
static size_t text_length(const char *text, size_t limit) {
  size_t length = 0u;

  while (length < limit && text[length] != '\0') {
    length++;
  }
  return length;
}

/*
 * Bounds-checked big-endian writers. Every non-bulk write of the header, the
 * table and the observer body goes through them: a write that would pass the
 * container length fails closed and the caller refuses the whole export.
 */
static bool put_bytes(uint8_t *base, uint64_t total, uint64_t offset,
                      const void *data, size_t length) {
  if (offset > total || (uint64_t)length > total - offset) {
    return false;
  }
  (void)memcpy(base + (size_t)offset, data, length);
  return true;
}

static bool put_u32(uint8_t *base, uint64_t total, uint64_t offset,
                    uint32_t value) {
  uint8_t bytes[4];

  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
  return put_bytes(base, total, offset, bytes, sizeof bytes);
}

static bool put_u64(uint8_t *base, uint64_t total, uint64_t offset,
                    uint64_t value) {
  uint8_t bytes[8];
  unsigned i;

  for (i = 0u; i < 8u; i++) {
    bytes[i] = (uint8_t)(value >> (56u - 8u * i));
  }
  return put_bytes(base, total, offset, bytes, sizeof bytes);
}

/*
 * Planning pass: stream the canonical state into a counting sink and turn the
 * recorded markers into the section layout. The sink lives only in this
 * frame; the layout record belongs to the caller.
 */
static t64_status plan_layout(const t64_machine *machine,
                              state_layout *layout) {
  t64_digest_sink sink;
  t64_status status;
  size_t index;
  size_t recorded;

  (void)memset(&sink, 0, sizeof sink);
  status = t64_digest_stream(machine, &sink);
  if (status != T64_STATUS_OK) {
    return status;
  }
  recorded = sink.section_count;
  /* One table entry more than the recorded sections: the observer. */
  if (sink.section_overflow || recorded == 0u ||
      recorded >= T64_STATE_MAX_SECTIONS) {
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  if (text_length(t64_build_identity(), T64_STATE_BUILD_ID_BYTES) >=
      T64_STATE_BUILD_ID_BYTES) {
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  (void)memset(layout, 0, sizeof *layout);
  layout->count = (uint32_t)recorded + 1u;
  layout->canonical_offset = (uint64_t)T64_STATE_HEADER_BYTES +
                             (uint64_t)T64_STATE_ENTRY_BYTES * layout->count;
  layout->canonical_length = (uint64_t)sink.offset;
  for (index = 0u; index < recorded; index++) {
    const uint64_t body = (uint64_t)sink.section_offsets[index] + 4u;
    const uint64_t end = index + 1u < recorded
                             ? (uint64_t)sink.section_offsets[index + 1u]
                             : (uint64_t)sink.offset;

    if (sink.section_ids[index] >= T64_STATE_SECTION_OBSERVER_CONTINUITY ||
        (index > 0u &&
         sink.section_ids[index] <= sink.section_ids[index - 1u]) ||
        end < body) {
      return T64_STATUS_BAD_STORAGE;
    }
    layout->entries[index].id = sink.section_ids[index];
    layout->entries[index].flags = 0u;
    layout->entries[index].offset = layout->canonical_offset + body;
    layout->entries[index].length = end - body;
  }
  layout->entries[recorded].id = T64_STATE_SECTION_OBSERVER_CONTINUITY;
  layout->entries[recorded].flags = T64_STATE_SECTION_FLAG_OUTSIDE_DIGEST;
  layout->entries[recorded].offset =
      layout->canonical_offset + layout->canonical_length;
  layout->entries[recorded].length = T64_STATE_OBSERVER_BYTES;
  if (!t64_state_container_length(layout->canonical_length, layout->count,
                                  &layout->total)) {
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  return T64_STATUS_OK;
}

/*
 * Emit pass for the canonical region: stream the same bytes into the caller
 * buffer while hashing them, and require that the stream matches the plan
 * (length, no overflow, the same markers at the same offsets). The sink and
 * the hash context live only in this frame.
 */
static t64_status emit_canonical(const t64_machine *machine,
                                 const state_layout *layout, uint8_t *bytes,
                                 uint8_t declared[T64_DIGEST_SIZE]) {
  t64_digest_sink sink;
  t64_sha256_ctx context;
  t64_status status;
  size_t index;

  (void)memset(&sink, 0, sizeof sink);
  t64_sha256_init(&context);
  sink.buffer = bytes + (size_t)layout->canonical_offset;
  sink.capacity = (size_t)layout->canonical_length;
  sink.hash = &context;
  status = t64_digest_stream(machine, &sink);
  if (status != T64_STATUS_OK) {
    return status;
  }
  if (sink.overflow || sink.section_overflow ||
      (uint64_t)sink.offset != layout->canonical_length ||
      (uint64_t)sink.section_count + 1u != layout->count) {
    return T64_STATUS_BAD_STORAGE;
  }
  for (index = 0u; index < sink.section_count; index++) {
    if (sink.section_ids[index] != layout->entries[index].id ||
        layout->canonical_offset + (uint64_t)sink.section_offsets[index] +
                4u !=
            layout->entries[index].offset) {
      return T64_STATUS_BAD_STORAGE;
    }
  }
  t64_sha256_final(&context, declared);
  return T64_STATUS_OK;
}

static bool write_observer(const t64_machine *machine,
                           const state_layout *layout, uint8_t *bytes) {
  const uint64_t at = layout->canonical_offset + layout->canonical_length;

  return put_u64(bytes, layout->total, at,
                 (uint64_t)machine->output_drain_cursor) &&
         put_u64(bytes, layout->total, at + 8u,
                 (uint64_t)machine->vi.packet_head) &&
         put_u64(bytes, layout->total, at + 16u,
                 (uint64_t)machine->vi.packet_count) &&
         put_u64(bytes, layout->total, at + 24u,
                 (uint64_t)machine->ai.packet_head) &&
         put_u64(bytes, layout->total, at + 32u,
                 (uint64_t)machine->ai.packet_count);
}

/* The section table: id, flags, body offset, body length, body SHA-256. */
static bool write_table(const state_layout *layout, uint8_t *bytes) {
  uint32_t index;

  for (index = 0u; index < layout->count; index++) {
    const state_entry *entry = &layout->entries[index];
    const uint64_t at = (uint64_t)T64_STATE_HEADER_BYTES +
                        (uint64_t)T64_STATE_ENTRY_BYTES * index;
    t64_sha256_ctx context;
    uint8_t digest[T64_DIGEST_SIZE];

    if (entry->offset > layout->total ||
        entry->length > layout->total - entry->offset) {
      return false;
    }
    t64_sha256_init(&context);
    t64_sha256_update(&context, bytes + (size_t)entry->offset,
                      (size_t)entry->length);
    t64_sha256_final(&context, digest);
    if (!put_u32(bytes, layout->total, at, entry->id) ||
        !put_u32(bytes, layout->total, at + 4u, entry->flags) ||
        !put_u64(bytes, layout->total, at + 8u, entry->offset) ||
        !put_u64(bytes, layout->total, at + 16u, entry->length) ||
        !put_bytes(bytes, layout->total, at + 24u, digest, sizeof digest)) {
      return false;
    }
  }
  return true;
}

static bool write_header(const t64_machine *machine,
                         const state_layout *layout,
                         const uint8_t declared[T64_DIGEST_SIZE],
                         uint8_t *bytes) {
  const uint64_t total = layout->total;
  const char *build = t64_build_identity();

  return put_bytes(bytes, total, 0x000u, T64_STATE_CONTAINER_MAGIC, 8u) &&
         put_u32(bytes, total, 0x008u, T64_STATE_CONTAINER_VERSION) &&
         put_u32(bytes, total, 0x00Cu,
                 machine->profile.state_serialization_version) &&
         put_u64(bytes, total, 0x010u, total) &&
         put_u32(bytes, total, 0x018u, layout->count) &&
         put_u32(bytes, total, 0x01Cu, 0u) &&
         put_bytes(bytes, total, 0x020u, machine->profile_digest,
                   sizeof machine->profile_digest) &&
         put_bytes(bytes, total, 0x040u, machine->content_digest,
                   sizeof machine->content_digest) &&
         put_u64(bytes, total, 0x060u, (uint64_t)machine->image_length) &&
         put_u64(bytes, total, 0x068u, machine->tick) &&
         put_bytes(bytes, total, 0x070u, declared, T64_DIGEST_SIZE) &&
         put_u64(bytes, total, 0x090u, layout->canonical_offset) &&
         put_u64(bytes, total, 0x098u, layout->canonical_length) &&
         put_bytes(bytes, total, 0x0A0u, build,
                   text_length(build, T64_STATE_BUILD_ID_BYTES));
}

static bool write_trailer(const state_layout *layout, uint8_t *bytes) {
  t64_sha256_ctx context;
  uint8_t digest[T64_DIGEST_SIZE];

  if (layout->total < T64_STATE_TRAILER_BYTES) {
    return false;
  }
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes,
                    (size_t)(layout->total - T64_STATE_TRAILER_BYTES));
  t64_sha256_final(&context, digest);
  return put_bytes(bytes, layout->total,
                   layout->total - T64_STATE_TRAILER_BYTES, digest,
                   sizeof digest);
}

/* All of the container around the canonical region and the region itself. */
static t64_status write_container(const t64_machine *machine,
                                  const state_layout *layout,
                                  uint8_t *bytes) {
  uint8_t declared[T64_DIGEST_SIZE];
  t64_status status;

  /* The header and the table are zeroed first so padding is deterministic. */
  (void)memset(bytes, 0, (size_t)layout->canonical_offset);
  status = emit_canonical(machine, layout, bytes, declared);
  if (status != T64_STATUS_OK) {
    return status;
  }
  if (!write_observer(machine, layout, bytes) || !write_table(layout, bytes) ||
      !write_header(machine, layout, declared, bytes) ||
      !write_trailer(layout, bytes)) {
    return T64_STATUS_BAD_STORAGE;
  }
  return T64_STATUS_OK;
}

t64_status t64_machine_state_export(const t64_machine *machine,
                                    uint8_t *bytes_out, size_t capacity,
                                    size_t *length_out) {
  t64_address_range length_range;
  t64_address_range bytes_range;
  state_layout layout;
  t64_status status;

  if (machine == NULL || length_out == NULL ||
      !t64_internal_make_range(length_out, sizeof *length_out,
                               &length_range) ||
      !t64_internal_observation_range_is_disjoint(machine, length_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (bytes_out != NULL &&
      (!t64_internal_make_range(bytes_out, capacity, &bytes_range) ||
       t64_internal_ranges_overlap(bytes_range, length_range) ||
       !t64_internal_observation_range_is_disjoint(machine, bytes_range))) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  /* Export is defined for the composite family (state version 25) only. */
  if (!t64_has_si_profile(machine)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = plan_layout(machine, &layout);
  if (status != T64_STATUS_OK) {
    return status;
  }
  if (bytes_out == NULL || (uint64_t)capacity < layout.total) {
    *length_out = (size_t)layout.total;
    return T64_STATUS_BUFFER_TOO_SMALL;
  }
  status = write_container(machine, &layout, bytes_out);
  if (status != T64_STATUS_OK) {
    /* Unreachable for a const machine: a defect must never publish a partial
     * container, so the whole container area is cleared before refusing. */
    (void)memset(bytes_out, 0, (size_t)layout.total);
    return status;
  }
  *length_out = (size_t)layout.total;
  return T64_STATUS_OK;
}

/*
 * State import (plan 09-09), RED stub: the entry point refuses every request
 * with NOT_READY until the importer exists.
 */
bool t64_state_summary_is_clear(const t64_machine *machine) {
  const uint8_t *bytes;
  size_t i;

  if (machine == NULL) {
    return false;
  }
  bytes = (const uint8_t *)&machine->state_summary;
  for (i = 0u; i < sizeof machine->state_summary; i++) {
    if (bytes[i] != 0u) {
      return false;
    }
  }
  return true;
}

t64_status t64_machine_state_import(t64_machine *machine,
                                    const uint8_t *bytes, size_t length,
                                    uint32_t flags,
                                    t64_state_import_report *report_out) {
  (void)bytes;
  (void)length;
  (void)flags;
  if (report_out != NULL) {
    (void)memset(report_out, 0, sizeof *report_out);
  }
  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  return T64_STATUS_NOT_READY;
}
