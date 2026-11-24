/*
 * Phase 10 digest v26 walker tests (plan 10-09).
 *
 * v26-completeness: on each of the four registered descriptor 13 ids, every
 * member of t64_rdp_state, every byte of the memory coverage table, strided
 * TMEM bytes and the first, a middle and the last byte of the hidden depth
 * table is flipped one bit at a time and must change the canonical stream; the
 * five depth members (depth_image_address, depth_image_valid, primitive_z,
 * primitive_dz, primitive_depth_valid) are plain rows, not exclusions. Padding
 * and packet words at or after the count must not change it. One flip is tied
 * to the SHA-256 digest. The test was written and run before RDP depth was
 * activated on the family: its first failure then named a depth member.
 *
 * v26-strip-differential: with the markers, the identity spans and every
 * new-span class removed, a v26 stream equals the v25 stream of the
 * corresponding si-v1 id for machines built by the same operations (idle,
 * mid-render, mid-scan-out, after an SI poll, after a save write), so the v25
 * framing is unchanged.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_sha256.h"
#include "t64_test.h"

#define IPL3_ASSET_BYTES 4096u
#define V1_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define V2_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2"
#define V26_DOMAIN "t64.state.si.26"
#define V26_DOMAIN_LENGTH 15u
#define SPAN_CAPACITY 4096u
#define MARKER_COUNT_FULL 10u
/* The three save ids add section 11 (save media) after section 10. */
#define MARKER_COUNT_SAVE 11u
#define FAMILY_COUNT 4u

/* Each row: the descriptor 12 id and the descriptor 13 id built the same way. */
typedef struct {
  const char *v25_id;
  const char *v26_id;
  size_t marker_count;
} id_pair;

static const id_pair pairs[FAMILY_COUNT] = {
    {V1_STEM, V2_STEM, MARKER_COUNT_FULL},
    {V1_STEM "-e4k", V2_STEM "-e4k", MARKER_COUNT_SAVE},
    {V1_STEM "-e16k", V2_STEM "-e16k", MARKER_COUNT_SAVE},
    {V1_STEM "-s32k", V2_STEM "-s32k", MARKER_COUNT_SAVE},
};

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) return false;
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static const t64_host_allocator allocator = {test_allocate, test_release,
                                             NULL};

static bool read_openipl3_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0' || bytes == NULL) return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 && length == IPL3_ASSET_BYTES &&
         extra == EOF;
}


