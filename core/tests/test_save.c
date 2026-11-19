/*
 * Phase 9 save media tests (plan 09-04): EEPROM on joybus channel 4, the SRAM
 * window and its PI DMA, selection by profile only, refusal statuses and the
 * save-media digest section.
 *
 * Every figure below is a named software-profile policy value written out as
 * a literal in this file; nothing is taken from a production helper, and no
 * hardware accuracy or hardware timing is claimed. All contents are authored
 * byte patterns; no ROM, save or firmware image is used (the only external
 * input is the already cleared openIPL3 asset named by T64_TEST_IPL3_PATH).
 *
 * usage: test_save [--list-cases|--suite device|--case NAME]
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"

#define STEM_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define E4K_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k"
#define E16K_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k"
#define S32K_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k"
#define IPL3_ASSET_BYTES 4096u

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
  return read_error == 0 && close_error == 0 &&
         length == IPL3_ASSET_BYTES && extra == EOF;
}

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

static t64_machine *create_admitted(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];

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
  T64_CHECK_INT(t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES),
                T64_STATUS_OK);
  return machine;
}

/* ---- digest stream capture ----------------------------------------------- */

#define SPAN_CAPACITY 4096u

typedef struct {
  uint8_t *bytes;
  size_t length;
  t64_digest_span spans[SPAN_CAPACITY];
  size_t span_count;
} stream;

static void free_stream(stream *captured) {
  free(captured->bytes);
  captured->bytes = NULL;
}

static bool capture_stream(const t64_machine *machine, stream *out) {
  t64_digest_sink sink;
  size_t length;

  (void)memset(out, 0, sizeof *out);
  (void)memset(&sink, 0, sizeof sink);
  T64_CHECK_INT(t64_digest_stream(machine, &sink), T64_STATUS_OK);
  length = sink.offset;
  out->bytes = (uint8_t *)malloc(length > 0u ? length : 1u);
  if (out->bytes == NULL) {
    T64_CHECK(false);
    return false;
  }
  (void)memset(&sink, 0, sizeof sink);
  sink.buffer = out->bytes;
  sink.capacity = length;
  sink.spans = out->spans;
  sink.span_capacity = SPAN_CAPACITY;
  T64_CHECK_INT(t64_digest_stream(machine, &sink), T64_STATUS_OK);
  T64_CHECK(!sink.overflow);
  T64_CHECK_INT(sink.offset, length);
  out->length = length;
  out->span_count = sink.span_count;
  return !sink.overflow && sink.offset == length;
}

static uint32_t be32(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

/* The marker ids in stream order; returns how many markers were found. The
 * ids are copied into ids[] (at most capacity). */
static size_t list_markers(const stream *captured, uint32_t *ids,
                           size_t capacity) {
  size_t index;
  size_t found = 0u;

  for (index = 0u; index < captured->span_count; index++) {
    if (captured->spans[index].class != T64_DIGEST_CLASS_MARKER) continue;
    if (found < capacity) {
      ids[found] = be32(captured->bytes + captured->spans[index].offset);
    }
    found++;
  }
  return found;
}

/* The span index of marker `id`, or SIZE_MAX. */
static size_t marker_span(const stream *captured, uint32_t id) {
  size_t index;

  for (index = 0u; index < captured->span_count; index++) {
    if (captured->spans[index].class != T64_DIGEST_CLASS_MARKER) continue;
    if (be32(captured->bytes + captured->spans[index].offset) == id) {
      return index;
    }
  }
  return SIZE_MAX;
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

static void digest_of(const t64_machine *machine,
                      uint8_t digest[T64_DIGEST_SIZE]) {
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
}

/* ---- machine and host bus helpers ---------------------------------------- */

/* A machine whose boot CPU event is cancelled so the devices are driven by
 * host bus writes and guest-tick advances alone. */
static t64_machine *make_idle_machine(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id);
  uint64_t cancelled_tick = UINT64_MAX;
  t64_status status;

  if (machine == NULL) return NULL;
  status = t64_schedule_cancel(machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP,
                               0u, &cancelled_tick);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(cancelled_tick, 1u);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

static bool advance_guest_to(t64_machine *machine, uint64_t tick) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
  t64_stop_result result = {0u, 0u};
  t64_status status = t64_machine_advance(machine, &request, &result);

  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, tick);
  return result.reason == T64_STOP_REASON_BOUND && result.tick == tick;
}

