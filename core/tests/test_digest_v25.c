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
  t64_stop_request request = {T64_STOP_GUEST_TICK, 0u};
  t64_stop_result stop = {0u, 0u};
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
#define MARKER_COUNT_FULL 9u

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
 * Compare the older stream with the newer one after removing framing:
 * length first, then marker order of the newer stream, then body bytes.
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
  t64_machine *machine = create_admitted(profile_id, true);

  if (machine == NULL) return NULL;
  install_spin_loop(machine);
  T64_CHECK(advance_to(machine, 64u));
  T64_CHECK_INT(machine->tick, 64u);
  T64_CHECK(machine->cpu.retired_instructions > 0u);
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

static void case_v25_strip_differential(void) {
  differential_for_state("idle", build_idle);
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"sink-legacy-identity",
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
  case_v25_strip_differential();
  return T64_TEST_FINISH("unit.core.digest.v25");
}