/* Create the machine for an id and admit the cleared openIPL3 asset. */
static t64_machine *create_admitted(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  t64_status status;

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  if (!read_openipl3_asset(asset)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return NULL;
  }
  status = t64_machine_admit_rom(machine, asset, sizeof asset);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

/* ---- stream capture and the strip-and-compare comparator --------------- */

typedef struct {
  uint8_t *bytes;
  size_t length;
  t64_digest_span *spans;
  size_t span_count;
} stream;

/* Distinct comparator result kinds; a control must reach its own kind. */
typedef enum {
  CMP_EQUAL = 0,
  CMP_LENGTH_MISMATCH = 1,
  CMP_MARKER_ORDER = 2,
  CMP_BODY_MISMATCH = 3,
  CMP_IDENTITY_MISMATCH = 4
} cmp_kind;

typedef struct {
  cmp_kind kind;
  size_t offset;      /* first differing offset in the stripped streams */
  size_t old_length;  /* stripped length of the older stream */
  size_t new_length;  /* stripped length of the newer stream */
} cmp_result;

static void free_stream(stream *captured) {
  free(captured->bytes);
  free(captured->spans);
  (void)memset(captured, 0, sizeof *captured);
}

/* Size query first, then the real stream into a buffer sink with spans. */
static bool capture_stream(const t64_machine *machine, stream *out) {
  t64_digest_sink sink;
  size_t length;

  (void)memset(out, 0, sizeof *out);
  (void)memset(&sink, 0, sizeof sink);
  T64_CHECK_INT(t64_digest_stream(machine, &sink), T64_STATUS_OK);
  length = sink.offset;
  T64_CHECK(length > 0u);
  out->bytes = (uint8_t *)malloc(length);
  out->spans = (t64_digest_span *)calloc(SPAN_CAPACITY, sizeof *out->spans);
  if (out->bytes == NULL || out->spans == NULL || length == 0u) {
    T64_CHECK(false);
    free_stream(out);
    return false;
  }
  (void)memset(&sink, 0, sizeof sink);
  sink.buffer = out->bytes;
  sink.capacity = length;
  sink.spans = out->spans;
  sink.span_capacity = SPAN_CAPACITY;
  T64_CHECK_INT(t64_digest_stream(machine, &sink), T64_STATUS_OK);
  T64_CHECK_INT(sink.offset, length);
  T64_CHECK(!sink.overflow);
  out->length = length;
  out->span_count = sink.span_count;
  return sink.offset == length && !sink.overflow;
}

/* Copy every body byte (outside all non-body spans); NULL out sizes only. */
static size_t strip_stream(const stream *captured, uint8_t *out) {
  size_t position = 0u;
  size_t written = 0u;
  size_t i;

  for (i = 0u; i <= captured->span_count; i++) {
    const size_t end =
        i < captured->span_count ? captured->spans[i].offset : captured->length;

    if (end > position) {
      if (out != NULL) {
        (void)memcpy(out + written, captured->bytes + position,
                     end - position);
      }
      written += end - position;
    }
    if (i < captured->span_count) {
      position = captured->spans[i].offset + captured->spans[i].length;
    }
  }
  return written;
}

static uint32_t read_u32(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

/*
 * The identity-class spans of a v26 stream are exactly two, as in v25: the
 * domain at the start, and the serialization word with the profile digest
 * right after the first marker. Every other non-marker span is new-class.
 */
static bool identity_spans_are_exact(const stream *captured,
                                     size_t *offset_out) {
  size_t seen = 0u;
  size_t i;

  *offset_out = 0u;
  for (i = 0u; i < captured->span_count; i++) {
    const t64_digest_span *span = &captured->spans[i];

    if (span->class == T64_DIGEST_CLASS_MARKER) continue;
    if (span->class == T64_DIGEST_CLASS_IDENTITY && seen == 0u &&
        span->offset == 0u && span->length == V26_DOMAIN_LENGTH) {
      seen = 1u;
    } else if (span->class == T64_DIGEST_CLASS_IDENTITY && seen == 1u &&
               span->offset == V26_DOMAIN_LENGTH + 4u &&
               span->length == 4u + T64_DIGEST_SIZE) {
      seen = 2u;
    } else if (span->class == T64_DIGEST_CLASS_NEW && seen == 2u) {
      continue;
    } else {
      *offset_out = span->offset;
      return false;
    }
  }
  if (seen != 2u) {
    *offset_out = captured->length;
    return false;
  }
  return true;
}

/* Marker spans carry ascending section ids 1..count, four bytes each. */
static bool markers_are_ordered(const stream *captured, size_t count,
                                size_t *offset_out) {
  size_t seen = 0u;
  size_t i;

  *offset_out = 0u;
  for (i = 0u; i < captured->span_count; i++) {
    const t64_digest_span *span = &captured->spans[i];

    if (span->class != T64_DIGEST_CLASS_MARKER) continue;
    if (span->length != 4u ||
        read_u32(captured->bytes + span->offset) != (uint32_t)(seen + 1u)) {
      *offset_out = span->offset;
      return false;
    }
    seen++;
  }
  if (seen != count) {
    *offset_out = captured->length;
    return false;
  }
  return true;
}

/*
 * Compare the v25 stream with the v26 stream after removing framing: the
 * stripped length first, then the marker order of both streams, then the
 * stripped body bytes, then the v26 domain text and the exact identity spans
 * of the v26 stream.
 */
static cmp_result compare_stripped(const stream *older, const stream *newer,
                                   size_t marker_count) {
  cmp_result result = {CMP_EQUAL, 0u, 0u, 0u};
  uint8_t *old_body;
  uint8_t *new_body;
  size_t common;
  size_t i;

  result.old_length = strip_stream(older, NULL);
  result.new_length = strip_stream(newer, NULL);
  old_body = (uint8_t *)malloc(result.old_length > 0u ? result.old_length : 1u);
  new_body = (uint8_t *)malloc(result.new_length > 0u ? result.new_length : 1u);
  if (old_body == NULL || new_body == NULL) {
    T64_CHECK(false);
    free(old_body);
    free(new_body);
    result.kind = CMP_LENGTH_MISMATCH;
    return result;
  }
  (void)strip_stream(older, old_body);
  (void)strip_stream(newer, new_body);
  common = result.old_length < result.new_length ? result.old_length
                                                  : result.new_length;
  for (i = 0u; i < common && old_body[i] == new_body[i]; i++) {
  }
  result.offset = i;
  if (result.old_length != result.new_length) {
    result.kind = CMP_LENGTH_MISMATCH;
  } else if (!markers_are_ordered(older, marker_count, &result.offset) ||
             !markers_are_ordered(newer, marker_count, &result.offset)) {
    result.kind = CMP_MARKER_ORDER;
  } else if (i != common) {
    result.kind = CMP_BODY_MISMATCH;
  } else if (newer->length < V26_DOMAIN_LENGTH ||
             memcmp(newer->bytes, V26_DOMAIN, V26_DOMAIN_LENGTH) != 0) {
    result.kind = CMP_BODY_MISMATCH;
    result.offset = 0u;
  } else if (!identity_spans_are_exact(newer, &result.offset)) {
    result.kind = CMP_IDENTITY_MISMATCH;
  }
  free(old_body);
  free(new_body);
  return result;
}

/* ---- machine states built identically on both profile ids -------------- */

typedef t64_machine *(*state_builder)(const char *profile_id);

static void install_spin_loop(t64_machine *machine) {
  machine->ram[UINT32_C(0x1000)] = 0x08u;
  machine->ram[UINT32_C(0x1001)] = 0x00u;
  machine->ram[UINT32_C(0x1002)] = 0x04u;
  machine->ram[UINT32_C(0x1003)] = 0x00u;
  (void)memset(machine->ram + UINT32_C(0x1004), 0, 4u);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
}

static bool advance_to(t64_machine *machine, uint64_t tick) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
  t64_stop_result stop = {0u, 0u};
  const t64_status status = t64_machine_advance(machine, &request, &stop);

  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
  return status == T64_STATUS_OK && stop.reason == T64_STOP_REASON_BOUND;
}

/* Idle: admitted, a guest spin loop retiring in RDRAM, tick 64. */
static t64_machine *build_idle(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id);

  if (machine == NULL) return NULL;
  install_spin_loop(machine);
  T64_CHECK(advance_to(machine, 64u));
  T64_CHECK_INT(machine->tick, 64u);
  T64_CHECK(machine->cpu.retired_instructions > 0u);
  return machine;
}