#define SI_DRAM_ADDR UINT32_C(0x04800000)
#define SI_RD64B UINT32_C(0x04800004)
#define SI_WR64B UINT32_C(0x04800010)
#define SI_STATUS UINT32_C(0x04800018)
#define PIF_ADDRESS UINT32_C(0x1FC007C0)
#define SEND_DRAM UINT32_C(0x3800)
#define RECEIVE_DRAM UINT32_C(0x3900)

/* ---- literal policy figures (written out here, never read from the core) -- */

/* Handshake totals H of single-entry frames on channel 4 of an EEPROM id:
 * 2000 n + 188 + 438 + 2000 m + 250 for the three exact triples. */
#define H_INFO UINT64_C(8876)    /* n 1, m 3  */
#define H_READ UINT64_C(20876)   /* n 2, m 8  */
#define H_WRITE UINT64_C(22876)  /* n 10, m 1 */
#define READ_TAIL UINT64_C(4096)
#define WRITE_DMA_TICKS UINT64_C(2500)
#define BUSY_WINDOW UINT64_C(375000)

/* One joybus frame: four skip bytes (channels 0 to 3), the channel 4 counts,
 * the transmit bytes, 0xFF receive fill, the end escape, zero fill and the
 * parse request in byte 63. */
static void build_frame(uint8_t frame[64], uint32_t tx_count,
                        uint32_t rx_count, const uint8_t *tx) {
  (void)memset(frame, 0, 64u);
  frame[4] = (uint8_t)tx_count;
  frame[5] = (uint8_t)rx_count;
  (void)memcpy(frame + 6, tx, tx_count);
  (void)memset(frame + 6 + tx_count, 0xFF, rx_count);
  frame[6 + tx_count + rx_count] = 0xFE;
  frame[63] = 0x01;
}

/* A complete accepted WR64B of the frame at the current tick: moves it into
 * the PIF RAM, parses it, settles the 2,500-tick completion, acknowledges. */
static void write_frame(t64_machine *machine, const uint8_t frame[64]) {
  const uint64_t start = machine->tick;

  (void)memcpy(machine->ram + SEND_DRAM, frame, 64u);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, SEND_DRAM),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_WR64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, start + WRITE_DMA_TICKS));
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
}

/* An RD64B started at `start`, with the destination pre-filled with 0xAA so
 * an untouched byte is visible; settles the completion and returns the image
 * the guest would find. The execute and completion ticks are checked against
 * the literal arithmetic start + handshake and execute + 4,096. */
static void read_image(t64_machine *machine, uint64_t start,
                       uint64_t handshake, uint8_t image[64]) {
  T64_CHECK(advance_guest_to(machine, start));
  (void)memset(machine->ram + RECEIVE_DRAM, 0xAA, 64u);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, RECEIVE_DRAM),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_RD64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->si.execute_tick, start + handshake);
  T64_CHECK_INT(machine->si.complete_tick, start + handshake + READ_TAIL);
  /* Settle at the completion tick the machine reports (asserted equal to the
     literal above), so one wrong tick reports itself and does not cascade. */
  T64_CHECK(advance_guest_to(machine, machine->si.complete_tick));
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  (void)memcpy(image, machine->ram + RECEIVE_DRAM, 64u);
}

/* ------------------------------------------------------------------------ */
/* storage-blank                                                             */
/* ------------------------------------------------------------------------ */

typedef struct {
  const char *id;
  uint32_t kind;
  size_t size;
  uint8_t fill;
} blank_row;

/* Independent literals: the kind numbering and the byte sizes are policy; the
 * blank contents are 0xFF for the EEPROM and 0x00 for the SRAM. */
