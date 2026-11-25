/*
 * Phase 9 digest sink and v25 walker tests (plan 09-01).
 *
 * sink-legacy-identity: the idle canonical digest of every pre-Phase-9
 * registered id, and the Phase 8 audio profile mid-DMA, are pinned as literal
 * hex values captured from the tree before the digest sink seam existed. The
 * values depend only on authored inputs and profile identity.
 *
 * v25-strip-differential: the composite family stream, with markers, identity
 * spans and new-section spans removed, equals the Phase 8 audio profile
 * stream with its identity spans removed, for machines built by the same
 * operations.
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
#define AUTHORED_ROM_BYTES 4096u
#define AI_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"
#define SI_STEM_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define AI_DRAM_ADDR UINT32_C(0x04500000)
#define AI_LEN UINT32_C(0x04500004)
#define AI_CONTROL UINT32_C(0x04500008)
#define AI_DACRATE UINT32_C(0x04500010)
#define AI_BITRATE UINT32_C(0x04500014)

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

static const t64_host_allocator allocator =
    T64_HOST_ALLOCATOR_INIT(test_allocate, test_release, NULL);

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

/* Authored 4 KiB image the skeleton profile accepts (same pattern as
 * core/tests/test_digest_trace.c). */
static void fill_authored_rom(uint8_t image[AUTHORED_ROM_BYTES]) {
  size_t i;

  for (i = 0u; i < AUTHORED_ROM_BYTES; i++) {
    image[i] = (uint8_t)((i * 13u + 7u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  image[0x3eu] = (uint8_t)'E';
}

static void to_hex(const uint8_t digest[T64_DIGEST_SIZE],
                   char hex[2u * T64_DIGEST_SIZE + 1u]) {
  static const char digits[] = "0123456789abcdef";
  size_t i;

  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    hex[2u * i] = digits[digest[i] >> 4];
    hex[2u * i + 1u] = digits[digest[i] & 0x0fu];
  }
  hex[2u * T64_DIGEST_SIZE] = '\0';
}

/* Create the machine for an id and admit the authored input it accepts. */
static t64_machine *create_admitted(const char *profile_id, bool ipl3) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  uint8_t image[AUTHORED_ROM_BYTES];
  t64_status status;

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  if (ipl3) {
    if (!read_openipl3_asset(asset)) {
      T64_CHECK(false);
      t64_machine_destroy(machine);
      return NULL;
    }
    status = t64_machine_admit_rom(machine, asset, sizeof asset);
  } else {
    fill_authored_rom(image);
    status = t64_machine_admit_rom(machine, image, sizeof image);
  }
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

typedef struct {
  const char *id;
  bool ipl3;
  const char *digest_hex;
} idle_golden;

/* Literal digests captured at the tree before the sink seam. */
static const idle_golden idle_goldens[] = {
    {"ntsc-cart-4m-skeleton", false,
     "6813739ff2fc1cffac6d85fa86df0c593e3035f8a640c642e37790acd96b6e77"},
    {"ntsc-cart-4m-openipl3-warm-v1", true,
     "d2212ea39cadf8642cab2451fbcea484f5c6997d51619faef41e9ada24aa9724"},
    {"ntsc-cart-4m-openipl3-vr4300-v1", true,
     "1e9cab87de3581578e4f5f49b1bcf6d8114046f910814b4af4f3f2e6ec42f906"},
    {"ntsc-cart-4m-openipl3-vr4300-compat-v1", true,
     "dd8a0509ecbd0211965618943e3657601f2a8de57dc3c4dfc6fd4f0495135628"},
    {"ntsc-cart-4m-openipl3-vr4300-compat-v2", true,
     "f943e44778ae482f12798f681335608b0c57812874874b84583674495e70ce2e"},
    {"ntsc-cart-4m-openipl3-rsp-functional-v1", true,
     "1258bdf042e3b37d380e123f7f0622c69d1724c12f9da40743ec002416d5e114"},
    {"ntsc-cart-4m-openipl3-rdram-a-t1-v1", true,
     "6d1f9c7e219d64943fbc76b648c9958b22eccab018d974f2266f40996629e05b"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-v1", true,
     "e9d01b65a1a4bab733d5c4b320687f255e6ae4dbe61a7521554e99b943b9c72e"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2", true,
     "778cf42f5d6878d2024d014b305d2604039c0134f9c444b77d844d9d1f3f30bd"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3", true,
     "8bd2a8e24d37cc5b87ec00e87246ea4d0739899325d80527a58c962728c15a08"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4", true,
     "ebdec6c9252447d6cbfda58eff8cd8672d5d43635ca733163827023983a7fde2"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-combiner-v5", true,
     "523f23a084f3861ed530fd081859e5771be99a4d457960e15610b6e56049f104"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6", true,
     "debd84e5b937902da94601aeaad6f2b566d39ab4c2ec1d64a5645f45d21547ec"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7", true,
     "77ec75199701fd1db4fba27ed73c3a35a30937ec9fa458691da24f4817f84321"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8", true,
     "b0b31d41190bb71eaa3bcad94a3cdd4f3461246ae28dd08223409de70e638290"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9", true,
     "9be8ab2aef568820b1926aa7815b7f87a5b8cb8859527fa3f4241fa04eca16f3"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1", true,
     "59132ab9cdf06889d5565184889773e62bbf7d2d31c2720867d3275aefb815cd"},
};

#define IDLE_GOLDEN_COUNT (sizeof idle_goldens / sizeof idle_goldens[0])

static const char ai_mid_dma_golden[] =
    "24bbb649254ff4f091a2492734d8995b1fce61c191eba4c9756772aa280643e4";

static void check_digest_literal(const char *label, const t64_machine *machine,
                                 const char *expected_hex) {
  uint8_t digest[T64_DIGEST_SIZE];
  char hex[2u * T64_DIGEST_SIZE + 1u];

  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  to_hex(digest, hex);
  if (strcmp(hex, expected_hex) != 0) {
    fprintf(stderr, "digest for %s: got %s want %s\n", label, hex,
            expected_hex);
  }
  T64_CHECK_STR(hex, expected_hex);
}

/*
 * AI profile with one DMA submitted and the CPU running, stopped one tick
 * before the first sample event dispatches: the descriptor is active, the
 * sample event is queued and no packet exists. Packet metadata (including the
 * build identity) is first written when that event dispatches, so a digest
 * taken here depends only on authored inputs and profile identity. Built only
 * through host bus writes and guest-tick advances, as in core/tests/test_ai.c.
 */
static t64_machine *build_ai_mid_dma(const char *profile_id) {
  static const uint8_t literal_pcm[16] = {
      0x00u, 0x00u, 0x7fu, 0xffu, 0x80u, 0x00u, 0xffu, 0xffu,
      0x12u, 0x34u, 0xedu, 0xccu, 0x00u, 0x01u, 0xffu, 0xfeu};
  t64_machine *machine = create_admitted(profile_id, true);
  t64_stop_request request = T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, 0u);
  t64_stop_result stop = T64_STOP_RESULT_INIT;
  uint64_t sample_tick = UINT64_MAX;
  size_t i;

  if (machine == NULL) return NULL;
  (void)memcpy(machine->ram + UINT32_C(0x100), literal_pcm, sizeof literal_pcm);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u, 1103u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u, 15u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u, 0x100u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 16u),
                T64_STATUS_OK);
  /* Guest spin loop so the CPU keeps running without leaving RDRAM. */
  machine->ram[UINT32_C(0x1000)] = 0x08u;
  machine->ram[UINT32_C(0x1001)] = 0x00u;
  machine->ram[UINT32_C(0x1002)] = 0x04u;
  machine->ram[UINT32_C(0x1003)] = 0x00u;
  (void)memset(machine->ram + UINT32_C(0x1004), 0, 4u);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  for (i = 0u; i < machine->queue_count; i++) {
    const size_t index = (machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];

    if (event->device == T64_AI_DEVICE && event->kind == T64_AI_EVENT_SAMPLE) {
      sample_tick = event->tick;
      break;
    }
  }
  T64_CHECK(sample_tick != UINT64_MAX && sample_tick > machine->tick + 8u);
  if (sample_tick == UINT64_MAX || sample_tick <= machine->tick + 8u) {
    t64_machine_destroy(machine);
    return NULL;
  }
  request.value = sample_tick - 1u;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
  /* Reached: DMA active, CPU has run, no sample dispatched, no packet. */
  T64_CHECK(machine->ai.active);
  T64_CHECK_INT(machine->tick, sample_tick - 1u);
  T64_CHECK(machine->cpu.retired_instructions > 0u);
  T64_CHECK_INT(machine->ai.sample_index, 0u);
  T64_CHECK_INT(machine->ai.packet_history_count, 0u);
  T64_CHECK_INT(machine->ai.packet_info[0].struct_size, 0u);
  T64_CHECK_INT(machine->ai.packet_lengths[0], 0u);
  return machine;
}