#define VI_BASE UINT32_C(0x04400000)
#define VI_ORIGIN UINT32_C(0x3000)

static void write_vi_register(t64_machine *machine, uint32_t offset,
                              uint32_t value) {
  T64_CHECK_INT(t64_machine_bus_write(machine, VI_BASE + offset, 4u, value),
                T64_STATUS_OK);
}

/* A VI field with authored 5551 source pixels: one retained video packet. */
static t64_machine *build_vi_frame(const char *profile_id) {
  static const uint16_t source[4] = {UINT16_C(0xf801), UINT16_C(0x07c1),
                                     UINT16_C(0x003f), UINT16_C(0xffff)};
  t64_machine *machine = create_admitted(profile_id);
  size_t i;

  if (machine == NULL) return NULL;
  for (i = 0u; i < sizeof source / sizeof source[0]; i++) {
    const uint32_t address = VI_ORIGIN + (uint32_t)(i * 2u);

    T64_CHECK_INT(t64_machine_bus_write(machine, address, 2u, source[i]),
                  T64_STATUS_OK);
    t64_rdp_memory_coverage_write(machine, address, 2u, 8u);
  }
  write_vi_register(machine, UINT32_C(0x04), VI_ORIGIN);
  write_vi_register(machine, UINT32_C(0x08), UINT32_C(4));
  write_vi_register(machine, UINT32_C(0x18), UINT32_C(8));
  write_vi_register(machine, UINT32_C(0x1c), UINT32_C(15));
  write_vi_register(machine, UINT32_C(0x24), UINT32_C(4));
  write_vi_register(machine, UINT32_C(0x28), UINT32_C(2));
  write_vi_register(machine, UINT32_C(0x30), UINT32_C(0x400));
  write_vi_register(machine, UINT32_C(0x34), UINT32_C(0x400));
  write_vi_register(machine, UINT32_C(0x00), UINT32_C(2));
  install_spin_loop(machine);
  for (i = 0u; i < 512u && machine->vi.packet_count == 0u; i++) {
    size_t q;
    uint64_t tick = UINT64_MAX;

    for (q = 0u; q < machine->queue_count; q++) {
      const size_t index = (machine->queue_head + q) % T64_EVENT_QUEUE_CAPACITY;
      const t64_event *event = &machine->events[index];

      if (event->device == T64_VI_DEVICE && event->kind == T64_VI_EVENT_FIELD &&
          event->token == machine->vi.event_generation) {
        tick = event->tick;
        break;
      }
    }
    if (tick == UINT64_MAX || !advance_to(machine, tick)) break;
  }
  /* Reached: a retained packet exists and carries its identity strings. */
  T64_CHECK(machine->vi.packet_count >= 1u);
  T64_CHECK(machine->vi.packet_lengths[machine->vi.packet_head] > 0u);
  T64_CHECK(machine->vi.packet_info[machine->vi.packet_head].profile_id[0] !=
            '\0');
  return machine;
}

#define DP_START_REGISTER UINT32_C(0x04100000)
#define DP_END_REGISTER UINT32_C(0x04100004)
#define SI_DRAM_ADDR UINT32_C(0x04800000)
#define SI_RD64B UINT32_C(0x04800004)
#define SI_WR64B UINT32_C(0x04800010)
#define SI_STATUS UINT32_C(0x04800018)
#define PIF_ADDRESS UINT32_C(0x1FC007C0)
#define SI_STATUS_INTERRUPT UINT32_C(0x1000)

/* Run exactly one queued event; false when the queue is empty. */
static bool step_one(t64_machine *machine) {
  t64_event event;

  if (machine->queue_count == 0u) return false;
  event = machine->events[machine->queue_head];
  machine->queue_head = (machine->queue_head + 1u) % T64_EVENT_QUEUE_CAPACITY;
  machine->queue_count--;
  machine->tick = event.tick;
  T64_CHECK(event.handler != NULL);
  if (event.handler != NULL) {
    T64_CHECK_INT(event.handler(machine, &event), T64_STATUS_OK);
  }
  return true;
}

/* Mid-render: a fill rectangle of the RDP is fetched and applied, so the
 * render is in flight (idiom of core/tests/test_rdp.c, with the boot CPU
 * event dropped so only the RDP events remain). */