static const blank_row blank_rows[] = {
    {STEM_ID, 0u, 0u, 0x00u},
    {E4K_ID, 1u, 512u, 0xFFu},
    {E16K_ID, 2u, 2048u, 0xFFu},
    {S32K_ID, 3u, 32768u, 0x00u},
};

/* The idle digest of the stem id (no save device) captured at the 09-03 tree
 * before this plan's first edit; the section 11 addition must not move it. */
#define STEM_IDLE_DIGEST \
  "5ed31137a518103e1086ec9d84350f44306f81fef57145ca4ca72a7f0815d9b0"

static void case_storage_blank(void) {
  size_t index;

  for (index = 0u; index < sizeof blank_rows / sizeof blank_rows[0];
       index++) {
    const blank_row *row = &blank_rows[index];
    t64_machine *machine = create_admitted(row->id);
    stream captured;
    uint32_t ids[16];
    size_t count;
    size_t at;
    size_t position;

    if (machine == NULL) continue;
    T64_CHECK_INT(machine->profile.save_kind, row->kind);
    T64_CHECK_INT(machine->save.busy_until, 0u);
    if (row->size == 0u) {
      T64_CHECK(machine->save.bytes == NULL);
    } else {
      T64_CHECK(machine->save.bytes != NULL);
      if (machine->save.bytes != NULL) {
        T64_CHECK_INT((uintptr_t)machine->save.bytes % 8u, 0u);
        for (position = 0u; position < row->size; position++) {
          if (machine->save.bytes[position] != row->fill) {
            T64_CHECK_INT(machine->save.bytes[position], row->fill);
            break;
          }
        }
      }
    }

    if (capture_stream(machine, &captured)) {
      count = list_markers(&captured, ids, 16u);
      /* Stem: markers 1 to 10 only; save ids: 1 to 11, ascending. */
      T64_CHECK_INT(count, row->kind == 0u ? 10u : 11u);
      for (at = 0u; at < count && at < 16u; at++) {
        T64_CHECK_INT(ids[at], at + 1u);
      }
      if (row->kind == 0u) {
        T64_CHECK(marker_span(&captured, 11u) == SIZE_MAX);
      } else {
        size_t span = marker_span(&captured, 11u);

        T64_CHECK(span != SIZE_MAX);
        if (span != SIZE_MAX) {
          /* Section 11 is the last section: the body is one new-span run of
             kind, size, deadline, direction and the storage bytes. */
          T64_CHECK(span + 1u < captured.span_count);
          if (span + 1u < captured.span_count) {
            const t64_digest_span *body = &captured.spans[span + 1u];

            T64_CHECK_INT(body->class, T64_DIGEST_CLASS_NEW);
            T64_CHECK_INT(body->length, 20u + row->size);
            T64_CHECK_INT(be32(captured.bytes + body->offset), row->kind);
            T64_CHECK_INT(be32(captured.bytes + body->offset + 4u),
                          row->size);
          }
        }
      }
      free_stream(&captured);
    }

    if (row->kind == 0u) {
      uint8_t digest[T64_DIGEST_SIZE];
      char hex[2u * T64_DIGEST_SIZE + 1u];

      digest_of(machine, digest);
      to_hex(digest, hex);
      if (strcmp(hex, STEM_IDLE_DIGEST) != 0) {
        fprintf(stderr, "stem idle digest: got %s\n", hex);
      }
      T64_CHECK_STR(hex, STEM_IDLE_DIGEST);
    } else if (machine->save.bytes != NULL) {
      uint8_t before[T64_DIGEST_SIZE];
      uint8_t after[T64_DIGEST_SIZE];

      /* A one-byte change of the storage alters the digest, at the first, a
         middle and the last byte. */
      const size_t spots[3] = {0u, row->size / 2u, row->size - 1u};
      size_t spot;

      digest_of(machine, before);
      for (spot = 0u; spot < 3u; spot++) {
        machine->save.bytes[spots[spot]] ^= UINT8_C(0x01);
        digest_of(machine, after);
        T64_CHECK(memcmp(before, after, T64_DIGEST_SIZE) != 0);
        machine->save.bytes[spots[spot]] ^= UINT8_C(0x01);
        digest_of(machine, after);
        T64_CHECK(memcmp(before, after, T64_DIGEST_SIZE) == 0);
      }
    }
    t64_machine_destroy(machine);
  }
}