static void case_sink_legacy_identity(void) {
  size_t index;
  t64_machine *machine;

  T64_CHECK_INT(IDLE_GOLDEN_COUNT, 17);
  for (index = 0u; index < IDLE_GOLDEN_COUNT; index++) {
    const idle_golden *golden = &idle_goldens[index];

    machine = create_admitted(golden->id, golden->ipl3);
    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    check_digest_literal(golden->id, machine, golden->digest_hex);
    t64_machine_destroy(machine);
  }
  machine = build_ai_mid_dma(AI_PROFILE_ID);
  T64_CHECK(machine != NULL);
  if (machine != NULL) {
    check_digest_literal("ai-mid-dma", machine, ai_mid_dma_golden);
    t64_machine_destroy(machine);
  }
}

/* ---- stream capture and the strip-and-compare comparator --------------- */

#define SPAN_CAPACITY 4096u
#define SI_DOMAIN "t64.state.si.25"
#define SI_DOMAIN_LENGTH 15u
#define MARKER_COUNT_FULL 10u
/* The three save ids add section 11 (save media) after section 10. */
#define MARKER_COUNT_SAVE 11u
#define SAVE_E4K_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k"
#define SAVE_E16K_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k"
#define SAVE_S32K_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k"

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

static bool find_marker(const stream *captured, uint32_t id,
                        size_t *offset_out);

/*
 * The identity-class spans of a v25 stream are exactly two: the domain at the
 * start, and the serialization word with the profile digest right after the
 * first marker. The other non-marker spans are new-class: the one that ends
 * section 8 (the RDP members the v24 walker never hashed, plan 09-10: the
 * combiner block and the memory coverage table; it ends exactly at the marker
 * of section 9), the one that carries the body of section 10 (SI, PIF and
 * latch) and runs from the tenth marker to the end of the stream; a save id
 * adds a further new-class span, the body of section 11, which then runs to
 * the end of the stream. Any other identity or new-section span is a defect
 * (the packet profile and build ids are omitted under v25).
 */
static bool identity_spans_are_exact(const stream *captured,
                                     size_t expected_new_spans,
                                     size_t *offset_out) {
  size_t seen = 0u;
  size_t i;
  size_t new_spans = 0u;
  size_t last_new_end = 0u;
  bool section8_tail = false;
  size_t marker8 = 0u;
  size_t marker9 = 0u;

  *offset_out = 0u;
  if (!find_marker(captured, 8u, &marker8) ||
      !find_marker(captured, 9u, &marker9)) {
    return false;
  }
  for (i = 0u; i < captured->span_count; i++) {
    const t64_digest_span *span = &captured->spans[i];

    if (span->class == T64_DIGEST_CLASS_MARKER) continue;
    if (span->class == T64_DIGEST_CLASS_IDENTITY && seen == 0u &&
        span->offset == 0u && span->length == SI_DOMAIN_LENGTH) {
      seen = 1u;
    } else if (span->class == T64_DIGEST_CLASS_IDENTITY && seen == 1u &&
               span->offset == SI_DOMAIN_LENGTH + 4u &&
               span->length == 4u + T64_DIGEST_SIZE) {
      seen = 2u;
    } else if (span->class == T64_DIGEST_CLASS_NEW && seen == 2u &&
               !section8_tail && span->offset > marker8 &&
               span->offset + span->length == marker9) {
      section8_tail = true;
    } else if (span->class == T64_DIGEST_CLASS_NEW && seen == 2u &&
               section8_tail && new_spans < expected_new_spans) {
      new_spans++;
      last_new_end = span->offset + span->length;
    } else {
      *offset_out = span->offset;
      return false;
    }
  }
  /* The last new-class span (section 10 for the stem, section 11 for a save
     id) runs to the end of the stream. */
  if (seen != 2u || !section8_tail || new_spans != expected_new_spans ||
      last_new_end != captured->length) {
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
 * Compare the older stream with the newer one after removing framing: the
 * stripped length first, then marker order of the newer stream, then the
 * stripped body bytes and the domain text (a body mismatch reports its offset),
 * then the exact set of identity spans of the newer stream.
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
  } else if (!markers_are_ordered(newer, marker_count, &result.offset)) {
    result.kind = CMP_MARKER_ORDER;
  } else if (i != common) {
    result.kind = CMP_BODY_MISMATCH;
  } else if (newer->length < SI_DOMAIN_LENGTH ||
             memcmp(newer->bytes, SI_DOMAIN, SI_DOMAIN_LENGTH) != 0) {
    result.kind = CMP_BODY_MISMATCH;
    result.offset = 0u;
  } else if (!identity_spans_are_exact(
                 newer, marker_count > MARKER_COUNT_FULL ? 2u : 1u,
                 &result.offset)) {
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
  t64_stop_request request = T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, tick);
  t64_stop_result stop = T64_STOP_RESULT_INIT;
  const t64_status status = t64_machine_advance(machine, &request, &stop);

  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
  return status == T64_STATUS_OK && stop.reason == T64_STOP_REASON_BOUND;
}