static t64_machine *build_mid_render(const char *profile_id) {
  static const uint8_t fill_rectangle[8] = {0x36u, 0x00u, 0xc0u, 0x08u,
                                            0x00u, 0x00u, 0x00u, 0x00u};
  t64_machine *machine = create_admitted(profile_id);
  size_t step;

  if (machine == NULL) return NULL;
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  machine->rdp.color_image_address = UINT32_C(0x3000);
  machine->rdp.color_image_width = 4u;
  machine->rdp.color_image_format = 0u;
  machine->rdp.color_image_size = 2u;
  machine->rdp.color_image_valid = true;
  machine->rdp.scissor_xh = 0u;
  machine->rdp.scissor_yh = 0u;
  machine->rdp.scissor_xl = UINT32_C(12);
  machine->rdp.scissor_yl = UINT32_C(8);
  machine->rdp.scissor_valid = true;
  machine->rdp.other_modes = UINT64_C(0x0030000000000000);
  machine->rdp.other_modes_valid = true;
  machine->rdp.fill_color = UINT32_C(0xf80107c1);
  machine->rdp.fill_color_valid = true;
  (void)memset(machine->ram + UINT32_C(0x3000), 0xcc, 24u);
  (void)memcpy(machine->ram + UINT32_C(0x100), fill_rectangle,
               sizeof fill_rectangle);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START_REGISTER, 4u,
                                      UINT32_C(0x100)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END_REGISTER, 4u,
                                      UINT32_C(0x108)),
                T64_STATUS_OK);
  for (step = 0u; step < 8u && !machine->rdp.render_active; step++) {
    if (!step_one(machine)) break;
  }
  /* Reached: a fill render is active and the device is busy. */
  T64_CHECK(machine->rdp.render_active);
  T64_CHECK(machine->rdp.busy);
  T64_CHECK(!machine->rdp.render_triangle);
  return machine;
}

/* After an SI poll: a joybus frame polling two channels is written to the PIF
 * (WR64B), then read back with one controller state queued (RD64B), and both
 * DMAs have completed with the interrupt raised. */
static t64_machine *build_after_si_poll(const char *profile_id) {
  static const uint8_t poll_frame[64] = {
      0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
      0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFE, [63] = 0x01};
  t64_machine *machine = create_admitted(profile_id);
  uint32_t status = 0u;
  uint64_t tick;

  if (machine == NULL) return NULL;
  install_spin_loop(machine);
  (void)memcpy(machine->ram + UINT32_C(0x3800), poll_frame, sizeof poll_frame);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, 0x3800u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_WR64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK(advance_to(machine, 2500u));
  T64_CHECK_INT(t64_machine_bus_read(machine, SI_STATUS, 4u, &status),
                T64_STATUS_OK);
  T64_CHECK_INT(status, SI_STATUS_INTERRUPT);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, 0x3900u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_queue_input(machine, 4000u, 0u, 0x8000u, 1, -1),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_RD64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  /* The spin loop retires about one instruction per tick and one advance call
   * is bounded to 4096 events, so the completion is reached in short steps. */
  for (tick = 4500u; tick <= 40000u; tick += 2000u) {
    if (!advance_to(machine, tick)) break;
  }
  T64_CHECK_INT(machine->tick, 38500u);
  T64_CHECK_INT(t64_machine_bus_read(machine, SI_STATUS, 4u, &status),
                T64_STATUS_OK);
  T64_CHECK_INT(status, SI_STATUS_INTERRUPT);
  T64_CHECK(machine->si.table.parsed);
  T64_CHECK_INT(machine->ram[UINT32_C(0x3903)], 0x80u);
  return machine;
}

/* After a save write: an EEPROM write executed through the device entry point,
 * so the busy window is open and the write is in the save bytes. */
static t64_machine *build_after_save_write(const char *profile_id) {
  static const uint8_t tx[10] = {0x05, 0x01, 0x11, 0x12, 0x13,
                                 0x14, 0x15, 0x16, 0x17, 0x18};
  uint8_t rx[1] = {0xFF};
  t64_machine *machine = build_idle(profile_id);

  if (machine == NULL) return NULL;
  T64_CHECK_INT(t64_save_channel4_execute(machine, machine->tick, tx, 10u, rx,
                                          1u),
                0u);
  T64_CHECK(machine->save.busy_until > machine->tick);
  T64_CHECK_INT(machine->save.bytes[8], 0x11u);
  return machine;
}

static void report_comparison(const char *label, const cmp_result *result) {
  if (result->kind != CMP_EQUAL) {
    fprintf(stderr,
            "differential %s: kind=%d old_stripped=%zu new_stripped=%zu "
            "first_differing_offset=%zu\n",
            label, (int)result->kind, result->old_length, result->new_length,
            result->offset);
  }
}

/* The same builder on the descriptor 12 id and the descriptor 13 id; the
 * stripped streams must be equal. */