/* ------------------------------------------------------------------------ */
/* eeprom-tracer                                                             */
/* ------------------------------------------------------------------------ */

/* The write frame of the tracer: block 1, data 11 to 18. */
static const uint8_t tracer_write_tx[10] = {0x05, 0x01, 0x11, 0x12, 0x13,
                                            0x14, 0x15, 0x16, 0x17, 0x18};
static const uint8_t tracer_read_tx[2] = {0x04, 0x01};

/* Writes block 1 at S1 = 10,000, then reads block 1 with the read executing
 * `delta` ticks from the deadline (E2 = deadline + delta). */
static void run_tracer(int64_t delta, uint8_t read_image_out[64],
                       uint8_t write_image_out[64]) {
  t64_machine *machine = make_idle_machine(E4K_ID);
  uint8_t frame[64];
  uint64_t deadline;
  uint64_t e1;

  if (machine == NULL) return;
  advance_guest_to(machine, 3000u);
  build_frame(frame, 10u, 1u, tracer_write_tx);
  write_frame(machine, frame);
  /* S1 = 10,000: E1 = S1 + 22,876, C1 = E1 + 4,096. */
  read_image(machine, 10000u, H_WRITE, write_image_out);
  e1 = UINT64_C(10000) + H_WRITE;
  T64_CHECK_INT(e1, 32876u);
  deadline = e1 + BUSY_WINDOW;
  T64_CHECK_INT(deadline, 407876u);
  T64_CHECK_INT(machine->save.busy_until, deadline);

  build_frame(frame, 2u, 8u, tracer_read_tx);
  write_frame(machine, frame);
  /* E2 = S2 + 20,876, so S2 = deadline - 20,876 + delta. */
  read_image(machine, (uint64_t)((int64_t)(deadline - H_READ) + delta), H_READ,
             read_image_out);
  t64_machine_destroy(machine);
}

static void case_eeprom_tracer(void) {
  uint8_t read_image_out[64];
  uint8_t write_image_out[64];
  size_t index;

  /* Write accepted: byte 5 is the receive count 1 with no flag, the data byte
   * (offset 16) is 0x00. */
  run_tracer(0, read_image_out, write_image_out);
  T64_CHECK_INT(write_image_out[4], 10u);
  T64_CHECK_INT(write_image_out[5], 0x01u);
  T64_CHECK_INT(write_image_out[16], 0x00u);
  /* A read whose E equals the deadline exactly is idle: bytes 11 to 18. */
  T64_CHECK_INT(read_image_out[4], 2u);
  T64_CHECK_INT(read_image_out[5], 0x08u);
  for (index = 0u; index < 8u; index++) {
    T64_CHECK_INT(read_image_out[8u + index], 0x11u + index);
  }
  /* One tick earlier it is busy: byte 5 carries the timeout flag with the
   * receive count and the fill bytes are untouched (0xFF). */
  run_tracer(-1, read_image_out, write_image_out);
  T64_CHECK_INT(write_image_out[5], 0x01u);
  T64_CHECK_INT(write_image_out[16], 0x00u);
  T64_CHECK_INT(read_image_out[5], 0x48u);
  for (index = 0u; index < 8u; index++) {
    T64_CHECK_INT(read_image_out[8u + index], 0xFFu);
  }
}

/* ------------------------------------------------------------------------ */
/* Bus and PI helpers for the SRAM cases                                      */
/* ------------------------------------------------------------------------ */

#define AI_ID "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"
#define LEGACY_ID "ntsc-cart-4m-openipl3-rdram-b-t1-v1"
#define SRAM_FIRST UINT32_C(0x08000000)
#define SRAM_LAST_WORD UINT32_C(0x08007FFC)
#define PI_DRAM UINT32_C(0x04600000)
#define PI_CART UINT32_C(0x04600004)
#define PI_RD_LEN UINT32_C(0x04600008)
#define PI_WR_LEN UINT32_C(0x0460000C)
#define PI_STATUS UINT32_C(0x04600010)
#define ROM_CART_BASE UINT32_C(0x10000000)
#define MI_PI_BIT UINT32_C(0x10)

