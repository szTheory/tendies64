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
 * State import (plan 09-09, provisional until Phase 10).
 *
 * The importer is a validate pass over the whole blob followed by one
 * infallible commit. Every section has exactly one reader function used in
 * both modes: in validate mode it evaluates every check and stores only the
 * scalars the structural checks need (in the machine-owned scratch), in apply
 * mode it re-reads the same bytes, evaluates no check and stores into the
 * machine. No function allocates, calls the host or releases anything, and the
 * live machine is not written before the validation has accepted the blob.
 *
 * The contract is memory safety and liveness: every count, index, length,
 * shift or loop bound read from the blob is bounded by a named rule before it
 * is used. The hashes inside the container are integrity checks, not
 * authentication, and an accepted state is not claimed to be reachable.
 */

#define STATE_DOMAIN "t64.state.si.25"
#define STATE_DOMAIN_BYTES 15u
#define STATE_VERSION_REGISTERED 25u
#define STATE_TABLE_ID_MAX 32u

typedef enum { STATE_MODE_VALIDATE = 0, STATE_MODE_APPLY = 1 } state_mode;

typedef struct {
  const uint8_t *base;
  uint64_t length;   /* total blob length */
  uint64_t position; /* absolute byte position of the next read */
  uint64_t limit;    /* end of the current section body */
  state_mode mode;
  bool failed;
  bool drop_observers;
  uint32_t section_id;
  t64_machine *machine;
  t64_state_import_report *report;
} state_reader;

typedef struct {
  uint32_t id;
  uint64_t offset;
  uint64_t length;
} state_section;

/* Everything one attempt needs besides the machine: the parsed table and the
 * header values the later passes compare. The record is small enough for the
 * frame limit; the large invariant summary lives in the machine. */
typedef struct {
  state_section sections[T64_STATE_MAX_SECTIONS];
  uint32_t count;
  uint64_t canonical_offset;
  uint64_t canonical_length;
  uint64_t header_tick;
} state_table;

static bool state_add_u64(uint64_t left, uint64_t right, uint64_t *sum_out) {
  if (right > UINT64_MAX - left) {
    return false;
  }
  *sum_out = left + right;
  return true;
}

static bool state_mul_u64(uint64_t left, uint64_t right, uint64_t *product_out) {
  if (left != 0u && right > UINT64_MAX / left) {
    return false;
  }
  *product_out = left * right;
  return true;
}

/* offset + length is inside [0, limit] without wrapping. */
static bool state_span_within(uint64_t offset, uint64_t length,
                              uint64_t limit) {
  return offset <= limit && length <= limit - offset;
}

static uint32_t state_be32(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static uint64_t state_be64(const uint8_t *bytes) {
  return ((uint64_t)state_be32(bytes) << 32) | (uint64_t)state_be32(bytes + 4);
}

/* The status each closed reason belongs to. */
static t64_status state_status_for(uint32_t reason) {
  switch (reason) {
  case T64_STATE_REASON_NONE:
    return T64_STATUS_OK;
  case T64_STATE_REASON_ARG_NULL:
  case T64_STATE_REASON_ARG_LENGTH:
  case T64_STATE_REASON_ARG_FLAGS:
  case T64_STATE_REASON_ARG_OVERLAP_LIVE:
    return T64_STATUS_INVALID_ARGUMENT;
  case T64_STATE_REASON_NOT_ADMITTED:
    return T64_STATUS_NOT_READY;
  case T64_STATE_REASON_TRAILER_SHA256:
  case T64_STATE_REASON_SECTION_SHA256:
  case T64_STATE_REASON_DECLARED_DIGEST:
    return T64_STATUS_STATE_INTEGRITY;
  case T64_STATE_REASON_STATE_VERSION:
    return T64_STATUS_STATE_VERSION;
  case T64_STATE_REASON_PROFILE_DIGEST:
    return T64_STATUS_STATE_PROFILE;
  case T64_STATE_REASON_CONTENT_DIGEST:
  case T64_STATE_REASON_CONTENT_LENGTH:
    return T64_STATUS_STATE_CONTENT;
  case T64_STATE_REASON_BUILD_ID:
    return T64_STATUS_STATE_BUILD;
  case T64_STATE_REASON_FIELD_RANGE:
  case T64_STATE_REASON_FIELD_PADDING:
  case T64_STATE_REASON_FIELD_RESERVED:
  case T64_STATE_REASON_FIELD_DERIVED:
  case T64_STATE_REASON_OBSERVER_RANGE:
    return T64_STATUS_STATE_RANGE;
  case T64_STATE_REASON_QUEUE_COUNT:
  case T64_STATE_REASON_QUEUE_ORDER:
  case T64_STATE_REASON_QUEUE_SEQUENCE:
  case T64_STATE_REASON_QUEUE_TICK:
  case T64_STATE_REASON_QUEUE_KIND_UNKNOWN:
  case T64_STATE_REASON_DEVICE_EVENT_MISSING:
  case T64_STATE_REASON_DEVICE_EVENT_EXTRA:
  case T64_STATE_REASON_DEVICE_TOKEN_MISMATCH:
  case T64_STATE_REASON_DEVICE_STATE_INCONSISTENT:
    return T64_STATUS_STATE_INVARIANT;
  default:
    /* the container reasons (magic, version, length, count, flags, reserved,
     * section id, order, flags, bounds, overlap, expected length, missing) */
    return T64_STATUS_STATE_FORMAT;
  }
}

/* Record the first failure only. The position is a byte position in the blob
 * or a member ordinal; it never carries blob bytes. */
static void state_report_failure(t64_state_import_report *report,
                                 uint32_t reason, uint32_t section_id,
                                 uint32_t rule, uint64_t position) {
  if (report == NULL || report->reason != T64_STATE_REASON_NONE) {
    return;
  }
  report->status = state_status_for(reason);
  report->reason = reason;
  report->section_id = section_id;
  report->rule = rule;
  report->position = position;
}

static void state_fail(state_reader *reader, uint32_t reason, uint32_t rule) {
  if (reader->failed) {
    return;
  }
  reader->failed = true;
  if (reader->mode == STATE_MODE_VALIDATE) {
    state_report_failure(reader->report, reason, reader->section_id, rule,
                         reader->position);
  }
}

/* A check evaluates only in validate mode; an apply-mode result is never
 * consulted, so apply cannot fail. */
#define STATE_CHECK(reader, condition, reason, rule)                          \
  do {                                                                        \
    if ((reader)->mode == STATE_MODE_VALIDATE && !(reader)->failed &&         \
        !(condition)) {                                                       \
      state_fail((reader), (reason), (rule));                                 \
    }                                                                         \
  } while (0)

#define STATE_STORE(reader, destination, value)                               \
  do {                                                                        \
    if ((reader)->mode == STATE_MODE_APPLY) {                                 \
      (destination) = (value);                                                \
    }                                                                         \
  } while (0)

static bool state_reader_has(const state_reader *reader, uint64_t count) {
  return !reader->failed && reader->limit >= reader->position &&
         count <= reader->limit - reader->position;
}

static uint32_t state_rd32(state_reader *reader) {
  uint32_t value;

  if (!state_reader_has(reader, 4u)) {
    state_fail(reader, T64_STATE_REASON_SECTION_BOUNDS, T64_STATE_RULE_NONE);
    return 0u;
  }
  value = state_be32(reader->base + (size_t)reader->position);
  reader->position += 4u;
  return value;
}

static uint64_t state_rd64(state_reader *reader) {
  uint64_t value;

  if (!state_reader_has(reader, 8u)) {
    state_fail(reader, T64_STATE_REASON_SECTION_BOUNDS, T64_STATE_RULE_NONE);
    return 0u;
  }
  value = state_be64(reader->base + (size_t)reader->position);
  reader->position += 8u;
  return value;
}

/* Raw bytes: apply copies them into destination when it is not NULL. The
 * returned pointer is the blob position (NULL after a failure). */
static const uint8_t *state_rd_bytes(state_reader *reader, void *destination,
                                     uint64_t count) {
  const uint8_t *source;

  if (!state_reader_has(reader, count)) {
    state_fail(reader, T64_STATE_REASON_SECTION_BOUNDS, T64_STATE_RULE_NONE);
    return NULL;
  }
  source = reader->base + (size_t)reader->position;
  if (reader->mode == STATE_MODE_APPLY && destination != NULL && count != 0u) {
    (void)memcpy(destination, source, (size_t)count);
  }
  reader->position += count;
  return source;
}

/* Bytes that must equal a live value (identity and derived byte fields). */
static void state_rd_bytes_equal(state_reader *reader, const void *expected,
                                 uint64_t count, uint32_t rule) {
  const uint8_t *source = state_rd_bytes(reader, NULL, count);

  STATE_CHECK(reader, source != NULL && memcmp(source, expected,
                                                (size_t)count) == 0,
              T64_STATE_REASON_FIELD_DERIVED, rule);
}

/* Field readers. v_ is the value read; the destination is written in apply
 * mode only. */
#define F32(dst)                                                              \
  do {                                                                        \
    const uint32_t v_ = state_rd32(r);                                        \
    STATE_STORE(r, dst, v_);                                                  \
  } while (0)
#define F64(dst)                                                              \
  do {                                                                        \
    const uint64_t v_ = state_rd64(r);                                        \
    STATE_STORE(r, dst, v_);                                                  \
  } while (0)
#define FBOOL(dst)                                                            \
  do {                                                                        \
    const uint32_t v_ = state_rd32(r);                                        \
    STATE_CHECK(r, v_ <= 1u, T64_STATE_REASON_FIELD_RANGE,                    \
                T64_STATE_RULE_BOOL_01);                                      \
    STATE_STORE(r, dst, v_ != 0u);                                            \
  } while (0)
/* v_ below bound (an index, an enumeration size or a count cap). */
#define F32_LT(dst, bound, rule)                                              \
  do {                                                                        \
    const uint32_t v_ = state_rd32(r);                                        \
    STATE_CHECK(r, v_ < (bound), T64_STATE_REASON_FIELD_RANGE, rule);         \
    STATE_STORE(r, dst, v_);                                                  \
  } while (0)
#define F32_LE(dst, bound, rule)                                              \
  do {                                                                        \
    const uint32_t v_ = state_rd32(r);                                        \
    STATE_CHECK(r, v_ <= (bound), T64_STATE_REASON_FIELD_RANGE, rule);        \
    STATE_STORE(r, dst, v_);                                                  \
  } while (0)
/* A 32-bit value stored in a narrower member: it must fit the member. */
#define F32_U16(dst)                                                          \
  do {                                                                        \
    const uint32_t v_ = state_rd32(r);                                        \
    STATE_CHECK(r, v_ <= UINT32_C(0xffff), T64_STATE_REASON_FIELD_RANGE,      \
                T64_STATE_RULE_INDEX_LT);                                     \
    STATE_STORE(r, dst, (uint16_t)v_);                                        \
  } while (0)
#define F32_U8(dst, bound)                                                    \
  do {                                                                        \
    const uint32_t v_ = state_rd32(r);                                        \
    STATE_CHECK(r, v_ <= (bound), T64_STATE_REASON_FIELD_RANGE,               \
                T64_STATE_RULE_INDEX_LT);                                     \
    STATE_STORE(r, dst, (uint8_t)v_);                                         \
  } while (0)
/* Bits outside the mask are reserved and must be zero. */
#define F32_MASK(dst, mask)                                                   \
  do {                                                                        \
    const uint32_t v_ = state_rd32(r);                                        \
    STATE_CHECK(r, (v_ & ~(uint32_t)(mask)) == 0u,                            \
                T64_STATE_REASON_FIELD_RESERVED,                              \
                T64_STATE_RULE_RESERVED_ZERO);                                \
    STATE_STORE(r, dst, v_);                                                  \
  } while (0)
#define F32_EQ(dst, want, rule)                                               \
  do {                                                                        \
    const uint32_t v_ = state_rd32(r);                                        \
    STATE_CHECK(r, v_ == (uint32_t)(want), T64_STATE_REASON_FIELD_DERIVED,    \
                rule);                                                        \
    STATE_STORE(r, dst, v_);                                                  \
  } while (0)
#define F64_LE(dst, bound, rule)                                              \
  do {                                                                        \
    const uint64_t v_ = state_rd64(r);                                        \
    STATE_CHECK(r, v_ <= (bound), T64_STATE_REASON_FIELD_RANGE, rule);        \
    STATE_STORE(r, dst, v_);                                                  \
  } while (0)
/* Narrow value hashed as u32: read and discard against a live value. */
#define F32_SAME(want)                                                        \
  do {                                                                        \
    const uint32_t v_ = state_rd32(r);                                        \
    STATE_CHECK(r, v_ == (uint32_t)(want), T64_STATE_REASON_FIELD_DERIVED,    \
                T64_STATE_RULE_DERIVED_EQ);                                   \
  } while (0)

#define F32_SAMER(want, rule)                                                 \
  do {                                                                        \
    const uint32_t v_ = state_rd32(r);                                        \
    STATE_CHECK(r, v_ == (uint32_t)(want), T64_STATE_REASON_FIELD_DERIVED,    \
                rule);                                                        \
  } while (0)
#define F64_SAMER(want, rule)                                                 \
  do {                                                                        \
    const uint64_t v_ = state_rd64(r);                                        \
    STATE_CHECK(r, v_ == (uint64_t)(want), T64_STATE_REASON_FIELD_DERIVED,    \
                rule);                                                        \
  } while (0)

/* A text member: a terminator inside the array and zero bytes after it. */
static void state_rd_text(state_reader *reader, char *destination,
                          uint64_t count) {
  const uint8_t *source = state_rd_bytes(reader, destination, count);
  uint64_t i = 0u;

  if (source == NULL) {
    return;
  }
  while (i < count && source[i] != 0u) {
    i++;
  }
  STATE_CHECK(reader, i < count, T64_STATE_REASON_FIELD_PADDING,
              T64_STATE_RULE_PAD_ZERO);
  while (i < count) {
    STATE_CHECK(reader, source[i] == 0u, T64_STATE_REASON_FIELD_PADDING,
                T64_STATE_RULE_PAD_ZERO);
    i++;
  }
}

static bool state_rd_bool(state_reader *r) {
  const uint32_t value = state_rd32(r);

  STATE_CHECK(r, value <= 1u, T64_STATE_REASON_FIELD_RANGE,
              T64_STATE_RULE_BOOL_01);
  return value != 0u;
}

/* The closed handler table (D-07): the ten production pairs of the event
 * ledger and nothing else; the synthetic test device and every other pair
 * return NULL. A switch keeps the table in code, with no writable data. */
t64_event_handler_fn t64_state_handler_lookup(uint32_t device,
                                              uint32_t kind) {
  switch (device) {
  case T64_DEVICE_CPU:
    return kind == T64_CPU_EVENT_STEP ? t64_cpu_step : NULL;
  case T64_DEVICE_PI:
    return kind == T64_PI_EVENT_COMPLETE ? t64_pi_complete : NULL;
  case T64_DEVICE_SP:
    if (kind == T64_SP_EVENT_COMPLETE) {
      return t64_sp_complete;
    }
    return kind == T64_SP_EVENT_RSP_ISSUE ? t64_rsp_issue_event_handler()
                                          : NULL;
  case T64_RDP_DEVICE:
    return kind == T64_RDP_EVENT_WORK ? t64_rdp_event : NULL;
  case T64_VI_DEVICE:
    return kind == T64_VI_EVENT_FIELD ? t64_vi_event : NULL;
  case T64_AI_DEVICE:
    return kind == T64_AI_EVENT_SAMPLE ? t64_ai_event : NULL;
  case T64_TRACE_DEVICE_INPUT:
    return kind == T64_TRACE_KIND_INPUT_SUBMITTED ? t64_input_event_handler
                                                  : NULL;
  case T64_SI_DEVICE:
    if (kind == T64_SI_EVENT_EXECUTE) {
      return t64_si_execute_event;
    }
    return kind == T64_SI_EVENT_COMPLETE ? t64_si_complete_event : NULL;
  default:
    return NULL;
  }
}

/* Index of a ledger pair in the summary, or T64_STATE_PAIR_COUNT. */
enum {
  PAIR_CPU_STEP = 0,
  PAIR_PI_COMPLETE,
  PAIR_SP_COMPLETE,
  PAIR_SP_RSP_ISSUE,
  PAIR_RDP_WORK,
  PAIR_VI_FIELD,
  PAIR_AI_SAMPLE,
  PAIR_INPUT,
  PAIR_SI_EXECUTE,
  PAIR_SI_COMPLETE
};

static uint32_t state_pair_index(uint32_t device, uint32_t kind) {
  if (t64_state_handler_lookup(device, kind) == NULL) {
    return T64_STATE_PAIR_COUNT;
  }
  switch (device) {
  case T64_DEVICE_CPU:
    return PAIR_CPU_STEP;
  case T64_DEVICE_PI:
    return PAIR_PI_COMPLETE;
  case T64_DEVICE_SP:
    return kind == T64_SP_EVENT_COMPLETE ? PAIR_SP_COMPLETE
                                         : PAIR_SP_RSP_ISSUE;
  case T64_RDP_DEVICE:
    return PAIR_RDP_WORK;
  case T64_VI_DEVICE:
    return PAIR_VI_FIELD;
  case T64_AI_DEVICE:
    return PAIR_AI_SAMPLE;
  case T64_TRACE_DEVICE_INPUT:
    return PAIR_INPUT;
  default:
    return kind == T64_SI_EVENT_EXECUTE ? PAIR_SI_EXECUTE : PAIR_SI_COMPLETE;
  }
}

static uint64_t state_sequence_limit(const t64_machine *machine) {
  const uint32_t width = machine->profile.sequence_width_bits;

  return width >= 64u ? UINT64_MAX : (UINT64_C(1) << width) - UINT64_C(1);
}

static uint32_t state_queue_limit(const t64_machine *machine) {
  const uint32_t limit = machine->profile.event_queue_capacity;

  return limit < T64_EVENT_QUEUE_CAPACITY ? limit : T64_EVENT_QUEUE_CAPACITY;
}

/* ---- sections 1 to 6 ---------------------------------------------------- */

/* Section 1: identity. Binding is exact, so every field must equal the admitted
 * value; nothing is stored. */
static void state_section_identity(state_reader *r) {
  const t64_machine *m = r->machine;

  F32_SAMER(m->profile.state_serialization_version,
            T64_STATE_RULE_PROFILE_DERIVED);
  state_rd_bytes_equal(r, m->profile_digest, sizeof m->profile_digest,
                       T64_STATE_RULE_PROFILE_DERIVED);
  state_rd_bytes_equal(r, m->content_digest, sizeof m->content_digest,
                       T64_STATE_RULE_PROFILE_DERIVED);
  F32_SAMER(1u, T64_STATE_RULE_PROFILE_DERIVED);
  F32_SAMER((uint32_t)m->image_length, T64_STATE_RULE_PROFILE_DERIVED);
  state_rd_bytes_equal(r, m->rom_header, sizeof m->rom_header,
                       T64_STATE_RULE_PROFILE_DERIVED);
  state_rd_bytes_equal(r, m->rom_title, sizeof m->rom_title,
                       T64_STATE_RULE_PROFILE_DERIVED);
  state_rd_bytes_equal(r, m->rom_game_code, sizeof m->rom_game_code,
                       T64_STATE_RULE_PROFILE_DERIVED);
  F32_SAMER((uint32_t)m->rom_version, T64_STATE_RULE_PROFILE_DERIVED);
}

/* Section 2: machine core. The tick equals the header cross-check tick. */
static void state_section_machine_core(state_reader *r) {
  t64_machine *m = r->machine;
  t64_state_device_facts *facts = &m->state_summary.facts;
  const uint64_t tick = state_rd64(r);
  bool halted;

  STATE_CHECK(r, tick == facts->tick, T64_STATE_REASON_FIELD_DERIVED,
              T64_STATE_RULE_DERIVED_EQ);
  STATE_STORE(r, m->tick, tick);
  F64(m->instruction_count);
  F64(m->frame_event_count);
  F32(m->sticky_error_status);
  halted = state_rd_bool(r);
  STATE_STORE(r, m->halted, halted);
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->halted = halted ? 1u : 0u;
  }
}