static void differential_for_state(const char *label, const id_pair *pair,
                                   state_builder build) {
  t64_machine *older = build(pair->v25_id);
  t64_machine *newer = build(pair->v26_id);
  stream old_stream;
  stream new_stream;

  T64_CHECK(older != NULL && newer != NULL);
  if (older != NULL && newer != NULL &&
      capture_stream(older, &old_stream)) {
    if (capture_stream(newer, &new_stream)) {
      const cmp_result result =
          compare_stripped(&old_stream, &new_stream, pair->marker_count);

      report_comparison(label, &result);
      T64_CHECK_INT(result.kind, CMP_EQUAL);
      free_stream(&new_stream);
    }
    free_stream(&old_stream);
  }
  if (older != NULL) t64_machine_destroy(older);
  if (newer != NULL) t64_machine_destroy(newer);
}

/* Wrong-value control: the same pair, but one RDRAM byte of the v26 machine is
 * changed after the build, so the comparison must report a body mismatch at
 * that byte and not any other kind. */
static void check_differential_detects_body_change(void) {
  t64_machine *older = build_idle(pairs[0].v25_id);
  t64_machine *newer = build_idle(pairs[0].v26_id);
  stream old_stream;
  stream new_stream;

  T64_CHECK(older != NULL && newer != NULL);
  if (older != NULL && newer != NULL) {
    newer->ram[UINT32_C(0x2000)] ^= 0x01u;
    if (capture_stream(older, &old_stream)) {
      if (capture_stream(newer, &new_stream)) {
        const cmp_result result =
            compare_stripped(&old_stream, &new_stream, pairs[0].marker_count);

        T64_CHECK_INT(result.kind, CMP_BODY_MISMATCH);
        T64_CHECK_INT(result.old_length, result.new_length);
        free_stream(&new_stream);
      }
      free_stream(&old_stream);
    }
  }
  if (older != NULL) t64_machine_destroy(older);
  if (newer != NULL) t64_machine_destroy(newer);
}

/* Offset of the marker of section `id` (markers are the MARKER spans, in
 * order, so section ids ascend from 1). */
static bool marker_offset(const stream *captured, uint32_t id,
                          size_t *offset_out) {
  uint32_t seen = 0u;
  size_t i;

  for (i = 0u; i < captured->span_count; i++) {
    if (captured->spans[i].class != T64_DIGEST_CLASS_MARKER) continue;
    seen++;
    if (seen == id) {
      *offset_out = captured->spans[i].offset;
      return true;
    }
  }
  return false;
}

/*
 * Layout of the two v26 depth blocks inside section 8, derived from the
 * documented member order and sizes rather than from the walker: the five
 * depth scalars (20 bytes, new class) sit immediately before the 4-byte
 * work_policy_version word, and the section 8 tail is the 88-byte combiner
 * block, the 4-byte coverage length and the coverage table, then the 4-byte
 * hidden length and the hidden depth table, ending at the marker of section 9.
 */
static void check_v26_depth_layout(const char *profile_id) {
  t64_machine *machine = build_idle(profile_id);
  stream captured;
  size_t marker8 = 0u;
  size_t marker9 = 0u;
  size_t scalar_spans = 0u;
  size_t tail_spans = 0u;
  size_t i;
  const size_t coverage_bytes =
      T64_RDP_MEMORY_COVERAGE_BYTES(UINT32_C(4) * 1024u * 1024u);
  const size_t hidden_bytes =
      T64_RDP_DEPTH_HIDDEN_BYTES(UINT32_C(4) * 1024u * 1024u);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->rdp.depth_image_address = UINT32_C(0x00006000);
  machine->rdp.depth_image_valid = true;
  machine->rdp.primitive_z = UINT32_C(0x1357);
  machine->rdp.primitive_dz = 9u;
  machine->rdp.primitive_depth_valid = true;
  machine->rdp_depth_hidden[0] = 0x83u;
  if (capture_stream(machine, &captured)) {
    T64_CHECK(marker_offset(&captured, 8u, &marker8));
    T64_CHECK(marker_offset(&captured, 9u, &marker9));
    for (i = 0u; i < captured.span_count; i++) {
      const t64_digest_span *span = &captured.spans[i];

      if (span->class != T64_DIGEST_CLASS_NEW || span->offset <= marker8 ||
          span->offset >= marker9) {
        continue;
      }
      if (span->offset + span->length == marker9) {
        const size_t tail_end = span->offset + span->length;
        const size_t hidden_at = tail_end - hidden_bytes;

        tail_spans++;
        T64_CHECK_INT(span->length,
                      88u + 4u + coverage_bytes + 4u + hidden_bytes);
        T64_CHECK_INT(read_u32(captured.bytes + hidden_at - 4u), hidden_bytes);
        T64_CHECK_INT(read_u32(captured.bytes + hidden_at - 4u - coverage_bytes -
                               4u),
                      coverage_bytes);
        T64_CHECK_INT(captured.bytes[hidden_at], 0x83u);
      } else {
        scalar_spans++;
        T64_CHECK_INT(span->length, 20u);
        T64_CHECK_INT(read_u32(captured.bytes + span->offset),
                      UINT32_C(0x00006000));
        T64_CHECK_INT(read_u32(captured.bytes + span->offset + 4u), 1u);
        T64_CHECK_INT(read_u32(captured.bytes + span->offset + 8u),
                      UINT32_C(0x1357));
        T64_CHECK_INT(read_u32(captured.bytes + span->offset + 12u), 9u);
        T64_CHECK_INT(read_u32(captured.bytes + span->offset + 16u), 1u);
        T64_CHECK_INT(read_u32(captured.bytes + span->offset + 20u),
                      machine->rdp.work_policy_version);
      }
    }
    T64_CHECK_INT(scalar_spans, 1u);
    T64_CHECK_INT(tail_spans, 1u);
    free_stream(&captured);
  }
  t64_machine_destroy(machine);
}