static t64_status bus_r32(t64_machine *machine, uint32_t address,
                          uint32_t *value) {
  return t64_machine_bus_read(machine, address, 4u, value);
}

static t64_status bus_w32(t64_machine *machine, uint32_t address,
                          uint32_t value) {
  return t64_machine_bus_write(machine, address, 4u, value);
}

static uint32_t bus_value(t64_machine *machine, uint32_t address) {
  uint32_t value = UINT32_MAX;

  T64_CHECK_INT(bus_r32(machine, address, &value), T64_STATUS_OK);
  return value;
}

/* ------------------------------------------------------------------------ */
/* sram-window                                                               */
/* ------------------------------------------------------------------------ */

static void case_sram_window(void) {
  static const char *const refusing_ids[] = {STEM_ID, E4K_ID, E16K_ID, AI_ID,
                                             LEGACY_ID};
  static const uint32_t outside[] = {UINT32_C(0x08008000),
                                     UINT32_C(0x0800FFFC),
                                     UINT32_C(0x08010000),
                                     UINT32_C(0x0FFFFFFC)};
  t64_machine *machine = create_admitted(S32K_ID);
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint8_t copy[T64_SAVE_SRAM_BYTES];
  uint64_t wide = 0u;
  size_t index;

  if (machine != NULL) {
    /* Accepted baseline in the same setup: blank read, first and last word,
       big-endian bytes in the storage. */
    T64_CHECK_INT(bus_value(machine, SRAM_LAST_WORD), 0u);
    T64_CHECK_INT(bus_w32(machine, SRAM_FIRST, UINT32_C(0x01234567)),
                  T64_STATUS_OK);
    T64_CHECK_INT(bus_value(machine, SRAM_FIRST), 0x01234567u);
    T64_CHECK_INT(bus_w32(machine, SRAM_LAST_WORD, UINT32_C(0x89ABCDEF)),
                  T64_STATUS_OK);
    T64_CHECK_INT(bus_value(machine, SRAM_LAST_WORD), 0x89ABCDEFu);
    if (machine->save.bytes != NULL) {
      T64_CHECK_INT(machine->save.bytes[0], 0x01u);
      T64_CHECK_INT(machine->save.bytes[1], 0x23u);
      T64_CHECK_INT(machine->save.bytes[2], 0x45u);
      T64_CHECK_INT(machine->save.bytes[3], 0x67u);
      T64_CHECK_INT(machine->save.bytes[T64_SAVE_SRAM_BYTES - 4u], 0x89u);
      T64_CHECK_INT(machine->save.bytes[T64_SAVE_SRAM_BYTES - 1u], 0xEFu);
      (void)memcpy(copy, machine->save.bytes, sizeof copy);
    }
    digest_of(machine, before);

    /* Every refusal: out-of-window addresses (no mirroring, no banking),
       widths 1, 2 and 8, and a misaligned word; the digest and the storage
       stay bit-identical. */
    for (index = 0u; index < sizeof outside / sizeof outside[0]; index++) {
      uint32_t value = 0xA5A5A5A5u;

      T64_CHECK_INT(bus_r32(machine, outside[index], &value),
                    T64_STATUS_BUS_UNSUPPORTED);
      T64_CHECK_INT(value, 0xA5A5A5A5u);
      T64_CHECK_INT(bus_w32(machine, outside[index], UINT32_C(0x55555555)),
                    T64_STATUS_BUS_UNSUPPORTED);
    }
    {
      uint32_t value = 0xA5A5A5A5u;

      T64_CHECK_INT(t64_machine_bus_read(machine, SRAM_FIRST, 1u, &value),
                    T64_STATUS_BUS_UNSUPPORTED);
      T64_CHECK_INT(t64_machine_bus_read(machine, SRAM_FIRST + 2u, 2u, &value),
                    T64_STATUS_BUS_UNSUPPORTED);
      T64_CHECK_INT(t64_machine_bus_write(machine, SRAM_FIRST, 1u, 0x11u),
                    T64_STATUS_BUS_UNSUPPORTED);
      T64_CHECK_INT(t64_machine_bus_write(machine, SRAM_FIRST + 2u, 2u,
                                          0x2222u),
                    T64_STATUS_BUS_UNSUPPORTED);
      T64_CHECK_INT(t64_bus_read_wide(machine, SRAM_FIRST, &wide),
                    T64_STATUS_BUS_UNSUPPORTED);
      T64_CHECK_INT(t64_bus_write_masked(machine, SRAM_FIRST, 8u, 0xFFu, 0u),
                    T64_STATUS_BUS_UNSUPPORTED);
      /* The literal status of a misaligned word access: the unchanged
         validation path answers BUS_ALIGNMENT (17) before the decode. */
      T64_CHECK_INT(bus_r32(machine, SRAM_FIRST + 2u, &value), 17u);
      T64_CHECK_INT(bus_w32(machine, SRAM_FIRST + 2u, 0u), 17u);
      T64_CHECK_INT(value, 0xA5A5A5A5u);
    }
    {
      uint8_t reread[T64_DIGEST_SIZE];

      digest_of(machine, reread);
      T64_CHECK(memcmp(before, reread, T64_DIGEST_SIZE) == 0);
    }
    if (machine->save.bytes != NULL) {
      T64_CHECK(memcmp(copy, machine->save.bytes, sizeof copy) == 0);
    }
    t64_machine_destroy(machine);
  }

  /* Every other profile refuses the window exactly as the legacy pin: the
     stem, both EEPROM ids, the AI profile and a legacy profile (these rows
     start green by design: the carve-out must not reach them). */
  for (index = 0u; index < sizeof refusing_ids / sizeof refusing_ids[0];
       index++) {
    t64_machine *other = create_admitted(refusing_ids[index]);
    uint32_t value = 0xA5A5A5A5u;

    if (other == NULL) continue;
    digest_of(other, before);
    T64_CHECK_INT(bus_r32(other, SRAM_FIRST, &value),
                  T64_STATUS_BUS_UNSUPPORTED);
    T64_CHECK_INT(bus_w32(other, SRAM_FIRST, UINT32_C(0x01234567)),
                  T64_STATUS_BUS_UNSUPPORTED);
    T64_CHECK_INT(bus_r32(other, SRAM_LAST_WORD, &value),
                  T64_STATUS_BUS_UNSUPPORTED);
    T64_CHECK_INT(value, 0xA5A5A5A5u);
    digest_of(other, after);
    T64_CHECK(memcmp(before, after, T64_DIGEST_SIZE) == 0);
    t64_machine_destroy(other);
  }
}