/* Idle: admitted, a guest spin loop retiring in RDRAM, tick 64. */
static t64_machine *build_idle(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id, true);

  if (machine == NULL) return NULL;
  install_spin_loop(machine);
  T64_CHECK(advance_to(machine, 64u));
  T64_CHECK_INT(machine->tick, 64u);
  T64_CHECK(machine->cpu.retired_instructions > 0u);
  return machine;
}

static bool next_ai_sample_tick(const t64_machine *machine, uint64_t *tick) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const size_t index = (machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];

    if (event->device == T64_AI_DEVICE && event->kind == T64_AI_EVENT_SAMPLE) {
      *tick = event->tick;
      return true;
    }
  }
  return false;
}

/* Two 16-byte DMAs of literal PCM, the rate pair 1103/15, the CPU in a spin
 * loop: the descriptor is active and the first sample event is queued. */
static t64_machine *start_ai_dma_pair(const char *profile_id) {
  static const uint8_t literal_pcm[32] = {
      0x00u, 0x00u, 0x7fu, 0xffu, 0x80u, 0x00u, 0xffu, 0xffu,
      0x12u, 0x34u, 0xedu, 0xccu, 0x00u, 0x01u, 0xffu, 0xfeu,
      0x80u, 0x00u, 0x7fu, 0xffu, 0xffu, 0xffu, 0x00u, 0x00u,
      0x55u, 0xaau, 0xaau, 0x55u, 0x00u, 0x00u, 0x80u, 0x00u};
  t64_machine *machine = create_admitted(profile_id, true);

  if (machine == NULL) return NULL;
  (void)memcpy(machine->ram + UINT32_C(0x100), literal_pcm, sizeof literal_pcm);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u, 1103u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u, 15u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u, 0x100u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 16u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u, 0x110u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 16u),
                T64_STATUS_OK);
  install_spin_loop(machine);
  return machine;
}

/* Advance one AI sample event at a time until `samples` have dispatched. */
static bool dispatch_ai_samples(t64_machine *machine, uint64_t samples) {
  while (machine->ai.sample_index < samples) {
    uint64_t tick = 0u;

    if (!next_ai_sample_tick(machine, &tick)) {
      T64_CHECK(false);
      return false;
    }
    if (!advance_to(machine, tick)) return false;
  }
  return true;
}

/* AI mid-DMA: the first sample event has dispatched, so a packet is being
 * built (its metadata, including the profile and build identity, exists) and
 * no packet is complete. */
static t64_machine *build_ai_mid_dma_packet(const char *profile_id) {
  t64_machine *machine = start_ai_dma_pair(profile_id);

  if (machine == NULL) return NULL;
  T64_CHECK(dispatch_ai_samples(machine, 1u));
  T64_CHECK(machine->ai.active);
  T64_CHECK_INT(machine->ai.sample_index, 1u);
  T64_CHECK_INT(machine->ai.packet_length, 4u);
  T64_CHECK_INT(machine->ai.packet_history_count, 0u);
  T64_CHECK_INT(machine->ai.packet_count, 0u);
  T64_CHECK(machine->ai.packet_info[0].profile_id[0] != '\0');
  return machine;
}

/* AI after the first packet: four samples dispatched, one packet complete and
 * queued, the second descriptor active. */
static t64_machine *build_ai_first_packet(const char *profile_id) {
  t64_machine *machine = start_ai_dma_pair(profile_id);

  if (machine == NULL) return NULL;
  T64_CHECK(dispatch_ai_samples(machine, 4u));
  T64_CHECK(machine->ai.active);
  T64_CHECK_INT(machine->ai.packet_history_count, 1u);
  T64_CHECK_INT(machine->ai.packet_count, 1u);
  T64_CHECK_INT(machine->ai.descriptor_count, 1u);
  T64_CHECK_INT(machine->ai.packet_info[0].frame_count, 4u);
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
  t64_machine *machine = create_admitted(profile_id, true);
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

/* An RDP command list of four sync words submitted and one work event
 * dispatched: the first word is fetched and pending, three words remain. */
static t64_machine *build_rdp_pending(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id, true);
  size_t i;
  size_t q;
  uint64_t tick = UINT64_MAX;
  bool pending = false;

  if (machine == NULL) return NULL;
  for (i = 0u; i < 4u; i++) {
    static const uint8_t full_sync[8] = {0xe9u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

    (void)memcpy(machine->ram + UINT32_C(0x100) + i * 8u, full_sync,
                 sizeof full_sync);
  }
  install_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START_REGISTER, 4u,
                                      UINT32_C(0x100)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END_REGISTER, 4u,
                                      UINT32_C(0x120)),
                T64_STATUS_OK);
  for (q = 0u; q < machine->queue_count; q++) {
    const size_t index = (machine->queue_head + q) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];

    if (event->device == T64_RDP_DEVICE && event->kind == T64_RDP_EVENT_WORK) {
      tick = event->tick;
      break;
    }
  }
  T64_CHECK(tick != UINT64_MAX);
  if (tick != UINT64_MAX) T64_CHECK(advance_to(machine, tick));
  for (q = 0u; q < machine->queue_count; q++) {
    const size_t index = (machine->queue_head + q) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];

    if (event->device == T64_RDP_DEVICE && event->kind == T64_RDP_EVENT_WORK) {
      pending = true;
    }
  }
  /* Reached: a fetched command word is pending, more words remain in the
   * list, and an RDP work event is still queued. */
  T64_CHECK(pending);
  T64_CHECK(machine->rdp.command_pending);
  T64_CHECK(machine->rdp.pending_command_word != 0u);
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x108));
  T64_CHECK(machine->rdp.dpc_current < machine->rdp.dpc_end);
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

/* The strip differential for a save id: the Phase 8 AI profile and the save
 * id are built identically; section 11 (and section 10) are new-class spans,
 * so the stripped streams are equal with eleven markers. A mid-write state is
 * an EEPROM write executed through the device entry point (the busy window is
 * open), so the machine state outside the save section is untouched. */