static void case_v26_strip_differential(void) {
  size_t i;

  for (i = 0u; i < FAMILY_COUNT; i++) {
    check_v26_depth_layout(pairs[i].v26_id);
  }
  check_differential_detects_body_change();
  for (i = 0u; i < FAMILY_COUNT; i++) {
    differential_for_state("idle", &pairs[i], build_idle);
  }
  differential_for_state("mid-render", &pairs[0], build_mid_render);
  differential_for_state("mid-scan-out", &pairs[0], build_vi_frame);
  differential_for_state("after-si-poll", &pairs[0], build_after_si_poll);
  /* the two EEPROM ids: the save write opens the busy window */
  differential_for_state("after-save-write e4k", &pairs[1],
                         build_after_save_write);
  differential_for_state("after-save-write e16k", &pairs[2],
                         build_after_save_write);
}

/* ---- completeness: every member of the RDP block and the tables ----- */

typedef struct {
  const char *name;
  size_t offset;
  size_t size;
  const char *excluded; /* NULL: the member is in the canonical stream */
} rdp_member;

#define RDP_OBSERVATION_REASON                                               \
  "test observation state written by the row handler, read only by the "     \
  "triangle rasterizer test"
#define M(name) {#name, offsetof(t64_rdp_state, name),                      \
                 sizeof(((t64_rdp_state *)0)->name), NULL}
#define MX(name, reason) {#name, offsetof(t64_rdp_state, name),             \
                          sizeof(((t64_rdp_state *)0)->name), reason}
#define MI(name, index) {#name "[" #index "]",                              \
                         offsetof(t64_rdp_state, name) +                    \
                         (index) * sizeof(((t64_rdp_state *)0)->name[0]),   \
                         sizeof(((t64_rdp_state *)0)->name[0]), NULL}
#define MT(index, field) {"tiles[" #index "]." #field,                      \
                          offsetof(t64_rdp_state, tiles) +                  \
                          (index) * sizeof(t64_rdp_tile) +                  \
                          offsetof(t64_rdp_tile, field),                    \
                          sizeof(((t64_rdp_tile *)0)->field), NULL}
#define MTILE(index)                                                        \
  MT(index, format), MT(index, size), MT(index, line_words),                \
  MT(index, tmem_word), MT(index, palette), MT(index, mask_s),              \
  MT(index, mask_t), MT(index, shift_s), MT(index, shift_t),                \
  MT(index, uls), MT(index, ult), MT(index, lrs), MT(index, lrt),           \
  MT(index, clamp_s), MT(index, mirror_s), MT(index, clamp_t),              \
  MT(index, mirror_t), MT(index, valid), MT(index, size_valid)

static const rdp_member rdp_members[] = {
    M(dpc_start), M(dpc_end), M(dpc_current), M(dpc_status),
    M(dpc_clock_counter), M(dpc_buffer_busy_counter),
    M(dpc_pipe_busy_counter), M(dpc_tmem_counter),
    M(pending_command_address), M(pending_command_byte),
    M(pending_opcode_field), M(pending_command_word), M(fence_generation),
    M(color_image_address), M(depth_image_address),
    M(color_image_width), M(color_image_format), M(color_image_size),
    M(scissor_xh), M(scissor_yh), M(scissor_xl), M(scissor_yl),
    M(other_modes), M(fill_color),
    M(primitive_z), M(primitive_dz),
    M(texture_image_address), M(texture_image_width),
    M(texture_image_format), M(texture_image_size), M(texture_image_valid),
    M(tlut_format), M(render_kind), M(render_texture_tile),
    M(render_texture_s), M(render_texture_t), M(render_texture_dsdx),
    M(render_texture_dtdy), M(copy_count),
    MI(convert_coefficients, 0), MI(convert_coefficients, 1),
    MI(convert_coefficients, 2), MI(convert_coefficients, 3),
    MI(convert_coefficients, 4), MI(convert_coefficients, 5),
    M(pending_texrect_word),
    /* the four packet words the count of 4 makes state; the words at or after
     * the count are not state (the reader zeroes them) and the sweep flips
     * them as inert bytes instead */
    MI(pending_triangle_words, 0), MI(pending_triangle_words, 1),
    MI(pending_triangle_words, 2), MI(pending_triangle_words, 3),
    M(pending_triangle_word_count), M(primitive_color),
    M(primitive_lod_fraction), M(environment_color), M(fog_color),
    M(blend_color), M(combine_mode),
    MI(key_center, 0), MI(key_center, 1), MI(key_center, 2),
    MI(key_scale, 0), MI(key_scale, 1), MI(key_scale, 2),
    MI(key_width, 0), MI(key_width, 1), MI(key_width, 2),
    M(triangle_count),
    MX(triangle_last_coverage_x, RDP_OBSERVATION_REASON),
    MX(triangle_last_coverage_y, RDP_OBSERVATION_REASON),
    MX(triangle_last_coverage_mask, RDP_OBSERVATION_REASON),
    {"tmem[0]", offsetof(t64_rdp_state, tmem), 1u, NULL},
    {"tmem[4095]", offsetof(t64_rdp_state, tmem) + 4095u, 1u, NULL},
    MTILE(0), MTILE(1), MTILE(2), MTILE(3), MTILE(4), MTILE(5), MTILE(6),
    MTILE(7),
    M(work_policy_version), M(random_state), M(refusal_reason),
    M(command_count), M(fill_count), M(fullsync_count), M(last_original_word),
    M(last_command_byte), M(last_opcode_field), M(render_x_begin),
    M(render_x_end), M(render_y_begin), M(render_y_end), M(render_y),
    M(color_image_valid), M(depth_image_valid),
    M(primitive_depth_valid), M(scissor_valid),
    M(other_modes_valid), M(fill_color_valid), M(tlut_valid),
    M(render_active), M(fence_pending), M(busy), M(dpc_xbus), M(dpc_freeze),
    M(dpc_flush), M(dpc_start_valid), M(dpc_end_valid), M(command_pending),
    M(texrect_pending), M(render_texture_flip), M(primitive_color_valid),
    M(environment_color_valid), M(fog_color_valid), M(blend_color_valid),
    M(key_center_valid), M(key_scale_valid), M(key_width_valid),
    M(combine_mode_valid), M(render_triangle), M(shade_triangle),
};

