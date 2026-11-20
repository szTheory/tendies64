/*
 * State container export tests (plan 09-08).
 *
 * The decoder in this file is written from the container layout only, with
 * literal offsets and its own big-endian readers. It never calls a production
 * helper and never includes the production section or container constants for
 * offsets, so a layout mistake in core/t64_state.c cannot be hidden by a
 * matching mistake here. The hashes are integrity checks, not authentication:
 * a blob whose hashes were recomputed after tampering still decodes.
 *
 * Every input is authored or the already cleared openIPL3 asset; no ROM, save,
 * firmware image, hardware measurement or oracle is used.
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
#define SI_STEM_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"

/* ---- machine construction (copied from core/tests/test_digest_v25.c) ---- */

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

/* Create the machine for an id with the given allocator and admit the cleared
 * openIPL3 asset. */
static t64_machine *create_admitted_with(const t64_host_allocator *host,
                                         const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  t64_status status;

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(host, profile, &machine), T64_STATUS_OK);
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

static t64_machine *create_admitted(const char *profile_id) {
  return create_admitted_with(&allocator, profile_id);
}

static void install_spin_loop(t64_machine *machine) {
  machine->ram[UINT32_C(0x1000)] = 0x08u;
  machine->ram[UINT32_C(0x1001)] = 0x00u;
  machine->ram[UINT32_C(0x1002)] = 0x04u;
  machine->ram[UINT32_C(0x1003)] = 0x00u;
  (void)memset(machine->ram + UINT32_C(0x1004), 0, 4u);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
}

/* The tick reported by the last stop result of advance_to, so the header tick
 * is compared with a value the machine reported through its public result. */
static uint64_t last_stop_tick;

static bool advance_to(t64_machine *machine, uint64_t tick) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
  t64_stop_result stop = {0u, 0u};
  const t64_status status = t64_machine_advance(machine, &request, &stop);

  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
  last_stop_tick = stop.tick;
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

/* ---- the other ids and the authored states (copied from -------------- */
/* ---- core/tests/test_digest_v25.c, each guarded by its reached check) --- */

#define AI_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"
#define SAVE_E4K_ID SI_STEM_ID "-e4k"
#define SAVE_E16K_ID SI_STEM_ID "-e16k"
#define SAVE_S32K_ID SI_STEM_ID "-s32k"
#define AI_DRAM_ADDR UINT32_C(0x04500000)
#define AI_LEN UINT32_C(0x04500004)
#define AI_CONTROL UINT32_C(0x04500008)
#define AI_DACRATE UINT32_C(0x04500010)
#define AI_BITRATE UINT32_C(0x04500014)
#define VI_BASE UINT32_C(0x04400000)
#define VI_ORIGIN UINT32_C(0x3000)
#define DP_START_REGISTER UINT32_C(0x04100000)
#define DP_END_REGISTER UINT32_C(0x04100004)
#define ISVIEWER_BASE UINT32_C(0x13ff0000)

typedef t64_machine *(*state_builder)(const char *profile_id);

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
  t64_machine *machine = create_admitted(profile_id);

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
 * built and no packet is complete. */
static t64_machine *build_ai_mid_dma_packet(const char *profile_id) {
  t64_machine *machine = start_ai_dma_pair(profile_id);

  if (machine == NULL) return NULL;
  T64_CHECK(dispatch_ai_samples(machine, 1u));
  T64_CHECK(machine->ai.active);
  T64_CHECK_INT(machine->ai.sample_index, 1u);
  T64_CHECK_INT(machine->ai.packet_length, 4u);
  T64_CHECK_INT(machine->ai.packet_history_count, 0u);
  T64_CHECK_INT(machine->ai.packet_count, 0u);
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

/* An RDP command list of four sync words submitted and one work event
 * dispatched: the first word is fetched and pending, three words remain. */
static t64_machine *build_rdp_pending(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id);
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
  T64_CHECK(pending);
  T64_CHECK(machine->rdp.command_pending);
  T64_CHECK(machine->rdp.pending_command_word != 0u);
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x108));
  T64_CHECK(machine->rdp.dpc_current < machine->rdp.dpc_end);
  return machine;
}

/* ---- observer progress through public APIs only ------------------------ */

static void isviewer_emit(t64_machine *machine, const uint8_t *bytes,
                          size_t count) {
  size_t i;

  for (i = 0u; i + 4u <= count; i += 4u) {
    const uint32_t word = ((uint32_t)bytes[i] << 24) |
                          ((uint32_t)bytes[i + 1u] << 16) |
                          ((uint32_t)bytes[i + 2u] << 8) |
                          (uint32_t)bytes[i + 3u];

    T64_CHECK_INT(t64_machine_bus_write(machine,
                                        ISVIEWER_BASE + UINT32_C(0x20) +
                                            (uint32_t)i,
                                        4u, word),
                  T64_STATUS_OK);
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, ISVIEWER_BASE + UINT32_C(0x14),
                                      4u, (uint32_t)count),
                T64_STATUS_OK);
}