static void differential_for_save_id(const char *label, const char *save_id,
                                     bool mid_write) {
  t64_machine *older = build_idle(AI_PROFILE_ID);
  t64_machine *newer = build_idle(save_id);
  stream old_stream;
  stream new_stream;

  T64_CHECK(older != NULL && newer != NULL);
  if (older != NULL && newer != NULL && mid_write) {
    static const uint8_t tx[10] = {0x05, 0x01, 0x11, 0x12, 0x13,
                                   0x14, 0x15, 0x16, 0x17, 0x18};
    uint8_t rx[1] = {0xFF};

    T64_CHECK_INT(t64_save_channel4_execute(newer, newer->tick, tx, 10u, rx,
                                            1u),
                  0u);
    T64_CHECK(newer->save.busy_until > newer->tick);
    T64_CHECK_INT(newer->save.bytes[8], 0x11u);
  }
  if (older != NULL && newer != NULL &&
      capture_stream(older, &old_stream)) {
    if (capture_stream(newer, &new_stream)) {
      const cmp_result result =
          compare_stripped(&old_stream, &new_stream, MARKER_COUNT_SAVE);

      report_comparison(label, &result);
      T64_CHECK_INT(result.kind, CMP_EQUAL);
      free_stream(&new_stream);
    }
    free_stream(&old_stream);
  }
  if (older != NULL) t64_machine_destroy(older);
  if (newer != NULL) t64_machine_destroy(newer);
}

static void differential_for_state(const char *label, state_builder build) {
  t64_machine *older = build(AI_PROFILE_ID);
  t64_machine *newer = build(SI_STEM_ID);
  stream old_stream;
  stream new_stream;

  T64_CHECK(older != NULL && newer != NULL);
  if (older != NULL && newer != NULL &&
      capture_stream(older, &old_stream)) {
    if (capture_stream(newer, &new_stream)) {
      const cmp_result result =
          compare_stripped(&old_stream, &new_stream, MARKER_COUNT_FULL);

      report_comparison(label, &result);
      T64_CHECK_INT(result.kind, CMP_EQUAL);
      free_stream(&new_stream);
    }
    free_stream(&old_stream);
  }
  if (older != NULL) t64_machine_destroy(older);
  if (newer != NULL) t64_machine_destroy(newer);
}

/* ---- independent reference encoder (framing and five sections) --------- */

/*
 * Written from the documented section table and the machine fields, without
 * calling production code. It covers the domain, the section ids and the
 * bodies of sections 1, 2, 3, 4 and 6, and the tail of section 8 that the
 * family carries since plan 09-10 (the combiner block and the memory coverage
 * table); sections 5, 7 and 9 and the rest of section 8 are covered by the
 * differential against the v24 stream, which the legacy digest literals pin.
 * Fields are fixed-width big-endian.
 */
typedef struct {
  uint8_t *data;
  size_t length;
  size_t capacity;
} blob;

static bool blob_init(blob *out, size_t capacity) {
  out->data = (uint8_t *)malloc(capacity);
  out->length = 0u;
  out->capacity = capacity;
  return out->data != NULL;
}

static void blob_bytes(blob *out, const void *bytes, size_t count) {
  if (out->length + count > out->capacity) {
    T64_CHECK(false);
    return;
  }
  (void)memcpy(out->data + out->length, bytes, count);
  out->length += count;
}

static void blob_u32(blob *out, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)(value >> 24), (uint8_t)(value >> 16),
                            (uint8_t)(value >> 8), (uint8_t)value};

  blob_bytes(out, bytes, sizeof bytes);
}

static void blob_u64(blob *out, uint64_t value) {
  blob_u32(out, (uint32_t)(value >> 32));
  blob_u32(out, (uint32_t)(value & UINT64_C(0xffffffff)));
}

static void encode_identity_section(blob *out, const t64_machine *machine) {
  blob_u32(out, 25u);  /* state serialization version of the family */
  blob_bytes(out, machine->profile_digest, sizeof machine->profile_digest);
  blob_bytes(out, machine->content_digest, sizeof machine->content_digest);
  blob_u32(out, 1u);  /* image admitted */
  blob_u32(out, (uint32_t)machine->image_length);
  blob_bytes(out, machine->rom_header, sizeof machine->rom_header);
  blob_bytes(out, machine->rom_title, sizeof machine->rom_title);
  blob_bytes(out, machine->rom_game_code, sizeof machine->rom_game_code);
  blob_u32(out, machine->rom_version);
}

static void encode_core_section(blob *out, const t64_machine *machine) {
  blob_u64(out, machine->tick);
  blob_u64(out, machine->instruction_count);
  blob_u64(out, machine->frame_event_count);
  blob_u32(out, machine->sticky_error_status);
  blob_u32(out, machine->halted ? 1u : 0u);
}

static void encode_rdram_section(blob *out, const t64_machine *machine) {
  blob_u32(out, machine->profile.rdram_bytes);
  blob_bytes(out, machine->ram, machine->profile.rdram_bytes);
}

static void encode_sp_memory_section(blob *out, const t64_machine *machine) {
  blob_u32(out, 4096u);
  blob_bytes(out, machine->sp_dmem, sizeof machine->sp_dmem);
  blob_u32(out, 4096u);
  blob_bytes(out, machine->sp_imem, sizeof machine->sp_imem);
}

static void encode_scheduler_section(blob *out, const t64_machine *machine) {
  size_t i;

  blob_u32(out, (uint32_t)machine->queue_count);
  blob_u64(out, machine->next_sequence);
  blob_u32(out, machine->sequence_exhausted ? 1u : 0u);
  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    blob_u64(out, event->tick);
    blob_u64(out, event->sequence);
    blob_u32(out, event->device);
    blob_u32(out, event->kind);
    blob_u64(out, event->value);
    blob_u64(out, event->token);
  }
}

/* The tail of section 8 (plan 09-10, STATE-CONTENT-F1), written from the
 * documented layout: primitive LOD fraction, environment, fog and blend colors,
 * the combine mode, seven valid flags (combine, environment, fog, blend, key
 * center, key scale, key width), the key center, scale and width of each of the
 * three channels, then the coverage table size word and the table (one byte per
 * two bytes of RDRAM). */
static void encode_rdp_carried_tail(blob *out, const t64_machine *machine) {
  const t64_rdp_state *rdp = &machine->rdp;
  size_t i;

  blob_u32(out, rdp->primitive_lod_fraction);
  blob_u32(out, rdp->environment_color);
  blob_u32(out, rdp->fog_color);
  blob_u32(out, rdp->blend_color);
  blob_u64(out, rdp->combine_mode);
  blob_u32(out, rdp->combine_mode_valid ? 1u : 0u);
  blob_u32(out, rdp->environment_color_valid ? 1u : 0u);
  blob_u32(out, rdp->fog_color_valid ? 1u : 0u);
  blob_u32(out, rdp->blend_color_valid ? 1u : 0u);
  blob_u32(out, rdp->key_center_valid ? 1u : 0u);
  blob_u32(out, rdp->key_scale_valid ? 1u : 0u);
  blob_u32(out, rdp->key_width_valid ? 1u : 0u);
  for (i = 0u; i < 3u; i++) {
    blob_u32(out, rdp->key_center[i]);
    blob_u32(out, rdp->key_scale[i]);
    blob_u32(out, rdp->key_width[i]);
  }
  blob_u32(out, machine->profile.rdram_bytes / 2u);
  blob_bytes(out, machine->rdp_memory_coverage,
             machine->profile.rdram_bytes / 2u);
}