/* The structure has no member the table does not name (a new member changes
 * the size, or lands in padding that the inert sweep below flips). */
#define RDP_STATE_SIZE 5144u
_Static_assert(sizeof(t64_rdp_state) == RDP_STATE_SIZE,
               "t64_rdp_state changed: add the new member to rdp_members");

static bool capture_into(const t64_machine *machine, uint8_t *buffer,
                         size_t capacity, size_t *length_out) {
  t64_digest_sink sink;

  (void)memset(&sink, 0, sizeof sink);
  sink.buffer = buffer;
  sink.capacity = capacity;
  if (t64_digest_stream(machine, &sink) != T64_STATUS_OK || sink.overflow) {
    return false;
  }
  *length_out = sink.offset;
  return true;
}

static bool stream_equals(const t64_machine *machine, const uint8_t *baseline,
                          uint8_t *scratch, size_t length) {
  size_t got = 0u;

  return capture_into(machine, scratch, length, &got) && got == length &&
         memcmp(baseline, scratch, length) == 0;
}

typedef struct {
  uint8_t *baseline;
  uint8_t *scratch;
  size_t length;
} stream_baseline;

/* One byte flips and the stream is compared with the baseline, then the byte is
 * restored. A member of the stream must change it; an excluded member, a packet
 * word at or after the count and padding must not. */
static void check_byte_flip(t64_machine *machine, const stream_baseline *base,
                            uint8_t *byte, const char *name,
                            const char *excluded) {
  const uint8_t saved = *byte;
  bool changed;

  *byte = (uint8_t)(saved ^ 0x01u);
  changed = !stream_equals(machine, base->baseline, base->scratch, base->length);
  *byte = saved;
  if (excluded == NULL && !changed) {
    fprintf(stderr, "digest completeness: %s is not in the canonical stream\n",
            name);
  }
  if (excluded != NULL && changed) {
    fprintf(stderr, "digest completeness: excluded %s (%s) is in the stream\n",
            name, excluded);
  }
  T64_CHECK(excluded == NULL ? changed : !changed);
}

static bool is_depth_member(const char *name) {
  static const char *const names[] = {"depth_image_address",
                                      "depth_image_valid", "primitive_z",
                                      "primitive_dz", "primitive_depth_valid"};
  size_t i;

  for (i = 0u; i < sizeof names / sizeof names[0]; i++) {
    if (strcmp(name, names[i]) == 0) return true;
  }
  return false;
}

/* The canonical stream of the descriptor 13 family sees every RDP member
 * except the named exclusions, the memory coverage table and the hidden depth
 * table. The sweep flips the first byte of every member row, three entries of
 * the memory coverage table, three bytes of the hidden depth table (first,
 * middle, last) and strided TMEM bytes; every byte no row claims (padding and
 * the packet words at or after the count) must be inert. The stem id gets the
 * full sweep; the three save ids a representative one that always includes
 * the five depth members and the hidden table. One flip is also compared as a
 * SHA-256 digest, so the stream equality is tied to the digest the importer
 * declares. */