/* One full dequeue of the oldest video or audio packet. */
static bool dequeue_packet(t64_machine *machine, bool video) {
  size_t required = 0u;
  size_t copied = 0u;
  uint8_t *bytes;
  t64_status status;

  status = video ? t64_video_copy(machine, NULL, 0u, &required)
                 : t64_audio_copy(machine, NULL, 0u, &required);
  T64_CHECK_INT(status, T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK(required > 0u);
  if (status != T64_STATUS_BUFFER_TOO_SMALL || required == 0u) return false;
  bytes = (uint8_t *)malloc(required);
  if (bytes == NULL) {
    T64_CHECK(false);
    return false;
  }
  status = video ? t64_video_copy(machine, bytes, required, &copied)
                 : t64_audio_copy(machine, bytes, required, &copied);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(copied, required);
  free(bytes);
  return status == T64_STATUS_OK;
}

/* ---- independent reference decoder: literal offsets only --------------- */

#define D_MAGIC 0x000u
#define D_CONTAINER_VERSION 0x008u
#define D_STATE_VERSION 0x00Cu
#define D_TOTAL 0x010u
#define D_COUNT 0x018u
#define D_FLAGS 0x01Cu
#define D_PROFILE 0x020u
#define D_CONTENT_SHA 0x040u
#define D_CONTENT_LEN 0x060u
#define D_TICK 0x068u
#define D_DECLARED 0x070u
#define D_CANON_OFF 0x090u
#define D_CANON_LEN 0x098u
#define D_BUILD 0x0A0u
#define D_BUILD_BYTES 128u
#define D_TABLE 0x120u
#define D_ENTRY 56u
#define E_ID 0u
#define E_FLAGS 4u
#define E_OFF 8u
#define E_LEN 16u
#define E_HASH 24u
#define D_OBSERVER_BYTES 40u
#define D_TRAILER_BYTES 32u
#define D_MAX_SECTIONS 32u
#define D_DOMAIN "t64.state.si.25"
#define D_DOMAIN_BYTES 15u
#define D_OBSERVER_ID 16u

typedef enum {
  DEC_OK = 0,
  DEC_MAGIC,
  DEC_CONTAINER_VERSION,
  DEC_TOTAL_LENGTH,
  DEC_SECTION_COUNT,
  DEC_FLAGS,
  DEC_TABLE_ORDER,
  DEC_SECTION_RANGE,
  DEC_MARKER,
  DEC_SECTION_HASH,
  DEC_CANONICAL_DIGEST,
  DEC_HEADER_IDENTITY,
  DEC_OBSERVER,
  DEC_TRAILER
} dec_kind;

static const char *const dec_names[] = {
    "ok",           "magic",        "container-version", "total-length",
    "section-count", "flags",       "table-order",       "section-range",
    "marker",       "section-hash", "canonical-digest",  "header-identity",
    "observer",     "trailer"};

typedef struct {
  uint32_t state_version;
  uint8_t profile_digest[T64_DIGEST_SIZE];
  uint8_t content_digest[T64_DIGEST_SIZE];
  uint64_t content_length;
  uint64_t tick;
  char build_id[D_BUILD_BYTES];
} dec_expect;

typedef struct {
  uint32_t container_version;
  uint32_t state_version;
  uint64_t total;
  uint32_t count;
  uint32_t flags;
  uint8_t profile_digest[T64_DIGEST_SIZE];
  uint8_t content_digest[T64_DIGEST_SIZE];
  uint64_t content_length;
  uint64_t tick;
  uint8_t declared[T64_DIGEST_SIZE];
  uint64_t canonical_offset;
  uint64_t canonical_length;
  uint8_t build_id[D_BUILD_BYTES];
  uint32_t ids[D_MAX_SECTIONS];
  uint32_t entry_flags[D_MAX_SECTIONS];
  uint64_t offsets[D_MAX_SECTIONS];
  uint64_t lengths[D_MAX_SECTIONS];
  uint64_t observer[5];
} dec_info;

static uint32_t rd32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t rd64(const uint8_t *p) {
  return ((uint64_t)rd32(p) << 32) | (uint64_t)rd32(p + 4);
}

static void sha256_of(const uint8_t *bytes, size_t length,
                      uint8_t out[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;

  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, out);
}

static bool bytes_equal(const uint8_t *left, const uint8_t *right,
                        size_t length) {
  return memcmp(left, right, length) == 0;
}

/* Decode and verify one container; the first failing check names the kind.
 * The check order is the order the controls rely on: header sanity, table
 * order, observer entry shape, canonical ranges and markers, body hashes,
 * the declared digest, the header identity, then the trailer. */
static dec_kind decode_container(const uint8_t *blob, size_t length,
                                 const dec_expect *expect, dec_info *info) {
  uint8_t digest[T64_DIGEST_SIZE];
  uint64_t canonical_end;
  uint64_t next_marker;
  uint32_t i;
  size_t j;

  (void)memset(info, 0, sizeof *info);
  if (length < D_TABLE) return DEC_TOTAL_LENGTH;
  if (memcmp(blob + D_MAGIC, "T64STATE", 8u) != 0) return DEC_MAGIC;
  info->container_version = rd32(blob + D_CONTAINER_VERSION);
  if (info->container_version != 1u) return DEC_CONTAINER_VERSION;
  info->total = rd64(blob + D_TOTAL);
  if (info->total != (uint64_t)length) return DEC_TOTAL_LENGTH;
  info->count = rd32(blob + D_COUNT);
  if (info->count < 2u || info->count > D_MAX_SECTIONS ||
      D_TABLE + (uint64_t)D_ENTRY * info->count > (uint64_t)length) {
    return DEC_SECTION_COUNT;
  }
  info->flags = rd32(blob + D_FLAGS);
  if (info->flags != 0u) return DEC_FLAGS;
  info->state_version = rd32(blob + D_STATE_VERSION);
  (void)memcpy(info->profile_digest, blob + D_PROFILE, T64_DIGEST_SIZE);
  (void)memcpy(info->content_digest, blob + D_CONTENT_SHA, T64_DIGEST_SIZE);
  info->content_length = rd64(blob + D_CONTENT_LEN);
  info->tick = rd64(blob + D_TICK);
  (void)memcpy(info->declared, blob + D_DECLARED, T64_DIGEST_SIZE);
  info->canonical_offset = rd64(blob + D_CANON_OFF);
  info->canonical_length = rd64(blob + D_CANON_LEN);
  (void)memcpy(info->build_id, blob + D_BUILD, D_BUILD_BYTES);
  for (i = 0u; i < info->count; i++) {
    const uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;

    info->ids[i] = rd32(entry + E_ID);
    info->entry_flags[i] = rd32(entry + E_FLAGS);
    info->offsets[i] = rd64(entry + E_OFF);
    info->lengths[i] = rd64(entry + E_LEN);
  }
  for (i = 1u; i < info->count; i++) {
    if (info->ids[i] <= info->ids[i - 1u]) return DEC_TABLE_ORDER;
  }
  if (info->canonical_offset != D_TABLE + (uint64_t)D_ENTRY * info->count ||
      info->canonical_length >
          (uint64_t)length - info->canonical_offset ||
      info->canonical_offset + info->canonical_length + D_OBSERVER_BYTES +
              D_TRAILER_BYTES !=
          (uint64_t)length) {
    return DEC_SECTION_RANGE;
  }
  canonical_end = info->canonical_offset + info->canonical_length;
  i = info->count - 1u;
  if (info->ids[i] != D_OBSERVER_ID || info->entry_flags[i] != 1u ||
      info->offsets[i] != canonical_end ||
      info->lengths[i] != D_OBSERVER_BYTES) {
    return DEC_OBSERVER;
  }
  for (i = 0u; i + 1u < info->count; i++) {
    if (info->entry_flags[i] != 0u) return DEC_FLAGS;
  }
  if (info->canonical_length < D_DOMAIN_BYTES ||
      memcmp(blob + info->canonical_offset, D_DOMAIN, D_DOMAIN_BYTES) != 0) {
    return DEC_MARKER;
  }
  next_marker = info->canonical_offset + D_DOMAIN_BYTES;
  for (i = 0u; i + 1u < info->count; i++) {
    /* The id sits four bytes before each body; then the bodies are
     * contiguous: the next marker starts where the previous body ends. */
    if (info->offsets[i] < info->canonical_offset + 4u ||
        info->offsets[i] > canonical_end) {
      return DEC_SECTION_RANGE;
    }
    if (rd32(blob + info->offsets[i] - 4u) != info->ids[i]) return DEC_MARKER;
    if (info->offsets[i] != next_marker + 4u ||
        info->lengths[i] > canonical_end - info->offsets[i]) {
      return DEC_SECTION_RANGE;
    }
    next_marker = info->offsets[i] + info->lengths[i];
  }
  if (next_marker != canonical_end) return DEC_SECTION_RANGE;
  for (i = 0u; i < info->count; i++) {
    const uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;

    sha256_of(blob + info->offsets[i], (size_t)info->lengths[i], digest);
    if (!bytes_equal(digest, entry + E_HASH, T64_DIGEST_SIZE)) {
      return DEC_SECTION_HASH;
    }
  }
  sha256_of(blob + info->canonical_offset, (size_t)info->canonical_length,
            digest);
  if (!bytes_equal(digest, info->declared, T64_DIGEST_SIZE)) {
    return DEC_CANONICAL_DIGEST;
  }
  /* The header agrees with the bodies it summarizes: the identity body starts
   * with the state serialization version, the profile digest and the content
   * digest; the machine-core body starts with the guest tick. */
  if (info->ids[0] != 1u || info->ids[1] != 2u || info->lengths[0] < 76u ||
      info->lengths[1] < 8u ||
      rd32(blob + info->offsets[0]) != info->state_version ||
      !bytes_equal(blob + info->offsets[0] + 4u, info->profile_digest,
                   T64_DIGEST_SIZE) ||
      !bytes_equal(blob + info->offsets[0] + 36u, info->content_digest,
                   T64_DIGEST_SIZE) ||
      rd32(blob + info->offsets[0] + 72u) != info->content_length ||
      rd64(blob + info->offsets[1]) != info->tick) {
    return DEC_HEADER_IDENTITY;
  }
  if (expect != NULL) {
    size_t build_length = 0u;

    while (build_length < D_BUILD_BYTES &&
           expect->build_id[build_length] != '\0') {
      build_length++;
    }
    if (info->state_version != expect->state_version ||
        !bytes_equal(info->profile_digest, expect->profile_digest,
                     T64_DIGEST_SIZE) ||
        !bytes_equal(info->content_digest, expect->content_digest,
                     T64_DIGEST_SIZE) ||
        info->content_length != expect->content_length ||
        info->tick != expect->tick || build_length >= D_BUILD_BYTES ||
        memcmp(info->build_id, expect->build_id, build_length) != 0) {
      return DEC_HEADER_IDENTITY;
    }
    /* NUL padded to the end, and no path character anywhere in the field. */
    for (j = build_length; j < D_BUILD_BYTES; j++) {
      if (info->build_id[j] != 0) return DEC_HEADER_IDENTITY;
    }
    for (j = 0u; j < D_BUILD_BYTES; j++) {
      if (info->build_id[j] == '/' || info->build_id[j] == '\\') {
        return DEC_HEADER_IDENTITY;
      }
    }
  }
  for (j = 0u; j < 5u; j++) {
    info->observer[j] = rd64(blob + canonical_end + 8u * j);
  }
  sha256_of(blob, length - D_TRAILER_BYTES, digest);
  if (!bytes_equal(digest, blob + length - D_TRAILER_BYTES,
                   D_TRAILER_BYTES)) {
    return DEC_TRAILER;
  }
  return DEC_OK;
}

/* Expected header values, all read through the public API. */
static bool make_expect(const t64_machine *machine, uint64_t tick,
                        dec_expect *expect) {
  const t64_profile_descriptor *profile = NULL;
  size_t rom_size = 0u;
  const char *build = t64_build_identity();

  (void)memset(expect, 0, sizeof *expect);
  T64_CHECK_INT(t64_machine_profile(machine, &profile), T64_STATUS_OK);
  if (profile == NULL) return false;
  T64_CHECK_INT(t64_profile_hash(profile, expect->profile_digest),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_content_digest(machine, expect->content_digest),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_rom_size(machine, &rom_size), T64_STATUS_OK);
  expect->state_version = profile->state_serialization_version;
  expect->content_length = (uint64_t)rom_size;
  expect->tick = tick;
  T64_CHECK(strlen(build) < D_BUILD_BYTES);
  if (strlen(build) < D_BUILD_BYTES) {
    (void)memcpy(expect->build_id, build, strlen(build));
  }
  return true;
}

/* ---- case export-layout (the tracer) ----------------------------------- */

#define CANARY_BYTES 64u
#define CANARY_FILL 0xc3u

/* Section ids of the stem id (the section 11 save-media section exists for
 * the three save ids only), then the observer section 16. */
static const uint32_t stem_ids[] = {1u, 2u, 3u, 4u, 5u,
                                    6u, 7u, 8u, 9u, 10u, 16u};

static void case_export_layout(void) {
  t64_machine *machine = build_idle(SI_STEM_ID);
  size_t required = 0u;
  size_t written = 0u;
  uint8_t *buffer;
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t canonical[T64_DIGEST_SIZE];
  dec_expect expect;
  dec_info info;
  dec_kind kind;
  t64_status status;
  uint32_t i;
  size_t k;

  if (machine == NULL) return;
  status = t64_machine_state_export(machine, NULL, 0u, &required);
  T64_CHECK_INT(status, T64_STATUS_BUFFER_TOO_SMALL);
  /* Independent arithmetic bounds: strictly above the major arrays alone
   * (RDRAM section 4,194,308, one 2,097,152-byte RDP coverage array, the
   * 4,096-byte TMEM and the 8,200-byte SP memory section) and below 11 MiB
   * (the same plus retained packet slots). */
  T64_CHECK(required > 6303756u);
  T64_CHECK(required < 11534336u);
  if (status != T64_STATUS_BUFFER_TOO_SMALL || required < D_TABLE) {
    t64_machine_destroy(machine);
    return;
  }
  buffer = (uint8_t *)malloc(required + CANARY_BYTES);
  if (buffer == NULL) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  (void)memset(buffer, 0, required);
  (void)memset(buffer + required, CANARY_FILL, CANARY_BYTES);
  status = t64_machine_state_export(machine, buffer, required, &written);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(written, required);
  for (k = 0u; k < CANARY_BYTES; k++) {
    T64_CHECK_INT(buffer[required + k], CANARY_FILL);
  }
  if (status == T64_STATUS_OK && written == required) {
    T64_CHECK(make_expect(machine, last_stop_tick, &expect));
    kind = decode_container(buffer, required, &expect, &info);
    if (kind != DEC_OK) {
      fprintf(stderr, "export-layout: decoder kind %s\n", dec_names[kind]);
    }
    T64_CHECK_INT(kind, DEC_OK);
    T64_CHECK_INT(info.container_version, 1);
    T64_CHECK_INT(info.state_version, 25);
    T64_CHECK_INT(info.flags, 0);
    T64_CHECK_INT(info.tick, 64);
    T64_CHECK_INT(info.count, sizeof stem_ids / sizeof stem_ids[0]);
    for (i = 0u; i < info.count && i < sizeof stem_ids / sizeof stem_ids[0];
         i++) {
      T64_CHECK_INT(info.ids[i], stem_ids[i]);
    }
    T64_CHECK_INT(info.entry_flags[info.count - 1u], 1);
    T64_CHECK_INT(info.lengths[info.count - 1u], 40);
    /* Literal body lengths: machine core 32, RDRAM 4,194,308, SP memory
     * 8,200 (ids 2, 3 and 4). */
    T64_CHECK_INT(info.lengths[1], 32);
    T64_CHECK_INT(info.lengths[2], 4194308);
    T64_CHECK_INT(info.lengths[3], 8200);
    /* The declared digest is the machine digest and the hash of the
     * canonical region. */
    T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
    T64_CHECK(bytes_equal(info.declared, digest, T64_DIGEST_SIZE));
    sha256_of(buffer + info.canonical_offset, (size_t)info.canonical_length,
              canonical);
    T64_CHECK(bytes_equal(canonical, digest, T64_DIGEST_SIZE));
    /* Nothing was drained or dequeued: the observer values are zero. */
    for (k = 0u; k < 5u; k++) {
      T64_CHECK_INT(info.observer[k], 0);
    }
  }
  free(buffer);
  t64_machine_destroy(machine);
}

/* ---- shared export and decode helpers ---------------------------------- */

static const uint32_t save_ids[] = {1u, 2u, 3u, 4u, 5u,  6u,
                                    7u, 8u, 9u, 10u, 11u, 16u};

/* Size query, then exact copy-out into a fresh buffer; NULL on any defect. */
static uint8_t *export_blob(const t64_machine *machine, size_t *length_out) {
  size_t required = 0u;
  size_t written = 0u;
  uint8_t *buffer;
  t64_status status;

  *length_out = 0u;
  status = t64_machine_state_export(machine, NULL, 0u, &required);
  T64_CHECK_INT(status, T64_STATUS_BUFFER_TOO_SMALL);
  if (status != T64_STATUS_BUFFER_TOO_SMALL) return NULL;
  buffer = (uint8_t *)malloc(required);
  if (buffer == NULL) {
    T64_CHECK(false);
    return NULL;
  }
  status = t64_machine_state_export(machine, buffer, required, &written);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(written, required);
  if (status != T64_STATUS_OK || written != required) {
    free(buffer);
    return NULL;
  }
  *length_out = written;
  return buffer;
}

/* Decode one blob against the public API and check its id list and that the
 * declared digest equals both the machine digest and the hash of the
 * canonical region. */
static bool check_blob(const char *label, const t64_machine *machine,
                       uint64_t tick, const uint32_t *ids, size_t id_count,
                       const uint8_t *blob, size_t length, dec_info *info) {
  dec_expect expect;
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t canonical[T64_DIGEST_SIZE];
  dec_kind kind;
  size_t i;

  if (!make_expect(machine, tick, &expect)) return false;
  kind = decode_container(blob, length, &expect, info);
  if (kind != DEC_OK) {
    fprintf(stderr, "%s: decoder kind %s\n", label, dec_names[kind]);
  }
  T64_CHECK_INT(kind, DEC_OK);
  if (kind != DEC_OK) return false;
  T64_CHECK_INT(info->count, id_count);
  for (i = 0u; i < id_count && i < info->count; i++) {
    T64_CHECK_INT(info->ids[i], ids[i]);
  }
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  T64_CHECK(bytes_equal(info->declared, digest, T64_DIGEST_SIZE));
  sha256_of(blob + info->canonical_offset, (size_t)info->canonical_length,
            canonical);
  T64_CHECK(bytes_equal(canonical, digest, T64_DIGEST_SIZE));
  return true;
}

/* ---- case decoder-controls --------------------------------------------- */

static void wr32(uint8_t *p, uint32_t value) {
  p[0] = (uint8_t)(value >> 24);
  p[1] = (uint8_t)(value >> 16);
  p[2] = (uint8_t)(value >> 8);
  p[3] = (uint8_t)value;
}

static void wr64(uint8_t *p, uint64_t value) {
  wr32(p, (uint32_t)(value >> 32));
  wr32(p + 4, (uint32_t)value);
}

static uint8_t *entry_at(uint8_t *blob, uint32_t index) {
  return blob + D_TABLE + (size_t)D_ENTRY * index;
}

static void fix_entry_hash(uint8_t *blob, const dec_info *info,
                           uint32_t index) {
  sha256_of(blob + info->offsets[index], (size_t)info->lengths[index],
            entry_at(blob, index) + E_HASH);
}

static void fix_declared(uint8_t *blob, const dec_info *info) {
  sha256_of(blob + info->canonical_offset, (size_t)info->canonical_length,
            blob + D_DECLARED);
}

static void fix_trailer(uint8_t *blob, size_t length) {
  sha256_of(blob, length - D_TRAILER_BYTES, blob + length - D_TRAILER_BYTES);
}

typedef void (*surgery_fn)(uint8_t *blob, size_t length,
                           const dec_info *info);

static void flip_body_byte(uint8_t *blob, const dec_info *info) {
  /* Section index 2 is the RDRAM body on every id. */
  blob[info->offsets[2] + 100u] = (uint8_t)(blob[info->offsets[2] + 100u] ^ 1u);
}

static void surgery_body_byte(uint8_t *blob, size_t length,
                              const dec_info *info) {
  (void)length;
  flip_body_byte(blob, info);
}

static void surgery_length_plus_one(uint8_t *blob, size_t length,
                                    const dec_info *info) {
  (void)length;
  wr64(entry_at(blob, 1u) + E_LEN, info->lengths[1] + 1u);
}

static void surgery_swap_ids(uint8_t *blob, size_t length,
                             const dec_info *info) {
  (void)length;
  wr32(entry_at(blob, 3u) + E_ID, info->ids[4]);
  wr32(entry_at(blob, 4u) + E_ID, info->ids[3]);
}

static void surgery_header_flags(uint8_t *blob, size_t length,
                                 const dec_info *info) {
  (void)length;
  (void)info;
  wr32(blob + D_FLAGS, 1u);
}

static void surgery_trailer_byte(uint8_t *blob, size_t length,
                                 const dec_info *info) {
  (void)info;
  blob[length - 1u] = (uint8_t)(blob[length - 1u] ^ 1u);
}

static void surgery_total_plus_one(uint8_t *blob, size_t length,
                                   const dec_info *info) {
  (void)info;
  wr64(blob + D_TOTAL, (uint64_t)length + 1u);
}

static void surgery_marker_byte(uint8_t *blob, size_t length,
                                const dec_info *info) {
  (void)length;
  blob[info->offsets[3] - 1u] = (uint8_t)(blob[info->offsets[3] - 1u] ^ 1u);
}

static void surgery_observer_flags(uint8_t *blob, size_t length,
                                   const dec_info *info) {
  (void)length;
  wr32(entry_at(blob, info->count - 1u) + E_FLAGS, 0u);
}

/* Body byte flipped, its entry hash and the trailer recomputed, the declared
 * digest left alone. */
static void surgery_declared_left(uint8_t *blob, size_t length,
                                  const dec_info *info) {
  flip_body_byte(blob, info);
  fix_entry_hash(blob, info, 2u);
  fix_trailer(blob, length);
}

/* Everything recomputed after the flip: the integrity-only limit. */
static void surgery_forged(uint8_t *blob, size_t length,
                           const dec_info *info) {
  flip_body_byte(blob, info);
  fix_entry_hash(blob, info, 2u);
  fix_declared(blob, info);
  fix_trailer(blob, length);
}

static void surgery_magic(uint8_t *blob, size_t length,
                          const dec_info *info) {
  (void)length;
  (void)info;
  blob[0] = (uint8_t)(blob[0] ^ 1u);
}

static void surgery_container_version(uint8_t *blob, size_t length,
                                      const dec_info *info) {
  (void)length;
  (void)info;
  wr32(blob + D_CONTAINER_VERSION, 2u);
}

static void surgery_count_zero(uint8_t *blob, size_t length,
                               const dec_info *info) {
  (void)length;
  (void)info;
  wr32(blob + D_COUNT, 0u);
}

static void surgery_count_33(uint8_t *blob, size_t length,
                             const dec_info *info) {
  (void)length;
  (void)info;
  wr32(blob + D_COUNT, 33u);
}

static void surgery_header_tick(uint8_t *blob, size_t length,
                                const dec_info *info) {
  (void)length;
  wr64(blob + D_TICK, info->tick + 1u);
}

static void surgery_profile_digest(uint8_t *blob, size_t length,
                                   const dec_info *info) {
  (void)length;
  (void)info;
  blob[D_PROFILE] = (uint8_t)(blob[D_PROFILE] ^ 1u);
}

static void run_surgery(const char *label, dec_kind want, const uint8_t *base,
                        size_t length, const dec_info *base_info,
                        const dec_expect *expect, surgery_fn apply) {
  uint8_t *copy = (uint8_t *)malloc(length);
  dec_info info;
  dec_kind kind;

  if (copy == NULL) {
    T64_CHECK(false);
    return;
  }
  (void)memcpy(copy, base, length);
  apply(copy, length, base_info);
  kind = decode_container(copy, length, expect, &info);
  if (kind != want) {
    fprintf(stderr, "%s: decoder kind %s want %s\n", label, dec_names[kind],
            dec_names[want]);
  }
  T64_CHECK_INT(kind, want);
  free(copy);
}

static void case_decoder_controls(void) {
  t64_machine *machine = build_idle(SI_STEM_ID);
  size_t length = 0u;
  uint8_t *blob;
  dec_expect expect;
  dec_info info;
  uint8_t digest[T64_DIGEST_SIZE];

  if (machine == NULL) return;
  blob = export_blob(machine, &length);
  if (blob == NULL || !make_expect(machine, last_stop_tick, &expect)) {
    free(blob);
    t64_machine_destroy(machine);
    return;
  }
  /* The accepted baseline of this setup. */
  T64_CHECK_INT(decode_container(blob, length, &expect, &info), DEC_OK);
  run_surgery("body byte", DEC_SECTION_HASH, blob, length, &info, &expect,
              surgery_body_byte);
  run_surgery("length plus one", DEC_SECTION_RANGE, blob, length, &info,
              &expect, surgery_length_plus_one);
  run_surgery("swapped ids", DEC_TABLE_ORDER, blob, length, &info, &expect,
              surgery_swap_ids);
  run_surgery("header flags", DEC_FLAGS, blob, length, &info, &expect,
              surgery_header_flags);
  run_surgery("trailer byte", DEC_TRAILER, blob, length, &info, &expect,
              surgery_trailer_byte);
  run_surgery("total plus one", DEC_TOTAL_LENGTH, blob, length, &info,
              &expect, surgery_total_plus_one);
  run_surgery("marker byte", DEC_MARKER, blob, length, &info, &expect,
              surgery_marker_byte);
  run_surgery("observer flags", DEC_OBSERVER, blob, length, &info, &expect,
              surgery_observer_flags);
  run_surgery("declared digest left", DEC_CANONICAL_DIGEST, blob, length,
              &info, &expect, surgery_declared_left);
  run_surgery("magic", DEC_MAGIC, blob, length, &info, &expect,
              surgery_magic);
  run_surgery("container version", DEC_CONTAINER_VERSION, blob, length, &info,
              &expect, surgery_container_version);
  run_surgery("section count zero", DEC_SECTION_COUNT, blob, length, &info,
              &expect, surgery_count_zero);
  run_surgery("section count 33", DEC_SECTION_COUNT, blob, length, &info,
              &expect, surgery_count_33);
  run_surgery("header tick", DEC_HEADER_IDENTITY, blob, length, &info,
              &expect, surgery_header_tick);
  run_surgery("profile digest", DEC_HEADER_IDENTITY, blob, length, &info,
              &expect, surgery_profile_digest);
  /* Integrity, not authentication: a flipped body with every hash, the
   * declared digest and the trailer recomputed still decodes. Semantic
   * refusal belongs to the importer; the machine digest tells it apart. */
  {
    uint8_t *forged = (uint8_t *)malloc(length);
    dec_info forged_info;

    if (forged == NULL) {
      T64_CHECK(false);
    } else {
      (void)memcpy(forged, blob, length);
      surgery_forged(forged, length, &info);
      T64_CHECK_INT(decode_container(forged, length, &expect, &forged_info),
                    DEC_OK);
      T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
      T64_CHECK(!bytes_equal(forged_info.declared, digest, T64_DIGEST_SIZE));
      free(forged);
    }
  }
  free(blob);
  t64_machine_destroy(machine);
}

/* ---- case states-and-ids ------------------------------------------------ */

typedef struct {
  const char *label;
  state_builder build;
} state_row;

static void case_states_and_ids(void) {
  static const state_row stem_states[] = {
      {"idle", build_idle},
      {"ai-mid-dma", build_ai_mid_dma_packet},
      {"ai-first-packet", build_ai_first_packet},
      {"vi-retained-packet", build_vi_frame},
      {"rdp-pending", build_rdp_pending}};
  static const char *const save_profiles[] = {SAVE_E4K_ID, SAVE_E16K_ID,
                                              SAVE_S32K_ID};
  size_t idle_length = 0u;
  size_t vi_length = 0u;
  size_t i;
  uint8_t sentinel[64];
  uint8_t before[64];
  size_t length_value = 0x1234u;
  t64_machine *legacy;

  for (i = 0u; i < sizeof stem_states / sizeof stem_states[0]; i++) {
    t64_machine *machine = stem_states[i].build(SI_STEM_ID);
    const uint64_t tick = last_stop_tick;
    size_t length = 0u;
    uint8_t *blob;
    dec_info info;

    if (machine == NULL) continue;
    blob = export_blob(machine, &length);
    if (blob != NULL) {
      T64_CHECK(check_blob(stem_states[i].label, machine, tick, stem_ids,
                           sizeof stem_ids / sizeof stem_ids[0], blob, length,
                           &info));
      T64_CHECK_INT(machine->tick, tick);
      if (i == 0u) idle_length = length;
      if (i == 3u) vi_length = length;
      fprintf(stderr, "states-and-ids: %s length %zu\n", stem_states[i].label,
              length);
    }
    free(blob);
    t64_machine_destroy(machine);
  }
  /* A retained video packet makes the blob larger than idle. */
  T64_CHECK(idle_length > 0u && vi_length > idle_length);
  for (i = 0u; i < sizeof save_profiles / sizeof save_profiles[0]; i++) {
    t64_machine *machine = build_idle(save_profiles[i]);
    size_t length = 0u;
    uint8_t *blob;
    dec_info info;

    if (machine == NULL) continue;
    blob = export_blob(machine, &length);
    if (blob != NULL) {
      T64_CHECK(check_blob(save_profiles[i], machine, last_stop_tick,
                           save_ids, sizeof save_ids / sizeof save_ids[0],
                           blob, length, &info));
      /* Section 11 carries the save storage: 20 bytes plus the storage. */
      T64_CHECK(info.lengths[10] > 20u);
    }
    free(blob);
    t64_machine_destroy(machine);
  }
  /* A legacy id (the Phase 8 AI profile) is refused, outputs unchanged. */
  legacy = create_admitted(AI_PROFILE_ID);
  if (legacy != NULL) {
    (void)memset(sentinel, 0xa7, sizeof sentinel);
    (void)memcpy(before, sentinel, sizeof sentinel);
    T64_CHECK_INT(t64_machine_state_export(legacy, sentinel, sizeof sentinel,
                                           &length_value),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(length_value, 0x1234);
    T64_CHECK(bytes_equal(sentinel, before, sizeof sentinel));
    T64_CHECK_INT(t64_machine_state_export(legacy, NULL, 0u, &length_value),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(length_value, 0x1234);
    t64_machine_destroy(legacy);
  }
}

/* ---- case observer-continuity ------------------------------------------ */

static const uint8_t isviewer_bytes[24] = {
    'o', 'b', 's', 'e', 'r', 'v', 'e', 'r', '-', 'c', 'o', 'n',
    't', 'i', 'n', 'u', 'i', 't', 'y', '!', '0', '9', '0', '8'};

/* The VI retained-packet machine with 24 captured ISViewer bytes; with
 * progress, 8 bytes are drained (cursor 8) and the video packet is dequeued. */
static t64_machine *build_video_observer(bool progress) {
  t64_machine *machine = build_vi_frame(SI_STEM_ID);
  uint8_t drained[32];
  t64_output_drain_result drain;

  if (machine == NULL) return NULL;
  T64_CHECK_INT(machine->vi.packet_head, 0);
  T64_CHECK_INT(machine->vi.packet_count, 1);
  isviewer_emit(machine, isviewer_bytes, 8u);
  if (progress) {
    (void)memset(&drain, 0, sizeof drain);
    T64_CHECK_INT(
        t64_machine_output_drain(machine, drained, sizeof drained, &drain),
        T64_STATUS_OK);
    T64_CHECK_INT(drain.copied_bytes, 8);
  }
  isviewer_emit(machine, isviewer_bytes + 8u, 16u);
  if (progress) T64_CHECK(dequeue_packet(machine, true));
  return machine;
}

static t64_machine *build_audio_observer(bool progress) {
  t64_machine *machine = build_ai_first_packet(SI_STEM_ID);

  if (machine == NULL) return NULL;
  T64_CHECK_INT(machine->ai.packet_head, 0);
  T64_CHECK_INT(machine->ai.packet_count, 1);
  if (progress) T64_CHECK(dequeue_packet(machine, false));
  return machine;
}

/* Count differing bytes and check that all of them sit in the observer entry
 * hash, the observer body and the trailer. */
static void check_difference_confined(const char *label, const uint8_t *a,
                                      const uint8_t *b, size_t length,
                                      const dec_info *info) {
  const size_t hash_at =
      D_TABLE + (size_t)D_ENTRY * (info->count - 1u) + E_HASH;
  const size_t body_at = (size_t)info->canonical_offset +
                         (size_t)info->canonical_length;
  size_t i;
  size_t differing = 0u;
  size_t outside = 0u;

  for (i = 0u; i < length; i++) {
    if (a[i] == b[i]) continue;
    differing++;
    if (!((i >= hash_at && i < hash_at + T64_DIGEST_SIZE) ||
          (i >= body_at && i < body_at + D_OBSERVER_BYTES) ||
          i >= length - D_TRAILER_BYTES)) {
      outside++;
    }
  }
  if (outside != 0u) {
    fprintf(stderr, "%s: %zu differing bytes outside the observer\n", label,
            outside);
  }
  T64_CHECK_INT(outside, 0);
  T64_CHECK(differing > 0u);
}

static void observer_pair(const char *label, t64_machine *progressed,
                          t64_machine *baseline, uint64_t progressed_tick,
                          uint64_t baseline_tick, const uint64_t want_progress[5],
                          const uint64_t want_baseline[5]) {
  size_t progress_length = 0u;
  size_t base_length = 0u;
  uint8_t *progress_blob = export_blob(progressed, &progress_length);
  uint8_t *base_blob = export_blob(baseline, &base_length);
  uint8_t digest_a[T64_DIGEST_SIZE];
  uint8_t digest_b[T64_DIGEST_SIZE];
  dec_info progress_info;
  dec_info base_info;
  size_t k;

  if (progress_blob != NULL && base_blob != NULL &&
      progress_length == base_length) {
    T64_CHECK(check_blob(label, progressed, progressed_tick, stem_ids,
                         sizeof stem_ids / sizeof stem_ids[0], progress_blob,
                         progress_length, &progress_info));
    T64_CHECK(check_blob(label, baseline, baseline_tick, stem_ids,
                         sizeof stem_ids / sizeof stem_ids[0], base_blob,
                         base_length, &base_info));
    for (k = 0u; k < 5u; k++) {
      T64_CHECK_INT(progress_info.observer[k], want_progress[k]);
      T64_CHECK_INT(base_info.observer[k], want_baseline[k]);
    }
    /* Equal machine digest, byte-equal canonical regions, equal declared
     * digests; different observer sections and trailers. */
    T64_CHECK_INT(t64_machine_digest(progressed, digest_a), T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_digest(baseline, digest_b), T64_STATUS_OK);
    T64_CHECK(bytes_equal(digest_a, digest_b, T64_DIGEST_SIZE));
    T64_CHECK_INT(progress_info.canonical_length, base_info.canonical_length);
    T64_CHECK(bytes_equal(progress_blob + progress_info.canonical_offset,
                          base_blob + base_info.canonical_offset,
                          (size_t)base_info.canonical_length));
    T64_CHECK(bytes_equal(progress_info.declared, base_info.declared,
                          T64_DIGEST_SIZE));
    T64_CHECK(!bytes_equal(progress_blob + progress_info.canonical_offset +
                               progress_info.canonical_length,
                           base_blob + base_info.canonical_offset +
                               base_info.canonical_length,
                           D_OBSERVER_BYTES));
    T64_CHECK(!bytes_equal(progress_blob + progress_length - D_TRAILER_BYTES,
                           base_blob + base_length - D_TRAILER_BYTES,
                           D_TRAILER_BYTES));
    check_difference_confined(label, progress_blob, base_blob, base_length,
                              &base_info);
  } else {
    T64_CHECK(false);
  }
  free(progress_blob);
  free(base_blob);
}

static void case_observer_continuity(void) {
  /* output drain cursor, video head, video count, audio head, audio count */
  static const uint64_t video_progress[5] = {8u, 1u, 0u, 0u, 0u};
  static const uint64_t video_baseline[5] = {0u, 0u, 1u, 0u, 0u};
  static const uint64_t audio_progress[5] = {0u, 0u, 0u, 1u, 0u};
  static const uint64_t audio_baseline[5] = {0u, 0u, 0u, 0u, 1u};
  t64_machine *progressed = build_video_observer(true);
  uint64_t progressed_tick = last_stop_tick;
  t64_machine *baseline = build_video_observer(false);
  uint64_t baseline_tick = last_stop_tick;
  t64_machine *trace_on;
  t64_machine *trace_off;
  uint8_t *blob_on;
  uint8_t *blob_off;
  size_t length_on = 0u;
  size_t length_off = 0u;

  if (progressed != NULL && baseline != NULL) {
    observer_pair("video-observer", progressed, baseline, progressed_tick,
                  baseline_tick, video_progress, video_baseline);
  }
  t64_machine_destroy(progressed);
  t64_machine_destroy(baseline);
  progressed = build_audio_observer(true);
  progressed_tick = last_stop_tick;
  baseline = build_audio_observer(false);
  baseline_tick = last_stop_tick;
  if (progressed != NULL && baseline != NULL) {
    observer_pair("audio-observer", progressed, baseline, progressed_tick,
                  baseline_tick, audio_progress, audio_baseline);
  }
  t64_machine_destroy(progressed);
  t64_machine_destroy(baseline);

  /* Trace enablement and queued trace records never reach the blob. */
  trace_on = build_idle(SI_STEM_ID);
  trace_off = build_idle(SI_STEM_ID);
  if (trace_on == NULL || trace_off == NULL) {
    t64_machine_destroy(trace_on);
    t64_machine_destroy(trace_off);
    return;
  }
  T64_CHECK_INT(t64_machine_trace_set_enabled(trace_on, true), T64_STATUS_OK);
  T64_CHECK(advance_to(trace_on, 128u));
  T64_CHECK(advance_to(trace_off, 128u));
  T64_CHECK(trace_on->trace_enabled && trace_on->trace_count > 0u);
  T64_CHECK(!trace_off->trace_enabled && trace_off->trace_count == 0u);
  blob_on = export_blob(trace_on, &length_on);
  blob_off = export_blob(trace_off, &length_off);
  T64_CHECK(blob_on != NULL && blob_off != NULL && length_on == length_off);
  if (blob_on != NULL && blob_off != NULL && length_on == length_off) {
    T64_CHECK(bytes_equal(blob_on, blob_off, length_on));
  }
  free(blob_on);
  free(blob_off);
  t64_machine_destroy(trace_on);
  t64_machine_destroy(trace_off);
}

/* ---- case idempotence-shape -------------------------------------------- */

#define EXTRA_CANARY_BYTES 4096u

static void case_idempotence_shape(void) {
  t64_machine *machine = build_idle(SI_STEM_ID);
  size_t required = 0u;
  size_t again = 0u;
  size_t written = 0u;
  size_t length_a = 0u;
  size_t length_b = 0u;
  uint8_t *first;
  uint8_t *second;
  uint8_t *buffer;
  uint8_t *pristine;
  size_t i;

  if (machine == NULL) return;
  first = export_blob(machine, &length_a);
  second = export_blob(machine, &length_b);
  T64_CHECK(first != NULL && second != NULL && length_a == length_b);
  if (first != NULL && second != NULL && length_a == length_b) {
    T64_CHECK(bytes_equal(first, second, length_a));
  }
  free(second);
  /* Ten repeats of the query agree with each other and with the copy-out. */
  required = length_a;
  for (i = 0u; i < 10u; i++) {
    again = 0u;
    T64_CHECK_INT(t64_machine_state_export(machine, NULL, 0u, &again),
                  T64_STATUS_BUFFER_TOO_SMALL);
    T64_CHECK_INT(again, required);
  }
  if (first == NULL) {
    t64_machine_destroy(machine);
    return;
  }
  /* A short buffer: BUFFER_TOO_SMALL, the required length, no byte written. */
  buffer = (uint8_t *)malloc(required);
  pristine = (uint8_t *)malloc(required);
  if (buffer == NULL || pristine == NULL) {
    T64_CHECK(false);
    free(buffer);
    free(pristine);
    free(first);
    t64_machine_destroy(machine);
    return;
  }
  (void)memset(buffer, 0x5a, required);
  (void)memcpy(pristine, buffer, required);
  again = 0u;
  T64_CHECK_INT(t64_machine_state_export(machine, buffer, required - 1u,
                                         &again),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(again, required);
  T64_CHECK(bytes_equal(buffer, pristine, required));
  /* A non-NULL pointer with capacity zero behaves as a query. */
  again = 0u;
  T64_CHECK_INT(t64_machine_state_export(machine, buffer, 0u, &again),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(again, required);
  T64_CHECK(bytes_equal(buffer, pristine, required));
  /* A buffer of exactly the required length succeeds. */
  written = 0u;
  T64_CHECK_INT(t64_machine_state_export(machine, buffer, required, &written),
                T64_STATUS_OK);
  T64_CHECK_INT(written, required);
  T64_CHECK(bytes_equal(buffer, first, required));
  free(buffer);
  free(pristine);
  /* Bytes past the written length keep their value (4,096 extra canaries). */
  buffer = (uint8_t *)malloc(required + EXTRA_CANARY_BYTES);
  if (buffer != NULL) {
    (void)memset(buffer + required, CANARY_FILL, EXTRA_CANARY_BYTES);
    written = 0u;
    T64_CHECK_INT(t64_machine_state_export(machine, buffer,
                                           required + EXTRA_CANARY_BYTES,
                                           &written),
                  T64_STATUS_OK);
    T64_CHECK_INT(written, required);
    for (i = 0u; i < EXTRA_CANARY_BYTES; i++) {
      T64_CHECK_INT(buffer[required + i], CANARY_FILL);
    }
    T64_CHECK(bytes_equal(buffer, first, required));
    free(buffer);
  } else {
    T64_CHECK(false);
  }
  free(first);
  t64_machine_destroy(machine);
}

/* ---- case non-interference --------------------------------------------- */

typedef struct {
  size_t allocations;
  size_t releases;
} allocator_counts;

static bool counting_allocate(void *context, size_t size, size_t alignment,
                              t64_host_storage *storage_out) {
  ((allocator_counts *)context)->allocations++;
  return test_allocate(NULL, size, alignment, storage_out);
}

static void counting_release(void *context, t64_host_storage storage) {
  ((allocator_counts *)context)->releases++;
  test_release(NULL, storage);
}

static uint8_t *snapshot_machine(const t64_machine *machine) {
  uint8_t *copy = (uint8_t *)malloc(sizeof *machine);

  if (copy == NULL) {
    T64_CHECK(false);
    return NULL;
  }
  (void)memcpy(copy, machine, sizeof *machine);
  return copy;
}

static void case_non_interference(void) {
  allocator_counts counts = {0u, 0u};
  const t64_host_allocator counting = {counting_allocate, counting_release,
                                       &counts};
  t64_machine *machine = create_admitted_with(&counting, SI_STEM_ID);
  t64_machine *with_exports;
  t64_machine *without_exports;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  uint8_t *before;
  uint8_t *after;
  uint8_t *blob;
  uint8_t *scratch;
  size_t length = 0u;
  size_t scratch_length = 0u;
  size_t written = 0u;
  allocator_counts counts_before;
  uint64_t step;
  uint8_t step_a[T64_DIGEST_SIZE];
  uint8_t step_b[T64_DIGEST_SIZE];
  uint8_t drained[32];
  t64_output_drain_result drain;

  if (machine == NULL) return;
  install_spin_loop(machine);
  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  T64_CHECK(advance_to(machine, 64u));
  T64_CHECK(advance_to(machine, 128u));
  T64_CHECK(machine->trace_count > 0u);
  /* Observer progress: 12 captured bytes drained, so the drain cursor is not
   * zero and a write to it by the export would show in the snapshot. */
  isviewer_emit(machine, isviewer_bytes, 12u);
  (void)memset(&drain, 0, sizeof drain);
  T64_CHECK_INT(t64_machine_output_drain(machine, drained, sizeof drained,
                                         &drain),
                T64_STATUS_OK);
  T64_CHECK_INT(drain.copied_bytes, 12);
  T64_CHECK_INT(machine->output_drain_cursor, 12);
  before = snapshot_machine(machine);
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  counts_before = counts;
  /* A successful export changes nothing: the whole structure (observer
   * cursors, trace records and counters, queue, tick), the digest and the
   * allocator counters. */
  blob = export_blob(machine, &length);
  after = snapshot_machine(machine);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  if (before != NULL && after != NULL) {
    T64_CHECK(bytes_equal(before, after, sizeof *machine));
  }
  T64_CHECK(bytes_equal(digest_before, digest_after, T64_DIGEST_SIZE));
  T64_CHECK_INT(counts.allocations, counts_before.allocations);
  T64_CHECK_INT(counts.releases, counts_before.releases);
  /* Failed exports change nothing either: a short buffer and an overlapping
   * span. */
  if (blob != NULL && length > 64u) {
    written = 0x77u;
    T64_CHECK_INT(t64_machine_state_export(machine, blob, length - 1u,
                                           &written),
                  T64_STATUS_BUFFER_TOO_SMALL);
    T64_CHECK_INT(t64_machine_state_export(
                      machine, blob, length,
                      (size_t *)(void *)(blob + 16)),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(t64_machine_state_export(machine, machine->ram, 64u,
                                           &written),
                  T64_STATUS_INVALID_ARGUMENT);
  }
  free(after);
  after = snapshot_machine(machine);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  if (before != NULL && after != NULL) {
    T64_CHECK(bytes_equal(before, after, sizeof *machine));
  }
  T64_CHECK(bytes_equal(digest_before, digest_after, T64_DIGEST_SIZE));
  T64_CHECK_INT(counts.allocations, counts_before.allocations);
  T64_CHECK_INT(counts.releases, counts_before.releases);
  free(before);
  free(after);
  free(blob);
  t64_machine_destroy(machine);

  /* A machine that exports between advances reaches the same digests as one
   * that never exports, across the first AI packet boundary (sample 4). */
  with_exports = start_ai_dma_pair(SI_STEM_ID);
  without_exports = start_ai_dma_pair(SI_STEM_ID);
  if (with_exports == NULL || without_exports == NULL) {
    t64_machine_destroy(with_exports);
    t64_machine_destroy(without_exports);
    return;
  }
  scratch = NULL;
  for (step = 1u; step <= 5u; step++) {
    T64_CHECK(dispatch_ai_samples(with_exports, step));
    T64_CHECK(dispatch_ai_samples(without_exports, step));
    if (step == 1u || step == 3u || step == 4u) {
      free(scratch);
      scratch = export_blob(with_exports, &scratch_length);
    }
    T64_CHECK_INT(t64_machine_digest(with_exports, step_a), T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_digest(without_exports, step_b),
                  T64_STATUS_OK);
    T64_CHECK(bytes_equal(step_a, step_b, T64_DIGEST_SIZE));
  }
  T64_CHECK_INT(with_exports->ai.packet_history_count,
                without_exports->ai.packet_history_count);
  T64_CHECK(with_exports->ai.packet_history_count >= 1u);
  free(scratch);
  t64_machine_destroy(with_exports);
  t64_machine_destroy(without_exports);
}

/* ---- case refusals ------------------------------------------------------ */

#define LENGTH_SENTINEL ((size_t)0x1badf00du)

/* One refusal on an accepted machine with caller storage: the specific
 * status, the caller bytes, the length value and the machine digest all
 * unchanged. */
static void check_refusal(const char *label, const t64_machine *machine,
                          uint8_t *bytes, size_t capacity, size_t *length_ptr,
                          t64_status want) {
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  uint8_t *saved = NULL;
  const size_t sentinel = length_ptr != NULL ? *length_ptr : 0u;
  t64_status digest_status = T64_STATUS_OK;
  t64_status status;

  /* A pre-admission machine has no digest (NOT_READY) before and after. */
  if (machine != NULL) {
    digest_status = t64_machine_digest(machine, digest_before);
  }
  if (bytes != NULL && capacity > 0u) {
    saved = (uint8_t *)malloc(capacity);
    if (saved != NULL) (void)memcpy(saved, bytes, capacity);
  }
  status = t64_machine_state_export(machine, bytes, capacity, length_ptr);
  if (status != want) {
    fprintf(stderr, "%s: status %u want %u\n", label, (unsigned)status,
            (unsigned)want);
  }
  T64_CHECK_INT(status, want);
  if (length_ptr != NULL) T64_CHECK_INT(*length_ptr, sentinel);
  if (saved != NULL) T64_CHECK(bytes_equal(bytes, saved, capacity));
  free(saved);
  if (machine != NULL) {
    T64_CHECK_INT(t64_machine_digest(machine, digest_after), digest_status);
    if (digest_status == T64_STATUS_OK) {
      T64_CHECK(bytes_equal(digest_before, digest_after, T64_DIGEST_SIZE));
    }
  }
}

static void case_refusals(void) {
  t64_machine *machine = build_idle(SI_STEM_ID);
  t64_machine *legacy;
  t64_machine *fresh = NULL;
  const t64_profile_descriptor *profile = NULL;
  size_t length = 0u;
  size_t required = 0u;
  size_t value;
  uint8_t *blob;
  uint8_t small[256];

  if (machine == NULL) return;
  /* The accepted baseline on this machine. */
  blob = export_blob(machine, &required);
  if (blob == NULL) {
    t64_machine_destroy(machine);
    return;
  }
  free(blob);
  blob = (uint8_t *)malloc(required);
  if (blob == NULL) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  (void)memset(blob, 0x6d, required);
  length = LENGTH_SENTINEL;
  check_refusal("null machine", NULL, blob, required, &length,
                T64_STATUS_INVALID_ARGUMENT);
  check_refusal("null length", machine, blob, required, NULL,
                T64_STATUS_INVALID_ARGUMENT);
  /* The length value inside the output span. */
  value = LENGTH_SENTINEL;
  (void)memcpy(blob + 64, &value, sizeof value);
  check_refusal("length inside span", machine, blob, required,
                (size_t *)(void *)(blob + 64), T64_STATUS_INVALID_ARGUMENT);
  /* The output span inside RDRAM, inside the ISViewer buffer; the length
   * value inside machine storage. */
  (void)memset(small, 0x3c, sizeof small);
  length = LENGTH_SENTINEL;
  check_refusal("span inside rdram", machine, machine->ram + 0x2000,
                sizeof small, &length, T64_STATUS_INVALID_ARGUMENT);
  check_refusal("span inside isviewer", machine, machine->output.buffer, 16u,
                &length, T64_STATUS_INVALID_ARGUMENT);
  check_refusal("length inside machine", machine, small, sizeof small,
                &machine->image_length, T64_STATUS_INVALID_ARGUMENT);
  /* A pre-admission machine of the same profile: NOT_READY in both forms;
   * after admission the same call is accepted and sizes like the baseline. */
  if (t64_machine_profile(machine, &profile) == T64_STATUS_OK &&
      t64_machine_create(&allocator, profile, &fresh) == T64_STATUS_OK &&
      fresh != NULL) {
    length = LENGTH_SENTINEL;
    check_refusal("pre-admission copy", fresh, small, sizeof small, &length,
                  T64_STATUS_NOT_READY);
    check_refusal("pre-admission query", fresh, NULL, 0u, &length,
                  T64_STATUS_NOT_READY);
  } else {
    T64_CHECK(false);
  }
  t64_machine_destroy(fresh);
  /* A legacy id: INVALID_ARGUMENT in both forms. */
  legacy = create_admitted(AI_PROFILE_ID);
  if (legacy != NULL) {
    length = LENGTH_SENTINEL;
    check_refusal("legacy copy", legacy, small, sizeof small, &length,
                  T64_STATUS_INVALID_ARGUMENT);
    check_refusal("legacy query", legacy, NULL, 0u, &length,
                  T64_STATUS_INVALID_ARGUMENT);
  }
  t64_machine_destroy(legacy);
  free(blob);
  t64_machine_destroy(machine);
}

/* ---- case size-arithmetic ---------------------------------------------- */

/* A deliberately generous worst case: the state of one machine with every
 * major array at its maximum (RDRAM, both coverage arrays, TMEM, SP memory,
 * both packet queues, the save storage) plus 128 KiB for every other device
 * section, all 32 table entries and the fixed parts. */
#define WORST_CANONICAL                                                  \
  ((uint64_t)0x400004u + 2u * 2097152u + 4096u + 8200u +                 \
   (uint64_t)T64_VIDEO_QUEUE_STORAGE_BYTES +                             \
   (uint64_t)T64_AUDIO_QUEUE_STORAGE_BYTES + 32788u + 131072u + 15u +    \
   4u * 32u)
#define WORST_TOTAL (WORST_CANONICAL + 288u + 56u * 32u + 40u + 32u)

_Static_assert(WORST_TOTAL < UINT64_C(67108864),
               "the worst-case container stays below the 64 MiB ceiling");
_Static_assert(WORST_TOTAL < (uint64_t)(SIZE_MAX / 2u),
               "the worst-case container stays below half of SIZE_MAX");

static void check_length_refused(const char *label, uint64_t canonical,
                                 uint32_t count) {
  uint64_t value = UINT64_C(0xfeedfacecafebeef);
  const bool accepted = t64_state_container_length(canonical, count, &value);

  if (accepted) fprintf(stderr, "%s: accepted\n", label);
  T64_CHECK(!accepted);
  T64_CHECK(value == UINT64_C(0xfeedfacecafebeef));
}

static void case_size_arithmetic(void) {
  uint64_t total = 0u;
  /* 288 + 56 x 32 + 40 + 32: the fixed part at the section limit. */
  const uint64_t fixed_at_limit = 288u + 56u * 32u + 40u + 32u;

  /* Exact totals for literal tuples. */
  T64_CHECK(t64_state_container_length(0u, 1u, &total));
  T64_CHECK_INT(total, 416);
  T64_CHECK(t64_state_container_length(1000u, 3u, &total));
  T64_CHECK_INT(total, 1528);
  T64_CHECK(t64_state_container_length(6350575u, 11u, &total));
  T64_CHECK_INT(total, 6351551);
  /* The ceiling itself is allowed, one more byte is not. */
  T64_CHECK(t64_state_container_length(UINT64_C(67108864) - fixed_at_limit,
                                       32u, &total));
  T64_CHECK(total == UINT64_C(67108864));
  check_length_refused("one over the ceiling",
                       UINT64_C(67108864) - fixed_at_limit + 1u, 32u);
  check_length_refused("64 MiB canonical", UINT64_C(67108864), 1u);
  check_length_refused("UINT64_MAX", UINT64_MAX, 1u);
  check_length_refused("UINT64_MAX minus 1", UINT64_MAX - 1u, 32u);
  check_length_refused("count zero", 100u, 0u);
  check_length_refused("count 33", 100u, 33u);
  check_length_refused("count UINT32_MAX", 100u, UINT32_MAX);
  T64_CHECK(!t64_state_container_length(100u, 1u, NULL));
}

int main(int argc, char **argv) {
  static const char *const cases[] = {
      "export-layout",       "decoder-controls", "states-and-ids",
      "observer-continuity", "idempotence-shape", "non-interference",
      "refusals",            "size-arithmetic"};
  static void (*const runners[])(void) = {
      case_export_layout,       case_decoder_controls, case_states_and_ids,
      case_observer_continuity, case_idempotence_shape, case_non_interference,
      case_refusals,            case_size_arithmetic};
  const size_t count = sizeof cases / sizeof cases[0];
  size_t index;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (index = 0u; index < count; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    for (index = 0u; index < count; index++) {
      if (strcmp(argv[2], cases[index]) == 0) {
        char name[96];

        runners[index]();
        (void)snprintf(name, sizeof name, "unit.core.state.export.%s",
                       cases[index]);
        return T64_TEST_FINISH(name);
      }
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  for (index = 0u; index < count; index++) {
    runners[index]();
  }
  return T64_TEST_FINISH("unit.core.state.export");
}