/* Marker span of section `id`: the id-th marker in stream order. */
static bool find_marker(const stream *captured, uint32_t id,
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

/* Body of section `id`: from its marker's end to the next marker or end. */
static bool section_body(const stream *captured, uint32_t id, size_t *begin,
                         size_t *end) {
  size_t marker;
  size_t next;

  if (!find_marker(captured, id, &marker)) return false;
  *begin = marker + 4u;
  *end = find_marker(captured, id + 1u, &next) ? next : captured->length;
  return true;
}

static void check_section_against_reference(const stream *captured,
                                            uint32_t id, const blob *expected,
                                            const char *name) {
  size_t begin = 0u;
  size_t end = 0u;
  size_t first_difference = 0u;
  size_t i;

  T64_CHECK(section_body(captured, id, &begin, &end));
  T64_CHECK_INT(end - begin, expected->length);
  if (end - begin != expected->length) {
    fprintf(stderr, "reference section %s: production %zu bytes, reference "
            "%zu bytes\n", name, end - begin, expected->length);
    return;
  }
  for (i = 0u; i < expected->length; i++) {
    if (captured->bytes[begin + i] != expected->data[i]) {
      first_difference = i + 1u;
      break;
    }
  }
  if (first_difference != 0u) {
    fprintf(stderr, "reference section %s differs at body offset %zu\n", name,
            first_difference - 1u);
  }
  T64_CHECK_INT(first_difference, 0u);
}

/* The last bytes of section 8 equal the reference tail. */
static void check_section8_tail(const stream *captured, const blob *expected) {
  size_t begin = 0u;
  size_t end = 0u;
  size_t first_difference = 0u;
  size_t i;

  T64_CHECK(section_body(captured, 8u, &begin, &end));
  T64_CHECK(end - begin >= expected->length);
  if (end - begin < expected->length) return;
  begin = end - expected->length;
  for (i = 0u; i < expected->length; i++) {
    if (captured->bytes[begin + i] != expected->data[i]) {
      first_difference = i + 1u;
      break;
    }
  }
  if (first_difference != 0u) {
    fprintf(stderr, "reference tail of section 8 differs at tail offset %zu\n",
            first_difference - 1u);
  }
  T64_CHECK_INT(first_difference, 0u);
}

/* ---- digest completeness of the RDP block (plan 09-10, STATE-CONTENT-F1) -- */

/*
 * Every member of t64_rdp_state is either in the canonical stream of the
 * composite family (flipping one byte changes the stream, and so the SHA-256
 * digest, which the hash sink computes over exactly these bytes) or on this
 * list with the reason it is not state. The table is the complete member list
 * of the structure: a size assertion fails the build when a member is added
 * without a row, and every byte that no row covers (padding) is flipped as well
 * and must not change the stream. The before-state of the old v24 walker had 24
 * members outside the stream on the family (the review of plan 09-10); this
 * list has the five depth members that K1 keeps inactive on descriptor 12 and
 * the three observation members that only core/tests/test_rdp_triangles.c reads.
 */
typedef struct {
  const char *name;
  size_t offset;
  size_t size;
  const char *excluded; /* NULL: the member is in the canonical stream */
} rdp_member;

#define RDP_DEPTH_REASON                                                     \
  "K1: depth is inactive on descriptor 12; SetZImage and SetPrimDepth are "  \
  "refused on the family, the commit zeroes the address"
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
    M(color_image_address), MX(depth_image_address, RDP_DEPTH_REASON),
    M(color_image_width), M(color_image_format), M(color_image_size),
    M(scissor_xh), M(scissor_yh), M(scissor_xl), M(scissor_yl),
    M(other_modes), M(fill_color),
    MX(primitive_z, RDP_DEPTH_REASON), MX(primitive_dz, RDP_DEPTH_REASON),
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
    M(color_image_valid), MX(depth_image_valid, RDP_DEPTH_REASON),
    MX(primitive_depth_valid, RDP_DEPTH_REASON), M(scissor_valid),
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

/* The canonical stream of the composite family sees every RDP member except
 * the named exclusions. The sweep flips the first byte of every member row, of
 * the memory coverage table and of strided TMEM bytes, and every byte no row
 * claims (padding and the packet words at or after the count) must be inert.
 * The stem id gets the full sweep; the three save ids a representative one
 * (the predicate is the family tuple, not the id). One flip is also compared
 * as a SHA-256 digest, so the stream equality is tied to the digest the
 * importer declares. */
static void check_rdp_digest_completeness(void) {
  static const char *const ids[] = {SI_STEM_ID, SAVE_E4K_ID, SAVE_E16K_ID,
                                    SAVE_S32K_ID};
  size_t id_index;

  for (id_index = 0u; id_index < sizeof ids / sizeof ids[0]; id_index++) {
    const bool full = id_index == 0u;
    t64_machine *machine = create_admitted(ids[id_index], true);
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
      /* the saved ids check the members a combiner and a coverage consumer
       * read, the stem id every row */
      if (full || member->excluded == NULL) {
        if (full || (i % 11u) == 0u) {
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

/* The v25 walker must never silently stop covering the memory coverage table
 * (plan 09-10, re-check R1 RC-01). The pointer is set at creation for every
 * family id, so the branch is unreachable through the API; the control edits
 * the member directly. Accepted baseline in the same setup: the unedited
 * machine digests, and so does the restored one, with the same digest. A
 * walker that skipped the table bytes would return OK with a shorter stream. */
static void check_digest_refuses_missing_coverage_table(void) {
  static const char *const ids[] = {SI_STEM_ID, SAVE_E4K_ID, SAVE_E16K_ID,
                                    SAVE_S32K_ID};
  size_t id_index;

  for (id_index = 0u; id_index < sizeof ids / sizeof ids[0]; id_index++) {
    t64_machine *machine = create_admitted(ids[id_index], true);
    uint8_t baseline[T64_DIGEST_SIZE];
    uint8_t refused[T64_DIGEST_SIZE];
    uint8_t restored[T64_DIGEST_SIZE];
    uint8_t *table;

    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    table = machine->rdp_memory_coverage;
    T64_CHECK(table != NULL);
    T64_CHECK_INT(t64_machine_digest(machine, baseline), T64_STATUS_OK);
    machine->rdp_memory_coverage = NULL;
    (void)memset(refused, 0xA5, sizeof refused);
    T64_CHECK_INT(t64_machine_digest(machine, refused),
                  T64_STATUS_INTERNAL_DEFECT);
    /* The missing table is an internal inconsistency (plan 11-03, AMBIG-B):
     * the digest answers INTERNAL_DEFECT and poisons the instance. The control
     * restores the member and clears the flag white-box to re-digest. */
    T64_CHECK_INT(machine->poisoned, 1);
    machine->rdp_memory_coverage = table;
    machine->poisoned = 0u;
    T64_CHECK_INT(t64_machine_digest(machine, restored), T64_STATUS_OK);
    T64_CHECK(memcmp(baseline, restored, T64_DIGEST_SIZE) == 0);
    t64_machine_destroy(machine);
  }
}

static void case_v25_reference(void) {
  t64_machine *machine = build_ai_mid_dma_packet(SI_STEM_ID);
  stream captured;
  blob section;
  uint32_t id;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  /* Make every core counter distinctive; the stream is only captured. */
  machine->instruction_count = UINT64_C(0x1112131415161718);
  machine->frame_event_count = UINT64_C(0x0102030405060708);
  machine->sticky_error_status = UINT32_C(0x21222324);
  machine->halted = true;
  /* Distinctive combiner block and coverage marks, so the reference tail of
   * section 8 is compared on non-default values. */
  machine->rdp.primitive_lod_fraction = 0x11u;
  machine->rdp.environment_color = UINT32_C(0x21222324);
  machine->rdp.fog_color = UINT32_C(0x31323334);
  machine->rdp.blend_color = UINT32_C(0x41424344);
  machine->rdp.combine_mode = UINT64_C(0x5152535455565758);
  machine->rdp.combine_mode_valid = true;
  machine->rdp.environment_color_valid = true;
  machine->rdp.fog_color_valid = false;
  machine->rdp.blend_color_valid = true;
  machine->rdp.key_center_valid = true;
  machine->rdp.key_scale_valid = false;
  machine->rdp.key_width_valid = true;
  machine->rdp.key_center[0] = 0x61u;
  machine->rdp.key_center[1] = 0x62u;
  machine->rdp.key_center[2] = 0x63u;
  machine->rdp.key_scale[0] = 0x71u;
  machine->rdp.key_scale[1] = 0x72u;
  machine->rdp.key_scale[2] = 0x73u;
  machine->rdp.key_width[0] = 0x181u;
  machine->rdp.key_width[1] = 0x182u;
  machine->rdp.key_width[2] = 0x183u;
  t64_rdp_memory_coverage_write(machine, 0x3000u, 2u, 8u);
  t64_rdp_memory_coverage_write(machine, 0x3002u, 4u, 5u);
  t64_rdp_memory_coverage_write(machine, 0x3ffffcu, 4u, 1u);
  if (!capture_stream(machine, &captured)) {
    t64_machine_destroy(machine);
    return;
  }

  /* Domain, then section ids 1..10 literally, ascending. */
  T64_CHECK(memcmp(captured.bytes, "t64.state.si.25", 15u) == 0);
  for (id = 1u; id <= MARKER_COUNT_FULL; id++) {
    size_t marker = 0u;

    T64_CHECK(find_marker(&captured, id, &marker));
    if (find_marker(&captured, id, &marker)) {
      T64_CHECK_INT(captured.spans[0].offset, 0u);
      T64_CHECK_INT(read_u32(captured.bytes + marker), id);
    }
  }
  {
    size_t beyond = 0u;

    /* Reserved ids 11 and 16 emit nothing yet: no eleventh marker. */
    T64_CHECK(!find_marker(&captured, MARKER_COUNT_FULL + 1u, &beyond));
  }

  if (blob_init(&section, 8u * 1024u * 1024u)) {
    section.length = 0u;
    encode_identity_section(&section, machine);
    check_section_against_reference(&captured, 1u, &section, "identity");
    section.length = 0u;
    encode_core_section(&section, machine);
    check_section_against_reference(&captured, 2u, &section, "machine core");
    section.length = 0u;
    encode_rdram_section(&section, machine);
    check_section_against_reference(&captured, 3u, &section, "rdram");
    section.length = 0u;
    encode_sp_memory_section(&section, machine);
    check_section_against_reference(&captured, 4u, &section, "sp memory");
    section.length = 0u;
    encode_scheduler_section(&section, machine);
    check_section_against_reference(&captured, 6u, &section, "scheduler");
    section.length = 0u;
    encode_rdp_carried_tail(&section, machine);
    check_section8_tail(&captured, &section);
    /* the tail is the combiner block 88, the size word 4 and the table */
    T64_CHECK_INT(section.length, 88u + 4u + 2097152u);
    free(section.data);
  } else {
    T64_CHECK(false);
  }
  free_stream(&captured);
  t64_machine_destroy(machine);
  check_rdp_digest_completeness();
  check_digest_refuses_missing_coverage_table();
}

static void sha256_of(const uint8_t *bytes, size_t length,
                      uint8_t digest[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;

  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
}

/*
 * Framing and sink-seam checks on the real family stream: domain, coalesced
 * and classed spans, ascending section ids, the hash sink equals SHA-256 of
 * the buffer sink, and a write past capacity or past the span array sets the
 * overflow flag, copies nothing and still advances the offset.
 */
static void check_family_stream(const t64_machine *machine,
                                const stream *captured) {
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t expected[T64_DIGEST_SIZE];
  size_t offset = 0u;
  size_t i;

  T64_CHECK(captured->length > SI_DOMAIN_LENGTH + 4u);
  T64_CHECK(memcmp(captured->bytes, SI_DOMAIN, SI_DOMAIN_LENGTH) == 0);
  T64_CHECK(markers_are_ordered(captured, MARKER_COUNT_FULL, &offset));
  /* Domain (identity), first marker, then serialization word and profile
   * digest coalesced into one identity span (4 + 32 bytes). */
  T64_CHECK(captured->span_count >= 3u);
  if (captured->span_count >= 3u) {
    T64_CHECK_INT(captured->spans[0].offset, 0u);
    T64_CHECK_INT(captured->spans[0].length, SI_DOMAIN_LENGTH);
    T64_CHECK_INT(captured->spans[0].class, T64_DIGEST_CLASS_IDENTITY);
    T64_CHECK_INT(captured->spans[1].offset, SI_DOMAIN_LENGTH);
    T64_CHECK_INT(captured->spans[1].length, 4u);
    T64_CHECK_INT(captured->spans[1].class, T64_DIGEST_CLASS_MARKER);
    T64_CHECK_INT(captured->spans[2].offset, SI_DOMAIN_LENGTH + 4u);
    T64_CHECK_INT(captured->spans[2].length, 4u + T64_DIGEST_SIZE);
    T64_CHECK_INT(captured->spans[2].class, T64_DIGEST_CLASS_IDENTITY);
    T64_CHECK_INT(read_u32(captured->bytes + SI_DOMAIN_LENGTH + 4u),
                  machine->profile.state_serialization_version);
  }
  /* Ten markers and exactly two new-class spans (the RDP members that end
   * section 8, and the body of section 10); no identity span carries a packet
   * profile or build id. */
  {
    size_t markers = 0u;
    size_t new_spans = 0u;

    for (i = 0u; i < captured->span_count; i++) {
      if (captured->spans[i].class == T64_DIGEST_CLASS_MARKER) markers++;
      if (captured->spans[i].class == T64_DIGEST_CLASS_NEW) new_spans++;
    }
    T64_CHECK_INT(markers, MARKER_COUNT_FULL);
    T64_CHECK_INT(new_spans, 2u);
  }
  /* Hash sink digest equals SHA-256 of the buffer-sink stream. */
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  sha256_of(captured->bytes, captured->length, expected);
  T64_CHECK(memcmp(digest, expected, sizeof digest) == 0);

  /* Overflow: capacity 105 holds the first 95 bytes (domain, marker, serial
   * word, profile digest, content digest, image flag and length); the 64-byte
   * header write would pass it, so it copies nothing. */
  {
    uint8_t small[105];
    t64_digest_sink sink;

    (void)memset(small, 0xee, sizeof small);
    (void)memset(&sink, 0, sizeof sink);
    sink.buffer = small;
    sink.capacity = sizeof small;
    T64_CHECK_INT(t64_digest_stream(machine, &sink), T64_STATUS_OK);
    T64_CHECK(sink.overflow);
    T64_CHECK_INT(sink.offset, captured->length);
    T64_CHECK(memcmp(small, captured->bytes, 95u) == 0);
    for (i = 95u; i < sizeof small; i++) T64_CHECK_INT(small[i], 0xee);
  }
  /* Span array too small: overflow is reported, the stream still completes. */
  {
    t64_digest_span one_span[1];
    t64_digest_sink sink;

    (void)memset(&sink, 0, sizeof sink);
    sink.spans = one_span;
    sink.span_capacity = 1u;
    T64_CHECK_INT(t64_digest_stream(machine, &sink), T64_STATUS_OK);
    T64_CHECK(sink.overflow);
    T64_CHECK_INT(sink.span_count, 1u);
    T64_CHECK_INT(sink.offset, captured->length);
  }
}

/* ---- stream-surgery controls on the real v25 buffer ---------------------- */

/*
 * Each control edits a copy of the real family stream into the exact bytes a
 * walker with one defect would emit, then compares it with the v24 stream of
 * the Phase 8 profile. The comparator must accept the unmodified stream first
 * (the same setup) and then report the intended distinct result kind.
 */
static bool clone_stream(const stream *source, stream *out) {
  (void)memset(out, 0, sizeof *out);
  out->bytes = (uint8_t *)malloc(source->length);
  out->spans = (t64_digest_span *)calloc(SPAN_CAPACITY, sizeof *out->spans);
  if (out->bytes == NULL || out->spans == NULL) {
    free_stream(out);
    return false;
  }
  (void)memcpy(out->bytes, source->bytes, source->length);
  (void)memcpy(out->spans, source->spans,
               source->span_count * sizeof *source->spans);
  out->length = source->length;
  out->span_count = source->span_count;
  return true;
}

/* Remove `count` body bytes at `at`; no span may overlap the cut. */
static bool erase_bytes(stream *captured, size_t at, size_t count) {
  size_t i;

  for (i = 0u; i < captured->span_count; i++) {
    const t64_digest_span *span = &captured->spans[i];

    if (span->offset < at + count && at < span->offset + span->length) {
      return false;
    }
  }
  (void)memmove(captured->bytes + at, captured->bytes + at + count,
                captured->length - at - count);
  captured->length -= count;
  for (i = 0u; i < captured->span_count; i++) {
    if (captured->spans[i].offset >= at + count) {
      captured->spans[i].offset -= count;
    }
  }
  return true;
}

/* Insert an identity-class span (profile text) before `at`. */
static bool insert_identity_span(stream *captured, size_t at,
                                 const uint8_t *bytes, size_t count) {
  uint8_t *grown = (uint8_t *)malloc(captured->length + count);
  size_t i;
  size_t slot = captured->span_count;

  if (grown == NULL || captured->span_count >= SPAN_CAPACITY) {
    free(grown);
    return false;
  }
  (void)memcpy(grown, captured->bytes, at);
  (void)memcpy(grown + at, bytes, count);
  (void)memcpy(grown + at + count, captured->bytes + at,
               captured->length - at);
  free(captured->bytes);
  captured->bytes = grown;
  captured->length += count;
  for (i = 0u; i < captured->span_count; i++) {
    if (captured->spans[i].offset >= at && slot == captured->span_count) {
      slot = i;
    }
  }
  for (i = captured->span_count; i > slot; i--) {
    captured->spans[i] = captured->spans[i - 1u];
    captured->spans[i].offset += count;
  }
  captured->spans[slot].offset = at;
  captured->spans[slot].length = count;
  captured->spans[slot].class = T64_DIGEST_CLASS_IDENTITY;
  captured->span_count++;
  return true;
}

/* Swap section `id` with section `id + 1` (marker and body as one unit). */
static bool swap_adjacent_sections(stream *captured, uint32_t id) {
  size_t first = 0u;
  size_t second = 0u;
  size_t after = 0u;
  size_t first_length;
  size_t second_length;
  uint8_t *copy;
  size_t i;

  if (!find_marker(captured, id, &first) ||
      !find_marker(captured, id + 1u, &second)) {
    return false;
  }
  after = find_marker(captured, id + 2u, &after) ? after : captured->length;
  first_length = second - first;
  second_length = after - second;
  copy = (uint8_t *)malloc(after - first);
  if (copy == NULL) return false;
  (void)memcpy(copy, captured->bytes + second, second_length);
  (void)memcpy(copy + second_length, captured->bytes + first, first_length);
  (void)memcpy(captured->bytes + first, copy, after - first);
  free(copy);
  for (i = 0u; i < captured->span_count; i++) {
    t64_digest_span *span = &captured->spans[i];

    if (span->offset >= first && span->offset < second) {
      span->offset += second_length;
    } else if (span->offset >= second && span->offset < after) {
      span->offset -= first_length;
    }
  }
  /* Keep the span array in stream order. */
  for (i = 1u; i < captured->span_count; i++) {
    size_t j = i;

    while (j > 0u && captured->spans[j - 1u].offset > captured->spans[j].offset) {
      const t64_digest_span swap = captured->spans[j - 1u];

      captured->spans[j - 1u] = captured->spans[j];
      captured->spans[j] = swap;
      j--;
    }
  }
  return true;
}

static void expect_comparison(const char *label, cmp_kind want,
                              const stream *older, const stream *newer) {
  const cmp_result result = compare_stripped(older, newer, MARKER_COUNT_FULL);

  if (result.kind != want) {
    fprintf(stderr,
            "control %s: kind=%d want=%d old_stripped=%zu new_stripped=%zu "
            "offset=%zu\n",
            label, (int)result.kind, (int)want, result.old_length,
            result.new_length, result.offset);
  }
  T64_CHECK_INT(result.kind, want);
}

static void run_surgery_controls(void) {
  t64_machine *older_machine = build_ai_first_packet(AI_PROFILE_ID);
  t64_machine *newer_machine = build_ai_first_packet(SI_STEM_ID);
  t64_machine *older_vi_machine = build_vi_frame(AI_PROFILE_ID);
  t64_machine *newer_vi_machine = build_vi_frame(SI_STEM_ID);
  stream older;
  stream newer;
  stream older_vi;
  stream newer_vi;
  stream edited;
  size_t at = 0u;

  T64_CHECK(older_machine != NULL && newer_machine != NULL &&
            older_vi_machine != NULL && newer_vi_machine != NULL);
  if (older_machine == NULL || newer_machine == NULL ||
      older_vi_machine == NULL || newer_vi_machine == NULL) {
    goto done;
  }
  T64_CHECK(capture_stream(older_machine, &older));
  T64_CHECK(capture_stream(newer_machine, &newer));
  T64_CHECK(capture_stream(older_vi_machine, &older_vi));
  T64_CHECK(capture_stream(newer_vi_machine, &newer_vi));

  /* Accepted baselines in the same setup. */
  expect_comparison("baseline audio state", CMP_EQUAL, &older, &newer);
  expect_comparison("baseline video state", CMP_EQUAL, &older_vi, &newer_vi);

  /* 1. An omitted 8-byte audio field (phase_remainder, 28 bytes into the
   * audio section): the stripped lengths differ. */
  if (clone_stream(&newer, &edited)) {
    T64_CHECK(section_body(&edited, 9u, &at, &(size_t){0u}));
    T64_CHECK(erase_bytes(&edited, at + 28u, 8u));
    expect_comparison("omitted audio field", CMP_LENGTH_MISMATCH, &older,
                      &edited);
    free_stream(&edited);
  }
  /* 2. Two sections swapped (rdp/vi and warm devices are adjacent 7 and 8;
   * swap 6 and 7 here): section ids leave ascending order. */
  if (clone_stream(&newer, &edited)) {
    T64_CHECK(swap_adjacent_sections(&edited, 6u));
    expect_comparison("two sections swapped", CMP_MARKER_ORDER, &older,
                      &edited);
    free_stream(&edited);
  }
  /* 3. One flipped domain byte: framing text is wrong. */
  if (clone_stream(&newer, &edited)) {
    edited.bytes[0] ^= UINT8_C(1);
    expect_comparison("flipped domain byte", CMP_BODY_MISMATCH, &older,
                      &edited);
    free_stream(&edited);
  }
  /* 4. A retained packet profile_id: 64 bytes of identity text inside the
   * retained video packet, where v25 emits none. Stripping hides the bytes,
   * so only the exact-identity-span check can see them. */
  if (clone_stream(&newer_vi, &edited)) {
    static const char policy[] = "rsp62500000-ntsc48681812-halfline-rational-v2";
    uint8_t profile_text[T64_PROFILE_TEXT_CAPACITY];
    size_t found = SIZE_MAX;
    size_t i;

    (void)memset(profile_text, 0, sizeof profile_text);
    (void)memcpy(profile_text, SI_STEM_ID, sizeof SI_STEM_ID);
    for (i = 0u; i + sizeof policy <= edited.length; i++) {
      if (memcmp(edited.bytes + i, policy, sizeof policy) == 0) {
        found = i;
        break;
      }
    }
    T64_CHECK(found != SIZE_MAX);
    if (found != SIZE_MAX) {
      T64_CHECK(insert_identity_span(&edited, found, profile_text,
                                     sizeof profile_text));
      expect_comparison("retained packet profile_id span",
                        CMP_IDENTITY_MISMATCH, &older_vi, &edited);
    }
    free_stream(&edited);
  }
  free_stream(&older);
  free_stream(&newer);
  free_stream(&older_vi);
  free_stream(&newer_vi);
done:
  if (older_machine != NULL) t64_machine_destroy(older_machine);
  if (newer_machine != NULL) t64_machine_destroy(newer_machine);
  if (older_vi_machine != NULL) t64_machine_destroy(older_vi_machine);
  if (newer_vi_machine != NULL) t64_machine_destroy(newer_vi_machine);
}

static void case_v25_strip_differential(void) {
  t64_machine *family = build_idle(SI_STEM_ID);
  stream family_stream;

  differential_for_state("idle", build_idle);
  differential_for_state("ai-mid-dma", build_ai_mid_dma_packet);
  differential_for_state("ai-first-packet", build_ai_first_packet);
  differential_for_state("vi-frame", build_vi_frame);
  differential_for_state("rdp-pending", build_rdp_pending);
  differential_for_save_id("idle e4k", SAVE_E4K_ID, false);
  differential_for_save_id("idle e16k", SAVE_E16K_ID, false);
  differential_for_save_id("idle s32k", SAVE_S32K_ID, false);
  differential_for_save_id("mid-write e4k", SAVE_E4K_ID, true);
  run_surgery_controls();
  T64_CHECK(family != NULL);
  if (family != NULL && capture_stream(family, &family_stream)) {
    check_family_stream(family, &family_stream);
    free_stream(&family_stream);
  }
  if (family != NULL) t64_machine_destroy(family);
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"sink-legacy-identity", "v25-reference",
                                      "v25-strip-differential"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "sink-legacy-identity") == 0) {
      case_sink_legacy_identity();
      return T64_TEST_FINISH("unit.core.digest.sink-legacy-identity");
    }
    if (strcmp(argv[2], "v25-reference") == 0) {
      case_v25_reference();
      return T64_TEST_FINISH("unit.core.digest.v25-reference");
    }
    if (strcmp(argv[2], "v25-strip-differential") == 0) {
      case_v25_strip_differential();
      return T64_TEST_FINISH("unit.core.digest.v25-strip-differential");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_sink_legacy_identity();
  case_v25_reference();
  case_v25_strip_differential();
  return T64_TEST_FINISH("unit.core.digest.v25");
}