/* Section 3: RDRAM. The length field is the profile RDRAM size. */
static void state_section_rdram(state_reader *r) {
  t64_machine *m = r->machine;
  const uint32_t bytes = m->profile.rdram_bytes;

  F32_SAMER(bytes, T64_STATE_RULE_PROFILE_DERIVED);
  (void)state_rd_bytes(r, m->ram, bytes);
}

/* Section 4: SP memory, two banks of fixed size. */
static void state_section_sp_memory(state_reader *r) {
  t64_machine *m = r->machine;

  F32_SAMER((uint32_t)sizeof m->sp_dmem, T64_STATE_RULE_PROFILE_DERIVED);
  (void)state_rd_bytes(r, m->sp_dmem, sizeof m->sp_dmem);
  F32_SAMER((uint32_t)sizeof m->sp_imem, T64_STATE_RULE_PROFILE_DERIVED);
  (void)state_rd_bytes(r, m->sp_imem, sizeof m->sp_imem);
}

/* One SP DMA descriptor (72 bytes). A valid descriptor must equal its decode
 * from the three raw latches (the rule of decode_descriptor in core/t64_sp.c
 * for the functional RSP profile); an invalid one is all zero. */
static void state_read_sp_descriptor(state_reader *r,
                                     t64_sp_dma_descriptor *d) {
  (void)memset(d, 0, sizeof *d);
  d->valid = state_rd_bool(r);
  d->dram_to_sp = state_rd_bool(r);
  d->imem = state_rd_bool(r);
  d->raw_mem_address = state_rd32(r);
  d->raw_dram_address = state_rd32(r);
  d->raw_length = state_rd32(r);
  d->aligned_dram_start = state_rd32(r);
  d->internal_start = state_rd32(r);
  d->bytes_per_line = state_rd32(r);
  d->line_count = state_rd32(r);
  d->skip = state_rd32(r);
  d->payload_bytes = state_rd32(r);
  d->start_tick = state_rd64(r);
  d->end_tick = state_rd64(r);
  d->token = state_rd64(r);
  if (d->valid) {
    const uint32_t bytes_per_line = ((d->raw_length & UINT32_C(0xfff)) |
                                     UINT32_C(7)) + UINT32_C(1);
    const uint32_t line_count =
        ((d->raw_length >> 12) & UINT32_C(0xff)) + UINT32_C(1);

    STATE_CHECK(r, d->imem == ((d->raw_mem_address & UINT32_C(0x1000)) != 0u),
                T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
    STATE_CHECK(r, d->internal_start ==
                       (d->raw_mem_address & UINT32_C(0x00000ff8)),
                T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
    STATE_CHECK(r, d->aligned_dram_start ==
                       (d->raw_dram_address & UINT32_C(0x00fffff8)),
                T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
    STATE_CHECK(r, d->bytes_per_line == bytes_per_line,
                T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
    STATE_CHECK(r, d->line_count == line_count,
                T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
    STATE_CHECK(r, d->skip == ((d->raw_length >> 20) & UINT32_C(0xfff)),
                T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
    STATE_CHECK(r, d->payload_bytes == bytes_per_line * line_count,
                T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
    STATE_CHECK(r, d->end_tick > d->start_tick, T64_STATE_REASON_FIELD_RANGE,
                T64_STATE_RULE_TICK_GE);
  } else {
    STATE_CHECK(r, !d->dram_to_sp && !d->imem && d->raw_mem_address == 0u &&
                       d->raw_dram_address == 0u && d->raw_length == 0u &&
                       d->aligned_dram_start == 0u && d->internal_start == 0u &&
                       d->bytes_per_line == 0u && d->line_count == 0u &&
                       d->skip == 0u && d->payload_bytes == 0u &&
                       d->start_tick == 0u && d->end_tick == 0u &&
                       d->token == 0u,
                T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
  }
}

/* Section 5: interrupt, PI and SP registers. */
static void state_section_interrupt_pi_sp(state_reader *r) {
  t64_machine *m = r->machine;
  t64_state_device_facts *facts = &m->state_summary.facts;
  t64_sp_dma_descriptor descriptor;
  uint32_t pending;
  uint32_t mask;
  uint32_t irq;
  bool active_valid;
  bool canceled;
  uint32_t active_dram;
  uint32_t active_cart;
  uint32_t active_length;
  uint64_t active_token;

  pending = state_rd32(r);
  STATE_CHECK(r, (pending & ~T64_MI_INTERRUPT_BITS) == 0u,
              T64_STATE_REASON_FIELD_RESERVED, T64_STATE_RULE_RESERVED_ZERO);
  mask = state_rd32(r);
  STATE_CHECK(r, (mask & ~T64_MI_INTERRUPT_BITS) == 0u,
              T64_STATE_REASON_FIELD_RESERVED, T64_STATE_RULE_RESERVED_ZERO);
  irq = state_rd32(r);
  STATE_CHECK(r, irq == ((pending & mask) != 0u ? 1u : 0u),
              T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
  STATE_STORE(r, m->mi_pending, pending);
  STATE_STORE(r, m->mi_mask, mask);
  STATE_STORE(r, m->mi_irq_line, (pending & mask) != 0u);

  F32_MASK(m->pi.dram_address, 0x00ffffffu);
  F32(m->pi.cart_address);
  F32_MASK(m->pi.read_length, 0x00ffffffu);
  F32_MASK(m->pi.write_length, 0x00ffffffu);
  F32_MASK(m->pi.domain1_latency, 0xffu);
  F32_MASK(m->pi.domain1_pulse_width, 0xffu);
  F32_MASK(m->pi.domain1_page_size, 0xfu);
  F32_MASK(m->pi.domain1_release_duration, 0x3u);
  F32_MASK(m->pi.domain2_latency, 0xffu);
  F32_MASK(m->pi.domain2_pulse_width, 0xffu);
  F32_MASK(m->pi.domain2_page_size, 0xfu);
  F32_MASK(m->pi.domain2_release_duration, 0x3u);
  FBOOL(m->pi.error);
  active_valid = state_rd_bool(r);
  canceled = state_rd_bool(r);
  active_dram = state_rd32(r);
  active_cart = state_rd32(r);
  active_length = state_rd32(r);
  active_token = state_rd64(r);
  if (active_valid) {
    /* The transfer start admits 1 to 16 MiB inside installed RDRAM. */
    STATE_CHECK(r, active_length >= 1u && active_length <= UINT32_C(0x01000000),
                T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_LEN_LE);
    STATE_CHECK(r, (uint64_t)active_dram + (uint64_t)active_length <=
                       (uint64_t)m->profile.rdram_bytes,
                T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_ADDR_RANGE);
  } else {
    STATE_CHECK(r, !canceled && active_dram == 0u && active_cart == 0u &&
                       active_length == 0u && active_token == 0u,
                T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
  }
  STATE_STORE(r, m->pi.active_valid, active_valid);
  STATE_STORE(r, m->pi.active_canceled, canceled);
  STATE_STORE(r, m->pi.active_dram_address, active_dram);
  STATE_STORE(r, m->pi.active_cart_address, active_cart);
  STATE_STORE(r, m->pi.active_length, active_length);
  STATE_STORE(r, m->pi.active_token, active_token);
  F64(m->pi.next_token);
  FBOOL(m->pi.token_exhausted);
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->pi_active_valid = active_valid ? 1u : 0u;
    facts->pi_active_token = active_token;
    facts->pi_active_length = active_length;
  }

  /* SP: the register latches are masked where they are consumed (the start of
   * a DMA masks the memory and DRAM addresses; core/t64_sp.c decode_descriptor),
   * so they are unconstrained here. */
  F32(m->sp.mem_address);
  F32(m->sp.dram_address);
  F32(m->sp.read_length);
  F32(m->sp.write_length);
  state_read_sp_descriptor(r, &descriptor);
  STATE_STORE(r, m->sp.active, descriptor);
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->sp_dma_active = descriptor.valid ? 1u : 0u;
  }
  state_read_sp_descriptor(r, &descriptor);
  STATE_CHECK(r, !descriptor.valid || facts->sp_dma_active != 0u,
              T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
  STATE_STORE(r, m->sp.pending, descriptor);
  if (r->mode == STATE_MODE_VALIDATE && descriptor.valid) {
    facts->sp_dma_active = 1u;
  }
  F64(m->sp.next_token);
  FBOOL(m->sp.token_exhausted);
}

/* Section 6: the scheduler queue. The tuples are checked as they are read
 * (order, sequence, tick, closed pair) and summarized per pair in the scratch
 * for the per-device pass; apply rebuilds the handlers from the closed table. */
static void state_section_scheduler(state_reader *r) {
  t64_machine *m = r->machine;
  t64_state_summary *summary = &m->state_summary;
  t64_state_device_facts *facts = &summary->facts;
  const uint32_t limit = state_queue_limit(m);
  const uint64_t sequence_limit = state_sequence_limit(m);
  uint32_t count;
  uint64_t next_sequence;
  bool exhausted;
  uint32_t index;

  count = state_rd32(r);
  STATE_CHECK(r, count <= limit, T64_STATE_REASON_QUEUE_COUNT,
              T64_STATE_RULE_COUNT_CAP);
  if (count > limit) {
    count = 0u;
  }
  next_sequence = state_rd64(r);
  exhausted = state_rd_bool(r);
  STATE_CHECK(r, next_sequence <= sequence_limit &&
                     (!exhausted || next_sequence == sequence_limit),
              T64_STATE_REASON_QUEUE_SEQUENCE, T64_STATE_RULE_SEQ_LT);
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->queue_count = count;
    facts->next_sequence = next_sequence;
    facts->sequence_exhausted = exhausted ? 1u : 0u;
  }
  for (index = 0u; index < count && !r->failed; index++) {
    t64_event event;
    uint32_t pair;

    (void)memset(&event, 0, sizeof event);
    event.tick = state_rd64(r);
    event.sequence = state_rd64(r);
    event.device = state_rd32(r);
    event.kind = state_rd32(r);
    event.value = state_rd64(r);
    event.token = state_rd64(r);
    event.handler = t64_state_handler_lookup(event.device, event.kind);
    pair = state_pair_index(event.device, event.kind);
    STATE_CHECK(r, event.handler != NULL && pair < T64_STATE_PAIR_COUNT,
                T64_STATE_REASON_QUEUE_KIND_UNKNOWN,
                T64_STATE_RULE_ENUM_CLOSED);
    STATE_CHECK(r, index == 0u || event.tick > facts->previous_tick ||
                       (event.tick == facts->previous_tick &&
                        event.sequence > facts->previous_sequence),
                T64_STATE_REASON_QUEUE_ORDER, T64_STATE_RULE_NONE);
    STATE_CHECK(r, exhausted ? event.sequence <= next_sequence
                             : event.sequence < next_sequence,
                T64_STATE_REASON_QUEUE_SEQUENCE, T64_STATE_RULE_SEQ_LT);
    STATE_CHECK(r, event.tick >= facts->tick, T64_STATE_REASON_QUEUE_TICK,
                T64_STATE_RULE_TICK_GE);
    STATE_CHECK(r, pair != PAIR_INPUT || (event.value >> 32) == 0u,
                T64_STATE_REASON_DEVICE_STATE_INCONSISTENT,
                T64_STATE_RULE_NONE);
    if (r->mode == STATE_MODE_VALIDATE && !r->failed) {
      t64_state_pair *entry = &summary->pairs[pair];

      if (entry->count == 0u) {
        entry->tick = event.tick;
        entry->token = event.token;
        entry->value = event.value;
      }
      if (entry->count == 0u || event.token > entry->token_max) {
        entry->token_max = event.token;
        entry->max_count = 1u;
      } else if (event.token == entry->token_max) {
        entry->max_count++;
      }
      entry->count++;
      facts->previous_tick = event.tick;
      facts->previous_sequence = event.sequence;
    }
    if (r->mode == STATE_MODE_APPLY) {
      m->events[index] = event;
    }
  }
  STATE_STORE(r, m->queue_head, 0u);
  STATE_STORE(r, m->queue_count, (size_t)count);
  STATE_STORE(r, m->next_sequence, next_sequence);
  STATE_STORE(r, m->sequence_exhausted, exhausted);
}

/* ---- section 7: warm devices ------------------------------------------- */

/* The VR4300 block of the walker (hash_vr4300_cpu_state) plus the two latches
 * the composite family appends after it. */
static void state_read_cpu_extended(state_reader *r) {
  t64_machine *m = r->machine;
  t64_cpu_state *cpu = &m->cpu;
  t64_state_device_facts *facts = &m->state_summary.facts;
  uint32_t outcome;
  size_t i;

  F64(cpu->pc_full);
  F64(cpu->last_pc_full);
  F64(cpu->delay_slot_pc_full);
  F64(cpu->delay_target_full);
  F64(cpu->stop_pc_full);
  outcome = state_rd32(r);
  STATE_CHECK(r, outcome <= (uint32_t)T64_CPU_ATTEMPT_HOST_ERROR,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_ENUM_CLOSED);
  STATE_STORE(r, cpu->attempt_outcome, outcome);
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->cpu_attempt_outcome = outcome;
  }
  /* The composite family carries the third semantic policy version. */
  F32_EQ(cpu->semantic_policy_version, 3u, T64_STATE_RULE_PROFILE_DERIVED);
  FBOOL(cpu->cp0_config_init_window);
  FBOOL(cpu->tlb_shutdown);
  FBOOL(cpu->pending_exception_valid);
  F32_LT(cpu->pending_exception_code, UINT32_C(32), T64_STATE_RULE_INDEX_LT);
  F64(cpu->pending_exception_address);
  F64(cpu->pending_exception_epc);
  FBOOL(cpu->pending_exception_bd);

  F32_SAMER(T64_VR4300_TLB_ENTRY_COUNT, T64_STATE_RULE_PROFILE_DERIVED);
  for (i = 0u; i < T64_VR4300_TLB_ENTRY_COUNT; i++) {
    F64(cpu->tlb[i].page_mask);
    F64(cpu->tlb[i].vpn2);
    F64(cpu->tlb[i].region);
    F32(cpu->tlb[i].asid);
    F64(cpu->tlb[i].entry_lo0);
    F64(cpu->tlb[i].entry_lo1);
  }

  F64(cpu->count_sync_tick);
  F32_LE(cpu->count_half_clock_phase, 1u, T64_STATE_RULE_ENUM_CLOSED);
  F64(cpu->count_compare_deadline);
  F64(cpu->count_equality_latch_generation);
  FBOOL(cpu->count_equality_latched);
  F32(cpu->linked_physical_address);
  F32(cpu->linked_width);
  FBOOL(cpu->linked_valid);

  F32_SAMER(T64_VR4300_ICACHE_LINE_COUNT, T64_STATE_RULE_PROFILE_DERIVED);
  for (i = 0u; i < T64_VR4300_ICACHE_LINE_COUNT; i++) {
    F32(cpu->icache[i].physical_tag);
    FBOOL(cpu->icache[i].valid);
    (void)state_rd_bytes(r, cpu->icache[i].data, sizeof cpu->icache[i].data);
  }
  F32_SAMER(T64_VR4300_DCACHE_LINE_COUNT, T64_STATE_RULE_PROFILE_DERIVED);
  for (i = 0u; i < T64_VR4300_DCACHE_LINE_COUNT; i++) {
    F32(cpu->dcache[i].physical_tag);
    FBOOL(cpu->dcache[i].valid);
    FBOOL(cpu->dcache[i].dirty);
    (void)state_rd_bytes(r, cpu->dcache[i].data, sizeof cpu->dcache[i].data);
  }
  F32_SAMER(32u, T64_STATE_RULE_PROFILE_DERIVED);
  for (i = 0u; i < 32u; i++) {
    F64(cpu->fpr[i]);
  }
  F32(cpu->fcr0);
  F32(cpu->fcr31);
  F64(cpu->delay_origin_pc_full);
  F64(cpu->cp2_transfer_latch);
}

static void state_read_cpu(state_reader *r) {
  t64_machine *m = r->machine;
  t64_cpu_state *cpu = &m->cpu;
  t64_state_device_facts *facts = &m->state_summary.facts;
  bool stopped;
  size_t i;

  for (i = 0u; i < 32u; i++) {
    F64(cpu->gpr[i]);
  }
  F64(cpu->hi);
  F64(cpu->lo);
  for (i = 0u; i < 32u; i++) {
    F64(cpu->cp0[i]);
  }
  F64(cpu->retired_instructions);
  F32(cpu->pc);
  F32(cpu->last_pc);
  F32(cpu->last_opcode);
  F32(cpu->delay_slot_pc);
  F32(cpu->delay_target);
  FBOOL(cpu->delay_slot_pending);
  stopped = state_rd_bool(r);
  STATE_STORE(r, cpu->stopped, stopped);
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->cpu_stopped = stopped ? 1u : 0u;
  }
  F32(cpu->stop_reason);
  F32(cpu->stop_pc);
  F32(cpu->stop_opcode);
  state_read_cpu_extended(r);
}

static void state_read_rdram_protocol(state_reader *r) {
  t64_rdram_protocol_state *p = &r->machine->guest_devices.rdram_protocol;
  size_t device_index;
  size_t i;
  uint32_t device_count;

  F32(p->ri_config);
  FBOOL(p->ri_config_known);
  F32(p->ri_current_load);
  FBOOL(p->ri_current_load_known);
  F64(p->ri_config_tick);
  F64(p->ri_current_load_deadline_tick);
  F32(p->ri_mode);
  FBOOL(p->ri_mode_known);
  F64(p->ri_mode_tick);
  F64(p->ri_mode_deadline_tick);
  F32(p->ri_refresh);
  FBOOL(p->ri_refresh_known);
  FBOOL(p->mi_repeat_enabled);
  FBOOL(p->mi_repeat_known);
  F32(p->mi_repeat_length);
  FBOOL(p->mi_upper_mode);
  F32_U16(p->broadcast_registers_known);
  for (i = 0u; i < T64_RDRAM_PROTOCOL_REGISTER_COUNT; i++) {
    F32(p->broadcast_register_values[i]);
  }
  device_count = state_rd32(r);
  STATE_CHECK(r, device_count <= T64_RDRAM_PROTOCOL_SLOT_COUNT,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_COUNT_CAP);
  STATE_STORE(r, p->logical_device_count, (uint8_t)device_count);
  FBOOL(p->capacity_scan_complete);
  F32(p->logical_capacity_bytes);
  for (device_index = 0u; device_index < T64_RDRAM_PROTOCOL_SLOT_COUNT;
       device_index++) {
    t64_rdram_device_state *d = &p->devices[device_index];

    F32_U16(d->device_id);
    F32_U8(d->calibration_state, T64_RDRAM_CALIBRATION_AUTO_COMMAND);
    FBOOL(d->present);
    FBOOL(d->device_id_known);
    F32_U16(d->registers_known);
    for (i = 0u; i < T64_RDRAM_PROTOCOL_REGISTER_COUNT; i++) {
      F32(d->register_values[i]);
    }
  }
}

static void state_read_rsp(state_reader *r) {
  t64_machine *m = r->machine;
  t64_rsp_state *rsp = &m->rsp;
  t64_state_device_facts *facts = &m->state_summary.facts;
  uint32_t status;
  size_t i;

  for (i = 0u; i < 32u; i++) {
    F32(rsp->gpr[i]);
  }
  (void)state_rd_bytes(r, rsp->vector, sizeof rsp->vector);
  for (i = 0u; i < 8u; i++) {
    const uint64_t v_ = state_rd64(r);

    STATE_CHECK(r, (v_ & ~UINT64_C(0x0000ffffffffffff)) == 0u,
                T64_STATE_REASON_FIELD_RESERVED, T64_STATE_RULE_RESERVED_ZERO);
    STATE_STORE(r, rsp->accumulator[i], v_);
  }
  F32_U16(rsp->vco);
  F32_U16(rsp->vcc);
  F32_U8(rsp->vce, UINT32_C(0xff));
  F32(rsp->pc);
  F32(rsp->delay_target);
  FBOOL(rsp->delay_slot_pending);
  FBOOL(rsp->branch_bubble_pending);
  F32(rsp->divider_input);
  F32(rsp->divider_result);
  FBOOL(rsp->divider_high_valid);
  for (i = 0u; i < 32u; i++) {
    F64(rsp->scalar_ready_tick[i]);
    F64(rsp->vector_ready_tick[i]);
  }
  for (i = 0u; i < 8u; i++) {
    F64(rsp->accumulator_ready_tick[i]);
  }
  F64(rsp->memory_access_ready_tick);
  {
    const uint64_t next_issue_tick = state_rd64(r);
    const uint64_t issue_sequence = state_rd64(r);

    STATE_STORE(r, rsp->next_issue_tick, next_issue_tick);
    STATE_STORE(r, rsp->issue_sequence, issue_sequence);
    if (r->mode == STATE_MODE_VALIDATE) {
      facts->rsp_next_issue_tick = next_issue_tick;
      facts->rsp_issue_sequence = issue_sequence;
    }
  }
  F64(rsp->retired_instructions);
  F64(rsp->stall_slots);
  status = state_rd32(r);
  STATE_STORE(r, rsp->sp_status, status);
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->rsp_running = (status & T64_RSP_STATUS_HALT) == 0u ? 1u : 0u;
  }
  FBOOL(rsp->sp_semaphore);
  F64(rsp->dma_beat_cursor);
  F64(rsp->dma_setup_remaining);
  F64(rsp->active_dma_token);
  F64(rsp->pending_dma_token);
  F64(rsp->dma_event_sequence);
}

/* Section 7. The boot record is fixed by the admitted asset and the profile
 * policy, so every boot field must equal the live (admitted) value. */
static void state_section_warm_devices(state_reader *r) {
  t64_machine *m = r->machine;
  const t64_boot_state *boot = &m->boot;
  t64_state_device_facts *facts = &m->state_summary.facts;
  uint32_t captured;
  size_t i;

  state_rd_bytes_equal(r, boot->asset_sha256, sizeof boot->asset_sha256,
                       T64_STATE_RULE_DERIVED_EQ);
  state_rd_bytes_equal(r, boot->payload_sha256, sizeof boot->payload_sha256,
                       T64_STATE_RULE_DERIVED_EQ);
  state_rd_bytes_equal(r, boot->policy_sha256, sizeof boot->policy_sha256,
                       T64_STATE_RULE_DERIVED_EQ);
  F32_SAMER(boot->source_offset, T64_STATE_RULE_DERIVED_EQ);
  F32_SAMER(boot->payload_length, T64_STATE_RULE_DERIVED_EQ);
  F32_SAMER(boot->dmem_offset, T64_STATE_RULE_DERIVED_EQ);
  F32_SAMER(boot->entry_pc, T64_STATE_RULE_DERIVED_EQ);
  F32_SAMER(boot->seed_reset_version, T64_STATE_RULE_DERIVED_EQ);
  F32_SAMER(boot->seed_s3_rom_type, T64_STATE_RULE_DERIVED_EQ);
  F32_SAMER(boot->seed_s4_tv_type, T64_STATE_RULE_DERIVED_EQ);
  F32_SAMER(boot->seed_s5_reset_type, T64_STATE_RULE_DERIVED_EQ);
  F32_SAMER(boot->seed_s6_entropy, T64_STATE_RULE_DERIVED_EQ);
  F32_SAMER(boot->seed_s7_boot_info_version, T64_STATE_RULE_DERIVED_EQ);
  F32_SAMER(boot->prepared ? 1u : 0u, T64_STATE_RULE_DERIVED_EQ);

  state_read_cpu(r);

  F32(m->cache.policy_version);
  F32(m->cache.last_operation);
  F32(m->cache.last_pc);
  F32(m->cache.last_address);
  FBOOL(m->cache.operation_seen);

  F32(m->guest_devices.policy_version);
  F32(m->guest_devices.mi_version);
  F32(m->guest_devices.ri_select);
  FBOOL(m->guest_devices.mi_version_known);
  FBOOL(m->guest_devices.ri_select_known);

  F32(m->output.policy_version);
  FBOOL(m->output.video_output_ready);
  FBOOL(m->output.audio_output_ready);
  F32(m->output.magic_register);
  F32(m->output.byte_count);
  F32_SAMER(T64_ISVIEWER_BUFFER_BYTES, T64_STATE_RULE_PROFILE_DERIVED);
  (void)state_rd_bytes(r, m->output.buffer, sizeof m->output.buffer);
  captured = state_rd32(r);
  STATE_CHECK(r, captured <= T64_ISVIEWER_CAPTURE_BYTES,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_LEN_LE);
  STATE_STORE(r, m->output.captured_bytes, captured);
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->captured_bytes = captured;
  }
  F32_SAMER(T64_ISVIEWER_CAPTURE_BYTES, T64_STATE_RULE_PROFILE_DERIVED);
  (void)state_rd_bytes(r, m->output.captured, sizeof m->output.captured);
  FBOOL(m->output.lost_output);
  F64(m->output.total_attempted_bytes);
  FBOOL(m->output.total_attempted_bytes_saturated);

  state_read_rsp(r);
  state_read_rdram_protocol(r);
  (void)i;
}

/* ---- section 8: RDP and VI ---------------------------------------------- */

static void state_read_rdp_tiles(state_reader *r) {
  t64_rdp_state *rdp = &r->machine->rdp;
  size_t i;

  (void)state_rd_bytes(r, rdp->tmem, sizeof rdp->tmem);
  for (i = 0u; i < T64_RDP_TILE_COUNT; i++) {
    t64_rdp_tile *tile = &rdp->tiles[i];

    /* The widths of the tile descriptor fields (core/t64_rdp_tmem.c): these
     * values index TMEM and drive shifts, so each is bounded by its width. */
    F32_LE(tile->format, 7u, T64_STATE_RULE_ENUM_CLOSED);
    F32_LE(tile->size, 3u, T64_STATE_RULE_ENUM_CLOSED);
    F32_LE(tile->line_words, UINT32_C(0x1ff), T64_STATE_RULE_INDEX_LT);
    F32_LE(tile->tmem_word, UINT32_C(0x1ff), T64_STATE_RULE_INDEX_LT);
    F32_LE(tile->palette, 15u, T64_STATE_RULE_INDEX_LT);
    F32_LE(tile->mask_s, 15u, T64_STATE_RULE_SHIFT_LT);
    F32_LE(tile->mask_t, 15u, T64_STATE_RULE_SHIFT_LT);
    F32_LE(tile->shift_s, 15u, T64_STATE_RULE_SHIFT_LT);
    F32_LE(tile->shift_t, 15u, T64_STATE_RULE_SHIFT_LT);
    F32_LE(tile->uls, UINT32_C(0x3fff), T64_STATE_RULE_LEN_LE);
    F32_LE(tile->ult, UINT32_C(0x3fff), T64_STATE_RULE_LEN_LE);
    F32_LE(tile->lrs, UINT32_C(0x3fff), T64_STATE_RULE_LEN_LE);
    F32_LE(tile->lrt, UINT32_C(0x3fff), T64_STATE_RULE_LEN_LE);
    FBOOL(tile->clamp_s);
    FBOOL(tile->mirror_s);
    FBOOL(tile->clamp_t);
    FBOOL(tile->mirror_t);
    FBOOL(tile->valid);
    FBOOL(tile->size_valid);
  }
}

/* A triangle render in progress (09-10 FX-01). The rows run later from the
 * packet words and the registers, and the rasterizer proved its bounds when it
 * accepted the command (core/t64_rdp_raster.c t64_rdp_triangle_extent): the
 * rows index the 1,024-row triangle coverage array and the shade and texture
 * products must fit 64 bits. The reader therefore re-establishes exactly that
 * proof from the blob values, through the same function, before the blob can
 * commit: the rows below the coverage height, an opcode the profile accepts,
 * the word count of the opcode, the shade flag of the opcode, the products and
 * the extent, the first and last row the extent gives, and the row being
 * rendered inside them. */
typedef struct {
  const uint64_t *packet;
  uint32_t word_count;
  bool shade;
  t64_rdp_triangle_view view;
  uint32_t y_begin;
  uint32_t y_end;
  uint32_t y_now;
} state_triangle;

static void state_check_triangle_render(state_reader *r,
                                        const state_triangle *triangle) {
  const t64_machine *m = r->machine;
  uint32_t opcode;
  uint32_t first = 1u;
  uint32_t last = 0u;
  bool extent;

  if (r->mode != STATE_MODE_VALIDATE || r->failed) {
    return;
  }
  STATE_CHECK(r, triangle->y_now < T64_RDP_TRIANGLE_COVERAGE_HEIGHT &&
                     triangle->y_end < T64_RDP_TRIANGLE_COVERAGE_HEIGHT,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_INDEX_LT);
  opcode = (uint32_t)((triangle->packet[0] >> 56) & UINT64_C(0x3f));
  STATE_CHECK(r, t64_rdp_triangle_opcode_accepted(m, opcode),
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_ENUM_CLOSED);
  STATE_CHECK(r, triangle->word_count == t64_rdp_triangle_word_count(opcode),
              T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
  STATE_CHECK(r, triangle->shade == (opcode == UINT32_C(0x0c) ||
                                     opcode == UINT32_C(0x0d) ||
                                     opcode == UINT32_C(0x0e) ||
                                     opcode == UINT32_C(0x0f)),
              T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
  extent = r->failed ? false
                     : t64_rdp_triangle_extent(m, opcode, triangle->packet,
                                               triangle->word_count,
                                               &triangle->view, &first, &last);
  STATE_CHECK(r, extent, T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_LEN_LE);
  STATE_CHECK(r, first == triangle->y_begin && last == triangle->y_end &&
                     first <= last,
              T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
  STATE_CHECK(r, triangle->y_now >= first && triangle->y_now <= last,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_INDEX_LT);
}

static void state_read_rdp(state_reader *r) {
  t64_machine *m = r->machine;
  t64_rdp_state *rdp = &m->rdp;
  t64_state_device_facts *facts = &m->state_summary.facts;
  uint32_t words;
  bool render_active;
  bool fence_pending;
  bool busy;
  bool command_pending;
  bool texrect_pending;
  bool freeze;
  bool flush;
  bool render_triangle;
  bool shade_triangle;
  uint32_t image_address;
  uint32_t image_width;
  uint32_t image_size;
  uint32_t scissor_xh;
  uint32_t scissor_yh;
  uint32_t scissor_xl;
  uint32_t scissor_yl;
  uint64_t other_modes;
  uint64_t packet[T64_RDP_TRIANGLE_MAX_WORDS];
  uint32_t x_begin;
  uint32_t x_end;
  uint32_t y_begin;
  uint32_t y_end;
  uint32_t y_now;
  uint32_t signed_value;
  size_t i;

  F32(rdp->dpc_start);
  F32(rdp->dpc_end);
  F32(rdp->dpc_current);
  F32(rdp->dpc_status);
  F32(rdp->dpc_clock_counter);
  F32(rdp->dpc_buffer_busy_counter);
  F32(rdp->dpc_pipe_busy_counter);
  F32(rdp->dpc_tmem_counter);
  F32(rdp->pending_command_address);
  F32(rdp->pending_command_byte);
  F32(rdp->pending_opcode_field);
  F64(rdp->pending_command_word);
  F64(rdp->fence_generation);
  /* The image and scissor values are kept in locals: a render in progress is
   * checked against them below (the fill, copy and triangle row handlers of
   * core/t64_rdp.c and core/t64_rdp_raster.c index RDRAM from these values
   * without checking them again). */
  image_address = state_rd32(r);
  STATE_STORE(r, rdp->color_image_address, image_address);
  image_width = state_rd32(r);
  STATE_STORE(r, rdp->color_image_width, image_width);
  F32_LE(rdp->color_image_format, 7u, T64_STATE_RULE_ENUM_CLOSED);
  image_size = state_rd32(r);
  STATE_CHECK(r, image_size <= 3u, T64_STATE_REASON_FIELD_RANGE,
              T64_STATE_RULE_ENUM_CLOSED);
  STATE_STORE(r, rdp->color_image_size, image_size);
  scissor_xh = state_rd32(r);
  STATE_CHECK(r, scissor_xh <= UINT32_C(0xfff), T64_STATE_REASON_FIELD_RANGE,
              T64_STATE_RULE_LEN_LE);
  STATE_STORE(r, rdp->scissor_xh, scissor_xh);
  scissor_yh = state_rd32(r);
  STATE_CHECK(r, scissor_yh <= UINT32_C(0xfff), T64_STATE_REASON_FIELD_RANGE,
              T64_STATE_RULE_LEN_LE);
  STATE_STORE(r, rdp->scissor_yh, scissor_yh);
  scissor_xl = state_rd32(r);
  STATE_CHECK(r, scissor_xl <= UINT32_C(0xfff), T64_STATE_REASON_FIELD_RANGE,
              T64_STATE_RULE_LEN_LE);
  STATE_STORE(r, rdp->scissor_xl, scissor_xl);
  scissor_yl = state_rd32(r);
  STATE_CHECK(r, scissor_yl <= UINT32_C(0xfff), T64_STATE_REASON_FIELD_RANGE,
              T64_STATE_RULE_LEN_LE);
  STATE_STORE(r, rdp->scissor_yl, scissor_yl);
  other_modes = state_rd64(r);
  STATE_STORE(r, rdp->other_modes, other_modes);
  F32(rdp->fill_color);
  F32(rdp->texture_image_address);
  F32(rdp->texture_image_width);
  F32_LE(rdp->texture_image_format, 7u, T64_STATE_RULE_ENUM_CLOSED);
  F32_LE(rdp->texture_image_size, 3u, T64_STATE_RULE_ENUM_CLOSED);
  F32_LE(rdp->texture_image_valid, 1u, T64_STATE_RULE_BOOL_01);
  F32(rdp->tlut_format);
  FBOOL(rdp->tlut_valid);
  F32_LE(rdp->render_kind, 3u, T64_STATE_RULE_ENUM_CLOSED);
  F32_LT(rdp->render_texture_tile, T64_RDP_TILE_COUNT,
         T64_STATE_RULE_INDEX_LT);
  signed_value = state_rd32(r);
  STATE_STORE(r, rdp->render_texture_s, (int32_t)signed_value);
  signed_value = state_rd32(r);
  STATE_STORE(r, rdp->render_texture_t, (int32_t)signed_value);
  signed_value = state_rd32(r);
  STATE_STORE(r, rdp->render_texture_dsdx, (int32_t)signed_value);
  signed_value = state_rd32(r);
  STATE_STORE(r, rdp->render_texture_dtdy, (int32_t)signed_value);
  F64(rdp->copy_count);
  for (i = 0u; i < 6u; i++) {
    F32(rdp->convert_coefficients[i]);
  }
  F64(rdp->pending_texrect_word);

  /* The triangle block: the packet word count bounds the words that follow. */
  words = state_rd32(r);
  STATE_CHECK(r, words <= T64_RDP_TRIANGLE_MAX_WORDS,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_COUNT_CAP);
  if (words > T64_RDP_TRIANGLE_MAX_WORDS) {
    words = 0u;
  }
  STATE_STORE(r, rdp->pending_triangle_word_count, words);
  if (r->mode == STATE_MODE_APPLY) {
    (void)memset(rdp->pending_triangle_words, 0,
                 sizeof rdp->pending_triangle_words);
  }
  (void)memset(packet, 0, sizeof packet);
  for (i = 0u; i < words && !r->failed; i++) {
    packet[i] = state_rd64(r);
    STATE_STORE(r, rdp->pending_triangle_words[i], packet[i]);
  }
  F32(rdp->primitive_color);
  FBOOL(rdp->primitive_color_valid);
  F32(rdp->triangle_count);
  {
    const uint32_t v_ = state_rd32(r);

    STATE_CHECK(r, v_ <= 1u, T64_STATE_REASON_FIELD_RANGE,
                T64_STATE_RULE_BOOL_01);
    render_triangle = v_ != 0u;
    STATE_STORE(r, rdp->render_triangle, render_triangle);
  }
  {
    const uint32_t v_ = state_rd32(r);

    STATE_CHECK(r, v_ <= 1u, T64_STATE_REASON_FIELD_RANGE,
                T64_STATE_RULE_BOOL_01);
    shade_triangle = v_ != 0u;
    STATE_STORE(r, rdp->shade_triangle, shade_triangle);
  }
  F32_SAMER(T64_RDP_TRIANGLE_COVERAGE_WIDTH, T64_STATE_RULE_PROFILE_DERIVED);
  F32_SAMER(T64_RDP_TRIANGLE_COVERAGE_HEIGHT, T64_STATE_RULE_PROFILE_DERIVED);
  (void)state_rd_bytes(r, m->rdp_triangle_coverage,
                       T64_RDP_TRIANGLE_COVERAGE_BYTES);

  F32(rdp->work_policy_version);
  F32(rdp->random_state);
  F32_LE(rdp->refusal_reason, T64_RDP_REFUSAL_TEXTURE_RANGE,
         T64_STATE_RULE_ENUM_CLOSED);
  F64(rdp->command_count);
  F64(rdp->fill_count);
  F64(rdp->fullsync_count);
  F64(rdp->last_original_word);
  F32(rdp->last_command_byte);
  F32(rdp->last_opcode_field);
  x_begin = state_rd32(r);
  STATE_STORE(r, rdp->render_x_begin, x_begin);
  x_end = state_rd32(r);
  STATE_STORE(r, rdp->render_x_end, x_end);
  y_begin = state_rd32(r);
  STATE_STORE(r, rdp->render_y_begin, y_begin);
  y_end = state_rd32(r);
  STATE_STORE(r, rdp->render_y_end, y_end);
  y_now = state_rd32(r);
  STATE_STORE(r, rdp->render_y, y_now);
  FBOOL(rdp->color_image_valid);
  FBOOL(rdp->scissor_valid);
  FBOOL(rdp->other_modes_valid);
  FBOOL(rdp->fill_color_valid);
  render_active = state_rd_bool(r);
  fence_pending = state_rd_bool(r);
  busy = state_rd_bool(r);
  STATE_STORE(r, rdp->render_active, render_active);
  STATE_STORE(r, rdp->fence_pending, fence_pending);
  STATE_STORE(r, rdp->busy, busy);
  FBOOL(rdp->dpc_xbus);
  freeze = state_rd_bool(r);
  flush = state_rd_bool(r);
  STATE_STORE(r, rdp->dpc_freeze, freeze);
  STATE_STORE(r, rdp->dpc_flush, flush);
  FBOOL(rdp->dpc_start_valid);
  FBOOL(rdp->dpc_end_valid);
  command_pending = state_rd_bool(r);
  texrect_pending = state_rd_bool(r);
  STATE_STORE(r, rdp->command_pending, command_pending);
  STATE_STORE(r, rdp->texrect_pending, texrect_pending);
  FBOOL(rdp->render_texture_flip);
  /* A triangle render flag always belongs to a render in progress (the
   * refusal path and the last row clear both together). */
  STATE_CHECK(r, !render_triangle || render_active,
              T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
  if (render_active && render_triangle) {
    state_triangle triangle;

    triangle.packet = packet;
    triangle.word_count = words;
    triangle.shade = shade_triangle;
    triangle.view.scissor_xh = scissor_xh;
    triangle.view.scissor_yh = scissor_yh;
    triangle.view.scissor_xl = scissor_xl;
    triangle.view.scissor_yl = scissor_yl;
    triangle.view.image_address = image_address;
    triangle.view.image_width = image_width;
    triangle.view.image_size = image_size;
    triangle.view.other_modes = other_modes;
    triangle.view.depth_address = 0u;
    triangle.view.depth_valid = false;
    triangle.view.primitive_depth_valid = false;
    triangle.y_begin = y_begin;
    triangle.y_end = y_end;
    triangle.y_now = y_now;
    state_check_triangle_render(r, &triangle);
  }
  if (render_active) {
    /* A render in progress writes the rows up to render_y_end of the color
     * image: the last pixel of the last row, from the same acceptance extents
     * as the command (core/t64_rdp.c validate_fill_store and
     * core/t64_rdp_raster.c t64_rdp_triangle_bounds), must lie inside RDRAM.
     * The rows are not checked again when they run. */
    const uint64_t ram = (uint64_t)m->profile.rdram_bytes;
    const uint32_t pixel_bytes = image_size == 3u ? 4u : 2u;
    const uint32_t last_row = y_now > y_end ? y_now : y_end;
    uint64_t row_pixels = 0u;
    uint64_t last_byte = 0u;

    if (render_triangle) {
      const uint32_t clip = scissor_xl >> 2;

      row_pixels = clip < image_width ? clip : image_width;
    } else if (x_begin <= x_end) {
      row_pixels = (uint64_t)x_end + 1u;
    }
    STATE_CHECK(r, row_pixels == 0u ||
                       ((image_size == 2u || image_size == 3u) &&
                        image_width != 0u && row_pixels <= image_width &&
                        image_width <= ram && last_row <= ram &&
                        (image_address & (pixel_bytes - 1u)) == 0u),
                T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_ADDR_RANGE);
    if (row_pixels != 0u && !r->failed) {
      last_byte = (uint64_t)image_address +
                  ((uint64_t)last_row * image_width + row_pixels) *
                      (uint64_t)pixel_bytes;
    }
    STATE_CHECK(r, last_byte <= ram, T64_STATE_REASON_FIELD_RANGE,
                T64_STATE_RULE_ADDR_RANGE);
  }
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->rdp_idle = (!busy && !render_active && !fence_pending &&
                       !command_pending && !texrect_pending)
                          ? 1u
                          : 0u;
    /* An operation in flight owns its continuation event unless a freeze or a
     * flush holds the device (core/t64_rdp.c t64_rdp_event returns before it
     * reschedules while either is set). */
    facts->rdp_in_flight = (render_active || fence_pending ||
                            command_pending || texrect_pending)
                               ? 1u
                               : 0u;
    facts->rdp_held = (freeze || flush) ? 1u : 0u;
  }
  state_read_rdp_tiles(r);
}

/* One retained video packet slot: its bytes (up to the slot length) and the
 * metadata the v25 walker hashes. The profile and build identity strings are
 * not part of the v25 state; a produced slot (a non-zero length: the first
 * packet carries sequence 0) gets them from the live machine (same profile,
 * same build), the unused slot keeps zeroes. */
static void state_read_video_slot(state_reader *r, size_t slot) {
  t64_machine *m = r->machine;
  t64_video_packet_info info;
  const uint64_t length = state_rd64(r);
  uint64_t source_bytes;
  uint64_t packet_bytes;

  (void)memset(&info, 0, sizeof info);
  STATE_CHECK(r, length <= (uint64_t)T64_VIDEO_MAX_PACKET_BYTES,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_LEN_LE);
  if (length <= (uint64_t)T64_VIDEO_MAX_PACKET_BYTES) {
    (void)state_rd_bytes(r,
                         m->video_packets + slot * T64_VIDEO_MAX_PACKET_BYTES,
                         length);
  }
  info.width = state_rd32(r);
  info.height = state_rd32(r);
  info.stride_bytes = state_rd32(r);
  info.source_address = state_rd32(r);
  info.source_format = state_rd32(r);
  info.output_encoding = state_rd32(r);
  info.field = state_rd32(r);
  info.parity = state_rd32(r);
  info.control = state_rd32(r);
  info.burst = state_rd32(r);
  info.h_sync = state_rd32(r);
  info.v_sync = state_rd32(r);
  info.h_start = state_rd32(r);
  info.v_start = state_rd32(r);
  info.x_scale = state_rd32(r);
  info.y_scale = state_rd32(r);
  info.start_tick = state_rd64(r);
  info.end_tick = state_rd64(r);
  info.sequence = state_rd64(r);
  info.commands_completed = state_rd64(r);
  info.fullsync_completed = state_rd64(r);
  info.last_command_word = state_rd64(r);
  info.last_command_byte = state_rd32(r);
  info.last_opcode_field = state_rd32(r);
  source_bytes = state_rd64(r);
  packet_bytes = state_rd64(r);
  STATE_CHECK(r, source_bytes <= (uint64_t)SIZE_MAX &&
                     packet_bytes <= (uint64_t)SIZE_MAX,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_LEN_LE);
  info.source_bytes = (size_t)source_bytes;
  info.packet_bytes = (size_t)packet_bytes;
  state_rd_text(r, info.vi_clock_policy, sizeof info.vi_clock_policy);
  info.h_end = state_rd32(r);
  info.v_end = state_rd32(r);
  info.source_width = state_rd32(r);
  info.source_pitch_bytes = state_rd32(r);
  info.source_x_offset = state_rd32(r);
  info.source_y_offset = state_rd32(r);
  info.filter = state_rd32(r);
  state_rd_text(r, info.vi_output_policy, sizeof info.vi_output_policy);
  if (r->mode == STATE_MODE_APPLY) {
    if (length != 0u) {
      size_t i = 0u;

      while (i + 1u < sizeof info.profile_id &&
             m->profile.profile_id[i] != '\0') {
        info.profile_id[i] = m->profile.profile_id[i];
        i++;
      }
      i = 0u;
      while (i + 1u < sizeof info.build_id && t64_build_identity()[i] != '\0') {
        info.build_id[i] = t64_build_identity()[i];
        i++;
      }
    }
    m->vi.packet_lengths[slot] = (size_t)length;
    m->vi.packet_info[slot] = info;
  }
}

static void state_read_vi(state_reader *r) {
  t64_machine *m = r->machine;
  t64_vi_state *vi = &m->vi;
  t64_state_device_facts *facts = &m->state_summary.facts;
  uint64_t generation;
  bool enabled;
  size_t i;

  for (i = 0u; i < sizeof vi->registers / sizeof vi->registers[0]; i++) {
    F32(vi->registers[i]);
  }
  F64(vi->phase_remainder);
  F64(vi->phase_cycles);
  F64(vi->last_guest_tick);
  F64(vi->next_sequence);
  generation = state_rd64(r);
  STATE_STORE(r, vi->event_generation, generation);
  F32(vi->field);
  F32_LE(vi->refusal_reason, T64_VI_REFUSAL_SEQUENCE_EXHAUSTED,
         T64_STATE_RULE_ENUM_CLOSED);
  enabled = state_rd_bool(r);
  STATE_STORE(r, vi->enabled, enabled);
  FBOOL(vi->sequence_exhausted);
  for (i = 0u; i < sizeof vi->active_registers / sizeof vi->active_registers[0];
       i++) {
    F32(vi->active_registers[i]);
  }
  F64(vi->phase_units);
  F64(vi->next_boundary_units);
  F64(vi->field_start_tick);
  F32(vi->current_readback);
  FBOOL(vi->latch_pending);
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->vi_enabled = enabled ? 1u : 0u;
    facts->vi_event_generation = generation;
  }
  for (i = 0u; i < T64_VIDEO_QUEUE_CAPACITY; i++) {
    state_read_video_slot(r, i);
  }
}

/* The RDP members that end section 8 (plan 09-10, STATE-CONTENT-F1; the
 * walker streams them in core/t64_digest.c): the combiner block and the
 * memory coverage table. Before this section part existed the commit zeroed
 * them, so a peer drew different pixels from the source it had been imported
 * from. The ranges are the widths of the registers the acceptance stores them
 * from (core/t64_rdp.c apply_pending_command); a coverage entry is either clear
 * or the valid bit with a coverage of 0 to 8 (core/t64_rdp_pipeline.c). */
static void state_read_rdp_carried(state_reader *r) {
  t64_machine *m = r->machine;
  t64_rdp_state *rdp = &m->rdp;
  const size_t table_bytes =
      T64_RDP_MEMORY_COVERAGE_BYTES(m->profile.rdram_bytes);
  const uint8_t *entries;
  size_t i;

  F32_LE(rdp->primitive_lod_fraction, UINT32_C(0xff), T64_STATE_RULE_INDEX_LT);
  F32(rdp->environment_color);
  F32(rdp->fog_color);
  F32(rdp->blend_color);
  F64(rdp->combine_mode);
  FBOOL(rdp->combine_mode_valid);
  FBOOL(rdp->environment_color_valid);
  FBOOL(rdp->fog_color_valid);
  FBOOL(rdp->blend_color_valid);
  FBOOL(rdp->key_center_valid);
  FBOOL(rdp->key_scale_valid);
  FBOOL(rdp->key_width_valid);
  for (i = 0u; i < 3u; i++) {
    F32_LE(rdp->key_center[i], UINT32_C(0xff), T64_STATE_RULE_INDEX_LT);
    F32_LE(rdp->key_scale[i], UINT32_C(0xff), T64_STATE_RULE_INDEX_LT);
    F32_LE(rdp->key_width[i], UINT32_C(0xfff), T64_STATE_RULE_INDEX_LT);
  }
  F32_SAMER((uint32_t)table_bytes, T64_STATE_RULE_PROFILE_DERIVED);
  entries = state_rd_bytes(r, m->rdp_memory_coverage, table_bytes);
  if (r->mode == STATE_MODE_VALIDATE && entries != NULL) {
    bool closed = true;

    for (i = 0u; i < table_bytes && closed; i++) {
      closed = entries[i] == 0u ||
               (entries[i] >= UINT8_C(0x80) && entries[i] <= UINT8_C(0x88));
    }
    STATE_CHECK(r, closed, T64_STATE_REASON_FIELD_RANGE,
                T64_STATE_RULE_ENUM_CLOSED);
  }
}

static void state_section_rdp_vi(state_reader *r) {
  state_read_rdp(r);
  state_read_vi(r);
  state_read_rdp_carried(r);
}

/* ---- sections 9 to 11 --------------------------------------------------- */

#define STATE_AI_CONTROL_ENABLE UINT32_C(1)
#define STATE_AI_DACRATE_MASK UINT32_C(0x00003fff)
#define STATE_AI_BITRATE_MASK UINT32_C(0x0000000f)

static void state_read_audio_slot(state_reader *r, size_t slot) {
  t64_machine *m = r->machine;
  t64_audio_packet_info info;
  const uint64_t length = state_rd64(r);
  uint64_t byte_length;

  (void)memset(&info, 0, sizeof info);
  STATE_CHECK(r, length <= (uint64_t)T64_AUDIO_MAX_PACKET_BYTES,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_LEN_LE);
  if (length <= (uint64_t)T64_AUDIO_MAX_PACKET_BYTES) {
    (void)state_rd_bytes(r,
                         m->audio_packets + slot * T64_AUDIO_MAX_PACKET_BYTES,
                         length);
  }
  info.struct_size = state_rd32(r);
  info.sequence = state_rd64(r);
  info.first_sample_index = state_rd64(r);
  info.first_tick = state_rd64(r);
  info.last_tick = state_rd64(r);
  info.frame_count = state_rd32(r);
  info.rate_numerator = state_rd64(r);
  info.rate_denominator = state_rd64(r);
  info.channels = state_rd32(r);
  info.bits_per_sample = state_rd32(r);
  info.encoding = state_rd32(r);
  byte_length = state_rd64(r);
  STATE_CHECK(r, byte_length <= (uint64_t)T64_AUDIO_MAX_PACKET_BYTES,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_LEN_LE);
  info.byte_length = (size_t)byte_length;
  if (r->mode == STATE_MODE_APPLY) {
    if (length != 0u) {
      size_t i = 0u;

      while (i + 1u < sizeof info.profile_id &&
             m->profile.profile_id[i] != '\0') {
        info.profile_id[i] = m->profile.profile_id[i];
        i++;
      }
      i = 0u;
      while (i + 1u < sizeof info.build_id && t64_build_identity()[i] != '\0') {
        info.build_id[i] = t64_build_identity()[i];
        i++;
      }
    }
    m->ai.packet_lengths[slot] = (size_t)length;
    m->ai.packet_info[slot] = info;
  }
}

/* Section 9: audio interface. Apply stores the queued descriptors in logical
 * order from slot 0; the head is not canonical (the walker hashes the
 * descriptors from the head), and every consumer indexes relative to it. */
static void state_section_audio(state_reader *r) {
  t64_machine *m = r->machine;
  t64_ai_state *ai = &m->ai;
  t64_state_device_facts *facts = &m->state_summary.facts;
  uint32_t staged;
  uint32_t cursor;
  uint32_t count;
  uint64_t packet_length;
  uint64_t event_token;
  uint64_t paused_remaining;
  bool active;
  bool paused;
  bool reserved;
  size_t i;

  staged = state_rd32(r);
  STATE_CHECK(r, (staged & UINT32_C(7)) == 0u, T64_STATE_REASON_FIELD_RANGE,
              T64_STATE_RULE_ALIGN);
  STATE_CHECK(r, staged < m->profile.rdram_bytes, T64_STATE_REASON_FIELD_RANGE,
              T64_STATE_RULE_ADDR_RANGE);
  STATE_STORE(r, ai->staged_address, staged);
  FBOOL(ai->address_staged);
  F32_MASK(ai->control, STATE_AI_CONTROL_ENABLE);
  F32_MASK(ai->dacrate, STATE_AI_DACRATE_MASK);
  F32_MASK(ai->bitrate, STATE_AI_BITRATE_MASK);
  F32_MASK(ai->latched_dacrate, STATE_AI_DACRATE_MASK);
  F32_MASK(ai->latched_bitrate, STATE_AI_BITRATE_MASK);
  F64(ai->phase_remainder);
  cursor = state_rd32(r);
  STATE_CHECK(r, cursor <= (uint32_t)T64_AUDIO_MAX_PACKET_BYTES,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_LEN_LE);
  STATE_STORE(r, ai->active_byte_cursor, cursor);
  F64(ai->sample_index);
  F64(ai->descriptor_sequence);
  event_token = state_rd64(r);
  STATE_STORE(r, ai->event_token, event_token);
  F64(ai->event_generation);
  F64(ai->packet_start_tick);
  F64(ai->packet_first_sample);
  packet_length = state_rd64(r);
  STATE_CHECK(r, packet_length <= (uint64_t)T64_AUDIO_MAX_PACKET_BYTES,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_LEN_LE);
  STATE_STORE(r, ai->packet_length, (size_t)packet_length);
  count = state_rd32(r);
  STATE_CHECK(r, count <= T64_AUDIO_QUEUE_CAPACITY,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_COUNT_CAP);
  if (count > T64_AUDIO_QUEUE_CAPACITY) {
    count = 0u;
  }
  STATE_STORE(r, ai->descriptor_count, (size_t)count);
  STATE_STORE(r, ai->descriptor_head, (size_t)0u);
  active = state_rd_bool(r);
  paused = state_rd_bool(r);
  STATE_STORE(r, ai->active, active);
  STATE_STORE(r, ai->paused, paused);
  paused_remaining = state_rd64(r);
  STATE_STORE(r, ai->paused_remaining_ticks, paused_remaining);
  reserved = state_rd_bool(r);
  STATE_STORE(r, ai->packet_reserved, reserved);
  /* An in-progress packet holds exactly the bytes of the frames consumed. */
  STATE_CHECK(r, reserved ? packet_length == (uint64_t)cursor
                          : packet_length == 0u,
              T64_STATE_REASON_FIELD_DERIVED, T64_STATE_RULE_DERIVED_EQ);
  F32_LT(ai->output_reservation_slot, T64_AUDIO_QUEUE_CAPACITY,
         T64_STATE_RULE_INDEX_LT);
  FBOOL(ai->sequence_exhausted);
  for (i = 0u; i < count && !r->failed; i++) {
    t64_ai_dma_descriptor descriptor;

    descriptor.address = state_rd32(r);
    descriptor.length = state_rd32(r);
    descriptor.sequence = state_rd64(r);
    descriptor.dacrate = state_rd32(r);
    descriptor.bitrate = state_rd32(r);
    STATE_CHECK(r, (descriptor.address & UINT32_C(7)) == 0u,
                T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_ALIGN);
    STATE_CHECK(r, descriptor.length >= 8u &&
                       descriptor.length <= (uint32_t)T64_AUDIO_MAX_PACKET_BYTES &&
                       (descriptor.length & UINT32_C(7)) == 0u,
                T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_LEN_LE);
    STATE_CHECK(r, (uint64_t)descriptor.address + (uint64_t)descriptor.length <=
                       (uint64_t)m->profile.rdram_bytes,
                T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_ADDR_RANGE);
    STATE_CHECK(r, descriptor.dacrate <= STATE_AI_DACRATE_MASK &&
                       descriptor.bitrate <= STATE_AI_BITRATE_MASK,
                T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_INDEX_LT);
    STATE_CHECK(r, !active || cursor <= descriptor.length || i != 0u,
                T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_LEN_LE);
    STATE_STORE(r, ai->descriptors[i], descriptor);
  }
  F64(ai->packet_history_count);
  (void)state_rd_bytes(r, ai->packet_history_sha256,
                       sizeof ai->packet_history_sha256);
  for (i = 0u; i < T64_AUDIO_QUEUE_CAPACITY; i++) {
    state_read_audio_slot(r, i);
  }
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->ai_descriptor_count = count;
    facts->ai_active = active ? 1u : 0u;
    facts->ai_paused = paused ? 1u : 0u;
    facts->ai_event_token = event_token;
    facts->ai_paused_remaining_ticks = paused_remaining;
  }
}

/* Section 10: SI, PIF RAM and the input latch (the 09-03 body). */
static void state_section_si_pif(state_reader *r) {
  t64_machine *m = r->machine;
  t64_si_state *si = &m->si;
  t64_state_device_facts *facts = &m->state_summary.facts;
  uint32_t direction;
  uint32_t entry_count;
  uint32_t address;
  uint64_t start_tick;
  uint64_t execute_tick;
  uint64_t complete_tick;
  uint64_t token;
  bool active;
  bool execute_pending;
  size_t i;

  address = state_rd32(r);
  STATE_CHECK(r, (address & ~T64_SI_DRAM_ADDRESS_MASK) == 0u,
              T64_STATE_REASON_FIELD_RESERVED, T64_STATE_RULE_RESERVED_ZERO);
  STATE_STORE(r, si->dram_address, address);
  FBOOL(si->error);
  active = state_rd_bool(r);
  STATE_STORE(r, si->active_valid, active);
  direction = state_rd32(r);
  STATE_CHECK(r, direction <= T64_SI_DIRECTION_RD64B,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_ENUM_CLOSED);
  STATE_STORE(r, si->active_direction, direction);
  address = state_rd32(r);
  STATE_CHECK(r, (address & UINT32_C(7)) == 0u, T64_STATE_REASON_FIELD_RANGE,
              T64_STATE_RULE_ALIGN);
  STATE_CHECK(r, (uint64_t)address + (uint64_t)T64_PIF_RAM_BYTES <=
                     (uint64_t)m->profile.rdram_bytes ||
                     !active,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_ADDR_RANGE);
  STATE_STORE(r, si->active_dram_address, address);
  start_tick = state_rd64(r);
  execute_tick = state_rd64(r);
  complete_tick = state_rd64(r);
  token = state_rd64(r);
  STATE_STORE(r, si->start_tick, start_tick);
  STATE_STORE(r, si->execute_tick, execute_tick);
  STATE_STORE(r, si->complete_tick, complete_tick);
  STATE_STORE(r, si->active_token, token);
  F64(si->next_token);
  FBOOL(si->token_exhausted);
  execute_pending = state_rd_bool(r);
  STATE_STORE(r, si->execute_pending, execute_pending);
  (void)state_rd_bytes(r, si->pif_ram, sizeof si->pif_ram);
  FBOOL(si->table.parsed);
  entry_count = state_rd32(r);
  STATE_CHECK(r, entry_count <= T64_JOYBUS_CHANNELS,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_COUNT_CAP);
  STATE_STORE(r, si->table.entry_count, entry_count);
  for (i = 0u; i < T64_JOYBUS_CHANNELS; i++) {
    t64_joybus_entry *entry = &si->table.entries[i];

    F32_LE(entry->kind, (uint32_t)T64_JOYBUS_ENTRY_RESET,
           T64_STATE_RULE_ENUM_CLOSED);
    F32_LT(entry->channel, T64_JOYBUS_CHANNELS, T64_STATE_RULE_INDEX_LT);
    F32_LE(entry->offset, UINT32_C(62), T64_STATE_RULE_INDEX_LT);
    F32_LE(entry->tx_count, UINT32_C(63), T64_STATE_RULE_LEN_LE);
    F32_LE(entry->rx_count, UINT32_C(63), T64_STATE_RULE_LEN_LE);
    F32_MASK(entry->flags, UINT32_C(0xc0));
  }
  for (i = 0u; i < T64_SI_PORTS; i++) {
    t64_si_port_latch *latch = &si->latch[i];

    F32_U16(latch->current_buttons);
    F32_U8(latch->current_x, UINT32_C(0xff));
    F32_U8(latch->current_y, UINT32_C(0xff));
    F32_U16(latch->next_buttons);
    F32_U8(latch->next_x, UINT32_C(0xff));
    F32_U8(latch->next_y, UINT32_C(0xff));
    F64(latch->next_visible_from);
    FBOOL(latch->next_valid);
  }
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->si_active_valid = active ? 1u : 0u;
    facts->si_direction = direction;
    facts->si_execute_pending = execute_pending ? 1u : 0u;
    facts->si_start_tick = start_tick;
    facts->si_execute_tick = execute_tick;
    facts->si_complete_tick = complete_tick;
    facts->si_active_token = token;
  }
}

/* Section 11: save media (the three save ids). The EEPROM deadline is bounded
 * above by the blob tick plus the profile busy window; an older (stale)
 * deadline is accepted and kept so the digest is reproduced exactly. */
static void state_section_save_media(state_reader *r) {
  t64_machine *m = r->machine;
  t64_state_device_facts *facts = &m->state_summary.facts;
  const uint32_t kind = m->profile.save_kind;
  const size_t size = t64_save_storage_bytes(kind);
  uint64_t window = 0u;
  uint64_t limit;
  uint64_t deadline;
  uint32_t direction;

  if (kind == T64_SAVE_EEPROM_4K || kind == T64_SAVE_EEPROM_16K) {
    window = T64_SAVE_EEPROM_BUSY_TICKS;
  }
  F32_SAMER(kind, T64_STATE_RULE_PROFILE_DERIVED);
  F32_SAMER((uint32_t)size, T64_STATE_RULE_PROFILE_DERIVED);
  deadline = state_rd64(r);
  limit = facts->tick > UINT64_MAX - window ? UINT64_MAX : facts->tick + window;
  STATE_CHECK(r, deadline <= limit, T64_STATE_REASON_FIELD_RANGE,
              T64_STATE_RULE_TICK_LE);
  STATE_STORE(r, m->save.busy_until, deadline);
  direction = state_rd32(r);
  STATE_CHECK(r, direction <= T64_PI_DIRECTION_RDRAM_TO_SRAM,
              T64_STATE_REASON_FIELD_RANGE, T64_STATE_RULE_ENUM_CLOSED);
  STATE_STORE(r, m->pi.active_save_direction, direction);
  if (r->mode == STATE_MODE_VALIDATE) {
    facts->pi_save_direction = direction;
  }
  (void)state_rd_bytes(r, m->save.bytes, size);
}

/* Section 16: observer continuity, outside the digest. Five u64 values: the
 * output drain cursor and the video and audio ring head and occupancy. */
static void state_section_observer(state_reader *r) {
  t64_machine *m = r->machine;
  const t64_state_device_facts *facts = &m->state_summary.facts;
  uint64_t cursor;
  uint64_t video_head;
  uint64_t video_count;
  uint64_t audio_head;
  uint64_t audio_count;

  cursor = state_rd64(r);
  video_head = state_rd64(r);
  video_count = state_rd64(r);
  audio_head = state_rd64(r);
  audio_count = state_rd64(r);
  STATE_CHECK(r, cursor <= (uint64_t)facts->captured_bytes,
              T64_STATE_REASON_OBSERVER_RANGE, T64_STATE_RULE_LEN_LE);
  STATE_CHECK(r, video_head < T64_VIDEO_QUEUE_CAPACITY &&
                     audio_head < T64_AUDIO_QUEUE_CAPACITY,
              T64_STATE_REASON_OBSERVER_RANGE, T64_STATE_RULE_INDEX_LT);
  STATE_CHECK(r, video_count <= T64_VIDEO_QUEUE_CAPACITY &&
                     audio_count <= T64_AUDIO_QUEUE_CAPACITY,
              T64_STATE_REASON_OBSERVER_RANGE, T64_STATE_RULE_COUNT_CAP);
  if (r->mode == STATE_MODE_APPLY) {
    if (r->drop_observers) {
      /* No pre-import byte is delivered again: both rings are empty and the
       * drain cursor sits at the retained capture length. */
      m->output_drain_cursor = (size_t)m->output.captured_bytes;
      m->vi.packet_head = 0u;
      m->vi.packet_count = 0u;
      m->ai.packet_head = 0u;
      m->ai.packet_count = 0u;
    } else {
      m->output_drain_cursor = (size_t)cursor;
      m->vi.packet_head = (size_t)video_head;
      m->vi.packet_count = (size_t)video_count;
      m->ai.packet_head = (size_t)audio_head;
      m->ai.packet_count = (size_t)audio_count;
    }
  }
}

/* One section reader per id, the same function in both modes. */
static void state_run_section(state_reader *reader, uint32_t id) {
  switch (id) {
  case T64_STATE_SECTION_IDENTITY:
    state_section_identity(reader);
    break;
  case T64_STATE_SECTION_MACHINE_CORE:
    state_section_machine_core(reader);
    break;
  case T64_STATE_SECTION_RDRAM:
    state_section_rdram(reader);
    break;
  case T64_STATE_SECTION_SP_MEMORY:
    state_section_sp_memory(reader);
    break;
  case T64_STATE_SECTION_INTERRUPT_PI_SP:
    state_section_interrupt_pi_sp(reader);
    break;
  case T64_STATE_SECTION_SCHEDULER:
    state_section_scheduler(reader);
    break;
  case T64_STATE_SECTION_WARM_DEVICES:
    state_section_warm_devices(reader);
    break;
  case T64_STATE_SECTION_RDP_VI:
    state_section_rdp_vi(reader);
    break;
  case T64_STATE_SECTION_AUDIO:
    state_section_audio(reader);
    break;
  case T64_STATE_SECTION_SI_PIF:
    state_section_si_pif(reader);
    break;
  case T64_STATE_SECTION_SAVE_MEDIA:
    state_section_save_media(reader);
    break;
  case T64_STATE_SECTION_OBSERVER_CONTINUITY:
    state_section_observer(reader);
    break;
  default:
    state_fail(reader, T64_STATE_REASON_SECTION_ID_UNKNOWN,
               T64_STATE_RULE_NONE);
    break;
  }
}

/* Run every reader in ascending id order over the validated table. A reader
 * that does not consume exactly its section is a length failure. */
static bool state_run_sections(t64_machine *machine, const uint8_t *bytes,
                               uint64_t length, const state_table *table,
                               uint32_t flags, state_mode mode,
                               t64_state_import_report *report) {
  state_reader reader;
  uint32_t index;

  (void)memset(&reader, 0, sizeof reader);
  reader.base = bytes;
  reader.length = length;
  reader.mode = mode;
  reader.machine = machine;
  reader.report = report;
  reader.drop_observers = (flags & T64_STATE_IMPORT_DROP_OBSERVERS) != 0u;
  for (index = 0u; index < table->count && !reader.failed; index++) {
    const state_section *section = &table->sections[index];

    reader.position = section->offset;
    reader.limit = section->offset + section->length;
    reader.section_id = section->id;
    state_run_section(&reader, section->id);
    STATE_CHECK(&reader, reader.position == reader.limit,
                T64_STATE_REASON_SECTION_LENGTH_EXPECTED, T64_STATE_RULE_NONE);
  }
  return !reader.failed;
}

/* ---- the structural and per-device invariants -------------------------- */

#define STATE_DEVICE_CHECK(condition, reason, report, section)                \
  do {                                                                        \
    if (!(condition)) {                                                       \
      state_report_failure((report), (reason), (section), T64_STATE_RULE_NONE, \
                           0u);                                               \
      return false;                                                           \
    }                                                                         \
  } while (0)

static bool state_check_cpu_pi_sp(const t64_state_summary *summary,
                                  const t64_machine *machine,
                                  t64_state_import_report *report) {
  const t64_state_device_facts *f = &summary->facts;
  const t64_state_pair *cpu = &summary->pairs[PAIR_CPU_STEP];
  const t64_state_pair *pi = &summary->pairs[PAIR_PI_COMPLETE];
  const t64_state_pair *sp_complete = &summary->pairs[PAIR_SP_COMPLETE];
  const t64_state_pair *rsp = &summary->pairs[PAIR_SP_RSP_ISSUE];
  const bool step_expected =
      f->cpu_stopped == 0u && f->halted == 0u &&
      f->cpu_attempt_outcome != (uint32_t)T64_CPU_ATTEMPT_HOST_ERROR;

  /* CPU: one step event while it runs (the exception and the step both
   * re-reserve; a refused re-reserve leaves the host-error outcome and none). */
  STATE_DEVICE_CHECK(cpu->count <= 1u, T64_STATE_REASON_DEVICE_EVENT_EXTRA,
                     report, T64_STATE_SECTION_SCHEDULER);
  STATE_DEVICE_CHECK(cpu->count == 1u || !step_expected,
                     T64_STATE_REASON_DEVICE_EVENT_MISSING, report,
                     T64_STATE_SECTION_SCHEDULER);
  STATE_DEVICE_CHECK(cpu->count == 0u || (cpu->token == 0u && cpu->value == 0u),
                     T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, report,
                     T64_STATE_SECTION_SCHEDULER);

  /* PI: an active transfer owns exactly one completion event. */
  if (f->pi_active_valid != 0u) {
    STATE_DEVICE_CHECK(pi->count >= 1u, T64_STATE_REASON_DEVICE_EVENT_MISSING,
                       report, T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(pi->count == 1u, T64_STATE_REASON_DEVICE_EVENT_EXTRA,
                       report, T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(pi->token == f->pi_active_token,
                       T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, report,
                       T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(pi->value == f->pi_active_length,
                       T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, report,
                       T64_STATE_SECTION_SCHEDULER);
  } else {
    STATE_DEVICE_CHECK(pi->count == 0u, T64_STATE_REASON_DEVICE_EVENT_EXTRA,
                       report, T64_STATE_SECTION_SCHEDULER);
  }
  /* The SRAM directions exist on the SRAM save id only and only while a
   * transfer is active. */
  STATE_DEVICE_CHECK(f->pi_save_direction == 0u ||
                         (f->pi_active_valid != 0u &&
                          machine->profile.save_kind == T64_SAVE_SRAM_32K),
                     T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, report,
                     T64_STATE_SECTION_SAVE_MEDIA);

  /* SP: the functional RSP profile moves SP DMA through the RSP beats, so no
   * scheduled completion event exists; the RSP issue event is one at most,
   * with the issue sequence as its token and the next issue tick as its tick,
   * and only while that tick has not passed. */
  STATE_DEVICE_CHECK(sp_complete->count == 0u,
                     T64_STATE_REASON_DEVICE_EVENT_EXTRA, report,
                     T64_STATE_SECTION_SCHEDULER);
  STATE_DEVICE_CHECK(rsp->count <= 1u, T64_STATE_REASON_DEVICE_EVENT_EXTRA,
                     report, T64_STATE_SECTION_SCHEDULER);
  if (rsp->count == 1u) {
    STATE_DEVICE_CHECK(f->rsp_next_issue_tick >= f->tick,
                       T64_STATE_REASON_DEVICE_EVENT_EXTRA, report,
                       T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(rsp->token == f->rsp_issue_sequence,
                       T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, report,
                       T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(rsp->tick == f->rsp_next_issue_tick && rsp->value == 0u,
                       T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, report,
                       T64_STATE_SECTION_SCHEDULER);
  }
  return true;
}

static bool state_check_video_audio_rdp(const t64_state_summary *summary,
                                        t64_state_import_report *report) {
  const t64_state_device_facts *f = &summary->facts;
  const t64_state_pair *rdp = &summary->pairs[PAIR_RDP_WORK];
  const t64_state_pair *vi = &summary->pairs[PAIR_VI_FIELD];
  const t64_state_pair *ai = &summary->pairs[PAIR_AI_SAMPLE];

  /* RDP: one work event at most, none on an idle device. */
  STATE_DEVICE_CHECK(rdp->count <= 1u, T64_STATE_REASON_DEVICE_EVENT_EXTRA,
                     report, T64_STATE_SECTION_SCHEDULER);
  STATE_DEVICE_CHECK(rdp->count == 0u || f->rdp_idle == 0u,
                     T64_STATE_REASON_DEVICE_EVENT_EXTRA, report,
                     T64_STATE_SECTION_SCHEDULER);
  STATE_DEVICE_CHECK(rdp->count == 0u || (rdp->token == 0u && rdp->value == 0u),
                     T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, report,
                     T64_STATE_SECTION_SCHEDULER);
  /* A fetched command, a render in progress, a pending fence or a pending
   * texture rectangle is applied by the next work event; only a freeze or a
   * flush leaves such a state without one (found by the boundary matrix row
   * B05: the drop mutant was accepted). */
  STATE_DEVICE_CHECK(rdp->count >= 1u || f->rdp_in_flight == 0u ||
                         f->rdp_held != 0u,
                     T64_STATE_REASON_DEVICE_EVENT_MISSING, report,
                     T64_STATE_SECTION_SCHEDULER);

  /* VI: an event with an older generation is stale and harmless; an enabled
   * VI owns exactly one event of the current generation (queued but not
   * dispatchable is accepted: the ring occupancy gates dispatch only); a
   * disabled VI owns none; no event carries a future generation. */
  STATE_DEVICE_CHECK(vi->count == 0u || vi->token_max <= f->vi_event_generation,
                     T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, report,
                     T64_STATE_SECTION_SCHEDULER);
  if (f->vi_enabled != 0u) {
    STATE_DEVICE_CHECK(vi->count >= 1u &&
                           vi->token_max == f->vi_event_generation,
                       T64_STATE_REASON_DEVICE_EVENT_MISSING, report,
                       T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(vi->max_count == 1u,
                       T64_STATE_REASON_DEVICE_EVENT_EXTRA, report,
                       T64_STATE_SECTION_SCHEDULER);
  } else {
    STATE_DEVICE_CHECK(vi->count == 0u ||
                           vi->token_max < f->vi_event_generation,
                       T64_STATE_REASON_DEVICE_EVENT_EXTRA, report,
                       T64_STATE_SECTION_SCHEDULER);
  }

  /* AI: an active, unpaused interface owns exactly one sample event with the
   * current token; a paused one owns none and a positive remaining delay. */
  STATE_DEVICE_CHECK(f->ai_active != 0u || f->ai_paused == 0u,
                     T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, report,
                     T64_STATE_SECTION_AUDIO);
  STATE_DEVICE_CHECK(f->ai_active == 0u || f->ai_descriptor_count != 0u,
                     T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, report,
                     T64_STATE_SECTION_AUDIO);
  STATE_DEVICE_CHECK(f->ai_paused == 0u || f->ai_paused_remaining_ticks != 0u,
                     T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, report,
                     T64_STATE_SECTION_AUDIO);
  if (f->ai_active != 0u && f->ai_paused == 0u) {
    STATE_DEVICE_CHECK(ai->count >= 1u, T64_STATE_REASON_DEVICE_EVENT_MISSING,
                       report, T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(ai->count == 1u, T64_STATE_REASON_DEVICE_EVENT_EXTRA,
                       report, T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(ai->token == f->ai_event_token,
                       T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, report,
                       T64_STATE_SECTION_SCHEDULER);
  } else {
    STATE_DEVICE_CHECK(ai->count == 0u, T64_STATE_REASON_DEVICE_EVENT_EXTRA,
                       report, T64_STATE_SECTION_SCHEDULER);
  }
  return true;
}

static bool state_check_si(const t64_state_summary *summary,
                           t64_state_import_report *report) {
  const t64_state_device_facts *f = &summary->facts;
  const t64_state_pair *execute = &summary->pairs[PAIR_SI_EXECUTE];
  const t64_state_pair *complete = &summary->pairs[PAIR_SI_COMPLETE];

  if (f->si_active_valid == 0u) {
    STATE_DEVICE_CHECK(execute->count == 0u && complete->count == 0u,
                       T64_STATE_REASON_DEVICE_EVENT_EXTRA, report,
                       T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(f->si_direction == T64_SI_DIRECTION_NONE &&
                           f->si_execute_pending == 0u,
                       T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, report,
                       T64_STATE_SECTION_SI_PIF);
    return true;
  }
  STATE_DEVICE_CHECK(f->si_direction == T64_SI_DIRECTION_WR64B ||
                         f->si_direction == T64_SI_DIRECTION_RD64B,
                     T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, report,
                     T64_STATE_SECTION_SI_PIF);
  STATE_DEVICE_CHECK(complete->count >= 1u,
                     T64_STATE_REASON_DEVICE_EVENT_MISSING, report,
                     T64_STATE_SECTION_SCHEDULER);
  STATE_DEVICE_CHECK(complete->count == 1u,
                     T64_STATE_REASON_DEVICE_EVENT_EXTRA, report,
                     T64_STATE_SECTION_SCHEDULER);
  STATE_DEVICE_CHECK(complete->token == f->si_active_token,
                     T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, report,
                     T64_STATE_SECTION_SCHEDULER);
  STATE_DEVICE_CHECK(complete->tick == f->si_complete_tick &&
                         complete->value == 0u &&
                         f->si_start_tick <= f->si_complete_tick,
                     T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, report,
                     T64_STATE_SECTION_SCHEDULER);
  if (f->si_direction == T64_SI_DIRECTION_WR64B) {
    STATE_DEVICE_CHECK(execute->count == 0u,
                       T64_STATE_REASON_DEVICE_EVENT_EXTRA, report,
                       T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(f->si_execute_pending == 0u,
                       T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, report,
                       T64_STATE_SECTION_SI_PIF);
    return true;
  }
  if (f->si_execute_pending != 0u) {
    STATE_DEVICE_CHECK(execute->count >= 1u,
                       T64_STATE_REASON_DEVICE_EVENT_MISSING, report,
                       T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(execute->count == 1u,
                       T64_STATE_REASON_DEVICE_EVENT_EXTRA, report,
                       T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(execute->token == f->si_active_token,
                       T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, report,
                       T64_STATE_SECTION_SCHEDULER);
    STATE_DEVICE_CHECK(execute->tick == f->si_execute_tick &&
                           execute->value == 0u &&
                           f->si_start_tick <= f->si_execute_tick &&
                           f->si_execute_tick <= f->si_complete_tick,
                       T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, report,
                       T64_STATE_SECTION_SCHEDULER);
  } else {
    STATE_DEVICE_CHECK(execute->count == 0u,
                       T64_STATE_REASON_DEVICE_EVENT_EXTRA, report,
                       T64_STATE_SECTION_SCHEDULER);
  }
  return true;
}

/* The per-device pass reads only the scratch facts and the per-pair queue
 * summary; each rule is derived from the cited device source and the table is
 * kept in docs/spec/state-container.md. */
static bool state_check_devices(const t64_machine *machine,
                                t64_state_import_report *report) {
  return state_check_cpu_pi_sp(&machine->state_summary, machine, report) &&
         state_check_video_audio_rdp(&machine->state_summary, report) &&
         state_check_si(&machine->state_summary, report);
}

/* ---- container verification: arguments, header, trailer, binding, table -- */

static bool state_sha256_span(const uint8_t *bytes, uint64_t length,
                              uint8_t digest[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;

  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, (size_t)length);
  t64_sha256_final(&context, digest);
  return true;
}

/* Returns the caller report pointer to use: NULL when the report span itself is
 * unusable (it overlaps live storage or the blob), so no byte is ever written
 * into it. */
static bool state_check_arguments(const t64_machine *machine,
                                  const uint8_t *bytes, size_t length,
                                  uint32_t flags,
                                  const t64_state_import_report *report_out,
                                  t64_state_import_report *report,
                                  bool *report_usable) {
  t64_address_range blob_range;
  t64_address_range report_range;

  *report_usable = report_out != NULL;
  if (machine == NULL || bytes == NULL) {
    state_report_failure(report, T64_STATE_REASON_ARG_NULL, 0u, 0u, 0u);
    return false;
  }
  if (length == 0u || (uint64_t)length > T64_STATE_MAX_BYTES) {
    state_report_failure(report, T64_STATE_REASON_ARG_LENGTH, 0u, 0u, 0u);
    return false;
  }
  if ((flags & ~T64_STATE_IMPORT_DROP_OBSERVERS) != 0u) {
    state_report_failure(report, T64_STATE_REASON_ARG_FLAGS, 0u, 0u, 0u);
    return false;
  }
  if (!t64_internal_make_range(bytes, length, &blob_range) ||
      !t64_internal_observation_range_is_disjoint(machine, blob_range)) {
    state_report_failure(report, T64_STATE_REASON_ARG_OVERLAP_LIVE, 0u, 0u,
                         0u);
    return false;
  }
  if (report_out != NULL &&
      (!t64_internal_make_range(report_out, sizeof *report_out,
                                &report_range) ||
       t64_internal_ranges_overlap(report_range, blob_range) ||
       !t64_internal_observation_range_is_disjoint(machine, report_range))) {
    *report_usable = false;
    state_report_failure(report, T64_STATE_REASON_ARG_OVERLAP_LIVE, 0u, 0u,
                         0u);
    return false;
  }
  if (!machine->has_image) {
    state_report_failure(report, T64_STATE_REASON_NOT_ADMITTED, 0u, 0u, 0u);
    return false;
  }
  return true;
}

#define HDR_MAGIC 0x000u
#define HDR_CONTAINER_VERSION 0x008u
#define HDR_STATE_VERSION 0x00Cu
#define HDR_TOTAL 0x010u
#define HDR_COUNT 0x018u
#define HDR_FLAGS 0x01Cu
#define HDR_PROFILE 0x020u
#define HDR_CONTENT 0x040u
#define HDR_CONTENT_LENGTH 0x060u
#define HDR_TICK 0x068u
#define HDR_DECLARED 0x070u
#define HDR_CANONICAL_OFFSET 0x090u
#define HDR_CANONICAL_LENGTH 0x098u
#define HDR_BUILD 0x0A0u

static bool state_parse_header(const uint8_t *bytes, uint64_t length,
                              state_table *table,
                              t64_state_import_report *report) {
  uint64_t total;
  uint32_t count;
  uint32_t index;
  bool terminated = false;

  if (length < (uint64_t)T64_STATE_HEADER_BYTES + T64_STATE_TRAILER_BYTES) {
    state_report_failure(report, T64_STATE_REASON_TOTAL_LENGTH, 0u, 0u, 0u);
    return false;
  }
  if (memcmp(bytes + HDR_MAGIC, T64_STATE_CONTAINER_MAGIC, 8u) != 0) {
    state_report_failure(report, T64_STATE_REASON_MAGIC, 0u, 0u, HDR_MAGIC);
    return false;
  }
  if (state_be32(bytes + HDR_CONTAINER_VERSION) != T64_STATE_CONTAINER_VERSION) {
    state_report_failure(report, T64_STATE_REASON_CONTAINER_VERSION, 0u, 0u,
                         HDR_CONTAINER_VERSION);
    return false;
  }
  total = state_be64(bytes + HDR_TOTAL);
  if (total != length) {
    state_report_failure(report, T64_STATE_REASON_TOTAL_LENGTH, 0u, 0u,
                         HDR_TOTAL);
    return false;
  }
  count = state_be32(bytes + HDR_COUNT);
  if (count < 1u || count > T64_STATE_MAX_SECTIONS) {
    state_report_failure(report, T64_STATE_REASON_SECTION_COUNT, 0u, 0u,
                         HDR_COUNT);
    return false;
  }
  if (state_be32(bytes + HDR_FLAGS) != 0u) {
    state_report_failure(report, T64_STATE_REASON_HEADER_FLAGS, 0u, 0u,
                         HDR_FLAGS);
    return false;
  }
  /* The build identity field is text with a terminator inside it; every byte
   * after the terminator is reserved and must be zero. */
  for (index = 0u; index < T64_STATE_BUILD_ID_BYTES; index++) {
    const uint8_t value = bytes[HDR_BUILD + index];

    if (!terminated) {
      terminated = value == 0u;
    } else if (value != 0u) {
      state_report_failure(report, T64_STATE_REASON_HEADER_RESERVED, 0u, 0u,
                           (uint64_t)HDR_BUILD + index);
      return false;
    }
  }
  if (!terminated) {
    state_report_failure(report, T64_STATE_REASON_HEADER_RESERVED, 0u, 0u,
                         HDR_BUILD);
    return false;
  }
  (void)memset(table, 0, sizeof *table);
  table->count = count;
  table->canonical_offset = state_be64(bytes + HDR_CANONICAL_OFFSET);
  table->canonical_length = state_be64(bytes + HDR_CANONICAL_LENGTH);
  table->header_tick = state_be64(bytes + HDR_TICK);
  return true;
}

static bool state_verify_trailer(const uint8_t *bytes, uint64_t length,
                                 t64_state_import_report *report) {
  uint8_t digest[T64_DIGEST_SIZE];

  (void)state_sha256_span(bytes, length - T64_STATE_TRAILER_BYTES, digest);
  if (memcmp(digest, bytes + (size_t)(length - T64_STATE_TRAILER_BYTES),
             sizeof digest) != 0) {
    state_report_failure(report, T64_STATE_REASON_TRAILER_SHA256, 0u, 0u,
                         length - T64_STATE_TRAILER_BYTES);
    return false;
  }
  return true;
}

/* The importer registry: one row per state serialization version, so an
 * unregistered version reports STATE_VERSION. */
static bool state_version_registered(const t64_machine *machine,
                                     uint32_t version) {
  return version == STATE_VERSION_REGISTERED && t64_has_si_profile(machine) &&
         machine->profile.state_serialization_version == version;
}

static bool state_check_binding(const t64_machine *machine,
                                const uint8_t *bytes,
                                t64_state_import_report *report) {
  const char *build = t64_build_identity();
  uint32_t build_length = 0u;

  if (!state_version_registered(machine,
                                state_be32(bytes + HDR_STATE_VERSION))) {
    state_report_failure(report, T64_STATE_REASON_STATE_VERSION, 0u, 0u,
                         HDR_STATE_VERSION);
    return false;
  }
  if (memcmp(bytes + HDR_PROFILE, machine->profile_digest,
             T64_DIGEST_SIZE) != 0) {
    state_report_failure(report, T64_STATE_REASON_PROFILE_DIGEST, 0u, 0u,
                         HDR_PROFILE);
    return false;
  }
  if (memcmp(bytes + HDR_CONTENT, machine->content_digest, T64_DIGEST_SIZE) !=
      0) {
    state_report_failure(report, T64_STATE_REASON_CONTENT_DIGEST, 0u, 0u,
                         HDR_CONTENT);
    return false;
  }
  if (state_be64(bytes + HDR_CONTENT_LENGTH) != (uint64_t)machine->image_length) {
    state_report_failure(report, T64_STATE_REASON_CONTENT_LENGTH, 0u, 0u,
                         HDR_CONTENT_LENGTH);
    return false;
  }
  while (build_length < T64_STATE_BUILD_ID_BYTES &&
         build[build_length] != '\0') {
    build_length++;
  }
  /* The field holds the live build text with NUL padding, byte for byte. */
  if (build_length >= T64_STATE_BUILD_ID_BYTES ||
      memcmp(bytes + HDR_BUILD, build, build_length) != 0 ||
      bytes[HDR_BUILD + build_length] != 0u) {
    state_report_failure(report, T64_STATE_REASON_BUILD_ID, 0u, 0u, HDR_BUILD);
    return false;
  }
  return true;
}

/* The exact body length of a fixed-size section for this profile, or
 * UINT64_MAX for a section whose length depends on its content (the queue, the
 * RDP and VI block with retained packets, the audio block). */
static uint64_t state_fixed_length(const t64_machine *machine, uint32_t id) {
  switch (id) {
  case T64_STATE_SECTION_IDENTITY:
    return 168u;
  case T64_STATE_SECTION_MACHINE_CORE:
    return 32u;
  case T64_STATE_SECTION_RDRAM:
    return 4u + (uint64_t)machine->profile.rdram_bytes;
  case T64_STATE_SECTION_SP_MEMORY:
    return 8200u;
  case T64_STATE_SECTION_INTERRUPT_PI_SP:
    return 276u;
  case T64_STATE_SECTION_WARM_DEVICES:
    return 43724u;
  case T64_STATE_SECTION_SI_PIF:
    return 404u;
  case T64_STATE_SECTION_SAVE_MEDIA:
    return 20u + (uint64_t)t64_save_storage_bytes(machine->profile.save_kind);
  case T64_STATE_SECTION_OBSERVER_CONTINUITY:
    return T64_STATE_OBSERVER_BYTES;
  default:
    return UINT64_MAX;
  }
}

static bool state_section_id_known(const t64_machine *machine, uint32_t id) {
  if (id == T64_STATE_SECTION_SAVE_MEDIA) {
    return machine->profile.save_kind != 0u;
  }
  return (id >= T64_STATE_SECTION_IDENTITY && id <= T64_STATE_SECTION_SI_PIF) ||
         id == T64_STATE_SECTION_OBSERVER_CONTINUITY;
}

static bool state_parse_table(const t64_machine *machine,
                              const uint8_t *bytes, uint64_t length,
                              state_table *table,
                              t64_state_import_report *report) {
  uint64_t table_bytes;
  uint64_t table_end;
  uint64_t canonical_end;
  uint64_t tail;
  uint64_t next_marker;
  uint32_t index;
  uint32_t previous_id = 0u;
  uint32_t wanted;

  if (!state_mul_u64(T64_STATE_ENTRY_BYTES, table->count, &table_bytes) ||
      !state_add_u64(T64_STATE_HEADER_BYTES, table_bytes, &table_end) ||
      !state_add_u64(T64_STATE_OBSERVER_BYTES, T64_STATE_TRAILER_BYTES, &tail) ||
      table->canonical_offset != table_end ||
      !state_add_u64(table->canonical_offset, table->canonical_length,
                     &canonical_end) ||
      table->canonical_length < STATE_DOMAIN_BYTES ||
      !state_span_within(canonical_end, tail, length) ||
      canonical_end + tail != length) {
    state_report_failure(report, T64_STATE_REASON_SECTION_BOUNDS, 0u, 0u,
                         HDR_CANONICAL_OFFSET);
    return false;
  }
  if (memcmp(bytes + (size_t)table->canonical_offset, STATE_DOMAIN,
             STATE_DOMAIN_BYTES) != 0) {
    state_report_failure(report, T64_STATE_REASON_SECTION_BOUNDS, 0u, 0u,
                         table->canonical_offset);
    return false;
  }
  next_marker = table->canonical_offset + STATE_DOMAIN_BYTES;
  for (index = 0u; index < table->count; index++) {
    const uint8_t *entry =
        bytes + (size_t)(T64_STATE_HEADER_BYTES +
                         (uint64_t)T64_STATE_ENTRY_BYTES * index);
    const uint64_t entry_position =
        T64_STATE_HEADER_BYTES + (uint64_t)T64_STATE_ENTRY_BYTES * index;
    const uint32_t id = state_be32(entry);
    const uint32_t flags = state_be32(entry + 4);
    const uint64_t offset = state_be64(entry + 8);
    const uint64_t body_length = state_be64(entry + 16);
    const bool observer = id == T64_STATE_SECTION_OBSERVER_CONTINUITY;
    const uint64_t fixed = state_fixed_length(machine, id);
    uint8_t digest[T64_DIGEST_SIZE];

    if (!state_section_id_known(machine, id)) {
      state_report_failure(report, T64_STATE_REASON_SECTION_ID_UNKNOWN, id, 0u,
                           entry_position);
      return false;
    }
    if ((index > 0u && id <= previous_id) ||
        (observer && index + 1u != table->count)) {
      state_report_failure(report, T64_STATE_REASON_SECTION_ORDER, id, 0u,
                           entry_position);
      return false;
    }
    previous_id = id;
    if (flags != (observer ? T64_STATE_SECTION_FLAG_OUTSIDE_DIGEST : 0u)) {
      state_report_failure(report, T64_STATE_REASON_SECTION_FLAGS, id, 0u,
                           entry_position + 4u);
      return false;
    }
    if (observer) {
      if (offset != canonical_end ||
          !state_span_within(offset, body_length, length - T64_STATE_TRAILER_BYTES)) {
        state_report_failure(report, T64_STATE_REASON_SECTION_BOUNDS, id, 0u,
                             entry_position + 8u);
        return false;
      }
    } else {
      /* The bodies are contiguous: each marker (four bytes) follows the end of
       * the previous body, so an earlier offset overlaps and a later one leaves
       * a gap. */
      if (offset < next_marker + 4u) {
        state_report_failure(report, T64_STATE_REASON_SECTION_OVERLAP, id, 0u,
                             entry_position + 8u);
        return false;
      }
      if (offset != next_marker + 4u ||
          !state_span_within(offset, body_length, canonical_end) ||
          state_be32(bytes + (size_t)(offset - 4u)) != id) {
        state_report_failure(report, T64_STATE_REASON_SECTION_BOUNDS, id, 0u,
                             entry_position + 8u);
        return false;
      }
    }
    if (fixed != UINT64_MAX && body_length != fixed) {
      state_report_failure(report, T64_STATE_REASON_SECTION_LENGTH_EXPECTED, id,
                           0u, entry_position + 16u);
      return false;
    }
    (void)state_sha256_span(bytes + (size_t)offset, body_length, digest);
    if (memcmp(digest, entry + 24, sizeof digest) != 0) {
      state_report_failure(report, T64_STATE_REASON_SECTION_SHA256, id, 0u,
                           entry_position + 24u);
      return false;
    }
    table->sections[index].id = id;
    table->sections[index].offset = offset;
    table->sections[index].length = body_length;
    if (!observer) {
      next_marker = offset + body_length;
    }
  }
  if (next_marker != canonical_end) {
    state_report_failure(report, T64_STATE_REASON_SECTION_BOUNDS, 0u, 0u,
                         canonical_end);
    return false;
  }
  /* Every mandatory section of the profile is present. */
  for (wanted = T64_STATE_SECTION_IDENTITY;
       wanted <= T64_STATE_SECTION_OBSERVER_CONTINUITY; wanted++) {
    bool present = false;

    if (!state_section_id_known(machine, wanted)) {
      continue;
    }
    for (index = 0u; index < table->count; index++) {
      if (table->sections[index].id == wanted) {
        present = true;
      }
    }
    if (!present) {
      state_report_failure(report, T64_STATE_REASON_SECTION_MISSING, wanted, 0u,
                           0u);
      return false;
    }
  }
  return true;
}

/* The canonical digest recomputed from the blob: the domain bytes, then for each
 * canonical section its big-endian id and its body, exactly as
 * t64_digest_stream hashes them, against the declared digest of the header. */
static bool state_recompute_digest(const uint8_t *bytes, const state_table *table,
                                   t64_state_import_report *report) {
  t64_sha256_ctx context;
  uint8_t digest[T64_DIGEST_SIZE];
  uint32_t index;

  t64_sha256_init(&context);
  t64_sha256_update(&context, (const uint8_t *)STATE_DOMAIN,
                    STATE_DOMAIN_BYTES);
  for (index = 0u; index < table->count; index++) {
    const state_section *section = &table->sections[index];
    uint8_t marker[4];

    if (section->id == T64_STATE_SECTION_OBSERVER_CONTINUITY) {
      continue;
    }
    marker[0] = (uint8_t)(section->id >> 24);
    marker[1] = (uint8_t)(section->id >> 16);
    marker[2] = (uint8_t)(section->id >> 8);
    marker[3] = (uint8_t)section->id;
    t64_sha256_update(&context, marker, sizeof marker);
    t64_sha256_update(&context, bytes + (size_t)section->offset,
                      (size_t)section->length);
  }
  t64_sha256_final(&context, digest);
  if (memcmp(digest, bytes + HDR_DECLARED, sizeof digest) != 0) {
    state_report_failure(report, T64_STATE_REASON_DECLARED_DIGEST, 0u, 0u,
                         HDR_DECLARED);
    return false;
  }
  return true;
}

/* ---- the commit and the entry point ------------------------------------- */

/* The commit makes no host call and no allocation. Every state structure that
 * the container describes starts from zero and receives the blob values, so a
 * member the container does not carry (and the dirty state of a peer that ran
 * other work) cannot survive; the observers and the trace are reset by the
 * rules recorded in docs/spec/state-container.md. Profile, content, the ROM
 * bytes, the allocator, the storage descriptors and every pointer stay. */
static void state_commit(t64_machine *machine, const uint8_t *bytes,
                         uint64_t length, const state_table *table,
                         uint32_t flags) {
  (void)memset(machine->events, 0, sizeof machine->events);
  (void)memset(&machine->pi, 0, sizeof machine->pi);
  (void)memset(&machine->sp, 0, sizeof machine->sp);
  (void)memset(&machine->rsp, 0, sizeof machine->rsp);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  (void)memset(&machine->cache, 0, sizeof machine->cache);
  (void)memset(&machine->guest_devices, 0, sizeof machine->guest_devices);
  (void)memset(&machine->output, 0, sizeof machine->output);
  (void)memset(&machine->rdp, 0, sizeof machine->rdp);
  (void)memset(&machine->vi, 0, sizeof machine->vi);
  (void)memset(&machine->ai, 0, sizeof machine->ai);
  (void)memset(&machine->si, 0, sizeof machine->si);
  if (machine->rdp_memory_coverage != NULL) {
    (void)memset(machine->rdp_memory_coverage, 0,
                 T64_RDP_MEMORY_COVERAGE_BYTES(machine->profile.rdram_bytes));
  }
  if (machine->rdp_depth_hidden != NULL) {
    (void)memset(machine->rdp_depth_hidden, 0,
                 T64_RDP_DEPTH_HIDDEN_BYTES(machine->profile.rdram_bytes));
  }
  if (machine->video_packets != NULL) {
    (void)memset(machine->video_packets, 0, T64_VIDEO_QUEUE_STORAGE_BYTES);
  }
  if (machine->audio_packets != NULL) {
    (void)memset(machine->audio_packets, 0, T64_AUDIO_QUEUE_STORAGE_BYTES);
  }
  (void)state_run_sections(machine, bytes, length, table, flags,
                           STATE_MODE_APPLY, NULL);
  /* The trace ring and its drop observation clear; enablement stays. The ring
   * indices and the depth image address are never read from the blob; they are
   * named here so the field-use audit (docs/spec/state-field-rules.txt) maps
   * them to this commit. */
  t64_trace_reset(machine);
  machine->trace_head = 0u;
  machine->trace_count = 0u;
  (void)memset(machine->trace_records, 0, sizeof machine->trace_records);
  machine->rdp.depth_image_address = 0u;
  machine->rsp_observed_pc = 0u;
  machine->rsp_observed_opcode = 0u;
  machine->rsp_observed_second_pc = 0u;
  machine->rsp_observed_second_opcode = 0u;
  machine->rsp_observed_tick = 0u;
  machine->rsp_observed_ready_tick = 0u;
  machine->rsp_observed_dma_cursor = 0u;
  machine->rsp_observed_stall_reason = 0u;
  machine->rsp_observed_refusal_reason = 0u;
  machine->rsp_observed_dma_action = 0u;
  machine->rsp_observed_outcome = 0u;
  machine->rsp_observation_valid = false;
  /* The generation counts successful imports of save content (plan 09-05): a
   * state import replaces the save bytes exactly as a save import does, so it
   * adds one, saturating, and only on a profile with a save device. It never
   * travels in the container, stays outside the digest and no refusal reaches
   * this line. The last write of the commit. */
  if (machine->profile.save_kind != 0u &&
      machine->save.write_generation != UINT64_MAX) {
    machine->save.write_generation++;
  }
}

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

/* One attempt: every pass in the locked order, ending in the commit. The
 * caller clears the scratch on the way out, so no exit path leaves it dirty. */
static t64_status state_import_attempt(
    t64_machine *machine, const uint8_t *bytes, size_t length, uint32_t flags,
    const t64_state_import_report *report_out,
    t64_state_import_report *report, bool *report_usable) {
  state_table table;

  if (!state_check_arguments(machine, bytes, length, flags, report_out, report,
                             report_usable)) {
    return report->status;
  }
  (void)memset(&machine->state_summary, 0, sizeof machine->state_summary);
  if (!state_parse_header(bytes, (uint64_t)length, &table, report) ||
      !state_verify_trailer(bytes, (uint64_t)length, report) ||
      !state_check_binding(machine, bytes, report) ||
      !state_parse_table(machine, bytes, (uint64_t)length, &table, report)) {
    return report->status;
  }
  machine->state_summary.facts.tick = table.header_tick;
  if (!state_run_sections(machine, bytes, (uint64_t)length, &table, flags,
                          STATE_MODE_VALIDATE, report) ||
      !state_check_devices(machine, report) ||
      !state_recompute_digest(bytes, &table, report)) {
    return report->status;
  }
  state_commit(machine, bytes, (uint64_t)length, &table, flags);
  return T64_STATUS_OK;
}

t64_status t64_machine_state_import(t64_machine *machine,
                                    const uint8_t *bytes, size_t length,
                                    uint32_t flags,
                                    t64_state_import_report *report_out) {
  t64_state_import_report report;
  bool report_usable = report_out != NULL;
  t64_status status;

  (void)memset(&report, 0, sizeof report);
  status = state_import_attempt(machine, bytes, length, flags, report_out,
                                &report, &report_usable);
  /* The single exit: the scratch is clear on every path. */
  if (machine != NULL) {
    (void)memset(&machine->state_summary, 0, sizeof machine->state_summary);
  }
  if (report_usable && report_out != NULL) {
    *report_out = report;
  }
  return status;
}