/* ------------------------------------------------------------------------ */
/* Dispatch                                                                  */
/* ------------------------------------------------------------------------ */

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"storage-blank", case_storage_blank},
    {"eeprom-tracer", case_eeprom_tracer},
    {"sram-window", case_sram_window},
};

int main(int argc, char **argv) {
  size_t index;
  const size_t count = sizeof cases / sizeof cases[0];

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (index = 0u; index < count; index++) puts(cases[index].name);
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0) {
    if (strcmp(argv[2], "device") != 0) {
      fprintf(stderr, "unknown save suite: %s\n", argv[2]);
      return 2;
    }
    for (index = 0u; index < count; index++) {
      printf("case %s\n", cases[index].name);
      cases[index].run();
    }
    return T64_TEST_FINISH("unit.core.save.device");
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    for (index = 0u; index < count; index++) {
      if (strcmp(cases[index].name, argv[2]) == 0) {
        cases[index].run();
        return T64_TEST_FINISH("unit.core.save.device");
      }
    }
    fprintf(stderr, "unknown save case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--suite device|--case NAME]\n",
            argv[0]);
    return 2;
  }
  for (index = 0u; index < count; index++) {
    printf("case %s\n", cases[index].name);
    cases[index].run();
  }
  return T64_TEST_FINISH("unit.core.save.device");
}