static void case_v26_completeness(void) {
  size_t id_index;

  for (id_index = 0u; id_index < FAMILY_COUNT; id_index++) {
    const bool full = id_index == 0u;
    t64_machine *machine = create_admitted(pairs[id_index].v26_id);
    stream_baseline base;
    uint8_t digest_before[T64_DIGEST_SIZE];
    uint8_t digest_after[T64_DIGEST_SIZE];
    uint8_t *rdp_bytes;
    uint8_t *claimed;
    size_t covered = 0u;
    size_t i;
    size_t byte;
    t64_digest_sink sink;

    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    /* a packet whose first four words are state: the count is 4 */
    machine->rdp.pending_triangle_word_count = 4u;
    (void)memset(&sink, 0, sizeof sink);
    T64_CHECK_INT(t64_digest_stream(machine, &sink), T64_STATUS_OK);
    base.length = sink.offset;
    base.baseline = (uint8_t *)malloc(base.length);
    base.scratch = (uint8_t *)malloc(base.length);
    claimed = (uint8_t *)calloc(sizeof machine->rdp, 1u);
    T64_CHECK(base.baseline != NULL && base.scratch != NULL && claimed != NULL);
    if (base.baseline == NULL || base.scratch == NULL || claimed == NULL) {
      free(base.baseline);
      free(base.scratch);
      free(claimed);
      t64_machine_destroy(machine);
      continue;
    }
    T64_CHECK(capture_into(machine, base.baseline, base.length, &covered));
    T64_CHECK_INT(covered, base.length);
    rdp_bytes = (uint8_t *)&machine->rdp;
    for (i = 0u; i < sizeof rdp_members / sizeof rdp_members[0]; i++) {
      const rdp_member *member = &rdp_members[i];

      T64_CHECK(member->offset + member->size <= sizeof machine->rdp);
      for (byte = member->offset; byte < member->offset + member->size &&
                                  byte < sizeof machine->rdp; byte++) {
        claimed[byte] = 1u;
      }
      /* the saved ids check the members a combiner, a coverage and a depth
       * consumer read, the stem id every row */
      if (full || member->excluded == NULL) {
        if (full || (i % 11u) == 0u || is_depth_member(member->name)) {
          check_byte_flip(machine, &base, rdp_bytes + member->offset,
                          member->name, member->excluded);
        }
      }
    }
    /* the memory coverage table: its first and last entries and a middle one */
    {
      const size_t table = machine->profile.rdram_bytes / 2u;
      static const size_t picks[3] = {0u, 0x1234u, 0u};
      size_t k;

      for (k = 0u; k < 3u; k++) {
        const size_t at = k == 2u ? table - 1u : picks[k];

        check_byte_flip(machine, &base, machine->rdp_memory_coverage + at,
                        "rdp_memory_coverage", NULL);
      }
    }
    /* the hidden depth table: the first, a middle and the last byte */
    {
      const size_t hidden = T64_RDP_DEPTH_HIDDEN_BYTES(machine->profile.rdram_bytes);
      const size_t picks[3] = {0u, hidden / 2u, hidden - 1u};
      size_t k;

      T64_CHECK(machine->rdp_depth_hidden != NULL);
      for (k = 0u; k < 3u && machine->rdp_depth_hidden != NULL; k++) {
        check_byte_flip(machine, &base, machine->rdp_depth_hidden + picks[k],
                        "rdp_depth_hidden", NULL);
      }
    }
    if (full) {
      const size_t words_begin = offsetof(t64_rdp_state, pending_triangle_words);
      const size_t words_end =
          words_begin + sizeof machine->rdp.pending_triangle_words;
      const size_t tmem_begin = offsetof(t64_rdp_state, tmem);

      for (byte = 0u; byte < sizeof machine->rdp; byte++) {
        if (claimed[byte]) continue;
        if (byte >= tmem_begin && byte < tmem_begin + 4096u) {
          if ((byte - tmem_begin) % 251u == 0u) {
            check_byte_flip(machine, &base, rdp_bytes + byte, "tmem", NULL);
          }
        } else if (byte >= words_begin && byte < words_end) {
          if ((byte - words_begin) % 8u == 0u) {
            check_byte_flip(machine, &base, rdp_bytes + byte,
                            "pending_triangle_words[>=count]",
                            "words at or after the count are not state");
          }
        } else {
          check_byte_flip(machine, &base, rdp_bytes + byte, "padding",
                          "padding");
        }
      }
    }
    /* every flip was restored: the stream equals its baseline again */
    T64_CHECK(stream_equals(machine, base.baseline, base.scratch, base.length));
    /* one flip tied to the digest itself */
    T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
    machine->rdp.combine_mode ^= UINT64_C(1);
    T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
    machine->rdp.combine_mode ^= UINT64_C(1);
    T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) != 0);
    free(base.baseline);
    free(base.scratch);
    free(claimed);
    t64_machine_destroy(machine);
  }
}


int main(int argc, char **argv) {
  static const char *const cases[] = {"v26-completeness",
                                      "v26-strip-differential"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "v26-completeness") == 0) {
      case_v26_completeness();
      return T64_TEST_FINISH("unit.core.digest.v26-completeness");
    }
    if (strcmp(argv[2], "v26-strip-differential") == 0) {
      case_v26_strip_differential();
      return T64_TEST_FINISH("unit.core.digest.v26-strip-differential");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_v26_completeness();
  case_v26_strip_differential();
  return T64_TEST_FINISH("unit.core.digest.v26");
}
